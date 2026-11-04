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

// This file is included inline by ax_object.cc via #include.
// It contains ArkWeb-specific CSS property serialization for OHOS
// accessibility consumption. Decoupled from the main Serialize() method
// to keep ax_object.cc clean and maintainable.

#include "arkweb/build/features/features.h"

#if BUILDFLAG(ARKWEB_ACCESSIBILITY)

#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/basic_shapes.h"
#include "url/gurl.h"

namespace blink {
// ArkWeb: Serialize kValue for non-atomic text fields (contenteditable).
// For non-atomic text fields, we compute the value from DOM inner text
// (which skips placeholder <br>) and serialize it explicitly, even when
// empty. Without kValue present, the browser falls back to computing from
// AX child nodes, which includes a kLineBreak for the placeholder <br>,
// producing a phantom "\n".
// Truncation logic is inlined here because TruncateAndAddStringAttribute
// lives in an anonymous namespace inside ax_object.cc that is not visible
// after the closing of namespace blink.
void AXObject::SerializeArkWebValueAttribute(
    ui::AXNodeData* node_data) const {
  if (IsNonAtomicTextField()) {
    String value = SlowGetValueForControlIncludingContentEditable();
    if (value.empty()) {
      node_data->AddStringAttribute(
          ax::mojom::blink::StringAttribute::kValue, std::string());
    } else {
      std::string value_utf8 = value.Utf8(Utf8ConversionMode::kStrict);
      constexpr uint32_t kMaxLen = 10000;
      if (value_utf8.size() > kMaxLen) {
        std::string truncated;
        base::TruncateUTF8ToByteSize(value_utf8, kMaxLen, &truncated);
        value_utf8 = truncated;
      }
      if (!value_utf8.empty()) {
        node_data->AddStringAttribute(
            ax::mojom::blink::StringAttribute::kValue, value_utf8);
      }
    }
  } else {
    String value = GetValueForControl();
    if (!value.empty()) {
      std::string value_utf8 = value.Utf8(Utf8ConversionMode::kStrict);
      constexpr uint32_t kMaxLen = 10000;
      if (value_utf8.size() > kMaxLen) {
        std::string truncated;
        base::TruncateUTF8ToByteSize(value_utf8, kMaxLen, &truncated);
        value_utf8 = truncated;
      }
      if (!value_utf8.empty()) {
        node_data->AddStringAttribute(
            ax::mojom::blink::StringAttribute::kValue, value_utf8);
      }
    }
  }
}

bool AXObject::OnArkWebReplaceSelectedTextAction(const String& text) {
  if (!IsTextField()) {
    return false;
  }

  Document* document = GetDocument();
  if (!document) {
    return false;
  }

  LocalFrame* frame = document->GetFrame();
  if (!frame) {
    return false;
  }

  // Ensure the element has focus so the frame selection is within it.
  // ReplaceSelectionWithText operates on the frame selection, not on |element|,
  // so the caret must be inside the target element first.
  Element* element = GetElement();
  if (!element) {
    // Non-Element text field: cannot position the caret, abort to avoid
    // inserting text at an unrelated frame-selection location.
    return false;
  }
  if (!element->IsFocusedElementInDocument()) {
    element->Focus(FocusParams(SelectionBehaviorOnFocus::kNone,
                               mojom::blink::FocusType::kNone, nullptr));
    // element->Focus() may run synchronous script (focus event) that can
    // detach the document/frame. Re-fetch and re-validate before continuing.
    if (document != GetDocument()) {
      return false;
    }
    frame = document->GetFrame();
    if (!frame) {
      return false;
    }
    // kNone leaves the frame selection untouched; explicitly move the caret
    // to the end of the target element so ReplaceSelectionWithText inserts
    // here.
    frame->Selection().SetSelection(
        SelectionInDOMTree::Builder()
            .Collapse(Position::LastPositionInOrAfterNode(*element))
            .Build(),
        SetSelectionOptions::Builder()
            .SetShouldCloseTyping(true)
            .SetShouldClearTypingStyle(true)
            .SetDoNotSetFocus(true)
            .Build());
  }

  Editor& editor = frame->GetEditor();
  editor.ReplaceSelectionWithText(
      text, /*select_replacement=*/false, /*smart_replace=*/false,
      InputEvent::InputType::kInsertText);
  // ReplaceSelectionWithText may run synchronous script (beforeinput/input
  // events). Do not touch |this| members after this point; just return.
  return true;
}

}  // namespace blink

#endif  // BUILDFLAG(ARKWEB_ACCESSIBILITY)
