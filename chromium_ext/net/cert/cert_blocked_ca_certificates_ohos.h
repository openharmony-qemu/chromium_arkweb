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

#ifndef NET_CERT_BLOCKED_CA_CERTIFICATES_OHOS_H_
#define NET_CERT_BLOCKED_CA_CERTIFICATES_OHOS_H_
 
#include "net/base/hash_value.h"
#include "net/base/net_export.h"
 
namespace net {
 
// Manages the set of blocked intermediate CA certificates for WebView.
// WebView uses CertVerifyProcAndroid (Android system TrustManager) which
// bypasses the upstream CRLSet SPKI blocklist (cert_verify_proc_blocklist.inc).
// This class provides an equivalent blocklist check for WebView by examining
// the SPKI hashes in the verified certificate chain.
class NET_EXPORT CertBlockedCaCertificatesOHOS {
 public:
  CertBlockedCaCertificatesOHOS() = delete;
 
  // Returns true if any certificate in the chain (identified by SPKI
  // SHA-256 hashes) matches a blocked intermediate CA. |spki_hashes|
  // contains the SHA-256 SPKI hashes of all certificates in the
  // verified chain (leaf, intermediates, root).
  static bool IsChainBlocked(const std::vector<SHA256HashValue>& spki_hashes);
 
  // Returns true if the given SPKI SHA-256 hash is in the block list.
  static bool IsBlocked(const SHA256HashValue& spki_hash);
};
 
}  // namespace net
 
#endif  // NET_CERT_BLOCKED_CA_CERTIFICATES_OHOS_H_