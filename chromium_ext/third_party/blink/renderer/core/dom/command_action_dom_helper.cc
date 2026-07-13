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
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
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

}  // namespace blink

#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)
