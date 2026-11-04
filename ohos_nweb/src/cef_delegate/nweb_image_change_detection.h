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

#ifndef NWEB_IAMGE_CHANGE_DETECTION_H
#define NWEB_IAMGE_CHANGE_DETECTION_H

#include "nweb_delegate.h"

namespace OHOS::NWeb {

class NWebImageChangeDetection {
 public:
  explicit NWebImageChangeDetection(base::WeakPtr<NWebDelegate> nweb_delegate);
  void SetImageChangeDetectionEnable(bool enable);

 private:
  void StartDetection();
  void StopDetection();

  bool enable_ = false;
  base::WeakPtr<NWebDelegate> nweb_delegate_;
  base::WeakPtrFactory<NWebImageChangeDetection> weak_factory_{this};
};

} // namespace OHOS::NWeb

#endif //NWEB_IAMGE_CHANGE_DETECTION_H