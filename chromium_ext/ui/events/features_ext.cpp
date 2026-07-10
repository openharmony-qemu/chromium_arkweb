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

#include "arkweb/chromium_ext/ui/events/features_ext.h"

namespace features {

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
BASE_FEATURE(kArkWebImeProcessKeyDomKeyFix,
             "ArkWebImeProcessKeyDomKeyFix",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

}  // namespace features
