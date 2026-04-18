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

#if (defined(OH_ENABLE_HEAP_TRANSLATE) || defined(OH_ENABLE_HEAP_DUMP_TEST))

#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-heap.h"

#include "arkweb/chromium_ext/v8/dfx/heap_dump/binary-reader-base.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-format.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/heap-dump-log.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-objects.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-roots.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-string-tables.h"
#include "src/common/ptr-compr-inl.h"
#include "src/common/ptr-compr.h"

namespace v8 {
namespace dfx {
HeapTranslator::HeapTranslator(BinaryReaderBase* reader,
                               SnapshotGenerator* generator)
    : reader_(reader), generator_(generator) {}

void HeapTranslator::TranslateHeader() {
  memset(&header_, 0, sizeof(header_));
  uint32_t size = static_cast<uint32_t>(sizeof(header_));
  CHECK(reader_->ReadData(size, reinterpret_cast<uint8_t*>(&header_), size));
  PrintHead(header_);
  CHECK(IsVersionCompatible(header_));
  CHECK(i::V8HeapCompressionScheme::GetPtrComprCageBaseAddress(
            header_.cage_base_) == header_.cage_base_);

  i::V8HeapCompressionScheme::InitBase(header_.cage_base_);
  i::ExternalCodeCompressionScheme::InitBase(header_.code_cage_base_);
}

void HeapTranslator::TranslateHeap() {
  v8::base::ElapsedTimer timer;
  timer.Start();
  TranslateHeader();
  TranslateRoots();
  // TranslateStringTable();
  TranslateObjects();
  double elapsed = timer.Elapsed().InMillisecondsF();
  LogInfo("[HeapDump]elapsed time: " + std::to_string(elapsed) + " ms\n");
  timer.Stop();
}

void HeapTranslator::TranslateRoots() {
  root_translator_ = std::make_unique<RootTranslator>(
      header_.root_table_offset_, header_.root_type_count_,
      header_.root_table_size_, reader_, generator_);
  root_translator_->Translate();
}

void HeapTranslator::TranslateStringTable() {}

void HeapTranslator::TranslateObjects() {
  object_translator_ = std::make_unique<ObjectTranslator>(
      reader_, header_.object_table_offset_, header_.object_table_size_,
      header_.object_count_, i::PtrComprCageBase(header_.cage_base_),
      i::PtrComprCageBase(header_.code_cage_base_), generator_);
  object_translator_->Translate();
}
}  // namespace dfx
}  // namespace v8
#endif
