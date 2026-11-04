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

#ifndef DUMP_STRING_TABLES_H
#define DUMP_STRING_TABLES_H

#if defined(OH_ENABLE_HEAP_DUMP)
#include "arkweb/chromium_ext/v8/dfx/heap_dump/binary-writer-base.h"
#include "src/heap/heap-visitor-inl.h"
#include "src/heap/heap-visitor.h"
#include "src/heap/heap.h"
#include "src/heap/safepoint.h"
#include "src/utils/ostreams.h"
namespace v8 {
namespace dfx {

class StringTableDumper {
 public:
  StringTableDumper(i::Heap* heap, BinaryWriterBase* writer);
  ~StringTableDumper() = default;

  void PreVisit();

  void DumpAll();

  uint32_t StringCount() const;
  uint32_t StringTableDumpSize() const;

  static const int SIZE_OF_ADDR = 8;
  static const int SIZE_OF_LENGTH = 4;

 private:
  uint32_t string_count_{0};
  uint32_t string_table_dump_size_{0};

  i::Heap* heap_{nullptr};
  BinaryWriterBase* writer_{nullptr};
};
}  // namespace dfx
}  // namespace v8
#endif

#endif
