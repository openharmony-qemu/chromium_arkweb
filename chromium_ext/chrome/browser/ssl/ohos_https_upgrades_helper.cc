// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arkweb/chromium_ext/chrome/browser/ssl/ohos_https_upgrades_helper.h"

#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "net/base/network_change_notifier.h"

OhosHttpsUpgradesHelper::~OhosHttpsUpgradesHelper() = default;

// static
void OhosHttpsUpgradesHelper::NullOpenLoginTabCallback(
    content::WebContents* web_contents,
    bool focus) {}

OhosHttpsUpgradesHelper::OhosHttpsUpgradesHelper(
    content::WebContents* web_contents)
    : content::WebContentsUserData<OhosHttpsUpgradesHelper>(*web_contents) {}

void OhosHttpsUpgradesHelper::set_is_arkweb_https_upgrades_enable(bool enable) {
  if (is_arkweb_https_upgrades_enable_ == enable) {
    return;
  }

  LOG_FEEDBACK(INFO, kHttpsUpgrades)
      << "EnableHttpsUpgrades enable:" << enable
      << " isCaptivePortal:" << net::NetworkChangeNotifier::IsCaptivePortal()
      << " isNetworkValidated:"
      << net::NetworkChangeNotifier::IsNetworkValidated();
  is_arkweb_https_upgrades_enable_ = enable;
}

bool OhosHttpsUpgradesHelper::should_upgrade_to_https() const {
  return !net::NetworkChangeNotifier::IsCaptivePortal() &&
         net::NetworkChangeNotifier::IsNetworkValidated() &&
         is_arkweb_https_upgrades_enable_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OhosHttpsUpgradesHelper);
