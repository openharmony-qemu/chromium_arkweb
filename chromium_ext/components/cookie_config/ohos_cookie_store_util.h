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

#ifndef COMPONENTS_COOKIE_CONFIG_OHOS_COOKIE_STORE_UTIL_H_
#define COMPONENTS_COOKIE_CONFIG_OHOS_COOKIE_STORE_UTIL_H_

#include <memory>

#include "net/extras/sqlite/cookie_crypto_delegate.h"

namespace cookie_config {

#if BUILDFLAG(ARKWEB_ENCRYPT)
// Factory method for returning a CookieCryptoDelegate if one is appropriate for
// this platform.
std::unique_ptr<net::CookieCryptoDelegate> GetCookieOHOSCryptoDelegate();
#endif // BUILDFLAG(ARKWEB_ENCRYPT)

} // namespace cookie_config

#endif  // COMPONENTS_COOKIE_CONFIG_OHOS_COOKIE_STORE_UTIL_H_
