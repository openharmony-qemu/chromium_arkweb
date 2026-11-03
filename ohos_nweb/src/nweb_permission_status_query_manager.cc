/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "nweb_permission_status_query_manager.h"

#include "base/logging.h"
#include "nweb_common.h"
#include "nweb_hilog.h"
#if BUILDFLAG(ARKWEB_NWEB_EX)
#include "ohos_nweb_ex/core/arkweb/nweb_web_permission_delegate_impl.h"
#endif

namespace OHOS::NWeb {

#if BUILDFLAG(ARKWEB_NWEB_EX)
NO_SANITIZE("cfi-icall")
void NWebPermissionStatusQueryManager::QueryPermissionStatus(
    ArkWebPermissionStatusQuery* query) {
  if (nweb_ex::NWebWebPermissionDelegateImpl::GetInstance()
          .OnPermissionStatusQuery(query)) {
    return;
  }

  if (query && query->arkweb_access_query) {
    query->arkweb_access_query->report_query_result(
        1, query->arkweb_access_query->cef_access_query_key);
  }
}
#endif

}  // namespace OHOS::NWeb