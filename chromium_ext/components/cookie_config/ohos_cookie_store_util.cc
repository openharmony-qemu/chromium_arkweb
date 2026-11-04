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

#include "components/cookie_config/cookie_store_util.h"

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"

namespace cookie_config {

#if BUILDFLAG(ARKWEB_ENCRYPT)
namespace {

// Use the operating system's mechanisms to encrypt cookies before writing
// them to persistent store.  Currently this only is done with desktop OS's
// because ChromeOS and Android already protect the entire profile contents.
class CookieOHOSCryptoDelegate : public net::CookieCryptoDelegate {
 public:
  void Init(base::OnceClosure callback) override;
  bool EncryptString(const std::string& plaintext,
                     std::string* ciphertext) override;
  bool DecryptString(const std::string& ciphertext,
                     std::string* plaintext) override;
};

void CookieOHOSCryptoDelegate::Init(base::OnceClosure callback) {
  std::move(callback).Run();
}

bool CookieOHOSCryptoDelegate::EncryptString(const std::string& plaintext,
                                             std::string* ciphertext) {
  if (!ciphertext) {
    return false;
  }
  return OSCrypt::EncryptString(plaintext, ciphertext);
}

bool CookieOHOSCryptoDelegate::DecryptString(const std::string& ciphertext,
                                             std::string* plaintext) {
  if (!plaintext) {
    return false;
  }
  return OSCrypt::DecryptString(ciphertext, plaintext);
}

}  // namespace

std::unique_ptr<net::CookieCryptoDelegate> GetCookieOHOSCryptoDelegate() {
  return std::make_unique<CookieOHOSCryptoDelegate>();
}

#endif // BUILDFLAG(ARKWEB_ENCRYPT)

}  // namespace cookie_config
