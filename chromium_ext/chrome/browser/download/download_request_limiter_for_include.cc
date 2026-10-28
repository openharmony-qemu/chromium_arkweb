/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

// This file is injected into chrome/browser/download/download_request_limiter.cc
// via #include. It must not be compiled standalone.

#if BUILDFLAG(ARKWEB_EXT_DOWNLOAD)
#include <map>
#include <tuple>
#include <utility>
#include <vector>

#include "arkweb/chromium_ext/base/ohos/sys_info_utils_ext.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches_ext.h"
#include "ohos_nweb/src/capi/browser_service/nweb_extension_prompt_types.h"
#include "ohos_nweb/src/cef_delegate/nweb_extension_prompt_cef_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ohos {

namespace {

// Mirrors PromptInfoHolder in
// arkweb/chromium_ext/chrome/browser/extensions/extension_install_prompt_ext.cc.
// Holds the download callback until the user responds to the OHOS permission
// dialog, then persists the AUTOMATIC_DOWNLOADS content setting and runs the
// callback. Self-managing: released in OnPromptAction (or on failure).
// Observes WebContents so that if the OHOS dialog never calls back (e.g. the
// Activity is killed by the system), the holder is destroyed together with
// the WebContents and the callback is run as a cancellation, avoiding both a
// holder leak and a dangling download request.
//
// Uses a static map keyed by WebContents to track the single in-flight prompt
// per tab. Subsequent downloads while the prompt is pending queue their
// callbacks in pending_callbacks_ instead of showing a second dialog. When
// the user responds, all callbacks are dispatched with the same result.
class DownloadPromptHolder : public content::WebContentsObserver {
 public:
  DownloadPromptHolder(content::WebContents* web_contents,
                       const url::Origin& request_origin,
                       DownloadRequestLimiter::Callback callback)
      : content::WebContentsObserver(web_contents),
        web_contents_(web_contents),
        request_origin_(request_origin) {
    pending_callbacks_.emplace_back(std::move(callback));
  }

  ~DownloadPromptHolder() override {
    // Safety net: cancel any callbacks still pending (e.g. ShowExtensionPrompt
    // failed synchronously without OnPromptAction being called).
    for (auto& cb : pending_callbacks_) {
      if (cb) {
        std::move(cb).Run(false);
      }
    }
  }

  // content::WebContentsObserver
  void WebContentsDestroyed() override {
    content::WebContents* wc = web_contents_;
    Observe(nullptr);
    UnregisterActiveHolder(wc);
    web_contents_ = nullptr;
    auto weak_this = weak_factory_.GetWeakPtr();
    std::vector<DownloadRequestLimiter::Callback> callbacks;
    callbacks.swap(pending_callbacks_);
    for (auto& cb : callbacks) {
      if (cb) {
        std::move(cb).Run(false);
      }
      if (!weak_this) {
        return;
      }
    }
    std::ignore = std::unique_ptr<DownloadPromptHolder>(this);
  }

  // Page navigation (reload / cross-origin navigation). Reset the holder so
  // that the next download on the new page triggers a fresh prompt.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInPrimaryMainFrame() ||
        !navigation_handle->HasCommitted()) {
      return;
    }

    has_decision_ = false;
    user_decision_ = kNoDecision;
    prompt_in_flight_ = false;
  }

  // Add a callback from a subsequent download that arrived while this prompt
  // is still pending. Returns true if the callback was queued (prompt is
  // already showing), false if a new prompt should be created.
  void TryQueueCallback(DownloadRequestLimiter::Callback callback) {
    pending_callbacks_.emplace_back(std::move(callback));
  }

  static void Show(std::unique_ptr<DownloadPromptHolder> holder) {
    // Save the WebContents pointer before any operation that might destroy
    // |holder| (e.g., synchronous OnPromptAction in the failure path).
    content::WebContents* wc = holder->web_contents_.get();

    // Check if there's already an active holder for this WebContents.
    if (DownloadPromptHolder* existing = GetActiveHolder(wc)) {
      if (existing->has_decision_) {
        // User already responded to the prompt. Dispatch immediately with the
        // recorded decision — no new prompt, no queueing.
        bool allow = existing->user_decision_ == kAllowed;
        std::move(holder->pending_callbacks_[0]).Run(allow);
        return;
      }
      if (existing->prompt_in_flight_) {
        // Prompt is still showing — queue the callback.
        existing->TryQueueCallback(std::move(holder->pending_callbacks_[0]));
        return;
      }
      // Holder exists but no prompt is in flight (e.g. after page navigation
      // reset). Fall through to show a new prompt. Reuse the existing holder
      // by moving the new callback into it.
      existing->TryQueueCallback(std::move(holder->pending_callbacks_[0]));
      existing->ShowPrompt();
      return;
    }

    // Register as the active holder for this WebContents.
    RegisterActiveHolder(wc, holder.get());
    holder->ShowPrompt();
    std::ignore = holder.release();  // managed by holder itself.
  }

  void ShowPrompt() {
    auto holder_weak_ptr = weak_factory_.GetWeakPtr();
    auto show_prompt_func =
        base::BindRepeating(&DownloadPromptHolder::OnPromptAction,
                            holder_weak_ptr);
    auto get_prompt_data_func =
        base::BindRepeating(&DownloadPromptHolder::GetPromptData,
                            holder_weak_ptr);
    gfx::Image empty_icon;
    prompt_in_flight_ = true;
    if (!OHOS::NWeb::NWebExtensionPromptCefDelegate::GetInstance()
             .ShowExtensionPrompt(PROMPT_PERMISSION,
                                  request_origin_.GetURL().spec(),
                                  empty_icon,
                                  show_prompt_func,
                                  get_prompt_data_func)) {
      prompt_in_flight_ = false;
      // OnPromptAction may have run synchronously and already cleared
      // callbacks. If not, cancel them here.
      for (auto& cb : pending_callbacks_) {
        if (cb) {
          std::move(cb).Run(false);
        }
      }
      pending_callbacks_.clear();
      return;
    }
  }

  void GetPromptData(NWebExtensionPromptData* data) {
    if (!data) {
      return;
    }

    data->free_memory_func =
        OHOS::NWeb::NWebExtensionPromptCefDelegate::FreePromptData;

    std::u16string host = base::UTF8ToUTF16(request_origin_.GetURL().spec());
    std::string title = base::UTF16ToUTF8(
        l10n_util::GetStringFUTF16(IDS_MULTI_DOWNLOAD_PERMISSION_WARNING, host));

    std::string accept_button_label =
        l10n_util::GetStringUTF8(IDS_PERMISSION_ALLOW);
    std::string abort_button_label =
        l10n_util::GetStringUTF8(IDS_PERMISSION_DENY);

    data->title = strdup(title.c_str());
    data->acceptButtonLabel = strdup(accept_button_label.c_str());
    data->abortButtonLabel = strdup(abort_button_label.c_str());
    data->permissionsHeading = nullptr;
    data->permissionCount = 0;
    data->permissions = nullptr;
    data->is_from_webstore = false;
  }

  void OnPromptAction(int action, const std::string& error) {
    if (!content::GetUIThreadTaskRunner({})->BelongsToCurrentThread()) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&DownloadPromptHolder::OnPromptAction,
                         weak_factory_.GetWeakPtr(), action, error));
      return;
    }

    prompt_in_flight_ = false;

    bool allow = static_cast<DialogAction>(action) == DialogAction::kAutoConfirm;
    if (allow) {
      SetContentSettingExt(CONTENT_SETTING_ALLOW);
    } else {
      SetContentSettingExt(CONTENT_SETTING_BLOCK);
    }

    // Record the user's decision so subsequent downloads (3rd, 4th, ...) can
    // be dispatched immediately without re-prompting. The holder stays alive
    // and registered in the static map until WebContents is destroyed (page
    // refresh / tab close).
    user_decision_ = allow ? kAllowed : kBlocked;
    has_decision_ = true;

    auto weak_this = weak_factory_.GetWeakPtr();
    std::vector<DownloadRequestLimiter::Callback> callbacks;
    callbacks.swap(pending_callbacks_);
    for (auto& cb : callbacks) {
      if (cb) {
        std::move(cb).Run(allow);
      }
      if (!weak_this) {
        return;
      }
    }
  }

 private:
  enum class DialogAction {
    kDefault,
    kAutoConfirm,
    kAutoReject,
    kProgrammatic,
  };

  static std::map<void*, DownloadPromptHolder*>& ActiveHoldersMap() {
    static auto* map = new std::map<void*, DownloadPromptHolder*>();
    return *map;
  }

  static void RegisterActiveHolder(content::WebContents* wc,
                                   DownloadPromptHolder* holder) {
    ActiveHoldersMap()[static_cast<void*>(wc)] = holder;
  }

  static void UnregisterActiveHolder(content::WebContents* wc) {
    ActiveHoldersMap().erase(static_cast<void*>(wc));
  }

  static DownloadPromptHolder* GetActiveHolder(content::WebContents* wc) {
    auto it = ActiveHoldersMap().find(static_cast<void*>(wc));
    return it != ActiveHoldersMap().end() ? it->second : nullptr;
  }

  // Mirrors TabDownloadState::SetContentSetting
  // (download_request_limiter.cc). Persists the user's choice so subsequent
  // downloads skip the prompt.
  void SetContentSettingExt(ContentSetting setting) {
    if (!web_contents_ || request_origin_.opaque()) {
      return;
    }
    HostContentSettingsMap* settings =
        HostContentSettingsMapFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
    if (!settings) {
      return;
    }
    content_settings::ContentSettingConstraints constraints;
    settings->SetContentSettingDefaultScope(
        request_origin_.GetURL(), GURL(), ContentSettingsType::AUTOMATIC_DOWNLOADS,
        setting, constraints);
    // Immediately read back to verify the write took effect.
    ContentSetting read_back = settings->GetContentSetting(
        request_origin_.GetURL(), request_origin_.GetURL(),
        ContentSettingsType::AUTOMATIC_DOWNLOADS);
  }

  raw_ptr<content::WebContents> web_contents_;
  url::Origin request_origin_;

  // Callbacks from all downloads that need the prompt result. The first
  // download's callback is set in the constructor; subsequent downloads add
  // via TryQueueCallback(). All are dispatched with the same result when the
  // user responds.
  std::vector<DownloadRequestLimiter::Callback> pending_callbacks_;

  // User's decision from the prompt. Once set, subsequent downloads are
  // dispatched immediately without re-prompting. The holder stays alive
  // until WebContents is destroyed (page refresh / tab close).
  enum UserDecision { kNoDecision, kAllowed, kBlocked };
  UserDecision user_decision_ = kNoDecision;
  bool has_decision_ = false;

  // True while the OHOS dialog is showing and waiting for user response.
  // Used to distinguish "prompt pending" from "no prompt" in Show().
  bool prompt_in_flight_ = false;

  base::WeakPtrFactory<DownloadPromptHolder> weak_factory_{this};
};

}  // namespace

void ShowDownloadPermissionPrompt(content::WebContents* web_contents,
                                  const url::Origin& request_origin,
                                  DownloadRequestLimiter::Callback callback) {
  auto holder = std::make_unique<DownloadPromptHolder>(
      web_contents, request_origin, std::move(callback));
  DownloadPromptHolder::Show(std::move(holder));
}

}  // namespace ohos

#endif  // BUILDFLAG(ARKWEB_EXT_DOWNLOAD)
