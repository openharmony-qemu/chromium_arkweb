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

#ifndef TRANSLATE_OFFHEAP_TABLES_H
#define TRANSLATE_OFFHEAP_TABLES_H

#if (defined(OH_ENABLE_HEAP_TRANSLATE) || defined(OH_ENABLE_HEAP_DUMP_TEST))

#include <map>
#include <string>
#include <vector>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-format.h"

namespace v8 {
namespace dfx {
class BinaryReaderBase;
class TranslateDiagnostics;

struct RawHeapOffheapTableSlice {
  uint32_t absolute_offset_{0};
  uint32_t size_{0};
  uint32_t count_{0};
};

struct WasmCanonicalStructFieldsView {
  const RawHeapWasmCanonicalStructField* data_{nullptr};
  uint32_t count_{0};
};

class OffheapTableResolver {
 public:
  OffheapTableResolver(BinaryReaderBase* reader, const RawHeapHeader& header,
                       TranslateDiagnostics* diagnostics = nullptr);
  ~OffheapTableResolver() = default;

  bool IsValid() const { return is_valid_; }
  bool HasTable(RawHeapOffheapTableKind kind) const;
  bool GetTable(RawHeapOffheapTableKind kind,
                RawHeapOffheapTableSlice* out) const;
  const RawHeapWasmCanonicalStructRecord* FindWasmCanonicalStruct(
      uint32_t type_index) const;
  bool GetWasmCanonicalStructFields(
      const RawHeapWasmCanonicalStructRecord& record,
      WasmCanonicalStructFieldsView* out) const;
  const RawHeapJSDispatchEntry* FindJSDispatchCode(uint32_t handle) const;
#if defined(V8_ENABLE_SANDBOX)
  const RawHeapSandboxTrustedPointerEntry* FindTrustedPointer(
      uint32_t handle, uint32_t expected_tag) const;
  const RawHeapSandboxCodePointerEntry* FindCodePointer(uint32_t handle) const;
#endif

 private:
  struct WasmCanonicalStructLayout {
    uint32_t record_index_{0};
    uint32_t field_index_{0};
  };

  struct SupportedTableRange {
    uint32_t start_{0};
    uint32_t end_{0};
    uint32_t kind_{0};
    uint32_t entry_offset_{0};
  };

  bool Init(BinaryReaderBase* reader, const RawHeapHeader& header);
  bool ReadDirectoryEntry(BinaryReaderBase* reader, uint32_t entry_offset,
                          RawHeapOffheapTableHeader* table_header);
  bool ValidateAndStoreTable(BinaryReaderBase* reader,
                             const RawHeapHeader& header,
                             const RawHeapOffheapTableHeader& table_header,
                             uint32_t entry_offset, uint32_t directory_size);
  bool DecodeWasmCanonicalTypeTable(BinaryReaderBase* reader,
                                    const RawHeapOffheapTableSlice& table,
                                    uint32_t entry_offset);
  bool DecodeJSDispatchTable(BinaryReaderBase* reader,
                             const RawHeapOffheapTableSlice& table,
                             uint32_t entry_offset);
  bool IsSupportedTableKind(RawHeapOffheapTableKind kind) const;
#if defined(V8_ENABLE_SANDBOX)
  bool DecodeSandboxTrustedPointerTable(BinaryReaderBase* reader,
                                        const RawHeapOffheapTableSlice& table,
                                        uint32_t entry_offset);
  bool DecodeSandboxCodePointerTable(BinaryReaderBase* reader,
                                     const RawHeapOffheapTableSlice& table,
                                     uint32_t entry_offset);
  bool IsSandboxUnsupportedTableKind(RawHeapOffheapTableKind kind) const;
  void ReportSandboxUnsupportedTableKind(uint32_t kind,
                                         uint32_t rawheap_offset);
#endif
  void ReportFatal(const char* code, const std::string& message,
                   uint32_t rawheap_offset) const;
  void ReportCorrupt(const char* code, const std::string& message,
                     uint32_t rawheap_offset) const;
  void ReportWarning(const char* code, const std::string& message,
                     uint32_t rawheap_offset) const;

  TranslateDiagnostics* diagnostics_{nullptr};
  std::map<RawHeapOffheapTableKind, RawHeapOffheapTableSlice> tables_;
  std::map<uint32_t, uint32_t> seen_kinds_;
  std::vector<SupportedTableRange> supported_table_ranges_;
  std::vector<RawHeapWasmCanonicalStructRecord> wasm_struct_records_;
  std::vector<RawHeapWasmCanonicalStructField> wasm_struct_fields_;
  std::map<uint32_t, WasmCanonicalStructLayout> wasm_struct_layouts_;
  std::map<uint32_t, RawHeapJSDispatchEntry> js_dispatch_entries_;
#if defined(V8_ENABLE_SANDBOX)
  std::map<uint32_t, RawHeapSandboxTrustedPointerEntry>
      trusted_pointer_entries_;
  std::map<uint32_t, RawHeapSandboxCodePointerEntry> code_pointer_entries_;
#endif
  bool is_valid_{true};
};
}  // namespace dfx
}  // namespace v8
#endif
#endif
