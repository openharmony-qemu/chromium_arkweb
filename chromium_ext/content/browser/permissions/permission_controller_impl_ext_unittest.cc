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
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_result.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ARKWEB_NOTIFICATION) || BUILDFLAG(ARKWEB_CLIPBOARD)

namespace content {

namespace {

using ::testing::Return;
using PermissionStatus = blink::mojom::PermissionStatus;
using StatusCallback = base::OnceCallback<void(PermissionStatus)>;
using RequestsCallback =
    base::OnceCallback<void(const std::vector<PermissionResult>&)>;

constexpr char kTestUrl[] = "https://example.com";

// MockPermissionManager provides a concrete no-op
// RequestPermissionsFromCurrentDocument and inherits the base
// PermissionControllerDelegate defaults for the ARKWEB gated methods; subclass
// to control the ARKWEB gated delegate methods with gmock.
class MockDelegateWithAsync : public MockPermissionManager {
 public:
  MOCK_METHOD(bool,
              IsPermissionOverridable,
              (PermissionType,
               base::optional_ref<const url::Origin>,
               base::optional_ref<const url::Origin>),
              (override));
#if BUILDFLAG(ARKWEB_NOTIFICATION)
  MOCK_METHOD(void,
              GetPermissionStatusAsync,
              (PermissionType, const GURL&, StatusCallback),
              (override));
#endif  // ARKWEB_NOTIFICATION
#if BUILDFLAG(ARKWEB_CLIPBOARD)
  MOCK_METHOD(bool, IsClipboardSitePermissionEnabled, (), (override));

  // MockPermissionManager's real implementation of
  // RequestPermissionsFromCurrentDocument is a no-op that never runs the
  // callback, which would strand the merged wrapper in
  // RequestPermissionsSkipPermissionsPolicy. Override it to behave like a real
  // delegate: invoke the callback with a GRANTED result per requested
  // permission.
  void RequestPermissionsFromCurrentDocument(
      RenderFrameHost* render_frame_host,
      const PermissionRequestDescription& request_description,
      base::OnceCallback<void(const std::vector<PermissionResult>&)> callback)
      override {
    std::vector<PermissionResult> results;
    for (size_t i = 0; i < request_description.permissions.size(); ++i) {
      results.emplace_back(PermissionStatus::GRANTED);
    }
    std::move(callback).Run(results);
  }
#endif  // ARKWEB_CLIPBOARD
};

class PermissionControllerImplExtTest : public ::testing::Test {
 public:
  PermissionControllerImplExtTest() {
    browser_context_.SetPermissionControllerDelegate(
        std::make_unique<::testing::NiceMock<MockDelegateWithAsync>>());
    permission_controller_ =
        std::make_unique<PermissionControllerImpl>(&browser_context_);
    rvh_test_enabler_ = std::make_unique<RenderViewHostTestEnabler>();
  }

  ~PermissionControllerImplExtTest() override {
    browser_context_.SetPermissionControllerDelegate(nullptr);
  }

  void SetUp() override {
    ON_CALL(*mock_manager(), IsPermissionOverridable)
        .WillByDefault(Return(true));
  }

  // The ARKWEB gated methods are private overrides on PermissionControllerImpl,
  // but public virtuals on the base PermissionController interface; call
  // through the interface to exercise the virtual dispatch into the impl.
  PermissionController* permission_controller() {
    return permission_controller_.get();
  }

  PermissionControllerImpl* permission_controller_impl() {
    return permission_controller_.get();
  }

  MockDelegateWithAsync* mock_manager() {
    return static_cast<MockDelegateWithAsync*>(
        browser_context_.GetPermissionControllerDelegate());
  }

  // Creates a committed WebContents and returns its primary main frame.
  RenderFrameHost* CreateCommittedMainFrame() {
    web_contents_.reset(WebContentsTester::CreateTestWebContents(
        WebContents::CreateParams(&browser_context_)));
    WebContentsTester::For(web_contents_.get())
        ->NavigateAndCommit(GURL(kTestUrl));
    return web_contents_->GetPrimaryMainFrame();
  }

  void SetNullDelegate() {
    browser_context_.SetPermissionControllerDelegate(nullptr);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  std::unique_ptr<PermissionControllerImpl> permission_controller_;
  std::unique_ptr<RenderViewHostTestEnabler> rvh_test_enabler_;
  std::unique_ptr<WebContents> web_contents_;
};

// ---------------------------------------------------------------------------
// GetPermissionStatusAsync (ARKWEB_NOTIFICATION)
// ---------------------------------------------------------------------------

// [A] isFromDocument && !render_host -> DENIED early return.
TEST_F(PermissionControllerImplExtTest,
       GetPermissionStatusAsync_DocumentWithoutHost_Denied) {
  base::test::TestFuture<PermissionStatus> future;
  permission_controller()->GetPermissionStatusAsync(
      PermissionType::GEOLOCATION, /*isFromDocument=*/true,
      /*render_host=*/nullptr, url::Origin::Create(GURL(kTestUrl)),
      future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionStatus::DENIED);
}

// [B] Non-document request with delegate present -> delegated to
// PermissionControllerDelegate::GetPermissionStatusAsync.
TEST_F(PermissionControllerImplExtTest,
       GetPermissionStatusAsync_NonDocument_Delegates) {
  base::test::TestFuture<PermissionStatus> future;
  EXPECT_CALL(*mock_manager(), GetPermissionStatusAsync)
      .WillOnce([](PermissionType, const GURL&, StatusCallback cb) {
        std::move(cb).Run(PermissionStatus::GRANTED);
      });
  permission_controller()->GetPermissionStatusAsync(
      PermissionType::GEOLOCATION, /*isFromDocument=*/false,
      /*render_host=*/reinterpret_cast<void*>(0x1),
      url::Origin::Create(GURL(kTestUrl)), future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionStatus::GRANTED);
}

// [C] Document request with a valid render host ->
// VerifyContextOfCurrentDocument passes (returns ASK) then delegated.
TEST_F(PermissionControllerImplExtTest,
       GetPermissionStatusAsync_DocumentWithHost_VerifiesAndDelegates) {
  RenderFrameHost* rfh = CreateCommittedMainFrame();
  base::test::TestFuture<PermissionStatus> future;
  EXPECT_CALL(*mock_manager(), GetPermissionStatusAsync)
      .WillOnce([](PermissionType, const GURL&, StatusCallback cb) {
        std::move(cb).Run(PermissionStatus::ASK);
      });
  permission_controller()->GetPermissionStatusAsync(
      PermissionType::GEOLOCATION, /*isFromDocument=*/true,
      reinterpret_cast<void*>(rfh), url::Origin::Create(GURL(kTestUrl)),
      future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionStatus::ASK);
}

// [D] Override present -> permission_overrides_.Get() hit returns the override
// status directly without touching the delegate. Uses DENIED (not GRANTED):
// SetPermissionOverride(GRANTED) triggers GrantPermissionOverridesToDelegate
// which dereferences the CefBrowserInfoManager singleton (null in tests).
TEST_F(PermissionControllerImplExtTest,
       GetPermissionStatusAsync_OverridePresent_ReturnsOverride) {
  base::test::TestFuture<PermissionControllerImpl::OverrideStatus> set_future;
  url::Origin origin = url::Origin::Create(GURL(kTestUrl));
  permission_controller_impl()->SetPermissionOverride(
      origin, origin, PermissionType::GEOLOCATION, PermissionStatus::DENIED,
      set_future.GetCallback());
  ASSERT_EQ(set_future.Get(),
            PermissionControllerImpl::OverrideStatus::kOverrideSet);

  base::test::TestFuture<PermissionStatus> future;
  EXPECT_CALL(*mock_manager(), GetPermissionStatusAsync).Times(0);
  permission_controller()->GetPermissionStatusAsync(
      PermissionType::GEOLOCATION, /*isFromDocument=*/false,
      /*render_host=*/nullptr, origin, future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionStatus::DENIED);
}

// [E] Delegate null -> DENIED.
TEST_F(PermissionControllerImplExtTest,
       GetPermissionStatusAsync_NullDelegate_Denied) {
  SetNullDelegate();
  base::test::TestFuture<PermissionStatus> future;
  permission_controller()->GetPermissionStatusAsync(
      PermissionType::GEOLOCATION, /*isFromDocument=*/false,
      /*render_host=*/nullptr, url::Origin::Create(GURL(kTestUrl)),
      future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionStatus::DENIED);
}

// ---------------------------------------------------------------------------
// RequestPermissionsSkipPermissionsPolicy (ARKWEB_CLIPBOARD)
// ---------------------------------------------------------------------------

// [F] Null render frame host -> IsRequestAllowed() returns false early.
TEST_F(PermissionControllerImplExtTest,
       RequestPermissionsSkipPermissionsPolicy_NullHost_IsRequestAllowedFalse) {
  PermissionRequestDescription description(
      PermissionDescriptorUtil::CreatePermissionDescriptorForPermissionTypes(
          {PermissionType::CLIPBOARD_READ_WRITE}));
  base::MockCallback<RequestsCallback> callback;
  EXPECT_CALL(callback, Run(::testing::_));
  permission_controller()->RequestPermissionsSkipPermissionsPolicy(
      /*render_frame_host=*/nullptr, std::move(description), callback.Get());
}

// [G] Valid host with empty permission list -> IsRequestAllowed() true, then
// empty branch runs the merged results callback with an empty vector.
TEST_F(PermissionControllerImplExtTest,
       RequestPermissionsSkipPermissionsPolicy_EmptyPermissions_RunsEmpty) {
  RenderFrameHost* rfh = CreateCommittedMainFrame();
  std::vector<blink::mojom::PermissionDescriptorPtr> empty_permissions;
  PermissionRequestDescription description(std::move(empty_permissions));
  base::MockCallback<RequestsCallback> callback;
  EXPECT_CALL(callback, Run(::testing::IsEmpty()));
  permission_controller()->RequestPermissionsSkipPermissionsPolicy(
      rfh, std::move(description), callback.Get());
}

// [H] Valid host, non-empty permissions, null delegate -> DENIED vector.
TEST_F(PermissionControllerImplExtTest,
       RequestPermissionsSkipPermissionsPolicy_NullDelegate_DeniedVector) {
  RenderFrameHost* rfh = CreateCommittedMainFrame();
  SetNullDelegate();
  PermissionRequestDescription description(
      PermissionDescriptorUtil::CreatePermissionDescriptorForPermissionTypes(
          {PermissionType::CLIPBOARD_READ_WRITE}));
  base::MockCallback<RequestsCallback> callback;
  EXPECT_CALL(callback,
              Run(::testing::ElementsAre(::testing::Field(
                  &PermissionResult::status, PermissionStatus::DENIED))));
  permission_controller()->RequestPermissionsSkipPermissionsPolicy(
      rfh, std::move(description), callback.Get());
}

// [I] Valid host, non-empty permissions, delegate present -> delegated to
// RequestPermissionsFromCurrentDocument.
TEST_F(PermissionControllerImplExtTest,
       RequestPermissionsSkipPermissionsPolicy_Delegates) {
  RenderFrameHost* rfh = CreateCommittedMainFrame();
  base::MockCallback<RequestsCallback> callback;
  EXPECT_CALL(callback, Run(::testing::_));
  PermissionRequestDescription description(
      PermissionDescriptorUtil::CreatePermissionDescriptorForPermissionTypes(
          {PermissionType::CLIPBOARD_READ_WRITE}));
  permission_controller()->RequestPermissionsSkipPermissionsPolicy(
      rfh, std::move(description), callback.Get());
}

// ---------------------------------------------------------------------------
// IsClipboardSitePermissionEnabled (ARKWEB_CLIPBOARD)
// ---------------------------------------------------------------------------

// [J] Null browser context -> false.
TEST_F(PermissionControllerImplExtTest,
       IsClipboardSitePermissionEnabled_NullBrowserContext_False) {
  auto controller = std::make_unique<PermissionControllerImpl>(nullptr);
  // Call through the base interface: IsClipboardSitePermissionEnabled is a
  // private override on PermissionControllerImpl but a public virtual on
  // PermissionController.
  PermissionController* base = controller.get();
  EXPECT_FALSE(base->IsClipboardSitePermissionEnabled());
}

// [K] Browser context with null delegate -> false.
TEST_F(PermissionControllerImplExtTest,
       IsClipboardSitePermissionEnabled_NullDelegate_False) {
  SetNullDelegate();
  EXPECT_FALSE(permission_controller()->IsClipboardSitePermissionEnabled());
}

// [L] Delegate present -> delegates to the delegate's implementation.
TEST_F(PermissionControllerImplExtTest,
       IsClipboardSitePermissionEnabled_Delegates) {
  EXPECT_CALL(*mock_manager(), IsClipboardSitePermissionEnabled)
      .WillOnce(Return(true));
  EXPECT_TRUE(permission_controller()->IsClipboardSitePermissionEnabled());

  EXPECT_CALL(*mock_manager(), IsClipboardSitePermissionEnabled)
      .WillOnce(Return(false));
  EXPECT_FALSE(permission_controller()->IsClipboardSitePermissionEnabled());
}

}  // namespace

}  // namespace content

#endif  // BUILDFLAG(ARKWEB_NOTIFICATION) || BUILDFLAG(ARKWEB_CLIPBOARD)
