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

#include "arkweb/chromium_ext/ui/gl/events/native_scroller_ohos.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cmath>
#include <memory>

#include "arkweb/chromium_ext/ui/events/gestures/blink/native_scroller_ohos_device_source_util.h"
#include "arkweb/ohos_adapter_ndk/hiviewdfx_adapter/hitrace_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/interfaces/mock/mock_ohos_adapter_helper.h"
#include "arkweb/ohos_adapter_ndk/interfaces/mock/mock_system_properties_adapter.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/web_gesture_curve.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gestures/blink/web_gesture_curve_impl.h"
#include "ui/events/gestures/fling_curve.h"

namespace ui {
using namespace ohos_gesture;
struct FlingTestCase {
  blink::WebGestureDevice device_source;
  float initial_velocity_y;
  double expected_start_delta_y;
  double expected_final_delta_y;
  std::string description;
};

const std::vector<FlingTestCase> kTestCases = {
    {blink::WebGestureDevice::kUninitialized, 5000, 2275, 1573,
     "Uninitialized - High speed"},
    {blink::WebGestureDevice::kUninitialized, 1000, 350, 210,
     "Uninitialized - Low speed"},
    {blink::WebGestureDevice::kSyntheticAutoscroll, 5000, 2275, 1573,
     "Synthetic Autoscroll - High speed"},
    {blink::WebGestureDevice::kSyntheticAutoscroll, 1000, 350, 210,
     "Synthetic Autoscroll - Low speed"},
    {blink::WebGestureDevice::kTouchpad, 5000, 1418, 968,
     "Touchpad - High speed"},
    {blink::WebGestureDevice::kTouchpad, 1000, 350, 210,
     "Touchpad - Low speed"},
};

class NativeScrollerOhosPropertyTest : public testing::Test {
 protected:
  void SetUp() override {
    OHOS::NWeb::OhosAdapterHelper::SetInstance(nullptr);
    EXPECT_CALL(helper_, GetHiTraceAdapterInstance())
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::ReturnRef(
            OHOS::NWeb::HiTraceAdapterImpl::GetInstance()));
    OHOS::NWeb::OhosAdapterHelper::SetInstance(&helper_);
  }

  void TearDown() override {
    OHOS::NWeb::OhosAdapterHelper::SetInstance(nullptr);
  }

  testing::NiceMock<OHOS::NWeb::MockOhosAdapterHelper> helper_;
  testing::NiceMock<OHOS::NWeb::MockSystemPropertiesAdapter> system_properties_;
};

TEST(NativeScrollerOhosTest, ParameterizedFlingTest) {
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);

    auto scroller = std::make_unique<NativeScrollerOhos>();
    gfx::Vector2dF velocity(0, test_case.initial_velocity_y);
    scroller->Fling(0, 0, velocity.x(), velocity.y(), INT_MIN,
                    static_cast<float>(INT_MAX), INT_MIN,
                    static_cast<float>(INT_MAX), base::TimeTicks(),
                    ohos_gesture::ConvertNativeScrollerDeviceSource(
                        test_case.device_source));

    gfx::Vector2dF offset;
    auto curve = WebGestureCurveImpl::CreateFromUICurveForTesting(
        std::move(scroller), offset);

    gfx::Vector2dF current_velocity;
    gfx::Vector2dF current_scroll_delta;
    gfx::Vector2dF cumulative_delta;
    EXPECT_TRUE(curve->Advance(0, current_velocity, current_scroll_delta));
    cumulative_delta += current_scroll_delta;
    EXPECT_TRUE(curve->Advance(0.25, current_velocity, current_scroll_delta));
    cumulative_delta += current_scroll_delta;
    EXPECT_NEAR(current_velocity.y(), test_case.expected_start_delta_y, 1);
    EXPECT_EQ(current_velocity.x(), 0);
    EXPECT_GT(cumulative_delta.y(), 0);
    EXPECT_TRUE(
        curve->Advance(0.45, current_velocity,
                       current_scroll_delta));  // Use non-uniform tick spacing.
    cumulative_delta += current_scroll_delta;

    // Ensure fling persists even if successive timestamps are identical.
    gfx::Vector2dF old_velocity = current_velocity;
    EXPECT_TRUE(curve->Advance(0.45, current_velocity, current_scroll_delta));
    EXPECT_EQ(gfx::Vector2dF(), current_scroll_delta);
    EXPECT_EQ(old_velocity, current_velocity);

    curve->Advance(0.75, current_velocity, current_scroll_delta);
    cumulative_delta += current_scroll_delta;
    EXPECT_FALSE(curve->Advance(1.5, current_velocity, current_scroll_delta));
    cumulative_delta += current_scroll_delta;
    EXPECT_NEAR(cumulative_delta.y(), test_case.expected_final_delta_y, 1);
  }
}

TEST(NativeScrollerOhosTest, ConvertNativeScrollerDeviceSource) {
  EXPECT_EQ(ConvertNativeScrollerDeviceSource(
                blink::WebGestureDevice::kUninitialized),
            NativeScrollerOhos::GestureDevice::kUninitialized);
  EXPECT_EQ(ConvertNativeScrollerDeviceSource(
                blink::WebGestureDevice::kTouchpad),
            NativeScrollerOhos::GestureDevice::kTouchpad);
  EXPECT_EQ(ConvertNativeScrollerDeviceSource(
                blink::WebGestureDevice::kTouchscreen),
            NativeScrollerOhos::GestureDevice::kTouchscreen);
  EXPECT_EQ(ConvertNativeScrollerDeviceSource(
                blink::WebGestureDevice::kSyntheticAutoscroll),
            NativeScrollerOhos::GestureDevice::kSyntheticAutoscroll);
  EXPECT_EQ(ConvertNativeScrollerDeviceSource(
                blink::WebGestureDevice::kScrollbar),
            NativeScrollerOhos::GestureDevice::kScrollbar);
}

TEST(NativeScrollerOhosTest, FlingTouchpadDevice) {
  auto scroller = std::make_unique<NativeScrollerOhos>();
  gfx::Vector2dF velocity(0, 5000);
  scroller->Fling(0, 0, velocity.x(), velocity.y(), INT_MIN,
                  static_cast<float>(INT_MAX), INT_MIN,
                  static_cast<float>(INT_MAX), base::TimeTicks(),
                  NativeScrollerOhos::GestureDevice::kTouchpad);

  gfx::Vector2dF offset;
  auto curve = WebGestureCurveImpl::CreateFromUICurveForTesting(
      std::move(scroller), offset);

  gfx::Vector2dF current_velocity;
  gfx::Vector2dF current_scroll_delta;
  EXPECT_TRUE(curve->Advance(0.25, current_velocity, current_scroll_delta));
  EXPECT_GT(current_velocity.y(), 0);
}

TEST(NativeScrollerOhosTest, FlingTouchscreenDevice) {
  auto scroller = std::make_unique<NativeScrollerOhos>();
  gfx::Vector2dF velocity(0, 5000);
  scroller->Fling(0, 0, velocity.x(), velocity.y(), INT_MIN,
                  static_cast<float>(INT_MAX), INT_MIN,
                  static_cast<float>(INT_MAX), base::TimeTicks(),
                  NativeScrollerOhos::GestureDevice::kTouchscreen);

  gfx::Vector2dF offset;
  auto curve = WebGestureCurveImpl::CreateFromUICurveForTesting(
      std::move(scroller), offset);

  gfx::Vector2dF current_velocity;
  gfx::Vector2dF current_scroll_delta;
  EXPECT_TRUE(curve->Advance(0.25, current_velocity, current_scroll_delta));
  EXPECT_GT(current_velocity.y(), 0);
}

TEST(NativeScrollerOhosTest, FlingLowVelocity) {
  auto scroller = std::make_unique<NativeScrollerOhos>();
  gfx::Vector2dF velocity(0, 100);
  scroller->Fling(0, 0, velocity.x(), velocity.y(), INT_MIN,
                  static_cast<float>(INT_MAX), INT_MIN,
                  static_cast<float>(INT_MAX), base::TimeTicks(),
                  NativeScrollerOhos::GestureDevice::kTouchscreen);

  gfx::Vector2dF offset;
  auto curve = WebGestureCurveImpl::CreateFromUICurveForTesting(
      std::move(scroller), offset);

  gfx::Vector2dF current_velocity;
  gfx::Vector2dF current_scroll_delta;
  EXPECT_TRUE(curve->Advance(0.25, current_velocity, current_scroll_delta));
}

TEST(NativeScrollerOhosTest, FlingNegativeVelocity) {
  auto scroller = std::make_unique<NativeScrollerOhos>();
  gfx::Vector2dF velocity(0, -5000);
  scroller->Fling(0, 0, velocity.x(), velocity.y(), INT_MIN,
                  static_cast<float>(INT_MAX), INT_MIN,
                  static_cast<float>(INT_MAX), base::TimeTicks(),
                  NativeScrollerOhos::GestureDevice::kTouchscreen);

  gfx::Vector2dF offset;
  auto curve = WebGestureCurveImpl::CreateFromUICurveForTesting(
      std::move(scroller), offset);

  gfx::Vector2dF current_velocity;
  gfx::Vector2dF current_scroll_delta;
  EXPECT_TRUE(curve->Advance(0.25, current_velocity, current_scroll_delta));
}

TEST(NativeScrollerOhosTest, FlingZeroVelocity) {
  auto scroller = std::make_unique<NativeScrollerOhos>();
  gfx::Vector2dF velocity(0, 0);
  scroller->Fling(0, 0, velocity.x(), velocity.y(), INT_MIN,
                  static_cast<float>(INT_MAX), INT_MIN,
                  static_cast<float>(INT_MAX), base::TimeTicks(),
                  NativeScrollerOhos::GestureDevice::kTouchscreen);

  gfx::Vector2dF offset;
  auto curve = WebGestureCurveImpl::CreateFromUICurveForTesting(
      std::move(scroller), offset);

  gfx::Vector2dF current_velocity;
  gfx::Vector2dF current_scroll_delta;
  EXPECT_FALSE(curve->Advance(0.25, current_velocity, current_scroll_delta));
}

TEST(NativeScrollerOhosTest, FlingScrollbarDevice) {
  auto scroller = std::make_unique<NativeScrollerOhos>();
  gfx::Vector2dF velocity(0, 5000);
  scroller->Fling(0, 0, velocity.x(), velocity.y(), INT_MIN,
                  static_cast<float>(INT_MAX), INT_MIN,
                  static_cast<float>(INT_MAX), base::TimeTicks(),
                  NativeScrollerOhos::GestureDevice::kScrollbar);

  gfx::Vector2dF offset;
  auto curve = WebGestureCurveImpl::CreateFromUICurveForTesting(
      std::move(scroller), offset);

  gfx::Vector2dF current_velocity;
  gfx::Vector2dF current_scroll_delta;
  EXPECT_TRUE(curve->Advance(0.25, current_velocity, current_scroll_delta));
}

TEST(NativeScrollerOhosTest, FlingUninitializedDevice) {
  auto scroller = std::make_unique<NativeScrollerOhos>();
  gfx::Vector2dF velocity(0, 5000);
  scroller->Fling(0, 0, velocity.x(), velocity.y(), INT_MIN,
                  static_cast<float>(INT_MAX), INT_MIN,
                  static_cast<float>(INT_MAX), base::TimeTicks(),
                  NativeScrollerOhos::GestureDevice::kUninitialized);

  gfx::Vector2dF offset;
  auto curve = WebGestureCurveImpl::CreateFromUICurveForTesting(
      std::move(scroller), offset);

  gfx::Vector2dF current_velocity;
  gfx::Vector2dF current_scroll_delta;
  EXPECT_TRUE(curve->Advance(0.25, current_velocity, current_scroll_delta));
}

TEST(NativeScrollerOhosTest, FlingSyntheticAutoscrollDevice) {
  auto scroller = std::make_unique<NativeScrollerOhos>();
  gfx::Vector2dF velocity(0, 5000);
  scroller->Fling(0, 0, velocity.x(), velocity.y(), INT_MIN,
                  static_cast<float>(INT_MAX), INT_MIN,
                  static_cast<float>(INT_MAX), base::TimeTicks(),
                  NativeScrollerOhos::GestureDevice::kSyntheticAutoscroll);

  gfx::Vector2dF offset;
  auto curve = WebGestureCurveImpl::CreateFromUICurveForTesting(
      std::move(scroller), offset);

  gfx::Vector2dF current_velocity;
  gfx::Vector2dF current_scroll_delta;
  EXPECT_TRUE(curve->Advance(0.25, current_velocity, current_scroll_delta));
}

TEST(NativeScrollerOhosTest, ComputeScrollOffsetFinishedState) {
  auto scroller = std::make_unique<NativeScrollerOhos>();
  gfx::Vector2dF velocity(0, 5000);
  scroller->Fling(0, 0, velocity.x(), velocity.y(), INT_MIN,
                  static_cast<float>(INT_MAX), INT_MIN,
                  static_cast<float>(INT_MAX), base::TimeTicks(),
                  NativeScrollerOhos::GestureDevice::kTouchscreen);

  gfx::Vector2dF offset;
  auto curve = WebGestureCurveImpl::CreateFromUICurveForTesting(
      std::move(scroller), offset);

  gfx::Vector2dF current_velocity;
  gfx::Vector2dF current_scroll_delta;
  curve->Advance(1.5, current_velocity, current_scroll_delta);

  EXPECT_FALSE(curve->Advance(2.0, current_velocity, current_scroll_delta));
}

TEST_F(NativeScrollerOhosPropertyTest, UsesConfiguredScrollFriction) {
  EXPECT_CALL(helper_, GetSystemPropertiesInstance())
      .WillOnce(testing::ReturnRef(system_properties_));
  EXPECT_CALL(system_properties_, GetScrollFriction())
      .WillOnce(testing::Return("0.5"));

  GestureConfiguration* configuration = GestureConfiguration::GetInstance();
  const float original_pixel_ratio = configuration->virtual_pixel_ratio();
  configuration->set_virtual_pixel_ratio(1.0f);

  NativeScrollerOhos scroller;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  scroller.Fling(0, 0, 0, 5000, INT_MIN, static_cast<float>(INT_MAX), INT_MIN,
                 static_cast<float>(INT_MAX), start_time,
                 NativeScrollerOhos::GestureDevice::kTouchscreen);

  gfx::Vector2dF offset;
  gfx::Vector2dF velocity;
  EXPECT_TRUE(scroller.ComputeScrollOffset(start_time + base::Milliseconds(250),
                                           &offset, &velocity));
  EXPECT_NEAR(velocity.y(), 5000.0 * std::exp(-2.1 * 0.25), 0.1);
  EXPECT_GT(offset.y(), 0);

  configuration->set_virtual_pixel_ratio(original_pixel_ratio);
}

TEST_F(NativeScrollerOhosPropertyTest,
       ZeroConfiguredFrictionUsesDeviceDefaultAndZeroPixelRatio) {
  EXPECT_CALL(helper_, GetSystemPropertiesInstance())
      .WillOnce(testing::ReturnRef(system_properties_));
  EXPECT_CALL(system_properties_, GetScrollFriction())
      .WillOnce(testing::Return("0"));

  GestureConfiguration* configuration = GestureConfiguration::GetInstance();
  const float original_pixel_ratio = configuration->virtual_pixel_ratio();
  configuration->set_virtual_pixel_ratio(0.0f);

  NativeScrollerOhos scroller;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  scroller.Fling(0, 0, 0, -5000, INT_MIN, static_cast<float>(INT_MAX), INT_MIN,
                 static_cast<float>(INT_MAX), start_time,
                 NativeScrollerOhos::GestureDevice::kTouchscreen);

  gfx::Vector2dF offset;
  gfx::Vector2dF velocity;
  EXPECT_TRUE(scroller.ComputeScrollOffset(start_time + base::Milliseconds(250),
                                           &offset, &velocity));
  EXPECT_NEAR(velocity.y(), -5000.0 * std::exp(-3.15 * 0.25), 0.1);
  EXPECT_LT(offset.y(), 0);

  configuration->set_virtual_pixel_ratio(original_pixel_ratio);
}
}  // namespace ui
