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

#ifndef DUMP_OFFHEAP_TABLES_H
#define DUMP_OFFHEAP_TABLES_H

#if defined(OH_ENABLE_HEAP_DUMP)

#include <cstdint>
#include <vector>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/binary-writer-base.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-format.h"
#include "src/heap/heap.h"

namespace v8 {
namespace dfx {

class OffheapTableDumper {
 public:
  OffheapTableDumper(i::Heap* heap, BinaryWriterBase* writer);
  ~OffheapTableDumper() = default;

  void PreVisit();
  void Dump();

  uint32_t OffheapTableDumpSize() const;
  uint32_t OffheapTableCount() const;

 private:
  void Clear();
  bool BuildWasmCanonicalStructTable();
  bool GetWasmCanonicalStructTablePayload(uint32_t* payload_size,
                                          uint32_t* record_count) const;
  bool WriteWasmCanonicalStructTableDirectory(uint32_t* payload_cursor);
  bool DumpWasmCanonicalStructTablePayload();

  bool BuildJSDispatchTable();
  bool GetJSDispatchTablePayload(uint32_t* payload_size,
                                 uint32_t* entry_count) const;
  bool WriteJSDispatchTableDirectory(uint32_t* payload_cursor);
  bool DumpJSDispatchTablePayload(uint32_t payload_size);
#if defined(V8_ENABLE_SANDBOX)
  bool BuildSandboxTrustedPointerTable();
  bool GetSandboxTrustedPointerTablePayload(uint32_t* payload_size,
                                            uint32_t* entry_count) const;
  bool WriteSandboxTrustedPointerTableDirectory(uint32_t* payload_cursor);
  bool DumpSandboxTrustedPointerTablePayload(uint32_t payload_size);

  bool BuildSandboxCodePointerTable();
  bool GetSandboxCodePointerTablePayload(uint32_t* payload_size,
                                         uint32_t* entry_count) const;
  bool WriteSandboxCodePointerTableDirectory(uint32_t* payload_cursor);
  bool DumpSandboxCodePointerTablePayload(uint32_t payload_size);
#endif  // defined(V8_ENABLE_SANDBOX)

  i::Heap* heap_{nullptr};
  BinaryWriterBase* writer_{nullptr};
  std::vector<RawHeapWasmCanonicalStructRecord> wasm_struct_records_;
  std::vector<RawHeapWasmCanonicalStructField> wasm_struct_fields_;
  std::vector<RawHeapJSDispatchEntry> js_dispatch_entries_;
#if defined(V8_ENABLE_SANDBOX)
  std::vector<RawHeapSandboxTrustedPointerEntry> trusted_pointer_entries_;
  std::vector<RawHeapSandboxCodePointerEntry> code_pointer_entries_;
#endif  // defined(V8_ENABLE_SANDBOX)
  uint32_t table_dump_size_{0};
  uint32_t table_count_{0};
};

}  // namespace dfx
}  // namespace v8
#endif
#endif  // DUMP_OFFHEAP_TABLES_H
