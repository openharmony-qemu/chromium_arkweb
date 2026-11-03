/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

#include <fuzzer/FuzzedDataProvider.h>
#include <memory>

#include "arkweb/chromium_ext/third_party/blink/renderer/core/editing/ime/arkweb_input_method_controller_utils.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

using namespace blink;

namespace {

std::unique_ptr<DummyPageHolder> CreatePageHolder() {
  return std::make_unique<DummyPageHolder>();
}

ArkwebInputMethodControllerUtils* CreateUtils(DummyPageHolder& holder) {
  InputMethodController& imc = holder.GetFrame().GetInputMethodController();
  return MakeGarbageCollected<ArkwebInputMethodControllerUtils>(&imc);
}

void FuzzPublicMethodsNoFocus(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  auto* utils = CreateUtils(*holder);

  // CompositionEphemeralRange - no composition, returns null range
  auto range = utils->CompositionEphemeralRange();
  (void)range.IsNull();

  // HasComposition - no composition active
  bool has_comp = utils->HasComposition();
  (void)has_comp;

  // Clear - no-op without composition
  utils->Clear();

  // GetInputElementAttributes - no focused element, early return
  HashMap<String, String> attributes;
  utils->GetInputElementAttributes(attributes);
}

void FuzzWithFocusedInputElement(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  auto* utils = CreateUtils(*holder);

  // Create <input> with fuzzed attributes
  auto* input = MakeGarbageCollected<HTMLInputElement>(doc,
                                                       CreateElementFlags());
  doc.body()->AppendChild(input);

  // Set attributes from fuzzer data
  size_t attr_count = fdp->ConsumeIntegralInRange<size_t>(0, 5);
  for (size_t i = 0; i < attr_count; ++i) {
    std::string name = fdp->ConsumeRandomLengthString(32);
    std::string value = fdp->ConsumeRandomLengthString(64);
    input->setAttribute(
        QualifiedName(AtomicString(name.c_str())),
        AtomicString(value.c_str()));
  }

  input->Focus();

  // Test all public methods with focused input
  utils->CompositionEphemeralRange();
  utils->HasComposition();
  utils->Clear();

  HashMap<String, String> attributes;
  utils->GetInputElementAttributes(attributes);
}

void FuzzWithFocusedTextAreaElement(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  auto* utils = CreateUtils(*holder);

  // Create <textarea> with fuzzed attributes
  auto* textarea = MakeGarbageCollected<HTMLTextAreaElement>(doc);
  doc.body()->AppendChild(textarea);

  size_t attr_count = fdp->ConsumeIntegralInRange<size_t>(0, 5);
  for (size_t i = 0; i < attr_count; ++i) {
    std::string name = fdp->ConsumeRandomLengthString(32);
    std::string value = fdp->ConsumeRandomLengthString(64);
    textarea->setAttribute(
        QualifiedName(AtomicString(name.c_str())),
        AtomicString(value.c_str()));
  }

  textarea->Focus();

  utils->CompositionEphemeralRange();
  utils->HasComposition();
  utils->Clear();

  HashMap<String, String> attributes;
  utils->GetInputElementAttributes(attributes);
}

void FuzzWithNonEditableElement(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  auto* utils = CreateUtils(*holder);

  // Create a non-editable element (e.g., <div>)
  auto* div = MakeGarbageCollected<HTMLDivElement>(doc);
  doc.body()->AppendChild(div);
  div->Focus();

  // GetInputElementAttributes with non-editable focused element
  // Should hit the IsEditable check and return without querying
  HashMap<String, String> attributes;
  utils->GetInputElementAttributes(attributes);
}

void FuzzMultipleElements(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  auto* utils = CreateUtils(*holder);

  // Create multiple elements
  auto* input = MakeGarbageCollected<HTMLInputElement>(doc,
                                                       CreateElementFlags());
  input->setAttribute(html_names::kTypeAttr, AtomicString("text"));
  input->setAttribute(html_names::kMaxlengthAttr, AtomicString("10"));
  input->setAttribute(html_names::kPlaceholderAttr, AtomicString("test"));
  doc.body()->AppendChild(input);

  auto* textarea = MakeGarbageCollected<HTMLTextAreaElement>(doc);
  textarea->setAttribute(html_names::kMaxlengthAttr, AtomicString("100"));
  doc.body()->AppendChild(textarea);

  // Focus one based on fuzzer choice
  uint8_t focus_target = fdp->ConsumeIntegralInRange<uint8_t>(0, 1);
  if (focus_target == 0) {
    input->Focus();
  } else {
    textarea->Focus();
  }

  // Exercise all public methods
  utils->CompositionEphemeralRange();
  utils->HasComposition();
  utils->Clear();

  HashMap<String, String> attributes;
  utils->GetInputElementAttributes(attributes);

  // Switch focus and test again
  if (focus_target == 0) {
    textarea->Focus();
  } else {
    input->Focus();
  }

  HashMap<String, String> attributes2;
  utils->GetInputElementAttributes(attributes2);
}

void FuzzClearAndHasComposition(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  auto* utils = CreateUtils(*holder);

  // Repeatedly check and clear
  uint8_t iterations = fdp->ConsumeIntegralInRange<uint8_t>(1, 10);
  for (uint8_t i = 0; i < iterations; ++i) {
    utils->HasComposition();
    utils->CompositionEphemeralRange();
    utils->Clear();
  }
}

void FuzzWithDifferentInputTypes(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  auto* utils = CreateUtils(*holder);

  // Test different input types
  std::vector<std::string> input_types = {
    "text", "password", "email", "tel", "url", "search", "number"
  };

  size_t type_idx = fdp->ConsumeIntegralInRange<size_t>(0, input_types.size() - 1);
  auto* input = MakeGarbageCollected<HTMLInputElement>(doc,
                                                       CreateElementFlags());
  input->setAttribute(html_names::kTypeAttr, AtomicString(input_types[type_idx].c_str()));

  // Add some common attributes
  size_t attr_count = fdp->ConsumeIntegralInRange<size_t>(0, 3);
  for (size_t i = 0; i < attr_count; ++i) {
    std::string name = fdp->ConsumeRandomLengthString(16);
    std::string value = fdp->ConsumeRandomLengthString(32);
    input->setAttribute(
        QualifiedName(AtomicString(name.c_str())),
        AtomicString(value.c_str()));
  }

  doc.body()->AppendChild(input);
  input->Focus();

  utils->CompositionEphemeralRange();
  utils->HasComposition();
  utils->Clear();

  HashMap<String, String> attributes;
  utils->GetInputElementAttributes(attributes);
}

void FuzzWithContentEditable(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  auto* utils = CreateUtils(*holder);

  // Create a contenteditable div
  auto* div = MakeGarbageCollected<HTMLDivElement>(doc);
  div->setAttribute(html_names::kContenteditableAttr, AtomicString("true"));
  doc.body()->AppendChild(div);
  div->Focus();

  utils->CompositionEphemeralRange();
  utils->HasComposition();
  utils->Clear();

  HashMap<String, String> attributes;
  utils->GetInputElementAttributes(attributes);
}

void FuzzWithMaxLength(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  auto* utils = CreateUtils(*holder);

  auto* input = MakeGarbageCollected<HTMLInputElement>(doc,
                                                       CreateElementFlags());
  input->setAttribute(html_names::kTypeAttr, AtomicString("text"));

  // Test with different maxlength values
  int maxlength = fdp->ConsumeIntegralInRange<int>(-1, 10000);
  input->setAttribute(html_names::kMaxlengthAttr,
                      AtomicString(String::Number(maxlength)));

  doc.body()->AppendChild(input);
  input->Focus();

  utils->CompositionEphemeralRange();
  utils->HasComposition();
  utils->Clear();

  HashMap<String, String> attributes;
  utils->GetInputElementAttributes(attributes);
}

void FuzzWithPlaceholder(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  auto* utils = CreateUtils(*holder);

  auto* input = MakeGarbageCollected<HTMLInputElement>(doc,
                                                       CreateElementFlags());
  input->setAttribute(html_names::kTypeAttr, AtomicString("text"));

  // Test with different placeholder values
  std::string placeholder = fdp->ConsumeRandomLengthString(128);
  input->setAttribute(html_names::kPlaceholderAttr,
                      AtomicString(placeholder.c_str()));

  doc.body()->AppendChild(input);
  input->Focus();

  utils->CompositionEphemeralRange();
  utils->HasComposition();
  utils->Clear();

  HashMap<String, String> attributes;
  utils->GetInputElementAttributes(attributes);
}

void FuzzWithReadOnly(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  auto* utils = CreateUtils(*holder);

  auto* input = MakeGarbageCollected<HTMLInputElement>(doc,
                                                       CreateElementFlags());
  input->setAttribute(html_names::kTypeAttr, AtomicString("text"));

  // Test with readonly attribute
  if (fdp->ConsumeBool()) {
    input->setAttribute(html_names::kReadonlyAttr, AtomicString(""));
  }

  doc.body()->AppendChild(input);
  input->Focus();

  utils->CompositionEphemeralRange();
  utils->HasComposition();
  utils->Clear();

  HashMap<String, String> attributes;
  utils->GetInputElementAttributes(attributes);
}

void FuzzWithDisabled(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  auto* utils = CreateUtils(*holder);

  auto* input = MakeGarbageCollected<HTMLInputElement>(doc,
                                                        CreateElementFlags());
  input->setAttribute(html_names::kTypeAttr, AtomicString("text"));

  // Test with disabled attribute
  if (fdp->ConsumeBool()) {
    input->setAttribute(html_names::kDisabledAttr, AtomicString(""));
  }

  doc.body()->AppendChild(input);
  input->Focus();

  utils->CompositionEphemeralRange();
  utils->HasComposition();
  utils->Clear();

  HashMap<String, String> attributes;
  utils->GetInputElementAttributes(attributes);
}

void FuzzWithComposition(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  InputMethodController& imc = holder->GetFrame().GetInputMethodController();

  // Create input element with small maxlength to trigger is_too_long condition
  auto* input = MakeGarbageCollected<HTMLInputElement>(doc, CreateElementFlags());
  input->setAttribute(html_names::kTypeAttr, AtomicString("text"));
  int maxlength = fdp->ConsumeIntegralInRange<int>(1, 10);
  input->setAttribute(html_names::kMaxlengthAttr, AtomicString(String::Number(maxlength)));
  doc.body()->AppendChild(input);
  input->Focus();

  // Set composition text
  std::string comp_text = fdp->ConsumeRandomLengthString(32);
  imc.SetComposition(AtomicString(comp_text.c_str()), Vector<ImeTextSpan>(), 0, 0);

  // Call CommitText to trigger ReplaceCompositionEx
  // This will trigger the is_too_long condition when composition + text exceeds maxlength
  std::string commit_text = fdp->ConsumeRandomLengthString(32);
  imc.CommitText(AtomicString(commit_text.c_str()), Vector<ImeTextSpan>(), 0);
}

void FuzzWithCompositionAndMaxLength(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  InputMethodController& imc = holder->GetFrame().GetInputMethodController();

  // Create input element with maxlength
  auto* input = MakeGarbageCollected<HTMLInputElement>(doc, CreateElementFlags());
  input->setAttribute(html_names::kTypeAttr, AtomicString("text"));
  int maxlength = fdp->ConsumeIntegralInRange<int>(5, 20);
  input->setAttribute(html_names::kMaxlengthAttr, AtomicString(String::Number(maxlength)));
  doc.body()->AppendChild(input);
  input->Focus();

  // Set composition with text that might exceed maxlength
  std::string comp_text = fdp->ConsumeRandomLengthString(30);
  imc.SetComposition(AtomicString(comp_text.c_str()), Vector<ImeTextSpan>(), 0, 0);

  // Commit with different text lengths to test various scenarios
  std::string commit_text = fdp->ConsumeRandomLengthString(30);
  imc.CommitText(AtomicString(commit_text.c_str()), Vector<ImeTextSpan>(), 0);

  // Test utils methods after composition
  auto* utils = CreateUtils(*holder);
  utils->HasComposition();
  utils->CompositionEphemeralRange();
  utils->Clear();
}

void FuzzWithTextAreaComposition(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  InputMethodController& imc = holder->GetFrame().GetInputMethodController();

  // Create textarea element with maxlength
  auto* textarea = MakeGarbageCollected<HTMLTextAreaElement>(doc);
  int maxlength = fdp->ConsumeIntegralInRange<int>(5, 20);
  textarea->setAttribute(html_names::kMaxlengthAttr, AtomicString(String::Number(maxlength)));
  doc.body()->AppendChild(textarea);
  textarea->Focus();

  // Set composition
  std::string comp_text = fdp->ConsumeRandomLengthString(30);
  imc.SetComposition(AtomicString(comp_text.c_str()), Vector<ImeTextSpan>(), 0, 0);

  // Commit to trigger ReplaceCompositionEx
  std::string commit_text = fdp->ConsumeRandomLengthString(30);
  imc.CommitText(AtomicString(commit_text.c_str()), Vector<ImeTextSpan>(), 0);
}

void FuzzWithContentEditableComposition(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  InputMethodController& imc = holder->GetFrame().GetInputMethodController();

  // Create contenteditable div
  auto* div = MakeGarbageCollected<HTMLDivElement>(doc);
  div->setAttribute(html_names::kContenteditableAttr, AtomicString("true"));
  doc.body()->AppendChild(div);
  div->Focus();

  // Set composition
  std::string comp_text = fdp->ConsumeRandomLengthString(32);
  imc.SetComposition(AtomicString(comp_text.c_str()), Vector<ImeTextSpan>(), 0, 0);

  // Commit to trigger ReplaceCompositionEx
  std::string commit_text = fdp->ConsumeRandomLengthString(32);
  imc.CommitText(AtomicString(commit_text.c_str()), Vector<ImeTextSpan>(), 0);
}

void FuzzSetCompositionOnly(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  InputMethodController& imc = holder->GetFrame().GetInputMethodController();

  auto* input = MakeGarbageCollected<HTMLInputElement>(doc, CreateElementFlags());
  input->setAttribute(html_names::kTypeAttr, AtomicString("text"));
  doc.body()->AppendChild(input);
  input->Focus();

  // Test SetComposition with various parameters
  std::string text = fdp->ConsumeRandomLengthString(64);
  int selection_start = fdp->ConsumeIntegralInRange<int>(0, text.length());
  int selection_end = fdp->ConsumeIntegralInRange<int>(selection_start, text.length());
  imc.SetComposition(AtomicString(text.c_str()), Vector<ImeTextSpan>(), selection_start, selection_end);
}

void FuzzCommitTextWithoutComposition(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  InputMethodController& imc = holder->GetFrame().GetInputMethodController();

  auto* input = MakeGarbageCollected<HTMLInputElement>(doc, CreateElementFlags());
  input->setAttribute(html_names::kTypeAttr, AtomicString("text"));
  doc.body()->AppendChild(input);
  input->Focus();

  // Commit text without composition (should use InsertText instead of ReplaceCompositionEx)
  std::string text = fdp->ConsumeRandomLengthString(64);
  imc.CommitText(AtomicString(text.c_str()), Vector<ImeTextSpan>(), 0);
}

void FuzzFinishComposingTextWithKeepSelection(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  InputMethodController& imc = holder->GetFrame().GetInputMethodController();

  // Create input element with small maxlength to trigger is_too_long condition
  auto* input = MakeGarbageCollected<HTMLInputElement>(doc, CreateElementFlags());
  input->setAttribute(html_names::kTypeAttr, AtomicString("text"));
  int maxlength = fdp->ConsumeIntegralInRange<int>(1, 10);
  input->setAttribute(html_names::kMaxlengthAttr, AtomicString(String::Number(maxlength)));
  doc.body()->AppendChild(input);
  input->Focus();

  // Set composition with text that exceeds maxlength to trigger is_too_long
  std::string comp_text = fdp->ConsumeRandomLengthString(32);
  imc.SetComposition(AtomicString(comp_text.c_str()), Vector<ImeTextSpan>(), 0, 0);

  // Call FinishComposingText with kKeepSelection to trigger ReplaceCompositionEx
  // This path calls ReplaceCompositionEx when is_too_long is true
  imc.FinishComposingText(InputMethodController::kKeepSelection);
}

void FuzzFinishComposingTextWithKeepSelectionTextArea(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  InputMethodController& imc = holder->GetFrame().GetInputMethodController();

  // Create textarea element with small maxlength
  auto* textarea = MakeGarbageCollected<HTMLTextAreaElement>(doc);
  int maxlength = fdp->ConsumeIntegralInRange<int>(1, 10);
  textarea->setAttribute(html_names::kMaxlengthAttr, AtomicString(String::Number(maxlength)));
  doc.body()->AppendChild(textarea);
  textarea->Focus();

  // Set composition with text that exceeds maxlength
  std::string comp_text = fdp->ConsumeRandomLengthString(32);
  imc.SetComposition(AtomicString(comp_text.c_str()), Vector<ImeTextSpan>(), 0, 0);

  // Call FinishComposingText with kKeepSelection to trigger ReplaceCompositionEx
  imc.FinishComposingText(InputMethodController::kKeepSelection);
}

void FuzzFinishComposingTextWithKeepSelectionContentEditable(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  InputMethodController& imc = holder->GetFrame().GetInputMethodController();

  // Create contenteditable div
  auto* div = MakeGarbageCollected<HTMLDivElement>(doc);
  div->setAttribute(html_names::kContenteditableAttr, AtomicString("true"));
  doc.body()->AppendChild(div);
  div->Focus();

  // Set composition
  std::string comp_text = fdp->ConsumeRandomLengthString(32);
  imc.SetComposition(AtomicString(comp_text.c_str()), Vector<ImeTextSpan>(), 0, 0);

  // Call FinishComposingText with kKeepSelection
  // For contenteditable, is_too_long is usually false, but we still test the path
  imc.FinishComposingText(InputMethodController::kKeepSelection);
}

void FuzzFinishComposingTextBothBehaviors(FuzzedDataProvider* fdp) {
  auto holder = CreatePageHolder();
  if (!holder) return;

  Document& doc = holder->GetDocument();
  InputMethodController& imc = holder->GetFrame().GetInputMethodController();

  // Create input element with maxlength
  auto* input = MakeGarbageCollected<HTMLInputElement>(doc, CreateElementFlags());
  input->setAttribute(html_names::kTypeAttr, AtomicString("text"));
  int maxlength = fdp->ConsumeIntegralInRange<int>(1, 10);
  input->setAttribute(html_names::kMaxlengthAttr, AtomicString(String::Number(maxlength)));
  doc.body()->AppendChild(input);
  input->Focus();

  // Set composition
  std::string comp_text = fdp->ConsumeRandomLengthString(32);
  imc.SetComposition(AtomicString(comp_text.c_str()), Vector<ImeTextSpan>(), 0, 0);

  // Test both confirm behaviors
  if (fdp->ConsumeBool()) {
    imc.FinishComposingText(InputMethodController::kKeepSelection);
  } else {
    imc.FinishComposingText(InputMethodController::kDoNotKeepSelection);
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static BlinkFuzzerTestSupport test_support;
  test::TaskEnvironment task_environment;

  if (!data || size < sizeof(uint8_t)) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);
  uint8_t selector = fdp.ConsumeIntegralInRange<uint8_t>(0, 23);

  switch (selector) {
    case 0:
      FuzzPublicMethodsNoFocus(&fdp);
      break;
    case 1:
      FuzzWithFocusedInputElement(&fdp);
      break;
    case 2:
      FuzzWithFocusedTextAreaElement(&fdp);
      break;
    case 3:
      FuzzWithNonEditableElement(&fdp);
      break;
    case 4:
      FuzzMultipleElements(&fdp);
      break;
    case 5:
      FuzzClearAndHasComposition(&fdp);
      break;
    case 6:
      FuzzWithDifferentInputTypes(&fdp);
      break;
    case 7:
      FuzzWithContentEditable(&fdp);
      break;
    case 8:
      FuzzWithMaxLength(&fdp);
      break;
    case 9:
      FuzzWithPlaceholder(&fdp);
      break;
    case 10:
      FuzzWithReadOnly(&fdp);
      break;
    case 11:
      FuzzWithDisabled(&fdp);
      break;
    case 12:
      FuzzWithComposition(&fdp);
      break;
    case 13:
      FuzzWithCompositionAndMaxLength(&fdp);
      break;
    case 14:
      FuzzWithTextAreaComposition(&fdp);
      break;
    case 15:
      FuzzWithContentEditableComposition(&fdp);
      break;
    case 16:
      FuzzSetCompositionOnly(&fdp);
      break;
    case 17:
      FuzzCommitTextWithoutComposition(&fdp);
      break;
    case 18:
      // Test all scenarios
      FuzzPublicMethodsNoFocus(&fdp);
      FuzzWithFocusedInputElement(&fdp);
      FuzzWithFocusedTextAreaElement(&fdp);
      FuzzWithNonEditableElement(&fdp);
      FuzzWithComposition(&fdp);
      FuzzWithCompositionAndMaxLength(&fdp);
      FuzzWithTextAreaComposition(&fdp);
      break;
    case 19:
      FuzzFinishComposingTextWithKeepSelection(&fdp);
      break;
    case 20:
      FuzzFinishComposingTextWithKeepSelectionTextArea(&fdp);
      break;
    case 21:
      FuzzFinishComposingTextWithKeepSelectionContentEditable(&fdp);
      break;
    case 22:
      FuzzFinishComposingTextBothBehaviors(&fdp);
      break;
  }

  return 0;
}