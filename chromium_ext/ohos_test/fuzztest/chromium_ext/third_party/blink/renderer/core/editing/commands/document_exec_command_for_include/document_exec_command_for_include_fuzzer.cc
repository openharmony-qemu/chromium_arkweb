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

#include <fuzzer/FuzzedDataProvider.h>

#include <memory>

#include "base/command_line.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/commands/editing_command_type.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

#include "arkweb/chromium_ext/content/public/common/content_switches_ext.h"

namespace blink {

static std::unique_ptr<DummyPageHolder> g_page_holder = nullptr;

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();
    g_page_holder = std::make_unique<DummyPageHolder>();
  }

  ~Environment() {
    g_page_holder.reset();
  }

  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

void SetupDocumentForClipboard(Document& doc, bool enable_site_permission) {
  LocalFrame* frame = doc.GetFrame();
  if (!frame) {
    return;
  }
  Settings* settings = frame->GetSettings();
  if (!settings) {
    return;
  }
  settings->SetJavaScriptCanAccessClipboard(true);
  settings->SetClipboardSitePermissionEnabled(enable_site_permission);
  settings->SetDOMPasteAllowed(true);
}

void SetNWebExCommandLineSwitch(bool enabled) {
#if BUILDFLAG(ARKWEB_NWEB_EX)
  if (enabled) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableNwebEx);
  } else {
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kEnableNwebEx);
  }
#endif  // BUILDFLAG(ARKWEB_NWEB_EX)
}

void FuzzExecCommandCopy(Document& doc, FuzzedDataProvider* fdp) {
  bool focused = fdp->ConsumeBool();
  bool enable_nweb_ex = fdp->ConsumeBool();
  SetNWebExCommandLineSwitch(enable_nweb_ex);
  
  if (focused) {
    doc.GetFrame()->GetPage()->GetFocusController().SetActive(true);
    doc.GetFrame()->GetPage()->GetFocusController().SetFocused(true);
  } else {
    doc.GetFrame()->GetPage()->GetFocusController().SetActive(false);
    doc.GetFrame()->GetPage()->GetFocusController().SetFocused(false);
  }

  DummyExceptionStateForTesting exception_state;
  doc.execCommand("copy", false, "", exception_state);
  
  SetNWebExCommandLineSwitch(!enable_nweb_ex);
  doc.execCommand("copy", false, "", exception_state);
}

void FuzzExecCommandCut(Document& doc, FuzzedDataProvider* fdp) {
  bool focused = fdp->ConsumeBool();
  bool enable_nweb_ex = fdp->ConsumeBool();
  SetNWebExCommandLineSwitch(enable_nweb_ex);
  
  if (focused) {
    doc.GetFrame()->GetPage()->GetFocusController().SetActive(true);
    doc.GetFrame()->GetPage()->GetFocusController().SetFocused(true);
  } else {
    doc.GetFrame()->GetPage()->GetFocusController().SetActive(false);
    doc.GetFrame()->GetPage()->GetFocusController().SetFocused(false);
  }

  DummyExceptionStateForTesting exception_state;
  doc.execCommand("cut", false, "", exception_state);
}

void FuzzExecCommandPaste(Document& doc, FuzzedDataProvider* fdp) {
  bool focused = fdp->ConsumeBool();
  bool enable_nweb_ex = fdp->ConsumeBool();
  SetNWebExCommandLineSwitch(enable_nweb_ex);
  
  if (focused) {
    doc.GetFrame()->GetPage()->GetFocusController().SetActive(true);
    doc.GetFrame()->GetPage()->GetFocusController().SetFocused(true);
  } else {
    doc.GetFrame()->GetPage()->GetFocusController().SetActive(false);
    doc.GetFrame()->GetPage()->GetFocusController().SetFocused(false);
  }

  DummyExceptionStateForTesting exception_state;
  doc.execCommand("paste", false, "", exception_state);
}

void FuzzExecCommandSelectAll(Document& doc) {
  DummyExceptionStateForTesting exception_state;
  doc.execCommand("selectall", false, "", exception_state);
}

void FuzzExecCommandWithSitePermission(Document& doc, FuzzedDataProvider* fdp) {
  bool enable_site_permission = fdp->ConsumeBool();
  SetupDocumentForClipboard(doc, enable_site_permission);

  bool focused = fdp->ConsumeBool();
  bool enable_nweb_ex = fdp->ConsumeBool();
  SetNWebExCommandLineSwitch(enable_nweb_ex);
  
  if (focused) {
    doc.GetFrame()->GetPage()->GetFocusController().SetActive(true);
    doc.GetFrame()->GetPage()->GetFocusController().SetFocused(true);
  } else {
    doc.GetFrame()->GetPage()->GetFocusController().SetActive(false);
    doc.GetFrame()->GetPage()->GetFocusController().SetFocused(false);
  }

  int cmd = fdp->ConsumeIntegralInRange<int>(0, 2);
  DummyExceptionStateForTesting exception_state;
  switch (cmd) {
    case 0:
      doc.execCommand("copy", false, "", exception_state);
      break;
    case 1:
      doc.execCommand("cut", false, "", exception_state);
      break;
    case 2:
      doc.execCommand("paste", false, "", exception_state);
      break;
  }
}

void FuzzExecCommandNonClipboard(Document& doc, FuzzedDataProvider* fdp) {
  const char* commands[] = {"bold", "italic", "underline", "indent", "outdent", 
                           "fontsize", "fontname", "forecolor", "hilitecolor",
                           "createlink", "unlink", "removeformat", "inserthorizontalrule"};
  for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    DummyExceptionStateForTesting exception_state;
    doc.execCommand(commands[i], false, "", exception_state);
  }
}

void FuzzExecCommandWithValue(Document& doc, FuzzedDataProvider* fdp) {
  const char* commands_with_value[] = {
    "fontsize", "fontname", "forecolor", "hilitecolor", 
    "createlink", "indent", "outdent", "backcolor"
  };
  for (size_t i = 0; i < sizeof(commands_with_value) / sizeof(commands_with_value[0]); i++) {
    unsigned len = fdp->ConsumeIntegralInRange<unsigned>(0, 50);
    std::string value = fdp->ConsumeBytesAsString(len);
    DummyExceptionStateForTesting exception_state;
    doc.execCommand(commands_with_value[i], false, String::FromUTF8(value), exception_state);
  }
}

void FuzzExecCommandVariousStates(Document& doc, FuzzedDataProvider* fdp) {
  bool focused = fdp->ConsumeBool();
  bool active = fdp->ConsumeBool();
  bool enable_nweb_ex = fdp->ConsumeBool();
  bool enable_site_perm = fdp->ConsumeBool();
  
  SetNWebExCommandLineSwitch(enable_nweb_ex);
  SetupDocumentForClipboard(doc, enable_site_perm);
  
  doc.GetFrame()->GetPage()->GetFocusController().SetActive(active);
  doc.GetFrame()->GetPage()->GetFocusController().SetFocused(focused);
  
  const char* commands[] = {"copy", "cut", "paste", "selectall", 
                            "bold", "italic", "underline"};
  for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    DummyExceptionStateForTesting exception_state;
    doc.execCommand(commands[i], false, "", exception_state);
  }
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

  static blink::Environment env;

  FuzzedDataProvider fdp(data, size);

  blink::Document& doc = *blink::g_page_holder->GetFrame().GetDocument();

  int selector = fdp.ConsumeIntegralInRange<int>(0, 6);
  switch (selector) {
    case 0:
      blink::FuzzExecCommandCopy(doc, &fdp);
      break;
    case 1:
      blink::FuzzExecCommandCut(doc, &fdp);
      break;
    case 2:
      blink::FuzzExecCommandPaste(doc, &fdp);
      break;
    case 3:
      blink::FuzzExecCommandSelectAll(doc);
      break;
    case 4:
      blink::FuzzExecCommandWithSitePermission(doc, &fdp);
      break;
    case 5:
      blink::FuzzExecCommandNonClipboard(doc, &fdp);
      break;
    case 6:
      blink::FuzzExecCommandWithValue(doc, &fdp);
      break;
  }

  return 0;
}