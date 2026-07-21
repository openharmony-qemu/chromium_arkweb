/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "nweb_extension_action_cef_delegate.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/ohos_cef_ext/libcef/browser/alloy/alloy_browser_host_impl_ext.h"
#include "cef/ohos_cef_ext/libcef/browser/extensions/tab_extensions_util.h"
#include "content/public/common/content_switches.h"
#include "nweb_common.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if BUILDFLAG(ARKWEB_NWEB_EX)
#include "ohos_nweb_ex/core/arkweb/nweb_extension_action_delegate_impl.h"
#endif

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
extern "C" {
void* __real_malloc(size_t);
}  // extern "C"
#endif

namespace OHOS::NWeb {

namespace {

NWebExtensionActionIconColorType GetColorTypeFromSkBitmap(
    const SkBitmap& bitmap) {
  NWebExtensionActionIconColorType colorType;

  switch (bitmap.colorType()) {
    case kRGBA_8888_SkColorType:
      colorType = NWebExtensionActionIconColorType::RGBA_8888;
      break;
    case kBGRA_8888_SkColorType:
      colorType = NWebExtensionActionIconColorType::BGRA_8888;
      break;
    default:
      colorType = NWebExtensionActionIconColorType::UNKNOWN;
      LOG(ERROR) << "NWebExtensionActionIconColorType convert failed";
      break;
  }
  return colorType;
}

NWebExtensionActionIconAlphaType GetAlphaTypeFromSkBitmap(
    const SkBitmap& bitmap) {
  NWebExtensionActionIconAlphaType alphaType;

  switch (bitmap.alphaType()) {
    case kOpaque_SkAlphaType:
      alphaType = NWebExtensionActionIconAlphaType::OPAQUE;
      break;
    case kPremul_SkAlphaType:
      alphaType = NWebExtensionActionIconAlphaType::PREMUL;
      break;
    case kUnpremul_SkAlphaType:
      alphaType = NWebExtensionActionIconAlphaType::UNPREMUL;
      break;
    default:
      alphaType = NWebExtensionActionIconAlphaType::UNKNOWN;
      LOG(ERROR) << "NWebExtensionActionIconAlphaType convert failed";
      break;
  }
  return alphaType;
}

NWebExtensionActionIconBitmap CreateIconBitmapFromImage(
    const SkBitmap& bitmap) {
  NWebExtensionActionIconBitmap iconBitmap;

  iconBitmap.colorType = GetColorTypeFromSkBitmap(bitmap);
  iconBitmap.alphaType = GetAlphaTypeFromSkBitmap(bitmap);
  iconBitmap.width = bitmap.width();
  iconBitmap.height = bitmap.height();
  iconBitmap.bitmap = static_cast<uint8_t*>(bitmap.getPixels());

  return iconBitmap;
}

void DeleteNWebExtensionActionIcon(NWebExtensionActionIcon* icon) {
  if (!icon) {
    return;
  }

  for (auto& it : icon->bitmaps) {
    delete it.second;
  }
  delete icon;
}

std::optional<NWebExtensionActionIconBitmapV2> ConvertToBitmapV2(
    const SkBitmap& source_bitmap,
    double scale) {
  if (source_bitmap.drawsNothing() || source_bitmap.empty()) {
    return std::nullopt;
  }
  NWebExtensionActionIconBitmapV2 bitmap;
  bitmap.colorType = GetColorTypeFromSkBitmap(source_bitmap);
  bitmap.alphaType = GetAlphaTypeFromSkBitmap(source_bitmap);
  bitmap.width = source_bitmap.width();
  bitmap.height = source_bitmap.height();
  const size_t byte_size = source_bitmap.computeByteSize();
  bitmap.bitmap.resize(byte_size);
  if (!source_bitmap.readPixels(
          SkImageInfo::Make(source_bitmap.width(), source_bitmap.height(),
                            source_bitmap.colorType(),
                            source_bitmap.alphaType()),
          bitmap.bitmap.data(), source_bitmap.rowBytes(), 0, 0)) {
    LOG(ERROR) << "Failed to read bitmap pixels";
    return std::nullopt;
  }
  return bitmap;
}

NWebExtensionActionSetIconDetailsV2 GetActionSetIconDetailsV2(
    const gfx::Image& icon_image,
    const std::optional<int32_t>& tabId,
    const std::optional<std::string>& contextType,
    const std::optional<bool>& includeIncognitoInfo) {
  NWebExtensionActionSetIconDetailsV2 details;
  if (!icon_image.IsEmpty()) {
    const gfx::ImageSkia image_skia = icon_image.AsImageSkia();
    const std::vector<gfx::ImageSkiaRep> image_reps = image_skia.image_reps();
    for (const auto& rep : image_reps) {
      const double scale = rep.scale();
      const SkBitmap& source_bitmap = rep.GetBitmap();
      auto bitmap = ConvertToBitmapV2(source_bitmap, scale);
      if (!bitmap) {
        continue;
      }
      details.icon.bitmaps.emplace(scale, std::move(*bitmap));
    }
  }
  details.tabId = tabId;
  details.contextType = contextType;
  details.includeIncognitoInfo = includeIncognitoInfo;
  return details;
}

}  // namespace

NWebExtensionActionIcon CreateFromImageSkiaReps(
    const std::vector<gfx::ImageSkiaRep>& imageSkiaReps) {
  NWebExtensionActionIcon actionIcon;
  for (auto rep : imageSkiaReps) {
    double scale = rep.scale();
#if defined(ADDRESS_SANITIZER) || defined(HWADDRESS_SANITIZER)
    actionIcon.bitmaps[scale] = new NWebExtensionActionIconBitmap(
        CreateIconBitmapFromImage(rep.GetBitmap()));
#else
    NWebExtensionActionIconBitmap* addr =
        (NWebExtensionActionIconBitmap*)__real_malloc(
            sizeof(NWebExtensionActionIconBitmap));
    actionIcon.bitmaps[scale] = new (addr) NWebExtensionActionIconBitmap(
        CreateIconBitmapFromImage(rep.GetBitmap()));
#endif
  }
  return actionIcon;
}

NWebExtensionActionIconV2 CreateNWebIconFromImageSkiaRepsV2(
    const std::vector<gfx::ImageSkiaRep>& imageSkiaReps) {
  NWebExtensionActionIconV2 actionIcon;
  for (const auto& rep : imageSkiaReps) {
    const double scale = rep.scale();
    const SkBitmap& source_bitmap = rep.GetBitmap();
    auto bitmap = ConvertToBitmapV2(source_bitmap, scale);
    if (!bitmap) {
      continue;
    }
    actionIcon.bitmaps.emplace(scale, std::move(*bitmap));
  }
  return actionIcon;
}

// static
NWebExtensionActionCefDelegate* NWebExtensionActionCefDelegate::GetInstance() {
  static NWebExtensionActionCefDelegate instance;
  return &instance;
}

// static
void NWebExtensionActionCefDelegate::WebExtensionActionPinnedStateChanged(
    std::string extensionId,
    bool isPinned) {
  AlloyBrowserHostImplExt::WebExtensionActionPinnedStateChanged(extensionId,
                                                                isPinned);
}

// static
void NWebExtensionActionCefDelegate::WebExtensionActionClicked(
    std::string extension_id,
    const NWebExtensionTabV2* tab) {
  AlloyBrowserHostImplExt::WebExtensionActionClicked(extension_id, tab);
}

// static
void NWebExtensionActionCefDelegate::WebExtensionActionShowPopup(
    int tabId,
    std::string extensionId) {
  AlloyBrowserHostImplExt::WebExtensionActionShowPopup(tabId, extensionId);
}

//  static
void NWebExtensionActionCefDelegate::WebExtensionErasePopupWindowId(
    int popupNwebId) {
  extensions::CefExtensionWindowIdManager::ErasePopupWindowId(popupNwebId);
}

//  static
void NWebExtensionActionCefDelegate::WebExtensionSetPopupWindowId(
    int popupNwebId,
    int windowId) {
  extensions::CefExtensionWindowIdManager::SetPopupWindowId(popupNwebId,
                                                            windowId);
}

NO_SANITIZE("cfi-icall")
void NWebExtensionActionCefDelegate::OnDisable(
    const std::string& extensionId,
    std::optional<int>& tabId,
    std::optional<std::string> contextType,
    std::optional<bool> includeIncognitoInfo) {
#if BUILDFLAG(ARKWEB_NWEB_EX)
  NWebExtensionActionDisableOptionsV2 options = {tabId, contextType, includeIncognitoInfo};
  nweb_ex::NWebExtensionActionDelegateImpl::GetInstance().OnDisableV2(extensionId, options);
  return;
#endif
}

NO_SANITIZE("cfi-icall")
void NWebExtensionActionCefDelegate::OnEnable(
    const std::string& extensionId,
    std::optional<int>& tabId,
    std::optional<std::string> contextType,
    std::optional<bool> includeIncognitoInfo) {
#if BUILDFLAG(ARKWEB_NWEB_EX)
  NWebExtensionActionEnableOptionsV2 options = {tabId, contextType, includeIncognitoInfo};
  nweb_ex::NWebExtensionActionDelegateImpl::GetInstance().OnEnableV2(extensionId, options);
  return;
#endif
}

NO_SANITIZE("cfi-icall")
void NWebExtensionActionCefDelegate::OnOpenPopup(
    const std::string& extensionId,
    const std::optional<NWebExtensionActionOpenPopupOptions>& options,
    std::optional<std::string> contextType,
    std::optional<bool> includeIncognitoInfo) {
#if BUILDFLAG(ARKWEB_NWEB_EX)
  NWebExtensionActionOpenPopupOptionsV2 optionsV2 = {
      options.has_value() ? options->windowId : std::nullopt, contextType, includeIncognitoInfo};
  nweb_ex::NWebExtensionActionDelegateImpl::GetInstance().OnOpenPopupV2(extensionId, optionsV2);
  return;
#endif
}

NO_SANITIZE("cfi-icall")
void NWebExtensionActionCefDelegate::OnSetTitle(
    const std::string& extensionId,
    const NWebExtensionActionSetTitleDetails& details,
    std::optional<std::string> contextType,
    std::optional<bool> includeIncognitoInfo) {
#if BUILDFLAG(ARKWEB_NWEB_EX)
  NWebExtensionActionSetTitleDetailsV2 detailsV2 = {details.title, details.tabId, contextType, includeIncognitoInfo};
  nweb_ex::NWebExtensionActionDelegateImpl::GetInstance().OnSetTitleV2(extensionId, detailsV2);
  return;
#endif
}

NO_SANITIZE("cfi-icall")
void NWebExtensionActionCefDelegate::OnSetPopup(
    const std::string& extensionId,
    const NWebExtensionActionSetPopupDetails& details,
    std::optional<std::string> contextType,
    std::optional<bool> includeIncognitoInfo) {
#if BUILDFLAG(ARKWEB_NWEB_EX)
  NWebExtensionActionSetPopupDetailsV2 detailsV2 = {details.popup, details.tabId, contextType, includeIncognitoInfo};
  nweb_ex::NWebExtensionActionDelegateImpl::GetInstance().OnSetPopupV2(extensionId, detailsV2);
  return;
#endif
}

NO_SANITIZE("cfi-icall")
void NWebExtensionActionCefDelegate::OnSetIcon(
    std::string extension_id,
    const gfx::Image& icon_image,
    int32_t tab_id,
    std::optional<std::string> contextType,
    std::optional<bool> includeIncognitoInfo) {
#if BUILDFLAG(ARKWEB_NWEB_EX)
  LOG(INFO) << "OnSetIconV2 extension ID:" << extension_id;
  NWebExtensionActionSetIconDetailsV2 details =
      GetActionSetIconDetailsV2(icon_image, tab_id, contextType, includeIncognitoInfo);
  nweb_ex::NWebExtensionActionDelegateImpl::GetInstance().OnSetIconV2(extension_id, details);
  return;
#endif
}

NO_SANITIZE("cfi-icall")
void NWebExtensionActionCefDelegate::OnSetBadgeText(
    const std::string& extensionId,
    const NWebExtensionActionSetBadgeTextDetails& details,
    std::optional<std::string> contextType,
    std::optional<bool> includeIncognitoInfo) {
#if BUILDFLAG(ARKWEB_NWEB_EX)
  NWebExtensionActionSetBadgeTextDetailsV2 detailsV2 = {details.text, details.tabId, contextType, includeIncognitoInfo};
  nweb_ex::NWebExtensionActionDelegateImpl::GetInstance().OnSetBadgeTextV2(extensionId, detailsV2);
  return;
#endif
}

NO_SANITIZE("cfi-icall")
void NWebExtensionActionCefDelegate::OnSetBadgeTextColor(
    const std::string& extensionId,
    const NWebExtensionActionSetBadgeTextColorDetails& details,
    std::optional<std::string> contextType,
    std::optional<bool> includeIncognitoInfo) {
#if BUILDFLAG(ARKWEB_NWEB_EX)
  NWebExtensionActionSetBadgeTextColorDetailsV2 detailsV2 = {
      details.color, details.tabId, contextType, includeIncognitoInfo};
  nweb_ex::NWebExtensionActionDelegateImpl::GetInstance().OnSetBadgeTextColorV2(extensionId, detailsV2);
  return;
#endif
}

NO_SANITIZE("cfi-icall")
void NWebExtensionActionCefDelegate::OnSetBadgeBackgroundColor(
    const std::string& extensionId,
    const NWebExtensionActionSetBadgeBackgroundColorDetails& details,
    std::optional<std::string> contextType,
    std::optional<bool> includeIncognitoInfo) {
#if BUILDFLAG(ARKWEB_NWEB_EX)
  NWebExtensionActionSetBadgeBackgroundColorDetailsV2 detailsV2 = {
      details.color, details.tabId, contextType, includeIncognitoInfo};
  nweb_ex::NWebExtensionActionDelegateImpl::GetInstance().OnSetBadgeBackgroundColorV2(extensionId, detailsV2);
  return;
#endif
}

NO_SANITIZE("cfi-icall")
std::optional<NWebExtensionActionUserSettings>
NWebExtensionActionCefDelegate::OnGetUserSettings(const std::string& extensionId) {
#if BUILDFLAG(ARKWEB_NWEB_EX)
  return nweb_ex::NWebExtensionActionDelegateImpl::GetInstance().OnGetUserSettings(extensionId);
#else
  return std::nullopt;
#endif
}

}  // namespace OHOS::NWeb
