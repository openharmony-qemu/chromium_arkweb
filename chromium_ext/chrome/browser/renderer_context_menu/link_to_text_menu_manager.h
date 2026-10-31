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

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_LINK_TO_TEXT_MENU_MANAGER_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_LINK_TO_TEXT_MENU_MANAGER_H_

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"
#include "third_party/blink/public/mojom/link_to_text/link_to_text.mojom.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// LinkToTextMenuManager manages the "Copy Link to Selected Text" feature
// for the OH/NWeb context. It replicates the logic from Chromium's
// LinkToTextMenuObserver but without depending on RenderViewContextMenu.
//
// Lifecycle (mirrors LinkToTextMenuObserver):
//   1. Context menu opens -> InitMenu() is called with context menu params.
//      It stores state, determines visibility, and calls
//      RequestLinkGeneration() if there is a new text selection.
//   2. RequestLinkGeneration() checks blocklist and iframe conditions,
//      then starts async Mojo RequestSelector() via
//      StartLinkGenerationRequestWithTimeout().
//   3. Mojo callback OnRequestLinkGenerationCompleted() caches
//      generated_link_ and sets is_generation_complete_.
//   4. App queries state via ShouldOfferLinkToText/HasGeneratedLink/
//      IsOnAnnotation/CanReshareLink/GetLinkUrl.
//   5. App executes commands via CopyLinkToText/RemoveLinkToText.
class LinkToTextMenuManager {
 public:
  LinkToTextMenuManager();
  ~LinkToTextMenuManager();

  // Called at context menu open time. Mirrors Chromium's
  // LinkToTextMenuObserver::InitMenu(const content::ContextMenuParams&).
  // Stores state from context menu params, determines visibility, and calls
  // RequestLinkGeneration if there is a new text selection.
  void InitMenu(const content::ContextMenuParams& params,
                content::WebContents* web_contents);

  // --- State query methods (called by NWebContextMenuParamsImpl) ---

  // Returns true if the prerequisite conditions for offering link-to-text
  // are met (pref enabled, HTTP(S) scheme, not editable, and either a new
  // selection or an existing annotation). This is a factual condition
  // check, not a UI decision — the app decides what this means for
  // visibility/enabled state.
  bool ShouldOfferLinkToText() const;

  // Returns true if a link-to-text URL has been generated (the async Mojo
  // callback arrived with a valid selector).
  bool HasGeneratedLink() const;

  // Returns the link-to-text URL string (page URL with :~:text= fragment).
  // Returns empty string if the selector hasn't been generated yet.
  std::string GetLinkUrl() const;

  // Returns true if the user right-clicked on an existing annotation highlight
  // (kSharedHighlight, kGlic, etc.).
  bool IsOnAnnotation() const;

  // Returns true if reshare is possible: on a kSharedHighlight annotation
  // with no new selection, AND prerequisite conditions are met.
  bool CanReshareLink() const;

  // --- Command execution methods (called by NWebContextMenuCallbackImpl) ---

  // Copies the link-to-text URL to the clipboard. Dispatches internally:
  //   - open_from_new_selection_ -> ExecuteCopyLinkToText()
  //   - on existing highlight -> ReshareLink()
  bool CopyLinkToText();

  // Removes text fragment highlights from the page. Mirrors Chromium's
  // RemoveHighlights(). For kSharedHighlight, calls RemoveFragments() on
  // each frame's TextFragmentReceiver. For kGlic, calls
  // RemoveAgentsOfType(kGlic) on AnnotationAgentContainer.
  // Returns false if annotation_type_ is not set.
  bool RemoveLinkToText();

  // Callback invoked when link generation completes with a valid link.
  // Receives true when generated_link_ is populated, false otherwise.
  using LinkGenerationCompletedCallback =
      base::RepeatingCallback<void(bool has_link)>;
  void SetLinkGenerationCompletedCallback(LinkGenerationCompletedCallback cb);

 private:
  // Requests link generation if needed. Mirrors Chromium's
  // LinkToTextMenuObserver::RequestLinkGeneration.
  void RequestLinkGeneration();

  // Makes an async request to the renderer to generate the link to text.
  // Mirrors Chromium's StartLinkGenerationRequestWithTimeout.
  void StartLinkGenerationRequestWithTimeout();

  // Callback after the request to generate the selector has completed.
  // Mirrors Chromium's OnRequestLinkGenerationCompleted.
  void OnRequestLinkGenerationCompleted(
      const std::string& selector,
      shared_highlighting::LinkGenerationError error,
      shared_highlighting::LinkGenerationReadyStatus ready_status);

  // Called when "Copy Link to Text" option is selected from a context menu
  // for a selected text. Mirrors Chromium's ExecuteCopyLinkToText.
  bool ExecuteCopyLinkToText();

  // Makes a request to the renderer to retrieve the selector for an
  // existing highlight. Mirrors Chromium's ReshareLink.
  bool ReshareLink();

  // only for ReshareLink.
  void GetExistingSelectorsForReshareLink(content::GlobalRenderFrameHostId rfh_id,
                                          size_t i);
  // Callback after the request to retrieve an existing selector is complete.
  // Mirrors Chromium's OnGetExistingSelectorsComplete.
  void OnGetExistingSelectorsComplete(
      const std::vector<std::string>& aggregated_selectors);

  // Copies given text to clipboard. Mirrors Chromium's
  // CopyTextToClipboard.
  void CopyTextToClipboard(const std::string& text);

  // Cancels link generation if we are still waiting for it.
  void Timeout();

  // --- State (mirrors LinkToTextMenuObserver members) ---
  GURL url_;
  GURL raw_url_;
  bool open_from_new_selection_ = false;
  std::optional<std::string> generated_link_;
  bool is_generation_complete_ = false;
  std::optional<blink::mojom::AnnotationType> annotation_type_;
  bool visible_ = false;

  raw_ptr<content::WebContents> web_contents_ = nullptr;

  // --- Reshare async state (mirrors LinkToTextMenuObserver) ---
  std::unordered_map<content::GlobalRenderFrameHostId,
                     std::vector<std::string>,
                     content::GlobalRenderFrameHostIdHasher>
      frames_selectors_;

  size_t get_frames_existing_selectors_counter_ = 0;

  mojo::Remote<blink::mojom::TextFragmentReceiver> remote_;
  std::vector<mojo::Remote<blink::mojom::TextFragmentReceiver>>
      text_fragment_remotes_;
  LinkGenerationCompletedCallback link_generation_completed_callback_;
  
  std::vector<content::GlobalRenderFrameHostId> render_frame_host_ids_;
  base::WeakPtrFactory<LinkToTextMenuManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_LINK_TO_TEXT_MENU_MANAGER_H_
