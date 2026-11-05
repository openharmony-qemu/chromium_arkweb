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

#include <memory>
#include <utility>
#include <vector>

#include "arkweb/build/features/features.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "cef/libcef/browser/browser_info_manager.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_result.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ARKWEB_PERMISSION)

namespace content {

namespace {

using ::testing::Return;
using PermissionStatus = blink::mojom::PermissionStatus;
using OverrideStatus = PermissionControllerImpl::OverrideStatus;

constexpr char kTestUrl[] = "https://example.com";

// Grants are dispatched to the CefBrowserInfoManager singleton; constructing it
// makes GetInstance() non-null with an empty browser list so the focused/main
// frame lookups return null and the grant delegate early-returns.
class CefManagerScoped {
 public:
  CefManagerScoped() { manager_ = std::make_unique<CefBrowserInfoManager>(); }
  ~CefManagerScoped() { manager_.reset(); }

 private:
  std::unique_ptr<CefBrowserInfoManager> manager_;
};

class GrantDelegateTest : public ::testing::Test {
 public:
  GrantDelegateTest() {
    browser_context_.SetPermissionControllerDelegate(
        std::make_unique<::testing::NiceMock<MockPermissionManager>>());
    permission_controller_ =
        std::make_unique<PermissionControllerImpl>(&browser_context_);
  }

  ~GrantDelegateTest() override {
    browser_context_.SetPermissionControllerDelegate(nullptr);
  }

  PermissionControllerImpl* permission_controller_impl() {
    return permission_controller_.get();
  }

  MockPermissionManager* mock_manager() {
    return static_cast<MockPermissionManager*>(
        browser_context_.GetPermissionControllerDelegate());
  }

  void SetNullDelegate() {
    browser_context_.SetPermissionControllerDelegate(nullptr);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  std::unique_ptr<PermissionControllerImpl> permission_controller_;
};

// [1] Null delegate -> early return with kOverrideSet (no CEF access).
TEST_F(GrantDelegateTest, NullDelegate_EarlyReturn) {
  SetNullDelegate();
  base::test::TestFuture<OverrideStatus> future;
  permission_controller_impl()->SetPermissionOverride(
      url::Origin::Create(GURL(kTestUrl)), url::Origin(),
      PermissionType::GEOLOCATION, PermissionStatus::GRANTED,
      future.GetCallback());
  EXPECT_EQ(future.Get(), OverrideStatus::kOverrideSet);
}

// [2] Empty permissions vector -> early return with kOverrideSet.
TEST_F(GrantDelegateTest, EmptyPermissions_EarlyReturn) {
  base::test::TestFuture<OverrideStatus> future;
  url::Origin origin = url::Origin::Create(GURL(kTestUrl));
  permission_controller_impl()->GrantPermissionOverrides(
      origin, origin, std::vector<PermissionType>{}, future.GetCallback());
  EXPECT_EQ(future.Get(), OverrideStatus::kOverrideSet);
}

// [3] All unsupported permission types -> supported.empty() early return.
TEST_F(GrantDelegateTest, UnsupportedPermissions_EarlyReturn) {
  base::test::TestFuture<OverrideStatus> future;
  url::Origin origin = url::Origin::Create(GURL(kTestUrl));
  permission_controller_impl()->GrantPermissionOverrides(
      origin, origin,
      std::vector<PermissionType>{PermissionType::MIDI,
                                  PermissionType::DURABLE_STORAGE,
                                  PermissionType::SENSORS},
      future.GetCallback());
  EXPECT_EQ(future.Get(), OverrideStatus::kOverrideSet);
}

// [4] Supported permission with requesting origin, CEF manager empty -> no
// focused main frame -> early return with kOverrideSet. Exercises the dedup +
// supported filter + requesting_origin.has_value() + !rfh branches.
TEST_F(GrantDelegateTest, RequestingOrigin_NoFocusedFrame_EarlyReturn) {
  CefManagerScoped cef_manager;
  base::test::TestFuture<OverrideStatus> future;
  url::Origin origin = url::Origin::Create(GURL(kTestUrl));
  // Duplicate GEOLOCATION to exercise the dedup branch.
  permission_controller_impl()->GrantPermissionOverrides(
      origin, origin,
      std::vector<PermissionType>{PermissionType::GEOLOCATION,
                                  PermissionType::GEOLOCATION,
                                  PermissionType::NOTIFICATIONS},
      future.GetCallback());
  EXPECT_EQ(future.Get(), OverrideStatus::kOverrideSet);
}

// [5] Supported permission without requesting origin, CEF manager empty ->
// CollectMainFrameByOrigin returns empty -> early return with kOverrideSet.
TEST_F(GrantDelegateTest, NoRequestingOrigin_NoMainFrames_EarlyReturn) {
  CefManagerScoped cef_manager;
  base::test::TestFuture<OverrideStatus> future;
  permission_controller_impl()->GrantPermissionOverrides(
      std::nullopt, std::nullopt,
      std::vector<PermissionType>{PermissionType::AUDIO_CAPTURE},
      future.GetCallback());
  EXPECT_EQ(future.Get(), OverrideStatus::kOverrideSet);
}

// [6] SetPermissionOverride(GRANTED) with a supported type and CEF manager
// present -> delegates through GrantPermissionOverridesToDelegate with a
// single-element permissions vector -> early return (no focused frame).
TEST_F(GrantDelegateTest, SetPermissionOverrideGranted_NoFocusedFrame) {
  CefManagerScoped cef_manager;
  base::test::TestFuture<OverrideStatus> future;
  permission_controller_impl()->SetPermissionOverride(
      url::Origin::Create(GURL(kTestUrl)), url::Origin(),
      PermissionType::VIDEO_CAPTURE, PermissionStatus::GRANTED,
      future.GetCallback());
  EXPECT_EQ(future.Get(), OverrideStatus::kOverrideSet);
}

}  // namespace

}  // namespace content

#endif  // BUILDFLAG(ARKWEB_PERMISSION)
