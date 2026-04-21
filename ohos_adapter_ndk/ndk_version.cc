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

#include "ndk_version.h"
#include <deviceinfo.h>
#include "arkweb/ohos_nweb/src/nweb_hilog.h"

#ifdef __cplusplus
extern "C" {
#endif

int GetParamApiVersion()
{
  int version = OH_GetSdkApiVersion();
  if (version <= 0) {
    WVLOG_W("GetParamApiVersion fallback to 0, sdk api version invalid: %{public}d", version);
    return 0;
  }
  return version;
}

bool CheckTargetApiVersion(int TargetApiVersion)
{
  int ParamApiVersion = GetParamApiVersion();
  if (TargetApiVersion > ParamApiVersion) {
    return true;
  }
  return false;
}

#ifdef __cplusplus
}
#endif