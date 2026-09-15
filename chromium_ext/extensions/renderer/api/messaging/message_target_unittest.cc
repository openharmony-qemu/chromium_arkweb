// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/messaging/message_target.h"

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS) && BUILDFLAG(IS_ARKWEB_EXT)

#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(MessageTargetTest, ForBrowserCreatesBrowserTarget)
{
  MessageTarget target = MessageTarget::ForBrowser();

  EXPECT_EQ(MessageTarget::BROWSER, target.type);
  EXPECT_FALSE(target.extension_id);
  EXPECT_FALSE(target.native_application_name);
  EXPECT_FALSE(target.tab_id);
  EXPECT_FALSE(target.frame_id);
  EXPECT_FALSE(target.document_id);
}

TEST(MessageTargetTest, BrowserTargetsCompareEqualOnlyToBrowserTargets)
{
  MessageTarget browser_target = MessageTarget::ForBrowser();

  EXPECT_EQ(browser_target, MessageTarget::ForBrowser());
  EXPECT_FALSE(browser_target == MessageTarget::ForExtension("test_extension"));
  EXPECT_FALSE(browser_target == MessageTarget::ForNativeApp("test_native_app"));
  EXPECT_FALSE(browser_target == MessageTarget::ForTab(1, 0));
}

}  // namespace extensions

#endif  // BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS) && BUILDFLAG(IS_ARKWEB_EXT)
