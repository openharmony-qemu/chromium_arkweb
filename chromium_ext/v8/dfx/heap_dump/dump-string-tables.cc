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

#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-string-tables.h"

#include "src/objects/string-table.h"
namespace v8 {
namespace dfx {
class VisitStringTableInfo : public i::RootVisitor {
 public:
  explicit VisitStringTableInfo(i::Heap* heap, BinaryWriterBase* writer)
      : heap_(heap),
        writer_(writer) {}

  void VisitRootPointers(i::Root root, const char* description,
                         i::FullObjectSlot start,
                         i::FullObjectSlot end) override {
    UNREACHABLE();
  }

  void VisitCompressedRootPointers(i::Root root, const char* description,
                                   i::OffHeapObjectSlot start,
                                   i::OffHeapObjectSlot end) override {
    if (root != i::Root::kStringTable) {
      return;
    }
    // i::Isolate* isolate = heap_->isolate();
    i::PtrComprCageBase cage_base(heap_->isolate());
    for (i::OffHeapObjectSlot p = start; p < end; ++p) {
      i::Tagged<i::Object> obj = p.load(cage_base);
      if (IsHeapObject(obj)) {
        i::Tagged<i::HeapObject> heap_object = Cast<i::HeapObject>(obj);
        i::Address object_address = heap_object->address();
        int len = Cast<i::String>(*heap_object)->length();
        total_object_size_ += sizeof(object_address) + sizeof(len);
        writer_->WriteBinBlock(reinterpret_cast<uint8_t*>(&object_address),
                               sizeof(object_address));
        writer_->WriteBinBlock(reinterpret_cast<uint8_t*>(&len), sizeof(len));
      }
    }
  }

  uint32_t GetTotalObjectSize() { return total_object_size_; }

 private:
  i::Heap* heap_;
  BinaryWriterBase* writer_;
  uint32_t total_object_size_{0};
};

StringTableDumper::StringTableDumper(i::Heap* heap, BinaryWriterBase* writer)
    : heap_(heap), writer_(writer) {}

void StringTableDumper::DumpAll() {
  i::Isolate* isolate = heap_->isolate();
  i::StringTable* string_table = isolate->string_table();
  VisitStringTableInfo stringTableInfo(heap_, writer_);
  string_table->IterateElements(&stringTableInfo);
}

void StringTableDumper::PreVisit() {
  i::Isolate* isolate = heap_->isolate();
  i::StringTable* string_table = isolate->string_table();
  string_count_ = string_table->NumberOfElements();
  string_table_dump_size_ = string_count_ * (SIZE_OF_ADDR + SIZE_OF_LENGTH);
}

uint32_t StringTableDumper::StringCount() const { return string_count_; }

uint32_t StringTableDumper::StringTableDumpSize() const {
  return string_table_dump_size_;
}

}  // namespace dfx
}  // namespace v8
#endif
