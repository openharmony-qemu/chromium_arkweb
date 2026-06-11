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

#include "base/memory/weak_ptr.h"

#define private public
#include "ui/ozone/platform/headless/headless_screen_ohos.h"
#undef private

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <memory>
#include <string>
#include <vector>

#include "arkweb/ohos_adapter_ndk/interfaces/hitrace_adapter.h"
#include "arkweb/ohos_adapter_ndk/interfaces/mock/mock_ohos_adapter_helper.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "mojo/core/embedder/configuration.h"
#include "mojo/core/embedder/embedder.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/date_time_format_adapter.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/display_manager_adapter.h"
#include "ui/display/display.h"
#include "ui/display/display_list.h"
#include "ui/display/display_observer.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/ozone/public/ozone_platform.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArg;

namespace {

class MockDateTimeFormatAdapter : public OHOS::NWeb::DateTimeFormatAdapter {
 public:
  MOCK_METHOD(void,
              RegTimezoneEvent,
              (std::shared_ptr<OHOS::NWeb::TimezoneEventCallbackAdapter>),
              (override));
  MOCK_METHOD(bool, StartListen, (), (override));
  MOCK_METHOD(void, StopListen, (), (override));
  MOCK_METHOD(std::string, GetTimezone, (), (override));
};

class MockHiTraceAdapter : public OHOS::NWeb::HiTraceAdapter {
 public:
  MOCK_METHOD(void, StartTrace, (const std::string& value, float limit),
              (override));
  MOCK_METHOD(void,
              StartTraceEx,
              (HiTrace_Output_Level level,
               const std::string& name,
               const std::string& value,
               float limit),
              (override));
  MOCK_METHOD(void, FinishTrace, (), (override));
  MOCK_METHOD(void,
              StartAsyncTrace,
              (const std::string& value, int32_t taskId, float limit),
              (override));
  MOCK_METHOD(void,
              FinishAsyncTrace,
              (const std::string& value, int32_t taskId),
              (override));
  MOCK_METHOD(void, CountTrace, (const std::string& name, int64_t count),
              (override));
  MOCK_METHOD(bool, IsHiTraceEnable, (), (override));
  MOCK_METHOD(void, StartOHOSTrace, (const std::string& value, float limit),
              (override));
  MOCK_METHOD(void, FinishOHOSTrace, (), (override));
  MOCK_METHOD(void, CountOHOSTrace, (const std::string& name, int64_t count),
              (override));
  MOCK_METHOD(bool, IsACETraceEnable, (), (override));
};

bool g_create_display_mgr_success = false;
OHOS::NWeb::DisplayManagerAdapter* g_mock_manager_holder = nullptr;

}  // namespace

int main(int argc, char** argv) {
  auto mock_helper = std::make_unique<OHOS::NWeb::MockOhosAdapterHelper>();
  MockHiTraceAdapter mock_hitrace;
  EXPECT_CALL(mock_hitrace, IsHiTraceEnable())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_hitrace, IsACETraceEnable())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_helper, GetHiTraceAdapterInstance())
      .WillRepeatedly(testing::ReturnRef(mock_hitrace));
  EXPECT_CALL(*mock_helper, CreateDateTimeFormatAdapter())
      .WillRepeatedly(testing::Invoke([]() {
        auto adapter = std::make_unique<MockDateTimeFormatAdapter>();
        EXPECT_CALL(*adapter, GetTimezone())
            .WillOnce(testing::Return("Asia/Shanghai"));
        return adapter;
      }));
  EXPECT_CALL(*mock_helper, CreateDisplayMgrAdapter())
      .WillRepeatedly(testing::Invoke([]() {
        if (g_create_display_mgr_success && g_mock_manager_holder) {
          auto* ptr = g_mock_manager_holder;
          g_mock_manager_holder = nullptr;
          return std::unique_ptr<OHOS::NWeb::DisplayManagerAdapter>(ptr);
        }
        return std::unique_ptr<OHOS::NWeb::DisplayManagerAdapter>(nullptr);
      }));
  OHOS::NWeb::OhosAdapterHelper::SetInstance(mock_helper.get());

  base::TestSuite test_suite(argc, argv);
  mojo::core::Init(mojo::core::Configuration());

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOzonePlatform)) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kOzonePlatform, "headless");
  }
  ui::OzonePlatform::InitParams platform_params;
  ui::OzonePlatform::InitializeForUI(platform_params);

  int result = base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));

  OHOS::NWeb::OhosAdapterHelper::SetInstance(nullptr);
  return result;
}

namespace ui {

using namespace OHOS::NWeb;

namespace {

struct MockDisplayManagerAdapter : public DisplayManagerAdapter {
  MockDisplayManagerAdapter() = default;
  ~MockDisplayManagerAdapter() override = default;

  MOCK_METHOD(std::shared_ptr<DisplayAdapter>, GetPrimaryDisplay, (), (override));
  MOCK_METHOD(std::shared_ptr<DisplayAdapter>, GetDefaultDisplay, (), (override));
  MOCK_METHOD(DisplayId, GetDefaultDisplayId, (), (override));
  MOCK_METHOD(std::vector<std::shared_ptr<DisplayAdapter>>,
              GetAllDisplays, (), (override));
  MOCK_METHOD(ListenerId, RegisterDisplayListener,
              (std::shared_ptr<DisplayListenerAdapter>), (override));
  MOCK_METHOD(bool, UnregisterDisplayListener, (ListenerId), (override));
  MOCK_METHOD(bool, IsDefaultPortrait, (), (override));
};

class MockDisplayAdapter : public DisplayAdapter {
 public:
  MOCK_METHOD(DisplayId, GetId, (), (override));
  MOCK_METHOD(int32_t, GetWidth, (), (override));
  MOCK_METHOD(int32_t, GetHeight, (), (override));
  MOCK_METHOD(float, GetVirtualPixelRatio, (), (override));
  MOCK_METHOD(RotationType, GetRotation, (), (override));
  MOCK_METHOD(OrientationType, GetOrientation, (), (override));
  MOCK_METHOD(int32_t, GetDpi, (), (override));
  MOCK_METHOD(DisplayOrientation, GetDisplayOrientation, (), (override));
  MOCK_METHOD(FoldStatus, GetFoldStatus, (), (override));
  MOCK_METHOD(bool, IsFoldable, (), (override));
  MOCK_METHOD(std::string, GetName, (), (override));
  MOCK_METHOD(int32_t, GetAvailableWidth, (), (override));
  MOCK_METHOD(int32_t, GetAvailableHeight, (), (override));
  MOCK_METHOD(bool, GetAliveStatus, (), (override));
  MOCK_METHOD(DisplayState, GetDisplayState, (), (override));
  MOCK_METHOD(int32_t, GetDensityDpi, (), (override));
  MOCK_METHOD(int32_t, GetX, (), (override));
  MOCK_METHOD(int32_t, GetY, (), (override));
  MOCK_METHOD(DisplaySourceMode, GetDisplaySourceMode, (), (override));
  MOCK_METHOD(int32_t, GetPhysicalWidth, (), (override));
  MOCK_METHOD(int32_t, GetPhysicalHeight, (), (override));
  MOCK_METHOD(float, GetDefaultVirtualPixelRatio, (), (override));
};

struct MockDisplayObserver : public display::DisplayObserver {
  MockDisplayObserver() = default;
  ~MockDisplayObserver() override = default;

  MOCK_METHOD1(OnDisplayAdded, void(const display::Display& new_display));
  MOCK_METHOD1(OnDisplaysRemoved,
               void(const display::Displays& removed_displays));
  MOCK_METHOD2(OnDisplayMetricsChanged,
               void(const display::Display& display, uint32_t changed_metrics));
};

void SetupValidDisplayAdapter(MockDisplayAdapter* adapter,
                              DisplayId id = 0,
                              float scale = 0.0f) {
  EXPECT_CALL(*adapter, GetDisplaySourceMode())
      .WillRepeatedly(Return(DisplaySourceMode::MAIN));
  EXPECT_CALL(*adapter, GetId()).WillRepeatedly(Return(id));
  EXPECT_CALL(*adapter, GetRotation())
      .WillRepeatedly(Return(RotationType::ROTATION_0));
  EXPECT_CALL(*adapter, GetVirtualPixelRatio()).WillRepeatedly(Return(scale));
  if (scale != 0.0f) {
    EXPECT_CALL(*adapter, GetX()).WillRepeatedly(Return(0));
    EXPECT_CALL(*adapter, GetY()).WillRepeatedly(Return(0));
    EXPECT_CALL(*adapter, GetWidth()).WillRepeatedly(Return(1920));
    EXPECT_CALL(*adapter, GetHeight()).WillRepeatedly(Return(1080));
    EXPECT_CALL(*adapter, GetAvailableWidth()).WillRepeatedly(Return(1920));
    EXPECT_CALL(*adapter, GetAvailableHeight()).WillRepeatedly(Return(1080));
  }
}

void SetupManagerForSuccess(
    MockDisplayManagerAdapter* manager,
    std::shared_ptr<MockDisplayAdapter> adapter) {
  EXPECT_CALL(*manager, GetDefaultDisplayId()).WillRepeatedly(Return(0));
  EXPECT_CALL(*manager, GetPrimaryDisplay())
      .WillRepeatedly(testing::Invoke(
          [adapter]() { return std::static_pointer_cast<DisplayAdapter>(adapter); }));
  EXPECT_CALL(*manager, GetAllDisplays())
      .WillRepeatedly(testing::Invoke(
          [adapter]() -> std::vector<std::shared_ptr<DisplayAdapter>> {
            return {std::static_pointer_cast<DisplayAdapter>(adapter)};
          }));
  EXPECT_CALL(*manager, RegisterDisplayListener(_))
      .WillRepeatedly(Return(1));
  EXPECT_CALL(*manager, UnregisterDisplayListener(_))
      .WillRepeatedly(Return(true));
}

void RemoveNonPrimaryDisplays(HeadlessScreenOhos* screen) {
  auto primary_it = screen->display_list_.GetPrimaryDisplayIterator();
  if (primary_it == screen->display_list_.displays().end()) {
    return;
  }
  int64_t primary_id = primary_it->id();
  std::vector<int64_t> to_remove;
  for (const auto& d : screen->display_list_.displays()) {
    if (d.id() != primary_id) {
      to_remove.push_back(d.id());
    }
  }
  for (int64_t id : to_remove) {
    screen->display_list_.RemoveDisplay(id);
  }
}

}  // namespace

class HeadlessScreenOhosTest : public testing::Test {
 protected:
  void SetUp() override {
    screen_ = std::make_unique<HeadlessScreenOhos>();
    RemoveNonPrimaryDisplays(screen_.get());
    listener_ = screen_->display_listener_;
  }

  void TearDown() override {
    if (screen_) {
      screen_->RemoveObserver(&observer_);
    }
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<HeadlessScreenOhos> screen_;
  std::shared_ptr<HeadlessScreenListener> listener_;
  display::Display dst_display{1, gfx::Rect(0, 0, 800, 600)};
  MockDisplayObserver observer_;
};

// --- Constructor / Destructor ---

TEST_F(HeadlessScreenOhosTest, ConstructorFallback) {
  ASSERT_NE(screen_, nullptr);
  auto displays = screen_->GetAllDisplays();
  ASSERT_EQ(displays.size(), 1u);
  EXPECT_EQ(displays[0].id(), 0);
}

TEST_F(HeadlessScreenOhosTest, ConstructorWithValidScreenSize) {
  screen_.reset();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kOzoneOverrideScreenSize, "1920,1080");
  auto screen = std::make_unique<HeadlessScreenOhos>();
  RemoveNonPrimaryDisplays(screen.get());
  auto displays = screen->GetAllDisplays();
  ASSERT_EQ(displays.size(), 1u);
  EXPECT_EQ(displays[0].bounds().width(), 1920);
  EXPECT_EQ(displays[0].bounds().height(), 1080);
}

TEST_F(HeadlessScreenOhosTest, ConstructorWithInvalidScreenSizeBadFormat) {
  screen_.reset();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kOzoneOverrideScreenSize, "abc");
  auto screen = std::make_unique<HeadlessScreenOhos>();
  RemoveNonPrimaryDisplays(screen.get());
  auto displays = screen->GetAllDisplays();
  ASSERT_EQ(displays.size(), 1u);
  EXPECT_EQ(displays[0].bounds().size().width(), 0);
}

TEST_F(HeadlessScreenOhosTest, ConstructorWithInvalidScreenSizeNonNumeric) {
  screen_.reset();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kOzoneOverrideScreenSize, "abc,def");
  auto screen = std::make_unique<HeadlessScreenOhos>();
  RemoveNonPrimaryDisplays(screen.get());
  auto displays = screen->GetAllDisplays();
  ASSERT_EQ(displays.size(), 1u);
  EXPECT_EQ(displays[0].bounds().size().width(), 0);
}

TEST_F(HeadlessScreenOhosTest, DestructorWithUnregister) {
  auto manager = std::make_unique<MockDisplayManagerAdapter>();
  EXPECT_CALL(*manager, UnregisterDisplayListener(_))
      .WillOnce(Return(true));
  screen_->display_manager_adapter_ = std::move(manager);
  screen_->display_listener_id_ = 1;
  screen_->display_listener_ =
      std::make_shared<HeadlessScreenListener>(
          screen_->weak_factory_.GetWeakPtr());
  screen_.reset();
}

TEST_F(HeadlessScreenOhosTest, DestructorWithoutListener) {
  screen_->display_listener_ = nullptr;
  screen_.reset();
}

TEST_F(HeadlessScreenOhosTest, DestructorWithoutManager) {
  screen_->display_manager_adapter_ = nullptr;
  screen_.reset();
}

TEST_F(HeadlessScreenOhosTest, DestructorWithNegativeListenerId) {
  auto manager = std::make_unique<MockDisplayManagerAdapter>();
  screen_->display_manager_adapter_ = std::move(manager);
  screen_->display_listener_id_ = -1;
  screen_->display_listener_ =
      std::make_shared<HeadlessScreenListener>(
          screen_->weak_factory_.GetWeakPtr());
  screen_.reset();
}

// --- Initialize (via successful construction) ---

TEST_F(HeadlessScreenOhosTest, InitializeSuccess) {
  screen_.reset();
  auto manager = std::make_unique<MockDisplayManagerAdapter>();
  auto adapter = std::make_shared<MockDisplayAdapter>();
  SetupValidDisplayAdapter(adapter.get(), 0, 0.0f);
  SetupManagerForSuccess(manager.get(), adapter);
  g_mock_manager_holder = manager.release();
  g_create_display_mgr_success = true;
  auto screen = std::make_unique<HeadlessScreenOhos>();
  g_create_display_mgr_success = false;
  g_mock_manager_holder = nullptr;
  RemoveNonPrimaryDisplays(screen.get());
  ASSERT_NE(screen, nullptr);
  auto displays = screen->GetAllDisplays();
  EXPECT_FALSE(displays.empty());
}

TEST_F(HeadlessScreenOhosTest, InitializeFetchDisplaysFails) {
  screen_.reset();
  auto manager = std::make_unique<MockDisplayManagerAdapter>();
  EXPECT_CALL(*manager, GetDefaultDisplayId()).WillRepeatedly(Return(0));
  EXPECT_CALL(*manager, GetPrimaryDisplay())
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(*manager, GetAllDisplays())
      .WillRepeatedly(Return(std::vector<std::shared_ptr<DisplayAdapter>>{}));
  EXPECT_CALL(*manager, RegisterDisplayListener(_))
      .WillRepeatedly(Return(1));
  g_mock_manager_holder = manager.release();
  g_create_display_mgr_success = true;
  auto screen = std::make_unique<HeadlessScreenOhos>();
  g_create_display_mgr_success = false;
  g_mock_manager_holder = nullptr;
  ASSERT_NE(screen, nullptr);
}

TEST_F(HeadlessScreenOhosTest, InitializeRegisterFails) {
  screen_.reset();
  auto manager = std::make_unique<MockDisplayManagerAdapter>();
  auto adapter = std::make_shared<MockDisplayAdapter>();
  SetupValidDisplayAdapter(adapter.get(), 0, 0.0f);
  SetupManagerForSuccess(manager.get(), adapter);
  EXPECT_CALL(*manager, RegisterDisplayListener(_))
      .WillRepeatedly(Return(-1));
  g_mock_manager_holder = manager.release();
  g_create_display_mgr_success = true;
  auto screen = std::make_unique<HeadlessScreenOhos>();
  g_create_display_mgr_success = false;
  g_mock_manager_holder = nullptr;
  ASSERT_NE(screen, nullptr);
  EXPECT_NE(screen->display_listener_, nullptr);
}

// --- FetchDisplays ---

TEST_F(HeadlessScreenOhosTest, FetchDisplaysNoAdapter) {
  display::DisplayList displays;
  screen_->display_manager_adapter_ = nullptr;
  EXPECT_FALSE(screen_->FetchDisplays(displays));
}

TEST_F(HeadlessScreenOhosTest, FetchDisplaysSuccessWithPrimary) {
  display::DisplayList displays;
  auto manager = std::make_unique<MockDisplayManagerAdapter>();
  auto adapter = std::make_shared<MockDisplayAdapter>();
  SetupValidDisplayAdapter(adapter.get(), 0, 0.0f);
  SetupManagerForSuccess(manager.get(), adapter);
  screen_->display_manager_adapter_ = std::move(manager);
  EXPECT_TRUE(screen_->FetchDisplays(displays));
  EXPECT_FALSE(displays.displays().empty());
}

TEST_F(HeadlessScreenOhosTest, FetchDisplaysSuccessWithNonPrimary) {
  display::DisplayList displays;
  auto manager = std::make_unique<MockDisplayManagerAdapter>();
  auto adapter = std::make_shared<MockDisplayAdapter>();
  SetupValidDisplayAdapter(adapter.get(), 1, 0.0f);
  EXPECT_CALL(*manager, GetDefaultDisplayId()).WillRepeatedly(Return(0));
  EXPECT_CALL(*manager, GetPrimaryDisplay())
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(*manager, GetAllDisplays())
      .WillRepeatedly(testing::Invoke([adapter]() {
        return std::vector<std::shared_ptr<DisplayAdapter>>{adapter};
      }));
  screen_->display_manager_adapter_ = std::move(manager);
  EXPECT_TRUE(screen_->FetchDisplays(displays));
  EXPECT_EQ(displays.displays().size(), 1u);
}

TEST_F(HeadlessScreenOhosTest, FetchDisplaysEmptyNoValidDisplays) {
  display::DisplayList displays;
  auto manager = std::make_unique<MockDisplayManagerAdapter>();
  auto adapter = std::make_shared<MockDisplayAdapter>();
  EXPECT_CALL(*manager, GetDefaultDisplayId()).WillRepeatedly(Return(0));
  EXPECT_CALL(*manager, GetPrimaryDisplay())
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(*manager, GetAllDisplays())
      .WillRepeatedly(testing::Invoke([adapter]() {
        return std::vector<std::shared_ptr<DisplayAdapter>>{adapter};
      }));
  EXPECT_CALL(*adapter, GetDisplaySourceMode())
      .WillRepeatedly(Return(DisplaySourceMode::NONE));
  EXPECT_CALL(*adapter, GetId()).WillRepeatedly(Return(UINT64_MAX));
  screen_->display_manager_adapter_ = std::move(manager);
  EXPECT_FALSE(screen_->FetchDisplays(displays));
  EXPECT_TRUE(displays.displays().empty());
}

TEST_F(HeadlessScreenOhosTest, FetchDisplaysPrimaryDisplayWithId) {
  display::DisplayList displays;
  auto manager = std::make_unique<MockDisplayManagerAdapter>();
  auto adapter = std::make_shared<MockDisplayAdapter>();
  SetupValidDisplayAdapter(adapter.get(), 42, 0.0f);
  EXPECT_CALL(*manager, GetDefaultDisplayId()).WillRepeatedly(Return(42));
  EXPECT_CALL(*manager, GetPrimaryDisplay())
      .WillRepeatedly(testing::Invoke([adapter]() {
        return std::shared_ptr<DisplayAdapter>(adapter);
      }));
  EXPECT_CALL(*manager, GetAllDisplays())
      .WillRepeatedly(testing::Invoke([adapter]() {
        return std::vector<std::shared_ptr<DisplayAdapter>>{adapter};
      }));
  screen_->display_manager_adapter_ = std::move(manager);
  EXPECT_TRUE(screen_->FetchDisplays(displays));
  ASSERT_EQ(displays.displays().size(), 1u);
  EXPECT_EQ(displays.displays()[0].id(), 42);
}

// --- ConvertDisplay ---

TEST_F(HeadlessScreenOhosTest, ConvertDisplayNullSrcDisplay) {
  EXPECT_FALSE(screen_->ConvertDisplay(nullptr, dst_display));
}

TEST_F(HeadlessScreenOhosTest, ConvertDisplayModeIsNone) {
  auto adapter = std::make_shared<MockDisplayAdapter>();
  EXPECT_CALL(*adapter, GetDisplaySourceMode())
      .WillRepeatedly(Return(DisplaySourceMode::NONE));
  EXPECT_FALSE(screen_->ConvertDisplay(adapter, dst_display));
}

TEST_F(HeadlessScreenOhosTest, ConvertDisplayModeIsMAIN) {
  auto adapter = std::make_shared<MockDisplayAdapter>();
  SetupValidDisplayAdapter(adapter.get(), 0, 0.0f);
  EXPECT_TRUE(screen_->ConvertDisplay(adapter, dst_display));
}

TEST_F(HeadlessScreenOhosTest, ConvertDisplayModeIsEXTEND) {
  auto adapter = std::make_shared<MockDisplayAdapter>();
  EXPECT_CALL(*adapter, GetDisplaySourceMode())
      .WillRepeatedly(Return(DisplaySourceMode::EXTEND));
  EXPECT_CALL(*adapter, GetId()).WillRepeatedly(Return(0));
  EXPECT_CALL(*adapter, GetRotation())
      .WillRepeatedly(Return(RotationType::ROTATION_0));
  EXPECT_CALL(*adapter, GetVirtualPixelRatio()).WillRepeatedly(Return(0.0f));
  EXPECT_TRUE(screen_->ConvertDisplay(adapter, dst_display));
}

TEST_F(HeadlessScreenOhosTest, ConvertDisplayNegativeId) {
  auto adapter = std::make_shared<MockDisplayAdapter>();
  EXPECT_CALL(*adapter, GetDisplaySourceMode())
      .WillRepeatedly(Return(DisplaySourceMode::MAIN));
  EXPECT_CALL(*adapter, GetId()).WillRepeatedly(Return(UINT64_MAX));
  EXPECT_FALSE(screen_->ConvertDisplay(adapter, dst_display));
}

TEST_F(HeadlessScreenOhosTest, ConvertDisplayScaleNonZero) {
  auto adapter = std::make_shared<MockDisplayAdapter>();
  SetupValidDisplayAdapter(adapter.get(), 1, 1.5f);
  display::Display display;
  EXPECT_TRUE(screen_->ConvertDisplay(adapter, display));
  EXPECT_EQ(display.id(), 1);
  EXPECT_GT(display.bounds().width(), 0);
}

TEST_F(HeadlessScreenOhosTest, ConvertDisplayScaleWithOverride) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kOzoneOverrideScreenSize, "800,600");
  auto adapter = std::make_shared<MockDisplayAdapter>();
  SetupValidDisplayAdapter(adapter.get(), 2, 2.0f);
  display::Display display;
  EXPECT_TRUE(screen_->ConvertDisplay(adapter, display));
  EXPECT_EQ(display.bounds().width(), 400);
  EXPECT_EQ(display.bounds().height(), 300);
}

TEST_F(HeadlessScreenOhosTest, ConvertDisplayRotation0) {
  auto adapter = std::make_shared<MockDisplayAdapter>();
  SetupValidDisplayAdapter(adapter.get(), 0, 0.0f);
  EXPECT_CALL(*adapter, GetRotation())
      .WillRepeatedly(Return(RotationType::ROTATION_0));
  display::Display display;
  EXPECT_TRUE(screen_->ConvertDisplay(adapter, display));
  EXPECT_EQ(display.rotation(), display::Display::ROTATE_0);
}

TEST_F(HeadlessScreenOhosTest, ConvertDisplayRotation90) {
  auto adapter = std::make_shared<MockDisplayAdapter>();
  SetupValidDisplayAdapter(adapter.get(), 0, 0.0f);
  EXPECT_CALL(*adapter, GetRotation())
      .WillRepeatedly(Return(RotationType::ROTATION_90));
  display::Display display;
  EXPECT_TRUE(screen_->ConvertDisplay(adapter, display));
  EXPECT_EQ(display.rotation(), display::Display::ROTATE_90);
}

TEST_F(HeadlessScreenOhosTest, ConvertDisplayRotation180) {
  auto adapter = std::make_shared<MockDisplayAdapter>();
  SetupValidDisplayAdapter(adapter.get(), 0, 0.0f);
  EXPECT_CALL(*adapter, GetRotation())
      .WillRepeatedly(Return(RotationType::ROTATION_180));
  display::Display display;
  EXPECT_TRUE(screen_->ConvertDisplay(adapter, display));
  EXPECT_EQ(display.rotation(), display::Display::ROTATE_180);
}

TEST_F(HeadlessScreenOhosTest, ConvertDisplayRotation270) {
  auto adapter = std::make_shared<MockDisplayAdapter>();
  SetupValidDisplayAdapter(adapter.get(), 0, 0.0f);
  EXPECT_CALL(*adapter, GetRotation())
      .WillRepeatedly(Return(RotationType::ROTATION_270));
  display::Display display;
  EXPECT_TRUE(screen_->ConvertDisplay(adapter, display));
  EXPECT_EQ(display.rotation(), display::Display::ROTATE_270);
}

TEST_F(HeadlessScreenOhosTest, ConvertDisplayRotationButt) {
  auto adapter = std::make_shared<MockDisplayAdapter>();
  SetupValidDisplayAdapter(adapter.get(), 0, 0.0f);
  EXPECT_CALL(*adapter, GetRotation())
      .WillRepeatedly(Return(RotationType::ROTATION_BUTT));
  display::Display display;
  EXPECT_TRUE(screen_->ConvertDisplay(adapter, display));
}

// --- OnDisplayEvent ---

TEST_F(HeadlessScreenOhosTest, OnDisplayEventCreateFetchFails) {
  screen_->display_manager_adapter_ = nullptr;
  screen_->OnDisplayEvent("create", 1);
  auto displays = screen_->GetAllDisplays();
  EXPECT_EQ(displays.size(), 1u);
}

TEST_F(HeadlessScreenOhosTest, OnDisplayEventCreatePrimary) {
  auto manager = std::make_unique<MockDisplayManagerAdapter>();
  auto adapter = std::make_shared<MockDisplayAdapter>();
  SetupValidDisplayAdapter(adapter.get(), 10, 0.0f);
  EXPECT_CALL(*manager, GetDefaultDisplayId()).WillRepeatedly(Return(10));
  EXPECT_CALL(*manager, GetPrimaryDisplay())
      .WillRepeatedly(testing::Invoke([adapter]() {
        return std::shared_ptr<DisplayAdapter>(adapter);
      }));
  EXPECT_CALL(*manager, GetAllDisplays())
      .WillRepeatedly(testing::Invoke([adapter]() {
        return std::vector<std::shared_ptr<DisplayAdapter>>{adapter};
      }));
  screen_->display_manager_adapter_ = std::move(manager);
  screen_->OnDisplayEvent("create", 10);
  auto displays = screen_->GetAllDisplays();
  EXPECT_GE(displays.size(), 1u);
}

TEST_F(HeadlessScreenOhosTest, OnDisplayEventCreateNotPrimary) {
  auto manager = std::make_unique<MockDisplayManagerAdapter>();
  auto adapter = std::make_shared<MockDisplayAdapter>();
  SetupValidDisplayAdapter(adapter.get(), 20, 0.0f);
  EXPECT_CALL(*manager, GetDefaultDisplayId()).WillRepeatedly(Return(0));
  EXPECT_CALL(*manager, GetPrimaryDisplay())
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(*manager, GetAllDisplays())
      .WillRepeatedly(testing::Invoke([adapter]() {
        return std::vector<std::shared_ptr<DisplayAdapter>>{adapter};
      }));
  screen_->display_manager_adapter_ = std::move(manager);
  screen_->OnDisplayEvent("create", 20);
}

TEST_F(HeadlessScreenOhosTest, OnDisplayEventChangeFetchFails) {
  screen_->display_manager_adapter_ = nullptr;
  screen_->OnDisplayEvent("change", 1);
}

TEST_F(HeadlessScreenOhosTest, OnDisplayEventDestroyFound) {
  display::Display display(100, gfx::Rect(0, 0, 800, 600));
  screen_->display_list_.AddDisplay(display, display::DisplayList::Type::PRIMARY);
  ASSERT_NE(screen_->display_list_.FindDisplayById(100),
            screen_->display_list_.displays().end());
  screen_->OnDisplayEvent("destroy", 100);
  EXPECT_EQ(screen_->display_list_.FindDisplayById(100),
            screen_->display_list_.displays().end());
}

TEST_F(HeadlessScreenOhosTest, OnDisplayEventDestroyNotFound) {
  auto size_before = screen_->GetAllDisplays().size();
  screen_->OnDisplayEvent("destroy", 9999);
  EXPECT_EQ(screen_->GetAllDisplays().size(), size_before);
}

TEST_F(HeadlessScreenOhosTest, OnDisplayEventCreateDisplayNotFound) {
  auto manager = std::make_unique<MockDisplayManagerAdapter>();
  auto adapter = std::make_shared<MockDisplayAdapter>();
  SetupValidDisplayAdapter(adapter.get(), 10, 0.0f);
  EXPECT_CALL(*manager, GetDefaultDisplayId()).WillRepeatedly(Return(10));
  EXPECT_CALL(*manager, GetPrimaryDisplay())
      .WillRepeatedly(testing::Invoke([adapter]() {
        return std::shared_ptr<DisplayAdapter>(adapter);
      }));
  EXPECT_CALL(*manager, GetAllDisplays())
      .WillRepeatedly(testing::Invoke([adapter]() {
        return std::vector<std::shared_ptr<DisplayAdapter>>{adapter};
      }));
  screen_->display_manager_adapter_ = std::move(manager);
  auto size_before = screen_->GetAllDisplays().size();
  screen_->OnDisplayEvent("create", 999);
  EXPECT_EQ(screen_->GetAllDisplays().size(), size_before);
}

TEST_F(HeadlessScreenOhosTest, OnDisplayEventUnknown) {
  screen_->OnDisplayEvent("unknown", 1);
}

// --- OnDisplayCreate / Destroy / Change ---

TEST_F(HeadlessScreenOhosTest, OnDisplayCreate) {
  screen_->OnDisplayCreate(1);
  EXPECT_FALSE(screen_->GetAllDisplays().empty());
}

TEST_F(HeadlessScreenOhosTest, OnDisplayDestroy) {
  screen_->OnDisplayCreate(1);
  screen_->OnDisplayDestroy(1);
}

TEST_F(HeadlessScreenOhosTest, OnDisplayChange) {
  screen_->OnDisplayChange(1);
}

TEST_F(HeadlessScreenOhosTest, OnDisplayCreateAfterDestroy) {
  auto listener = std::make_shared<HeadlessScreenListener>(
      screen_->weak_factory_.GetWeakPtr());
  screen_.reset();
  listener->OnCreate(1);
  task_environment_.RunUntilIdle();
}

// --- HeadlessScreenListener ---

TEST_F(HeadlessScreenOhosTest, ListenerOnCreateForwards) {
  screen_.reset();
  auto screen = std::make_unique<HeadlessScreenOhos>();
  RemoveNonPrimaryDisplays(screen.get());
  auto listener = std::make_shared<HeadlessScreenListener>(
      screen->weak_factory_.GetWeakPtr());
  ASSERT_NE(listener, nullptr);
  listener->OnCreate(1);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(screen->GetAllDisplays().empty());
}

TEST_F(HeadlessScreenOhosTest, ListenerOnDestroyForwards) {
  screen_.reset();
  auto screen = std::make_unique<HeadlessScreenOhos>();
  RemoveNonPrimaryDisplays(screen.get());
  auto listener = std::make_shared<HeadlessScreenListener>(
      screen->weak_factory_.GetWeakPtr());
  listener->OnDestroy(1);
  task_environment_.RunUntilIdle();
}

TEST_F(HeadlessScreenOhosTest, ListenerOnChangeForwards) {
  screen_.reset();
  auto screen = std::make_unique<HeadlessScreenOhos>();
  RemoveNonPrimaryDisplays(screen.get());
  auto listener = std::make_shared<HeadlessScreenListener>(
      screen->weak_factory_.GetWeakPtr());
  listener->OnChange(1);
  task_environment_.RunUntilIdle();
}

TEST_F(HeadlessScreenOhosTest, ListenerOnCreateAfterScreenDestroyed) {
  auto listener = std::make_shared<HeadlessScreenListener>(
      screen_->weak_factory_.GetWeakPtr());
  screen_.reset();
  listener->OnCreate(1);
  task_environment_.RunUntilIdle();
}

TEST_F(HeadlessScreenOhosTest, ListenerOnDestroyAfterScreenDestroyed) {
  auto listener = std::make_shared<HeadlessScreenListener>(
      screen_->weak_factory_.GetWeakPtr());
  screen_.reset();
  listener->OnDestroy(1);
  task_environment_.RunUntilIdle();
}

TEST_F(HeadlessScreenOhosTest, ListenerOnChangeAfterScreenDestroyed) {
  auto listener = std::make_shared<HeadlessScreenListener>(
      screen_->weak_factory_.GetWeakPtr());
  screen_.reset();
  listener->OnChange(1);
  task_environment_.RunUntilIdle();
}

// --- LayoutDisplays ---

TEST_F(HeadlessScreenOhosTest, LayoutDisplaysNoPrimaryDisplay) {
  display::Displays displays = {
      display::Display(0, gfx::Rect(100, 0, 100, 100)),
      display::Display(1, gfx::Rect(-100, 0, 100, 100))};
  screen_->LayoutDisplays(displays);
  EXPECT_EQ(displays[0].bounds().origin(), gfx::Point(100, 0));
  EXPECT_EQ(displays[1].bounds().origin(), gfx::Point(-100, 0));
}

TEST_F(HeadlessScreenOhosTest, LayoutDisplaysPrimaryWithZeroSize) {
  display::Displays displays = {
      display::Display(0, gfx::Rect(0, 0, 0, 0)),
      display::Display(1, gfx::Rect(0, 0, 100, 100))};
  screen_->LayoutDisplays(displays);
  EXPECT_EQ(displays[0].bounds().origin(), gfx::Point(0, 0));
}

TEST_F(HeadlessScreenOhosTest, LayoutDisplaysMultiDisplay) {
  display::Displays displays = {
      display::Display(0, gfx::Rect(0, 0, 2000, 2000)),
      display::Display(1, gfx::Rect(2000, 0, 1000, 2000)),
      display::Display(2, gfx::Rect(1000, 0, 2000, 2000)),
      display::Display(3, gfx::Rect(2000, 3000, 1000, 1000)),
      display::Display(4, gfx::Rect(2000, -1000, 1000, 1000)),
      display::Display(5, gfx::Rect(3000, 2000, 1000, 1000)),
      display::Display(6, gfx::Rect(0, 1000, 2000, 2000)),
      display::Display(7, gfx::Rect(0, 2000, 2000, 1000)),
      display::Display(8, gfx::Rect(-1000, 2000, 1000, 1000)),
      display::Display(9, gfx::Rect(1000, 1000, 1000, 1000)),
      display::Display(10, gfx::Rect(-2000, 2000, 1000, 1000)),
      display::Display(11, gfx::Rect(2000, -2000, 1000, 1000)),
      display::Display(12, gfx::Rect(-500, 2000, 3000, 1000)),
      display::Display(13, gfx::Rect(-1000, -1000, 1000, 1000)),
      display::Display(14, gfx::Rect(500, -1000, 1000, 1000))};
  screen_->LayoutDisplays(displays);
  EXPECT_EQ(displays[0].bounds().origin(), gfx::Point(0, 0));
  EXPECT_EQ(displays[1].bounds().origin(), gfx::Point(2000, 0));
  EXPECT_EQ(displays[4].bounds().origin(), gfx::Point(2000, -1000));
  EXPECT_EQ(displays[7].bounds().origin(), gfx::Point(0, 2000));
  EXPECT_EQ(displays[8].bounds().origin(), gfx::Point(-1000, 2000));
  EXPECT_EQ(displays[13].bounds().origin(), gfx::Point(-1000, -1000));
}

TEST_F(HeadlessScreenOhosTest, LayoutDisplaysComplexLayout) {
  display::Displays displays = {
      display::Display(0, gfx::Rect(0, 0, 100, 3000)),
      display::Display(1, gfx::Rect(1000, 0, 1000, 2000)),
      display::Display(2, gfx::Rect(1000, 0, 2000, 2000)),
      display::Display(3, gfx::Rect(2000, 3000, 1000, 1000)),
      display::Display(4, gfx::Rect(3000, -2000, 1000, 1000)),
      display::Display(5, gfx::Rect(3000, 2000, 1000, 1000)),
      display::Display(6, gfx::Rect(0, 1000, 2000, 2000)),
      display::Display(7, gfx::Rect(500, 1000, 2000, 1000)),
      display::Display(8, gfx::Rect(-400, 3000, 1000, 1000)),
      display::Display(9, gfx::Rect(1000, 1000, 1000, 1000)),
      display::Display(10, gfx::Rect(-2000, 2000, 1000, 1000))};
  screen_->LayoutDisplays(displays);
  EXPECT_EQ(displays[0].bounds().origin(), gfx::Point(0, 0));
  EXPECT_EQ(displays[1].bounds().origin(), gfx::Point(4100, 0));
  EXPECT_EQ(displays[2].bounds().origin(), gfx::Point(2100, 0));
  EXPECT_EQ(displays[8].bounds().origin(), gfx::Point(-400, 3000));
  EXPECT_EQ(displays[10].bounds().origin(), gfx::Point(-2000, 2000));
}

TEST_F(HeadlessScreenOhosTest, LayoutDisplaysZeroScaleParent) {
  display::Display primary(0, gfx::Rect(0, 0, 2000, 2000));
  primary.set_device_scale_factor(0.0f);
  display::Displays displays = {
      primary,
      display::Display(1, gfx::Rect(2000, 500, 1000, 1000))};
  screen_->LayoutDisplays(displays);
  EXPECT_EQ(displays[0].bounds().origin(), gfx::Point(0, 0));
}

TEST_F(HeadlessScreenOhosTest, GetAllDisplays) {
  auto displays = screen_->GetAllDisplays();
  EXPECT_FALSE(displays.empty());
}

TEST_F(HeadlessScreenOhosTest, GetPrimaryDisplay) {
  auto primary = screen_->GetPrimaryDisplay();
  EXPECT_NE(primary.id(), display::kInvalidDisplayId);
}

TEST_F(HeadlessScreenOhosTest, AddAndRemoveObserver) {
  EXPECT_CALL(observer_, OnDisplayAdded(_)).Times(0);
  screen_->AddObserver(&observer_);
  screen_->RemoveObserver(&observer_);
  screen_->OnDisplayCreate(1);
}

TEST_F(HeadlessScreenOhosTest, ObserverNotifiesOnDisplayCreate) {
  screen_->AddObserver(&observer_);
  screen_->OnDisplayCreate(1);
}

TEST_F(HeadlessScreenOhosTest, ObserverNotRemovedDoesNotify) {
  screen_->AddObserver(&observer_);
  screen_->OnDisplayCreate(1);
}

// --- HeadlessScreenOhos_001 (destructor coverage) ---

TEST_F(HeadlessScreenOhosTest, DestructorWithNullListener) {
  screen_.reset();
  auto headless_screen = std::make_unique<HeadlessScreenOhos>();
  headless_screen->display_listener_ = nullptr;
  headless_screen.reset();
}

}  // namespace ui
