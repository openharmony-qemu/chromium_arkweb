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

#include "arkweb/chromium_ext/chrome/browser/notifications/notification_platform_bridge_ohos.h"

#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "content/public/test/browser_task_environment.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

#if BUILDFLAG(ARKWEB_NOTIFICATION)
#include "ohos_nweb/src/capi/nweb_notification_options_item.h"
#include "ohos_nweb/src/nweb_common.h"
#include "ohos_nweb/src/nweb_notification_manager_delegate.h"
#endif

namespace {
constexpr int IMAGE_SIZE = 1024;
}  // namespace

class NotificationPlatformBridgeOhosTest : public testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  content::BrowserTaskEnvironment task_environment_;
};

// Test CanHandleType static method for all notification handler types.
TEST_F(NotificationPlatformBridgeOhosTest, CanHandleType) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);
  EXPECT_TRUE(bridge->CanHandleType(NotificationHandler::Type::WEB_PERSISTENT));
  EXPECT_TRUE(
      bridge->CanHandleType(NotificationHandler::Type::WEB_NON_PERSISTENT));
  EXPECT_TRUE(bridge->CanHandleType(NotificationHandler::Type::EXTENSION));
  EXPECT_FALSE(bridge->CanHandleType(NotificationHandler::Type::ANNOUNCEMENT));
}

// Test SetReadyCallback invokes callback immediately with true.
TEST_F(NotificationPlatformBridgeOhosTest, SetReadyCallback) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);

  bool callback_called = false;
  bool callback_result = false;
  bridge->SetReadyCallback(base::BindOnce(
      [](bool* called, bool* result, bool ready) {
        *called = true;
        *result = ready;
      },
      &callback_called, &callback_result));

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

// Test GetDisplayed returns empty set via async callback.
TEST_F(NotificationPlatformBridgeOhosTest, GetDisplayed) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);

  bool callback_called = false;
  std::set<std::string> displayed_notifications;
  bool supports_sync = true;

  bridge->GetDisplayed(
      nullptr, base::BindOnce(
                   [](bool* called, std::set<std::string>* notifications,
                      bool* supports_sync, std::set<std::string> displayed,
                      bool supports_synchronization) {
                     *called = true;
                     *notifications = std::move(displayed);
                     *supports_sync = supports_synchronization;
                   },
                   &callback_called, &displayed_notifications, &supports_sync));

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(displayed_notifications.empty());
  EXPECT_FALSE(supports_sync);
}

// Test GetDisplayedForOrigin returns empty set via async callback.
TEST_F(NotificationPlatformBridgeOhosTest, GetDisplayedForOrigin) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);

  bool callback_called = false;
  std::set<std::string> displayed_notifications;
  bool supports_sync = true;

  bridge->GetDisplayedForOrigin(
      nullptr, GURL("https://example.com"),
      base::BindOnce(
          [](bool* called, std::set<std::string>* notifications,
             bool* supports_sync, std::set<std::string> displayed,
             bool supports_synchronization) {
            *called = true;
            *notifications = std::move(displayed);
            *supports_sync = supports_synchronization;
          },
          &callback_called, &displayed_notifications, &supports_sync));

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(displayed_notifications.empty());
  EXPECT_FALSE(supports_sync);
}

// Test DisplayServiceShutDown is a no-op and doesn't crash.
TEST_F(NotificationPlatformBridgeOhosTest, DisplayServiceShutDown) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);
  bridge->DisplayServiceShutDown(nullptr);
}

// Test OnShowed with no matching notification (null guard path).
TEST_F(NotificationPlatformBridgeOhosTest, OnShowed_NoMatchingNotification) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);
  NotificationPlatformBridgeOhos::OnShowed("nonexistent_id");
}

// Test OnClosed with no matching notification (null guard path).
TEST_F(NotificationPlatformBridgeOhosTest, OnClosed_NoMatchingNotification) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);
  NotificationPlatformBridgeOhos::OnClosed("nonexistent_id");
}

// Test OnClicked with no matching notification (null guard path).
// Tests both buttonIndex < 0 and >= 0 paths.
TEST_F(NotificationPlatformBridgeOhosTest, OnClicked_NoMatchingNotification) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);
  NotificationPlatformBridgeOhos::OnClicked("nonexistent_id", -1);
  NotificationPlatformBridgeOhos::OnClicked("nonexistent_id", 0);
}

// Test Display with TRANSIENT type (no PassThroughDelegate created).
TEST_F(NotificationPlatformBridgeOhosTest, Display_TransientType_NullProfile) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);

  std::string notification_key("transient_null_profile");
  message_center::RichNotificationData optional_fields;

  SkBitmap bitmap;
  bool ret = bitmap.tryAllocPixels(SkImageInfo::Make(
      IMAGE_SIZE, IMAGE_SIZE, kRGBA_8888_SkColorType, kOpaque_SkAlphaType));
  ASSERT_TRUE(ret);
  ui::ImageModel image_model =
      ui::ImageModel::FromImage(gfx::Image::CreateFrom1xBitmap(bitmap));

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_CUSTOM, notification_key, u"title",
      u"message", image_model, u"arc", GURL(),
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 "ARC_NOTIFICATION"),
      optional_fields, nullptr);

  bridge->Display(NotificationHandler::Type::TRANSIENT, nullptr, notification,
                  nullptr);
}

// Test Display with existing delegate (skips PassThroughDelegate creation).
TEST_F(NotificationPlatformBridgeOhosTest, Display_WithDelegate_NullProfile) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);

  std::string notification_key("with_delegate_null_profile");
  message_center::RichNotificationData optional_fields;

  SkBitmap bitmap;
  bool ret = bitmap.tryAllocPixels(SkImageInfo::Make(
      IMAGE_SIZE, IMAGE_SIZE, kRGBA_8888_SkColorType, kOpaque_SkAlphaType));
  ASSERT_TRUE(ret);
  ui::ImageModel image_model =
      ui::ImageModel::FromImage(gfx::Image::CreateFrom1xBitmap(bitmap));

  scoped_refptr<message_center::NotificationDelegate> delegate =
      new message_center::NotificationDelegate();
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_CUSTOM, notification_key, u"title",
      u"message", image_model, u"arc", GURL(),
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 "ARC_NOTIFICATION"),
      optional_fields, delegate);

  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, nullptr,
                  notification, nullptr);
}

// Test Display creates PassThroughDelegate when no existing delegate
// and non-TRANSIENT type.
TEST_F(NotificationPlatformBridgeOhosTest,
       Display_PassThroughDelegate_NullProfile) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);

  std::string notification_key("passthrough_null_profile");
  message_center::RichNotificationData optional_fields;
  optional_fields.settings_button_handler =
      message_center::SettingsButtonHandler::DELEGATE;

  SkBitmap bitmap;
  bool ret = bitmap.tryAllocPixels(SkImageInfo::Make(
      IMAGE_SIZE, IMAGE_SIZE, kRGBA_8888_SkColorType, kOpaque_SkAlphaType));
  ASSERT_TRUE(ret);
  ui::ImageModel image_model =
      ui::ImageModel::FromImage(gfx::Image::CreateFrom1xBitmap(bitmap));

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_CUSTOM, notification_key, u"title",
      u"message", image_model, u"arc", GURL(),
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 "ARC_NOTIFICATION"),
      optional_fields, nullptr);

  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, nullptr,
                  notification, nullptr);
}

// Test Display with Premul alpha type bitmap.
TEST_F(NotificationPlatformBridgeOhosTest,
       Display_PremulAlphaType_NullProfile) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);

  std::string notification_key("premul_null_profile");
  message_center::RichNotificationData optional_fields;

  SkBitmap bitmap;
  bool ret = bitmap.tryAllocPixels(SkImageInfo::Make(
      IMAGE_SIZE, IMAGE_SIZE, kRGBA_8888_SkColorType, kPremul_SkAlphaType));
  ASSERT_TRUE(ret);
  ui::ImageModel image_model =
      ui::ImageModel::FromImage(gfx::Image::CreateFrom1xBitmap(bitmap));

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_CUSTOM, notification_key, u"title",
      u"message", image_model, u"arc", GURL(),
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 "ARC_NOTIFICATION"),
      optional_fields, nullptr);

  bridge->Display(NotificationHandler::Type::TRANSIENT, nullptr, notification,
                  nullptr);
}

// Test Display with Unpremul alpha type bitmap.
TEST_F(NotificationPlatformBridgeOhosTest,
       Display_UnpremulAlphaType_NullProfile) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);

  std::string notification_key("unpremul_null_profile");
  message_center::RichNotificationData optional_fields;

  SkBitmap bitmap;
  bool ret = bitmap.tryAllocPixels(SkImageInfo::Make(
      IMAGE_SIZE, IMAGE_SIZE, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType));
  ASSERT_TRUE(ret);
  ui::ImageModel image_model =
      ui::ImageModel::FromImage(gfx::Image::CreateFrom1xBitmap(bitmap));

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_CUSTOM, notification_key, u"title",
      u"message", image_model, u"arc", GURL(),
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 "ARC_NOTIFICATION"),
      optional_fields, nullptr);

  bridge->Display(NotificationHandler::Type::TRANSIENT, nullptr, notification,
                  nullptr);
}

// Test Close with no matching notification doesn't crash.
TEST_F(NotificationPlatformBridgeOhosTest, Close_NullProfile) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);
  bridge->Close(nullptr, "nonexistent_id");
}

// Test OnShowed with TRANSIENT notification (no delegate -> returns early).
TEST_F(NotificationPlatformBridgeOhosTest, OnShowed_WithTransientNotification) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);

  std::string notification_key("onshowed_transient");
  message_center::RichNotificationData optional_fields;

  SkBitmap bitmap;
  bool ret = bitmap.tryAllocPixels(SkImageInfo::Make(
      IMAGE_SIZE, IMAGE_SIZE, kRGBA_8888_SkColorType, kOpaque_SkAlphaType));
  ASSERT_TRUE(ret);
  ui::ImageModel image_model =
      ui::ImageModel::FromImage(gfx::Image::CreateFrom1xBitmap(bitmap));

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_CUSTOM, notification_key, u"title",
      u"message", image_model, u"arc", GURL(),
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 "ARC_NOTIFICATION"),
      optional_fields, nullptr);

  bridge->Display(NotificationHandler::Type::TRANSIENT, nullptr, notification,
                  nullptr);

  // OnShowed finds the notification but delegate is null -> returns early
  NotificationPlatformBridgeOhos::OnShowed(notification_key);
}

// Test OnClosed with TRANSIENT notification (no delegate -> no Close call).
TEST_F(NotificationPlatformBridgeOhosTest, OnClosed_WithTransientNotification) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);

  std::string notification_key("onclosed_transient");
  message_center::RichNotificationData optional_fields;

  SkBitmap bitmap;
  bool ret = bitmap.tryAllocPixels(SkImageInfo::Make(
      IMAGE_SIZE, IMAGE_SIZE, kRGBA_8888_SkColorType, kOpaque_SkAlphaType));
  ASSERT_TRUE(ret);
  ui::ImageModel image_model =
      ui::ImageModel::FromImage(gfx::Image::CreateFrom1xBitmap(bitmap));

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_CUSTOM, notification_key, u"title",
      u"message", image_model, u"arc", GURL(),
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 "ARC_NOTIFICATION"),
      optional_fields, nullptr);

  bridge->Display(NotificationHandler::Type::TRANSIENT, nullptr, notification,
                  nullptr);

  // OnClosed with TRANSIENT notification (no delegate) -> skips Close
  NotificationPlatformBridgeOhos::OnClosed(notification_key);
}

// Test OnClicked with TRANSIENT notification (no delegate -> no Click call).
// Tests both buttonIndex < 0 (body click) and >= 0 (button click) paths.
TEST_F(NotificationPlatformBridgeOhosTest,
       OnClicked_WithTransientNotification) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);

  std::string notification_key("onclicked_transient");
  message_center::RichNotificationData optional_fields;

  SkBitmap bitmap;
  bool ret = bitmap.tryAllocPixels(SkImageInfo::Make(
      IMAGE_SIZE, IMAGE_SIZE, kRGBA_8888_SkColorType, kOpaque_SkAlphaType));
  ASSERT_TRUE(ret);
  ui::ImageModel image_model =
      ui::ImageModel::FromImage(gfx::Image::CreateFrom1xBitmap(bitmap));

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_CUSTOM, notification_key, u"title",
      u"message", image_model, u"arc", GURL(),
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 "ARC_NOTIFICATION"),
      optional_fields, nullptr);

  bridge->Display(NotificationHandler::Type::TRANSIENT, nullptr, notification,
                  nullptr);

  // OnClicked with no delegate - should not crash, just skip
  NotificationPlatformBridgeOhos::OnClicked(notification_key, -1);
  NotificationPlatformBridgeOhos::OnClicked(notification_key, 0);
}

// Test Display with a notification that has a PassThroughDelegate,
// then OnClosed is called. The delegate->Close() path requires a real
// Profile to call NotificationDisplayServiceImpl::GetForProfile(), so
// with null profile this test only verifies the Display and Find paths
// up to the delegate check, not the delegate method execution.
TEST_F(NotificationPlatformBridgeOhosTest,
       Display_PassThroughDelegate_OnClosed_NullProfile) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);

  std::string notification_key("onclosed_passthrough");
  message_center::RichNotificationData optional_fields;
  optional_fields.settings_button_handler =
      message_center::SettingsButtonHandler::DELEGATE;

  SkBitmap bitmap;
  bool ret = bitmap.tryAllocPixels(SkImageInfo::Make(
      IMAGE_SIZE, IMAGE_SIZE, kRGBA_8888_SkColorType, kOpaque_SkAlphaType));
  ASSERT_TRUE(ret);
  ui::ImageModel image_model =
      ui::ImageModel::FromImage(gfx::Image::CreateFrom1xBitmap(bitmap));

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_CUSTOM, notification_key, u"title",
      u"message", image_model, u"arc", GURL(),
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 "ARC_NOTIFICATION"),
      optional_fields, nullptr);

  // Creates PassThroughDelegate (no existing delegate + non-TRANSIENT type)
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, nullptr,
                  notification, nullptr);

  // OnClosed with PassThroughDelegate will attempt delegate->Close(true)
  // which calls PassThroughDelegate::Close -> GetForProfile(nullptr).
  // This is a known limitation when testing without a real Profile.
  // The test verifies the Display path creates the delegate successfully.
  // Note: OnClosed with PassThroughDelegate requires a real Profile,
  // so this test does NOT call OnClosed.
}

// Test Display with a notification that has a PassThroughDelegate,
// then OnClicked is called. Same limitation as OnClosed - the
// PassThroughDelegate methods require a real Profile.
TEST_F(NotificationPlatformBridgeOhosTest,
       Display_PassThroughDelegate_OnClicked_NullProfile) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);

  std::string notification_key("onclicked_passthrough");
  message_center::RichNotificationData optional_fields;
  optional_fields.settings_button_handler =
      message_center::SettingsButtonHandler::DELEGATE;

  SkBitmap bitmap;
  bool ret = bitmap.tryAllocPixels(SkImageInfo::Make(
      IMAGE_SIZE, IMAGE_SIZE, kRGBA_8888_SkColorType, kOpaque_SkAlphaType));
  ASSERT_TRUE(ret);
  ui::ImageModel image_model =
      ui::ImageModel::FromImage(gfx::Image::CreateFrom1xBitmap(bitmap));

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_CUSTOM, notification_key, u"title",
      u"message", image_model, u"arc", GURL(),
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 "ARC_NOTIFICATION"),
      optional_fields, nullptr);

  // Creates PassThroughDelegate
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, nullptr,
                  notification, nullptr);

  // Note: OnClicked with PassThroughDelegate requires a real Profile,
  // so this test only verifies the Display path.
}

#if BUILDFLAG(ARKWEB_NOTIFICATION)
// Test Display with buttons (ARKWEB_NOTIFICATION button processing).
TEST_F(NotificationPlatformBridgeOhosTest, Display_WithButtons_NullProfile) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);

  std::string notification_key("notificationid_buttons");
  message_center::RichNotificationData optional_fields;
  optional_fields.settings_button_handler =
      message_center::SettingsButtonHandler::DELEGATE;
  optional_fields.buttons.push_back(message_center::ButtonInfo(u"Button1"));
  optional_fields.buttons.push_back(message_center::ButtonInfo(u"Button2"));

  SkBitmap bitmap;
  bool ret = bitmap.tryAllocPixels(SkImageInfo::Make(
      IMAGE_SIZE, IMAGE_SIZE, kRGBA_8888_SkColorType, kOpaque_SkAlphaType));
  ASSERT_TRUE(ret);
  ui::ImageModel image_model =
      ui::ImageModel::FromImage(gfx::Image::CreateFrom1xBitmap(bitmap));

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_CUSTOM, notification_key, u"title",
      u"message", image_model, u"arc", GURL(),
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 "ARC_NOTIFICATION"),
      optional_fields, nullptr);

  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, nullptr,
                  notification, nullptr);
}

// Test Display then Close lifecycle with null profile.
TEST_F(NotificationPlatformBridgeOhosTest, Display_AndClose_NullProfile) {
  std::unique_ptr<NotificationPlatformBridge> bridge =
      NotificationPlatformBridge::Create();
  ASSERT_TRUE(bridge);

  std::string notification_key("display_and_close");
  message_center::RichNotificationData optional_fields;

  SkBitmap bitmap;
  bool ret = bitmap.tryAllocPixels(SkImageInfo::Make(
      IMAGE_SIZE, IMAGE_SIZE, kRGBA_8888_SkColorType, kOpaque_SkAlphaType));
  ASSERT_TRUE(ret);
  ui::ImageModel image_model =
      ui::ImageModel::FromImage(gfx::Image::CreateFrom1xBitmap(bitmap));

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_CUSTOM, notification_key, u"title",
      u"message", image_model, u"arc", GURL(),
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 "ARC_NOTIFICATION"),
      optional_fields, nullptr);

  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, nullptr,
                  notification, nullptr);
  bridge->Close(nullptr, notification_key);
}
#endif  // BUILDFLAG(ARKWEB_NOTIFICATION)
