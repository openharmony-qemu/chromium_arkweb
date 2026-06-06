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

#ifndef BASE_TRACE_EVENT_HITRACE_EXPORT_OH_H_
#define BASE_TRACE_EVENT_HITRACE_EXPORT_OH_H_

#include "base/base_export.h"
#include "build/build_config.h"
#include "arkweb/build/features/features.h"

namespace base {
namespace hitrace {

// Enables exporting of track events to HiTrace in the current process.
// This should be called during tracing initialization if HiTrace is available.
BASE_EXPORT void EnableHiTraceExport();

}  // namespace hitrace
}  // namespace base

#endif  // BASE_TRACE_EVENT_HITRACE_EXPORT_OH_H_
