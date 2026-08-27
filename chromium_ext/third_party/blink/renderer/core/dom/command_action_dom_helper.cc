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

#include "arkweb/chromium_ext/third_party/blink/renderer/core/dom/command_action_dom_helper.h"

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)

#include <array>
#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/commands/typing_command.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "cc/trees/scroll_source_type.h"
#include "third_party/blink/renderer/core/xml/document_xpath_evaluator.h"
#include "third_party/blink/renderer/core/xml/xpath_result.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

using OHOS::NWeb::WebCommandErrorCode;

constexpr std::array<std::string_view, 13> kStringValueInputTypes = {
    "text", "password", "email", "url", "tel", "search", "number",
    "date", "datetime-local", "month", "range", "time", "week"};

struct ParsedElementId {
  std::string frame_token;
  std::string document_scope_token;
  int dom_node_id = 0;
};

std::string SerializeFrameToken(const FrameToken& token) {
  if (token.Is<LocalFrameToken>()) {
    return token.GetAs<LocalFrameToken>().ToString();
  }
  return token.GetAs<RemoteFrameToken>().ToString();
}

std::string NormalizeFrameToken(std::string_view token) {
  static constexpr std::string_view kLocalPrefix = "local:";
  static constexpr std::string_view kRemotePrefix = "remote:";
  if (token.substr(0, kLocalPrefix.size()) == kLocalPrefix) {
    return std::string(token.substr(kLocalPrefix.size()));
  }
  if (token.substr(0, kRemotePrefix.size()) == kRemotePrefix) {
    return std::string(token.substr(kRemotePrefix.size()));
  }
  return std::string(token);
}

std::string GetDocumentScopeToken(WebLocalFrame* frame) {
  if (!frame || !frame->GetEmbeddingToken()) {
    return std::string();
  }
  return frame->GetEmbeddingToken()->ToString();
}

std::optional<ParsedElementId> ParseElementId(std::string_view element_id) {
  size_t dom_separator = element_id.rfind('|');
  if (dom_separator == std::string_view::npos ||
      dom_separator + 1 >= element_id.size()) {
    return std::nullopt;
  }
  size_t document_separator = element_id.rfind('|', dom_separator - 1);
  if (document_separator == std::string_view::npos || document_separator == 0 ||
      document_separator + 1 == dom_separator) {
    return std::nullopt;
  }

  int dom_node_id = 0;
  if (!base::StringToInt(std::string(element_id.substr(dom_separator + 1)),
                         &dom_node_id) ||
      dom_node_id <= 0) {
    return std::nullopt;
  }

  ParsedElementId parsed;
  parsed.frame_token = std::string(element_id.substr(0, document_separator));
  parsed.document_scope_token = std::string(element_id.substr(
      document_separator + 1, dom_separator - document_separator - 1));
  parsed.dom_node_id = dom_node_id;
  return parsed;
}

void PushChildrenInReverse(Node* node, std::vector<Node*>* stack) {
  if (!node || !stack) {
    return;
  }
  for (Node* child = node->lastChild(); child;
       child = child->previousSibling()) {
    stack->push_back(child);
  }
}

Element* FindElementByDomNodeId(Document* document, int dom_node_id) {
  if (!document || dom_node_id <= 0) {
    return nullptr;
  }
  Element* root = document->documentElement();
  if (!root) {
    return nullptr;
  }

  std::vector<Node*> stack;
  stack.push_back(root);
  while (!stack.empty()) {
    Node* node = stack.back();
    stack.pop_back();
    if (!node) {
      continue;
    }
    if (node->GetDomNodeId() == dom_node_id) {
      return DynamicTo<Element>(node);
    }
    PushChildrenInReverse(node, &stack);
    if (Element* element = DynamicTo<Element>(node)) {
      if (Node* shadow_root = element->GetShadowRoot()) {
        stack.push_back(shadow_root);
      }
    }
  }
  return nullptr;
}

bool IsStringValueInputType(std::string_view type) {
  return base::Contains(kStringValueInputTypes, type);
}

}  // namespace

Element* CommandActionDOMHelper::FindElementByXPath(Document* document,
                                                     const std::string& xpath) {
  if (!document) {
    LOG(ERROR) << "[WebCommandAction] FindElementByXPath: document is null";
    return nullptr;
  }

  DummyExceptionStateForTesting exception_state;
  XPathResult* result = DocumentXPathEvaluator::evaluate(
      *document,
      String::FromUTF8(xpath),
      document,
      nullptr,  // namespace resolver
      XPathResult::kFirstOrderedNodeType,
      ScriptValue(),
      exception_state);
  if (exception_state.HadException() || !result) {
    return nullptr;
  }
  Node* node = result->singleNodeValue(exception_state);
  if (!node) {
    return nullptr;
  }
  return DynamicTo<Element>(node);
}

Element* CommandActionDOMHelper::FindElementById(WebLocalFrame* frame,
                                                  Document* document,
                                                  const std::string& id,
                                                  int32_t* error_code) {
  if (error_code) {
    *error_code = static_cast<int32_t>(WebCommandErrorCode::FAILED);
  }
  if (!frame || !document) {
    LOG(ERROR) << "[WebCommandAction] FindElementById: frame or document is null";
    if (error_code) {
      *error_code = static_cast<int32_t>(WebCommandErrorCode::PAGE_NOT_READY);
    }
    return nullptr;
  }

  std::optional<ParsedElementId> parsed = ParseElementId(id);
  if (!parsed) {
    if (error_code) {
      *error_code = static_cast<int32_t>(WebCommandErrorCode::PARAM_INVALID);
    }
    return nullptr;
  }

  std::string current_frame_token =
      NormalizeFrameToken(SerializeFrameToken(frame->GetFrameToken()));
  std::string current_document_scope_token = GetDocumentScopeToken(frame);
  if (NormalizeFrameToken(parsed->frame_token) != current_frame_token ||
      parsed->document_scope_token != current_document_scope_token) {
    if (error_code) {
      *error_code = static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND);
    }
    return nullptr;
  }

  Element* element = FindElementByDomNodeId(document, parsed->dom_node_id);
  if (!element) {
    if (error_code) {
      *error_code = static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND);
    }
    return nullptr;
  }
  if (error_code) {
    *error_code = static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
  }
  return element;
}


int32_t CommandActionDOMHelper::ExecuteInput(Document* document,
                                              const std::string& xpath,
                                              const std::string& value,
                                              const std::string& event_type) {
  Element* element = FindElementByXPath(document, xpath);
  if (!element) {
    LOG(ERROR) << "[WebCommandAction] ExecuteInput: element not found for xpath=" << xpath;
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND);
  }

  auto* input = DynamicTo<HTMLInputElement>(element);
  if (!input) {
    LOG(ERROR) << "[WebCommandAction] ExecuteInput: element is not an input element";
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH);
  }

  // Verify event_type matches element's actual type.
  // event_type format is guaranteed as "input-<type>" by ParseEventType's map.
  const std::string kPrefix = "input-";
  if (event_type.substr(kPrefix.size()) != input->type().Utf8()) {
    LOG(ERROR) << "[WebCommandAction] ExecuteInput: event type mismatch, expected="
               << input->type().Utf8() << ", got=" << event_type.substr(kPrefix.size());
    return static_cast<int32_t>(WebCommandErrorCode::INPUT_EVENT_TYPE_MISMATCH);
  }

  String value_str = String::FromUTF8(value);
  // SanitizeValue normalizes input (trim, remove line breaks, etc.).
  // For number-type inputs it returns empty for non-finite values.
  // Use IsValidValue for format validation; SanitizeValue is not a validation API.
  if (input->SanitizeValue(value_str).empty()) {
    LOG(ERROR) << "[WebCommandAction] ExecuteInput: invalid value for input type="
               << input->type().Utf8();
    return static_cast<int32_t>(WebCommandErrorCode::INPUT_VALUE_FORMAT_INVALID);
  }

  input->SetValue(value_str,
                  TextFieldEventBehavior::kDispatchInputAndChangeEvent);
  return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
}

int32_t CommandActionDOMHelper::ExecuteAIPageInputOnElement(
    Element* element,
    const std::string& value,
    const std::string& input_type) {
  auto* input = DynamicTo<HTMLInputElement>(element);
  if (!input) {
    LOG(ERROR) << "[WebCommandAction] ExecuteAIPageInput: element is not an input element";
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH);
  }

  std::string actual_type = input->type().Utf8();
  if (!IsStringValueInputType(actual_type)) {
    return static_cast<int32_t>(WebCommandErrorCode::INPUT_TYPE_INVALID);
  }
  if (input_type != "default") {
    if (!IsStringValueInputType(input_type)) {
      return static_cast<int32_t>(WebCommandErrorCode::INPUT_TYPE_INVALID);
    }
    if (input_type != actual_type) {
      LOG(ERROR) << "[WebCommandAction] ExecuteAIPageInput: type mismatch, expected="
                 << actual_type << ", got=" << input_type;
      return static_cast<int32_t>(WebCommandErrorCode::INPUT_EVENT_TYPE_MISMATCH);
    }
  }

  String value_str = String::FromUTF8(value);
  if (!input->IsValidValue(value_str)) {
    LOG(ERROR) << "[WebCommandAction] ExecuteAIPageInput: invalid value for input type="
               << actual_type;
    return static_cast<int32_t>(WebCommandErrorCode::INPUT_VALUE_FORMAT_INVALID);
  }

  input->SetValue(value_str,
                  TextFieldEventBehavior::kDispatchInputAndChangeEvent);
  return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
}

int32_t CommandActionDOMHelper::ExecuteAIPageInputByXPath(
    Document* document,
    const std::string& xpath,
    const std::string& value,
    const std::string& input_type) {
  Element* element = FindElementByXPath(document, xpath);
  if (!element) {
    LOG(ERROR) << "[WebCommandAction] ExecuteAIPageInput: element not found for xpath=" << xpath;
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND);
  }
  return ExecuteAIPageInputOnElement(element, value, input_type);
}

int32_t CommandActionDOMHelper::ExecuteAIPageInputById(
    WebLocalFrame* frame,
    Document* document,
    const std::string& id,
    const std::string& value,
    const std::string& input_type) {
  int32_t error_code = static_cast<int32_t>(WebCommandErrorCode::FAILED);
  Element* element = FindElementById(frame, document, id, &error_code);
  if (!element) {
    return error_code;
  }
  return ExecuteAIPageInputOnElement(element, value, input_type);
}

int32_t CommandActionDOMHelper::ExecuteSelect(
    Document* document,
    const std::string& xpath,
    const std::vector<int32_t>& indexes,
    const std::vector<std::string>& values) {
  Element* element = FindElementByXPath(document, xpath);
  if (!element) {
    LOG(ERROR) << "[WebCommandAction] ExecuteSelect: element not found for xpath=" << xpath;
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND);
  }

  auto* select = DynamicTo<HTMLSelectElement>(element);
  if (!select) {
    LOG(ERROR) << "[WebCommandAction] ExecuteSelect: element is not a select element";
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH);
  }

  if (!indexes.empty()) {
    return SelectByIndexes(select, indexes);
  }
  if (!values.empty()) {
    return SelectByValues(select, values);
  }

  LOG(ERROR) << "[WebCommandAction] ExecuteSelect: empty indexes and values";
  return static_cast<int32_t>(WebCommandErrorCode::SELECT_EMPTY_OPTIONS);
}

int32_t CommandActionDOMHelper::ExecuteSelectById(
    WebLocalFrame* frame,
    Document* document,
    const std::string& id,
    const std::vector<int32_t>& indexes,
    const std::vector<std::string>& values) {
  int32_t error_code = static_cast<int32_t>(WebCommandErrorCode::FAILED);
  Element* element = FindElementById(frame, document, id, &error_code);
  if (!element) {
    return error_code;
  }

  auto* select = DynamicTo<HTMLSelectElement>(element);
  if (!select) {
    LOG(ERROR) << "[WebCommandAction] ExecuteSelect: element is not a select element";
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH);
  }

  if (!indexes.empty()) {
    return SelectByIndexes(select, indexes);
  }
  if (!values.empty()) {
    return SelectByValues(select, values);
  }

  LOG(ERROR) << "[WebCommandAction] ExecuteSelect: empty indexes and values";
  return static_cast<int32_t>(WebCommandErrorCode::SELECT_EMPTY_OPTIONS);
}

int32_t CommandActionDOMHelper::SelectByIndexes(
    HTMLSelectElement* select,
    const std::vector<int32_t>& indexes) {
  if (!select->IsMultiple() && indexes.size() > 1) {
    LOG(ERROR) << "[WebCommandAction] SelectByIndexes: single select received multiple indexes";
    return static_cast<int32_t>(WebCommandErrorCode::SELECT_NOT_MULTIPLE);
  }

  if (!select->IsMultiple()) {
    int32_t index = indexes[0];
    if (index < 0 || index >= static_cast<int32_t>(select->length())) {
      LOG(ERROR) << "[WebCommandAction] SelectByIndexes: index out of range, index=" << index
                 << ", length=" << select->length();
      return static_cast<int32_t>(WebCommandErrorCode::SELECT_INDEX_OUT_OF_RANGE);
    }
    auto* option = select->item(static_cast<unsigned>(index));
    if (!option) {
      LOG(ERROR) << "[WebCommandAction] SelectByIndexes: option is null for index=" << index;
      return static_cast<int32_t>(WebCommandErrorCode::SELECT_INDEX_OUT_OF_RANGE);
    }
    if (option->IsDisabledFormControl()) {
      LOG(ERROR) << "[WebCommandAction] SelectByIndexes: option is disabled at index=" << index;
      return static_cast<int32_t>(WebCommandErrorCode::SELECT_OPTION_DISABLED);
    }
    select->SelectOptionByPopup(option->ListIndex());
    return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
  }

  Vector<int> list_indices;
  for (int32_t index : indexes) {
    if (index < 0 || index >= static_cast<int32_t>(select->length())) {
      LOG(ERROR) << "[WebCommandAction] SelectByIndexes: index out of range, index=" << index
                 << ", length=" << select->length();
      return static_cast<int32_t>(WebCommandErrorCode::SELECT_INDEX_OUT_OF_RANGE);
    }
    auto* option = select->item(static_cast<unsigned>(index));
    if (option && option->IsDisabledFormControl()) {
      LOG(ERROR) << "[WebCommandAction] SelectByIndexes: option is disabled at index=" << index;
      return static_cast<int32_t>(WebCommandErrorCode::SELECT_OPTION_DISABLED);
    }
    if (option) {
      list_indices.push_back(option->ListIndex());
    }
  }
  select->SelectMultipleOptionsByPopup(list_indices);
  return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
}

int32_t CommandActionDOMHelper::SelectByValues(
    HTMLSelectElement* select,
    const std::vector<std::string>& values) {
  if (!select->IsMultiple() && values.size() > 1) {
    LOG(ERROR) << "[WebCommandAction] SelectByValues: single select received multiple values";
    return static_cast<int32_t>(WebCommandErrorCode::SELECT_NOT_MULTIPLE);
  }

  if (!select->IsMultiple()) {
    String target = String::FromUTF8(values[0]);
    for (auto& option : select->GetOptionList()) {
      if (option.value() != target) {
        continue;
      }
      if (option.IsDisabledFormControl()) {
        LOG(ERROR) << "[WebCommandAction] SelectByValues: option is disabled";
        return static_cast<int32_t>(WebCommandErrorCode::SELECT_OPTION_DISABLED);
      }
      select->SelectOptionByPopup(option.ListIndex());
      return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
    }
    LOG(ERROR) << "[WebCommandAction] SelectByValues: value not found";
    return static_cast<int32_t>(WebCommandErrorCode::SELECT_VALUE_NOT_FOUND);
  }

  HashMap<String, unsigned> value_to_index;
  for (unsigned i = 0; i < select->length(); ++i) {
    auto* option = select->item(i);
    if (option && !value_to_index.Contains(option->value())) {
      value_to_index.insert(option->value(), i);
    }
  }
  Vector<int> list_indices;
  for (const auto& v : values) {
    String wtf_value = String::FromUTF8(v);
    auto it = value_to_index.find(wtf_value);
    if (it == value_to_index.end()) {
      LOG(ERROR) << "[WebCommandAction] SelectByValues: value not found";
      return static_cast<int32_t>(WebCommandErrorCode::SELECT_VALUE_NOT_FOUND);
    }
    auto* option = select->item(it->value);
    if (option && option->IsDisabledFormControl()) {
      LOG(ERROR) << "[WebCommandAction] SelectByValues: option is disabled";
      return static_cast<int32_t>(WebCommandErrorCode::SELECT_OPTION_DISABLED);
    }
    if (option) {
      list_indices.push_back(option->ListIndex());
    }
  }
  select->SelectMultipleOptionsByPopup(list_indices);
  return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
}

// ===== Private helper methods =====

bool CommandActionDOMHelper::IsEditableElement(Element* element) {
  if (!element) return false;
  if (DynamicTo<HTMLInputElement>(element)) return true;
  if (DynamicTo<HTMLTextAreaElement>(element)) return true;
  auto* html_el = DynamicTo<HTMLElement>(element);
  return html_el && (html_el->contentEditableNormalized() == ContentEditableType::kContentEditable ||
                    html_el->contentEditableNormalized() == ContentEditableType::kPlaintextOnly);
}

void CommandActionDOMHelper::SetCursorToEnd(Element* element) {
  auto* html_el = DynamicTo<HTMLElement>(element);
  if (!html_el) return;
  LocalFrame* frame = html_el->GetDocument().GetFrame();
  if (!frame) return;
  Range* range = Range::Create(html_el->GetDocument());
  range->selectNodeContents(html_el, ASSERT_NO_EXCEPTION);
  range->collapse(false);
  frame->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(EphemeralRange(range->StartPosition(), range->EndPosition()))
          .Build(),
      SetSelectionOptions());
}

void CommandActionDOMHelper::InsertTextAtPosition(Element* element,
                                                   const std::string& content,
                                                   int32_t index) {
  auto* html_el = DynamicTo<HTMLElement>(element);
  if (!html_el) return;
  LocalFrame* frame = html_el->GetDocument().GetFrame();
  if (!frame) return;

  // For contentEditable: use Blink editing API to insert text.
  // First set selection to the insert position using FindPositionInContentEditable,
  // then use TypingCommand to insert (preserves DOM subtree, undo/redo, IME).
  Position insert_pos = FindPositionInContentEditable(html_el, index);
  frame->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(EphemeralRange(insert_pos))
          .Build(),
      SetSelectionOptions());
  String text_to_insert = String::FromUTF8(content);
  TypingCommand::InsertText(*frame->GetDocument(), text_to_insert, 0);
}

// ===== Private helper: contentEditable Position =====

// Find a Position in a contentEditable element by character offset.
// Walks text nodes, accumulating length until the target offset is reached.
// This avoids setInnerText-based rebuilding which destroys the DOM subtree.
Position CommandActionDOMHelper::FindPositionInContentEditable(
    HTMLElement* html_el, int offset) {
  int remaining = std::max(0, offset);
  for (Node* node = html_el->firstChild(); node; node = node->nextSibling()) {
    if (node->IsTextNode()) {
      auto* text = To<Text>(node);
      int text_length = static_cast<int>(text->length());
      if (remaining <= text_length) {
        return Position(text, remaining);
      }
      remaining -= text_length;
    } else if (node->IsHTMLElement()) {
      // For <br> and other elements, count as 1 character (matches innerText behavior)
      if (remaining <= 0) {
        return Position::BeforeNode(*node);
      }
      remaining--;
    }
  }
  // Offset exceeds content, place at end of last text node
  Node* last_child = html_el->lastChild();
  if (last_child && last_child->IsTextNode()) {
    return Position(last_child, static_cast<int>(To<Text>(last_child)->length()));
  }
  return Position::AfterNode(*html_el);
}

// ===== Private helper: event dispatch =====

void CommandActionDOMHelper::DispatchInputAndChangeEvents(Element* element,
                                                           const std::string& content) {
  auto* input_event = InputEvent::CreateInput(
      InputEvent::InputType::kInsertText, String::FromUTF8(content),
      InputEvent::EventIsComposing::kNotComposing, nullptr);
  element->DispatchEvent(*input_event);
  // Match JS: el.dispatchEvent(new Event('change', {bubbles:true}))
  Event* change_event = Event::CreateBubble(event_type_names::kChange);
  element->DispatchEvent(*change_event);
}

// ===== New public methods for inputAutoFill =====

// suppress_keyboard implementation note:
// This temporarily modifies DOM attributes (readonly/contentEditable/tabindex) to
// prevent the IME keyboard from showing on focus. Known limitations:
// 1. MutationObserver may observe the transient attribute changes.
// 2. Focus event handlers may see the element as readonly/contentEditable=false
//    during the brief window before attribute restoration.
// These are accepted trade-offs: the attribute changes and restoration happen
// synchronously within the same microtask, minimizing the observable window.
// Future improvement: if Blink provides a Frame-level IMF control API,
// replace this DOM attribute workaround.
int32_t CommandActionDOMHelper::ExecuteFocus(Document* document,
                                              const std::string& xpath,
                                              bool suppress_keyboard) {
  Element* element = FindElementByXPath(document, xpath);
  if (!element) {
    LOG(ERROR) << "[WebCommandAction] ExecuteFocus: element not found for xpath=" << xpath;
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND);
  }

  auto* input = DynamicTo<HTMLInputElement>(element);
  auto* textarea = DynamicTo<HTMLTextAreaElement>(element);
  auto* html_el = DynamicTo<HTMLElement>(element);

  if (input || textarea) {
    if (suppress_keyboard) {
      bool was_readonly = input ? input->IsReadOnly() : textarea->IsReadOnly();
      if (!was_readonly) {
        // Not readonly: temporarily add readonly to suppress keyboard, then remove
        element->setAttribute(html_names::kReadonlyAttr, AtomicString(""));
        element->Focus();
        element->removeAttribute(html_names::kReadonlyAttr);
      } else {
        // Already readonly: just focus (keyboard won't show for readonly inputs)
        element->Focus();
      }
    } else {
      element->Focus();
      if (html_el) {
        html_el->click();
      }
    }
  } else {
    // contentEditable or other focusable element
    bool is_content_editable = html_el &&
        (html_el->contentEditableNormalized() == ContentEditableType::kContentEditable ||
         html_el->contentEditableNormalized() == ContentEditableType::kPlaintextOnly);
    if (suppress_keyboard && is_content_editable) {
      // For contentEditable elements, suppress keyboard by temporarily
      // setting contentEditable=false. Since contentEditable=false makes
      // the element non-focusable, we add tabindex=-1 to keep it
      // programmatically focusable. After focus, restore both attributes.
      // The IMF only shows keyboard on focus events, not attribute changes,
      // so restoring contentEditable after focus won't trigger the keyboard.
      bool had_tabindex = element->hasAttribute(html_names::kTabindexAttr);
      String original_tabindex;
      if (had_tabindex) {
        original_tabindex = element->getAttribute(html_names::kTabindexAttr);
      }
      bool had_ce_attr = element->hasAttribute(html_names::kContenteditableAttr);
      String original_ce;
      if (had_ce_attr) {
        original_ce = element->getAttribute(html_names::kContenteditableAttr);
      }
      // Ensure programmatically focusable when contentEditable=false
      if (!had_tabindex) {
        element->setAttribute(html_names::kTabindexAttr, AtomicString("-1"));
      }
      // Temporarily make non-editable to prevent keyboard
      element->setAttribute(html_names::kContenteditableAttr, AtomicString("false"));
      element->Focus();
      // Restore contentEditable (keyboard only triggers on focus, not attribute changes)
      if (had_ce_attr) {
        element->setAttribute(html_names::kContenteditableAttr, AtomicString(original_ce));
      } else {
        element->removeAttribute(html_names::kContenteditableAttr);
      }
      // Restore tabindex
      if (!had_tabindex) {
        element->removeAttribute(html_names::kTabindexAttr);
      } else {
        element->setAttribute(html_names::kTabindexAttr, AtomicString(original_tabindex));
      }
      SetCursorToEnd(element);
    } else {
      element->Focus();
      // Only click on known interactive element types to avoid spurious click events
      if (html_el) {
        auto* button = DynamicTo<HTMLButtonElement>(html_el);
        auto* anchor = DynamicTo<HTMLAnchorElement>(html_el);
        auto* select_el = DynamicTo<HTMLSelectElement>(html_el);
        if (button || anchor || select_el ||
            html_el->hasAttribute(html_names::kTabindexAttr)) {
          html_el->click();
        }
      }
      if (is_content_editable) {
        SetCursorToEnd(element);
      }
    }
  }
  return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
}

int32_t CommandActionDOMHelper::ExecuteInputModify(Document* document,
                                                    const std::string& content) {
  Element* active = document->FocusedElement();
  if (!active) {
    LOG(ERROR) << "[WebCommandAction] ExecuteInputModify: FocusedElement is null";
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH);
  }
  if (!IsEditableElement(active)) {
    LOG(ERROR) << "[WebCommandAction] ExecuteInputModify: focused element not editable";
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH);
  }

  auto* input = DynamicTo<HTMLInputElement>(active);
  if (input) {
    if (input->IsReadOnly()) {
      LOG(ERROR) << "[WebCommandAction] ExecuteInputModify: input is readonly";
      return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH);
    }
    String value_str = String::FromUTF8(content);
    input->SetValue(value_str, TextFieldEventBehavior::kDispatchNoEvent);
    DispatchInputAndChangeEvents(input, content);
    return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
  }

  auto* textarea = DynamicTo<HTMLTextAreaElement>(active);
  if (textarea) {
    if (textarea->IsReadOnly()) {
      LOG(ERROR) << "[WebCommandAction] ExecuteInputModify: textarea is readonly";
      return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH);
    }
    String value_str = String::FromUTF8(content);
    textarea->SetValue(value_str, TextFieldEventBehavior::kDispatchNoEvent);
    DispatchInputAndChangeEvents(textarea, content);
    return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
  }

  // contentEditable element
  auto* html_el = DynamicTo<HTMLElement>(active);
  if (html_el && (html_el->contentEditableNormalized() == ContentEditableType::kContentEditable ||
                  html_el->contentEditableNormalized() == ContentEditableType::kPlaintextOnly)) {
    html_el->setInnerText(String::FromUTF8(content));
    SetCursorToEnd(html_el);
    DispatchInputAndChangeEvents(html_el, content);
    return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
  }

  return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH);
}

int32_t CommandActionDOMHelper::ExecuteInputInsert(Document* document,
                                                    const std::string& content,
                                                    int32_t index) {
  Element* active = document->FocusedElement();
  if (!active || !IsEditableElement(active)) {
    LOG(ERROR) << "[WebCommandAction] ExecuteInputInsert: no focused editable element";
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH);
  }

  auto* input = DynamicTo<HTMLInputElement>(active);
  if (input) {
    if (input->IsReadOnly()) {
      LOG(ERROR) << "[WebCommandAction] ExecuteInputInsert: input is readonly";
      return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH);
    }
    String old_value = input->Value();
    int safe_index = std::min(index, static_cast<int32_t>(old_value.length()));
    String new_value = old_value.Substring(0, safe_index)
                     + String::FromUTF8(content)
                     + old_value.Substring(safe_index);
    input->SetValue(new_value, TextFieldEventBehavior::kDispatchNoEvent);
    // Move cursor after inserted content (before event dispatch, matching JS behavior)
    int cursor_pos = safe_index + static_cast<int32_t>(content.length());
    input->SetSelectionRange(cursor_pos, cursor_pos, kSelectionHasNoDirection);
    DispatchInputAndChangeEvents(input, content);
    return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
  }

  auto* textarea = DynamicTo<HTMLTextAreaElement>(active);
  if (textarea) {
    if (textarea->IsReadOnly()) {
      LOG(ERROR) << "[WebCommandAction] ExecuteInputInsert: textarea is readonly";
      return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH);
    }
    String old_value = textarea->Value();
    int safe_index = std::min(index, static_cast<int32_t>(old_value.length()));
    String new_value = old_value.Substring(0, safe_index)
                     + String::FromUTF8(content)
                     + old_value.Substring(safe_index);
    textarea->SetValue(new_value, TextFieldEventBehavior::kDispatchNoEvent);
    int cursor_pos = safe_index + static_cast<int32_t>(content.length());
    textarea->SetSelectionRange(cursor_pos, cursor_pos, kSelectionHasNoDirection);
    DispatchInputAndChangeEvents(textarea, content);
    return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
  }

  // contentEditable element
  auto* html_el = DynamicTo<HTMLElement>(active);
  if (html_el && (html_el->contentEditableNormalized() == ContentEditableType::kContentEditable ||
                  html_el->contentEditableNormalized() == ContentEditableType::kPlaintextOnly)) {
    InsertTextAtPosition(active, content, index);
    DispatchInputAndChangeEvents(html_el, content);
    return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
  }

  return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH);
}

int32_t CommandActionDOMHelper::ExecuteAutoFill(Document* document,
                                                 const std::string& xpath,
                                                 const std::string& content,
                                                 int32_t index,
                                                 int32_t mode) {
  Element* element = FindElementByXPath(document, xpath);
  if (!element) {
    LOG(ERROR) << "[WebCommandAction] ExecuteAutoFill: element not found, xpath=" << xpath;
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND);
  }

  auto* input = DynamicTo<HTMLInputElement>(element);
  auto* textarea = DynamicTo<HTMLTextAreaElement>(element);

  if (!input && !textarea) {
    LOG(ERROR) << "[WebCommandAction] ExecuteAutoFill: not input/textarea";
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH);
  }

  // Reject disabled elements (both Insert and Overwrite)
  if ((input && input->IsDisabledFormControl()) ||
      (textarea && textarea->IsDisabledFormControl())) {
    LOG(ERROR) << "[WebCommandAction] ExecuteAutoFill: element is disabled";
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_INTERACTABLE);
  }

  // Reject readonly elements (both Insert and Overwrite)
  if ((input && input->IsReadOnly()) ||
      (textarea && textarea->IsReadOnly())) {
    LOG(ERROR) << "[WebCommandAction] ExecuteAutoFill: element is readonly";
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_INTERACTABLE);
  }

  String value_str = String::FromUTF8(content);

  if (mode == 1 /* Insert */) {
    // Insert: splice content into current value at index position.
    // No Focus needed — this is string concatenation + SetValue, not cursor-based insertion.
    if (input) {
      String old_value = input->Value();
      int safe_index = std::min(index, static_cast<int32_t>(old_value.length()));
      String new_value = old_value.Substring(0, safe_index)
                       + value_str
                       + old_value.Substring(safe_index);
      input->SetValue(new_value, TextFieldEventBehavior::kDispatchNoEvent);
      int cursor_pos = safe_index + static_cast<int32_t>(value_str.length());
      input->SetSelectionRange(cursor_pos, cursor_pos, kSelectionHasNoDirection);
      DispatchInputAndChangeEvents(input, content);
    } else if (textarea) {
      String old_value = textarea->Value();
      int safe_index = std::min(index, static_cast<int32_t>(old_value.length()));
      String new_value = old_value.Substring(0, safe_index)
                       + value_str
                       + old_value.Substring(safe_index);
      textarea->SetValue(new_value, TextFieldEventBehavior::kDispatchNoEvent);
      int cursor_pos = safe_index + static_cast<int32_t>(value_str.length());
      textarea->SetSelectionRange(cursor_pos, cursor_pos, kSelectionHasNoDirection);
      DispatchInputAndChangeEvents(textarea, content);
    }
  } else {
    // Overwrite mode: SetValue directly, no Focus, no focus event
    if (input) {
      input->SetValue(value_str, TextFieldEventBehavior::kDispatchInputAndChangeEvent);
    } else if (textarea) {
      textarea->SetValue(value_str, TextFieldEventBehavior::kDispatchInputAndChangeEvent);
    }
  }

  return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
}

int32_t CommandActionDOMHelper::ExecuteClick(Document* document,
                                             const std::string& xpath) {
  Element* element = FindElementByXPath(document, xpath);
  if (!element) {
    LOG(ERROR) << "[WebCommandAction] ExecuteClick: element not found for xpath=" << xpath;
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND);
  }

  auto* html_el = DynamicTo<HTMLElement>(element);
  if (html_el) {
    html_el->click();
  } else {
    LOG(ERROR) << "[WebCommandAction] ExecuteClick: element is not an HTMLElement";
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH);
  }

  return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
}

// NOTE: ExecuteScroll currently supports vertical scrolling only.
// The `align` parameter controls vertical alignment ("top"/"mid"/"center"/"bottom").
// This matches the legacy JS injection behavior (CombineCommandJsString) which also
// only scrolls vertically (scrollTo({ top: ... })).
// Horizontal scroll support can be added in a future update if needed.
int32_t CommandActionDOMHelper::ExecuteScroll(Document* document,
                                              const std::string& xpath,
                                              const std::string& align,
                                              int32_t offset) {
  Element* element = FindElementByXPath(document, xpath);
  if (!element) {
    LOG(ERROR) << "[WebCommandAction] ExecuteScroll: element not found for xpath=" << xpath;
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND);
  }

  // Find nearest scrollable ancestor (matching JS: walk up parentElement checking overflow)
  Element* scroll_container = nullptr;
  Element* parent = element->parentElement();
  while (parent && parent != document->documentElement()) {
    const ComputedStyle* style = parent->GetComputedStyle();
    if (style) {
      EOverflow overflow_y = style->OverflowY();
      if ((overflow_y == EOverflow::kAuto || overflow_y == EOverflow::kScroll) &&
          parent->scrollHeight() > parent->clientHeight()) {
        scroll_container = parent;
        break;
      }
    }
    parent = parent->parentElement();
  }

  bool is_window = !scroll_container ||
                   scroll_container == document->documentElement() ||
                   scroll_container == document->body();

  // Get element bounding rect
  gfx::RectF element_rect = element->GetBoundingClientRectNoLifecycleUpdateNoAdjustment();

  // Calculate current scroll position and container dimensions
  double current_top = 0;
  double container_height = 0;
  double container_top = 0;
  if (is_window) {
    LocalFrame* frame = document->GetFrame();
    if (frame && frame->View()) {
      ScrollableArea* scrollable = frame->View()->GetScrollableArea();
      if (scrollable) {
        current_top = scrollable->GetScrollOffset().y();
        container_height = scrollable->VisibleContentRect().height();
      }
      container_top = 0;
    }
  } else {
    current_top = scroll_container->scrollTop();
    container_height = scroll_container->clientHeight();
    gfx::RectF container_rect = scroll_container->GetBoundingClientRectNoLifecycleUpdateNoAdjustment();
    container_top = container_rect.y();
  }

  double absolute_top = current_top + (element_rect.y() - container_top);

  // Calculate target position based on align
  double target = 0;
  if (align == "top") {
    target = absolute_top;
  } else if (align == "mid" || align == "center") {
    target = absolute_top - (container_height / 2) + (element_rect.height() / 2);
  } else if (align == "bottom") {
    target = absolute_top - container_height + element_rect.height();
  } else {
    target = absolute_top;
  }

  double final_pos = target + offset;

  // Perform scroll
  if (is_window) {
    LocalFrame* frame = document->GetFrame();
    if (frame && frame->View()) {
      ScrollableArea* scrollable = frame->View()->GetScrollableArea();
      if (scrollable) {
        scrollable->SetScrollOffset(
            ScrollOffset(scrollable->GetScrollOffset().x(), final_pos),
            mojom::blink::ScrollType::kProgrammatic,
            cc::ScrollSourceType::kAbsoluteScroll);
      }
    }
  } else {
    scroll_container->setScrollTop(static_cast<int>(final_pos));
  }

  return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
}

int32_t CommandActionDOMHelper::ExecuteInputSelect(Document* document,
                                                    int32_t start_index,
                                                    int32_t finish_index) {
  Element* active = document->FocusedElement();
  if (!active) {
    LOG(ERROR) << "[WebCommandAction] ExecuteInputSelect: no focused element";
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND);
  }

  auto* input = DynamicTo<HTMLInputElement>(active);
  auto* textarea = DynamicTo<HTMLTextAreaElement>(active);
  if (input || textarea) {
    int safe_start = std::max(0, start_index);
    int safe_finish = std::max(0, finish_index);
    if (input) {
      input->SetSelectionRange(safe_start, safe_finish, kSelectionHasNoDirection);
    } else {
      textarea->SetSelectionRange(safe_start, safe_finish, kSelectionHasNoDirection);
    }
    return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
  }

  // contentEditable element
  auto* html_el = DynamicTo<HTMLElement>(active);
  if (html_el && (html_el->contentEditableNormalized() == ContentEditableType::kContentEditable ||
                  html_el->contentEditableNormalized() == ContentEditableType::kPlaintextOnly)) {
    int safe_start = std::max(0, start_index);
    int safe_finish = std::max(0, finish_index);
    Position start_pos = FindPositionInContentEditable(html_el, safe_start);
    Position end_pos = FindPositionInContentEditable(html_el, safe_finish);
    LocalFrame* frame = html_el->GetDocument().GetFrame();
    if (frame) {
      frame->Selection().SetSelection(
          SelectionInDOMTree::Builder()
              .SetBaseAndExtent(EphemeralRange(start_pos, end_pos))
              .Build(),
          SetSelectionOptions());
    }
    return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
  }

  LOG(ERROR) << "[WebCommandAction] ExecuteInputSelect: focused element is not editable";
  return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH);
}

int32_t CommandActionDOMHelper::ExecuteInputSetCursor(Document* document,
                                                       int32_t index) {
  Element* active = document->FocusedElement();
  if (!active) {
    LOG(ERROR) << "[WebCommandAction] ExecuteInputSetCursor: no focused element";
    return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND);
  }

  auto* input = DynamicTo<HTMLInputElement>(active);
  auto* textarea = DynamicTo<HTMLTextAreaElement>(active);
  if (input || textarea) {
    int safe_pos = std::max(0, index);
    if (input) {
      input->SetSelectionRange(safe_pos, safe_pos, kSelectionHasNoDirection);
    } else {
      textarea->SetSelectionRange(safe_pos, safe_pos, kSelectionHasNoDirection);
    }
    return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
  }

  // contentEditable element
  auto* html_el = DynamicTo<HTMLElement>(active);
  if (html_el && (html_el->contentEditableNormalized() == ContentEditableType::kContentEditable ||
                  html_el->contentEditableNormalized() == ContentEditableType::kPlaintextOnly)) {
    Position pos = FindPositionInContentEditable(html_el, std::max(0, index));
    LocalFrame* frame = html_el->GetDocument().GetFrame();
    if (frame) {
      frame->Selection().SetSelection(
          SelectionInDOMTree::Builder()
              .SetBaseAndExtent(EphemeralRange(pos, pos))
              .Build(),
          SetSelectionOptions());
    }
    return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
  }

  LOG(ERROR) << "[WebCommandAction] ExecuteInputSetCursor: focused element is not editable";
  return static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH);
}

}  // namespace blink

#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)
