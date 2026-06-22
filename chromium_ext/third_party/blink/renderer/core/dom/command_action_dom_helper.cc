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

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/xml/document_xpath_evaluator.h"
#include "third_party/blink/renderer/core/xml/xpath_result.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"

namespace blink {

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
