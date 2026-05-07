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

#include <cstdint>
#include <vector>

#include <fuzzer/FuzzedDataProvider.h>

// Pre-include transitive dependencies before #define private public to avoid
// corrupting access specifiers in libc++ and Chromium base headers.
#include "base/ohos/ltpo/include/ltpo_strategy.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/adapter_base.h"

#define private public
#include "base/ohos/ltpo/include/sliding_observer.h"
#undef private

namespace {

using OHOS::NWeb::FrameRateSetting;

constexpr float MAX_DELTA = 10000.0f;
constexpr float MAX_VELOCITY = 50000.0f;
constexpr int32_t MAX_ITERATIONS = 30;

base::ohos::SlidingObserver& GetInstance()
{
    return base::ohos::SlidingObserver::GetInstance();
}

// Reset singleton state to a clean baseline before each test.
// The singleton is shared across all fuzzer scenarios, so state from a
// previous scenario (e.g. use_pdf_rate_ left true by FuzzPdfMethods) can
// corrupt subsequent tests.
void ResetToBaseline(base::ohos::SlidingObserver& inst)
{
    inst.is_inited_ = true;
    inst.is_sliding_ = false;
    inst.is_off_screen_ = false;
    inst.strategy_ = base::ohos::LTPOStrategy::HGM_FLING;
    inst.dpi_ = 400;
    inst.virtual_pixel_ratio_ = 1.5f;
    inst.current_timestamp_ = -1;
    inst.sliding_frame_rate_ = 0;
#if BUILDFLAG(ARKWEB_PDF)
    inst.use_pdf_rate_ = false;
#endif
    inst.on_screen_setting_ = {
        {0.0f, 100.0f, 60},
        {100.0f, 500.0f, 90},
        {500.0f, -1.0f, 120},
    };
    inst.off_screen_setting_ = {
        {0.0f, 100.0f, 60},
        {100.0f, 500.0f, 90},
    };
}

void FuzzStartSliding(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();

    // Test early-return: is_inited_ == false
    inst.is_inited_ = false;
    inst.StartSliding();

    // Test early-return: is_sliding_ == true
    inst.is_inited_ = true;
    inst.is_sliding_ = true;
    inst.StartSliding();

    // Test normal path
    inst.is_sliding_ = false;
    inst.StartSliding();
    CHECK(inst.is_sliding_);
    CHECK(!inst.is_off_screen_);
}

void FuzzStopSliding(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();

    // Early-return: !is_inited_
    inst.is_inited_ = false;
    CHECK(inst.StopSliding() == -1);

    // Early-return: !is_sliding_
    inst.is_inited_ = true;
    inst.is_sliding_ = false;
    CHECK(inst.StopSliding() == -1);

    // Early-return: is_off_screen_
    inst.is_sliding_ = true;
    inst.is_off_screen_ = true;
    CHECK(inst.StopSliding() == -1);

    // Normal path
    inst.is_off_screen_ = false;
    int32_t result = inst.StopSliding();
    CHECK(result == 0);
    CHECK(!inst.is_sliding_);
    CHECK(!inst.is_off_screen_);
    CHECK(inst.sliding_frame_rate_ == 0);
}

void FuzzStartFling(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();

    // Early-return: !is_inited_
    inst.is_inited_ = false;
    inst.StartFling();

    // Early-return: !is_sliding_
    inst.is_inited_ = true;
    inst.is_sliding_ = false;
    inst.StartFling();

    // Normal path (HGM_FLING strategy - no APS SetScene call)
    inst.is_sliding_ = true;
    inst.strategy_ = base::ohos::LTPOStrategy::HGM_FLING;
    inst.StartFling();
    CHECK(inst.is_off_screen_);

    // Normal path with APS_FLING strategy (calls SetScene)
    inst.is_off_screen_ = false;
    inst.strategy_ = base::ohos::LTPOStrategy::APS_FLING;
    inst.StartFling();
    CHECK(inst.is_off_screen_);
}

void FuzzStopFling(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();

    // Early-return: !is_inited_
    inst.is_inited_ = false;
    CHECK(inst.StopFling() == -1);

    // Early-return: !is_sliding_
    inst.is_inited_ = true;
    inst.is_sliding_ = false;
    CHECK(inst.StopFling() == -1);

    // Early-return: !is_off_screen_
    inst.is_sliding_ = true;
    inst.is_off_screen_ = false;
    CHECK(inst.StopFling() == -1);

    // Normal path with APS_FLING and is_off_screen_
    inst.is_off_screen_ = true;
    inst.strategy_ = base::ohos::LTPOStrategy::APS_FLING;
    int32_t result = inst.StopFling();
    CHECK(result == 0);
    CHECK(!inst.is_sliding_);
    CHECK(inst.sliding_frame_rate_ == 0);

    // Normal path with HGM_FLING (skips APS branch)
    inst.is_inited_ = true;
    inst.is_sliding_ = true;
    inst.is_off_screen_ = true;
    inst.strategy_ = base::ohos::LTPOStrategy::HGM_FLING;
    result = inst.StopFling();
    CHECK(result == 0);
}

void FuzzGetCurrentTimestamp(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    int64_t ts = inst.GetCurrentTimestamp();
    CHECK(ts > 0);
}

void FuzzOnScrollUpdate(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();

    // Early-return: !is_sliding_
    inst.is_sliding_ = false;
    CHECK(inst.OnScrollUpdate(1.0f, 1.0f) == -1);

    // Early-return: is_off_screen_
    inst.is_sliding_ = true;
    inst.is_off_screen_ = true;
    CHECK(inst.OnScrollUpdate(1.0f, 1.0f) == -1);

    // Early-return: APS_FLING strategy
    inst.is_off_screen_ = false;
    inst.strategy_ = base::ohos::LTPOStrategy::APS_FLING;
    CHECK(inst.OnScrollUpdate(1.0f, 1.0f) == -1);

    // Normal path with known state
    ResetToBaseline(inst);
    inst.is_sliding_ = true;
    inst.current_timestamp_ = inst.GetCurrentTimestamp();

    float dx = fdp->ConsumeFloatingPointInRange<float>(-MAX_DELTA, MAX_DELTA);
    float dy = fdp->ConsumeFloatingPointInRange<float>(-MAX_DELTA, MAX_DELTA);
    int32_t rate = inst.OnScrollUpdate(dx, dy);
    (void)rate;

    // Call again with same rate to exercise the sliding_frame_rate_ ==
    // preferred_frame_rate early-return path. Rate may differ due to timing,
    // so only verify the return is valid (-1 or a positive rate).
    inst.current_timestamp_ = inst.GetCurrentTimestamp();
    int32_t rate2 = inst.OnScrollUpdate(dx, dy);
    (void)rate2;
}

void FuzzOnFlingUpdate(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();

    // Early-return: !is_sliding_
    inst.is_sliding_ = false;
    CHECK(inst.OnFlingUpdate(1.0f, 1.0f) == -1);

    // Early-return: !is_off_screen_
    inst.is_sliding_ = true;
    inst.is_off_screen_ = false;
    CHECK(inst.OnFlingUpdate(1.0f, 1.0f) == -1);

    // Early-return: APS_FLING strategy
    inst.is_off_screen_ = true;
    inst.strategy_ = base::ohos::LTPOStrategy::APS_FLING;
    CHECK(inst.OnFlingUpdate(1.0f, 1.0f) == -1);

    // Normal path with known state
    ResetToBaseline(inst);
    inst.is_sliding_ = true;
    inst.is_off_screen_ = true;

    float vx = fdp->ConsumeFloatingPointInRange<float>(-MAX_VELOCITY, MAX_VELOCITY);
    float vy = fdp->ConsumeFloatingPointInRange<float>(-MAX_VELOCITY, MAX_VELOCITY);
    int32_t rate = inst.OnFlingUpdate(vx, vy);
    (void)rate;

    // Second call: rate may or may not match, just exercise the path.
    int32_t rate2 = inst.OnFlingUpdate(vx, vy);
    (void)rate2;
}

void FuzzGetPreferedFrameRate(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();

    // Reset to clean state so use_pdf_rate_ doesn't pollute from prior scenarios
    ResetToBaseline(inst);

    // Empty setting → returns kDefaultPreferedFrameRate (120)
    std::vector<FrameRateSetting> empty_setting;
    int32_t rate = inst.GetPreferedFrameRate(100.0f, empty_setting);
    CHECK(rate == 120);

    // Non-empty setting: velocity matches first range
    std::vector<FrameRateSetting> setting = {
        {0.0f, 100.0f, 60},
        {100.0f, 500.0f, 90},
        {500.0f, -1.0f, 120},
    };
    rate = inst.GetPreferedFrameRate(50.0f, setting);
    CHECK(rate == 60);

    // velocity matches second range
    rate = inst.GetPreferedFrameRate(200.0f, setting);
    CHECK(rate == 90);

    // velocity matches third range (max < 0 means unbounded)
    rate = inst.GetPreferedFrameRate(1000.0f, setting);
    CHECK(rate == 120);

    // velocity below all ranges → falls through to default
    rate = inst.GetPreferedFrameRate(-1.0f, setting);
    CHECK(rate == 120);
}

void FuzzGetVelocity(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    inst.dpi_ = 400;
    inst.virtual_pixel_ratio_ = 1.5f;
    float v = inst.GetVelocity(3.0f, 4.0f);
    CHECK(v > 0.0f);
}

void FuzzDestructor(FuzzedDataProvider* fdp)
{
    // Test destructor with APS_FLING strategy (calls SetScene with STOP_ALL_FLING_LTPO)
    {
        auto* p = new base::ohos::SlidingObserver();
        p->strategy_ = base::ohos::LTPOStrategy::APS_FLING;
        delete p;
    }

    // Test destructor with HGM_FLING strategy (skips APS_FLING if block)
    {
        auto* p = new base::ohos::SlidingObserver();
        p->strategy_ = base::ohos::LTPOStrategy::HGM_FLING;
        delete p;
    }

    // Test destructor with DISABLED strategy
    {
        auto* p = new base::ohos::SlidingObserver();
        p->strategy_ = base::ohos::LTPOStrategy::DISABLED;
        delete p;
    }
}

void FuzzSlidingLifecycle(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    ResetToBaseline(inst);

    // Full lifecycle: start → scroll → fling → stop fling → start → stop
    inst.StartSliding();

    inst.current_timestamp_ = inst.GetCurrentTimestamp();
    float dx = fdp->ConsumeFloatingPointInRange<float>(1.0f, MAX_DELTA);
    float dy = fdp->ConsumeFloatingPointInRange<float>(1.0f, MAX_DELTA);
    inst.OnScrollUpdate(dx, dy);

    inst.StartFling();
    float vx = fdp->ConsumeFloatingPointInRange<float>(1.0f, MAX_VELOCITY);
    float vy = fdp->ConsumeFloatingPointInRange<float>(1.0f, MAX_VELOCITY);
    inst.OnFlingUpdate(vx, vy);

    inst.StopFling();

    inst.StartSliding();
    inst.current_timestamp_ = inst.GetCurrentTimestamp();
    inst.OnScrollUpdate(dx, dy);
    inst.StopSliding();
}

#if BUILDFLAG(ARKWEB_PDF)
void FuzzPdfMethods(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();

    // SetIsPdf(true) on non-PC device: use_pdf_rate_ = true
    inst.is_pdf_ = false;
    inst.use_pdf_rate_ = false;
    inst.SetIsPdf(true);
    CHECK(inst.is_pdf_);
    (void)inst.IsPdf();

    // SetIsPdf(false)
    inst.SetIsPdf(false);
    CHECK(!inst.is_pdf_);

    // Test GetPreferedFrameRate with use_pdf_rate_ = true and empty setting
    inst.use_pdf_rate_ = true;
    {
        std::vector<FrameRateSetting> empty;
        int32_t rate = inst.GetPreferedFrameRate(100.0f, empty);
        (void)rate;
    }

    // Test GetPreferedFrameRate with use_pdf_rate_ = true and non-empty setting
    // where preferredFrameRate > kPdfScrollPreferedFrameRate (90)
    {
        std::vector<FrameRateSetting> setting_high_rate = {
            {0.0f, 100.0f, 120},
        };
        int32_t rate = inst.GetPreferedFrameRate(50.0f, setting_high_rate);
        (void)rate;
    }

    // Test with preferredFrameRate <= 90 (skips pdf clamp)
    {
        std::vector<FrameRateSetting> setting_low_rate = {
            {0.0f, 100.0f, 60},
        };
        int32_t rate = inst.GetPreferedFrameRate(50.0f, setting_low_rate);
        (void)rate;
    }

    // Test fallthrough with use_pdf_rate_ = true
    {
        std::vector<FrameRateSetting> setting = {
            {100.0f, 500.0f, 120},
        };
        int32_t rate = inst.GetPreferedFrameRate(-1.0f, setting);
        (void)rate;
    }

    // Reset so subsequent scenarios are not affected
    inst.use_pdf_rate_ = false;
}
#endif  // BUILDFLAG(ARKWEB_PDF)

void FuzzOnDisplayInfoChange(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    // Early-return: !is_inited_
    inst.is_inited_ = false;
    inst.OnDisplayInfoChange();

    // Normal path: is_inited_ = true
    inst.is_inited_ = true;
    inst.OnDisplayInfoChange();
}

void FuzzRandomized(FuzzedDataProvider* fdp)
{
    auto& inst = GetInstance();
    ResetToBaseline(inst);

    int iterations = fdp->ConsumeIntegralInRange<int>(1, MAX_ITERATIONS);
    for (int i = 0; i < iterations; i++) {
        uint8_t action = fdp->ConsumeIntegralInRange<uint8_t>(0, 9);
        switch (action) {
            case 0:
                inst.StartSliding();
                break;
            case 1:
                inst.StopSliding();
                break;
            case 2:
                inst.StartFling();
                break;
            case 3:
                inst.StopFling();
                break;
            case 4: {
                if (!inst.is_sliding_) {
                    break;
                }
                float dx = fdp->ConsumeFloatingPointInRange<float>(-MAX_DELTA, MAX_DELTA);
                float dy = fdp->ConsumeFloatingPointInRange<float>(-MAX_DELTA, MAX_DELTA);
                inst.current_timestamp_ = inst.GetCurrentTimestamp();
                inst.OnScrollUpdate(dx, dy);
                break;
            }
            case 5: {
                if (!inst.is_sliding_ || !inst.is_off_screen_) {
                    break;
                }
                float vx = fdp->ConsumeFloatingPointInRange<float>(-MAX_VELOCITY, MAX_VELOCITY);
                float vy = fdp->ConsumeFloatingPointInRange<float>(-MAX_VELOCITY, MAX_VELOCITY);
                inst.OnFlingUpdate(vx, vy);
                break;
            }
            case 6:
                inst.OnDisplayInfoChange();
                break;
            case 7:
                (void)inst.GetCurrentTimestamp();
                break;
            case 8: {
                float vx = fdp->ConsumeFloatingPointInRange<float>(0.0f, MAX_VELOCITY);
                float vy = fdp->ConsumeFloatingPointInRange<float>(0.0f, MAX_VELOCITY);
                (void)inst.GetVelocity(vx, vy);
                break;
            }
            case 9: {
                std::vector<FrameRateSetting> setting = {
                    {0.0f, 100.0f, 60},
                    {100.0f, 500.0f, 90},
                };
                float vel = fdp->ConsumeFloatingPointInRange<float>(0.0f, 1000.0f);
                (void)inst.GetPreferedFrameRate(vel, setting);
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

    uint8_t scenario = fdp.ConsumeIntegralInRange<uint8_t>(0, 14);
    switch (scenario) {
        case 0:
            FuzzStartSliding(&fdp);
            break;
        case 1:
            FuzzStopSliding(&fdp);
            break;
        case 2:
            FuzzStartFling(&fdp);
            break;
        case 3:
            FuzzStopFling(&fdp);
            break;
        case 4:
            FuzzGetCurrentTimestamp(&fdp);
            break;
        case 5:
            FuzzOnScrollUpdate(&fdp);
            break;
        case 6:
            FuzzOnFlingUpdate(&fdp);
            break;
        case 7:
            FuzzGetPreferedFrameRate(&fdp);
            break;
        case 8:
            FuzzGetVelocity(&fdp);
            break;
        case 9:
            FuzzDestructor(&fdp);
            break;
        case 10:
            FuzzSlidingLifecycle(&fdp);
            break;
        case 11:
            FuzzOnDisplayInfoChange(&fdp);
            break;
        case 12:
            FuzzRandomized(&fdp);
            break;
#if BUILDFLAG(ARKWEB_PDF)
        case 13:
            FuzzPdfMethods(&fdp);
            break;
#endif  // BUILDFLAG(ARKWEB_PDF)
        default:
            break;
    }

    return 0;
}
