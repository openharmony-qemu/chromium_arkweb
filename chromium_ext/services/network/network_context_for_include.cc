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

namespace network {

#if BUILDFLAG(ARKWEB_MEDIA)
void NetworkContext::LookupServerBasicAuthCredentials(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    LookupServerBasicAuthCredentialsCallback callback) {
  net::HttpAuthCache* http_auth_cache =
      url_request_context_->http_transaction_factory()
          ->GetSession()
          ->http_auth_cache();
  net::HttpAuthCache::Entry* entry = http_auth_cache->LookupByPath(
      url::SchemeHostPort(url), net::HttpAuth::AUTH_SERVER,
      network_anonymization_key, url.GetPath());
  if (entry && entry->scheme() == net::HttpAuth::AUTH_SCHEME_BASIC) {
    std::move(callback).Run(entry->credentials());
  } else {
    std::move(callback).Run(std::nullopt);
  }
}
#endif // ARKWEB_MEDIA

}  // namespace network
