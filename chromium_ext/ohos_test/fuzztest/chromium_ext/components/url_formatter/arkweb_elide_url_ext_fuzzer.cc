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

#include "arkweb/chromium_ext/components/url_formatter/arkweb_elide_url_ext.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/strings/utf_string_conversions.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

namespace {

// Bound the maximum length of the fuzzed input to keep each iteration cheap.
constexpr size_t kMaxInputLen = 4096;

// Scheme prefixes that exercise distinct ParseInput branches (file, javascript,
// data, blob, arkweb, dataability, resource, standard http/https and a bare
// query tail).
constexpr std::array<std::string_view, 12> kSchemePrefixes = {
    "",          "http://",      "https://",     "file://",
    "javascript:", "data:",       "blob:",        "arkweb://",
    "dataability://", "resource://", "ftp://",    "unknown://",
};

void FuzzParseInput(const std::u16string& input,
                    bool pass_parts,
                    bool pass_scheme,
                    bool pass_url) {
  url::Parsed parts;
  std::u16string scheme;
  GURL canonicalized_url;

  url_formatter::ParseInput(input,
                            pass_parts ? &parts : nullptr,
                            pass_scheme ? &scheme : nullptr,
                            pass_url ? &canonicalized_url : nullptr);
}

struct IcuEnvironment {
  IcuEnvironment() {
    base::i18n::AllowMultipleInitializeCallsForTesting();
    CHECK(base::i18n::InitializeICU());
  }
  base::AtExitManager at_exit_manager;
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static IcuEnvironment env;
  FuzzedDataProvider data_provider(data, size);

  // An optional scheme prefix drives scheme-specific branches deterministically
  // even with short random tails.
  const uint32_t prefix_index = data_provider.ConsumeIntegralInRange<uint32_t>(
      0, static_cast<uint32_t>(kSchemePrefixes.size()));
  const std::string prefix(prefix_index < kSchemePrefixes.size()
                               ? std::string(kSchemePrefixes[prefix_index])
                               : std::string());
  std::string tail = data_provider.ConsumeRandomLengthString(kMaxInputLen);

  const std::u16string input = base::UTF8ToUTF16(prefix + tail);

  // Cycle the nullable out-parameters to cover the nullptr fast paths.
  const bool pass_parts = data_provider.ConsumeBool();
  const bool pass_scheme = data_provider.ConsumeBool();
  const bool pass_url = data_provider.ConsumeBool();
  FuzzParseInput(input, pass_parts, pass_scheme, pass_url);

  // Also exercise the empty / whitespace-only early-return paths.
  FuzzParseInput(u"", true, true, true);
  FuzzParseInput(u"   ", true, true, true);

  return 0;
}
