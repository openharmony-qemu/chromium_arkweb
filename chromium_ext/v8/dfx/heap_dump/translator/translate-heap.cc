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

#include <string>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/binary-reader-base.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-format.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/heap-dump-log.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-header.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-objects.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-roots.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-string-tables.h"
#include "src/common/ptr-compr-inl.h"
#include "src/common/ptr-compr.h"
#include "src/flags/flags.h"

namespace v8 {
namespace dfx {
HeapTranslator::HeapTranslator(BinaryReaderBase* reader,
                               SnapshotGenerator* generator)
    : reader_(reader), generator_(generator) {}

HeapTranslator::~HeapTranslator() = default;

bool HeapTranslator::TranslateHeader() {
  if (reader_ == nullptr) {
    diagnostics_.Fatal("reader_missing", "rawheap reader is null", "header");
    return false;
  }
  std::string error;
  if (!ReadRawHeapHeaderForTranslate(reader_, &header_, &error)) {
    diagnostics_.Fatal("header_read_failed", error, "header", 0, 0, 0, 0,
                       reader_->CurrentPosition());
    return false;
  }
  PrintHead(header_);
  if (!ValidateRawHeapHeaderForTranslate(reader_, header_, &diagnostics_)) {
    return false;
  }

  // FakeHeapObject field loads decode compressed tagged values using the bases
  // captured in the producer isolate. Without this, raw payload bytes would
  // decode to addresses in the translator process instead of rawheap identity
  // addresses.
  i::V8HeapCompressionScheme::InitBase(header_.cage_base_);
  i::ExternalCodeCompressionScheme::InitBase(header_.code_cage_base_);
#if defined(V8_ENABLE_SANDBOX)
  if (header_.dump_format_version_ >= kDumpFormatVersionWithTrustedCageBase &&
      header_.trusted_cage_base_ != 0) {
    i::TrustedSpaceCompressionScheme::InitBase(header_.trusted_cage_base_);
  }
#endif
  return true;
}

bool HeapTranslator::TranslateHeap() {
  v8::base::ElapsedTimer timer;
  timer.Start();
  success_ = false;
  // Translation is the offline equivalent of V8 HeapSnapshotGenerator:
  // validate the rawheap envelope, load side tables, create synthetic roots,
  // then create all object nodes before emitting edges between them.
  if (!TranslateHeader()) {
    diagnostics_.PrintSummary();
    return false;
  }
  if (!InitOffheapTableResolver()) {
    diagnostics_.PrintSummary();
    return false;
  }
  if (!TranslateRoots()) {
    diagnostics_.PrintSummary();
    return false;
  }
  // TranslateStringTable();
  if (!TranslateObjects()) {
    diagnostics_.PrintSummary();
    return false;
  }
  root_translator_->EmitUserGlobalReferences();
  double elapsed = timer.Elapsed().InMillisecondsF();
  LogInfo("[HeapDump]elapsed time: " + std::to_string(elapsed) + " ms\n");
  timer.Stop();
  success_ = !diagnostics_.has_errors();
  diagnostics_.PrintSummary();
  return success_;
}

bool HeapTranslator::InitOffheapTableResolver() {
  offheap_table_resolver_ =
      std::make_unique<OffheapTableResolver>(reader_, header_, &diagnostics_);
  return offheap_table_resolver_->IsValid();
}

bool HeapTranslator::TranslateRoots() {
  root_translator_ = std::make_unique<RootTranslator>(
      header_.root_table_offset_, header_.root_type_count_,
      header_.root_table_size_, reader_, generator_, &diagnostics_);
  return root_translator_->Translate();
}

void HeapTranslator::TranslateStringTable() {}

bool HeapTranslator::TranslateObjects() {
  object_translator_ = std::make_unique<ObjectTranslator>(
      reader_, header_.object_table_offset_, header_.object_table_size_,
      header_.object_count_, i::PtrComprCageBase(header_.cage_base_),
      i::PtrComprCageBase(header_.code_cage_base_),
      header_.dump_format_version_ >= kDumpFormatVersionWithTrustedCageBase &&
          header_.trusted_cage_base_ != 0,
      generator_, &diagnostics_,
      offheap_table_resolver_.get());
  return object_translator_->Translate();
}
}  // namespace dfx
}  // namespace v8
#endif
