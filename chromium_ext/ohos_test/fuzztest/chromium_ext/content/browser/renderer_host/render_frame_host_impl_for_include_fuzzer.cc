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

#include <cstdint>
#include <fcntl.h>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "arkweb/build/features/features.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/network_service_instance_impl.h"

#ifdef BUILDFLAG_INTERNAL_ARKWEB_TEST
#undef BUILDFLAG_INTERNAL_ARKWEB_TEST
#endif
#define BUILDFLAG_INTERNAL_ARKWEB_TEST() (1)

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/test/test_render_frame_host.h"
#include "mojo/core/embedder/embedder.h"
#include "net/base/mock_network_change_notifier.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/events/devices/device_data_manager.h"

#if BUILDFLAG(ARKWEB_JS_ON_DOCUMENT_END)
#include "arkweb/ohos_nweb/src/capi/nweb_extension_javascript_item.h"
#endif  // BUILDFLAG(ARKWEB_JS_ON_DOCUMENT_END)

namespace content {

extern void CommitNavigationExt(const std::string& effective_scheme,
    ContentBrowserClient::NonNetworkURLLoaderFactoryMap& non_network_factories,
    BrowserContext* browser_context);

class RenderFrameHostImplForIncludeTest {
 public:
  static bool CanExecuteJavaScript(RenderFrameHostImpl* rfh) {
    return rfh->CanExecuteJavaScript();
  }
  static void OnPdfScrollAtBottom(RenderFrameHostImpl* rfh,
                                  const std::string& url) {
    rfh->OnPdfScrollAtBottom(url);
  }
  static void OnPdfLoadEvent(RenderFrameHostImpl* rfh,
                             int32_t result,
                             const std::string& url) {
    rfh->OnPdfLoadEvent(result, url);
  }
  static net::Error GetNetErrorCode(RenderFrameHostImpl* rfh) {
    return rfh->GetNetErrorCode();
  }
};

namespace {

constexpr size_t kMaxStringLen = 256;
constexpr size_t kMaxVectorLen = 32;

class FuzzerEnv {
 public:
  FuzzerEnv()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        content_client_initializer_(),
        web_contents_factory_() {
    ui::DeviceDataManager::CreateInstance();
    aura_test_helper_ = std::make_unique<aura::test::AuraTestHelper>(
        ImageTransportFactory::GetInstance()->GetContextFactory());
    aura_test_helper_->SetUp();
    network_change_notifier_ =
        net::test::MockNetworkChangeNotifier::Create();
    browser_context_ = std::make_unique<TestBrowserContext>();
    web_contents_ = web_contents_factory_.CreateWebContents(
        browser_context_.get());
    rfh_ = static_cast<TestRenderFrameHost*>(
        web_contents_->GetPrimaryMainFrame());
    GpuDataManagerImpl::GetInstance();
    if (rfh_) {
      RenderFrameHost::AllowInjectingJavaScript();
      rfh_->InitializeRenderFrameIfNeeded();
    }
  }

  TestRenderFrameHost* rfh() { return rfh_; }
  BrowserContext* browser_context() { return browser_context_.get(); }

 private:
  struct CommandLineInitializer {
    CommandLineInitializer() { base::CommandLine::Init(0, nullptr); }
  };
  struct MojoInitializer {
    MojoInitializer() {
      ForceCreateNetworkServiceDirectlyForTesting();
      StoragePartitionImpl::ForceInProcessStorageServiceForTesting();
      mojo::core::Init(
          mojo::core::Configuration{.is_broker_process = true});
    }
  };
  CommandLineInitializer cmdline_init_;
  MojoInitializer mojo_init_;
  BrowserTaskEnvironment task_environment_;
  TestContentClientInitializer content_client_initializer_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  TestWebContentsFactory web_contents_factory_;
  std::unique_ptr<aura::test::AuraTestHelper> aura_test_helper_;
  std::unique_ptr<net::test::MockNetworkChangeNotifier>
      network_change_notifier_;
  WebContents* web_contents_ = nullptr;
  TestRenderFrameHost* rfh_ = nullptr;
};

void FuzzGetWorldId(FuzzedDataProvider& fdp, TestRenderFrameHost* rfh) {
  int32_t world_id = 0;
  std::string name = fdp.ConsumeRandomLengthString(kMaxStringLen);
  rfh->GetWorldId(name, &world_id);
  rfh->GetWorldId(std::string(), &world_id);
  std::string name2 = fdp.ConsumeRandomLengthString(kMaxStringLen);
  rfh->GetWorldId(name2, &world_id);
  rfh->GetWorldId(name2, &world_id);
}

void FuzzCommitNavigationExt(FuzzedDataProvider& fdp,
                             BrowserContext* browser_context) {
  ContentBrowserClient::NonNetworkURLLoaderFactoryMap factories;
  uint8_t choice = fdp.ConsumeIntegral<uint8_t>();
  std::string scheme;
  if (choice % 2 == 0) {
    scheme = url::kResourcesScheme;
  } else {
    scheme = fdp.ConsumeRandomLengthString(kMaxStringLen);
  }
  CommitNavigationExt(scheme, factories, browser_context);
}

void FuzzGetAllImage(FuzzedDataProvider& fdp, TestRenderFrameHost* rfh) {
  int32_t task_id = fdp.ConsumeIntegral<int32_t>();
  std::string url = fdp.ConsumeRandomLengthString(kMaxStringLen);
  rfh->GetAllImage(task_id, url, base::DoNothing());
}

void FuzzGetImageByXPath(FuzzedDataProvider& fdp, TestRenderFrameHost* rfh) {
  int32_t task_id = fdp.ConsumeIntegral<int32_t>();
  std::string xpath = fdp.ConsumeRandomLengthString(kMaxStringLen);
  rfh->GetImageByXPath(task_id, xpath, base::DoNothing());
}

void FuzzExecuteJavaScriptInFrames(FuzzedDataProvider& fdp,
                                   TestRenderFrameHost* rfh) {
  if (!RenderFrameHostImplForIncludeTest::CanExecuteJavaScript(rfh)) {
    return;
  }
  std::string js_bytes = fdp.ConsumeRandomLengthString(kMaxStringLen);
  std::u16string javascript(js_bytes.begin(), js_bytes.end());
  bool recursive = fdp.ConsumeBool();
  std::string world_name = fdp.ConsumeRandomLengthString(kMaxStringLen);
  rfh->ExecuteJavaScriptInFrames(javascript, recursive, world_name,
                                  base::DoNothing());
}

#if BUILDFLAG(IS_ARKWEB)
void FuzzExecuteJavaScriptExt(FuzzedDataProvider& fdp,
                              TestRenderFrameHost* rfh) {
  if (!RenderFrameHostImplForIncludeTest::CanExecuteJavaScript(rfh)) {
    return;
  }
  uint64_t script_length = fdp.ConsumeIntegral<uint64_t>();
  int fd = open("/dev/null", O_RDWR);
  if (fd < 0) {
    return;
  }
  rfh->ExecuteJavaScriptExt(fd, script_length, base::DoNothing());
}
#endif  // BUILDFLAG(IS_ARKWEB)

#if BUILDFLAG(ARKWEB_ACCESSIBILITY)
void FuzzSendAccessibilityEvent(FuzzedDataProvider& fdp,
                                TestRenderFrameHost* rfh) {
  int64_t accessibility_id = fdp.ConsumeIntegral<int64_t>();
  int32_t event_type = fdp.ConsumeIntegral<int32_t>();
  std::string argument = fdp.ConsumeRandomLengthString(kMaxStringLen);
  rfh->SendAccessibilityEvent(accessibility_id, event_type, argument);
}
#endif  // BUILDFLAG(ARKWEB_ACCESSIBILITY)

#if BUILDFLAG(ARKWEB_EXT_FREE_COPY)
void FuzzNotifyContextMenuWillShow(TestRenderFrameHost* rfh) {
  rfh->NotifyContextMenuWillShow();
}
#endif  // BUILDFLAG(ARKWEB_EXT_FREE_COPY)

#if BUILDFLAG(ARKWEB_MULTI_WINDOW)
void FuzzGetCreateNewWindow(FuzzedDataProvider& fdp,
                            TestRenderFrameHost* rfh) {
  std::string url_str = fdp.ConsumeRandomLengthString(kMaxStringLen);
  GURL target_url(url_str);
  int32_t disposition = fdp.ConsumeIntegralInRange<int32_t>(0, 7);
  bool allow_popup = fdp.ConsumeBool();
  auto features = blink::mojom::WindowFeatures::New();
  rfh->GetCreateNewWindow(
      target_url, static_cast<WindowOpenDisposition>(disposition),
      allow_popup, std::move(features), base::DoNothing());
}
#endif  // BUILDFLAG(ARKWEB_MULTI_WINDOW)

#if BUILDFLAG(ARKWEB_PRECOMPILE)
void FuzzGenerateCodeCache(FuzzedDataProvider& fdp,
                           TestRenderFrameHost* rfh) {
  std::string url = fdp.ConsumeRandomLengthString(kMaxStringLen);
  std::string script = fdp.ConsumeRandomLengthString(kMaxStringLen);
  uint8_t header_count = fdp.ConsumeIntegralInRange<uint8_t>(0, 8);
  std::map<std::string, std::string> headers;
  for (uint8_t i = 0; i < header_count && fdp.remaining_bytes() > 0; ++i) {
    std::string key = fdp.ConsumeRandomLengthString(64);
    std::string value = fdp.ConsumeRandomLengthString(64);
    headers[key] = value;
  }
  auto cache_options =
      std::make_shared<oh_code_cache::CacheOptions>(headers);
  rfh->GenerateCodeCache(url, script, cache_options, base::DoNothing());
}
#endif  // BUILDFLAG(ARKWEB_PRECOMPILE)

#if BUILDFLAG(ARKWEB_MENU) || BUILDFLAG(IS_ARKWEB_EXT)
void FuzzGetImageFromCache(FuzzedDataProvider& fdp,
                           TestRenderFrameHost* rfh) {
  std::string url = fdp.ConsumeRandomLengthString(kMaxStringLen);
  rfh->GetImageFromCache(url, base::DoNothing());
}
#endif  // BUILDFLAG(ARKWEB_MENU) || BUILDFLAG(IS_ARKWEB_EXT)

#if BUILDFLAG(ARKWEB_DRAG_DROP)
void FuzzOnClearContextMenu(TestRenderFrameHost* rfh) {
  rfh->OnClearContextMenu();
}
#endif  // BUILDFLAG(ARKWEB_DRAG_DROP)

#if BUILDFLAG(ARKWEB_ADBLOCK)
void FuzzUpdateAdBlockEnabledToRender(FuzzedDataProvider& fdp,
                                     TestRenderFrameHost* rfh) {
  bool enabled = fdp.ConsumeBool();
  rfh->UpdateAdBlockEnabledToRender(enabled);
}
#endif  // BUILDFLAG(ARKWEB_ADBLOCK)

#if BUILDFLAG(ARKWEB_MENU)
void FuzzMenuFunctions(FuzzedDataProvider& fdp, TestRenderFrameHost* rfh) {
  bool show = fdp.ConsumeBool();
  rfh->MouseSelectMenuShow(show);
  rfh->ChangeVisibilityOfQuickMenu();
  rfh->HideQuickMenu();
}
#endif  // BUILDFLAG(ARKWEB_MENU)

#if BUILDFLAG(ARKWEB_AI)
void FuzzCloseImageOverlaySelection(TestRenderFrameHost* rfh) {
  rfh->CloseImageOverlaySelection();
}
#endif  // BUILDFLAG(ARKWEB_AI)

#if BUILDFLAG(ARKWEB_DISATCH_BEFORE_UNLOAD)
void FuzzIsJsDialogShowOrBeforeUnloadTimedOut(TestRenderFrameHost* rfh) {
  rfh->IsJsDialogShowOrBeforeUnloadTimedOut();
}
#endif  // BUILDFLAG(ARKWEB_DISATCH_BEFORE_UNLOAD)

#if BUILDFLAG(ARKWEB_PDF)
void FuzzPdfFunctions(FuzzedDataProvider& fdp, TestRenderFrameHost* rfh) {
  std::string url = fdp.ConsumeRandomLengthString(kMaxStringLen);
  int32_t result = fdp.ConsumeIntegral<int32_t>();
  bool is_pdf = fdp.ConsumeBool();
  RenderFrameHostImplForIncludeTest::OnPdfScrollAtBottom(rfh, url);
  RenderFrameHostImplForIncludeTest::OnPdfLoadEvent(rfh, result, url);
  rfh->SetIsPDF(is_pdf);
}
#endif  // BUILDFLAG(ARKWEB_PDF)

#if BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)
void FuzzBlankScreenFunctions(FuzzedDataProvider& fdp,
                              TestRenderFrameHost* rfh) {
  bool enable = fdp.ConsumeBool();
  std::vector<double> timing;
  uint8_t timing_count =
      fdp.ConsumeIntegralInRange<uint8_t>(0, kMaxVectorLen);
  for (uint8_t i = 0; i < timing_count && fdp.remaining_bytes() > 0; ++i) {
    timing.push_back(fdp.ConsumeFloatingPoint<double>());
  }
  std::vector<int32_t> methods;
  uint8_t method_count =
      fdp.ConsumeIntegralInRange<uint8_t>(0, kMaxVectorLen);
  for (uint8_t i = 0; i < method_count && fdp.remaining_bytes() > 0; ++i) {
    methods.push_back(fdp.ConsumeIntegral<int32_t>());
  }
  int32_t threshold = fdp.ConsumeIntegral<int32_t>();
  rfh->SetBlankScreenDetectionConfig(enable, timing, methods, threshold);
  std::string url = fdp.ConsumeRandomLengthString(kMaxStringLen);
  rfh->DetectBlankScreen(url);
  int32_t reason = fdp.ConsumeIntegral<int32_t>();
  int32_t count = fdp.ConsumeIntegral<int32_t>();
  rfh->OnDetectedBlankScreen(url, reason, count);
}
#endif  // BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)

#if BUILDFLAG(ARKWEB_FIRST_SCREEN_PAINT)
void FuzzOnFirstScreenPaint(FuzzedDataProvider& fdp,
                            TestRenderFrameHost* rfh) {
  std::string url = fdp.ConsumeRandomLengthString(kMaxStringLen);
  int64_t nav_start = fdp.ConsumeIntegral<int64_t>();
  int64_t paint_time = fdp.ConsumeIntegral<int64_t>();
  rfh->OnFirstScreenPaint(url, nav_start, paint_time);
}
#endif  // BUILDFLAG(ARKWEB_FIRST_SCREEN_PAINT)

#if BUILDFLAG(ARKWEB_READER_MODE)
void FuzzGetNetErrorCode(TestRenderFrameHost* rfh) {
  RenderFrameHostImplForIncludeTest::GetNetErrorCode(rfh);
}
#endif  // BUILDFLAG(ARKWEB_READER_MODE)

#if BUILDFLAG(ARKWEB_CONSOLE_LOGGING)
void FuzzDidAddMessageToConsoleV2(FuzzedDataProvider& fdp,
                                  TestRenderFrameHost* rfh) {
  int32_t log_level_int = fdp.ConsumeIntegralInRange<int32_t>(0, 2);
  int32_t log_source_int = fdp.ConsumeIntegralInRange<int32_t>(0, 3);
  auto log_level =
      static_cast<blink::mojom::ConsoleMessageLevel>(log_level_int);
  auto log_source =
      static_cast<blink::mojom::ConsoleMessageSource>(log_source_int);
  std::string msg_bytes = fdp.ConsumeRandomLengthString(kMaxStringLen);
  std::u16string message(msg_bytes.begin(), msg_bytes.end());
  uint32_t line_no = fdp.ConsumeIntegral<uint32_t>();
  std::optional<std::u16string> source_id;
  if (fdp.ConsumeBool()) {
    std::string sid_bytes = fdp.ConsumeRandomLengthString(64);
    source_id = std::u16string(sid_bytes.begin(), sid_bytes.end());
  }
  std::optional<std::u16string> stack_trace;
  if (fdp.ConsumeBool()) {
    std::string st_bytes = fdp.ConsumeRandomLengthString(64);
    stack_trace = std::u16string(st_bytes.begin(), st_bytes.end());
  }
  rfh->DidAddMessageToConsoleV2(log_level, log_source, message, line_no,
                                 source_id, stack_trace);
}
#endif  // BUILDFLAG(ARKWEB_CONSOLE_LOGGING)

#if BUILDFLAG(ARKWEB_JS_ON_DOCUMENT_END)
void FuzzOnDocumentEndReady(TestRenderFrameHost* rfh) {
  rfh->OnDocumentEndReady();
}
#endif  // BUILDFLAG(ARKWEB_JS_ON_DOCUMENT_END)

#if BUILDFLAG(ARKWEB_GET_SCROLL_OFFSET)
void FuzzOnOverScrollOffsetChanged(FuzzedDataProvider& fdp,
                                   TestRenderFrameHost* rfh) {
  float offset_x = fdp.ConsumeFloatingPoint<float>();
  float offset_y = fdp.ConsumeFloatingPoint<float>();
  rfh->OnOverScrollOffsetChanged(offset_x, offset_y);
}
#endif  // BUILDFLAG(ARKWEB_GET_SCROLL_OFFSET)

constexpr int kNumCases = 21;

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static base::AtExitManager* at_exit = new base::AtExitManager();
  static FuzzerEnv* env = new FuzzerEnv();

  if (!data || size == 0) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);
  uint8_t selector = fdp.ConsumeIntegral<uint8_t>();

  TestRenderFrameHost* rfh = env->rfh();
  if (!rfh) {
    return 0;
  }

  switch (selector % kNumCases) {
    case 0:
      FuzzGetWorldId(fdp, rfh);
      break;
    case 1:
      FuzzCommitNavigationExt(fdp, env->browser_context());
      break;
    case 2:
      FuzzGetAllImage(fdp, rfh);
      break;
    case 3:
      FuzzGetImageByXPath(fdp, rfh);
      break;
    case 4:
      FuzzExecuteJavaScriptInFrames(fdp, rfh);
      break;
    case 5: {
#if BUILDFLAG(IS_ARKWEB)
      FuzzExecuteJavaScriptExt(fdp, rfh);
#endif  // BUILDFLAG(IS_ARKWEB)
      break;
    }
    case 6: {
#if BUILDFLAG(ARKWEB_ACCESSIBILITY)
      FuzzSendAccessibilityEvent(fdp, rfh);
#endif  // BUILDFLAG(ARKWEB_ACCESSIBILITY)
      break;
    }
    case 7: {
#if BUILDFLAG(ARKWEB_EXT_FREE_COPY)
      FuzzNotifyContextMenuWillShow(rfh);
#endif  // BUILDFLAG(ARKWEB_EXT_FREE_COPY)
      break;
    }
    case 8: {
#if BUILDFLAG(ARKWEB_MULTI_WINDOW)
      FuzzGetCreateNewWindow(fdp, rfh);
#endif  // BUILDFLAG(ARKWEB_MULTI_WINDOW)
      break;
    }
    case 9: {
#if BUILDFLAG(ARKWEB_PRECOMPILE)
      FuzzGenerateCodeCache(fdp, rfh);
#endif  // BUILDFLAG(ARKWEB_PRECOMPILE)
      break;
    }
    case 10: {
#if BUILDFLAG(ARKWEB_MENU) || BUILDFLAG(IS_ARKWEB_EXT)
      FuzzGetImageFromCache(fdp, rfh);
#endif  // BUILDFLAG(ARKWEB_MENU) || BUILDFLAG(IS_ARKWEB_EXT)
      break;
    }
    case 11: {
#if BUILDFLAG(ARKWEB_DRAG_DROP)
      FuzzOnClearContextMenu(rfh);
#endif  // BUILDFLAG(ARKWEB_DRAG_DROP)
      break;
    }
    case 12: {
#if BUILDFLAG(ARKWEB_ADBLOCK)
      FuzzUpdateAdBlockEnabledToRender(fdp, rfh);
#endif  // BUILDFLAG(ARKWEB_ADBLOCK)
      break;
    }
    case 13: {
#if BUILDFLAG(ARKWEB_MENU)
      FuzzMenuFunctions(fdp, rfh);
#endif  // BUILDFLAG(ARKWEB_MENU)
      break;
    }
    case 14: {
#if BUILDFLAG(ARKWEB_AI)
      FuzzCloseImageOverlaySelection(rfh);
#endif  // BUILDFLAG(ARKWEB_AI)
      break;
    }
    case 15: {
#if BUILDFLAG(ARKWEB_DISATCH_BEFORE_UNLOAD)
      FuzzIsJsDialogShowOrBeforeUnloadTimedOut(rfh);
#endif  // BUILDFLAG(ARKWEB_DISATCH_BEFORE_UNLOAD)
      break;
    }
    case 16: {
#if BUILDFLAG(ARKWEB_PDF)
      FuzzPdfFunctions(fdp, rfh);
#endif  // BUILDFLAG(ARKWEB_PDF)
      break;
    }
    case 17: {
#if BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)
      FuzzBlankScreenFunctions(fdp, rfh);
#endif  // BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)
      break;
    }
    case 18: {
#if BUILDFLAG(ARKWEB_FIRST_SCREEN_PAINT)
      FuzzOnFirstScreenPaint(fdp, rfh);
#endif  // BUILDFLAG(ARKWEB_FIRST_SCREEN_PAINT)
      break;
    }
    case 19: {
#if BUILDFLAG(ARKWEB_READER_MODE)
      FuzzGetNetErrorCode(rfh);
#endif  // BUILDFLAG(ARKWEB_READER_MODE)
      break;
    }
    case 20: {
#if BUILDFLAG(ARKWEB_CONSOLE_LOGGING)
      FuzzDidAddMessageToConsoleV2(fdp, rfh);
#endif  // BUILDFLAG(ARKWEB_CONSOLE_LOGGING)
#if BUILDFLAG(ARKWEB_JS_ON_DOCUMENT_END)
      FuzzOnDocumentEndReady(rfh);
#endif  // BUILDFLAG(ARKWEB_JS_ON_DOCUMENT_END)
#if BUILDFLAG(ARKWEB_GET_SCROLL_OFFSET)
      FuzzOnOverScrollOffsetChanged(fdp, rfh);
#endif  // BUILDFLAG(ARKWEB_GET_SCROLL_OFFSET)
      break;
    }
  }

  return 0;
}

}  // namespace content
