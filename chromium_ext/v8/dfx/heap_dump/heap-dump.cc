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
#if defined(OH_ENABLE_HEAP_DUMP)
#include "heap-dump.h"

#include "arkweb/chromium_ext/v8/dfx/heap_dump/binary-writer-base.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/binary-writer.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-heap.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-objects.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/heap-dump-log.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-heap.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-objects.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/ut/binary-reader-ut.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/ut/binary-writer-ut.h"
namespace v8 {
namespace dfx {
void DumpRawHeap(i::Heap* heap, v8::OutputStream* stream) {
  v8::dfx::OutputStreamWriter writer(stream);
  dfx::HeapDumper dumper(heap, &writer);
  dumper.DumpHeap();
}

void DumpRawHeap(i::Heap* heap, std::string file) {
#ifndef USING_OHOS_WEB
  dfx::BinaryWriterBase* writer = nullptr;
  dfx::BinaryWriter temp_writer;
  writer = &temp_writer;
  if (writer == nullptr) {
    return;
  }
  writer->OpenFile(file);
  dfx::HeapDumper dumper(heap, writer);
  dumper.DumpHeap();
  writer->CloseFile();
#endif
}

#ifdef USING_OHOS_WEB
static const std::string base_path = "data/storage/el2/base/";
#elif defined(USING_OHOS)
static const std::string base_path = "/data/storage/el2/base/";
#else
static const std::string base_path = "";
#endif

void DumpHeapAfterGC(i::Heap* heap) {
  int64_t time = i::V8::GetCurrentPlatform()->CurrentClockTimeMilliseconds();
  std::string file = base_path + "js-" + std::to_string(time) + ".rawheap";
  DumpRawHeap(heap, file);
}

void DumpHeapAfterOOM(i::Heap* heap) {
  int64_t time = i::V8::GetCurrentPlatform()->CurrentClockTimeMilliseconds();
  std::string file = base_path + "oom-" + std::to_string(time) + ".rawheap";
  LogInfo("DumpHeapAfterOOM to file:" + file);
  DumpRawHeap(heap, file);
}
}  // namespace dfx
}  // namespace v8
#endif
