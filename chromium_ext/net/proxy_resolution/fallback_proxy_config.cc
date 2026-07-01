/*
 * Copyright (c) 2025-2025 Huawei Device Co., Ltd.
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
 
#include "arkweb/chromium_ext/net/proxy_resolution/fallback_proxy_config.h"
 
namespace net {
bool FallbackProxyConfigStatus::proxy_config_enable_ = false;
bool FallbackProxyConfigStatus::has_token_ = false;
std::atomic<bool> FallbackProxyConfigStatus::active_only_mode_{false};

void FallbackProxyConfigStatus::SetStatus(bool enable, bool has_token) {
  proxy_config_enable_ = enable;
  has_token_ = has_token;
}

int FallbackProxyConfigStatus::ProxyConfigEnable() {
  return proxy_config_enable_ ? 1 : 0;
}

int FallbackProxyConfigStatus::HasToken() {
  return has_token_ ? 1 : 0;
}

void FallbackProxyConfigStatus::SetActiveOnlyMode(bool active_only_mode) {
  active_only_mode_.store(active_only_mode, std::memory_order_release);
}

bool FallbackProxyConfigStatus::IsActiveOnlyMode() {
  return active_only_mode_.load(std::memory_order_acquire);
}

}  // namespace net
