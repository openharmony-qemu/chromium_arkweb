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

#include "arkweb/chromium_ext/third_party/blink/renderer/core/frame/arkweb_local_frame_mojo_handler_ext.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <cstdint>
#include <memory>
#include <string>

#include "arkweb/build/features/features.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

namespace blink {

namespace {
constexpr uint8_t MAX_STRING_LENGTH = 255;
constexpr int32_t MAX_INT32_SIZE = 255;
constexpr int32_t MIN_INT32_SIZE = -255;
}  // namespace

static std::unique_ptr<DummyPageHolder> g_page_holder = nullptr;

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();
    g_page_holder = std::make_unique<DummyPageHolder>();
  }

  ~Environment() { g_page_holder.reset(); }

  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

class ArkWebLocalFrameMojoHandlerExtFuzzerHelper {
 public:
#if BUILDFLAG(ARKWEB_MENU) || BUILDFLAG(IS_ARKWEB_EXT)
  static void GetImageByXPath(ArkWebLocalFrameMojoHandlerExt* handler,
                               int32_t taskid,
                               const String& xpath,
                               LocalFrameMojoHandler::GetImageByXPathCallback callback) {
    handler->GetImageByXPath(taskid, xpath, std::move(callback));
  }

  static void GetAllImage(ArkWebLocalFrameMojoHandlerExt* handler,
                           int32_t taskid,
                           const String& url,
                           LocalFrameMojoHandler::GetAllImageCallback callback) {
    handler->GetAllImage(taskid, url, std::move(callback));
  }

  static void GetImageFromCache(ArkWebLocalFrameMojoHandlerExt* handler,
                                 const String& url,
                                 LocalFrameMojoHandler::GetImageFromCacheCallback callback) {
    handler->GetImageFromCache(url, std::move(callback));
  }
#endif  // BUILDFLAG(ARKWEB_MENU) || BUILDFLAG(IS_ARKWEB_EXT)

#if BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)
  static void DetectBlankScreen(ArkWebLocalFrameMojoHandlerExt* handler,
                                 const String& url,
                                 const Vector<double>& detection_timing,
                                 const Vector<int32_t>& detection_methods,
                                 int32_t threshold) {
    handler->DetectBlankScreen(url, detection_timing, detection_methods,
                               threshold);
  }
#endif  // BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)
};

#if BUILDFLAG(ARKWEB_MENU) || BUILDFLAG(IS_ARKWEB_EXT)
void FuzzGetImageByXPath(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();
  auto* handler = MakeGarbageCollected<ArkWebLocalFrameMojoHandlerExt>(frame);
  CHECK(handler);

  int32_t taskid =
      fdp->ConsumeIntegralInRange<int32_t>(MIN_INT32_SIZE, MAX_INT32_SIZE);
  std::string xpath_str = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  String xpath = String::FromUTF8(xpath_str.c_str());
  auto callback = base::BindOnce(
      [](uint32_t, base::ReadOnlySharedMemoryRegion, uint32_t, uint32_t) {});

  ArkWebLocalFrameMojoHandlerExtFuzzerHelper::GetImageByXPath(
      handler, taskid, xpath, std::move(callback));
}

void FuzzGetAllImage(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();
  auto* handler = MakeGarbageCollected<ArkWebLocalFrameMojoHandlerExt>(frame);
  CHECK(handler);

  int32_t taskid =
      fdp->ConsumeIntegralInRange<int32_t>(MIN_INT32_SIZE, MAX_INT32_SIZE);
  std::string url_str = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  String url = String::FromUTF8(url_str.c_str());

  auto callback = base::BindOnce(
      [](uint32_t, base::ReadOnlySharedMemoryRegion) {});

  ArkWebLocalFrameMojoHandlerExtFuzzerHelper::GetAllImage(
      handler, taskid, url, std::move(callback));
}

void FuzzGetImageFromCache(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();
  auto* handler = MakeGarbageCollected<ArkWebLocalFrameMojoHandlerExt>(frame);
  CHECK(handler);

  std::string url_str = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  String url = String::FromUTF8(url_str.c_str());

  auto callback = base::BindOnce(
      [](uint32_t, base::ReadOnlySharedMemoryRegion) {});

  ArkWebLocalFrameMojoHandlerExtFuzzerHelper::GetImageFromCache(
      handler, url, std::move(callback));
}
#endif  // BUILDFLAG(ARKWEB_MENU) || BUILDFLAG(IS_ARKWEB_EXT)

#if BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)
void FuzzDetectBlankScreen(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();
  auto* handler = MakeGarbageCollected<ArkWebLocalFrameMojoHandlerExt>(frame);
  CHECK(handler);

  std::string url_str = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  String url = String::FromUTF8(url_str.c_str());

  size_t timing_count = fdp->ConsumeIntegralInRange<size_t>(0, 10);
  Vector<double> detection_timing;
  detection_timing.reserve(timing_count);
  for (size_t i = 0; i < timing_count; ++i) {
    detection_timing.push_back(fdp->ConsumeFloatingPoint<double>());
  }

  size_t methods_count = fdp->ConsumeIntegralInRange<size_t>(0, 10);
  Vector<int32_t> detection_methods;
  detection_methods.reserve(methods_count);
  for (size_t i = 0; i < methods_count; ++i) {
    detection_methods.push_back(fdp->ConsumeIntegral<int32_t>());
  }

  int32_t threshold =
      fdp->ConsumeIntegralInRange<int32_t>(MIN_INT32_SIZE, MAX_INT32_SIZE);

  ArkWebLocalFrameMojoHandlerExtFuzzerHelper::DetectBlankScreen(
      handler, url, detection_timing, detection_methods, threshold);
}
#endif  // BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

  static blink::Environment env;

  FuzzedDataProvider fdp(data, size);

  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 3);

  switch (selector) {
#if BUILDFLAG(ARKWEB_MENU) || BUILDFLAG(IS_ARKWEB_EXT)
    case 0:
      blink::FuzzGetImageByXPath(&fdp);
      break;
    case 1:
      blink::FuzzGetAllImage(&fdp);
      break;
    case 2:
      blink::FuzzGetImageFromCache(&fdp);
      break;
#endif  // BUILDFLAG(ARKWEB_MENU) || BUILDFLAG(IS_ARKWEB_EXT)
#if BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)
    case 3:
      blink::FuzzDetectBlankScreen(&fdp);
      break;
#endif  // BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)
    default:
#if BUILDFLAG(ARKWEB_MENU) || BUILDFLAG(IS_ARKWEB_EXT)
      blink::FuzzGetImageByXPath(&fdp);
#endif  // BUILDFLAG(ARKWEB_MENU) || BUILDFLAG(IS_ARKWEB_EXT)
      break;
  }

  return 0;
}