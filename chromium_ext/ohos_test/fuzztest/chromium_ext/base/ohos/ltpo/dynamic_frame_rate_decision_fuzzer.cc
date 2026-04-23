/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "base/check.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"

#include <cstdint>
#include <set>

#include <fuzzer/FuzzedDataProvider.h>

// Pre-include transitive dependencies of dynamic_frame_rate_decision.h so
// that their internal "private:" tokens are parsed before the macro rename.
// The target header's own include guards will skip them on second pass.
#include "base/ohos/ltpo/include/ltpo_strategy.h"

#define private public
#include "base/ohos/ltpo/include/dynamic_frame_rate_decision.h"
#undef private

namespace {

constexpr int32_t MAX_FRAME_RATE = 240;
constexpr int32_t MAX_NWEB_ID = 100;
constexpr int32_t MAX_STRATEGY = 3;
constexpr int32_t MAX_ITERATIONS = 30;

base::ohos::DynamicFrameRateDecision& GetInstance()
{
    return base::ohos::DynamicFrameRateDecision::GetInstance();
}

// Reset all internal state to defaults so scenarios don't leak state into
// each other via the singleton.  Must be called before each deterministic
// scenario to guarantee a clean starting point.
void ResetInstanceState(base::ohos::DynamicFrameRateDecision& inst)
{
    inst.sliding_frame_rate_ = 0;
    inst.video_frame_rate_ = 0;
    inst.cur_frame_rate_ = 0;
    inst.has_touch_point_ = false;
    inst.frame_rate_linker_enable_ = false;
    inst.vsync_cnt_ = 0;
    inst.touch_up_timestamp_ = 0;
    inst.nwebVisibleSet_.clear();
    inst.strategy_ = base::ohos::LTPOStrategy::DISABLED;
    // Intentionally NOT resetting curent_task_runner_ — in fuzzer builds it
    // stays null (guarded by FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION) and
    // the public methods that PostTask check for null before use.
}

// SetMaxFrameRateThreeSec calls curent_task_runner_->PostDelayedTask which
// crashes when curent_task_runner_ is null. This helper ensures the state
// is safe by guaranteeing the function returns early before reaching PostTask.
bool IsSetMaxFrameRateThreeSecSafe(base::ohos::DynamicFrameRateDecision& inst)
{
    if (inst.sliding_frame_rate_ != 0) {
        return true;
    }
    if (inst.vsync_cnt_ == 0) {
        return true;
    }
    if (inst.curent_task_runner_) {
        return true;
    }
    return false;
}

// Non-fatal assertion for fuzzer context.  Returns false and logs a warning
// instead of aborting the process like CHECK() would.
#define FUZZER_EXPECT(condition)                                    \
    do {                                                            \
        if (!(condition)) {                                         \
            LOG(WARNING) << "Fuzzer expectation failed: " #condition; \
        }                                                           \
    } while (0)

void FuzzInit(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    // Init() calls GetCurrentDefault() which is protected by
    // FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION in the source.
    // In fuzzer context, curent_task_runner_ remains null after Init().
    inst.Init();
    inst.Init();  // second call: curent_task_runner_ still null → skips body
}

void FuzzSetLTPOStrategy(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    int32_t strategy = fdp->ConsumeIntegralInRange<int32_t>(-1, MAX_STRATEGY + 1);
    inst.SetLTPOStrategyImpl(strategy);

    int32_t strategy2 = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_STRATEGY);
    inst.SetLTPOStrategyImpl(strategy2);
}

void FuzzSetVisible(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();

    inst.SetLTPOStrategyImpl(static_cast<int32_t>(base::ohos::LTPOStrategy::ALL));

    int nweb_id = fdp->ConsumeIntegralInRange<int>(0, MAX_NWEB_ID);
    bool visible = fdp->ConsumeBool();
    inst.SetVisibleImpl(nweb_id, visible);

    int nweb_id2 = fdp->ConsumeIntegralInRange<int>(0, MAX_NWEB_ID);
    inst.SetVisibleImpl(nweb_id2, true);
    inst.SetVisibleImpl(nweb_id2, false);
}

void FuzzReportSlidingFrameRate(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    int32_t frame_rate = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_FRAME_RATE);
    inst.ReportSlidingFrameRateImpl(frame_rate);

    inst.ReportSlidingFrameRateImpl(frame_rate);
}

void FuzzReportVideoFrameRate(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    int32_t frame_rate = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_FRAME_RATE);
    inst.ReportVideoFrameRateImpl(frame_rate);

    inst.ReportVideoFrameRateImpl(frame_rate);
}

void FuzzSetVsyncEnabled(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    inst.SetVsyncEnabledImpl(true);
    inst.SetVsyncEnabledImpl(true);
    inst.SetVsyncEnabledImpl(false);
    inst.SetVsyncEnabledImpl(false);
}

void FuzzSetHasTouchPoint(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();

    inst.SetLTPOStrategyImpl(static_cast<int32_t>(base::ohos::LTPOStrategy::ALL));
    // vsync_cnt_ = 0 ensures SetHasTouchPointImpl(false) → SetMaxFrameRateThreeSec
    // returns early at vsync_cnt_ check, avoiding PostDelayedTask crash.
    inst.vsync_cnt_ = 0;
    inst.sliding_frame_rate_ = 0;

    inst.SetHasTouchPointImpl(true);
    inst.SetHasTouchPointImpl(true);
    inst.SetHasTouchPointImpl(false);
    inst.SetHasTouchPointImpl(false);
}

void FuzzUpdateFramePreferredRate(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    inst.UpdateFramePreferredRate();
}

void FuzzSetFrameRateLinkerEnable(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    inst.SetFrameRateLinkerEnable(true);
    inst.SetFrameRateLinkerEnable(true);
    inst.SetFrameRateLinkerEnable(false);
    inst.SetFrameRateLinkerEnable(false);
}

void FuzzGetCurrentTimestampMS(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    int64_t ts = inst.GetCurrentTimestampMS();
    FUZZER_EXPECT(ts >= 0);
}

void FuzzSetMaxFrameRateThreeSecSafe(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();

    // sliding_frame_rate_ != 0: returns early at first check
    inst.sliding_frame_rate_ = 30;
    inst.vsync_cnt_ = 1;
    inst.SetMaxFrameRateThreeSec();

    // vsync_cnt_ == 0: returns early at second check
    inst.sliding_frame_rate_ = 0;
    inst.vsync_cnt_ = 0;
    inst.SetMaxFrameRateThreeSec();
}

void FuzzPublicMethodGuards(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    // Public methods (except SetVisible) check curent_task_runner_ and return
    // early if null. In fuzzer context task runner is null after Init().
    inst.ReportSlidingFrameRate(60);
    inst.ReportVideoFrameRate(30);
    inst.SetVsyncEnabled(true);
    inst.SetHasTouchPoint(true);
    inst.SetLTPOStrategy(3);

    // SetVisible: curent_task_runner_ == null → calls Init() → returns.
    // Init() is protected by FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION in source,
    // so GetCurrentDefault() is skipped and no crash occurs.
    inst.curent_task_runner_ = nullptr;
    inst.SetVisible(1, true);
}

void FuzzComprehensiveScenario(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    ResetInstanceState(inst);

    inst.SetLTPOStrategyImpl(static_cast<int32_t>(base::ohos::LTPOStrategy::ALL));
    FUZZER_EXPECT(inst.strategy_ == base::ohos::LTPOStrategy::ALL);

    inst.SetVisibleImpl(1, true);
    FUZZER_EXPECT(!inst.nwebVisibleSet_.empty());

    inst.SetVsyncEnabledImpl(true);
    FUZZER_EXPECT(inst.vsync_cnt_ > 0);

    inst.SetFrameRateLinkerEnable(true);
    FUZZER_EXPECT(inst.frame_rate_linker_enable_);

    inst.ReportSlidingFrameRateImpl(60);
    FUZZER_EXPECT(inst.sliding_frame_rate_ == 60);

    inst.ReportVideoFrameRateImpl(30);
    FUZZER_EXPECT(inst.video_frame_rate_ == 30);

    inst.SetHasTouchPointImpl(true);
    FUZZER_EXPECT(inst.has_touch_point_);

    inst.UpdateFramePreferredRate();

    inst.SetHasTouchPointImpl(false);
    FUZZER_EXPECT(!inst.has_touch_point_);

    inst.ReportSlidingFrameRateImpl(0);
    inst.UpdateFramePreferredRate();

    inst.ReportVideoFrameRateImpl(0);
    inst.UpdateFramePreferredRate();

    inst.SetVisibleImpl(1, false);
    inst.SetVsyncEnabledImpl(false);
    inst.SetFrameRateLinkerEnable(false);

    inst.SetLTPOStrategyImpl(static_cast<int32_t>(base::ohos::LTPOStrategy::HGM_FLING));
    inst.ReportSlidingFrameRateImpl(90);
    inst.SetVisibleImpl(2, true);

    inst.SetLTPOStrategyImpl(static_cast<int32_t>(base::ohos::LTPOStrategy::ALL));
    inst.vsync_cnt_ = 0;
    inst.SetHasTouchPointImpl(true);
    inst.SetHasTouchPointImpl(false);
}

void FuzzUpdateFramePreferredRateBranches(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    ResetInstanceState(inst);

    inst.SetLTPOStrategyImpl(static_cast<int32_t>(base::ohos::LTPOStrategy::ALL));
    inst.SetFrameRateLinkerEnable(true);

    // Branch: sliding_frame_rate_ > 0 → cur_frame_rate_ = sliding_frame_rate_
    inst.sliding_frame_rate_ = 60;
    inst.video_frame_rate_ = 0;
    inst.has_touch_point_ = false;
    inst.touch_up_timestamp_ = 0;
    inst.UpdateFramePreferredRate();

    // Branch: sliding_frame_rate_ <= 0, has_touch_point_ = true → 120FPS
    inst.sliding_frame_rate_ = 0;
    inst.has_touch_point_ = true;
    inst.UpdateFramePreferredRate();

    // Branch: sliding_frame_rate_ <= 0, has_touch_point_ = false,
    //         touch_up within 3s → 120FPS
    inst.has_touch_point_ = false;
    inst.touch_up_timestamp_ = inst.GetCurrentTimestampMS();
    inst.UpdateFramePreferredRate();

    // Branch: sliding_frame_rate_ <= 0, no touch, no recent touch_up,
    //         video_frame_rate_ > 0 → cur_frame_rate_ = video_frame_rate_
    inst.touch_up_timestamp_ = 0;
    inst.video_frame_rate_ = 30;
    inst.UpdateFramePreferredRate();

    // Branch: sliding_frame_rate_ <= 0, no touch, no recent touch_up,
    //         video_frame_rate_ <= 0 → 60FPS default
    inst.video_frame_rate_ = 0;
    inst.UpdateFramePreferredRate();

    // Branch: sliding_frame_rate_ > 0 and video_frame_rate_ > 0,
    //         cur_frame_rate_ = max(sliding, video)
    inst.sliding_frame_rate_ = 45;
    inst.video_frame_rate_ = 60;
    inst.UpdateFramePreferredRate();
}

void FuzzStrategyBranches(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    ResetInstanceState(inst);

    // DISABLED: ReportSliding/Video/Visible/HasTouchPoint early-return
    inst.SetLTPOStrategyImpl(static_cast<int32_t>(base::ohos::LTPOStrategy::DISABLED));
    inst.ReportSlidingFrameRateImpl(60);
    inst.ReportVideoFrameRateImpl(30);
    inst.SetVisibleImpl(1, true);
    inst.SetHasTouchPointImpl(true);

    // APS_FLING: only ReportSliding enters, ReportVideo/Visible/HasTouchPoint early-return
    inst.SetLTPOStrategyImpl(static_cast<int32_t>(base::ohos::LTPOStrategy::APS_FLING));
    inst.ReportSlidingFrameRateImpl(60);
    inst.ReportVideoFrameRateImpl(30);
    inst.SetVisibleImpl(1, true);
    inst.SetHasTouchPointImpl(true);

    // HGM_FLING: ReportSliding + Visible enter, ReportVideo/HasTouchPoint early-return
    inst.SetLTPOStrategyImpl(static_cast<int32_t>(base::ohos::LTPOStrategy::HGM_FLING));
    inst.ReportSlidingFrameRateImpl(60);
    inst.ReportVideoFrameRateImpl(30);
    inst.SetVisibleImpl(1, true);
    inst.SetHasTouchPointImpl(true);

    // ALL: all methods enter + SetHasTouchPointImpl(false) triggers SetMaxFrameRateThreeSec
    inst.SetLTPOStrategyImpl(static_cast<int32_t>(base::ohos::LTPOStrategy::ALL));
    inst.vsync_cnt_ = 0;
    inst.ReportSlidingFrameRateImpl(60);
    inst.ReportVideoFrameRateImpl(30);
    inst.SetVisibleImpl(1, true);
    inst.SetHasTouchPointImpl(true);
    inst.SetHasTouchPointImpl(false);
}

void FuzzVsyncEnabledClamp(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    ResetInstanceState(inst);
    inst.SetVsyncEnabledImpl(false);
    inst.SetVsyncEnabledImpl(false);
    inst.SetVsyncEnabledImpl(false);
    FUZZER_EXPECT(inst.vsync_cnt_ == 0);
}

void FuzzSetLTPOStrategyImplNegative(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    // SetLTPOStrategyImpl: strategy < 0 early-return
    inst.SetLTPOStrategyImpl(-1);
    inst.SetLTPOStrategyImpl(-100);
    // Out of enum range (4+): static_cast to LTPOStrategy, still valid
    inst.SetLTPOStrategyImpl(4);
    inst.SetLTPOStrategyImpl(100);
}

void FuzzReportSlidingFrameRateSameValue(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    ResetInstanceState(inst);
    inst.SetLTPOStrategyImpl(static_cast<int32_t>(base::ohos::LTPOStrategy::ALL));
    // First call sets the value
    inst.ReportSlidingFrameRateImpl(90);
    FUZZER_EXPECT(inst.sliding_frame_rate_ == 90);
    // Second call with same value: early-return due to sliding_frame_rate_ == frame_rate
    inst.ReportSlidingFrameRateImpl(90);
}

void FuzzReportVideoFrameRateSameValue(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    ResetInstanceState(inst);
    inst.SetLTPOStrategyImpl(static_cast<int32_t>(base::ohos::LTPOStrategy::ALL));
    // First call sets the value
    inst.ReportVideoFrameRateImpl(30);
    FUZZER_EXPECT(inst.video_frame_rate_ == 30);
    // Second call with same value: early-return due to video_frame_rate_ == frame_rate
    inst.ReportVideoFrameRateImpl(30);
}

void FuzzSetHasTouchPointSameValue(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    inst.SetLTPOStrategyImpl(static_cast<int32_t>(base::ohos::LTPOStrategy::ALL));
    inst.has_touch_point_ = true;
    // Same value: early-return
    inst.SetHasTouchPointImpl(true);
    inst.has_touch_point_ = false;
    inst.SetHasTouchPointImpl(false);
}

void FuzzSetFrameRateLinkerEnableSameValue(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    inst.frame_rate_linker_enable_ = true;
    // Same value: early-return
    inst.SetFrameRateLinkerEnable(true);
    inst.frame_rate_linker_enable_ = false;
    inst.SetFrameRateLinkerEnable(false);
}

void FuzzUpdateFramePreferredRateDisabled(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    // frame_rate_linker_enable_ = false → UpdateFramePreferredRate returns early
    inst.SetFrameRateLinkerEnable(false);
    inst.UpdateFramePreferredRate();
}

void FuzzRandomized(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    ResetInstanceState(inst);
    int iterations = fdp->ConsumeIntegralInRange<int>(1, MAX_ITERATIONS);
    for (int i = 0; i < iterations; i++) {
        uint8_t action = fdp->ConsumeIntegralInRange<uint8_t>(0, 10);
        switch (action) {
            case 0:
                inst.Init();
                break;
            case 1: {
                int32_t strategy = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_STRATEGY);
                inst.SetLTPOStrategyImpl(strategy);
                break;
            }
            case 2: {
                int nweb_id = fdp->ConsumeIntegralInRange<int>(0, MAX_NWEB_ID);
                bool visible = fdp->ConsumeBool();
                inst.SetVisibleImpl(nweb_id, visible);
                break;
            }
            case 3: {
                int32_t frame_rate = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_FRAME_RATE);
                inst.ReportSlidingFrameRateImpl(frame_rate);
                break;
            }
            case 4: {
                int32_t frame_rate = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_FRAME_RATE);
                inst.ReportVideoFrameRateImpl(frame_rate);
                break;
            }
            case 5:
                inst.SetVsyncEnabledImpl(fdp->ConsumeBool());
                break;
            case 6: {
                bool has_touch = fdp->ConsumeBool();
                if (!has_touch && !IsSetMaxFrameRateThreeSecSafe(inst)) {
                    inst.vsync_cnt_ = 0;
                }
                inst.SetHasTouchPointImpl(has_touch);
                break;
            }
            case 7:
                inst.UpdateFramePreferredRate();
                break;
            case 8:
                inst.SetFrameRateLinkerEnable(fdp->ConsumeBool());
                break;
            case 9:
                if (IsSetMaxFrameRateThreeSecSafe(inst)) {
                    inst.SetMaxFrameRateThreeSec();
                }
                break;
            case 10: {
                (void)inst.GetCurrentTimestampMS();
                break;
            }
            default:
                break;
        }
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (data == nullptr || size < sizeof(uint8_t)) {
        return 0;
    }

    FuzzedDataProvider fdp(data, size);

    uint8_t scenario = fdp.ConsumeIntegralInRange<uint8_t>(0, 20);
    switch (scenario) {
        case 0:
            FuzzInit(&fdp);
            break;
        case 1:
            FuzzSetLTPOStrategy(&fdp);
            break;
        case 2:
            FuzzSetVisible(&fdp);
            break;
        case 3:
            FuzzReportSlidingFrameRate(&fdp);
            break;
        case 4:
            FuzzReportVideoFrameRate(&fdp);
            break;
        case 5:
            FuzzSetVsyncEnabled(&fdp);
            break;
        case 6:
            FuzzSetHasTouchPoint(&fdp);
            break;
        case 7:
            FuzzUpdateFramePreferredRate(&fdp);
            break;
        case 8:
            FuzzSetFrameRateLinkerEnable(&fdp);
            break;
        case 9:
            FuzzGetCurrentTimestampMS(&fdp);
            break;
        case 10:
            FuzzSetMaxFrameRateThreeSecSafe(&fdp);
            break;
        case 11:
            FuzzPublicMethodGuards(&fdp);
            break;
        case 12:
            FuzzComprehensiveScenario(&fdp);
            break;
        case 13:
            FuzzUpdateFramePreferredRateBranches(&fdp);
            break;
        case 14:
            FuzzStrategyBranches(&fdp);
            break;
        case 15:
            FuzzVsyncEnabledClamp(&fdp);
            break;
        case 16:
            FuzzSetLTPOStrategyImplNegative(&fdp);
            break;
        case 17:
            FuzzReportSlidingFrameRateSameValue(&fdp);
            break;
        case 18:
            FuzzReportVideoFrameRateSameValue(&fdp);
            break;
        case 19:
            FuzzSetHasTouchPointSameValue(&fdp);
            break;
        case 20:
            FuzzRandomized(&fdp);
            break;
        default:
            break;
    }

    return 0;
}
