/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "nweb_imf_input_attribute_adapter_impl.h"

namespace OHOS::NWeb {

int32_t NWebIMFInputAttributeAdapterImpl::GetInputPattern() {
  return input_pattern_;
}

int32_t NWebIMFInputAttributeAdapterImpl::GetEnterKeyType() {
  return enterKey_type_;
}

int32_t NWebIMFInputAttributeAdapterImpl::GetKeyboardImmersiveMode() {
  return immersive_mode_;
}

void NWebIMFInputAttributeAdapterImpl::SetInputPattern(int32_t inputPattern) {
  input_pattern_ = inputPattern;
}

void NWebIMFInputAttributeAdapterImpl::SetEnterKeyType(int32_t enterKeyType) {
  enterKey_type_ = enterKeyType;
}

void NWebIMFInputAttributeAdapterImpl::SetKeyboardImmersiveMode(int32_t immersiveMode) {
  immersive_mode_ = immersiveMode;
}
}  // namespace OHOS::NWeb
