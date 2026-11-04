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

#include "arkweb/chromium_ext/ui/events/blink/fling_booster_utils.h"
#include "arkweb/ohos_adapter_ndk/interfaces/mock/mock_ohos_adapter_helper.h"
#include "arkweb/ohos_adapter_ndk/interfaces/mock/mock_system_properties_adapter.h"
#include "base/base_switches.h"
#include "base/test/scoped_command_line.h"
#if BUILDFLAG(ARKWEB_PDF)
#include "arkweb/ohos_adapter_ndk/mock_ndk_api/include/mock_sys_info_util_ext.h"
#include "base/ohos/ltpo/include/sliding_observer.h"
#endif

namespace ui {
namespace test {

constexpr float kMaxBoostFlingSpeed = 9000;
constexpr float kMaxBoostFlingSpeedPdf = 5000;
using testing::Return;
using testing::ReturnRef;

class ScopedOhosAdapterHelperOverride {
 public:
  explicit ScopedOhosAdapterHelperOverride(
      OHOS::NWeb::MockOhosAdapterHelper& helper) {
    OHOS::NWeb::OhosAdapterHelper::SetInstance(nullptr);
    auto& original_hitrace = OHOS::NWeb::OhosAdapterHelper::GetInstance()
                                 .GetHiTraceAdapterInstance();
    ON_CALL(helper, GetHiTraceAdapterInstance())
        .WillByDefault(ReturnRef(original_hitrace));
    OHOS::NWeb::OhosAdapterHelper::SetInstance(&helper);
  }

  ~ScopedOhosAdapterHelperOverride() {
    OHOS::NWeb::OhosAdapterHelper::SetInstance(nullptr);
  }

  ScopedOhosAdapterHelperOverride(const ScopedOhosAdapterHelperOverride&) =
      delete;
  ScopedOhosAdapterHelperOverride& operator=(
      const ScopedOhosAdapterHelperOverride&) = delete;
};

#if BUILDFLAG(ARKWEB_PDF)
class ScopedFlingGlobalState {
 public:
  ScopedFlingGlobalState()
      : original_mock_is_pc_device_(
            base::ohos::SysInfoUtilsMock::mockIsPcDevice),
        original_is_pdf_(base::ohos::SlidingObserver::GetInstance().IsPdf()) {}

  ~ScopedFlingGlobalState() {
    testing::Mock::VerifyAndClearExpectations(
        &base::ohos::SysInfoUtilsMock::GetInstance());
    base::ohos::SysInfoUtilsMock::mockIsPcDevice = original_mock_is_pc_device_;
    base::ohos::SlidingObserver::GetInstance().SetIsPdf(original_is_pdf_);
  }

  ScopedFlingGlobalState(const ScopedFlingGlobalState&) = delete;
  ScopedFlingGlobalState& operator=(const ScopedFlingGlobalState&) = delete;

 private:
  const bool original_mock_is_pc_device_;
  const bool original_is_pdf_;
};
#endif

TEST_F(FlingBoosterTest, ScaleVelocityUsesConfiguredVerticalScale) {
  testing::NiceMock<OHOS::NWeb::MockOhosAdapterHelper> helper;
  testing::NiceMock<OHOS::NWeb::MockSystemPropertiesAdapter> system_properties;
  EXPECT_CALL(helper, GetSystemPropertiesInstance())
      .WillRepeatedly(ReturnRef(system_properties));
  EXPECT_CALL(system_properties,
              GetBoolParameter("web.instructionOptimize.enable", false))
      .WillOnce(Return(false));
  EXPECT_CALL(system_properties, GetScrollVelocityScale())
      .WillOnce(Return("2.0"));
  ScopedOhosAdapterHelperOverride scoped_helper(helper);
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kDisableLowEndDeviceMode);
  WebGestureEvent fling_start = CreateFlingStart(Vector2dF(100, 2000));
  Vector2dF velocity(100, 2000);

  ScaleVelocity(fling_start, velocity);

  EXPECT_EQ(velocity, Vector2dF(100, 4000));
}

TEST_F(FlingBoosterTest, ScaleVelocitySkipsInstructionOptimizedFling) {
  testing::NiceMock<OHOS::NWeb::MockOhosAdapterHelper> helper;
  testing::NiceMock<OHOS::NWeb::MockSystemPropertiesAdapter> system_properties;
  EXPECT_CALL(helper, GetSystemPropertiesInstance())
      .WillRepeatedly(ReturnRef(system_properties));
  EXPECT_CALL(system_properties,
              GetBoolParameter("web.instructionOptimize.enable", false))
      .WillOnce(Return(true));
  EXPECT_CALL(system_properties, GetScrollVelocityScale()).Times(0);
  ScopedOhosAdapterHelperOverride scoped_helper(helper);
  WebGestureEvent fling_start = CreateFlingStart(Vector2dF(100, 2000));
  Vector2dF velocity(100, 2000);

  ScaleVelocity(fling_start, velocity);

  EXPECT_EQ(velocity, Vector2dF(100, 2000));
}

TEST_F(FlingBoosterTest, ScaleVelocityUsesDefaultForInvalidScale) {
  testing::NiceMock<OHOS::NWeb::MockOhosAdapterHelper> helper;
  testing::NiceMock<OHOS::NWeb::MockSystemPropertiesAdapter> system_properties;
  EXPECT_CALL(helper, GetSystemPropertiesInstance())
      .WillRepeatedly(ReturnRef(system_properties));
  EXPECT_CALL(system_properties,
              GetBoolParameter("web.instructionOptimize.enable", false))
      .WillOnce(Return(false));
  EXPECT_CALL(system_properties, GetScrollVelocityScale())
      .WillOnce(Return("invalid"));
  ScopedOhosAdapterHelperOverride scoped_helper(helper);
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kDisableLowEndDeviceMode);
  WebGestureEvent fling_start = CreateFlingStart(Vector2dF(100, 1000));
  Vector2dF velocity(100, 1000);

  ScaleVelocity(fling_start, velocity);

  EXPECT_EQ(velocity, Vector2dF(100, 1200));
}

TEST_F(FlingBoosterTest, ScaleVelocitySkipsLowEndDevice) {
  testing::NiceMock<OHOS::NWeb::MockOhosAdapterHelper> helper;
  testing::NiceMock<OHOS::NWeb::MockSystemPropertiesAdapter> system_properties;
  EXPECT_CALL(helper, GetSystemPropertiesInstance())
      .WillRepeatedly(ReturnRef(system_properties));
  EXPECT_CALL(system_properties,
              GetBoolParameter("web.instructionOptimize.enable", false))
      .WillOnce(Return(false));
  EXPECT_CALL(system_properties, GetScrollVelocityScale()).Times(0);
  ScopedOhosAdapterHelperOverride scoped_helper(helper);
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnableLowEndDeviceMode);
  WebGestureEvent fling_start = CreateFlingStart(Vector2dF(100, 2000));
  Vector2dF velocity(100, 2000);

  ScaleVelocity(fling_start, velocity);

  EXPECT_EQ(velocity, Vector2dF(100, 2000));
}

TEST_F(FlingBoosterTest, ScaleVelocitySkipsHorizontalDominantFling) {
  testing::NiceMock<OHOS::NWeb::MockOhosAdapterHelper> helper;
  testing::NiceMock<OHOS::NWeb::MockSystemPropertiesAdapter> system_properties;
  EXPECT_CALL(helper, GetSystemPropertiesInstance())
      .WillRepeatedly(ReturnRef(system_properties));
  EXPECT_CALL(system_properties,
              GetBoolParameter("web.instructionOptimize.enable", false))
      .WillOnce(Return(false));
  EXPECT_CALL(system_properties, GetScrollVelocityScale()).Times(0);
  ScopedOhosAdapterHelperOverride scoped_helper(helper);
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kDisableLowEndDeviceMode);
  WebGestureEvent fling_start = CreateFlingStart(Vector2dF(2000, 1000));
  Vector2dF velocity(2000, 1000);

  ScaleVelocity(fling_start, velocity);

  EXPECT_EQ(velocity, Vector2dF(2000, 1000));
}

TEST_F(FlingBoosterTest, LimitVelocityPDF) {
  ScopedFlingGlobalState scoped_global_state;
  base::ohos::SysInfoUtilsMock::mockIsPcDevice = true;
  auto& system_properties_mock = ::base::ohos::SysInfoUtilsMock::GetInstance();
  EXPECT_CALL(system_properties_mock, IsPcDevice())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(false));
  Vector2dF max_fling_velocity{std::numeric_limits<float>::max(),
                               std::numeric_limits<float>::max()};
  Vector2dF fling_velocity{10000, 10000};
  base::ohos::SlidingObserver::GetInstance().SetIsPdf(true);
  LimitVelocity(fling_velocity, max_fling_velocity);
  EXPECT_LE(fling_velocity.y(), kMaxBoostFlingSpeedPdf);
  fling_velocity.set_x(-10000);
  fling_velocity.set_y(-10000);
  LimitVelocity(fling_velocity, max_fling_velocity);
  EXPECT_GE(fling_velocity.y(), -kMaxBoostFlingSpeedPdf);
  fling_velocity.set_x(kMaxBoostFlingSpeedPdf);
  fling_velocity.set_y(kMaxBoostFlingSpeedPdf);
  LimitVelocity(fling_velocity, max_fling_velocity);
  EXPECT_EQ(fling_velocity.y(), kMaxBoostFlingSpeedPdf);
  base::ohos::SlidingObserver::GetInstance().SetIsPdf(false);
  base::ohos::SysInfoUtilsMock::mockIsPcDevice = false;
}

TEST_F(FlingBoosterTest, LimitVelocityForPC) {
  ScopedFlingGlobalState scoped_global_state;
  base::ohos::SysInfoUtilsMock::mockIsPcDevice = true;
  auto& system_properties_mock = ::base::ohos::SysInfoUtilsMock::GetInstance();
  EXPECT_CALL(system_properties_mock, IsPcDevice())
      .WillOnce(Return(true))
      .WillRepeatedly(Return(true));
  Vector2dF max_fling_velocity{std::numeric_limits<float>::max(),
                               std::numeric_limits<float>::max()};
  Vector2dF fling_velocity{10000, 10000};
  LimitVelocity(fling_velocity, max_fling_velocity);
  EXPECT_LE(fling_velocity.y(), kMaxBoostFlingSpeed);
  fling_velocity.set_x(-10000);
  fling_velocity.set_y(-10000);
  LimitVelocity(fling_velocity, max_fling_velocity);
  EXPECT_GE(fling_velocity.y(), -kMaxBoostFlingSpeed);
  fling_velocity.set_x(kMaxBoostFlingSpeed);
  fling_velocity.set_y(kMaxBoostFlingSpeed);
  LimitVelocity(fling_velocity, max_fling_velocity);
  EXPECT_EQ(fling_velocity.y(), kMaxBoostFlingSpeed);
  base::ohos::SysInfoUtilsMock::mockIsPcDevice = false;
}

TEST_F(FlingBoosterTest, LimitFlingVelocity_PositiveVelocity) {
  Vector2dF max_fling_velocity_{5000, 5000};
  Vector2dF velocity{10000, 10000};
  EXPECT_NE(velocity, max_fling_velocity_);
  ShouldLimitFlingVelocity(velocity, max_fling_velocity_);
  EXPECT_EQ(velocity, max_fling_velocity_);
}

TEST_F(FlingBoosterTest, LimitFlingVelocity_NegativeVelocity) {
  Vector2dF max_fling_velocity_{5000, 5000};
  Vector2dF velocity{-10000, -10000};
  Vector2dF target{-5000, -5000};
  EXPECT_NE(velocity, max_fling_velocity_);
  ShouldLimitFlingVelocity(velocity, max_fling_velocity_);
  EXPECT_EQ(velocity, target);
}

TEST_F(FlingBoosterTest, LimitFlingVelocity_UnderVelocity) {
  Vector2dF max_fling_velocity_{5000, 5000};
  Vector2dF velocity{100, 500};
  EXPECT_NE(velocity, max_fling_velocity_);
  ShouldLimitFlingVelocity(velocity, max_fling_velocity_);
  EXPECT_NE(velocity, max_fling_velocity_);
}

TEST_F(FlingBoosterTest, LimitVelocityForPCPreservesCustomMaxVelocity) {
  ScopedFlingGlobalState scoped_global_state;
  base::ohos::SysInfoUtilsMock::mockIsPcDevice = true;
  auto& system_properties_mock = ::base::ohos::SysInfoUtilsMock::GetInstance();
  EXPECT_CALL(system_properties_mock, IsPcDevice())
      .WillOnce(Return(true))
      .WillRepeatedly(Return(true));
  Vector2dF max_fling_velocity{5000, 5000};
  Vector2dF fling_velocity{10000, 10000};
  LimitVelocity(fling_velocity, max_fling_velocity);
  EXPECT_EQ(fling_velocity, Vector2dF(10000, 10000));
  base::ohos::SysInfoUtilsMock::mockIsPcDevice = false;
}

TEST_F(FlingBoosterTest, LimitVelocityForNonPC) {
  ScopedFlingGlobalState scoped_global_state;
  base::ohos::SysInfoUtilsMock::mockIsPcDevice = true;
  auto& system_properties_mock = ::base::ohos::SysInfoUtilsMock::GetInstance();
  EXPECT_CALL(system_properties_mock, IsPcDevice())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(false));
  Vector2dF max_fling_velocity{std::numeric_limits<float>::max(),
                               std::numeric_limits<float>::max()};
  Vector2dF fling_velocity{10000, 10000};
  base::ohos::SlidingObserver::GetInstance().SetIsPdf(false);
  LimitVelocity(fling_velocity, max_fling_velocity);
  EXPECT_EQ(fling_velocity.x(), 10000);
  EXPECT_EQ(fling_velocity.y(), 10000);
  base::ohos::SysInfoUtilsMock::mockIsPcDevice = false;
}

TEST_F(FlingBoosterTest, ShouldLimitFlingVelocity_Pdf) {
  ScopedFlingGlobalState scoped_global_state;
  base::ohos::SysInfoUtilsMock::mockIsPcDevice = true;
  auto& system_properties_mock = ::base::ohos::SysInfoUtilsMock::GetInstance();
  EXPECT_CALL(system_properties_mock, IsPcDevice())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(false));
  Vector2dF max_fling_velocity_{5000, 5000};
  Vector2dF velocity{10000, 10000};
  base::ohos::SlidingObserver::GetInstance().SetIsPdf(true);
  Vector2dF original_velocity = velocity;
  ShouldLimitFlingVelocity(velocity, max_fling_velocity_);
  EXPECT_EQ(velocity.x(), original_velocity.x());
  EXPECT_EQ(velocity.y(), original_velocity.y());
  base::ohos::SysInfoUtilsMock::mockIsPcDevice = false;
}

TEST_F(FlingBoosterTest, ShouldLimitFlingVelocity_MixedVelocity) {
  ScopedFlingGlobalState scoped_global_state;
  Vector2dF max_fling_velocity_{5000, 5000};
  Vector2dF velocity{10000, 100};
  base::ohos::SlidingObserver::GetInstance().SetIsPdf(false);
  ShouldLimitFlingVelocity(velocity, max_fling_velocity_);
  EXPECT_EQ(velocity.x(), 5000);
  EXPECT_EQ(velocity.y(), 100);
}

TEST_F(FlingBoosterTest, ShouldLimitFlingVelocity_MixedVelocityNegative) {
  ScopedFlingGlobalState scoped_global_state;
  Vector2dF max_fling_velocity_{5000, 5000};
  Vector2dF velocity{-10000, -100};
  base::ohos::SlidingObserver::GetInstance().SetIsPdf(false);
  ShouldLimitFlingVelocity(velocity, max_fling_velocity_);
  EXPECT_EQ(velocity.x(), -5000);
  EXPECT_EQ(velocity.y(), -100);
}

TEST_F(FlingBoosterTest, ShouldLimitFlingVelocity_OnlyXExceeds) {
  ScopedFlingGlobalState scoped_global_state;
  Vector2dF max_fling_velocity_{5000, 5000};
  Vector2dF velocity{10000, 100};
  base::ohos::SlidingObserver::GetInstance().SetIsPdf(false);
  ShouldLimitFlingVelocity(velocity, max_fling_velocity_);
  EXPECT_EQ(velocity.x(), 5000);
  EXPECT_EQ(velocity.y(), 100);
}

TEST_F(FlingBoosterTest, ShouldLimitFlingVelocity_OnlyYExceeds) {
  ScopedFlingGlobalState scoped_global_state;
  Vector2dF max_fling_velocity_{5000, 5000};
  Vector2dF velocity{100, 10000};
  base::ohos::SlidingObserver::GetInstance().SetIsPdf(false);
  ShouldLimitFlingVelocity(velocity, max_fling_velocity_);
  EXPECT_EQ(velocity.x(), 100);
  EXPECT_EQ(velocity.y(), 5000);
}
}  // namespace test
}  // namespace ui
