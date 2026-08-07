// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARKWEB_CHROMIUM_EXT_CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NWEB_TAB_FACTORY_H_
#define ARKWEB_CHROMIUM_EXT_CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NWEB_TAB_FACTORY_H_

#include "base/functional/callback.h"

class GURL;

namespace ohos {

// Callback type for async NWeb tab creation.
// |success|: true if popup was created successfully.
// |result|: target_id on success, error message on failure.
using NWebTabCreatedCallback =
    base::OnceCallback<void(bool success, const std::string& result)>;

// Triggers creation of a visible NWeb tab via the ARKWEB_MULTI_WINDOW popup
// flow. The |callback| will be invoked when the popup creation completes
// (either success or failure).
// When ARKWEB_MULTI_WINDOW is disabled, invokes |callback| immediately with
// (false, error_message).
void TryCreateNWebTab(const GURL& url,
                      bool is_tab,
                      NWebTabCreatedCallback callback);

}  // namespace ohos

#endif  // ARKWEB_CHROMIUM_EXT_CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NWEB_TAB_FACTORY_H_
