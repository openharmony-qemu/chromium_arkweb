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

#ifndef DUMP_OBJECTS_H
#define DUMP_OBJECTS_H

#if defined(OH_ENABLE_HEAP_DUMP)
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/binary-writer-base.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-format.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/heap-dump-log.h"
#include "src/codegen/assembler-inl.h"
#include "src/heap/combined-heap.h"
#include "src/heap/heap-visitor-inl.h"
#include "src/heap/heap-visitor.h"
#include "src/heap/heap.h"
#include "src/heap/visit-object.h"

namespace v8 {
namespace dfx {

struct InstructionStreamInfo {
  i::Address object_addr_;
  int8_t mode_;
};

class ObjectDumper {
 public:
  explicit ObjectDumper(i::Heap* heap, BinaryWriterBase* writer);
  ~ObjectDumper() = default;

  void DumpReadOnly();

  void Dump();

  void PreVisit();
  uint32_t PreVisitInstructionStream(i::Tagged<i::HeapObject> object);
  void AddInstructionStreamInfo(i::Address addr, InstructionStreamInfo info);

  uint32_t ObjectCount() const;
  uint32_t ObjectDumpSize() const;

  void IterateInstructionStream(i::Tagged<i::HeapObject> object);
  void WriteInstructionStreamInfo(i::Tagged<i::HeapObject> object);
  BinaryWriterBase* getWriter() { return writer_; }

 private:
  void DumpObjectWithoutContent(i::Tagged<i::HeapObject> object);
#ifdef DEBUG
  i::DISALLOW_GARBAGE_COLLECTION(no_heap_allocation_)
#else
  DISALLOW_GARBAGE_COLLECTION(no_heap_allocation_)
#endif

  i::Heap* heap_{nullptr};
  uint32_t object_cnt_{0};
  uint32_t total_object_size_{0};
  BinaryWriterBase* writer_{nullptr};
  std::unordered_map<i::Address, std::vector<InstructionStreamInfo>>
      instruction_stream_map_;

#ifdef OH_ENABLE_HEAP_DUMP_TEST
  // only for test
  std::unordered_map<i::Address, bool> heap_objects_;
#endif
};
}  // namespace dfx
}  // namespace v8
#endif  // OH_ENABLE_HEAP_DUMP_H
#endif
