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
 
#ifndef BASE_OHOS_SAFE_BROWSING_DATA_CONTROLLER_H_
#define BASE_OHOS_SAFE_BROWSING_DATA_CONTROLLER_H_
 
#include "mojo/public/cpp/system/buffer.h"
#include "services/viz/privileged/mojom/gl/gpu_host.mojom.h"
 
namespace base {
namespace ohos {
 
class SafeBrowsingDataController {
public:
  static SafeBrowsingDataController& GetInstance();
  ~SafeBrowsingDataController() = default;
 
  void DumpAntiFraudSnapshot(mojo::ScopedSharedBufferHandle buffer,
                             viz::mojom::SbBitmapMetadataPtr metadata);
 
private:
  SafeBrowsingDataController();
};
 
}  // namespace ohos
}  // namespace base
 
#endif  // BASE_OHOS_SAFE_BROWSING_DATA_CONTROLLER_H_