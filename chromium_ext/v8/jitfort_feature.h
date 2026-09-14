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

#ifndef ARKWEB_CHROMIUM_EXT_V8_JITFORT_FEATURE_H_
#define ARKWEB_CHROMIUM_EXT_V8_JITFORT_FEATURE_H_

// Returns true if JitFort is enabled (default), false if disabled via
// --disable-features=JitFort. Implemented in a separate compilation unit
// (jitfort_feature.cc) that only includes Chromium base headers, avoiding
// macro conflicts with V8's own base headers.
bool IsJitFortEnabled();

#endif  // ARKWEB_CHROMIUM_EXT_V8_JITFORT_FEATURE_H_
