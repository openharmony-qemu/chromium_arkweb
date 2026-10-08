// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arkweb/chromium_ext/chrome/browser/devtools/devtools_nweb_tab_factory.h"

#include "arkweb/build/features/features.h"

#if BUILDFLAG(ARKWEB_MULTI_WINDOW)
#include <queue>

#include "arkweb/ohos_nweb/src/cef_delegate/nweb_handler_delegate.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/browser_info_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#endif

namespace ohos {

#if BUILDFLAG(ARKWEB_MULTI_WINDOW)
namespace {

using OHOS::NWeb::NWebHandlerDelegate;

// CefCallbackImpl wraps the popup completion logic for CDP Target.createTarget.
// On Continue(), it runs CompletePopupCreation and invokes |callback| with the
// resulting WebContents*. On Cancel(), invokes |callback| with nullptr.
class CefCallbackImpl : public CefCallback {
 public:
  using PopupCallback = base::OnceCallback<void(content::WebContents*)>;

  CefCallbackImpl(NWebHandlerDelegate* handler_delegate,
                  const std::string& target_url,
                  content::BrowserContext* browser_context,
                  PopupCallback callback)
      : handler_delegate_(handler_delegate),
        target_url_(target_url),
        browser_context_(browser_context),
        callback_(std::move(callback)) {}

  CefCallbackImpl(const CefCallbackImpl&) = delete;
  CefCallbackImpl& operator=(const CefCallbackImpl&) = delete;

  void Continue() override {
    LOG(INFO) << "CefCallbackImpl Continue";
    if (!handler_delegate_) {
      std::move(callback_).Run(nullptr);
      return;
    }
    content::WebContents* popup_wc =
        handler_delegate_->CompletePopupCreation(target_url_, false,  // isAlert
                                                 true,  // isUserTrigger
                                                 browser_context_);
    std::move(callback_).Run(popup_wc);
  }

  void Cancel() override {
    LOG(INFO) << "CefCallbackImpl Cancel";
    if (!callback_.is_null()) {
      std::move(callback_).Run(nullptr);
    }
  }

 private:
  raw_ptr<NWebHandlerDelegate> handler_delegate_;
  std::string target_url_;
  raw_ptr<content::BrowserContext> browser_context_;
  PopupCallback callback_;
  IMPLEMENT_REFCOUNTING(CefCallbackImpl);
};

// Finds the NWebHandlerDelegate for the focused tab via
// NWebHandlerDelegate::GetFocusState().
NWebHandlerDelegate* FindNWebHandlerDelegate() {
  base::TimeTicks last_active_time_ticks;
  NWebHandlerDelegate* last_active_delegate = nullptr;
  for (const auto& info :
       CefBrowserInfoManager::GetInstance()->GetBrowserInfoList()) {
    if (!info->IsValid() || info->IsClosing())
      continue;
    auto browser_base = info->browser();
    if (!browser_base)
      continue;
    CefRefPtr<CefClient> client = browser_base->GetClient();
    if (!client)
      continue;
    auto* handler_delegate =
        static_cast<NWebHandlerDelegate*>(client.get());
    if (!handler_delegate)
      continue;
    if (browser_base->IsVisible())
      return handler_delegate;
    content::WebContents* web_contents = browser_base->GetWebContents();
    if (web_contents && web_contents->GetLastActiveTimeTicks() > last_active_time_ticks) {
      last_active_time_ticks = web_contents->GetLastActiveTimeTicks();
      last_active_delegate = handler_delegate;
    }
  }

  LOG(ERROR) << "FindNWebHandlerDelegate: no focused tab found";
  return last_active_delegate;
}

// Queue for serializing concurrent CDP Target.createTarget requests.
// Only one popup creation can be in progress at a time because
// NWebHandlerDelegate::popupWindowCallback_ is a single slot.
struct PendingRequest {
  std::string target_url;
  bool is_tab;
  raw_ptr<content::BrowserContext> browser_context;
  NWebTabCreatedCallback callback;
};

bool g_popup_in_progress = false;
std::queue<PendingRequest> g_pending_queue;

void ProcessNextPending();

void StartPopupCreation(const std::string& target_url,
                        bool is_tab,
                        content::BrowserContext* browser_context,
                        NWebTabCreatedCallback callback) {
  NWebHandlerDelegate* handler_delegate = FindNWebHandlerDelegate();
  if (!handler_delegate) {
    std::move(callback).Run(false, "No NWebHandlerDelegate found");
    // No delegate means no popup can ever complete, so drain the queue and
    // reset the in-progress flag to unblock future requests.
    while (!g_pending_queue.empty()) {
      auto& pending = g_pending_queue.front();
      std::move(pending.callback).Run(false, "No NWebHandlerDelegate found");
      g_pending_queue.pop();
    }
    g_popup_in_progress = false;
    return;
  }

  auto on_popup_complete = base::BindOnce(
      [](bool is_tab, NWebTabCreatedCallback callback,
         content::WebContents* popup_wc) {
        if (!popup_wc) {
          std::move(callback).Run(false, "Popup creation failed");
        } else {
          scoped_refptr<content::DevToolsAgentHost> agent_host =
              is_tab ? content::DevToolsAgentHost::GetOrCreateForTab(popup_wc)
                     : content::DevToolsAgentHost::GetOrCreateFor(popup_wc);
          std::move(callback).Run(true, agent_host->GetId());
        }
        // Do NOT set g_popup_in_progress = false here. ProcessNextPending
        // will clear it when the queue is empty, or keep it true while
        // dequeuing the next request. This prevents new TryCreateNWebTab
        // calls from bypassing the queue during the gap between this
        // callback and the PostTask execution.
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(&ProcessNextPending));
      },
      is_tab, std::move(callback));

  auto cef_callback = new CefCallbackImpl(handler_delegate, target_url,
                                          browser_context,
                                          std::move(on_popup_complete));

  handler_delegate->TriggerWindowNewByJS(target_url, false,  // isAlert
                                         true,               // isUserTrigger
                                         std::move(cef_callback));

  LOG(INFO) << "StartPopupCreation: triggered popup flow";
}

void ProcessNextPending() {
  if (g_pending_queue.empty()) {
    g_popup_in_progress = false;
    return;
  }

  auto pending = std::move(g_pending_queue.front());
  g_pending_queue.pop();

  StartPopupCreation(pending.target_url, pending.is_tab,
                     pending.browser_context, std::move(pending.callback));
}

}  // namespace
#endif  // BUILDFLAG(ARKWEB_MULTI_WINDOW)

void TryCreateNWebTab(const GURL& url,
                      bool is_tab,
                      content::BrowserContext* browser_context,
                      NWebTabCreatedCallback callback) {
#if BUILDFLAG(ARKWEB_MULTI_WINDOW)
  std::string target_url =
      url.is_empty() ? std::string(url::kAboutBlankURL) : url.spec();

  if (g_popup_in_progress) {
    g_pending_queue.push({target_url, is_tab, browser_context,
                          std::move(callback)});
    LOG(INFO) << "TryCreateNWebTab: popup in progress, enqueued";
    return;
  }

  g_popup_in_progress = true;
  StartPopupCreation(target_url, is_tab, browser_context,
                     std::move(callback));
#else
  std::move(callback).Run(false, "ARKWEB_MULTI_WINDOW is disabled");
#endif  // BUILDFLAG(ARKWEB_MULTI_WINDOW)
}

}  // namespace ohos
