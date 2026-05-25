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

#include "arkweb/chromium_ext/third_party/blink/renderer/core/html/html_plugin_element_utils.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

constexpr size_t kMaxFuzzStringLength = 512;

static frame_test_helpers::WebViewHelper* g_web_view_helper = nullptr;

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();

    g_web_view_helper = new frame_test_helpers::WebViewHelper();
    g_web_view_helper->Initialize();

    auto* frame = g_web_view_helper->LocalMainFrame()->GetFrame();
    CHECK(frame);
    auto* settings = frame->GetSettings();
    CHECK(settings);
    settings->SetNativeEmbedModeEnabled(true);
    settings->RegisterNativeEmbedRule(
        WebString::FromUTF8(web_pref::kEmbedTag),
        WebString::FromUTF8("test/"));

    auto* doc = static_cast<Document*>(
        g_web_view_helper->LocalMainFrame()->GetDocument());
    CHECK(doc);
    CHECK(doc->body());
    doc->body()->SetHTMLUnsafeWithoutTrustedTypes(
        "<embed id='test-embed' type='test/native'>"
        "<embed id='test-embed-frame' type='text/html' src='about:blank'>");
  }

  ~Environment() {
    delete g_web_view_helper;
    g_web_view_helper = nullptr;
  }

  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

HTMLEmbedElement* GetEmbedElementById(const char* id) {
  if (!g_web_view_helper) return nullptr;
  auto* doc = static_cast<Document*>(
      g_web_view_helper->LocalMainFrame()->GetDocument());
  if (!doc) return nullptr;
  auto* element = doc->getElementById(AtomicString(id));
  if (!element) return nullptr;
  return DynamicTo<HTMLEmbedElement>(element);
}

Settings* GetSettings() {
  if (!g_web_view_helper) return nullptr;
  auto* frame = g_web_view_helper->LocalMainFrame()->GetFrame();
  if (!frame) return nullptr;
  return frame->GetSettings();
}

void RestoreTestEnvironment() {
  auto* frame = g_web_view_helper->LocalMainFrame()->GetFrame();
  if (!frame) return;
  auto* settings = frame->GetSettings();
  if (settings) {
    settings->SetNativeEmbedModeEnabled(true);
    settings->RegisterNativeEmbedRule(
        WebString::FromUTF8(web_pref::kEmbedTag),
        WebString::FromUTF8("test/"));
  }
  auto* doc = static_cast<Document*>(
      g_web_view_helper->LocalMainFrame()->GetDocument());
  if (doc && doc->body()) {
    doc->body()->SetHTMLUnsafeWithoutTrustedTypes(
        "<embed id='test-embed' type='test/native'>"
        "<embed id='test-embed-frame' type='text/html' src='about:blank'>");
  }
}

Element::AttributeModificationParams BuildStretchParams(
    const AtomicString& old_value,
    const AtomicString& new_value) {
  return Element::AttributeModificationParams(
      html_names::kArkwebnativestyleAttr,
      old_value,
      new_value,
      Element::AttributeModificationReason::kDirectly);
}

#if BUILDFLAG(ARKWEB_SAME_LAYER)

void FuzzCheckNativeType(FuzzedDataProvider* fdp) {
  auto* settings = GetSettings();
  if (!settings) return;

  uint8_t sub = fdp->ConsumeIntegralInRange<uint8_t>(0, 6);

  const char* embed_id = (sub == 6) ? "test-embed-frame" : "test-embed";
  auto* embed = GetEmbedElementById(embed_id);
  if (!embed) return;

  auto* utils = MakeGarbageCollected<HTMLPlugInElementUtils>(embed);
  Persistent<HTMLPlugInElementUtils> utils_ref(utils);

  switch (sub) {
    case 0: {
      settings->SetNativeEmbedModeEnabled(false);
      utils->CheckNativeType(web_pref::kEmbedTag);
      settings->SetNativeEmbedModeEnabled(true);
      break;
    }
    case 1: {
      utils->CheckNativeType("nonexistent-tag");
      break;
    }
    case 2: {
      settings->RegisterNativeEmbedRule(
          WebString::FromUTF8(web_pref::kEmbedTag),
          WebString::FromUTF8("wrong-prefix/"));
      utils->CheckNativeType(web_pref::kEmbedTag);
      settings->RegisterNativeEmbedRule(
          WebString::FromUTF8(web_pref::kEmbedTag),
          WebString::FromUTF8("test/"));
      break;
    }
    case 3: {
      utils->CheckNativeType(web_pref::kEmbedTag);
      break;
    }
    case 4: {
      static const char* kTagNames[] = {"embed", "object", "div", "span"};
      int pick = fdp->ConsumeIntegralInRange<int>(0, 3);
      utils->CheckNativeType(kTagNames[pick]);
      break;
    }
    case 5: {
      utils->CheckNativeType("");
      break;
    }
    case 6: {
      utils->CheckNativeType(web_pref::kEmbedTag);
      break;
    }
  }
}

void FuzzCheckNativeTypeInactiveDoc(FuzzedDataProvider* fdp) {
  if (!g_web_view_helper) return;

  auto* embed = GetEmbedElementById("test-embed");
  if (!embed) return;

  Persistent<HTMLEmbedElement> embed_ref(embed);

  frame_test_helpers::LoadFrame(
      g_web_view_helper->LocalMainFrame(), "about:blank");

  if (embed_ref.Get()) {
    auto* utils = MakeGarbageCollected<HTMLPlugInElementUtils>(embed_ref.Get());
    Persistent<HTMLPlugInElementUtils> utils_ref(utils);
    utils->CheckNativeType("embed");
  }

  RestoreTestEnvironment();
}

void FuzzTraceCoverage(FuzzedDataProvider* fdp) {
  auto* embed = GetEmbedElementById("test-embed");
  if (!embed) return;

  auto* utils = MakeGarbageCollected<HTMLPlugInElementUtils>(embed);
  Persistent<HTMLPlugInElementUtils> utils_ref(utils);

  ThreadState::Current()->CollectAllGarbageForTesting();
}

void FuzzAnalysisStretchContentToFillBounds(FuzzedDataProvider* fdp) {
  auto* embed = GetEmbedElementById("test-embed");
  if (!embed) return;

  auto* utils = MakeGarbageCollected<HTMLPlugInElementUtils>(embed);
  Persistent<HTMLPlugInElementUtils> utils_ref(utils);

  static const char* kKnownStyleValues[] = {
      "",
      "object-fit:none",
      "object-fit:stretch",
      "object-fit: fill",
      "object-fit:cover",
      "object-fit:contain",
      "  object-fit  :  none  ",
      "prefix:val;object-fit:none;suffix:val",
      "OBJECT-FIT:NONE",
      "object-fit : stretch ; other: val",
      "key1:val1;key2:val2",
      ";;;",
      ":::;;;",
  };

  int idx = fdp->ConsumeIntegralInRange<int>(0, 12);
  int num_calls = fdp->ConsumeIntegralInRange<int>(1, 5);
  for (int i = 0; i < num_calls && fdp->remaining_bytes() > 0; ++i) {
    int pick = fdp->ConsumeIntegralInRange<int>(0, 12);
    AtomicString new_val(kKnownStyleValues[pick]);
    auto params = BuildStretchParams(g_null_atom, new_val);
    utils->AnalysisStretchContentToFillBounds(params);
  }
}

void FuzzAnalysisStretchWithOldNewValues(FuzzedDataProvider* fdp) {
  auto* embed = GetEmbedElementById("test-embed");
  if (!embed) return;

  auto* utils = MakeGarbageCollected<HTMLPlugInElementUtils>(embed);
  Persistent<HTMLPlugInElementUtils> utils_ref(utils);

  static const char* kStyleValues[] = {
      "",
      "object-fit:none",
      "object-fit:stretch",
      "key1:val1;key2:val2",
      ";;;",
  };

  int old_idx = fdp->ConsumeIntegralInRange<int>(0, 4);
  int new_idx = fdp->ConsumeIntegralInRange<int>(0, 4);

  AtomicString old_val(kStyleValues[old_idx]);
  AtomicString new_val(kStyleValues[new_idx]);
  auto params = BuildStretchParams(old_val, new_val);
  utils->AnalysisStretchContentToFillBounds(params);
}

void FuzzOverlayAndStretch(FuzzedDataProvider* fdp) {
  auto* embed = GetEmbedElementById("test-embed");
  if (!embed) return;

  auto* utils = MakeGarbageCollected<HTMLPlugInElementUtils>(embed);
  Persistent<HTMLPlugInElementUtils> utils_ref(utils);

  int num_ops = fdp->ConsumeIntegralInRange<int>(1, 30);
  for (int i = 0; i < num_ops && fdp->remaining_bytes() > 2; ++i) {
    uint8_t op = fdp->ConsumeIntegralInRange<uint8_t>(0, 8);
    switch (op) {
      case 0:
        utils->SetNativeEmbedOverlay(fdp->ConsumeBool());
        break;
      case 1:
        utils->SetNativeEmbedOverlayInfinity(fdp->ConsumeBool());
        break;
      case 2:
        utils->SetStretchContentToFillBounds(fdp->ConsumeBool());
        break;
      case 3:
        utils->ProcessStretchContentToFillBounds();
        break;
      case 4: {
        static const char* kQuickStyles[] = {
            "", "object-fit:none", "object-fit:stretch", "key1:val1"};
        int pick = fdp->ConsumeIntegralInRange<int>(0, 3);
        AtomicString new_val(kQuickStyles[pick]);
        auto params = BuildStretchParams(g_null_atom, new_val);
        utils->AnalysisStretchContentToFillBounds(params);
        break;
      }
      case 5:
        utils->SetNativeEmbedOverlay(false);
        utils->SetNativeEmbedOverlay(false);
        utils->SetNativeEmbedOverlay(true);
        utils->SetNativeEmbedOverlay(true);
        break;
      case 6:
        utils->SetNativeEmbedOverlayInfinity(false);
        utils->SetNativeEmbedOverlayInfinity(false);
        utils->SetNativeEmbedOverlayInfinity(true);
        utils->SetNativeEmbedOverlayInfinity(true);
        break;
      case 7:
        utils->IsOverlay();
        utils->IsOverlayInfinity();
        break;
      case 8:
        utils->SetStretchContentToFillBounds(true);
        utils->SetStretchContentToFillBounds(false);
        utils->ProcessStretchContentToFillBounds();
        break;
    }
  }
}

void FuzzProcessParamChanges(FuzzedDataProvider* fdp) {
  auto* embed = GetEmbedElementById("test-embed");
  if (!embed) return;

  auto* utils = MakeGarbageCollected<HTMLPlugInElementUtils>(embed);
  Persistent<HTMLPlugInElementUtils> utils_ref(utils);

  static const char* kParamIds[] = {"p1", "p2", "p3", "p4"};
  static const char* kParamNames[] = {"name", "type", "value", "data"};
  static const char* kParamValues[] = {"val1", "val2", "test", "hello"};

  int num_params = fdp->ConsumeIntegralInRange<int>(0, 15);
  Vector<ParamChangeInfo> changes;

  for (int i = 0; i < num_params && fdp->remaining_bytes() > 4; ++i) {
    int status_int = fdp->ConsumeIntegralInRange<int>(0, 2);
    auto status = static_cast<ParamChangeInfo::Status>(status_int);

    int id_pick = fdp->ConsumeIntegralInRange<int>(0, 3);
    int name_pick = fdp->ConsumeIntegralInRange<int>(0, 3);
    int value_pick = fdp->ConsumeIntegralInRange<int>(0, 3);

    changes.emplace_back(status,
                         AtomicString(kParamIds[id_pick]),
                         AtomicString(kParamNames[name_pick]),
                         AtomicString(kParamValues[value_pick]));
  }

  utils->ProcessParamChanges(changes);
}

void FuzzBufferedParamChanges(FuzzedDataProvider* fdp) {
  auto* embed = GetEmbedElementById("test-embed");
  if (!embed) return;

  auto* utils = MakeGarbageCollected<HTMLPlugInElementUtils>(embed);
  Persistent<HTMLPlugInElementUtils> utils_ref(utils);

  static const char* kParamIds[] = {"p1", "p2", "p3"};
  static const char* kParamNames[] = {"name", "type", "value"};
  static const char* kParamValues[] = {"val1", "val2", "val3"};

  int num_rounds = fdp->ConsumeIntegralInRange<int>(1, 10);
  for (int r = 0; r < num_rounds && fdp->remaining_bytes() > 4; ++r) {
    int num_params = fdp->ConsumeIntegralInRange<int>(0, 5);
    Vector<ParamChangeInfo> changes;

    for (int i = 0; i < num_params && fdp->remaining_bytes() > 4; ++i) {
      int status_int = fdp->ConsumeIntegralInRange<int>(0, 2);
      auto status = static_cast<ParamChangeInfo::Status>(status_int);

      int id_pick = fdp->ConsumeIntegralInRange<int>(0, 2);
      int name_pick = fdp->ConsumeIntegralInRange<int>(0, 2);
      int value_pick = fdp->ConsumeIntegralInRange<int>(0, 2);

      changes.emplace_back(status,
                           AtomicString(kParamIds[id_pick]),
                           AtomicString(kParamNames[name_pick]),
                           AtomicString(kParamValues[value_pick]));
    }

    utils->ProcessParamChanges(changes);
  }

  utils->ProcessBufferedParamChanges();
  utils->ProcessBufferedParamChanges();
}

void FuzzCheckIntrinsicSizeAndCssDisplay(FuzzedDataProvider* fdp) {
  auto* embed = GetEmbedElementById("test-embed");
  if (!embed) return;

  auto* utils = MakeGarbageCollected<HTMLPlugInElementUtils>(embed);
  Persistent<HTMLPlugInElementUtils> utils_ref(utils);

  auto* settings = GetSettings();
  if (!settings) return;

  int num_checks = fdp->ConsumeIntegralInRange<int>(1, 10);
  for (int i = 0; i < num_checks && fdp->remaining_bytes() > 0; ++i) {
    uint8_t op = fdp->ConsumeIntegralInRange<uint8_t>(0, 7);
    switch (op) {
      case 0:
        utils->CheckIntrinsicSizeEnable();
        break;
      case 1: {
        settings->SetNativeEmbedModeEnabled(false);
        utils->CheckIntrinsicSizeEnable();
        settings->SetNativeEmbedModeEnabled(true);
        break;
      }
      case 2: {
        settings->SetIntrinsicSizeEnabled(true);
        utils->CheckIntrinsicSizeEnable();
        settings->SetIntrinsicSizeEnabled(false);
        break;
      }
      case 3:
        utils->IsCssDisplayChangeEnabled();
        break;
      case 4: {
        settings->SetNativeEmbedModeEnabled(false);
        utils->IsCssDisplayChangeEnabled();
        settings->SetNativeEmbedModeEnabled(true);
        break;
      }
      case 5: {
        settings->SetCSSDisplayChangeEnabled(true);
        utils->IsCssDisplayChangeEnabled();
        settings->SetCSSDisplayChangeEnabled(false);
        break;
      }
      case 6: {
        settings->SetCSSDisplayChangeEnabled(true);
        utils->IsCssDisplayChangeEnabled();
        settings->SetCSSDisplayChangeEnabled(false);
        break;
      }
      case 7: {
        settings->SetIntrinsicSizeEnabled(true);
        settings->SetCSSDisplayChangeEnabled(true);
        utils->CheckIntrinsicSizeEnable();
        utils->IsCssDisplayChangeEnabled();
        settings->SetIntrinsicSizeEnabled(false);
        settings->SetCSSDisplayChangeEnabled(false);
        break;
      }
    }
  }
}

void FuzzMultipleStretchAnalysis(FuzzedDataProvider* fdp) {
  auto* embed = GetEmbedElementById("test-embed");
  if (!embed) return;

  auto* utils = MakeGarbageCollected<HTMLPlugInElementUtils>(embed);
  Persistent<HTMLPlugInElementUtils> utils_ref(utils);

  static const char* kStyleValues[] = {
      "",       "object-fit:none",      "object-fit:stretch",
      "object-fit: fill",              "object-fit:cover",
      "object-fit:contain",            "  object-fit  :  none  ",
      "prefix:val;object-fit:none;suffix:val",
      "OBJECT-FIT:NONE",               "object-fit : stretch ; other: val",
      "key1:val1;key2:val2",           ";;;",
      ":::;;;",
  };

  int num_iterations = fdp->ConsumeIntegralInRange<int>(1, 20);
  for (int i = 0; i < num_iterations && fdp->remaining_bytes() > 0; ++i) {
    int pick = fdp->ConsumeIntegralInRange<int>(0, 12);
    AtomicString new_val(kStyleValues[pick]);
    auto params = BuildStretchParams(g_null_atom, new_val);
    utils->AnalysisStretchContentToFillBounds(params);
  }
}

#endif  // BUILDFLAG(ARKWEB_SAME_LAYER)

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(int32_t)) return 0;

  static blink::Environment env;

  FuzzedDataProvider fdp(data, size);
  int32_t scenario = fdp.ConsumeIntegralInRange<int32_t>(0, 9);

#if BUILDFLAG(ARKWEB_SAME_LAYER)
  switch (scenario) {
    case 0:
      blink::FuzzCheckNativeType(&fdp);
      break;
    case 1:
      blink::FuzzCheckNativeTypeInactiveDoc(&fdp);
      break;
    case 2:
      blink::FuzzTraceCoverage(&fdp);
      break;
    case 3:
      blink::FuzzAnalysisStretchContentToFillBounds(&fdp);
      break;
    case 4:
      blink::FuzzAnalysisStretchWithOldNewValues(&fdp);
      break;
    case 5:
      blink::FuzzOverlayAndStretch(&fdp);
      break;
    case 6:
      blink::FuzzProcessParamChanges(&fdp);
      break;
    case 7:
      blink::FuzzBufferedParamChanges(&fdp);
      break;
    case 8:
      blink::FuzzCheckIntrinsicSizeAndCssDisplay(&fdp);
      break;
    case 9:
      blink::FuzzMultipleStretchAnalysis(&fdp);
      break;
    default:
      break;
  }
#endif  // BUILDFLAG(ARKWEB_SAME_LAYER)

  return 0;
}
