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

#ifndef COMMAND_ACTION_DOM_HELPER_H_
#define COMMAND_ACTION_DOM_HELPER_H_

#include "arkweb/build/features/features.h"

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)

#include <string>
#include <vector>

#include "arkweb/ohos_nweb/include/nweb_errors.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/events/input_event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using OHOS::NWeb::WebCommandErrorCode;

namespace blink {

class HTMLSelectElement;
class WebLocalFrame;

class CommandActionDOMHelper {
 public:
  static Element* FindElementByXPath(Document* document,
                                     const std::string& xpath);
  static Element* FindElementById(WebLocalFrame* frame,
                                  Document* document,
                                  const std::string& id,
                                  int32_t* error_code);

  static int32_t ExecuteInput(Document* document,
                              const std::string& xpath,
                              const std::string& value,
                              const std::string& event_type);

  static int32_t ExecuteSelect(Document* document,
                               const std::string& xpath,
                               const std::vector<int32_t>& indexes,
                               const std::vector<std::string>& values);

  static int32_t ExecuteAIPageInputByXPath(Document* document,
                                           const std::string& xpath,
                                           const std::string& value,
                                           const std::string& input_type);
  static int32_t ExecuteAIPageInputById(WebLocalFrame* frame,
                                        Document* document,
                                        const std::string& id,
                                        const std::string& value,
                                        const std::string& input_type);
  static int32_t ExecuteSelectById(WebLocalFrame* frame,
                                   Document* document,
                                   const std::string& id,
                                   const std::vector<int32_t>& indexes,
                                   const std::vector<std::string>& values);

  // Focus an element; suppress_keyboard=true prevents soft keyboard from showing
  static int32_t ExecuteFocus(Document* document,
                              const std::string& xpath,
                              bool suppress_keyboard);

  // Overwrite the value of the currently focused editable element
  static int32_t ExecuteInputModify(Document* document,
                                    const std::string& content);

  // Insert content at the specified index in the currently focused editable element
  static int32_t ExecuteInputInsert(Document* document,
                                    const std::string& content,
                                    int32_t index);

  // Batch autofill: find element by xpath, then overwrite or insert.
  // mode: 0=Overwrite (SetValue without Focus), 1=Insert (Focus + insert at index)
  static int32_t ExecuteAutoFill(Document* document,
                                 const std::string& xpath,
                                 const std::string& content,
                                 int32_t index,
                                 int32_t mode);

  // Click an element identified by xpath
  static int32_t ExecuteClick(Document* document,
                              const std::string& xpath);

  // Scroll an element identified by xpath into view with alignment and offset
  static int32_t ExecuteScroll(Document* document,
                               const std::string& xpath,
                               const std::string& align,
                               int32_t offset);

  // Select text range in the currently focused editable element
  static int32_t ExecuteInputSelect(Document* document,
                                    int32_t start_index,
                                    int32_t finish_index);

  // Set cursor position in the currently focused editable element
  static int32_t ExecuteInputSetCursor(Document* document,
                                       int32_t index);

 private:
  static int32_t ExecuteAIPageInputOnElement(Element* element,
                                             const std::string& value,
                                             const std::string& input_type);
  static int32_t SelectByIndexes(HTMLSelectElement* select,
                                 const std::vector<int32_t>& indexes);
  static int32_t SelectByValues(HTMLSelectElement* select,
                                const std::vector<std::string>& values);
  static bool IsEditableElement(Element* element);
  static void SetCursorToEnd(Element* element);
  static void InsertTextAtPosition(Element* element,
                                   const std::string& content,
                                   int32_t index);
  static Position FindPositionInContentEditable(HTMLElement* html_el, int offset);
  static void DispatchInputAndChangeEvents(Element* element,
                                           const std::string& content = "");
};

}  // namespace blink

#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)

#endif  // COMMAND_ACTION_DOM_HELPER_H_
