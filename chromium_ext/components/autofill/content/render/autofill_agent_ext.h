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

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_EXT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_EXT_H_

#include <cstdint>
#include <optional>

#include "arkweb/build/features/features.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/strong_alias.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {
class WebNode;
class WebFormControlElement;
class WebFormElement;
}  // namespace blink

namespace autofill {

class PasswordAutofillAgent;
class PasswordGenerationAgent;
class AutofillAgent;

class AutofillAgentExt : public AutofillAgent {
 public:
  AutofillAgentExt(
      content::RenderFrame* render_frame,
      std::unique_ptr<PasswordAutofillAgent> password_autofill_agent,
      std::unique_ptr<PasswordGenerationAgent> password_generation_agent,
      blink::AssociatedInterfaceRegistry* registry);

  AutofillAgentExt(const AutofillAgentExt&) = delete;
  AutofillAgentExt& operator=(const AutofillAgentExt&) = delete;
  ~AutofillAgentExt() override;
  AutofillAgentExt* AsAutofillAgentExt() override { return this; }

#if BUILDFLAG(ARKWEB_PASSWORD_AUTOFILL)
  bool isAutofillEnabled() const;
#endif

#if BUILDFLAG(ARKWEB_PASSWORD_AUTOFILL)
  void ArkFillAccountSuggestion(const std::u16string& username,
                                const std::u16string& password);
  void OhFormControlElementClicked();
  void OhPreSuppressKeyboardIfNeed(const blink::WebNode& node);
#endif
#if BUILDFLAG(ARKWEB_UNITTESTS)
  PasswordAutofillAgent* GetPasswordAutofillAgent() {
    return password_autofill_agent_.get();
  }
#endif
#if BUILDFLAG(ARKWEB_AUTOFILL)
  void OhAutoFillEditableElementClicked(const blink::WebNode& node);
  bool OhAutoFillDidChangeScrollOffset();
  void OnAutofillFocusedElementChanged(
      const blink::WebElement& new_focused_element);
  bool MaybeDeferFocusedElementRequest(
      bool focused_node_was_last_clicked,
      bool restart_if_sent);
  bool MaybeDeferPositionRequest(
      const blink::WebElement& focused_element,
      bool focused_node_was_last_clicked);
  void OnAutofillMainFrameViewportRectangleChanged(
      const gfx::Rect& main_frame_viewport_rect);
  bool OnFocusedElementPositionMaybeChanged();
  void ClearPositionRequestState();
  void ResetPositionRequestState();
  bool FillFieldWithValue(FieldRendererId field_id,
                          const std::u16string& value);
  void SetIsNeedToCreatedPopup(bool is_need_to_created_popup) {
    is_need_to_created_popup_ = is_need_to_created_popup;
    if (!is_need_to_created_popup_) {
      autofill_scroll_timer_.Stop();
    }
  }
  void SetCreatedPopupTime(base::TimeTicks created_popup_time) {
    created_popup_time_ = created_popup_time;
  }

  enum class PositionRequestState {
    kIdle,
    kWaitingForPositionChange,
    kWaitingForStablePosition,
    kRequestDispatched,
  };

  struct PositionSnapshot {
    gfx::Rect field_bounds;
    gfx::Rect view_rect;
    gfx::Rect window_rect;
    std::optional<gfx::Rect> main_frame_viewport_rect;
  };

  bool IsAutofillPositionRequestPending() const;
  bool IsSameViewportGeometry(const PositionSnapshot& lhs,
                              const PositionSnapshot& rhs) const;
  bool IsPositionTrackingElement(
      const blink::WebElement& element) const;
  bool IsPasswordAutofillElement(const blink::WebElement& element);
  bool GetCurrentFocusedElement(blink::WebElement& element);
  bool TryDeferPositionRequest(
      const blink::WebElement& element,
      bool restart_if_sent);
  bool CapturePositionSnapshot(
      const blink::WebElement& element,
      PositionSnapshot& snapshot);
  bool StartPositionTracking(
      const blink::WebElement& element);
  void SchedulePositionCheck();
  void RunPositionCheck(uint32_t generation);

  base::OneShotTimer autofill_scroll_timer_;
  base::OneShotTimer autofill_position_check_timer_;
  FieldRendererId position_field_id_;
  PositionSnapshot position_snapshot_;
  std::optional<PositionSnapshot>
      last_stable_position_snapshot_;
  std::optional<gfx::Rect> main_frame_viewport_rect_;
  PositionRequestState position_request_state_ =
      PositionRequestState::kIdle;
  uint32_t position_generation_ = 0;
  int position_stable_check_count_ = 0;
  bool is_dispatching_position_request_ = false;
  bool is_need_to_created_popup_ = false;
  base::TimeTicks created_popup_time_ = base::TimeTicks::Now();

protected:
  void OnDestruct() override;
 
private:
  base::WeakPtrFactory<AutofillAgentExt> ext_weak_ptr_factory_{this};
#endif
};
}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_EXT_H_
