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
#include "arkweb/chromium_ext/components/autofill/content/render/autofill_agent_ext.h"

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_autofill_state.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_settings.h"
// Follow-up Processing: need check
#define INSIDE_BLINK 1
#include "third_party/blink/renderer/core/frame/settings.h"

#if BUILDFLAG(ARKWEB_PASSWORD_AUTOFILL)
#include "arkweb/chromium_ext/base/ohos/sys_info_utils_ext.h"
#endif

using blink::WebAutofillState;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;
using blink::WebNode;

namespace autofill {
#if BUILDFLAG(ARKWEB_AUTOFILL)
constexpr base::TimeDelta kWaitTimeForScrollIntoViewMs =
    base::Milliseconds(700);
constexpr base::TimeDelta kWaitTimeForRequestAutoFillMs =
    base::Milliseconds(50);
constexpr base::TimeDelta kAutofillPositionCheckInterval =
    base::Milliseconds(20);
constexpr int kAutofillPositionStableCheckCount = 3;
constexpr int kAutofillPositionNoChangeCheckCount = 9;
#endif

AutofillAgentExt::AutofillAgentExt(
    content::RenderFrame* render_frame,
    std::unique_ptr<PasswordAutofillAgent> password_autofill_agent,
    std::unique_ptr<PasswordGenerationAgent> password_generation_agent,
    blink::AssociatedInterfaceRegistry* registry)
    : AutofillAgent(render_frame,
                    std::move(password_autofill_agent),
                    std::move(password_generation_agent),
                    registry) {}

AutofillAgentExt::~AutofillAgentExt() {}

#if BUILDFLAG(ARKWEB_PASSWORD_AUTOFILL)
void AutofillAgentExt::ArkFillAccountSuggestion(
    const std::u16string& username,
    const std::u16string& password) {
  WebFormControlElement last_queried_element = last_queried_element_.GetField();
  if (!last_queried_element) {
    return;
  }

  bool handled =
      password_autofill_agent_->AsPasswordAutofillAgentExt()
          ->FillAccountSuggestion(last_queried_element, username, password);
  DCHECK(handled);
}
#endif

#if BUILDFLAG(ARKWEB_PASSWORD_AUTOFILL)
// LCOV_EXCL_START
void AutofillAgentExt::OhFormControlElementClicked() {
  if (!isAutofillEnabled()) {
    LOG(INFO) << "[PassWord Autofill] autofill interception successful.";
    return;
  }
  WebElement focused_element =
      unsafe_render_frame()->GetWebFrame()->GetDocument().FocusedElement();
  if (!focused_element.IsNull() && focused_element.IsFormControlElement()) {
    WebFormControlElement focused_form_control_element =
        focused_element.To<WebFormControlElement>();
    if (form_util::IsTextAreaElementOrTextInput(focused_form_control_element)) {
      LOG(INFO) << "[Autofill] Mouse down triggers RequestAutofill";
      bool result = password_autofill_agent_->AsPasswordAutofillAgentExt()
                        ->RequestAutofill(focused_form_control_element);
      if (result && base::ohos::IsPcDevice()) {
        is_popup_possibly_visible_ = true;
      }
    }
  }
}

void AutofillAgentExt::OhPreSuppressKeyboardIfNeed(const blink::WebNode& node) {
  if (!isAutofillEnabled()) {
    return;
  }
  WebElement focused_element = node.To<WebElement>();
  if (!focused_element.IsNull() && focused_element.IsFormControlElement()) {
    WebFormControlElement focused_form_control_element =
        focused_element.To<WebFormControlElement>();
    if (form_util::IsTextAreaElementOrTextInput(focused_form_control_element)) {
      password_autofill_agent_->AsPasswordAutofillAgentExt()
          ->PreSuppressKeyboardIfNeed(focused_form_control_element);
    }
  }
}
// LCOV_EXCL_STOP
#endif

#if BUILDFLAG(ARKWEB_AUTOFILL)
void AutofillAgentExt::OhAutoFillEditableElementClicked(
    const WebNode& node) {
  WebElement element = node.DynamicTo<WebElement>();
  if (!IsPositionTrackingElement(element)) {
    element = node.RootEditableElement();
  }
  if (!IsPositionTrackingElement(element) || !element.Focused()) {
    return;
  }
  if (IsPasswordAutofillElement(element)) {
    return;
  }
  if (MaybeDeferFocusedElementRequest(
          /*focused_node_was_last_clicked=*/true,
          /*restart_if_sent=*/true)) {
    return;
  }
  HidePopup();
  const SynchronousFormCache form_cache;
  HandleFocusChangeComplete(/*focused_node_was_last_clicked=*/true, form_cache);
  is_need_to_created_popup_ = true;
  created_popup_time_ = base::TimeTicks::Now();
}
#endif

#if BUILDFLAG(ARKWEB_AUTOFILL)
// LCOV_EXCL_START
bool AutofillAgentExt::OhAutoFillDidChangeScrollOffset() {
  if (IsAutofillPositionRequestPending()) {
    OnFocusedElementPositionMaybeChanged();
    return true;
  }

  WebElement last_queried_element = last_queried_element_.GetField();
  if (!last_queried_element) {
    last_queried_element = last_queried_element_.GetContentEditable();
  }
  if (!IsPositionTrackingElement(last_queried_element)) {
    return false;
  }
  if (is_need_to_created_popup_ &&
      !IsPasswordAutofillElement(last_queried_element) &&
      base::TimeTicks::Now() > created_popup_time_ &&
      base::TimeTicks::Now() <
          created_popup_time_ + kWaitTimeForScrollIntoViewMs) {
    if (autofill_scroll_timer_.IsRunning()) {
      autofill_scroll_timer_.Stop();
    }
    autofill_scroll_timer_.Start(FROM_HERE, kWaitTimeForRequestAutoFillMs,
                                 base::BindOnce(
                                     [](base::WeakPtr<AutofillAgent> self) {
                                       if (!self) {
                                         return;
                                       }
                                       self->HidePopup();
                                       const SynchronousFormCache form_cache;
                                       self->HandleFocusChangeComplete(true, form_cache);
                                     },
                                     weak_ptr_factory_.GetWeakPtr()));
    return true;
  }
  return false;
}
// LCOV_EXCL_STOP
#endif

#if BUILDFLAG(ARKWEB_AUTOFILL)
void AutofillAgentExt::OnDestruct() {
  ext_weak_ptr_factory_.InvalidateWeakPtrs();
  AutofillAgent::OnDestruct();
}

void AutofillAgentExt::OnAutofillFocusedElementChanged(
    const WebElement& new_focused_element) {
  if (position_request_state_ == PositionRequestState::kIdle) {
    return;
  }

  if (!IsPositionTrackingElement(new_focused_element) ||
      IsPasswordAutofillElement(new_focused_element) ||
      form_util::GetFieldRendererId(new_focused_element) !=
          position_field_id_) {
    ClearPositionRequestState();
  }
}

bool AutofillAgentExt::IsAutofillPositionRequestPending() const {
  return position_request_state_ ==
             PositionRequestState::kWaitingForPositionChange ||
         position_request_state_ ==
             PositionRequestState::kWaitingForStablePosition;
}

bool AutofillAgentExt::IsSameViewportGeometry(
    const PositionSnapshot& lhs,
    const PositionSnapshot& rhs) const {
  return lhs.view_rect == rhs.view_rect && lhs.window_rect == rhs.window_rect &&
         lhs.main_frame_viewport_rect == rhs.main_frame_viewport_rect;
}

bool AutofillAgentExt::IsPositionTrackingElement(
    const WebElement& element) const {
  if (element.IsNull()) {
    return false;
  }
  WebFormControlElement form_control =
      element.DynamicTo<WebFormControlElement>();
  if (form_util::IsTextAreaElementOrTextInput(form_control)) {
    return true;
  }
  return element.IsContentEditable() &&
         !element.DynamicTo<WebFormElement>() &&
         element == element.RootEditableElement();
}

bool AutofillAgentExt::IsPasswordAutofillElement(
    const WebElement& element) {
  if (element.IsNull()) {
    return false;
  }
  WebInputElement input_element = element.DynamicTo<WebInputElement>();
  return !input_element.IsNull() &&
         password_autofill_agent_->AsPasswordAutofillAgentExt()
             ->IsPasswordAutofill(input_element);
}

bool AutofillAgentExt::MaybeDeferFocusedElementRequest(
    bool focused_node_was_last_clicked,
    bool restart_if_sent) {
  if (!focused_node_was_last_clicked) {
    return false;
  }

  WebElement element;
  if (!GetCurrentFocusedElement(element)) {
    return false;
  }
  return TryDeferPositionRequest(element, restart_if_sent);
}

bool AutofillAgentExt::TryDeferPositionRequest(
    const WebElement& element,
    bool restart_if_sent) {
  if (!IsPositionTrackingElement(element)) {
    return false;
  }
  if (IsPasswordAutofillElement(element)) {
    return false;
  }
  const FieldRendererId field_id = form_util::GetFieldRendererId(element);
  if (field_id == FieldRendererId()) {
    return false;
  }

  if (field_id == position_field_id_) {
    if (IsAutofillPositionRequestPending()) {
      return true;
    }
    if (position_request_state_ ==
        PositionRequestState::kRequestDispatched) {
      if (!restart_if_sent) {
        return true;
      }
      HidePopup();
    }
  } else {
    ClearPositionRequestState();
  }

  if (StartPositionTracking(element)) {
    return true;
  }

  // The caller falls back to the original synchronous request.
  position_field_id_ = field_id;
  position_request_state_ =
      PositionRequestState::kRequestDispatched;
  return false;
}

bool AutofillAgentExt::MaybeDeferPositionRequest(
    const WebElement& focused_element,
    bool focused_node_was_last_clicked) {
  if (is_dispatching_position_request_ ||
      !focused_node_was_last_clicked) {
    return false;
  }

  return TryDeferPositionRequest(focused_element,
                                 /*restart_if_sent=*/false);
}

bool AutofillAgentExt::GetCurrentFocusedElement(WebElement& element) {
  WebDocument document = GetDocument();
  if (!document) {
    return false;
  }
  element = document.FocusedElement();
  return IsPositionTrackingElement(element);
}

bool AutofillAgentExt::CapturePositionSnapshot(
    const WebElement& element,
    PositionSnapshot& snapshot) {
  auto* render_frame = unsafe_render_frame();
  if (element.IsNull() || !render_frame || !render_frame->GetWebFrame()) {
    return false;
  }
  blink::WebLocalFrame* local_root = render_frame->GetWebFrame()->LocalRoot();
  blink::WebFrameWidget* widget =
      local_root ? local_root->FrameWidget() : nullptr;
  if (!widget) {
    return false;
  }
  snapshot.field_bounds = element.BoundsInWidget();
  snapshot.view_rect = widget->ViewRect();
  snapshot.window_rect = widget->WindowRect();
  snapshot.main_frame_viewport_rect = main_frame_viewport_rect_;
  return true;
}

bool AutofillAgentExt::StartPositionTracking(
    const WebElement& element) {
  PositionSnapshot snapshot;
  if (!CapturePositionSnapshot(element, snapshot)) {
    LOG(WARNING) << "[Autofill] Failed to start position request, field_id="
                 << form_util::GetFieldRendererId(element).value();
    ClearPositionRequestState();
    return false;
  }

  autofill_scroll_timer_.Stop();
  is_need_to_created_popup_ = false;
  position_field_id_ = form_util::GetFieldRendererId(element);
  position_snapshot_ = snapshot;
  position_stable_check_count_ = 0;
  position_request_state_ =
      last_stable_position_snapshot_ &&
              IsSameViewportGeometry(snapshot,
                                     *last_stable_position_snapshot_)
          ? PositionRequestState::kWaitingForStablePosition
          : PositionRequestState::kWaitingForPositionChange;
  LOG(INFO) << "[Autofill] Position request started, field_id="
            << position_field_id_.value() << ", state="
            << (position_request_state_ ==
                        PositionRequestState::kWaitingForStablePosition
                    ? "waiting_for_stable"
                    : "waiting_for_change");
  SchedulePositionCheck();
  return true;
}

void AutofillAgentExt::ClearPositionRequestState() {
  autofill_scroll_timer_.Stop();
  autofill_position_check_timer_.Stop();
  is_need_to_created_popup_ = false;
  position_field_id_ = FieldRendererId();
  position_snapshot_ = PositionSnapshot();
  position_stable_check_count_ = 0;
  position_request_state_ = PositionRequestState::kIdle;
  ++position_generation_;
}

void AutofillAgentExt::ResetPositionRequestState() {
  ClearPositionRequestState();
  last_stable_position_snapshot_.reset();
  main_frame_viewport_rect_.reset();
}

void AutofillAgentExt::OnAutofillMainFrameViewportRectangleChanged(
    const gfx::Rect& main_frame_viewport_rect) {
  if (main_frame_viewport_rect_.has_value() &&
      main_frame_viewport_rect_.value() == main_frame_viewport_rect) {
    return;
  }
  main_frame_viewport_rect_ = main_frame_viewport_rect;
  OnFocusedElementPositionMaybeChanged();
}

bool AutofillAgentExt::OnFocusedElementPositionMaybeChanged() {
  if (!IsAutofillPositionRequestPending()) {
    return false;
  }

  WebElement element;
  if (!GetCurrentFocusedElement(element) ||
      IsPasswordAutofillElement(element) ||
      form_util::GetFieldRendererId(element) !=
          position_field_id_) {
    ClearPositionRequestState();
    return false;
  }

  PositionSnapshot snapshot;
  if (!CapturePositionSnapshot(element, snapshot)) {
    ClearPositionRequestState();
    return false;
  }

  if (snapshot.field_bounds == position_snapshot_.field_bounds &&
      snapshot.view_rect == position_snapshot_.view_rect &&
      snapshot.window_rect == position_snapshot_.window_rect &&
      snapshot.main_frame_viewport_rect ==
          position_snapshot_.main_frame_viewport_rect) {
    return false;
  }

  position_snapshot_ = snapshot;
  position_stable_check_count_ = 0;
  position_request_state_ =
      PositionRequestState::kWaitingForStablePosition;
  if (!autofill_position_check_timer_.IsRunning()) {
    SchedulePositionCheck();
  }
  return true;
}

void AutofillAgentExt::SchedulePositionCheck() {
  ++position_generation_;
  const uint32_t generation = position_generation_;
  autofill_position_check_timer_.Start(
      FROM_HERE, kAutofillPositionCheckInterval,
      base::BindOnce(
          [](base::WeakPtr<AutofillAgent> self, uint32_t generation) {
            if (!self) {
              return;
            }
            self->AsAutofillAgentExt()->RunPositionCheck(generation);
          },
          ext_weak_ptr_factory_.GetWeakPtr(), generation));
}

void AutofillAgentExt::RunPositionCheck(uint32_t generation) {
  if (generation != position_generation_ ||
      !IsAutofillPositionRequestPending()) {
    return;
  }

  WebElement element;
  if (!GetCurrentFocusedElement(element) ||
      IsPasswordAutofillElement(element) ||
      form_util::GetFieldRendererId(element) !=
          position_field_id_) {
    ClearPositionRequestState();
    return;
  }

  PositionSnapshot snapshot;
  if (!CapturePositionSnapshot(element, snapshot)) {
    ClearPositionRequestState();
    return;
  }

  if (snapshot.field_bounds != position_snapshot_.field_bounds ||
      snapshot.view_rect != position_snapshot_.view_rect ||
      snapshot.window_rect != position_snapshot_.window_rect ||
      snapshot.main_frame_viewport_rect !=
          position_snapshot_.main_frame_viewport_rect) {
    position_snapshot_ = snapshot;
    position_stable_check_count_ = 0;
    position_request_state_ =
        PositionRequestState::kWaitingForStablePosition;
    SchedulePositionCheck();
    return;
  }

  ++position_stable_check_count_;
  const int required_check_count =
      position_request_state_ ==
              PositionRequestState::kWaitingForPositionChange
          ? kAutofillPositionNoChangeCheckCount
          : kAutofillPositionStableCheckCount;
  if (position_stable_check_count_ < required_check_count) {
    SchedulePositionCheck();
    return;
  }

  const bool dispatched_without_position_change =
      position_request_state_ ==
          PositionRequestState::kWaitingForPositionChange;
  position_request_state_ =
      PositionRequestState::kRequestDispatched;
  autofill_position_check_timer_.Stop();
  ++position_generation_;
  is_need_to_created_popup_ = false;
  last_stable_position_snapshot_ = snapshot;
  LOG(INFO) << "[Autofill] Position request dispatched, field_id="
            << position_field_id_.value() << ", reason="
            << (dispatched_without_position_change ? "no_change_stable"
                                                   : "position_stable")
            << ", check_count=" << position_stable_check_count_;
  const SynchronousFormCache form_cache;
  base::AutoReset<bool> dispatching_autofill_position_request(
      &is_dispatching_position_request_, true);
  HandleFocusChangeComplete(/*focused_node_was_last_clicked=*/true, form_cache);
}
#endif

bool AutofillAgentExt::FillFieldWithValue(FieldRendererId field_id,
                                          const std::u16string& value) {
  LOG(INFO) << "FillFieldWithValue";
  WebFormControlElement last_queried_element = last_queried_element_.GetField();
  if (!last_queried_element) {
    return true;
  }

  if (field_id != form_util::GetFieldRendererId(last_queried_element)) {
    // oh supports fill multiple form fields, including unfocused
    // field
    WebFormElement form = last_queried_element.Form();
    if (form.IsNull()) {
      WebDocument document = GetDocument();
      if (document.IsNull()) {
        return true;
      }
      WebFormControlElement fillElement =
          form_util::GetFormControlByRendererId(field_id);
      if (fillElement.IsNull()) {
        return true;
      }
      DoFillFieldWithValue(value, fillElement, WebAutofillState::kAutofilled);
      return true;
    }
    for (WebFormControlElement& formElement : form.GetFormControlElements()) {
      if (formElement.IsNull() ||
          form_util::GetFieldRendererId(formElement) != field_id) {
        continue;
      }
      DoFillFieldWithValue(value, formElement, WebAutofillState::kAutofilled);
      break;
    }
    return true;
  }
  return false;
}

bool AutofillAgentExt::isAutofillEnabled() const {
  if (GetDocument() && GetDocument().GetFrame()) {
    auto webview = GetDocument().GetFrame()->View();
    if (webview && webview->GetSettings()) {
      bool autofill_enabled = webview->GetSettings()->GetEnableAutoFill();
      return autofill_enabled;
    }
  }
  LOG(INFO) << "[PassWord Autofill] Failed to get the autofill interception state.";
  return true;
}
}  // namespace autofill
