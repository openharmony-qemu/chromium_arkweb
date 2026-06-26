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

#include "base/command_line.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/page/chrome_client_impl_ext.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

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

void FuzzChromeClientImplExt(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }

  LocalFrame* frame = &g_page_holder->GetFrame();
  if (!frame) {
    return;
  }

  Page* page = frame->View()->GetPage();
  if (!page) {
    return;
  }

  ChromeClient& chrome_client = page->GetChromeClient();
  ChromeClientImpl* client_impl = DynamicTo<ChromeClientImpl>(chrome_client);
  if (!client_impl) {
    return;
  }

  ChromeClientImplExt* client = client_impl->AsChromeClientImplExt();
  if (!client) {
    return;
  }

#if BUILDFLAG(ARKWEB_SCROLLBAR_AVOID_AREA)
  double margin = fdp->ConsumeFloatingPoint<double>();
  client->SetScrollbarAvoidAreaTop(margin);
  client->SetScrollbarAvoidAreaBottom(margin);
#endif

#if BUILDFLAG(ARKWEB_DRAG_DROP)
  gfx::Rect rect = client->GetVisibleRectToWeb(frame);
  (void)rect;
#endif

#if BUILDFLAG(ARKWEB_AI)
  uint32_t fold_status = client->GetFoldStatus(frame);
  (void)fold_status;

  std::string text_str = fdp->ConsumeRandomLengthString();
  String text = String::FromUTF8(text_str);
  int8_t offset = fdp->ConsumeIntegral<int8_t>();
  Vector<int8_t> word_selection = client->GetWordSelection(frame, text, offset);
  (void)word_selection;
#endif
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

  static blink::Environment env;

  FuzzedDataProvider fdp(data, size);

  blink::FuzzChromeClientImplExt(&fdp);

  return 0;
}