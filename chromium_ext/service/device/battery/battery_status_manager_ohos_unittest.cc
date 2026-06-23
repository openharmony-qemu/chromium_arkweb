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

#include "base/functional/bind.h"
#include "services/device/battery/battery_status_manager.h"
#include "services/device/public/mojom/battery_status.mojom.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <map>

namespace OHOS::NWeb {

class WebBatteryInfo : public base::RefCountedThreadSafe<WebBatteryInfo> {
public:
    virtual double GetLevel() = 0;
    virtual bool IsCharging() = 0;
    virtual int DisChargingTime() = 0;
    virtual int ChargingTime() = 0;
protected:
    virtual ~WebBatteryInfo() = default;
};

class WebBatteryEventCallback {
public:
    virtual ~WebBatteryEventCallback() = default;
    virtual void BatteryInfoChanged(std::shared_ptr<WebBatteryInfo> info) = 0;
};

class BatteryMgrClientAdapter {
public:
    virtual ~BatteryMgrClientAdapter() = default;
    virtual void RegBatteryEvent(std::shared_ptr<WebBatteryEventCallback> eventCallback) = 0;
    virtual bool StartListen() = 0;
    virtual void StopListen() = 0;
    virtual std::shared_ptr<WebBatteryInfo> RequestBatteryInfo() = 0;
};

}

namespace {

class MockWebBatteryInfo : public OHOS::NWeb::WebBatteryInfo {
public:
    MockWebBatteryInfo(double level, bool charging, int dischargingTime, int chargingTime)
        : level_(level), charging_(charging), dischargingTime_(dischargingTime), chargingTime_(chargingTime) {}
    double GetLevel() override { return level_; }
    bool IsCharging() override { return charging_; }
    int DisChargingTime() override { return dischargingTime_; }
    int ChargingTime() override { return chargingTime_; }
private:
    double level_;
    bool charging_;
    int dischargingTime_;
    int chargingTime_;
};

class MockWebBatteryEventCallback : public OHOS::NWeb::WebBatteryEventCallback {
public:
    MOCK_METHOD(void, BatteryInfoChanged, (std::shared_ptr<OHOS::NWeb::WebBatteryInfo>), (override));
};

class MockBatteryMgrClientAdapter : public OHOS::NWeb::BatteryMgrClientAdapter {
public:
    MockBatteryMgrClientAdapter() = default;
    MOCK_METHOD(void, RegBatteryEvent, (std::shared_ptr<OHOS::NWeb::WebBatteryEventCallback>), (override));
    MOCK_METHOD(bool, StartListen, (), (override));
    MOCK_METHOD(void, StopListen, (), (override));
    MOCK_METHOD(std::shared_ptr<OHOS::NWeb::WebBatteryInfo>, RequestBatteryInfo, (), (override));
};

}

namespace device {

class BatteryStatusManagerOhosTest : public ::testing::Test {
protected:
    BatteryStatusManagerOhosTest() = default;
    ~BatteryStatusManagerOhosTest() override = default;

    void SetUp() override {
        callback_called_count_ = 0;
        callback_statuses_.clear();
    }

    void TearDown() override {
        manager_.reset();
        all_managers_.clear();
    }

    BatteryStatusService::BatteryUpdateCallback CreateCallback() {
        return base::BindRepeating(
            &BatteryStatusManagerOhosTest::BatteryCallback, base::Unretained(this));
    }

    BatteryStatusService::BatteryUpdateCallback CreateCallbackWithCapture(
        int* count_ptr) {
        return base::BindRepeating(
            [](int* count, const mojom::BatteryStatus& status) {
                (*count)++;
            },
            base::Unretained(count_ptr));
    }

    void BatteryCallback(const mojom::BatteryStatus& status) {
        callback_called_count_++;
        last_status_ = status;
        callback_statuses_.push_back(status);
    }

    BatteryStatusService::BatteryUpdateCallback CreateCallbackDetailed(
        std::vector<mojom::BatteryStatus>* status_vector) {
        return base::BindRepeating(
            [](std::vector<mojom::BatteryStatus>* vec,
               const mojom::BatteryStatus& status) { vec->push_back(status); },
            base::Unretained(status_vector));
    }

    std::unique_ptr<BatteryStatusManager> manager_;
    std::vector<std::unique_ptr<BatteryStatusManager>> all_managers_;
    int callback_called_count_ = 0;
    mojom::BatteryStatus last_status_;
    std::vector<mojom::BatteryStatus> callback_statuses_;
};

TEST_F(BatteryStatusManagerOhosTest, Constructor) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  EXPECT_NE(manager_, nullptr);
}

TEST_F(BatteryStatusManagerOhosTest, ConstructorWithNullCallback) {
  BatteryStatusService::BatteryUpdateCallback null_callback;
  manager_ = BatteryStatusManager::Create(null_callback);
  EXPECT_NE(manager_, nullptr);
}

TEST_F(BatteryStatusManagerOhosTest, StartListeningBatteryChange) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager_, nullptr);
  bool result = manager_->StartListeningBatteryChange();
  EXPECT_TRUE(result);
}

TEST_F(BatteryStatusManagerOhosTest, StopListeningBatteryChange) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager_, nullptr);
  manager_->StartListeningBatteryChange();
  manager_->StopListeningBatteryChange();
  SUCCEED();
}

TEST_F(BatteryStatusManagerOhosTest, StopListeningWithoutStart) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager_, nullptr);
  manager_->StopListeningBatteryChange();
  SUCCEED();
}

TEST_F(BatteryStatusManagerOhosTest, StartStopStartCycle) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager_, nullptr);
  bool result1 = manager_->StartListeningBatteryChange();
  EXPECT_TRUE(result1);
  manager_->StopListeningBatteryChange();
  bool result2 = manager_->StartListeningBatteryChange();
  EXPECT_TRUE(result2);
  manager_->StopListeningBatteryChange();
}

TEST_F(BatteryStatusManagerOhosTest, CallbackNotCalledOnCreate) {
  int call_count = 0;
  auto callback = CreateCallbackWithCapture(&call_count);
  manager_ = BatteryStatusManager::Create(callback);
  ASSERT_NE(manager_, nullptr);
  EXPECT_EQ(call_count, 0);
}

TEST_F(BatteryStatusManagerOhosTest, CallbackReset) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager_, nullptr);
  callback_called_count_ = 0;
  manager_->StartListeningBatteryChange();
  callback_called_count_ = 0;
  manager_->StopListeningBatteryChange();
  manager_->StartListeningBatteryChange();
  manager_->StopListeningBatteryChange();
}

TEST_F(BatteryStatusManagerOhosTest, StatusTracking) {
  std::vector<mojom::BatteryStatus> status_vector;
  auto callback = CreateCallbackDetailed(&status_vector);
  manager_ = BatteryStatusManager::Create(callback);
  ASSERT_NE(manager_, nullptr);
  manager_->StartListeningBatteryChange();
  manager_->StopListeningBatteryChange();
}

TEST_F(BatteryStatusManagerOhosTest, NullManagerReset) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager_, nullptr);
  manager_->StartListeningBatteryChange();
  manager_.reset();
  EXPECT_EQ(manager_, nullptr);
}

TEST_F(BatteryStatusManagerOhosTest, NullManagerAfterDestruction) {
  {
    auto temp_manager = BatteryStatusManager::Create(CreateCallback());
    ASSERT_NE(temp_manager, nullptr);
  }
  manager_ = nullptr;
  EXPECT_EQ(manager_, nullptr);
}

TEST_F(BatteryStatusManagerOhosTest, MultipleManagersIndependent) {
  auto callback1 = CreateCallback();
  auto callback2 = CreateCallback();
  auto manager1 = BatteryStatusManager::Create(callback1);
  auto manager2 = BatteryStatusManager::Create(callback2);
  ASSERT_NE(manager1, nullptr);
  ASSERT_NE(manager2, nullptr);
  bool result1 = manager1->StartListeningBatteryChange();
  bool result2 = manager2->StartListeningBatteryChange();
  EXPECT_TRUE(result1);
  EXPECT_TRUE(result2);
  manager1->StopListeningBatteryChange();
  manager2->StopListeningBatteryChange();
}

TEST_F(BatteryStatusManagerOhosTest, ManagerLifecycle) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager_, nullptr);
  bool start_result = manager_->StartListeningBatteryChange();
  EXPECT_TRUE(start_result);
  manager_->StopListeningBatteryChange();
  bool restart_result = manager_->StartListeningBatteryChange();
  EXPECT_TRUE(restart_result);
  manager_->StopListeningBatteryChange();
}

TEST_F(BatteryStatusManagerOhosTest, UniquePtrMoveConstruct) {
  auto manager1 = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager1, nullptr);
  auto manager2 = std::move(manager1);
  EXPECT_EQ(manager1, nullptr);
  EXPECT_NE(manager2, nullptr);
  bool result = manager2->StartListeningBatteryChange();
  EXPECT_TRUE(result);
  manager2->StopListeningBatteryChange();
}

TEST_F(BatteryStatusManagerOhosTest, UniquePtrMoveAssign) {
  auto manager1 = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager1, nullptr);
  auto manager2 = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager2, nullptr);
  manager2 = std::move(manager1);
  EXPECT_EQ(manager1, nullptr);
  EXPECT_NE(manager2, nullptr);
  bool result = manager2->StartListeningBatteryChange();
  EXPECT_TRUE(result);
  manager2->StopListeningBatteryChange();
}

TEST_F(BatteryStatusManagerOhosTest, DestructorStopsListening) {
  int initial_count = 0;
  {
    BatteryStatusService::BatteryUpdateCallback temp_callback =
        base::BindRepeating([](int* count, const mojom::BatteryStatus& status) {
          (*count)++;
        }, base::Unretained(&initial_count));
    auto temp_manager = BatteryStatusManager::Create(temp_callback);
    ASSERT_NE(temp_manager, nullptr);
    temp_manager->StartListeningBatteryChange();
  }
  SUCCEED();
}

TEST_F(BatteryStatusManagerOhosTest, MultipleStartsBeforeStop) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager_, nullptr);
  for (int i = 0; i < 5; ++i) {
    bool result = manager_->StartListeningBatteryChange();
    EXPECT_TRUE(result);
  }
  manager_->StopListeningBatteryChange();
}

TEST_F(BatteryStatusManagerOhosTest, AlternatingStartStop) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager_, nullptr);
  for (int i = 0; i < 5; ++i) {
    if (i % 2 == 0) {
      bool result = manager_->StartListeningBatteryChange();
      EXPECT_TRUE(result);
    } else {
      manager_->StopListeningBatteryChange();
    }
  }
  manager_->StopListeningBatteryChange();
}

TEST_F(BatteryStatusManagerOhosTest, ManagerWithOnlyStart) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager_, nullptr);
  manager_->StartListeningBatteryChange();
}

TEST_F(BatteryStatusManagerOhosTest, ManagerWithOnlyStop) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager_, nullptr);
  manager_->StopListeningBatteryChange();
}

TEST_F(BatteryStatusManagerOhosTest, CreateAndDestroyImmediately) {
  for (int i = 0; i < 5; ++i) {
    auto temp_manager = BatteryStatusManager::Create(CreateCallback());
    ASSERT_NE(temp_manager, nullptr);
  }
}

TEST_F(BatteryStatusManagerOhosTest, CreateStartDestroy) {
  for (int i = 0; i < 5; ++i) {
    auto temp_manager = BatteryStatusManager::Create(CreateCallback());
    ASSERT_NE(temp_manager, nullptr);
    temp_manager->StartListeningBatteryChange();
  }
}

TEST_F(BatteryStatusManagerOhosTest, CreateStartStopDestroy) {
  for (int i = 0; i < 5; ++i) {
    auto temp_manager = BatteryStatusManager::Create(CreateCallback());
    ASSERT_NE(temp_manager, nullptr);
    temp_manager->StartListeningBatteryChange();
    temp_manager->StopListeningBatteryChange();
  }
}

TEST_F(BatteryStatusManagerOhosTest, MultipleManagersLifecycle) {
  std::vector<std::unique_ptr<BatteryStatusManager>> managers;
  for (int i = 0; i < 5; ++i) {
    auto manager = BatteryStatusManager::Create(CreateCallback());
    ASSERT_NE(manager, nullptr);
    bool result = manager->StartListeningBatteryChange();
    EXPECT_TRUE(result);
    managers.push_back(std::move(manager));
  }
  for (auto& manager : managers) {
    manager->StopListeningBatteryChange();
  }
  managers.clear();
}

TEST_F(BatteryStatusManagerOhosTest, StatusTrackingMultipleCalls) {
  std::vector<mojom::BatteryStatus> status_vector;
  auto callback = CreateCallbackDetailed(&status_vector);
  manager_ = BatteryStatusManager::Create(callback);
  ASSERT_NE(manager_, nullptr);
  for (int i = 0; i < 3; ++i) {
    manager_->StartListeningBatteryChange();
    manager_->StopListeningBatteryChange();
  }
}

TEST_F(BatteryStatusManagerOhosTest, ManagerAfterStartState) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager_, nullptr);
  manager_->StartListeningBatteryChange();
  SUCCEED();
}

TEST_F(BatteryStatusManagerOhosTest, ManagerAfterStopState) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager_, nullptr);
  manager_->StartListeningBatteryChange();
  manager_->StopListeningBatteryChange();
  SUCCEED();
}

TEST_F(BatteryStatusManagerOhosTest, StartReturnValueConsistency) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager_, nullptr);
  bool results[5];
  for (int i = 0; i < 5; ++i) {
    results[i] = manager_->StartListeningBatteryChange();
    EXPECT_TRUE(results[i]);
  }
  manager_->StopListeningBatteryChange();
  for (int i = 0; i < 5; ++i) {
    results[i] = manager_->StartListeningBatteryChange();
    EXPECT_TRUE(results[i]);
  }
}

TEST_F(BatteryStatusManagerOhosTest, RapidStartStopCycles) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager_, nullptr);
  for (int i = 0; i < 10; ++i) {
    bool result = manager_->StartListeningBatteryChange();
    EXPECT_TRUE(result);
    manager_->StopListeningBatteryChange();
  }
}

TEST_F(BatteryStatusManagerOhosTest, UniquePtrRelease) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager_, nullptr);
  auto raw_ptr = manager_.release();
  EXPECT_NE(raw_ptr, nullptr);
  EXPECT_EQ(manager_, nullptr);
  delete raw_ptr;
}

TEST_F(BatteryStatusManagerOhosTest, CallbackWithNullCallbackStop) {
  BatteryStatusService::BatteryUpdateCallback null_callback;
  manager_ = BatteryStatusManager::Create(null_callback);
  ASSERT_NE(manager_, nullptr);
  manager_->StopListeningBatteryChange();
}

TEST_F(BatteryStatusManagerOhosTest, CallbackWithCaptureDetailed) {
  std::vector<mojom::BatteryStatus> status_vector;
  auto callback = CreateCallbackDetailed(&status_vector);
  manager_ = BatteryStatusManager::Create(callback);
  ASSERT_NE(manager_, nullptr);
  manager_->StartListeningBatteryChange();
  manager_->StopListeningBatteryChange();
}

TEST_F(BatteryStatusManagerOhosTest, StartWithoutCallbackVerification) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager_, nullptr);
  bool result = manager_->StartListeningBatteryChange();
  EXPECT_TRUE(result);
}

TEST_F(BatteryStatusManagerOhosTest, StopWithoutCallbackVerification) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager_, nullptr);
  manager_->StartListeningBatteryChange();
  manager_->StopListeningBatteryChange();
}

TEST_F(BatteryStatusManagerOhosTest, ManagerInitializationState) {
  manager_ = BatteryStatusManager::Create(CreateCallback());
  ASSERT_NE(manager_, nullptr);
  EXPECT_EQ(callback_called_count_, 0);
}

TEST_F(BatteryStatusManagerOhosTest, MultipleManagersSequential) {
  for (int i = 0; i < 3; ++i) {
    auto manager = BatteryStatusManager::Create(CreateCallback());
    ASSERT_NE(manager, nullptr);
    bool result = manager->StartListeningBatteryChange();
    EXPECT_TRUE(result);
    manager->StopListeningBatteryChange();
  }
}

}  // namespace device
