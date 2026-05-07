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
#include "base/test/task_environment.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/begin_frame_source_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <climits>
#include <cstdint>
#include <memory>

#define private public
#define protected public
#include "arkweb/chromium_ext/components/viz/service/frame_sinks/external_begin_frame_source_ohos.h"
#undef private
#undef protected

namespace viz {

constexpr uint32_t MAX_RESTART_ID = 100000;
constexpr int64_t VSYNC_PERIOD_60HZ = 16666666;
constexpr int64_t VSYNC_PERIOD_90HZ = 11111111;
constexpr int64_t VSYNC_PERIOD_120HZ = 8333333;
constexpr int64_t VSYNC_PERIOD_90120HZ_MID = 9800000;
constexpr int64_t VSYNC_PERIOD_6090HZ_MID = 13000000;

static std::unique_ptr<base::test::TaskEnvironment> task_environment_ = nullptr;

struct Environment {
    Environment() {
        task_environment_ = std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::IO);
    }
    ~Environment() {
        task_environment_.reset();
    }
};

class ExternalBeginFrameSourceOHOSFuzzer {
 public:
  ExternalBeginFrameSourceOHOSFuzzer() {}

  void FuzzOnVSyncImplCore(FuzzedDataProvider* fdp) {
    auto frame_sink_manager = std::make_unique<viz::FrameSinkManagerImpl>(
      viz::FrameSinkManagerImpl::InitParams());
    auto source = std::make_unique<ExternalBeginFrameSourceOHOS>(
      1, frame_sink_manager.get());

    if (!source) {
      return;
    }

    testing::NiceMock<MockBeginFrameObserver> obs;
    source->AddObserver(&obs);

    source->vsync_notification_enabled_ = true;

    if (!source->user_data_) {
      return;
    }

    int64_t base_timestamp = 1000000000000LL;

    source->first_vsync_since_notify_enabled_ = true;
    int64_t timestamp1 = base_timestamp;
    source->OnVSyncImpl(timestamp1, source->user_data_.get());

    source->first_vsync_since_notify_enabled_ = false;
    int64_t timestamp2 = base_timestamp + VSYNC_PERIOD_60HZ;
    source->OnVSyncImpl(timestamp2, source->user_data_.get());

    source->RemoveObserver(&obs);
  }

  void FuzzVsyncPeriodCalculation120Hz(FuzzedDataProvider* fdp) {
    auto frame_sink_manager = std::make_unique<viz::FrameSinkManagerImpl>(
      viz::FrameSinkManagerImpl::InitParams());
    auto source = std::make_unique<ExternalBeginFrameSourceOHOS>(
      1, frame_sink_manager.get());
    if (!source) {
      return;
    }

    testing::NiceMock<MockBeginFrameObserver> obs;
    source->AddObserver(&obs);
    source->vsync_notification_enabled_ = true;

    int64_t base_timestamp = 1000000000000LL;

    source->first_vsync_since_notify_enabled_ = true;
    source->OnVSyncImpl(base_timestamp, source->user_data_.get());

    source->first_vsync_since_notify_enabled_ = false;
    source->vsync_period_ = 0;

    int64_t timestamp2 = base_timestamp + VSYNC_PERIOD_120HZ;
    source->OnVSyncImpl(timestamp2, source->user_data_.get());

    source->RemoveObserver(&obs);
  }

  void FuzzVsyncPeriodCalculation90Hz(FuzzedDataProvider* fdp) {
    auto frame_sink_manager = std::make_unique<viz::FrameSinkManagerImpl>(
      viz::FrameSinkManagerImpl::InitParams());
    auto source = std::make_unique<ExternalBeginFrameSourceOHOS>(
      1, frame_sink_manager.get());
    if (!source) {
      return;
    }

    testing::NiceMock<MockBeginFrameObserver> obs;
    source->AddObserver(&obs);
    source->vsync_notification_enabled_ = true;

    int64_t base_timestamp = 1000000000000LL;

    source->first_vsync_since_notify_enabled_ = true;
    source->OnVSyncImpl(base_timestamp, source->user_data_.get());

    source->first_vsync_since_notify_enabled_ = false;
    source->vsync_period_ = 0;

    int64_t timestamp2 = base_timestamp + VSYNC_PERIOD_90HZ;
    source->OnVSyncImpl(timestamp2, source->user_data_.get());

    source->RemoveObserver(&obs);
  }

  void FuzzVsyncPeriodCalculation60Hz(FuzzedDataProvider* fdp) {
    auto frame_sink_manager = std::make_unique<viz::FrameSinkManagerImpl>(
      viz::FrameSinkManagerImpl::InitParams());
    auto source = std::make_unique<ExternalBeginFrameSourceOHOS>(
      1, frame_sink_manager.get());
    if (!source) {
      return;
    }

    testing::NiceMock<MockBeginFrameObserver> obs;
    source->AddObserver(&obs);
    source->vsync_notification_enabled_ = true;

    int64_t base_timestamp = 1000000000000LL;

    source->first_vsync_since_notify_enabled_ = true;
    source->OnVSyncImpl(base_timestamp, source->user_data_.get());

    source->first_vsync_since_notify_enabled_ = false;
    source->vsync_period_ = 0;

    int64_t timestamp2 = base_timestamp + VSYNC_PERIOD_60HZ;
    source->OnVSyncImpl(timestamp2, source->user_data_.get());

    source->RemoveObserver(&obs);
  }

  void FuzzLowerFrameRateLogic(FuzzedDataProvider* fdp) {
    auto frame_sink_manager = std::make_unique<viz::FrameSinkManagerImpl>(
      viz::FrameSinkManagerImpl::InitParams());
    auto source = std::make_unique<ExternalBeginFrameSourceOHOS>(
      1, frame_sink_manager.get());
    if (!source) {
      return;
    }

    testing::NiceMock<MockBeginFrameObserver> obs;
    source->AddObserver(&obs);
    source->vsync_notification_enabled_ = true;

    int64_t base_timestamp = 1000000000000LL;

    source->lower_frame_rate_enabled_ = true;
    source->OnVSyncImpl(base_timestamp, source->user_data_.get());

    source->OnVSyncImpl(base_timestamp + VSYNC_PERIOD_60HZ, source->user_data_.get());

    source->lower_frame_rate_enabled_ = false;
    source->OnVSyncImpl(base_timestamp + 2 * VSYNC_PERIOD_60HZ, source->user_data_.get());

    source->RemoveObserver(&obs);
  }

  void FuzzHalfFrameRateLogic(FuzzedDataProvider* fdp) {
    auto frame_sink_manager = std::make_unique<viz::FrameSinkManagerImpl>(
      viz::FrameSinkManagerImpl::InitParams());
    auto source = std::make_unique<ExternalBeginFrameSourceOHOS>(
      1, frame_sink_manager.get());
    if (!source) {
      return;
    }

    testing::NiceMock<MockBeginFrameObserver> obs;
    source->AddObserver(&obs);
    source->vsync_notification_enabled_ = true;

    int64_t base_timestamp = 1000000000000LL;

    source->half_frame_rate_enabled_ = true;
    source->OnVSyncImpl(base_timestamp, source->user_data_.get());

    source->OnVSyncImpl(base_timestamp + VSYNC_PERIOD_60HZ, source->user_data_.get());

    source->half_frame_rate_enabled_ = false;
    source->OnVSyncImpl(base_timestamp + 2 * VSYNC_PERIOD_60HZ, source->user_data_.get());

    source->RemoveObserver(&obs);
  }

  void FuzzSkipVsyncLogic(FuzzedDataProvider* fdp) {
    auto frame_sink_manager = std::make_unique<viz::FrameSinkManagerImpl>(
      viz::FrameSinkManagerImpl::InitParams());
    auto source = std::make_unique<ExternalBeginFrameSourceOHOS>(
      1, frame_sink_manager.get());
    if (!source) {
      return;
    }

    testing::NiceMock<MockBeginFrameObserver> obs;
    source->AddObserver(&obs);
    source->vsync_notification_enabled_ = true;

    int64_t base_timestamp = 1000000000000LL;

    source->lower_frame_rate_enabled_ = true;
    source->g_skip_vsync_ = true;
    source->OnVSyncImpl(base_timestamp, source->user_data_.get());

    source->g_skip_vsync_ = false;
    source->OnVSyncImpl(base_timestamp + VSYNC_PERIOD_60HZ, source->user_data_.get());

    source->RemoveObserver(&obs);
  }

  void FuzzDrawRect(FuzzedDataProvider* fdp) {
    auto frame_sink_manager = std::make_unique<viz::FrameSinkManagerImpl>(
      viz::FrameSinkManagerImpl::InitParams());
    auto source = std::make_unique<ExternalBeginFrameSourceOHOS>(
      1, frame_sink_manager.get());
    if (!source) {
      return;
    }

    testing::NiceMock<MockBeginFrameObserver> obs;
    source->AddObserver(&obs);
    source->vsync_notification_enabled_ = true;

    int32_t x = fdp->ConsumeIntegralInRange<int32_t>(0, 1000);
    int32_t y = fdp->ConsumeIntegralInRange<int32_t>(0, 1000);
    int32_t width = fdp->ConsumeIntegralInRange<int32_t>(1, 1000);
    int32_t height = fdp->ConsumeIntegralInRange<int32_t>(1, 1000);

    gfx::Rect rect(x, y, width, height);
    source->SetDrawRect(rect);

    int64_t base_timestamp = 1000000000000LL;
    source->OnVSyncImpl(base_timestamp, source->user_data_.get());

    source->RemoveObserver(&obs);
  }

  void FuzzSetEnabledToggle(FuzzedDataProvider* fdp) {
    auto frame_sink_manager = std::make_unique<viz::FrameSinkManagerImpl>(
      viz::FrameSinkManagerImpl::InitParams());
    auto source = std::make_unique<ExternalBeginFrameSourceOHOS>(
      1, frame_sink_manager.get());
    if (!source) {
      return;
    }

    int iterations = fdp->ConsumeIntegralInRange<int>(2, 5);
    for (int i = 0; i < iterations; ++i) {
      bool enabled = fdp->ConsumeBool();
      source->SetEnabled(enabled);

      if (enabled) {
        int64_t timestamp = 1000000000000LL + (i * VSYNC_PERIOD_60HZ);
        if (source->user_data_) {
          source->OnVSyncImpl(timestamp, source->user_data_.get());
        }
      }
    }
  }

  void FuzzMultipleVsyncCalls(FuzzedDataProvider* fdp) {
    auto frame_sink_manager = std::make_unique<viz::FrameSinkManagerImpl>(
      viz::FrameSinkManagerImpl::InitParams());
    auto source = std::make_unique<ExternalBeginFrameSourceOHOS>(
      1, frame_sink_manager.get());
    if (!source) {
      return;
    }

    testing::NiceMock<MockBeginFrameObserver> obs;
    source->AddObserver(&obs);
    source->vsync_notification_enabled_ = true;

    int64_t base_timestamp = 1000000000000LL;
    int count = fdp->ConsumeIntegralInRange<int>(5, 20);

    for (int i = 0; i < count; ++i) {
      int64_t period = fdp->ConsumeIntegralInRange<int64_t>(VSYNC_PERIOD_120HZ, VSYNC_PERIOD_60HZ);
      int64_t timestamp = base_timestamp + (i * period);

      source->first_vsync_since_notify_enabled_ = (i == 0);
      source->vsync_period_ = (i % 3 == 0) ? 0 : period;

      source->OnVSyncImpl(timestamp, source->user_data_.get());
    }

    source->RemoveObserver(&obs);
  }

  void FuzzFrameRateCombinations(FuzzedDataProvider* fdp) {
    auto frame_sink_manager = std::make_unique<viz::FrameSinkManagerImpl>(
      viz::FrameSinkManagerImpl::InitParams());
    auto source = std::make_unique<ExternalBeginFrameSourceOHOS>(
      1, frame_sink_manager.get());
    if (!source) {
      return;
    }

    testing::NiceMock<MockBeginFrameObserver> obs;
    source->AddObserver(&obs);
    source->vsync_notification_enabled_ = true;

    int64_t base_timestamp = 1000000000000LL;

    bool lower_enabled = fdp->ConsumeBool();
    bool half_enabled = fdp->ConsumeBool();

    source->lower_frame_rate_enabled_ = lower_enabled;
    source->half_frame_rate_enabled_ = half_enabled;

    source->OnVSyncImpl(base_timestamp, source->user_data_.get());
    source->OnVSyncImpl(base_timestamp + VSYNC_PERIOD_60HZ, source->user_data_.get());

    source->lower_frame_rate_enabled_ = !lower_enabled;
    source->half_frame_rate_enabled_ = !half_enabled;

    source->OnVSyncImpl(base_timestamp + 2 * VSYNC_PERIOD_60HZ, source->user_data_.get());

    source->RemoveObserver(&obs);
  }

  void FuzzOnVSyncStaticMethod(FuzzedDataProvider* fdp) {
    auto frame_sink_manager = std::make_unique<viz::FrameSinkManagerImpl>(
      viz::FrameSinkManagerImpl::InitParams());
    auto source = std::make_unique<ExternalBeginFrameSourceOHOS>(
      1, frame_sink_manager.get());
    if (!source || !source->user_data_) {
      return;
    }

    int64_t timestamp = fdp->ConsumeIntegralInRange<int64_t>(1000000000000LL, 2000000000000LL);

    ExternalBeginFrameSourceOHOS::OnVSync(timestamp, source->user_data_.get());
  }

  void FuzzOnVSyncNullData(FuzzedDataProvider* fdp) {
    int64_t timestamp = fdp->ConsumeIntegralInRange<int64_t>(1000000000000LL, 2000000000000LL);

    ExternalBeginFrameSourceOHOS::OnVSync(timestamp, nullptr);
  }

  void FuzzMixedOperations(FuzzedDataProvider* fdp) {
    auto frame_sink_manager = std::make_unique<viz::FrameSinkManagerImpl>(
      viz::FrameSinkManagerImpl::InitParams());
    auto source = std::make_unique<ExternalBeginFrameSourceOHOS>(
      1, frame_sink_manager.get());
    if (!source) {
      return;
    }

    testing::NiceMock<MockBeginFrameObserver> obs;
    source->AddObserver(&obs);
    source->vsync_notification_enabled_ = true;

    int64_t base_timestamp = 1000000000000LL;
    int operations = fdp->ConsumeIntegralInRange<int>(5, 15);

    for (int i = 0; i < operations; ++i) {
      int op_type = fdp->ConsumeIntegralInRange<int>(0, 5);

      switch (op_type) {
        case 0:
          source->lower_frame_rate_enabled_ = fdp->ConsumeBool();
          break;
        case 1:
          source->half_frame_rate_enabled_ = fdp->ConsumeBool();
          break;
        case 2:
          source->g_skip_vsync_ = fdp->ConsumeBool();
          break;
        case 3:
          {
            gfx::Rect rect(
              fdp->ConsumeIntegralInRange<int32_t>(0, 100),
              fdp->ConsumeIntegralInRange<int32_t>(0, 100),
              fdp->ConsumeIntegralInRange<int32_t>(1, 100),
              fdp->ConsumeIntegralInRange<int32_t>(1, 100));
            source->SetDrawRect(rect);
          }
          break;
        case 4:
          source->SetEnabled(fdp->ConsumeBool());
          break;
        case 5:
          {
            int64_t timestamp = base_timestamp + (i * VSYNC_PERIOD_60HZ);
            if (source->user_data_) {
              source->OnVSyncImpl(timestamp, source->user_data_.get());
            }
          }
          break;
      }
    }

    source->RemoveObserver(&obs);
  }

  void RunFuzzTest(FuzzedDataProvider* fdp) {
    int32_t selector = fdp->ConsumeIntegralInRange<int32_t>(0, 13);

    switch (selector) {
      case 0:
        FuzzOnVSyncImplCore(fdp);
        break;
      case 1:
        FuzzVsyncPeriodCalculation120Hz(fdp);
        break;
      case 2:
        FuzzVsyncPeriodCalculation90Hz(fdp);
        break;
      case 3:
        FuzzVsyncPeriodCalculation60Hz(fdp);
        break;
      case 4:
        FuzzLowerFrameRateLogic(fdp);
        break;
      case 5:
        FuzzHalfFrameRateLogic(fdp);
        break;
      case 6:
        FuzzSkipVsyncLogic(fdp);
        break;
      case 7:
        FuzzDrawRect(fdp);
        break;
      case 8:
        FuzzSetEnabledToggle(fdp);
        break;
      case 9:
        FuzzMultipleVsyncCalls(fdp);
        break;
      case 10:
        FuzzFrameRateCombinations(fdp);
        break;
      case 11:
        FuzzOnVSyncStaticMethod(fdp);
        break;
      case 12:
        FuzzOnVSyncNullData(fdp);
        break;
      case 13:
        FuzzMixedOperations(fdp);
        break;
      default:
        FuzzOnVSyncImplCore(fdp);
        break;
    }
  }
};

}  // namespace viz

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(int32_t)) {
    return 0;
  }

  base::CommandLine::Init(0, nullptr);
  static viz::Environment env;

  viz::task_environment_->RunUntilIdle();

  static viz::ExternalBeginFrameSourceOHOSFuzzer* fuzzer = nullptr;

  if (fuzzer == nullptr) {
    fuzzer = new viz::ExternalBeginFrameSourceOHOSFuzzer();
  }

  FuzzedDataProvider fdp(data, size);

  fuzzer->RunFuzzTest(&fdp);

  return 0;
}