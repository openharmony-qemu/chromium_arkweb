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
#include <memory>
#include <string>
#include <vector>

#include "arkweb/build/features/features.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "content/browser/network_service_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "mojo/core/embedder/embedder.h"

#if BUILDFLAG(ARKWEB_READER_MODE)
#include "arkweb/ohos_nweb_ex/overrides/cef/libcef/browser/alloy/alloy_browser_reader_mode_config_utils.h"
#endif  // BUILDFLAG(ARKWEB_READER_MODE)

#if BUILDFLAG(ARKWEB_EXT_VIDEO_LOAD_OPTIMIZATION)
#include "arkweb/ohos_nweb_ex/overrides/cef/libcef/browser/alloy/alloy_video_load_optimization_config.h"
#endif  // BUILDFLAG(ARKWEB_EXT_VIDEO_LOAD_OPTIMIZATION)

#define private public
#include "arkweb/chromium_ext/content/browser/renderer_host/arkweb_render_process_host_impl_ext.h"
#include "arkweb/chromium_ext/content/browser/renderer_host/arkweb_render_process_host_impl_utils.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#undef private

namespace content {

#if BUILDFLAG(ARKWEB_RENDERER_ANR_DUMP)
extern std::string GetProcessName();
extern void OnUidRetrieved(int32_t pid, int32_t uid);
#endif  // BUILDFLAG(ARKWEB_RENDERER_ANR_DUMP)

namespace {

constexpr size_t kMaxStringLen = 256;

class FuzzerEnv {
 public:
  FuzzerEnv()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        content_client_initializer_() {
    browser_context_ = std::make_unique<TestBrowserContext>();
    storage_partition_ = static_cast<StoragePartitionImpl*>(
        browser_context_->GetDefaultStoragePartition());
  }

  BrowserContext* browser_context() { return browser_context_.get(); }

  StoragePartitionImpl* storage_partition() { return storage_partition_; }

 private:
  // Step 1: CommandLine (matches base::TestSuite constructor)
  struct CommandLineInitializer {
    CommandLineInitializer() { base::CommandLine::Init(0, nullptr); }
  };
  // Step 2: Mojo (matches UnitTestTestSuite constructor)
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
  // Step 3: BrowserTaskEnvironment (provides BrowserThread::UI/IO)
  BrowserTaskEnvironment task_environment_;
  // Step 4: ContentClient (matches TestContentClientInitializer)
  TestContentClientInitializer content_client_initializer_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  StoragePartitionImpl* storage_partition_ = nullptr;
};

std::unique_ptr<ArkwebRenderProcessHostImplExt> CreateHost(
    FuzzerEnv* env) {
  if (!env || !env->browser_context() || !env->storage_partition()) {
    return nullptr;
  }
  return std::make_unique<ArkwebRenderProcessHostImplExt>(
      env->browser_context(), env->storage_partition(), 0, true);
}

void CleanupHost(ArkwebRenderProcessHostImplExt* host) {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
  if (host) {
    host->CancelChannelConnectedCheckTask(host);
  }
#endif  // BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
}

#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
class MockHostForConnectCheck : public ArkwebRenderProcessHostImplExt {
 public:
  MockHostForConnectCheck(BrowserContext* ctx, StoragePartitionImpl* sp)
      : ArkwebRenderProcessHostImplExt(ctx, sp, 0, true) {}

  bool mock_is_ready_ = true;
  bool IsReady() override { return mock_is_ready_; }
};
#endif  // BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)

#if BUILDFLAG(IS_ARKWEB)
void FuzzReportEngineEvent(FuzzerEnv* env, FuzzedDataProvider* fdp) {
  auto host = CreateHost(env);
  if (!host) return;
  std::string module = fdp->ConsumeRandomLengthString(kMaxStringLen);
  std::string resource = fdp->ConsumeRandomLengthString(kMaxStringLen);
  std::string error_code = fdp->ConsumeRandomLengthString(kMaxStringLen);
  std::string error_msg = fdp->ConsumeRandomLengthString(kMaxStringLen);
  host->ReportEngineEvent(module, resource, error_code, error_msg);
  CleanupHost(host.get());
}
#endif  // BUILDFLAG(IS_ARKWEB)

#if BUILDFLAG(ARKWEB_LOGGER_REPORT)
void FuzzReportRendererLog(FuzzerEnv* env, FuzzedDataProvider* fdp) {
  auto host = CreateHost(env);
  if (!host) return;
  int policy = fdp->ConsumeIntegral<int>();
  std::string msg = fdp->ConsumeRandomLengthString(kMaxStringLen);
  host->ReportRendererLog(policy, msg);
  CleanupHost(host.get());
}
#endif  // BUILDFLAG(ARKWEB_LOGGER_REPORT)

#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
void FuzzProcessBackground(FuzzerEnv* env) {
  auto host = CreateHost(env);
  if (!host) return;
  host->IsProcessBackgrounded();
  host->ProcessBackgroundTime();
  CleanupHost(host.get());
}

void FuzzChannelConnectCheck(FuzzerEnv* env, FuzzedDataProvider* fdp) {
  if (!env || !env->browser_context() || !env->storage_partition()) return;
  auto mock_host = std::make_unique<MockHostForConnectCheck>(
      env->browser_context(), env->storage_partition());
  if (!mock_host) return;
  mock_host->RenderProcessChannelConnectCheck();
  mock_host->CancelChannelConnectedCheckTask(mock_host.get());
}

void FuzzStartCancelChannelCheck(FuzzerEnv* env) {
  auto host = CreateHost(env);
  if (!host) return;
  host->StartChannelConnectedCheckTask(host.get());
  host->StartChannelConnectedCheckTask(host.get());
  CleanupHost(host.get());
}

void FuzzCancelNullCallback(FuzzerEnv* env) {
  auto host = CreateHost(env);
  if (!host) return;
  host->CancelChannelConnectedCheckTask(host.get());
  CleanupHost(host.get());
}

void FuzzStartThenCancel(FuzzerEnv* env) {
  auto host = CreateHost(env);
  if (!host) return;
  host->StartChannelConnectedCheckTask(host.get());
  host->CancelChannelConnectedCheckTask(host.get());
  host->CancelChannelConnectedCheckTask(host.get());
  CleanupHost(host.get());
}
#endif  // BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)

#if BUILDFLAG(ARKWEB_THEME_FONT)
void FuzzOnThemeFontChange(FuzzerEnv* env) {
  auto host = CreateHost(env);
  if (!host) return;
  host->OnThemeFontChange();
  CleanupHost(host.get());
}
#endif  // BUILDFLAG(ARKWEB_THEME_FONT)

#if BUILDFLAG(ARKWEB_RENDERER_ANR_DUMP)
void FuzzAnrDumpFunctions(FuzzedDataProvider* fdp) {
  GetProcessName();
  int32_t pid = fdp->ConsumeIntegral<int32_t>();
  int32_t uid = fdp->ConsumeIntegral<int32_t>();
  OnUidRetrieved(pid, uid);
}
#endif  // BUILDFLAG(ARKWEB_RENDERER_ANR_DUMP)

#if BUILDFLAG(ARKWEB_READER_MODE)
void FuzzUpdateReaderModeConfigFull(FuzzerEnv* env, FuzzedDataProvider* fdp) {
  {
    auto host = CreateHost(env);
    if (!host) return;
    host->UpdateReaderModeConfig(nullptr);
    CleanupHost(host.get());
  }

  {
    auto host = CreateHost(env);
    if (!host) return;
    nweb_ex::BrowserReaderModeConfigData config;
    config.enabled = fdp->ConsumeBool();
    host->renderer_interface_.reset();
    host->UpdateReaderModeConfig(&config);
    CleanupHost(host.get());
  }

  {
    auto host = CreateHost(env);
    if (!host) return;
    nweb_ex::BrowserReaderModeConfigData config;
    config.enabled = fdp->ConsumeBool();

    uint8_t detail_count = fdp->ConsumeIntegralInRange<uint8_t>(0, 4);
    for (uint8_t i = 0; i < detail_count && fdp->remaining_bytes() > 0; ++i) {
      nweb_ex::BrowserReaderModeDetailTemplateConfig tpl;
      tpl.templateId = fdp->ConsumeIntegral<int>();
      uint8_t cfg_count = fdp->ConsumeIntegralInRange<uint8_t>(0, 8);
      for (uint8_t j = 0; j < cfg_count && fdp->remaining_bytes() > 0; ++j) {
        tpl.templateConfig.push_back(
            fdp->ConsumeRandomLengthString(kMaxStringLen));
      }
      config.detail_config.templates.push_back(std::move(tpl));
    }

    uint8_t content_count = fdp->ConsumeIntegralInRange<uint8_t>(0, 4);
    for (uint8_t i = 0; i < content_count && fdp->remaining_bytes() > 0;
         ++i) {
      nweb_ex::BrowserReaderModeContentTemplateConfig tpl;
      tpl.template_id = fdp->ConsumeIntegral<int>();
      uint8_t cfg_count = fdp->ConsumeIntegralInRange<uint8_t>(0, 8);
      for (uint8_t j = 0; j < cfg_count && fdp->remaining_bytes() > 0; ++j) {
        tpl.template_config.push_back(
            fdp->ConsumeRandomLengthString(kMaxStringLen));
      }
      config.content_config.template_match.push_back(std::move(tpl));
    }

    uint8_t feature_count = fdp->ConsumeIntegralInRange<uint8_t>(0, 8);
    for (uint8_t i = 0; i < feature_count && fdp->remaining_bytes() > 0; ++i) {
      std::vector<std::string> group;
      uint8_t group_size = fdp->ConsumeIntegralInRange<uint8_t>(0, 4);
      for (uint8_t j = 0; j < group_size && fdp->remaining_bytes() > 0; ++j) {
        group.push_back(fdp->ConsumeRandomLengthString(kMaxStringLen));
      }
      config.content_config.feature_match.push_back(std::move(group));
    }

    config.content_config.meta_data.must_have_catalog = fdp->ConsumeBool();
    config.content_config.meta_data.must_have_prev_and_next =
        fdp->ConsumeBool();
    config.content_config.meta_data.minimum_content_length =
        fdp->ConsumeIntegral<int>();

    host->UpdateReaderModeConfig(&config);
    CleanupHost(host.get());
  }

  ArkwebRenderProcessHostImplExt::UpdateCloudControlReaderModeConfigData(
      nullptr);
  {
    nweb_ex::BrowserReaderModeConfigData config;
    config.enabled = fdp->ConsumeBool();
    ArkwebRenderProcessHostImplExt::UpdateCloudControlReaderModeConfigData(
        &config);
  }
}
#endif  // BUILDFLAG(ARKWEB_READER_MODE)

#if BUILDFLAG(ARKWEB_EXT_VIDEO_LOAD_OPTIMIZATION)
void FuzzVideoLoadOptimization(FuzzerEnv* env, FuzzedDataProvider* fdp) {
  {
    auto host = CreateHost(env);
    if (!host) return;
    nweb_ex::AlloyVideoLoadOptimizationData data;
    data.use_video_load_optimization_ = fdp->ConsumeBool();
    host->renderer_interface_.reset();
    host->UpdateVideoLoadOptimizationConfig(data);
    CleanupHost(host.get());
  }

  {
    auto host = CreateHost(env);
    if (!host) return;
    nweb_ex::AlloyVideoLoadOptimizationData data;
    data.use_video_load_optimization_ = fdp->ConsumeBool();
    data.preload_video_time_ = fdp->ConsumeIntegral<int>();
    data.min_cache_time_ = fdp->ConsumeIntegral<int>();
    data.max_cache_time_ = fdp->ConsumeIntegral<int>();
    data.moov_size_ = fdp->ConsumeIntegral<int>();
    data.bit_rate_ = fdp->ConsumeIntegral<int>();
    uint8_t domain_count = fdp->ConsumeIntegralInRange<uint8_t>(0, 8);
    for (uint8_t i = 0; i < domain_count && fdp->remaining_bytes() > 0; ++i) {
      data.support_domains_.push_back(
          fdp->ConsumeRandomLengthString(kMaxStringLen));
    }
    host->UpdateVideoLoadOptimizationConfig(data);
    CleanupHost(host.get());
  }

  {
    nweb_ex::AlloyVideoLoadOptimizationData data;
    data.use_video_load_optimization_ = fdp->ConsumeBool();
    ArkwebRenderProcessHostImplExt::UpdateVideoLoadOptimizationConfigData(data);
  }
}
#endif  // BUILDFLAG(ARKWEB_EXT_VIDEO_LOAD_OPTIMIZATION)

constexpr int kNumCases = 11;

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static base::AtExitManager* at_exit = new base::AtExitManager();
  static FuzzerEnv* env = new FuzzerEnv();

  if (!data || size == 0) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);
  uint8_t selector = fdp.ConsumeIntegral<uint8_t>();

  switch (selector % kNumCases) {
    case 0: {
#if BUILDFLAG(IS_ARKWEB)
      FuzzReportEngineEvent(env, &fdp);
#endif  // BUILDFLAG(IS_ARKWEB)
      break;
    }
    case 1: {
#if BUILDFLAG(ARKWEB_LOGGER_REPORT)
      FuzzReportRendererLog(env, &fdp);
#endif  // BUILDFLAG(ARKWEB_LOGGER_REPORT)
      break;
    }
    case 2: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      FuzzProcessBackground(env);
#endif  // BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      break;
    }
    case 3: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      FuzzChannelConnectCheck(env, &fdp);
#endif  // BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      break;
    }
    case 4: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      FuzzStartCancelChannelCheck(env);
#endif  // BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      break;
    }
    case 5: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      FuzzStartThenCancel(env);
#endif  // BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      break;
    }
    case 6: {
#if BUILDFLAG(ARKWEB_THEME_FONT)
      FuzzOnThemeFontChange(env);
#endif  // BUILDFLAG(ARKWEB_THEME_FONT)
      break;
    }
    case 7: {
#if BUILDFLAG(ARKWEB_RENDERER_ANR_DUMP)
      FuzzAnrDumpFunctions(&fdp);
#endif  // BUILDFLAG(ARKWEB_RENDERER_ANR_DUMP)
      break;
    }
    case 8: {
#if BUILDFLAG(ARKWEB_READER_MODE)
      FuzzUpdateReaderModeConfigFull(env, &fdp);
#endif  // BUILDFLAG(ARKWEB_READER_MODE)
      break;
    }
    case 9: {
#if BUILDFLAG(ARKWEB_EXT_VIDEO_LOAD_OPTIMIZATION)
      FuzzVideoLoadOptimization(env, &fdp);
#endif  // BUILDFLAG(ARKWEB_EXT_VIDEO_LOAD_OPTIMIZATION)
      break;
    }
    case 10: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      FuzzCancelNullCallback(env);
#endif  // BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      break;
    }
  }

  return 0;
}

}  // namespace content
