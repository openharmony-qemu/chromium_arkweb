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

#include "content/browser/permissions/permission_controller_impl.h"
#include "arkweb/build/features/features.h"

#if BUILDFLAG(ARKWEB_PERMISSION)

#include "base/barrier_closure.h"
#include "cef/libcef/browser/browser_info_manager.h"
#include "cef/libcef/browser/frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

namespace content {

namespace {

// Collects all main-frame RenderFrameHosts in the given BrowserContext,
// keyed by their origin for deduplication.
std::map<GURL, RenderFrameHost*> CollectMainFrameByOrigin(
    BrowserContext* context) {
  std::map<GURL, RenderFrameHost*> result;
  for (const auto& info :
       CefBrowserInfoManager::GetInstance()->GetBrowserInfoList()) {
    auto main_frame = info->GetMainFrame();
    if (!main_frame)
      continue;
    auto* rfh = main_frame->GetRenderFrameHost();
    if (!rfh || rfh->GetBrowserContext() != context)
      continue;
    auto origin = rfh->GetLastCommittedURL();
    if (!origin.is_valid() || result.count(origin))
      continue;
    result[origin] = rfh;
  }
  return result;
}

// Finds the main-frame RenderFrameHost of the focused tab.
RenderFrameHost* FindFocusedMainFrameForContext(BrowserContext* context) {
  base::TimeTicks last_active_time_ticks;
  RenderFrameHost* last_active_rfh = nullptr;
  for (const auto& info :
       CefBrowserInfoManager::GetInstance()->GetBrowserInfoList()) {
    auto main_frame = info->GetMainFrame();
    if (!main_frame)
      continue;
    auto* rfh = main_frame->GetRenderFrameHost();
    if (!rfh || rfh->GetBrowserContext() != context || rfh->GetLastCommittedURL().SchemeIs(kChromeDevToolsScheme))
      continue;
    if (!static_cast<RenderFrameHostImpl*>(rfh)->GetRenderWidgetHost()->IsHidden())
      return rfh;
    WebContents* web_contents = WebContents::FromRenderFrameHost(rfh);
    if (web_contents && web_contents->GetLastActiveTimeTicks() > last_active_time_ticks) {
      last_active_time_ticks = web_contents->GetLastActiveTimeTicks();
      last_active_rfh = rfh;
    }
  }
  return last_active_rfh;
}

}  // namespace

void PermissionControllerImpl::GrantPermissionOverridesToDelegate(
    base::optional_ref<const url::Origin> requesting_origin,
    const std::vector<PermissionType>& permissions,
    base::OnceCallback<void(OverrideStatus)> callback) {
  auto* delegate = browser_context_->GetPermissionControllerDelegate();

  if (!delegate || permissions.empty()) {
    std::move(callback).Run(OverrideStatus::kOverrideSet);
    return;
  }

  // Filter to supported permission types (deduplicated).
  std::set<PermissionType> seen;
  std::vector<PermissionType> supported;
  for (auto permission : permissions) {
    if (seen.count(permission))
      continue;
    switch (permission) {
      case PermissionType::GEOLOCATION:
      case PermissionType::NOTIFICATIONS:
      case PermissionType::AUDIO_CAPTURE:
      case PermissionType::VIDEO_CAPTURE:
      case PermissionType::CLIPBOARD_READ_WRITE:
      case PermissionType::CLIPBOARD_SANITIZED_WRITE:
      case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      case PermissionType::MIDI_SYSEX:
        seen.insert(permission);
        supported.push_back(permission);
        break;
      default:
        break;
    }
  }

  if (supported.empty()) {
    std::move(callback).Run(OverrideStatus::kOverrideSet);
    return;
  }

  // Determine which origins to grant for.
  // When requesting_origin is specified, grant for that single origin.
  // When empty, grant for all origins across tabs in the BrowserContext.
  std::map<GURL, RenderFrameHost*> origin_to_rfh;

  if (requesting_origin.has_value()) {
    // Single origin: use the focused tab's RFH.
    auto* rfh = FindFocusedMainFrameForContext(browser_context_);
    if (!rfh) {
      std::move(callback).Run(OverrideStatus::kOverrideSet);
      return;
    }
    origin_to_rfh[requesting_origin->GetURL()] = rfh;
  } else {
    // No origin specified: collect all origins across tabs.
    origin_to_rfh = CollectMainFrameByOrigin(browser_context_);
    if (origin_to_rfh.empty()) {
      std::move(callback).Run(OverrideStatus::kOverrideSet);
      return;
    }
  }

  auto descriptors =
      PermissionDescriptorUtil::CreatePermissionDescriptorForPermissionTypes(
          supported);

  if (descriptors.empty()) {
    std::move(callback).Run(OverrideStatus::kOverrideSet);
    return;
  }

  // Dispatch RequestPermissions per origin, BarrierClosure to aggregate results.
  auto all_granted = std::make_shared<bool>(true);
  auto done = base::BarrierClosure(
      origin_to_rfh.size(),
      base::BindOnce(
          [](base::OnceCallback<void(OverrideStatus)> cb,
             std::shared_ptr<bool> all_granted) {
            std::move(cb).Run(*all_granted ? OverrideStatus::kOverrideSet
                                           : OverrideStatus::kOverrideNotSet);
          },
          std::move(callback), all_granted));

  for (const auto& [origin_url, rfh] : origin_to_rfh) {
    // Each call needs its own copy of descriptors.
    std::vector<blink::mojom::PermissionDescriptorPtr> descriptors_copy;
    descriptors_copy.reserve(descriptors.size());
    for (const auto& d : descriptors) {
      descriptors_copy.push_back(d.Clone());
    }

    auto per_origin_cb = base::BindOnce(
        [](base::RepeatingClosure done,
           std::shared_ptr<bool> all_granted,
           const std::vector<PermissionResult>& results) {
          for (const auto& r : results) {
            if (r.status != PermissionStatus::GRANTED) {
              *all_granted = false;
              break;
            }
          }
          done.Run();
        },
        done, all_granted);

    PermissionRequestDescription request_description(
        std::move(descriptors_copy), /*user_gesture=*/false, origin_url);

    delegate->RequestPermissions(rfh, request_description,
                                 std::move(per_origin_cb));
  }
}

}  // namespace content

#endif  // BUILDFLAG(ARKWEB_PERMISSION)
