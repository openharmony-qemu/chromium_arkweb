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
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-heap.h"

#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-format.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/heap-dump-log.h"
#include "include/v8-version.h"
#include "src/common/ptr-compr.h"
#include "src/flags/flags.h"
#include "src/heap/heap-inl.h"
#include "src/init/v8.h"

namespace v8 {
namespace dfx {
HeapDumper::HeapDumper(i::Heap* heap, BinaryWriterBase* writer)
    : heap_(heap),
      safepoint_scope_(std::make_unique<i::SafepointScope>(
          heap->isolate(), i::kGlobalSafepointForSharedSpaceIsolate)),
      writer_(writer),
      root_dumper_(heap, writer),
      string_table_dumper_(heap, writer),
      object_dumper_(heap, writer),
      offheap_table_dumper_(heap, writer) {
  heap_->MakeHeapIterable(i::CompleteSweepingReason::kHeapObjectIterator);
}

void HeapDumper::DumpHeap() {
  v8::base::ElapsedTimer timer;
  timer.Start();
  // PreVisit computes all section sizes before the header is written. Rawheap
  // keeps fixed offsets in the header, so every variable-size section must be
  // measured before any bytes are emitted.
  PreVisit();
  DumpHeader();
  root_dumper_.Dump();
  object_dumper_.Dump();
  offheap_table_dumper_.Dump();
  writer_->Flush();
  double elapsed = timer.Elapsed().InMillisecondsF();
  LogInfo("[HeapDump]elapsed time: " + std::to_string(elapsed) + " ms\n");
  timer.Stop();
}

void HeapDumper::DumpHeader() {
  memset(&header_, 0, sizeof(header_));
  header_.magic_ = kRawHeapMagic;
  header_.dump_format_version_ = kDumpFormatVersion;
  header_.flags_ = GetCurrentRawHeapFlags();
  header_.timestamp_ = static_cast<uint64_t>(
      i::V8::GetCurrentPlatform()->CurrentClockTimeMilliseconds());
  header_.v8_major_version_ = V8_MAJOR_VERSION;
  header_.v8_minor_version_ = V8_MINOR_VERSION;
  header_.v8_build_number_ = V8_BUILD_NUMBER;
  header_.v8_patch_level_ = V8_PATCH_LEVEL;

  header_.cage_base_ = heap_->isolate()->cage_base();
  header_.code_cage_base_ = heap_->isolate()->code_cage_base();
#if defined(V8_ENABLE_SANDBOX)
  header_.trusted_cage_base_ = heap_->isolate()->trusted_cage_base();
#else
  header_.trusted_cage_base_ = header_.cage_base_;
#endif

  header_.root_table_offset_ = sizeof(RawHeapHeader);
  header_.root_table_size_ = root_dumper_.RootDumpSize();
  header_.root_type_count_ = root_dumper_.RootKindCount();

  header_.string_table_offset_ =
      header_.root_table_offset_ + header_.root_table_size_;
  header_.string_table_size_ = string_table_dumper_.StringTableDumpSize();
  header_.string_count_ = string_table_dumper_.StringCount();

  header_.object_table_offset_ =
      header_.string_table_offset_ + header_.string_table_size_;
  header_.object_table_size_ = object_dumper_.ObjectDumpSize();
  header_.object_count_ = object_dumper_.ObjectCount();

  header_.offheap_table_offset_ =
      header_.object_table_offset_ + header_.object_table_size_;
  header_.offheap_table_size_ = offheap_table_dumper_.OffheapTableDumpSize();
  header_.offheap_table_count_ = offheap_table_dumper_.OffheapTableCount();

  header_.metadata_offset_ =
      header_.offheap_table_offset_ + header_.offheap_table_size_;
  header_.metadata_size_ = 0;

  PrintHead(header_);

  writer_->WriteBinBlock(reinterpret_cast<uint8_t*>(&header_), sizeof(header_));
}

void HeapDumper::PreVisit() {
  root_dumper_.PreVisit();
  object_dumper_.PreVisit();
  offheap_table_dumper_.PreVisit();
}
}  // namespace dfx
}  // namespace v8
#endif
