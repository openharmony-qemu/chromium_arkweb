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
void AutofillAgentExt::OhAutoFillFormControlElementClicked(
    const WebNode& node) {
  WebInputElement input_element = node.DynamicTo<WebInputElement>();
  if (input_element.IsNull() || !node.Focused()) {
    return;
  }
  auto is_password_autofill =
      password_autofill_agent_->AsPasswordAutofillAgentExt()
          ->IsPasswordAutofill(input_element);
  if (is_password_autofill) {
    return;
  }
  if (MaybeStartAutofillPositionRequestContext(
          /*focused_node_was_last_clicked=*/true,
          /*restart_if_sent=*/true)) {
    return;
  }
  HidePopup();
  const SynchronousFormCache form_cache;
  HandleFocusChangeComplete(/*focused_node_was_last_clicked=*/node.Focused(), form_cache);
  is_need_to_created_popup_ = true;
  created_popup_time_ = base::TimeTicks::Now();
}
#endif

#if BUILDFLAG(ARKWEB_AUTOFILL)
// LCOV_EXCL_START
bool AutofillAgentExt::OhAutoFillDidChangeScrollOffset() {
  if (IsAutofillPositionRequestPending()) {
    OnFocusedInputPositionMaybeChanged();
    return true;
  }

  WebFormControlElement last_queried_element = last_queried_element_.GetField();
  if (!last_queried_element) {
    return false;
  }
  const WebInputElement input_element =
      last_queried_element.DynamicTo<WebInputElement>();
  if (input_element.IsNull()) {
    return false;
  }
  auto is_password_autofill =
      password_autofill_agent_->AsPasswordAutofillAgentExt()
          ->IsPasswordAutofill(input_element);
  if (is_need_to_created_popup_ && !is_password_autofill &&
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
void AutofillAgentExt::OnAutofillFocusedElementChanged(
    const WebElement& new_focused_element) {
  if (autofill_position_request_state_ == AutofillPositionRequestState::kIdle) {
    return;
  }

  WebInputElement input_element =
      new_focused_element.DynamicTo<WebInputElement>();
  if (input_element.IsNull() || form_util::GetFieldRendererId(input_element) !=
                                    autofill_position_field_id_) {
    ClearAutofillPositionRequestContext();
  }
}

bool AutofillAgentExt::IsAutofillPositionRequestPending() const {
  return autofill_position_request_state_ ==
             AutofillPositionRequestState::kWaitingForPositionChange ||
         autofill_position_request_state_ ==
             AutofillPositionRequestState::kWaitingForStablePosition;
}

bool AutofillAgentExt::IsSameAutofillViewport(
    const AutofillPositionSnapshot& lhs,
    const AutofillPositionSnapshot& rhs) const {
  return lhs.view_rect == rhs.view_rect && lhs.window_rect == rhs.window_rect &&
         lhs.main_frame_viewport_rect == rhs.main_frame_viewport_rect;
}

bool AutofillAgentExt::MaybeStartAutofillPositionRequestContext(
    bool focused_node_was_last_clicked,
    bool restart_if_sent) {
  if (!focused_node_was_last_clicked) {
    return false;
  }

  WebInputElement input_element;
  if (!GetCurrentFocusedInput(input_element) ||
      password_autofill_agent_->AsPasswordAutofillAgentExt()
          ->IsPasswordAutofill(input_element)) {
    return false;
  }

  const FieldRendererId field_id = form_util::GetFieldRendererId(input_element);
  if (field_id == FieldRendererId()) {
    return false;
  }

  if (field_id == autofill_position_field_id_) {
    if (IsAutofillPositionRequestPending()) {
      return true;
    }
    if (autofill_position_request_state_ ==
        AutofillPositionRequestState::kRequestDispatched) {
      if (!restart_if_sent) {
        return true;
      }
      HidePopup();
    }
  } else {
    ClearAutofillPositionRequestContext();
  }

  if (StartAutofillPositionRequestContext(input_element)) {
    return true;
  }

  // The caller falls back to the original synchronous request.
  autofill_position_field_id_ = field_id;
  autofill_position_request_state_ =
      AutofillPositionRequestState::kRequestDispatched;
  return false;
}

bool AutofillAgentExt::GetCurrentFocusedInput(WebInputElement& input_element) {
  WebDocument document = GetDocument();
  if (!document) {
    return false;
  }
  input_element = document.FocusedElement().DynamicTo<WebInputElement>();
  return !input_element.IsNull() && input_element.Focused();
}

bool AutofillAgentExt::GetAutofillPositionSnapshot(
    const WebInputElement& input_element,
    AutofillPositionSnapshot& snapshot) {
  auto* render_frame = unsafe_render_frame();
  if (input_element.IsNull() || !render_frame || !render_frame->GetWebFrame()) {
    return false;
  }
  blink::WebLocalFrame* local_root = render_frame->GetWebFrame()->LocalRoot();
  blink::WebFrameWidget* widget =
      local_root ? local_root->FrameWidget() : nullptr;
  if (!widget) {
    return false;
  }
  snapshot.field_bounds = input_element.BoundsInWidget();
  snapshot.view_rect = widget->ViewRect();
  snapshot.window_rect = widget->WindowRect();
  snapshot.main_frame_viewport_rect = main_frame_viewport_rect_;
  return true;
}

bool AutofillAgentExt::StartAutofillPositionRequestContext(
    const WebInputElement& input_element) {
  AutofillPositionSnapshot snapshot;
  if (!GetAutofillPositionSnapshot(input_element, snapshot)) {
    LOG(WARNING) << "[Autofill] Failed to start position request, field_id="
                 << form_util::GetFieldRendererId(input_element).value();
    ClearAutofillPositionRequestContext();
    return false;
  }

  autofill_scroll_timer_.Stop();
  is_need_to_created_popup_ = false;
  autofill_position_field_id_ = form_util::GetFieldRendererId(input_element);
  autofill_position_snapshot_ = snapshot;
  autofill_position_stable_check_count_ = 0;
  autofill_position_request_state_ =
      last_stable_autofill_position_snapshot_ &&
              IsSameAutofillViewport(snapshot,
                                     *last_stable_autofill_position_snapshot_)
          ? AutofillPositionRequestState::kWaitingForStablePosition
          : AutofillPositionRequestState::kWaitingForPositionChange;
  LOG(INFO) << "[Autofill] Position request started, field_id="
            << autofill_position_field_id_.value() << ", state="
            << (autofill_position_request_state_ ==
                        AutofillPositionRequestState::kWaitingForStablePosition
                    ? "waiting_for_stable"
                    : "waiting_for_change")
            << ", field_bounds=" << snapshot.field_bounds.ToString()
            << ", view_rect=" << snapshot.view_rect.ToString()
            << ", window_rect=" << snapshot.window_rect.ToString()
            << ", main_frame_viewport_rect="
            << (snapshot.main_frame_viewport_rect.has_value()
                    ? snapshot.main_frame_viewport_rect->ToString()
                    : "unset");
  ScheduleAutofillPositionCheck();
  return true;
}

void AutofillAgentExt::ClearAutofillPositionRequestContext() {
  autofill_scroll_timer_.Stop();
  autofill_position_check_timer_.Stop();
  is_need_to_created_popup_ = false;
  autofill_position_field_id_ = FieldRendererId();
  autofill_position_snapshot_ = AutofillPositionSnapshot();
  autofill_position_stable_check_count_ = 0;
  autofill_position_request_state_ = AutofillPositionRequestState::kIdle;
  ++autofill_position_generation_;
}

void AutofillAgentExt::ResetAutofillPositionRequestContext() {
  ClearAutofillPositionRequestContext();
  last_stable_autofill_position_snapshot_.reset();
  main_frame_viewport_rect_.reset();
}

void AutofillAgentExt::OnAutofillMainFrameViewportRectangleChanged(
    const gfx::Rect& main_frame_viewport_rect) {
  if (main_frame_viewport_rect_.has_value() &&
      main_frame_viewport_rect_.value() == main_frame_viewport_rect) {
    return;
  }
  main_frame_viewport_rect_ = main_frame_viewport_rect;
  OnFocusedInputPositionMaybeChanged();
}

bool AutofillAgentExt::OnFocusedInputPositionMaybeChanged() {
  if (!IsAutofillPositionRequestPending()) {
    return false;
  }

  WebInputElement input_element;
  if (!GetCurrentFocusedInput(input_element) ||
      form_util::GetFieldRendererId(input_element) !=
          autofill_position_field_id_ ||
      password_autofill_agent_->AsPasswordAutofillAgentExt()
          ->IsPasswordAutofill(input_element)) {
    ClearAutofillPositionRequestContext();
    return false;
  }

  AutofillPositionSnapshot snapshot;
  if (!GetAutofillPositionSnapshot(input_element, snapshot)) {
    ClearAutofillPositionRequestContext();
    return false;
  }

  if (snapshot.field_bounds == autofill_position_snapshot_.field_bounds &&
      snapshot.view_rect == autofill_position_snapshot_.view_rect &&
      snapshot.window_rect == autofill_position_snapshot_.window_rect &&
      snapshot.main_frame_viewport_rect ==
          autofill_position_snapshot_.main_frame_viewport_rect) {
    return false;
  }

  autofill_position_snapshot_ = snapshot;
  autofill_position_stable_check_count_ = 0;
  autofill_position_request_state_ =
      AutofillPositionRequestState::kWaitingForStablePosition;
  if (!autofill_position_check_timer_.IsRunning()) {
    ScheduleAutofillPositionCheck();
  }
  return true;
}

void AutofillAgentExt::ScheduleAutofillPositionCheck() {
  ++autofill_position_generation_;
  const uint32_t generation = autofill_position_generation_;
  autofill_position_check_timer_.Start(
      FROM_HERE, kAutofillPositionCheckInterval,
      base::BindOnce(
          [](base::WeakPtr<AutofillAgent> self, uint32_t generation) {
            if (!self) {
              return;
            }
            self->AsAutofillAgentExt()->RunAutofillPositionCheck(generation);
          },
          ext_weak_ptr_factory_.GetWeakPtr(), generation));
}

void AutofillAgentExt::RunAutofillPositionCheck(uint32_t generation) {
  if (generation != autofill_position_generation_ ||
      !IsAutofillPositionRequestPending()) {
    return;
  }

  WebInputElement input_element;
  if (!GetCurrentFocusedInput(input_element) ||
      form_util::GetFieldRendererId(input_element) !=
          autofill_position_field_id_ ||
      password_autofill_agent_->AsPasswordAutofillAgentExt()
          ->IsPasswordAutofill(input_element)) {
    ClearAutofillPositionRequestContext();
    return;
  }

  AutofillPositionSnapshot snapshot;
  if (!GetAutofillPositionSnapshot(input_element, snapshot)) {
    ClearAutofillPositionRequestContext();
    return;
  }

  if (snapshot.field_bounds != autofill_position_snapshot_.field_bounds ||
      snapshot.view_rect != autofill_position_snapshot_.view_rect ||
      snapshot.window_rect != autofill_position_snapshot_.window_rect ||
      snapshot.main_frame_viewport_rect !=
          autofill_position_snapshot_.main_frame_viewport_rect) {
    autofill_position_snapshot_ = snapshot;
    autofill_position_stable_check_count_ = 0;
    autofill_position_request_state_ =
        AutofillPositionRequestState::kWaitingForStablePosition;
    ScheduleAutofillPositionCheck();
    return;
  }

  ++autofill_position_stable_check_count_;
  const int required_check_count =
      autofill_position_request_state_ ==
              AutofillPositionRequestState::kWaitingForPositionChange
          ? kAutofillPositionNoChangeCheckCount
          : kAutofillPositionStableCheckCount;
  if (autofill_position_stable_check_count_ < required_check_count) {
    ScheduleAutofillPositionCheck();
    return;
  }

  const bool dispatched_without_position_change =
      autofill_position_request_state_ ==
          AutofillPositionRequestState::kWaitingForPositionChange;
  autofill_position_request_state_ =
      AutofillPositionRequestState::kRequestDispatched;
  autofill_position_check_timer_.Stop();
  ++autofill_position_generation_;
  is_need_to_created_popup_ = false;
  last_stable_autofill_position_snapshot_ = snapshot;
  LOG(INFO) << "[Autofill] Position request dispatched, field_id="
            << autofill_position_field_id_.value() << ", reason="
            << (dispatched_without_position_change ? "no_change_stable"
                                                   : "position_stable")
            << ", check_count=" << autofill_position_stable_check_count_
            << ", field_bounds=" << snapshot.field_bounds.ToString()
            << ", view_rect=" << snapshot.view_rect.ToString()
            << ", window_rect=" << snapshot.window_rect.ToString()
            << ", main_frame_viewport_rect="
            << (snapshot.main_frame_viewport_rect.has_value()
                    ? snapshot.main_frame_viewport_rect->ToString()
                    : "unset");
  const SynchronousFormCache form_cache;
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
