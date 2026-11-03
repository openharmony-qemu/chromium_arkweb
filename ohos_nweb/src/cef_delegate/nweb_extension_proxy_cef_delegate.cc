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

#include "nweb_extension_proxy_cef_delegate.h"

#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#endif
#include "base/logging.h"

#if BUILDFLAG(ARKWEB_NWEB_EX)
#include "ohos_nweb_ex/core/arkweb/nweb_extension_proxy_delegate_impl.h"
#endif

namespace OHOS::NWeb {

void NWebExtensionProxyCefDelegate::NotifyProxyInfo(const NWebExtensionProxyInfo& config) {
#if BUILDFLAG(ARKWEB_NWEB_EX)
  nweb_ex::NWebExtensionProxyDelegateImpl::GetInstance().NotifyProxyInfo(config);
#else
  LOG(ERROR) << "NWebExtensionProxyCefDelegate::NotifyProxyInfo: ARKWEB_NWEB_EX is not enabled";
#endif
}

}  // namespace OHOS::NWeb
