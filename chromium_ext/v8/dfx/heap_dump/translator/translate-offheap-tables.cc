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

#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-offheap-tables.h"

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/binary-reader-base.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-diagnostics.h"

namespace v8 {
namespace dfx {
namespace {
constexpr char kOffheapTableSection[] = "offheap_table";
}  // namespace

OffheapTableResolver::OffheapTableResolver(
    BinaryReaderBase* reader, const RawHeapHeader& header,
    TranslateDiagnostics* diagnostics)
    : diagnostics_(diagnostics) {
  is_valid_ = Init(reader, header);
}

bool OffheapTableResolver::Init(BinaryReaderBase* reader,
                                const RawHeapHeader& header) {
  tables_.clear();
  seen_kinds_.clear();
  supported_table_ranges_.clear();
  wasm_struct_records_.clear();
  wasm_struct_fields_.clear();
  wasm_struct_layouts_.clear();
  js_dispatch_entries_.clear();
#if defined(V8_ENABLE_SANDBOX)
  trusted_pointer_entries_.clear();
#endif
  if (!HasRawHeapOffheapTableSection(header)) {
    return true;
  }
  if (reader == nullptr) {
    ReportFatal("offheap_table_reader_missing",
                "rawheap reader is null while offheap table section exists",
                header.offheap_table_offset_);
    return false;
  }

  const uint32_t binary_size = reader->BinarySize();
  const uint32_t section_offset = header.offheap_table_offset_;
  const uint32_t section_size = header.offheap_table_size_;
  if (section_offset == 0 || section_size == 0 ||
      header.offheap_table_count_ == 0) {
    ReportFatal("offheap_table_directory_truncated",
                "offheap table section has inconsistent header fields",
                section_offset);
    return false;
  }
  if (section_offset > binary_size ||
      section_size > binary_size - section_offset) {
    ReportFatal("offheap_table_payload_out_of_range",
                "offheap table section extends beyond rawheap file",
                section_offset);
    return false;
  }

  const uint64_t directory_size =
      static_cast<uint64_t>(header.offheap_table_count_) *
      sizeof(RawHeapOffheapTableHeader);
  if (directory_size > section_size) {
    ReportFatal("offheap_table_directory_truncated",
                "offheap table directory exceeds the offheap section",
                section_offset);
    return false;
  }

  for (uint32_t index = 0; index < header.offheap_table_count_; ++index) {
    const uint64_t relative_entry_offset =
        static_cast<uint64_t>(index) * sizeof(RawHeapOffheapTableHeader);
    if (relative_entry_offset > section_size ||
        sizeof(RawHeapOffheapTableHeader) >
            section_size - relative_entry_offset) {
      ReportFatal("offheap_table_directory_truncated",
                  "offheap table directory entry is outside the section",
                  section_offset);
      return false;
    }
    const uint64_t absolute_entry_offset =
        static_cast<uint64_t>(section_offset) + relative_entry_offset;
    if (absolute_entry_offset > std::numeric_limits<uint32_t>::max()) {
      ReportFatal("offheap_table_directory_truncated",
                  "offheap table directory entry offset overflows uint32_t",
                  section_offset);
      return false;
    }

    RawHeapOffheapTableHeader table_header{};
    if (!ReadDirectoryEntry(reader, static_cast<uint32_t>(absolute_entry_offset),
                            &table_header)) {
      return false;
    }
    if (!ValidateAndStoreTable(reader, header, table_header,
                               static_cast<uint32_t>(absolute_entry_offset),
                               static_cast<uint32_t>(directory_size))) {
      return false;
    }
  }
  return true;
}

bool OffheapTableResolver::ReadDirectoryEntry(
    BinaryReaderBase* reader, uint32_t entry_offset,
    RawHeapOffheapTableHeader* table_header) {
  if (table_header == nullptr ||
      !reader->ReadDataAt(entry_offset, sizeof(*table_header),
                          reinterpret_cast<uint8_t*>(table_header),
                          sizeof(*table_header))) {
    ReportFatal("offheap_table_directory_truncated",
                "failed to read offheap table directory entry", entry_offset);
    return false;
  }
  return true;
}

bool OffheapTableResolver::ValidateAndStoreTable(
    BinaryReaderBase* reader,
    const RawHeapHeader& header,
    const RawHeapOffheapTableHeader& table_header, uint32_t entry_offset,
    uint32_t directory_size) {
  const uint32_t section_offset = header.offheap_table_offset_;
  const uint32_t section_size = header.offheap_table_size_;

  const RawHeapOffheapTableKind kind =
      static_cast<RawHeapOffheapTableKind>(table_header.kind_);
  if (!IsSupportedTableKind(kind)) {
#if defined(V8_ENABLE_SANDBOX)
    if (IsSandboxUnsupportedTableKind(kind)) {
      ReportSandboxUnsupportedTableKind(table_header.kind_, entry_offset);
      return true;
    }
#endif
    ReportWarning("offheap_table_unknown_kind",
                  "ignoring unsupported offheap table kind " +
                      std::to_string(table_header.kind_),
                  entry_offset);
    return true;
  }

  if (auto [it, inserted] = seen_kinds_.try_emplace(table_header.kind_,
                                                    entry_offset);
      !inserted) {
    ReportFatal("offheap_table_duplicate_kind",
                "duplicate offheap table kind " +
                    std::to_string(table_header.kind_),
                entry_offset);
    return false;
  }

  if (table_header.offset_ < directory_size) {
    ReportFatal("offheap_table_payload_overlaps_directory",
                "supported offheap table payload starts inside the directory",
                entry_offset);
    return false;
  }

  if (table_header.offset_ > section_size ||
      table_header.size_ > section_size - table_header.offset_) {
    ReportFatal("offheap_table_payload_out_of_range",
                "offheap table payload is outside the offheap section",
                entry_offset);
    return false;
  }

  const uint64_t absolute_payload_offset =
      static_cast<uint64_t>(section_offset) + table_header.offset_;
  if (absolute_payload_offset > std::numeric_limits<uint32_t>::max()) {
    ReportFatal("offheap_table_payload_out_of_range",
                "offheap table payload offset overflows uint32_t",
                entry_offset);
    return false;
  }

  const uint32_t payload_end = table_header.offset_ + table_header.size_;
  for (const SupportedTableRange& range : supported_table_ranges_) {
    if (table_header.size_ == 0 || range.start_ == range.end_) {
      continue;
    }
    if (table_header.offset_ < range.end_ && range.start_ < payload_end) {
      ReportFatal("offheap_table_payload_overlap",
                  "supported offheap table payload overlaps another supported "
                  "table payload",
                  entry_offset);
      return false;
    }
  }

  RawHeapOffheapTableSlice table{
      static_cast<uint32_t>(absolute_payload_offset), table_header.size_,
      table_header.count_};
  if (kind == RawHeapOffheapTableKind::kWasmCanonicalTypeTable &&
      !DecodeWasmCanonicalTypeTable(reader, table, entry_offset)) {
    return false;
  }
  if (kind == RawHeapOffheapTableKind::kJSDispatchTable &&
      !DecodeJSDispatchTable(reader, table, entry_offset)) {
    return false;
  }
#if defined(V8_ENABLE_SANDBOX)
  if (kind == RawHeapOffheapTableKind::kSandboxTrustedPointerTable &&
      !DecodeSandboxTrustedPointerTable(reader, table, entry_offset)) {
    return false;
  }
  if (kind == RawHeapOffheapTableKind::kSandboxJSDispatchTable &&
      !DecodeJSDispatchTable(reader, table, entry_offset)) {
    return false;
  }
  if (kind == RawHeapOffheapTableKind::kSandboxCodePointerTable &&
      !DecodeSandboxCodePointerTable(reader, table, entry_offset)) {
    return false;
  }
#endif

  tables_.emplace(kind, table);
  supported_table_ranges_.push_back(SupportedTableRange{
      table_header.offset_, payload_end, table_header.kind_, entry_offset});
  return true;
}

bool OffheapTableResolver::DecodeWasmCanonicalTypeTable(
    BinaryReaderBase* reader, const RawHeapOffheapTableSlice& table,
    uint32_t entry_offset) {
  wasm_struct_records_.clear();
  wasm_struct_fields_.clear();
  wasm_struct_layouts_.clear();

  if (reader == nullptr) {
    ReportFatal("wasm_canonical_type_table_reader_missing",
                "rawheap reader is null while decoding Wasm canonical type "
                "table",
                entry_offset);
    return false;
  }

  if (table.count_ >
      table.size_ / sizeof(RawHeapWasmCanonicalStructRecord)) {
    ReportCorrupt("wasm_canonical_type_table_records_truncated",
                  "Wasm canonical type table records exceed table payload",
                  entry_offset);
    return false;
  }

  const uint32_t records_size =
      table.count_ * sizeof(RawHeapWasmCanonicalStructRecord);
  std::vector<RawHeapWasmCanonicalStructRecord> records;
  records.resize(table.count_);
  if (records_size != 0 &&
      !reader->ReadDataAt(
          table.absolute_offset_, records_size,
          reinterpret_cast<uint8_t*>(records.data()), records_size)) {
    ReportCorrupt("wasm_canonical_type_table_records_read_failed",
                  "failed to read Wasm canonical type table records",
                  table.absolute_offset_);
    return false;
  }

  std::vector<RawHeapWasmCanonicalStructField> fields;
  std::map<uint32_t, WasmCanonicalStructLayout> layouts;
  for (uint32_t index = 0; index < table.count_; ++index) {
    const RawHeapWasmCanonicalStructRecord& record = records[index];
    const uint64_t record_offset64 =
        static_cast<uint64_t>(table.absolute_offset_) +
        static_cast<uint64_t>(index) *
            sizeof(RawHeapWasmCanonicalStructRecord);
    const uint32_t record_offset =
        record_offset64 > std::numeric_limits<uint32_t>::max()
            ? table.absolute_offset_
            : static_cast<uint32_t>(record_offset64);

    if (record.fields_offset_ > table.size_) {
      ReportCorrupt("wasm_canonical_struct_fields_offset_out_of_range",
                    "Wasm canonical struct fields offset is outside the table "
                    "payload; type_index=" +
                        std::to_string(record.type_index_),
                    record_offset);
      return false;
    }
    const uint32_t available_fields_bytes =
        table.size_ - record.fields_offset_;
    if (record.field_count_ >
        available_fields_bytes / sizeof(RawHeapWasmCanonicalStructField)) {
      ReportCorrupt("wasm_canonical_struct_fields_truncated",
                    "Wasm canonical struct field records exceed the table "
                    "payload; type_index=" +
                        std::to_string(record.type_index_),
                    record_offset);
      return false;
    }
    if (record.field_count_ != 0 && record.fields_offset_ < records_size) {
      ReportCorrupt("wasm_canonical_struct_fields_overlap_records",
                    "Wasm canonical struct field records overlap the record "
                    "array; type_index=" +
                        std::to_string(record.type_index_),
                    record_offset);
      return false;
    }
    if (layouts.find(record.type_index_) != layouts.end()) {
      ReportCorrupt("wasm_canonical_struct_duplicate_type_index",
                    "duplicate Wasm canonical struct type_index=" +
                        std::to_string(record.type_index_),
                    record_offset);
      return false;
    }

    const uint32_t field_start_index = static_cast<uint32_t>(fields.size());
    if (record.field_count_ != 0) {
      if (record.field_count_ >
          (std::numeric_limits<uint32_t>::max() - field_start_index)) {
        ReportCorrupt("wasm_canonical_struct_fields_too_large",
                      "Wasm canonical struct field count exceeds uint32_t; "
                      "type_index=" +
                          std::to_string(record.type_index_),
                      record_offset);
        return false;
      }
      const uint32_t field_bytes =
          record.field_count_ * sizeof(RawHeapWasmCanonicalStructField);
      if (record.fields_offset_ >
          std::numeric_limits<uint32_t>::max() - table.absolute_offset_) {
        ReportCorrupt("wasm_canonical_struct_fields_offset_overflow",
                      "Wasm canonical struct field payload offset overflows "
                      "uint32_t; type_index=" +
                          std::to_string(record.type_index_),
                      record_offset);
        return false;
      }
      const uint32_t fields_absolute_offset =
          table.absolute_offset_ + record.fields_offset_;
      fields.resize(static_cast<size_t>(field_start_index) +
                    record.field_count_);
      if (!reader->ReadDataAt(
              fields_absolute_offset, field_bytes,
              reinterpret_cast<uint8_t*>(fields.data() + field_start_index),
              field_bytes)) {
        ReportCorrupt("wasm_canonical_struct_fields_read_failed",
                      "failed to read Wasm canonical struct fields; "
                      "type_index=" +
                          std::to_string(record.type_index_),
                      fields_absolute_offset);
        return false;
      }
    }

    layouts.emplace(record.type_index_,
                    WasmCanonicalStructLayout{index, field_start_index});
  }

  wasm_struct_records_ = std::move(records);
  wasm_struct_fields_ = std::move(fields);
  wasm_struct_layouts_ = std::move(layouts);
  return true;
}

bool OffheapTableResolver::IsSupportedTableKind(
    RawHeapOffheapTableKind kind) const {
  switch (kind) {
    case RawHeapOffheapTableKind::kWasmCanonicalTypeTable:
      return true;
    case RawHeapOffheapTableKind::kJSDispatchTable:
      return true;
    case RawHeapOffheapTableKind::kStringTableReserved:
      return false;
#if defined(V8_ENABLE_SANDBOX)
    case RawHeapOffheapTableKind::kSandboxTrustedPointerTable:
      return true;
    case RawHeapOffheapTableKind::kSandboxJSDispatchTable:
      return true;
    case RawHeapOffheapTableKind::kSandboxCodePointerTable:
      return true;
    case RawHeapOffheapTableKind::kSandboxExternalPointerTable:
    case RawHeapOffheapTableKind::kSandboxCppHeapPointerTable:
      return false;
#endif
  }
  return false;
}

#if defined(V8_ENABLE_SANDBOX)
bool OffheapTableResolver::DecodeSandboxTrustedPointerTable(
    BinaryReaderBase* reader, const RawHeapOffheapTableSlice& table,
    uint32_t entry_offset) {
  trusted_pointer_entries_.clear();

  if (reader == nullptr) {
    ReportFatal("sandbox_trusted_pointer_table_reader_missing",
                "rawheap reader is null while decoding sandbox trusted "
                "pointer table",
                entry_offset);
    return false;
  }

  // Keep this decoder stricter than unknown-table skipping: a known sandbox
  // schema with trailing bytes likely means the producer and translator
  // disagree, and treating those bytes as optional could hide rawheap skew.
  if (table.size_ % sizeof(RawHeapSandboxTrustedPointerEntry) != 0) {
    ReportCorrupt("sandbox_trusted_pointer_table_payload_size_mismatch",
                  "sandbox trusted pointer table payload is not entry-aligned",
                  entry_offset);
    return false;
  }

  if (table.count_ >
      table.size_ / sizeof(RawHeapSandboxTrustedPointerEntry)) {
    ReportCorrupt("sandbox_trusted_pointer_table_entries_truncated",
                  "sandbox trusted pointer table entries exceed table payload",
                  entry_offset);
    return false;
  }

  const uint32_t entries_size =
      table.count_ * sizeof(RawHeapSandboxTrustedPointerEntry);
  if (entries_size != table.size_) {
    ReportCorrupt("sandbox_trusted_pointer_table_payload_count_mismatch",
                  "sandbox trusted pointer table payload size does not match "
                  "entry count",
                  entry_offset);
    return false;
  }

  std::vector<RawHeapSandboxTrustedPointerEntry> entries;
  entries.resize(table.count_);
  if (entries_size != 0 &&
      !reader->ReadDataAt(table.absolute_offset_, entries_size,
                          reinterpret_cast<uint8_t*>(entries.data()),
                          entries_size)) {
    ReportCorrupt("sandbox_trusted_pointer_table_entries_read_failed",
                  "failed to read sandbox trusted pointer table entries",
                  table.absolute_offset_);
    return false;
  }

  std::map<uint32_t, RawHeapSandboxTrustedPointerEntry> decoded_entries;
  for (uint32_t index = 0; index < table.count_; ++index) {
    const RawHeapSandboxTrustedPointerEntry& entry = entries[index];
    switch (entry.state_) {
      case kRawHeapSandboxTrustedPointerEntryActive:
      case kRawHeapSandboxTrustedPointerEntryEmpty:
      case kRawHeapSandboxTrustedPointerEntryZapped:
      case kRawHeapSandboxTrustedPointerEntryUnpublished:
        break;
      default:
        ReportCorrupt("sandbox_trusted_pointer_table_unknown_state",
                      "sandbox trusted pointer table entry has unknown state " +
                          std::to_string(entry.state_),
                      entry_offset);
        return false;
    }
    if (decoded_entries.find(entry.handle_) != decoded_entries.end()) {
      ReportCorrupt("sandbox_trusted_pointer_table_duplicate_handle",
                    "duplicate sandbox trusted pointer handle " +
                        std::to_string(entry.handle_),
                    entry_offset);
      return false;
    }
    decoded_entries.emplace(entry.handle_, entry);
  }

  trusted_pointer_entries_ = std::move(decoded_entries);
  return true;
}
#endif

bool OffheapTableResolver::DecodeJSDispatchTable(
    BinaryReaderBase* reader, const RawHeapOffheapTableSlice& table,
    uint32_t entry_offset) {
  js_dispatch_entries_.clear();

  if (reader == nullptr) {
    ReportFatal("js_dispatch_table_reader_missing",
                "rawheap reader is null while decoding JS dispatch table",
                entry_offset);
    return false;
  }

  if (table.size_ % sizeof(RawHeapJSDispatchEntry) != 0) {
    ReportCorrupt("js_dispatch_table_payload_size_mismatch",
                  "JS dispatch table payload is not entry-aligned",
                  entry_offset);
    return false;
  }

  if (table.count_ >
      table.size_ / sizeof(RawHeapJSDispatchEntry)) {
    ReportCorrupt("js_dispatch_table_entries_truncated",
                  "JS dispatch table entries exceed table payload",
                  entry_offset);
    return false;
  }

  const uint32_t entries_size =
      table.count_ * sizeof(RawHeapJSDispatchEntry);
  if (entries_size != table.size_) {
    ReportCorrupt("js_dispatch_table_payload_count_mismatch",
                  "JS dispatch table payload size does not match entry count",
                  entry_offset);
    return false;
  }

  if (table.count_ == 0) {
    return true;
  }

  std::vector<RawHeapJSDispatchEntry> entries;
  entries.resize(table.count_);
  if (!reader->ReadDataAt(table.absolute_offset_, entries_size,
                          reinterpret_cast<uint8_t*>(entries.data()),
                          entries_size)) {
    ReportCorrupt("js_dispatch_table_entries_read_failed",
                  "failed to read JS dispatch table entries",
                  table.absolute_offset_);
    return false;
  }

  std::map<uint32_t, RawHeapJSDispatchEntry> decoded_entries;
  for (uint32_t index = 0; index < table.count_; ++index) {
    const RawHeapJSDispatchEntry& entry = entries[index];
    switch (entry.state_) {
      case kRawHeapJSDispatchEntryActive:
      case kRawHeapJSDispatchEntryEmpty:
        break;
      default:
        ReportCorrupt("js_dispatch_table_unknown_state",
                      "JS dispatch table entry has unknown state " +
                          std::to_string(entry.state_),
                      entry_offset);
        return false;
    }
    if (decoded_entries.find(entry.handle_) != decoded_entries.end()) {
      ReportCorrupt("js_dispatch_entry_duplicate",
                    "duplicate JS dispatch handle " +
                        std::to_string(entry.handle_),
                    entry_offset);
      return false;
    }
    decoded_entries.emplace(entry.handle_, entry);
  }

  js_dispatch_entries_ = std::move(decoded_entries);
  return true;
}

#if defined(V8_ENABLE_SANDBOX)
bool OffheapTableResolver::DecodeSandboxCodePointerTable(
    BinaryReaderBase* reader, const RawHeapOffheapTableSlice& table,
    uint32_t entry_offset) {
  code_pointer_entries_.clear();

  if (reader == nullptr) {
    ReportFatal("sandbox_code_pointer_table_reader_missing",
                "rawheap reader is null while decoding sandbox code pointer "
                "table",
                entry_offset);
    return false;
  }

  if (table.size_ % sizeof(RawHeapSandboxCodePointerEntry) != 0) {
    ReportCorrupt("sandbox_code_pointer_table_payload_size_mismatch",
                  "sandbox code pointer table payload is not entry-aligned",
                  entry_offset);
    return false;
  }

  if (table.count_ > table.size_ / sizeof(RawHeapSandboxCodePointerEntry)) {
    ReportCorrupt("sandbox_code_pointer_table_entries_truncated",
                  "sandbox code pointer table entries exceed table payload",
                  entry_offset);
    return false;
  }

  const uint32_t entries_size =
      table.count_ * sizeof(RawHeapSandboxCodePointerEntry);
  if (entries_size != table.size_) {
    ReportCorrupt("sandbox_code_pointer_table_payload_count_mismatch",
                  "sandbox code pointer table payload size does not match "
                  "entry count",
                  entry_offset);
    return false;
  }

  if (table.count_ == 0) {
    return true;
  }

  std::vector<RawHeapSandboxCodePointerEntry> entries;
  entries.resize(table.count_);
  if (!reader->ReadDataAt(table.absolute_offset_, entries_size,
                          reinterpret_cast<uint8_t*>(entries.data()),
                          entries_size)) {
    ReportCorrupt("sandbox_code_pointer_table_entries_read_failed",
                  "failed to read sandbox code pointer table entries",
                  table.absolute_offset_);
    return false;
  }

  std::map<uint32_t, RawHeapSandboxCodePointerEntry> decoded_entries;
  for (uint32_t index = 0; index < table.count_; ++index) {
    const RawHeapSandboxCodePointerEntry& entry = entries[index];
    if (entry.handle_ == 0) {
      ReportCorrupt("sandbox_code_pointer_entry_zero_handle",
                    "sandbox code pointer table entry has zero handle",
                    entry_offset);
      return false;
    }
    if (decoded_entries.find(entry.handle_) != decoded_entries.end()) {
      ReportCorrupt("sandbox_code_pointer_entry_duplicate",
                    "duplicate sandbox code pointer handle " +
                        std::to_string(entry.handle_),
                    entry_offset);
      return false;
    }
    decoded_entries.emplace(entry.handle_, entry);
  }

  code_pointer_entries_ = std::move(decoded_entries);
  return true;
}

bool OffheapTableResolver::IsSandboxUnsupportedTableKind(
    RawHeapOffheapTableKind kind) const {
  switch (kind) {
    case RawHeapOffheapTableKind::kSandboxExternalPointerTable:
    case RawHeapOffheapTableKind::kSandboxCppHeapPointerTable:
      return true;
    case RawHeapOffheapTableKind::kWasmCanonicalTypeTable:
    case RawHeapOffheapTableKind::kJSDispatchTable:
    case RawHeapOffheapTableKind::kStringTableReserved:
    case RawHeapOffheapTableKind::kSandboxTrustedPointerTable:
    case RawHeapOffheapTableKind::kSandboxJSDispatchTable:
    case RawHeapOffheapTableKind::kSandboxCodePointerTable:
      return false;
  }
  return false;
}

void OffheapTableResolver::ReportSandboxUnsupportedTableKind(
    uint32_t kind, uint32_t rawheap_offset) {
  ReportWarning("sandbox_offheap_table_decoder_unsupported",
                "ignoring sandbox offheap table kind without decoder " +
                    std::to_string(kind),
                rawheap_offset);
}
#endif

bool OffheapTableResolver::HasTable(RawHeapOffheapTableKind kind) const {
  return tables_.find(kind) != tables_.end();
}

bool OffheapTableResolver::GetTable(RawHeapOffheapTableKind kind,
                                    RawHeapOffheapTableSlice* out) const {
  auto it = tables_.find(kind);
  if (it == tables_.end()) {
    return false;
  }
  if (out != nullptr) {
    *out = it->second;
  }
  return true;
}

const RawHeapWasmCanonicalStructRecord*
OffheapTableResolver::FindWasmCanonicalStruct(uint32_t type_index) const {
  auto it = wasm_struct_layouts_.find(type_index);
  if (it == wasm_struct_layouts_.end()) {
    return nullptr;
  }
  if (it->second.record_index_ >= wasm_struct_records_.size()) {
    return nullptr;
  }
  return &wasm_struct_records_[it->second.record_index_];
}

bool OffheapTableResolver::GetWasmCanonicalStructFields(
    const RawHeapWasmCanonicalStructRecord& record,
    WasmCanonicalStructFieldsView* out) const {
  if (out == nullptr) {
    return false;
  }
  out->data_ = nullptr;
  out->count_ = 0;

  auto it = wasm_struct_layouts_.find(record.type_index_);
  if (it == wasm_struct_layouts_.end()) {
    return false;
  }
  const WasmCanonicalStructLayout& layout = it->second;
  if (layout.record_index_ >= wasm_struct_records_.size() ||
      layout.field_index_ > wasm_struct_fields_.size() ||
      record.field_count_ > wasm_struct_fields_.size() - layout.field_index_) {
    return false;
  }

  out->count_ = record.field_count_;
  if (record.field_count_ != 0) {
    out->data_ = wasm_struct_fields_.data() + layout.field_index_;
  }
  return true;
}

#if defined(V8_ENABLE_SANDBOX)
const RawHeapSandboxTrustedPointerEntry*
OffheapTableResolver::FindTrustedPointer(uint32_t handle,
                                         uint32_t expected_tag) const {
  if (!HasTable(RawHeapOffheapTableKind::kSandboxTrustedPointerTable)) {
    ReportWarning("sandbox_trusted_pointer_table_missing",
                  "sandbox trusted pointer table is missing; handle=" +
                      std::to_string(handle),
                  0);
    return nullptr;
  }

  auto it = trusted_pointer_entries_.find(handle);
  if (it == trusted_pointer_entries_.end()) {
    ReportWarning("sandbox_trusted_pointer_entry_missing",
                  "sandbox trusted pointer entry is missing; handle=" +
                      std::to_string(handle),
                  0);
    return nullptr;
  }

  const RawHeapSandboxTrustedPointerEntry& entry = it->second;
  if (entry.state_ != kRawHeapSandboxTrustedPointerEntryActive) {
    ReportWarning("sandbox_trusted_pointer_entry_inactive",
                  "sandbox trusted pointer entry is not active; handle=" +
                      std::to_string(handle) +
                      " state=" + std::to_string(entry.state_),
                  0);
    return nullptr;
  }
  if (expected_tag != kRawHeapUnknownIndirectPointerTag &&
      entry.indirect_pointer_tag_ != kRawHeapUnknownIndirectPointerTag &&
      entry.indirect_pointer_tag_ != expected_tag) {
    ReportWarning("sandbox_trusted_pointer_tag_mismatch",
                  "sandbox trusted pointer tag mismatch; handle=" +
                      std::to_string(handle),
                  0);
    return nullptr;
  }
  return &entry;
}
#endif

const RawHeapJSDispatchEntry*
OffheapTableResolver::FindJSDispatchCode(uint32_t handle) const {
  if (!HasTable(RawHeapOffheapTableKind::kJSDispatchTable)
#if defined(V8_ENABLE_SANDBOX)
      && !HasTable(RawHeapOffheapTableKind::kSandboxJSDispatchTable)
#endif
  ) {
    ReportWarning("js_dispatch_table_missing",
                  "JS dispatch table is missing; handle=" +
                      std::to_string(handle),
                  0);
    return nullptr;
  }

  auto it = js_dispatch_entries_.find(handle);
  if (it == js_dispatch_entries_.end()) {
    ReportWarning("js_dispatch_entry_missing",
                  "JS dispatch entry is missing; handle=" +
                      std::to_string(handle),
                  0);
    return nullptr;
  }

  const RawHeapJSDispatchEntry& entry = it->second;
  if (entry.state_ != kRawHeapJSDispatchEntryActive) {
    ReportWarning("js_dispatch_entry_inactive",
                  "JS dispatch entry is not active; handle=" +
                      std::to_string(handle) +
                      " state=" + std::to_string(entry.state_),
                  0);
    return nullptr;
  }
  return &entry;
}

#if defined(V8_ENABLE_SANDBOX)
const RawHeapSandboxCodePointerEntry* OffheapTableResolver::FindCodePointer(
    uint32_t handle) const {
  if (!HasTable(RawHeapOffheapTableKind::kSandboxCodePointerTable)) {
    ReportWarning("sandbox_code_pointer_table_missing",
                  "sandbox code pointer table is missing; handle=" +
                      std::to_string(handle),
                  0);
    return nullptr;
  }

  auto it = code_pointer_entries_.find(handle);
  if (it == code_pointer_entries_.end()) {
    ReportWarning("sandbox_code_pointer_entry_missing",
                  "sandbox code pointer entry is missing; handle=" +
                      std::to_string(handle),
                  0);
    return nullptr;
  }

  const RawHeapSandboxCodePointerEntry& entry = it->second;
  if (entry.code_object_address_ == 0) {
    ReportWarning("sandbox_code_pointer_entry_missing_code_object",
                  "sandbox code pointer entry has no observed Code object; "
                  "handle=" +
                      std::to_string(handle),
                  0);
    return nullptr;
  }
  return &entry;
}
#endif

void OffheapTableResolver::ReportFatal(const char* code,
                                       const std::string& message,
                                       uint32_t rawheap_offset) const {
  if (diagnostics_ != nullptr) {
    diagnostics_->Fatal(code, message, kOffheapTableSection, 0, 0, 0, 0,
                        rawheap_offset);
  }
}

void OffheapTableResolver::ReportCorrupt(const char* code,
                                         const std::string& message,
                                         uint32_t rawheap_offset) const {
  if (diagnostics_ != nullptr) {
    diagnostics_->CorruptInput(code, message, kOffheapTableSection, 0, 0, 0, 0,
                               rawheap_offset);
  }
}

void OffheapTableResolver::ReportWarning(const char* code,
                                         const std::string& message,
                                         uint32_t rawheap_offset) const {
  if (diagnostics_ != nullptr) {
    diagnostics_->Warning(code, message, kOffheapTableSection, 0, 0, 0, 0,
                          rawheap_offset);
  }
}
}  // namespace dfx
}  // namespace v8
#endif
