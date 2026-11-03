// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/ohos/font_unique_name_lookup_ohos.h"

#include "base/files/file.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "skia/ext/font_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/font_unique_name_lookup/icu_fold_case_util.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#if BUILDFLAG(ARKWEB_CUSTOM_FONTS)
#include "third_party/blink/renderer/platform/wtf/functional.h"
#endif
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

FontUniqueNameLookupOhos::~FontUniqueNameLookupOhos() = default;

sk_sp<SkTypeface> FontUniqueNameLookupOhos::MatchUniqueName(
    const String& font_unique_name) {
#if BUILDFLAG(ARKWEB_CUSTOM_FONTS)
  auto custom_font = MatchUniqueNameFromArkwebCustomFonts(font_unique_name);
  if (custom_font) {
    return custom_font;
  }
#endif // ARKWEB_CUSTOM_FONTS
  if (!IsFontUniqueNameLookupReadyForSyncLookup()) {
    return nullptr;
  }
  std::optional<FontTableMatcher::MatchResult> match_result =
      font_table_matcher_->MatchName(font_unique_name.Utf8().c_str());
  if (!match_result) {
    return nullptr;
  }
  return skia::DefaultFontMgr()->makeFromFile(match_result->font_path.c_str(),
                                              match_result->ttc_index);
}

void FontUniqueNameLookupOhos::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(ARKWEB_CUSTOM_FONTS)
  EnsureCustomFontsServiceConnected();
  if (arkweb_custom_font_lookup_service_) {
    LOG(DEBUG) << "prefetch arkweb custom fonts";
    arkweb_custom_font_lookup_service_->FetchAllFontFiles(
        blink::BindOnce(&FontUniqueNameLookupOhos::CustomFontsPrefetched,
                        weak_factory_.GetWeakPtr()));
  }
#endif // ARKWEB_CUSTOM_FONTS

  if (!base::FeatureList::IsEnabled(features::kPrefetchFontLookupTables)) {
    return;
  }

  EnsureServiceConnected();
}

bool FontUniqueNameLookupOhos::IsFontUniqueNameLookupReadyForSyncLookup() {
  if (!RuntimeEnabledFeatures::FontSrcLocalMatchingEnabled()) {
    return true;
  }

  EnsureServiceConnected();

  if (font_table_matcher_.get()) {
    return true;
  }

  if (sync_available_.has_value()) {
    return sync_available_.value();
  }

  bool sync_available_from_mojo = false;
  base::ReadOnlySharedMemoryRegion shared_memory_region;
  ohos_font_lookup_service_->GetUniqueNameLookupTableIfAvailable(
      &sync_available_from_mojo, &shared_memory_region);
  sync_available_ = sync_available_from_mojo;

  if (*sync_available_) {
    DCHECK_EQ(pending_callbacks_.size(), 0u);
    ReceiveReadOnlySharedMemoryRegion(std::move(shared_memory_region));
  }

  return *sync_available_;
}

void FontUniqueNameLookupOhos::PrepareFontUniqueNameLookup(
    NotifyFontUniqueNameLookupReady callback) {
  DCHECK(!font_table_matcher_.get());
  DCHECK(RuntimeEnabledFeatures::FontSrcLocalMatchingEnabled());

  pending_callbacks_.push_back(std::move(callback));

  if (pending_callbacks_.size() > 1) {
    return;
  }

  EnsureServiceConnected();

  ohos_font_lookup_service_->GetUniqueNameLookupTable(base::BindOnce(
      &FontUniqueNameLookupOhos::ReceiveReadOnlySharedMemoryRegion,
      weak_factory_.GetWeakPtr()));
}

void FontUniqueNameLookupOhos::EnsureServiceConnected() {
  if (!ohos_font_lookup_service_) {
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        ohos_font_lookup_service_.BindNewPipeAndPassReceiver());
  }
#if BUILDFLAG(ARKWEB_CUSTOM_FONTS)
  EnsureCustomFontsServiceConnected();
#endif // ARKWEB_CUSTOM_FONTS
}

void FontUniqueNameLookupOhos::ReceiveReadOnlySharedMemoryRegion(
    base::ReadOnlySharedMemoryRegion shared_memory_region) {
  font_table_matcher_ =
      std::make_unique<FontTableMatcher>(shared_memory_region.Map());
  while (!pending_callbacks_.empty()) {
    NotifyFontUniqueNameLookupReady callback = pending_callbacks_.TakeFirst();
    std::move(callback).Run();
  }
}

#if BUILDFLAG(ARKWEB_CUSTOM_FONTS)
void FontUniqueNameLookupOhos::EnsureCustomFontsServiceConnected() {
#if !BUILDFLAG(ARKWEB_TEST) && !BUILDFLAG(ARKWEB_USE_FUZZING_ENGINE)
  if (!arkweb_custom_font_lookup_service_) {
    LOG(DEBUG) << "arkweb_custom_font_lookup_service_ bind new pipe";
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        arkweb_custom_font_lookup_service_.BindNewPipeAndPassReceiver());
  }
#endif
}

void FontUniqueNameLookupOhos::CustomFontsPrefetched(
    HashMap<String, base::File> font_files) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  prefetched_font_map_ = std::move(font_files);
  LOG(DEBUG) << "prefetched_font_map_ size " << prefetched_font_map_.size();
}

bool FontUniqueNameLookupOhos::RequestedNameInQueryableFonts(
    const String &font_unique_name) {
  if (!queryable_fonts_) {
    Vector<String> retrieved_fonts;
    arkweb_custom_font_lookup_service_->GetUniqueNameLookupTable(
        &retrieved_fonts);
    queryable_fonts_ = std::move(retrieved_fonts);
    LOG(DEBUG) << "GetUniqueNameLookupTable return queryable_fonts_ "
               << queryable_fonts_->size();
  }
  return queryable_fonts_ && queryable_fonts_->Contains(String::FromUTF8(
                                 IcuFoldCase(font_unique_name.Utf8())));
}

sk_sp<SkTypeface>
FontUniqueNameLookupOhos::MatchUniqueNameFromArkwebCustomFonts(
    const String &font_unique_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!arkweb_custom_font_lookup_service_.is_bound()) {
    LOG(ERROR) << "custom font service not connected";
    return nullptr;
  }
  if (!RequestedNameInQueryableFonts(font_unique_name)) {
    LOG(DEBUG) << "RequestedNameInQueryableFonts check fail for "
               << font_unique_name.Utf8();
    return nullptr;
  }

  base::File font_file;
  String cased_font_name =
      String::FromUTF8(IcuFoldCase(font_unique_name.Utf8()));
  auto it = prefetched_font_map_.find(cased_font_name);
  if (it != prefetched_font_map_.end()) {
    LOG(DEBUG) << "prefetched_font_map_ matched for" << cased_font_name.Utf8();
    font_file = it->value.Duplicate();
  } else if (!arkweb_custom_font_lookup_service_->MatchLocalFontByUniqueName(
                 cased_font_name, &font_file)) {
    LOG(ERROR) << "MatchLocalFontByUniqueName failed for "
               << cased_font_name.Utf8();
    return nullptr;
  }

  LOG(INFO) << "custom font matched for " << cased_font_name.Utf8();
  if (!font_file.IsValid()) {
    LOG(ERROR) << "received platform font handle invalid, fd: "
               << font_file.GetPlatformFile();
    return nullptr;
  }

  sk_sp<SkData> font_data = SkData::MakeFromFD(font_file.GetPlatformFile());
  if (!font_data || font_data->isEmpty()) {
    LOG(ERROR) << "received file descriptor has no data";
    return nullptr;
  }

  sk_sp<SkFontMgr> mgr = skia::DefaultFontMgr();
  sk_sp<SkTypeface> result_typeface = mgr->makeFromData(font_data);
  if (!result_typeface) {
    LOG(ERROR) << "cannot instantiate SkTypeface from font blob SkData";
  }

  LOG(DEBUG) << "custom font valid for " << cased_font_name.Utf8();
  return result_typeface;
}
#endif // ARKWEB_CUSTOM_FONTS
}  // namespace blink
