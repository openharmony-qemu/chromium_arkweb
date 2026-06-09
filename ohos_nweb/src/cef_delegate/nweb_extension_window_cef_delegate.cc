/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
 
#include "nweb_extension_window_cef_delegate.h"
 
#include "base/logging.h"
#include "content/public/browser/browser_context.h"
#include "cef/libcef/browser/request_context_impl.h"
#include "cef/ohos_cef_ext/libcef/browser/extensions/api/windows/cef_windows_event_router.h"
#include "nweb_extension_utils.h"

#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#endif

#if BUILDFLAG(ARKWEB_NWEB_EX)
#include "ohos_nweb_ex/core/arkweb/nweb_extension_windows_delegate_impl.h"
#endif
 
namespace OHOS::NWeb {
namespace {
  static std::map<int, WindowCreatedCallback> g_window_created_map_;
  static std::map<int, WindowUpdatedCallback> g_window_updated_map_;
  static std::map<int, WindowRemovedCallback> g_window_removed_map_;

  content::BrowserContext* GetBrowserContextInUse(const WebExtensionWindow& window) {
    auto browser_context = GetBrowserContext();
    if (window.incognito) {
      browser_context = GetIncognitoContext(browser_context);
    }

    return browser_context;
  }
}

// static
NweExtensionWindowCefDelegate* NweExtensionWindowCefDelegate::GetInstance() {
  static NweExtensionWindowCefDelegate instance;
  return &instance;
}

NweExtensionWindowCefDelegate::NweExtensionWindowCefDelegate() {}

void NweExtensionWindowCefDelegate::WindowCreated(const WebExtensionWindow& window) {
  auto browser_context = GetBrowserContextInUse(window);
  if (!browser_context) {
    LOG(ERROR) << "WindowCreated get browser context failed.";
    return;
  }

  extensions::CefWindowsEventRouter::GetInstance()->DispatchWindowCreatedEvent(browser_context, window);
}
 
void NweExtensionWindowCefDelegate::WindowRemoved(const WebExtensionWindow& window) {
  auto browser_context = GetBrowserContextInUse(window);
  if (!browser_context) {
    LOG(ERROR) << "WindowRemoved get browser context failed.";
    return;
  }

  extensions::CefWindowsEventRouter::GetInstance()->DispatchWindowRemovedEvent(browser_context, window);
}
 
void NweExtensionWindowCefDelegate::WindowBoundsChanged(const WebExtensionWindow& window) {
  auto browser_context = GetBrowserContextInUse(window);
  if (!browser_context) {
    LOG(ERROR) << "WindowBoundsChanged get browser context failed.";
    return;
  }

  extensions::CefWindowsEventRouter::GetInstance()->DispatchWindowBoundsChangedEvent(browser_context, window);
}
 
void NweExtensionWindowCefDelegate::WindowFocusChanged(const WebExtensionWindow& window) {
  auto browser_context = GetBrowserContextInUse(window);
  if (!browser_context) {
    LOG(ERROR) << "WindowFocusChanged get browser context failed.";
    return;
  }

  extensions::CefWindowsEventRouter::GetInstance()->DispatchWindowFocusChangedEvent(browser_context, window);
}

NO_SANITIZE("cfi-icall")
bool NweExtensionWindowCefDelegate::OnCreateWindowV2(
    const WebExtensionWindowCreateData& create_date,
    const WebExtensionWindowCreateDataV2& create_date_V2,
    WindowCreatedCallback callback) {
#if !BUILDFLAG(ARKWEB_NWEB_EX)
  return false;
#else

  static int request_id = 0;
  request_id++;

  g_window_created_map_[request_id] = std::move(callback);
  if (!nweb_ex::NWebExtensionWindowsDelegateImpl::GetInstance().OnCreateWindowV2(request_id, create_date_V2)) {
    g_window_created_map_.erase(request_id);
    return false;
  }

  return true;
#endif
}

NO_SANITIZE("cfi-icall")
bool NweExtensionWindowCefDelegate::OnUpdateWindowV2(
    int windowId,
    const WebExtensionWindowUpdateInfoV2& update_info,
    WindowUpdatedCallback callback) {
#if !BUILDFLAG(ARKWEB_NWEB_EX)
  return false;
#else
  static int request_id = 0;
  request_id++;

  g_window_updated_map_[request_id] = std::move(callback);
  if (!nweb_ex::NWebExtensionWindowsDelegateImpl::GetInstance().OnUpdateWindowV2(request_id, windowId, update_info)) {
    g_window_updated_map_.erase(request_id);
    return false;
  }

  return true;
#endif
}

NO_SANITIZE("cfi-icall")
bool NweExtensionWindowCefDelegate::OnRemoveWindowV2(
    int windowId,
    const WebExtensionWindowRemoveInfoV2& remove_info,
    WindowRemovedCallback callback) {
#if !BUILDFLAG(ARKWEB_NWEB_EX)
  return false;
#else
  static int request_id = 0;
  request_id++;

  g_window_removed_map_[request_id] = std::move(callback);
  if (!nweb_ex::NWebExtensionWindowsDelegateImpl::GetInstance().OnRemoveWindowV2(request_id, windowId, remove_info)) {
    g_window_removed_map_.erase(request_id);
    return false;
  }

  return true;
#endif
}

NO_SANITIZE("cfi-icall")
std::optional<WebExtensionWindow> NweExtensionWindowCefDelegate::OnGetCurrentWindow(
      int currentWindowId,
      const WebExtensionWindowQueryOptions& queryOptions) {
#if !BUILDFLAG(ARKWEB_NWEB_EX)
  return std::nullopt;
#else
  return nweb_ex::NWebExtensionWindowsDelegateImpl::GetInstance().OnGetCurrentWindow(currentWindowId, queryOptions);
#endif
}

NO_SANITIZE("cfi-icall")
bool NweExtensionWindowCefDelegate::HasOnGetCurrentWindowV2CallBack() {
#if !BUILDFLAG(ARKWEB_NWEB_EX)
  return false;
#else
  return nweb_ex::NWebExtensionWindowsDelegateImpl::GetInstance().HasOnGetCurrentWindowV2CallBack();
#endif
}

NO_SANITIZE("cfi-icall")
std::optional<WebExtensionWindow> NweExtensionWindowCefDelegate::OnGetWindowV2(
    int windowId,
    const WebExtensionWindowQueryOptionsV2& queryOptions) {
#if !BUILDFLAG(ARKWEB_NWEB_EX)
  return std::nullopt;
#else
  return nweb_ex::NWebExtensionWindowsDelegateImpl::GetInstance().OnGetWindowV2(windowId, queryOptions);
#endif
}

NO_SANITIZE("cfi-icall")
std::vector<WebExtensionWindow> NweExtensionWindowCefDelegate::OnGetAllWindowsV2(
    const WebExtensionWindowQueryOptionsV2& queryOptions) {
#if !BUILDFLAG(ARKWEB_NWEB_EX)
  return std::vector<WebExtensionWindow>();
#else
  return nweb_ex::NWebExtensionWindowsDelegateImpl::GetInstance().OnGetAllWindowsV2(queryOptions);
#endif
}

NO_SANITIZE("cfi-icall")
std::optional<WebExtensionWindow> NweExtensionWindowCefDelegate::OnGetCurrentWindowV2(
      int currentWindowId,
      const WebExtensionWindowQueryOptionsV2& queryOptions) {
#if !BUILDFLAG(ARKWEB_NWEB_EX)
  return std::nullopt;
#else
  return nweb_ex::NWebExtensionWindowsDelegateImpl::GetInstance().OnGetCurrentWindowV2(currentWindowId, queryOptions);
#endif
}

NO_SANITIZE("cfi-icall")
std::optional<WebExtensionWindow> NweExtensionWindowCefDelegate::OnGetLastFocusedWindowV2(
      const WebExtensionWindowQueryOptionsV2& queryOptions) {
#if !BUILDFLAG(ARKWEB_NWEB_EX)
  return std::nullopt;
#else
  return nweb_ex::NWebExtensionWindowsDelegateImpl::GetInstance().OnGetLastFocusedWindowV2(queryOptions);
#endif
}

void NweExtensionWindowCefDelegate::WindowCreateCallback(int request_id,
                                                             const std::optional<WebExtensionWindow>& window,
                                                             const std::optional<std::string>& error) {
  if (g_window_created_map_.count(request_id)) {
    std::move(g_window_created_map_[request_id]).Run(window, error);
    g_window_created_map_.erase(request_id);
  }
}
 
void NweExtensionWindowCefDelegate::WindowUpdateCallback(int request_id,
                                                             const std::optional<WebExtensionWindow>& window,
                                                             const std::optional<std::string>& error) {
  if (g_window_updated_map_.count(request_id)) {
    std::move(g_window_updated_map_[request_id]).Run(window, error);
    g_window_updated_map_.erase(request_id);
  }
}
 
void NweExtensionWindowCefDelegate::WindowRemoveCallback(int request_id, const std::optional<std::string>& error) {
  if (g_window_removed_map_.count(request_id)) {
    std::move(g_window_removed_map_[request_id]).Run(error);
    g_window_removed_map_.erase(request_id);
  }
}

} // namespace OHOS::NWeb
