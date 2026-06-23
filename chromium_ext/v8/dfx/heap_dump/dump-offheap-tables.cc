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

#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-offheap-tables.h"

#include <limits>
#include <string>
#include <vector>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/heap-dump-log.h"

#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/init/isolate-group.h"
#include "src/sandbox/js-dispatch-table-inl.h"

#if defined(V8_ENABLE_SANDBOX)
#include "src/sandbox/code-pointer-table-inl.h"
#include "src/sandbox/trusted-pointer-table-inl.h"
#endif  // defined(V8_ENABLE_SANDBOX)

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/canonical-types.h"
#include "src/wasm/struct-types.h"
#include "src/wasm/value-type.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace dfx {
namespace {

bool CheckedUint32(uint64_t value, const char* value_name, uint32_t* out) {
  if (value > std::numeric_limits<uint32_t>::max()) {
    LogInfo("OffheapTableDumper: not writing offheap table payload; " +
            std::string(value_name) +
            " exceeds uint32_t: " + std::to_string(value));
    return false;
  }
  *out = static_cast<uint32_t>(value);
  return true;
}

void WriteTableDirectoryEntry(BinaryWriterBase* writer,
                              RawHeapOffheapTableKind kind, uint32_t offset,
                              uint32_t size, uint32_t count) {
  RawHeapOffheapTableHeader header{static_cast<uint32_t>(kind), offset, size,
                                   count};
  writer->WriteBinBlock(reinterpret_cast<const uint8_t*>(&header),
                        sizeof(header));
}

#if V8_ENABLE_WEBASSEMBLY
bool CheckedAddUint64(uint64_t left, uint64_t right, const char* value_name,
                      uint64_t* out) {
  if (left > std::numeric_limits<uint64_t>::max() - right) {
    LogInfo("OffheapTableDumper: not writing kWasmCanonicalTypeTable; " +
            std::string(value_name) + " overflows uint64_t");
    return false;
  }
  *out = left + right;
  return true;
}

uint32_t BuildStructFlags(const i::wasm::CanonicalStructType* struct_type) {
  uint32_t flags = 0;
  if (struct_type->is_descriptor()) {
    flags |= kRawHeapWasmStructIsDescriptor;
  }
  if (struct_type->is_shared()) {
    flags |= kRawHeapWasmStructIsShared;
  }
  return flags;
}

uint32_t BuildFieldFlags(const i::wasm::CanonicalStructType* struct_type,
                         uint32_t field_index,
                         i::wasm::CanonicalValueType field) {
  uint32_t flags = 0;
  if (field.is_ref()) {
    flags |= kRawHeapWasmFieldIsRef;
  }
  if (field.is_nullable()) {
    flags |= kRawHeapWasmFieldIsNullable;
  }
  if (field.has_index()) {
    flags |= kRawHeapWasmFieldHasIndex;
  }
  if (field.is_shared()) {
    flags |= kRawHeapWasmFieldIsShared;
  }
  if (field.is_exact()) {
    flags |= kRawHeapWasmFieldIsExact;
  }
  if (struct_type->mutability(field_index)) {
    flags |= kRawHeapWasmFieldIsMutable;
  }
  return flags;
}
#endif  // V8_ENABLE_WEBASSEMBLY

}  // namespace

OffheapTableDumper::OffheapTableDumper(i::Heap* heap, BinaryWriterBase* writer)
    : heap_(heap), writer_(writer) {}

void OffheapTableDumper::Clear() {
  table_dump_size_ = 0;
  table_count_ = 0;
  wasm_struct_records_.clear();
  wasm_struct_fields_.clear();
  js_dispatch_entries_.clear();
#if defined(V8_ENABLE_SANDBOX)
  trusted_pointer_entries_.clear();
  code_pointer_entries_.clear();
#endif  // defined(V8_ENABLE_SANDBOX)
}

void OffheapTableDumper::PreVisit() {
  Clear();
  BuildWasmCanonicalStructTable();
  BuildJSDispatchTable();
#if defined(V8_ENABLE_SANDBOX)
  BuildSandboxTrustedPointerTable();
  BuildSandboxCodePointerTable();
#endif  // defined(V8_ENABLE_SANDBOX)
}

#if defined(V8_ENABLE_SANDBOX)
// Exports RawHeapOffheapTableKind::kSandboxTrustedPointerTable.
//
// Directory entry:
//   RawHeapOffheapTableHeader{kind=kSandboxTrustedPointerTable,
//                            offset=content_offset, size, count}
//
// Table payload format:
//   RawHeapSandboxTrustedPointerEntry[count]
//
// V8's TrustedPointerTable iteration exposes active handle -> unchecked
// target address, but not the private entry tag/state bits, so dump writes the
// observed active entries with kRawHeapUnknownIndirectPointerTag. Translate
// must still prove trusted_object_address_ is present in the dumped object
// table before using it.
//
// V8 sources:
//   src/sandbox/trusted-pointer-table.{h,inl.h}
//     TrustedPointerTable::IterateActiveEntriesIn().
bool OffheapTableDumper::BuildSandboxTrustedPointerTable() {
  trusted_pointer_entries_.clear();

  if (heap_ == nullptr) {
    LogInfo(
        "OffheapTableDumper: not writing kSandboxTrustedPointerTable; heap "
        "is null");
    return false;
  }

  i::Isolate* isolate = heap_->isolate();
  if (isolate == nullptr) {
    LogInfo(
        "OffheapTableDumper: not writing kSandboxTrustedPointerTable; "
        "isolate is null");
    return false;
  }

  i::TrustedPointerTable::Space* isolate_space = heap_->trusted_pointer_space();
  if (isolate_space == nullptr) {
    LogInfo(
        "OffheapTableDumper: not writing kSandboxTrustedPointerTable; no "
        "trusted pointer table space is available on the heap");
    return false;
  }

  isolate->trusted_pointer_table().IterateActiveEntriesIn(
      isolate_space, [&](i::TrustedPointerHandle handle, i::Address content) {
        RawHeapSandboxTrustedPointerEntry entry{};
        entry.handle_ = handle;
        // TrustedPointerTable::IterateActiveEntriesIn exposes only handle and
        // unchecked target address. The entry tag/state bits are private to V8,
        // so keep the tag unknown and mark only the observed active entries.
        entry.indirect_pointer_tag_ = kRawHeapUnknownIndirectPointerTag;
        entry.trusted_object_address_ = static_cast<uint64_t>(content);
        entry.state_ = kRawHeapSandboxTrustedPointerEntryActive;
        entry.flags_ = 0;
        trusted_pointer_entries_.push_back(entry);
      });

  if (trusted_pointer_entries_.empty()) {
    return true;
  }

  const uint64_t payload_size64 =
      static_cast<uint64_t>(trusted_pointer_entries_.size()) *
      sizeof(RawHeapSandboxTrustedPointerEntry);
  const uint64_t table_dump_size64 =
      static_cast<uint64_t>(sizeof(RawHeapOffheapTableHeader)) + payload_size64;

  uint32_t payload_size = 0;
  uint32_t table_dump_size = 0;
  if (!CheckedUint32(payload_size64, "trusted pointer table payload size",
                     &payload_size) ||
      !CheckedUint32(table_dump_size64, "trusted pointer table dump size",
                     &table_dump_size)) {
    trusted_pointer_entries_.clear();
    return false;
  }

  uint32_t new_table_dump_size = 0;
  const uint64_t combined64 =
      static_cast<uint64_t>(table_dump_size_) + table_dump_size64;
  if (!CheckedUint32(combined64, "combined offheap table dump size",
                     &new_table_dump_size)) {
    trusted_pointer_entries_.clear();
    return false;
  }

  table_dump_size_ = new_table_dump_size;
  table_count_ += 1;
  static_cast<void>(payload_size);
  return true;
}

bool OffheapTableDumper::GetSandboxTrustedPointerTablePayload(
    uint32_t* payload_size, uint32_t* entry_count) const {
  if (trusted_pointer_entries_.empty()) {
    *payload_size = 0;
    *entry_count = 0;
    return true;
  }
  return CheckedUint32(static_cast<uint64_t>(trusted_pointer_entries_.size()) *
                           sizeof(RawHeapSandboxTrustedPointerEntry),
                       "trusted pointer table payload size", payload_size) &&
         CheckedUint32(static_cast<uint64_t>(trusted_pointer_entries_.size()),
                       "trusted pointer table entry count", entry_count);
}

bool OffheapTableDumper::WriteSandboxTrustedPointerTableDirectory(
    uint32_t* payload_cursor) {
  uint32_t payload_size = 0;
  uint32_t entry_count = 0;
  if (!GetSandboxTrustedPointerTablePayload(&payload_size, &entry_count)) {
    return false;
  }
  if (entry_count == 0) {
    return true;
  }
  WriteTableDirectoryEntry(writer_,
                           RawHeapOffheapTableKind::kSandboxTrustedPointerTable,
                           *payload_cursor, payload_size, entry_count);
  *payload_cursor += payload_size;
  return true;
}

bool OffheapTableDumper::DumpSandboxTrustedPointerTablePayload(
    uint32_t payload_size) {
  if (trusted_pointer_entries_.empty()) {
    return true;
  }
  writer_->WriteBinBlock(
      reinterpret_cast<const uint8_t*>(trusted_pointer_entries_.data()),
      payload_size);
  return true;
}

// Exports RawHeapOffheapTableKind::kSandboxCodePointerTable.
//
// Directory entry:
//   RawHeapOffheapTableHeader{kind=kSandboxCodePointerTable,
//                            offset=content_offset, size, count}
//
// Table payload format:
//   RawHeapSandboxCodePointerEntry[count]
//
// V8 currently exposes active handle -> Code object
// address through CodePointerTable::IterateActiveEntriesIn(). It does not
// expose the CodeEntrypointTag needed to read the entrypoint, so entrypoint_ is
// kept zero until there is an approved tag source.
//
// V8 sources:
//   src/sandbox/code-pointer-table.{h,inl.h}
//     CodePointerTable::IterateActiveEntriesIn().
bool OffheapTableDumper::BuildSandboxCodePointerTable() {
  code_pointer_entries_.clear();

#if defined(V8_COMPRESS_POINTERS)
  if (heap_ == nullptr) {
    LogInfo(
        "OffheapTableDumper: not writing kSandboxCodePointerTable; heap "
        "is null");
    return false;
  }

  i::Isolate* isolate = heap_->isolate();
  if (isolate == nullptr) {
    LogInfo(
        "OffheapTableDumper: not writing kSandboxCodePointerTable; "
        "isolate is null");
    return false;
  }

  i::IsolateGroup* isolate_group = i::IsolateGroup::current();
  if (isolate_group == nullptr) {
    LogInfo(
        "OffheapTableDumper: not writing kSandboxCodePointerTable; "
        "IsolateGroup::current() is null");
    return false;
  }

  i::CodePointerTable* cpt = isolate_group->code_pointer_table();
  if (cpt == nullptr) {
    LogInfo(
        "OffheapTableDumper: not writing kSandboxCodePointerTable; "
        "code pointer table is null");
    return false;
  }

  auto append_entries = [&](i::CodePointerTable::Space* space) {
    if (space == nullptr) {
      return;
    }
    cpt->IterateActiveEntriesIn(
        space, [&](i::CodePointerHandle handle, i::Address content) {
          RawHeapSandboxCodePointerEntry entry{};
          entry.handle_ = handle;
          entry.flags_ = 0;
          entry.code_object_address_ = static_cast<uint64_t>(content);
          entry.entrypoint_ = 0;
          code_pointer_entries_.push_back(entry);
        });
  };

  i::ReadOnlyHeap* ro_heap = isolate->read_only_heap();
  if (ro_heap != nullptr) {
    append_entries(ro_heap->code_pointer_space());
  }
  append_entries(heap_->code_pointer_space());

  if (code_pointer_entries_.empty()) {
    return true;
  }

  const uint64_t payload_size64 =
      static_cast<uint64_t>(code_pointer_entries_.size()) *
      sizeof(RawHeapSandboxCodePointerEntry);
  const uint64_t table_dump_size64 =
      static_cast<uint64_t>(sizeof(RawHeapOffheapTableHeader)) + payload_size64;

  uint32_t payload_size = 0;
  uint32_t table_dump_size = 0;
  if (!CheckedUint32(payload_size64, "code pointer table payload size",
                     &payload_size) ||
      !CheckedUint32(table_dump_size64, "code pointer table dump size",
                     &table_dump_size)) {
    code_pointer_entries_.clear();
    return false;
  }

  uint32_t new_table_dump_size = 0;
  const uint64_t combined64 =
      static_cast<uint64_t>(table_dump_size_) + table_dump_size64;
  if (!CheckedUint32(combined64, "combined offheap table dump size",
                     &new_table_dump_size)) {
    code_pointer_entries_.clear();
    return false;
  }

  table_dump_size_ = new_table_dump_size;
  table_count_ += 1;
  static_cast<void>(payload_size);
  return true;
#else
  return true;
#endif  // defined(V8_COMPRESS_POINTERS)
}

bool OffheapTableDumper::GetSandboxCodePointerTablePayload(
    uint32_t* payload_size, uint32_t* entry_count) const {
  if (code_pointer_entries_.empty()) {
    *payload_size = 0;
    *entry_count = 0;
    return true;
  }
  return CheckedUint32(static_cast<uint64_t>(code_pointer_entries_.size()) *
                           sizeof(RawHeapSandboxCodePointerEntry),
                       "code pointer table payload size", payload_size) &&
         CheckedUint32(static_cast<uint64_t>(code_pointer_entries_.size()),
                       "code pointer table entry count", entry_count);
}

bool OffheapTableDumper::WriteSandboxCodePointerTableDirectory(
    uint32_t* payload_cursor) {
  uint32_t payload_size = 0;
  uint32_t entry_count = 0;
  if (!GetSandboxCodePointerTablePayload(&payload_size, &entry_count)) {
    return false;
  }
  if (entry_count == 0) {
    return true;
  }
  WriteTableDirectoryEntry(writer_,
                           RawHeapOffheapTableKind::kSandboxCodePointerTable,
                           *payload_cursor, payload_size, entry_count);
  *payload_cursor += payload_size;
  return true;
}

bool OffheapTableDumper::DumpSandboxCodePointerTablePayload(
    uint32_t payload_size) {
  if (code_pointer_entries_.empty()) {
    return true;
  }
  writer_->WriteBinBlock(
      reinterpret_cast<const uint8_t*>(code_pointer_entries_.data()),
      payload_size);
  return true;
}
#endif  // defined(V8_ENABLE_SANDBOX)

// Exports RawHeapOffheapTableKind::kJSDispatchTable.
//
// Directory entry:
//   RawHeapOffheapTableHeader{kind=kJSDispatchTable,
//                            offset=content_offset, size, count}
//
// Table payload format:
//   RawHeapJSDispatchEntry[count]
//
// The JS dispatch table gives the JSFunction code indirection path in both
// sandbox and no-sandbox builds: handle, active state, target Code object
// address, entrypoint, and parameter count. Unlike CodePointerTable,
// JSDispatchTable exposes all fields currently needed by the rawheap schema.
//
// V8 sources:
//   src/sandbox/js-dispatch-table.{h,inl.h}
//     JSDispatchTable::IterateActiveEntriesIn(), GetCode(), GetEntrypoint(),
//     GetParameterCount().
bool OffheapTableDumper::BuildJSDispatchTable() {
  js_dispatch_entries_.clear();

  if (heap_ == nullptr) {
    LogInfo(
        "OffheapTableDumper: not writing kJSDispatchTable; heap "
        "is null");
    return false;
  }

  i::Isolate* isolate = heap_->isolate();
  if (isolate == nullptr) {
    LogInfo(
        "OffheapTableDumper: not writing kJSDispatchTable; "
        "isolate is null");
    return false;
  }

  // IsolateGroup::current() can return null in diagnostic contexts (e.g. when
  // called outside of an active isolate group during a snapshot walk).
  i::IsolateGroup* isolate_group = i::IsolateGroup::current();
  if (isolate_group == nullptr) {
    LogInfo(
        "OffheapTableDumper: not writing kJSDispatchTable; "
        "IsolateGroup::current() is null");
    return false;
  }
  i::JSDispatchTable* jdt = isolate_group->js_dispatch_table();
  if (jdt == nullptr) {
    LogInfo(
        "OffheapTableDumper: not writing kJSDispatchTable; "
        "js_dispatch_table is null");
    return false;
  }

  i::JSDispatchTable::Space* heap_space = heap_->js_dispatch_table_space();
  if (heap_space != nullptr) {
    jdt->IterateActiveEntriesIn(heap_space, [&](i::JSDispatchHandle handle) {
      RawHeapJSDispatchEntry entry{};
      entry.handle_ = handle.value();
      entry.state_ = kRawHeapJSDispatchEntryActive;
      // GetCode() returns Tagged<Code>; .address() strips kHeapObjectTag,
      // giving the untagged HeapObject address that FindObjectHead expects.
      entry.code_object_address_ =
          static_cast<uint64_t>(jdt->GetCode(handle)->address());
      entry.entrypoint_ = static_cast<uint64_t>(jdt->GetEntrypoint(handle));
      entry.parameter_count_ =
          static_cast<uint32_t>(jdt->GetParameterCount(handle));
      entry.flags_ = 0;
      js_dispatch_entries_.push_back(entry);
    });
  }

  i::ReadOnlyHeap* ro_heap = isolate->read_only_heap();
  if (ro_heap != nullptr) {
    i::JSDispatchTable::Space* ro_space = ro_heap->js_dispatch_table_space();
    if (ro_space != nullptr) {
      jdt->IterateActiveEntriesIn(ro_space, [&](i::JSDispatchHandle handle) {
        RawHeapJSDispatchEntry entry{};
        entry.handle_ = handle.value();
        entry.state_ = kRawHeapJSDispatchEntryActive;
        entry.code_object_address_ =
            static_cast<uint64_t>(jdt->GetCode(handle)->address());
        entry.entrypoint_ = static_cast<uint64_t>(jdt->GetEntrypoint(handle));
        entry.parameter_count_ =
            static_cast<uint32_t>(jdt->GetParameterCount(handle));
        entry.flags_ = 0;
        js_dispatch_entries_.push_back(entry);
      });
    }
  }

  if (js_dispatch_entries_.empty()) {
    return true;
  }

  const uint64_t payload_size64 =
      static_cast<uint64_t>(js_dispatch_entries_.size()) *
      sizeof(RawHeapJSDispatchEntry);
  const uint64_t table_dump_size64 =
      static_cast<uint64_t>(sizeof(RawHeapOffheapTableHeader)) + payload_size64;

  uint32_t payload_size = 0;
  uint32_t table_dump_size = 0;
  if (!CheckedUint32(payload_size64, "JS dispatch table payload size",
                     &payload_size) ||
      !CheckedUint32(table_dump_size64, "JS dispatch table dump size",
                     &table_dump_size)) {
    js_dispatch_entries_.clear();
    return false;
  }

  uint32_t new_table_dump_size = 0;
  const uint64_t combined64 =
      static_cast<uint64_t>(table_dump_size_) + table_dump_size64;
  if (!CheckedUint32(combined64, "combined offheap table dump size",
                     &new_table_dump_size)) {
    js_dispatch_entries_.clear();
    return false;
  }

  table_dump_size_ = new_table_dump_size;
  table_count_ += 1;
  static_cast<void>(payload_size);
  return true;
}

bool OffheapTableDumper::GetJSDispatchTablePayload(
    uint32_t* payload_size, uint32_t* entry_count) const {
  if (js_dispatch_entries_.empty()) {
    *payload_size = 0;
    *entry_count = 0;
    return true;
  }
  return CheckedUint32(static_cast<uint64_t>(js_dispatch_entries_.size()) *
                           sizeof(RawHeapJSDispatchEntry),
                       "JS dispatch table payload size", payload_size) &&
         CheckedUint32(static_cast<uint64_t>(js_dispatch_entries_.size()),
                       "JS dispatch table entry count", entry_count);
}

bool OffheapTableDumper::WriteJSDispatchTableDirectory(
    uint32_t* payload_cursor) {
  uint32_t payload_size = 0;
  uint32_t entry_count = 0;
  if (!GetJSDispatchTablePayload(&payload_size, &entry_count)) {
    return false;
  }
  if (entry_count == 0) {
    return true;
  }
  WriteTableDirectoryEntry(writer_, RawHeapOffheapTableKind::kJSDispatchTable,
                           *payload_cursor, payload_size, entry_count);
  *payload_cursor += payload_size;
  return true;
}

bool OffheapTableDumper::DumpJSDispatchTablePayload(
    uint32_t payload_size) {
  if (js_dispatch_entries_.empty()) {
    return true;
  }
  writer_->WriteBinBlock(
      reinterpret_cast<const uint8_t*>(js_dispatch_entries_.data()),
      payload_size);
  return true;
}

// Exports RawHeapOffheapTableKind::kWasmCanonicalTypeTable.
//
// Directory entry:
//   RawHeapOffheapTableHeader{kind=kWasmCanonicalTypeTable,
//                            offset=content_offset, size, count=record_count}
//
// Table payload format:
//   RawHeapWasmCanonicalStructRecord[record_count]
//   RawHeapWasmCanonicalStructField[field_count_sum]
//
// RawHeapOffheapTableHeader lives in the offheap section directory. It
// describes this table's payload byte range with:
//   content_offset = offheap_table_offset_ + header.offset_
//   table_size = header.size_
//   record_count = header.count_
// The RawHeapWasmCanonicalStructRecord array starts at content_offset, so
// record i is:
//   content_offset + i * sizeof(RawHeapWasmCanonicalStructRecord)
//
// A record's trailing field slice is not found by walking past that record.
// Instead, fields_offset_ is an offset from content_offset. The first field for
// the record is:
//   content_offset + record.fields_offset_
// and the slice length is record.field_count_. total_fields_size_ is the
// runtime WasmStruct field-byte size, not the serialized field descriptor byte
// count.
//
// Each record is keyed by the canonical wasm type index stored on the object's
// WasmTypeInfo. Each field records a field_offset_ relative to the start of
// WasmStruct fields, i.e. the same raw offset passed to WasmStruct::RawField(),
// not an object-relative offset.
//
// V8 sources:
//   src/wasm/struct-types.h:
//     wasm::CanonicalStructType, field_count(), field_offset(),
//     total_fields_size(), field(), mutability(), is_descriptor(), is_shared().
//   src/wasm/canonical-types.{h,cc}:
//     wasm::TypeCanonicalizer::LookupStruct().
//   src/wasm/wasm-objects.{h,inl,cc}:
//     WasmTypeInfo::type_index(), WasmStruct::RawField(),
//     WasmStruct::GcSafeType(), WasmStruct::Size().
//   src/objects/objects-body-descriptors-inl.h:
//     WasmStruct::BodyDescriptor::IterateBody(), the GC-side reference
//     traversal this offline table mirrors.
bool OffheapTableDumper::BuildWasmCanonicalStructTable() {
#if V8_ENABLE_WEBASSEMBLY
  // V8's live heap snapshot can ask the process-global TypeCanonicalizer for a
  // WasmStruct layout while extracting references. Offline translate cannot,
  // so dump stores the stable part it needs: type_index -> ref field offsets.
  if (heap_ == nullptr) {
    LogInfo(
        "OffheapTableDumper: not writing kWasmCanonicalTypeTable; heap is "
        "null");
    return false;
  }

  i::wasm::TypeCanonicalizer* canonicalizer = i::wasm::GetTypeCanonicalizer();
  if (canonicalizer == nullptr) {
    return true;
  }

  uint32_t type_count = 0;
  if (!CheckedUint32(
          static_cast<uint64_t>(canonicalizer->GetCurrentNumberOfTypes()),
          "canonical type count", &type_count)) {
    Clear();
    return false;
  }

  std::vector<uint32_t> struct_type_indices;
  uint64_t total_field_count = 0;
  for (uint32_t type_index_value = 0; type_index_value < type_count;
       ++type_index_value) {
    i::wasm::CanonicalTypeIndex type_index{type_index_value};
    if (!canonicalizer->IsStruct(type_index)) {
      continue;
    }

    const i::wasm::CanonicalStructType* struct_type =
        canonicalizer->LookupStruct(type_index);
    if (struct_type == nullptr) {
      LogInfo(
          "OffheapTableDumper: not writing kWasmCanonicalTypeTable; "
          "LookupStruct returned null for type index " +
          std::to_string(type_index_value));
      Clear();
      return false;
    }

    if (!CheckedAddUint64(total_field_count, struct_type->field_count(),
                          "total Wasm canonical struct field count",
                          &total_field_count)) {
      Clear();
      return false;
    }
    struct_type_indices.push_back(type_index_value);
  }

  if (struct_type_indices.empty()) {
    return true;
  }

  const uint64_t records_size64 =
      static_cast<uint64_t>(struct_type_indices.size()) *
      sizeof(RawHeapWasmCanonicalStructRecord);
  const uint64_t fields_size64 =
      total_field_count * sizeof(RawHeapWasmCanonicalStructField);
  uint64_t payload_size64 = 0;
  if (!CheckedAddUint64(records_size64, fields_size64,
                        "Wasm canonical type table payload size",
                        &payload_size64)) {
    Clear();
    return false;
  }

  uint64_t table_dump_size64 = 0;
  if (!CheckedAddUint64(sizeof(RawHeapOffheapTableHeader), payload_size64,
                        "Wasm canonical type table dump size",
                        &table_dump_size64)) {
    Clear();
    return false;
  }

  uint32_t records_size = 0;
  uint32_t payload_size = 0;
  uint32_t table_dump_size = 0;
  if (!CheckedUint32(records_size64, "Wasm canonical struct records size",
                     &records_size) ||
      !CheckedUint32(payload_size64, "Wasm canonical type table payload size",
                     &payload_size) ||
      !CheckedUint32(table_dump_size64, "Wasm canonical type table dump size",
                     &table_dump_size)) {
    Clear();
    return false;
  }

  wasm_struct_records_.reserve(struct_type_indices.size());
  wasm_struct_fields_.reserve(static_cast<size_t>(total_field_count));
  for (uint32_t type_index_value : struct_type_indices) {
    i::wasm::CanonicalTypeIndex type_index{type_index_value};
    const i::wasm::CanonicalStructType* struct_type =
        canonicalizer->LookupStruct(type_index);
    const uint64_t fields_offset64 =
        static_cast<uint64_t>(records_size) +
        static_cast<uint64_t>(wasm_struct_fields_.size()) *
            sizeof(RawHeapWasmCanonicalStructField);
    uint32_t fields_offset = 0;
    if (!CheckedUint32(fields_offset64, "Wasm canonical struct fields offset",
                       &fields_offset)) {
      Clear();
      return false;
    }

    wasm_struct_records_.push_back(RawHeapWasmCanonicalStructRecord{
        type_index_value, struct_type->field_count(), fields_offset,
        struct_type->total_fields_size(), BuildStructFlags(struct_type)});

    for (uint32_t field_index = 0; field_index < struct_type->field_count();
         ++field_index) {
      i::wasm::CanonicalValueType field = struct_type->field(field_index);
      wasm_struct_fields_.push_back(RawHeapWasmCanonicalStructField{
          struct_type->field_offset(field_index),
          static_cast<uint32_t>(field.value_kind_size()),
          BuildFieldFlags(struct_type, field_index, field),
          field.has_index() ? field.ref_index().index
                            : kRawHeapInvalidCanonicalIndex});
    }
  }

  table_count_ += 1;
  table_dump_size_ = table_dump_size;
  static_cast<void>(payload_size);
  return true;
#else
  return true;
#endif  // V8_ENABLE_WEBASSEMBLY
}

bool OffheapTableDumper::GetWasmCanonicalStructTablePayload(
    uint32_t* payload_size, uint32_t* record_count) const {
  if (wasm_struct_records_.empty()) {
    *payload_size = 0;
    *record_count = 0;
    return true;
  }
  const uint64_t records_size64 =
      static_cast<uint64_t>(wasm_struct_records_.size()) *
      sizeof(RawHeapWasmCanonicalStructRecord);
  const uint64_t fields_size64 =
      static_cast<uint64_t>(wasm_struct_fields_.size()) *
      sizeof(RawHeapWasmCanonicalStructField);
  uint64_t payload_size64 = 0;
  if (!CheckedAddUint64(records_size64, fields_size64,
                        "Wasm canonical type table payload size",
                        &payload_size64)) {
    return false;
  }
  return CheckedUint32(payload_size64, "Wasm canonical type table payload size",
                       payload_size) &&
         CheckedUint32(static_cast<uint64_t>(wasm_struct_records_.size()),
                       "Wasm canonical struct record count", record_count);
}

bool OffheapTableDumper::WriteWasmCanonicalStructTableDirectory(
    uint32_t* payload_cursor) {
  uint32_t payload_size = 0;
  uint32_t record_count = 0;
  if (!GetWasmCanonicalStructTablePayload(&payload_size, &record_count)) {
    return false;
  }
  if (record_count == 0) {
    return true;
  }
  WriteTableDirectoryEntry(writer_,
                           RawHeapOffheapTableKind::kWasmCanonicalTypeTable,
                           *payload_cursor, payload_size, record_count);
  *payload_cursor += payload_size;
  return true;
}

bool OffheapTableDumper::DumpWasmCanonicalStructTablePayload() {
  if (wasm_struct_records_.empty()) {
    return true;
  }

  uint32_t records_size = 0;
  if (!CheckedUint32(static_cast<uint64_t>(wasm_struct_records_.size()) *
                         sizeof(RawHeapWasmCanonicalStructRecord),
                     "Wasm canonical struct records size", &records_size)) {
    return false;
  }
  writer_->WriteBinBlock(
      reinterpret_cast<const uint8_t*>(wasm_struct_records_.data()),
      records_size);

  if (!wasm_struct_fields_.empty()) {
    uint32_t fields_size = 0;
    if (!CheckedUint32(static_cast<uint64_t>(wasm_struct_fields_.size()) *
                           sizeof(RawHeapWasmCanonicalStructField),
                       "Wasm canonical struct fields size", &fields_size)) {
      return false;
    }
    writer_->WriteBinBlock(
        reinterpret_cast<const uint8_t*>(wasm_struct_fields_.data()),
        fields_size);
  }
  return true;
}

void OffheapTableDumper::Dump() {
  if (table_count_ == 0) {
    return;
  }
  if (writer_ == nullptr) {
    LogInfo("OffheapTableDumper: writer is null");
    return;
  }

  // The section layout: all directory entries first, then all payloads.
  //
  // RawHeapHeader points at the whole section. Each directory entry identifies
  // one side table by kind and gives an offset relative to the section start.
  // This keeps future sandbox/Wasm tables appendable without changing the root
  // or object table layouts.
  // directory_size = table_count_ * sizeof(RawHeapOffheapTableHeader).
  // Each table's offset_ field is relative to the section start and must be
  // >= directory_size so payloads never overlap the directory.
  const uint32_t directory_size =
      table_count_ * static_cast<uint32_t>(sizeof(RawHeapOffheapTableHeader));
  uint32_t payload_cursor = directory_size;

  uint32_t jdt_payload_size = 0;
  uint32_t unused_entry_count = 0;
  if (!GetJSDispatchTablePayload(&jdt_payload_size, &unused_entry_count)) {
    return;
  }
#if defined(V8_ENABLE_SANDBOX)
  uint32_t trusted_pointer_payload_size = 0;
  uint32_t code_pointer_payload_size = 0;
  if (!GetSandboxTrustedPointerTablePayload(&trusted_pointer_payload_size,
                                            &unused_entry_count) ||
      !GetSandboxCodePointerTablePayload(&code_pointer_payload_size,
                                         &unused_entry_count)) {
    return;
  }
#endif  // defined(V8_ENABLE_SANDBOX)

  // === Pass 1: write directory entries ===
  if (!WriteWasmCanonicalStructTableDirectory(&payload_cursor)) {
    return;
  }
  if (!WriteJSDispatchTableDirectory(&payload_cursor)) {
    return;
  }
#if defined(V8_ENABLE_SANDBOX)
  if (!WriteSandboxTrustedPointerTableDirectory(&payload_cursor) ||
      !WriteSandboxCodePointerTableDirectory(&payload_cursor)) {
    return;
  }
#endif  // defined(V8_ENABLE_SANDBOX)

  // === Pass 2: write payloads ===
  if (!DumpWasmCanonicalStructTablePayload()) {
    return;
  }
  if (!DumpJSDispatchTablePayload(jdt_payload_size)) {
    return;
  }
#if defined(V8_ENABLE_SANDBOX)
  if (!DumpSandboxTrustedPointerTablePayload(trusted_pointer_payload_size) ||
      !DumpSandboxCodePointerTablePayload(code_pointer_payload_size)) {
    return;
  }
#endif  // defined(V8_ENABLE_SANDBOX)
}

uint32_t OffheapTableDumper::OffheapTableDumpSize() const {
  return table_dump_size_;
}

uint32_t OffheapTableDumper::OffheapTableCount() const { return table_count_; }

}  // namespace dfx
}  // namespace v8
#endif
