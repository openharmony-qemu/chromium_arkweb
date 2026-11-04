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

#ifndef V8_V8_OOM_REPORTER_H_
#define V8_V8_OOM_REPORTER_H_

#include "v8-callbacks.h"  // for OOMDetails

namespace v8 {
namespace dfx {
using V8OOMReportCallbackWithData = void (*)(const char* location,
                                             const OOMDetails& details,
                                             void* data);

V8_EXPORT V8OOMReportCallbackWithData GetOOMReportCallback();
V8_EXPORT void SetOOMReportCallback(V8OOMReportCallbackWithData callback);
}  // namespace dfx
}  // namespace v8

#endif  // V8_V8_OOM_REPORTER_H_
