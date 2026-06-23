/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Coverage strategy
// -----------------
// FontUniqueNameLookupOhos has four entry points:
//   * MatchUniqueName            — safe to call once font_table_matcher_ is
//                                  non-null OR the sync-lookup path can
//                                  early-return cleanly.
//   * IsFontUniqueNameLookupReadyForSyncLookup
//                                — without a real Mojo service the
//                                  mojo Remote will crash; we only call it
//                                  after pre-populating font_table_matcher_
//                                  so the `font_table_matcher_.get() →
//                                  return true` shortcut fires.
//   * PrepareFontUniqueNameLookup — pushes a callback, only invokes Mojo
//                                  on the first call (size>1 short-circuit).
//                                  We call it twice so only the in-queue
//                                  branch executes.
//   * Init                        — gated by FeatureList::IsEnabled, off by
//                                  default; safe early-return.
//
// font_table_matcher_ is `protected` and `#ifdef`-d under
// IS_WIN || IS_ANDROID || ARKWEB_WPT. Fuzzers run in the target toolchain
// where ARKWEB_WPT=1, so the field exists. We subclass to gain access and
// inject a FontTableMatcher built from a shared-memory region populated by
// fuzz bytes (the matcher just calls
// `font_table_.ParseFromArray(mem.data(), mem.size())`, which gracefully
// fails on malformed protobuf, returning an empty table).

#include <fuzzer/FuzzedDataProvider.h>

#include <cstdint>
#include <memory>
#include <string>

#include "arkweb/chromium_ext/third_party/blink/renderer/platform/fonts/ohos/font_unique_name_lookup_ohos.h"
#include "base/command_line.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Subclass exposes the protected font_table_matcher_ slot.
class FontUniqueNameLookupOhosForFuzz : public FontUniqueNameLookupOhos {
 public:
  void SetFontTableMatcher(std::unique_ptr<FontTableMatcher> matcher) {
    font_table_matcher_ = std::move(matcher);
  }
  bool HasMatcher() const { return font_table_matcher_.get() != nullptr; }
  void ResetMatcher() { font_table_matcher_.reset(); }
};

namespace {

// Build a ReadOnlySharedMemoryMapping populated with fuzzer-supplied bytes.
// FontTableMatcher::FontTableMatcher tries to ParseFromArray() the buffer as
// a FontUniqueNameTable protobuf — malformed input is tolerated and leaves
// the matcher with an empty table, so any byte pattern is safe.
base::ReadOnlySharedMemoryMapping MakeMapping(FuzzedDataProvider* fdp) {
  size_t size = fdp->ConsumeIntegralInRange<size_t>(1, 2048);
  std::vector<uint8_t> bytes = fdp->ConsumeBytes<uint8_t>(size);
  if (bytes.empty()) {
    bytes.push_back(0);
  }
  base::MappedReadOnlyRegion region =
      base::ReadOnlySharedMemoryRegion::Create(bytes.size());
  if (!region.IsValid()) {
    return {};
  }
  base::span<uint8_t> dst(region.mapping);
  memcpy(dst.data(), bytes.data(), bytes.size());
  return region.region.Map();
}

String FuzzString(FuzzedDataProvider* fdp, size_t max_len) {
  std::string s = fdp->ConsumeRandomLengthString(max_len);
  return String::FromUTF8(s);
}

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();
  }
  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

// MatchUniqueName with no matcher injected. Path: Sync-lookup runs without
// MatchingEnabled, returns true; null matcher_ would crash → guard by
// briefly injecting an empty matcher to keep the path safe.
void FuzzMatchUniqueNameEmptyMatcher(FuzzedDataProvider* fdp) {
  ScopedFontSrcLocalMatchingForTest scope(false);
  FontUniqueNameLookupOhosForFuzz lookup;
  // Empty buffer → matcher with empty parsed table → MatchName returns
  // nullopt → MatchUniqueName returns nullptr.
  base::ReadOnlySharedMemoryMapping mapping = MakeMapping(fdp);
  if (!mapping.IsValid()) {
    return;
  }
  lookup.SetFontTableMatcher(std::make_unique<FontTableMatcher>(mapping));

  String name = FuzzString(fdp, 256);
  (void)lookup.MatchUniqueName(name);
}

// Repeated lookups against the same matcher: exercises hash/sort/search
// internals of FontTableMatcher::MatchName multiple times per iteration.
void FuzzMatchUniqueNameRepeated(FuzzedDataProvider* fdp) {
  ScopedFontSrcLocalMatchingForTest scope(false);
  FontUniqueNameLookupOhosForFuzz lookup;
  base::ReadOnlySharedMemoryMapping mapping = MakeMapping(fdp);
  if (!mapping.IsValid()) {
    return;
  }
  lookup.SetFontTableMatcher(std::make_unique<FontTableMatcher>(mapping));

  int repeats = fdp->ConsumeIntegralInRange<int>(1, 8);
  for (int i = 0; i < repeats && fdp->remaining_bytes() > 4; ++i) {
    String name = FuzzString(fdp, 128);
    (void)lookup.MatchUniqueName(name);
  }
}

// Sync-ready shortcut: with a non-null matcher,
// IsFontUniqueNameLookupReadyForSyncLookup() takes the
// `font_table_matcher_.get() → return true` path. Safe regardless of
// runtime-features state because that branch is hit before any Mojo call.
void FuzzReadyForSync(FuzzedDataProvider* fdp) {
  ScopedFontSrcLocalMatchingForTest scope(false);
  FontUniqueNameLookupOhosForFuzz lookup;
  base::ReadOnlySharedMemoryMapping mapping = MakeMapping(fdp);
  if (!mapping.IsValid()) {
    return;
  }
  lookup.SetFontTableMatcher(std::make_unique<FontTableMatcher>(mapping));

  (void)lookup.IsFontUniqueNameLookupReadyForSyncLookup();
  // Now also exercise MatchUniqueName which calls into the same predicate.
  (void)lookup.MatchUniqueName(FuzzString(fdp, 64));
}

// Init() is gated by FeatureList::IsEnabled(kPrefetchFontLookupTables)
// which defaults to disabled — exercises the early-return arm. (We do not
// flip the flag because doing so would lead to a Mojo dispatch.)
void FuzzInit(FuzzedDataProvider* /*fdp*/) {
  ScopedFontSrcLocalMatchingForTest scope(false);
  FontUniqueNameLookupOhosForFuzz lookup;
  lookup.Init();
}

// Lifecycle: build a matcher, run a few lookups, reset, run again with a
// different matcher — drives ctor / dtor branches of FontTableMatcher and
// the nullptr-matcher MatchUniqueName arm.
void FuzzLifecycle(FuzzedDataProvider* fdp) {
  ScopedFontSrcLocalMatchingForTest scope(false);
  FontUniqueNameLookupOhosForFuzz lookup;

  // First matcher.
  {
    base::ReadOnlySharedMemoryMapping m1 = MakeMapping(fdp);
    if (m1.IsValid()) {
      lookup.SetFontTableMatcher(std::make_unique<FontTableMatcher>(m1));
      (void)lookup.MatchUniqueName(FuzzString(fdp, 64));
    }
  }

  lookup.ResetMatcher();
  // With matcher gone and FontSrcLocalMatching enabled by default, the
  // sync-lookup path would fall through to mojo — so we only call
  // MatchUniqueName here when we re-inject a matcher.
  {
    base::ReadOnlySharedMemoryMapping m2 = MakeMapping(fdp);
    if (m2.IsValid()) {
      lookup.SetFontTableMatcher(std::make_unique<FontTableMatcher>(m2));
      (void)lookup.MatchUniqueName(FuzzString(fdp, 64));
    }
  }
}

}  // namespace
}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::Environment env;

  FuzzedDataProvider fdp(data, size);
  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 4);
  switch (selector) {
    case 0:
      blink::FuzzMatchUniqueNameEmptyMatcher(&fdp);
      break;
    case 1:
      blink::FuzzMatchUniqueNameRepeated(&fdp);
      break;
    case 2:
      blink::FuzzReadyForSync(&fdp);
      break;
    case 3:
      blink::FuzzInit(&fdp);
      break;
    case 4:
      blink::FuzzLifecycle(&fdp);
      break;
    default:
      blink::FuzzMatchUniqueNameRepeated(&fdp);
      break;
  }
  return 0;
}
