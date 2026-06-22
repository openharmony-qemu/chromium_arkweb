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

#include "src/profiler/heap-profiler.h"

#if defined(OH_ENABLE_HEAP_DUMP)
#include "arkweb/chromium_ext/v8/dfx/heap_dump/heap-dump.h"
#endif

#if defined(OHOS_JS_ENGINE)
namespace v8 {

void HeapProfiler::DumpRawHeapSnapshot(OutputStream* stream) {
#if defined(OH_ENABLE_HEAP_DUMP)
  reinterpret_cast<i::HeapProfiler*>(this)->DumpRawHeapSnapshot(stream);
#endif
}

namespace internal {
#if defined(OH_ENABLE_HEAP_DUMP)
void HeapProfiler::DumpRawHeapSnapshot(OutputStream* stream) {
  dfx::DumpRawHeap(heap(), stream);
}
#endif
}  // namespace internal

}  // namespace v8

#endif
