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
  void OhAutoFillFormControlElementClicked(const blink::WebNode& node);
  bool OhAutoFillDidChangeScrollOffset();
  void OnAutofillFocusedElementChanged(
      const blink::WebElement& new_focused_element);
  bool MaybeStartAutofillPositionRequestContext(
      bool focused_node_was_last_clicked,
      bool restart_if_sent);
  void OnAutofillMainFrameViewportRectangleChanged(
      const gfx::Rect& main_frame_viewport_rect);
  bool OnFocusedInputPositionMaybeChanged();
  void ClearAutofillPositionRequestContext();
  void ResetAutofillPositionRequestContext();
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

  enum class AutofillPositionRequestState {
    kIdle,
    kWaitingForPositionChange,
    kWaitingForStablePosition,
    kRequestDispatched,
  };

  struct AutofillPositionSnapshot {
    gfx::Rect field_bounds;
    gfx::Rect view_rect;
    gfx::Rect window_rect;
    std::optional<gfx::Rect> main_frame_viewport_rect;
  };

  bool IsAutofillPositionRequestPending() const;
  bool IsSameAutofillViewport(const AutofillPositionSnapshot& lhs,
                              const AutofillPositionSnapshot& rhs) const;
  bool GetCurrentFocusedInput(blink::WebInputElement& input_element);
  bool GetAutofillPositionSnapshot(const blink::WebInputElement& input_element,
                                   AutofillPositionSnapshot& snapshot);
  bool StartAutofillPositionRequestContext(
      const blink::WebInputElement& input_element);
  void ScheduleAutofillPositionCheck();
  void RunAutofillPositionCheck(uint32_t generation);

  base::OneShotTimer autofill_scroll_timer_;
  base::OneShotTimer autofill_position_check_timer_;
  FieldRendererId autofill_position_field_id_;
  AutofillPositionSnapshot autofill_position_snapshot_;
  std::optional<AutofillPositionSnapshot>
      last_stable_autofill_position_snapshot_;
  std::optional<gfx::Rect> main_frame_viewport_rect_;
  AutofillPositionRequestState autofill_position_request_state_ =
      AutofillPositionRequestState::kIdle;
  uint32_t autofill_position_generation_ = 0;
  int autofill_position_stable_check_count_ = 0;
  bool is_need_to_created_popup_ = false;
  base::TimeTicks created_popup_time_ = base::TimeTicks::Now();

  base::WeakPtrFactory<AutofillAgentExt> ext_weak_ptr_factory_{this};
#endif
};
}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_EXT_H_
