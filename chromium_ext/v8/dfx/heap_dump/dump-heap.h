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

#ifndef DUMP_HEAP_H
#define DUMP_HEAP_H

#if defined(OH_ENABLE_HEAP_DUMP)

#include "arkweb/chromium_ext/v8/dfx/heap_dump/binary-writer-base.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-format.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-offheap-tables.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-objects.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-roots.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-string-tables.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/heap-dump-log.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/codegen/assembler-inl.h"
#include "src/heap/combined-heap.h"
#include "src/heap/heap-visitor-inl.h"
#include "src/heap/heap-visitor.h"
#include "src/heap/heap.h"
#include "src/heap/visit-object.h"

namespace v8 {
namespace dfx {

class HeapDumper {
 public:
  HeapDumper(i::Heap* heap, BinaryWriterBase* writer);
  ~HeapDumper() = default;

  void DumpHeap();

 private:
#ifdef DEBUG
  i::DISALLOW_GARBAGE_COLLECTION(no_heap_allocation_)
#else
  DISALLOW_GARBAGE_COLLECTION(no_heap_allocation_)
#endif

  void PreVisit();
  void DumpHeader();

  // v8 related
  i::Heap* heap_{nullptr};
  std::unique_ptr<i::SafepointScope> safepoint_scope_;

  // dump related
  BinaryWriterBase* writer_{nullptr};
  RawHeapHeader header_;
  RootDumper root_dumper_;
  StringTableDumper string_table_dumper_;
  ObjectDumper object_dumper_;
  OffheapTableDumper offheap_table_dumper_;
};
}  // namespace dfx
}  // namespace v8
#endif
#endif  // DUMP_HEAP_H
