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

#include "arkweb/chromium_ext/chrome/browser/renderer_context_menu/link_to_text_menu_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/shared_highlighting/core/common/disabled_sites.h"
#include "components/shared_highlighting/core/common/fragment_directives_constants.h"
#include "components/shared_highlighting/core/common/fragment_directives_utils.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

LinkToTextMenuManager::LinkToTextMenuManager() = default;

LinkToTextMenuManager::~LinkToTextMenuManager() = default;

void LinkToTextMenuManager::SetLinkGenerationCompletedCallback(
    LinkGenerationCompletedCallback cb) {
  link_generation_completed_callback_ = std::move(cb);
}

void LinkToTextMenuManager::InitMenu(const content::ContextMenuParams& params,
                                      content::WebContents* web_contents) {
  open_from_new_selection_ = !params.selection_text.empty();
  generated_link_.reset();
  is_generation_complete_ = false;
  visible_ = false;
  annotation_type_ = params.annotation_type;
  web_contents_ = web_contents;

  if (!web_contents) {
    LOG(ERROR) << "InitMenu: FAIL web_contents=null";
    return;
  }

  raw_url_ = params.page_url;
  if (params.page_url.has_ref()) {
    url_ = shared_highlighting::RemoveFragmentSelectorDirectives(raw_url_);
  } else {
    url_ = params.page_url;
  }

  // --- Visible checks ---
  // V0: Scroll-to-text-fragment pref must be enabled.
  {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    if (profile) {
      PrefService* prefs = profile->GetPrefs();
      if (prefs && prefs->HasPrefPath(prefs::kScrollToTextFragmentEnabled) &&
          !prefs->GetBoolean(prefs::kScrollToTextFragmentEnabled)) {
        LOG(INFO) << "InitMenu: FAIL V0 pref disabled";
        return;
      }
    }
  }

  // V1: Page URL must be HTTP or HTTPS.
  if (!params.page_url.SchemeIsHTTPOrHTTPS()) {
    LOG(INFO) << "InitMenu: FAIL V1 scheme=" << params.page_url.scheme();
    return;
  }

  // V2: Editable context should not show this item.
  content::RenderFrameHost* focused_frame = web_contents->GetFocusedFrame();
  content::RenderFrameHostImpl* focused_rfh =
      focused_frame
          ? static_cast<content::RenderFrameHostImpl*>(focused_frame)
          : nullptr;
  if (focused_rfh && focused_rfh->has_focused_editable_element()) {
    LOG(INFO) << "InitMenu: FAIL V2 editable";
    return;
  }

  // It is possible that there is a new text selection on top of an annotation,
  // in which case, both `open_from_new_selection_` and
  // `annotation_type_.has_value()` are true. Consequently, a context menu for
  // new text selection is created.
  visible_ = true;
  if (open_from_new_selection_) {
    RequestLinkGeneration();
  }
}

void LinkToTextMenuManager::RequestLinkGeneration() {
  content::RenderFrameHost* main_frame =
      web_contents_ ? web_contents_->GetPrimaryMainFrame() : nullptr;
  if (!main_frame)
    return;

  // Check whether current url is blocklisted for link to text generation.
  // This check should happen before iframe check so that if both conditions
  // are present then blocklist error is logged.
  if (!shared_highlighting::ShouldOfferLinkToText(url_)) {
    LOG(INFO) << "RequestLinkGeneration: FAIL blocklist";
    return;
  }

  // Check whether the selected text is in an iframe.
  if (main_frame != web_contents_->GetFocusedFrame()) {
    LOG(INFO) << "RequestLinkGeneration: FAIL iframe";
    return;
  }

  StartLinkGenerationRequestWithTimeout();
}

void LinkToTextMenuManager::StartLinkGenerationRequestWithTimeout() {
  base::TimeDelta timeout_length_ms = base::Milliseconds(
      shared_highlighting::GetPreemptiveLinkGenTimeoutLengthMs());

  content::RenderFrameHost* focused_frame =
      web_contents_ ? web_contents_->GetFocusedFrame() : nullptr;
  content::RenderFrameHost* rfh =
      focused_frame ? focused_frame : web_contents_->GetPrimaryMainFrame();
  if (!rfh) {
    LOG(INFO) << "StartLinkGenerationRequestWithTimeout: FAIL no rfh";
    return;
  }

  LOG(INFO) << "StartLinkGenerationRequestWithTimeout: sending Mojo RequestSelector";
  remote_.reset();
  rfh->GetRemoteInterfaces()->GetInterface(remote_.BindNewPipeAndPassReceiver());

  remote_->RequestSelector(
      base::BindOnce(&LinkToTextMenuManager::OnRequestLinkGenerationCompleted,
                     weak_factory_.GetWeakPtr()));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LinkToTextMenuManager::Timeout,
                     weak_factory_.GetWeakPtr()),
      timeout_length_ms);
}

bool LinkToTextMenuManager::ShouldOfferLinkToText() const {
  // for blocklist
  return visible_;
}

bool LinkToTextMenuManager::HasGeneratedLink() const {
  return generated_link_.has_value();
}

std::string LinkToTextMenuManager::GetLinkUrl() const {
  if (generated_link_.has_value()) {
    return generated_link_.value();
  }
  return "";
}

bool LinkToTextMenuManager::IsOnAnnotation() const {
  return annotation_type_.has_value();
}

bool LinkToTextMenuManager::CanReshareLink() const {
  return visible_ &&
         annotation_type_.has_value() &&
         annotation_type_.value() ==
             blink::mojom::AnnotationType::kSharedHighlight &&
         !open_from_new_selection_;
}

void LinkToTextMenuManager::Timeout() {
  if (remote_.is_bound() && remote_.is_connected()) {
    if (is_generation_complete_)
      return;
    remote_->Cancel();
    remote_.reset();
  }
}

bool LinkToTextMenuManager::CopyLinkToText() {
  LOG(INFO) << "LinkToTextMenuManager::CopyLinkToText: ";
  if (open_from_new_selection_) {
    return ExecuteCopyLinkToText();
  }
  return ReshareLink();
}

bool LinkToTextMenuManager::RemoveLinkToText() {
  if (!annotation_type_.has_value() || !web_contents_) {
    return false;
  }
  // Mirrors Chromium's RemoveHighlights().
  switch (annotation_type_.value()) {
    case blink::mojom::AnnotationType::kSharedHighlight:
      web_contents_->GetPrimaryMainFrame()->ForEachRenderFrameHost(
          [](content::RenderFrameHost* rfh) {
            mojo::Remote<blink::mojom::TextFragmentReceiver> remote;
            rfh->GetRemoteInterfaces()->GetInterface(
                remote.BindNewPipeAndPassReceiver());
            remote->RemoveFragments();
          });
      return true;
    case blink::mojom::AnnotationType::kGlic: {
      mojo::Remote<blink::mojom::AnnotationAgentContainer> container;
      web_contents_->GetPrimaryMainFrame()->GetRemoteInterfaces()->GetInterface(
          container.BindNewPipeAndPassReceiver());
      container->RemoveAgentsOfType(
          blink::mojom::AnnotationType::kGlic);
      return true;
    }
    default:
      return false;
  }
}

void LinkToTextMenuManager::OnRequestLinkGenerationCompleted(
    const std::string& selector,
    shared_highlighting::LinkGenerationError error,
    shared_highlighting::LinkGenerationReadyStatus ready_status) {
  is_generation_complete_ = true;

  LOG(INFO) << "OnRequestLinkGenerationCompleted: selector_len="
            << selector.size() << " error=" << static_cast<int>(error)
            << " ready_status=" << static_cast<int>(ready_status);

  if (!selector.empty() &&
      error == shared_highlighting::LinkGenerationError::kNone) {
    generated_link_ =
        shared_highlighting::AppendSelectors(url_, {selector}).spec();
  }

  if (link_generation_completed_callback_) {
    link_generation_completed_callback_.Run(generated_link_.has_value());
  }
}

bool LinkToTextMenuManager::ExecuteCopyLinkToText() {
  LOG(INFO) << "ExecuteCopyLinkToText: ";
  if (!generated_link_.has_value()) {
    LOG(INFO) << "ExecuteCopyLinkToText: link not ready";
    return false;
  }
  CopyTextToClipboard(generated_link_.value());
  return true;
}

bool LinkToTextMenuManager::ReshareLink() {
  if (!annotation_type_.has_value() ||
      annotation_type_.value() !=
          blink::mojom::AnnotationType::kSharedHighlight ||
      !web_contents_) {
    return false;
  }

  content::RenderFrameHost* main_frame =
      web_contents_->GetPrimaryMainFrame();
  if (!main_frame) {
    return false;
  }

  // Clear previous state.
  render_frame_host_ids_.clear();
  text_fragment_remotes_.clear();
  frames_selectors_.clear();

  // Collect all frame IDs and set up remotes.
  main_frame->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* rfh) {
        render_frame_host_ids_.push_back(rfh->GetGlobalId());
        mojo::Remote<blink::mojom::TextFragmentReceiver> remote;
        text_fragment_remotes_.push_back(std::move(remote));
      });

  get_frames_existing_selectors_counter_ = render_frame_host_ids_.size();

  if (get_frames_existing_selectors_counter_ == 0) {
    return false;
  }

  for (size_t i = 0; i < render_frame_host_ids_.size(); i++) {
    content::GlobalRenderFrameHostId rfh_id = render_frame_host_ids_[i];
    GetExistingSelectorsForReshareLink(rfh_id, i);
  }
  return true;
}

void LinkToTextMenuManager::GetExistingSelectorsForReshareLink(
    content::GlobalRenderFrameHostId rfh_id,
    size_t i) {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(rfh_id);
  if (!rfh) {
    get_frames_existing_selectors_counter_--;
    if (get_frames_existing_selectors_counter_ == 0) {
      OnGetExistingSelectorsComplete({});
    }
    return;
  }
  rfh->GetRemoteInterfaces()->GetInterface(
      text_fragment_remotes_[i].BindNewPipeAndPassReceiver());

  text_fragment_remotes_[i]->GetExistingSelectors(
      base::BindOnce(
          [](const base::WeakPtr<LinkToTextMenuManager>& manager_ptr,
             const content::GlobalRenderFrameHostId rfh_id,
             const std::vector<std::string>& selectors) {
            if (!manager_ptr) {
              return;
            }
            if (!selectors.empty()) {
              manager_ptr->frames_selectors_.insert({rfh_id, selectors});
            }
            manager_ptr->get_frames_existing_selectors_counter_--;
            if (manager_ptr->get_frames_existing_selectors_counter_ == 0) {
              // Aggregate selectors in frame order.
              std::vector<std::string> aggregated;
              for (const auto& id : manager_ptr->render_frame_host_ids_) {
                auto it = manager_ptr->frames_selectors_.find(id);
                if (it != manager_ptr->frames_selectors_.end()) {
                  aggregated.insert(aggregated.end(),
                                    it->second.begin(),
                                    it->second.end());
                }
              }
              manager_ptr->OnGetExistingSelectorsComplete(aggregated);
            }
          },
          weak_factory_.GetWeakPtr(), rfh_id));
}

void LinkToTextMenuManager::OnGetExistingSelectorsComplete(
    const std::vector<std::string>& aggregated_selectors) {
  GURL url_to_share =
      shared_highlighting::RemoveFragmentSelectorDirectives(url_);
  url_to_share =
      shared_highlighting::AppendSelectors(url_to_share, aggregated_selectors);
  if (url_to_share.is_valid()) {
    CopyTextToClipboard(url_to_share.spec());
  }
}

void LinkToTextMenuManager::CopyTextToClipboard(const std::string& text) {
  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
  scw.WriteText(base::UTF8ToUTF16(text));
}
