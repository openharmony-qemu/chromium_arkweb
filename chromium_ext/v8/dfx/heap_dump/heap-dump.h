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
#ifndef HEAP_DUMP_H
#define HEAP_DUMP_H

#if defined(OH_ENABLE_HEAP_DUMP)

#include "src/heap/heap.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/outputstream-writer.h"
namespace v8 {
namespace dfx {
void DumpRawHeap(i::Heap* heap, v8::OutputStream* stream);
void DumpRawHeap(i::Heap* heap, std::string file);
void DumpHeapAfterGC(i::Heap* heap);
void DumpHeapAfterOOM(i::Heap* heap);
}  // namespace dfx
}  // namespace v8
#endif
#endif
