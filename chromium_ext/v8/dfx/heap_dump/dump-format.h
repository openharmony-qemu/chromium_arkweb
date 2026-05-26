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

#ifndef DUMP_FORMAT_H
#define DUMP_FORMAT_H
#if defined(OH_ENABLE_HEAP_DUMP) || defined(OH_ENABLE_HEAP_TRANSLATE) || \
    defined(OH_ENABLE_HEAP_DUMP_TEST)

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

#include "include/v8-internal.h"
#include "src/heap/heap.h"

namespace v8 {
namespace dfx {

static_assert(sizeof(uintptr_t) == sizeof(i::Address));
static_assert(i::kTaggedSize == sizeof(i::Tagged_t));
static_assert(i::HeapObject::kMapOffset == 0);
static_assert(i::HeapObject::kHeaderSize == sizeof(i::Tagged_t));

constexpr i::Address kRawHeapWeakObjectTag = 0x1;
constexpr uint32_t kNoContentObjectDumpSize =
    static_cast<uint32_t>(sizeof(i::Tagged_t)   // copy map pointer
                          + sizeof(uint32_t));  // real size

constexpr uint64_t kRawHeapMagic = 0x56384844;  // "V8HD"
// Rawheap dump format versions are binary header/layout compatibility points.
// v1: roots/string/object tables only; no offheap table fields and no
// dedicated i18n_support flag bit in the exported flags.
// v2: adds offheap table section fields and exports the independent
// i18n_support flag bit used by flag compatibility checks.
// v3: adds trusted_cage_base_ for sandbox ProtectedPointerSlot decoding.
constexpr uint32_t kDumpFormatVersionV1 = 1;
constexpr uint32_t kDumpFormatVersionWithOffheapTables = 2;
constexpr uint32_t kDumpFormatVersionWithTrustedCageBase = 3;
constexpr uint32_t kDumpFormatVersionCurrent =
    kDumpFormatVersionWithTrustedCageBase;
constexpr uint32_t kDumpFormatVersion = kDumpFormatVersionCurrent;

// File-level envelope for ArkWeb rawheap dumps.
//
// The dump side writes a compact binary image of roots, object payloads and
// optional offheap side tables while the producer isolate is still alive. The
// translate side validates this header before constructing fake V8 objects from
// copied payload bytes. This is intentionally not the DevTools heap snapshot
// JSON format; it is the offline input used to rebuild that graph later.
struct RawHeapHeader {
  uint64_t magic_;
  uint32_t dump_format_version_;
  uint32_t flags_;
  uint64_t timestamp_;

  uint32_t v8_major_version_;
  uint32_t v8_minor_version_;
  uint32_t v8_build_number_;
  uint32_t v8_patch_level_;

  i::Address cage_base_;
  i::Address code_cage_base_;
  i::Address trusted_cage_base_;

  uint32_t root_table_offset_;
  uint32_t root_table_size_;
  uint32_t root_type_count_;

  uint32_t string_table_offset_;
  uint32_t string_table_size_;
  uint32_t string_count_;

  uint32_t object_table_offset_;
  uint32_t object_table_size_;
  uint32_t object_count_;

  uint32_t offheap_table_offset_;
  uint32_t offheap_table_size_;
  uint32_t offheap_table_count_;

  uint32_t metadata_offset_;
  uint32_t metadata_size_;
  // ...
};

// Numbering scheme: values 1..99 are reserved for table kinds shared with
// no-sandbox builds; values 100+ are gated by V8_ENABLE_SANDBOX so no-sandbox
// readers can detect and skip sandbox-only kinds without linking sandbox
// decoders. New kinds must keep this split — the gap between 2 and 100 is
// intentional, not a placeholder.
enum class RawHeapOffheapTableKind : uint32_t {
  // The existing string table section keeps its dedicated header fields.
  kStringTableReserved = 1,
  kWasmCanonicalTypeTable = 2,
  // Current-format shared JSDispatchTable payload. Legacy v1/v2 and older v3
  // dumps may omit it; translators must warn and skip dependent edges.
  kJSDispatchTable = 3,
#if defined(V8_ENABLE_SANDBOX)
  kSandboxTrustedPointerTable = 100,
  kSandboxExternalPointerTable = 101,
  kSandboxCppHeapPointerTable = 102,
  // Legacy kind kept for rawheap v2/v3 sandbox dumps produced before the
  // JSDispatchTable schema was made configuration-independent.
  kSandboxJSDispatchTable = 103,
  kSandboxCodePointerTable = 104,
#endif
};

enum RawHeapJSDispatchEntryState : uint32_t {
  kRawHeapJSDispatchEntryActive = 1u,
  kRawHeapJSDispatchEntryEmpty = 2u,
};

struct RawHeapJSDispatchEntry {
  uint32_t handle_;
  uint32_t state_;
  uint64_t code_object_address_;
  uint64_t entrypoint_;
  uint32_t parameter_count_;
  uint32_t flags_;
};

static_assert(std::is_standard_layout<RawHeapJSDispatchEntry>::value);
static_assert(std::is_trivially_copyable<RawHeapJSDispatchEntry>::value);

#if defined(V8_ENABLE_SANDBOX)
constexpr uint32_t kRawHeapUnknownIndirectPointerTag = 0xffffffffu;

enum RawHeapSandboxTrustedPointerEntryState : uint32_t {
  kRawHeapSandboxTrustedPointerEntryActive = 1u,
  kRawHeapSandboxTrustedPointerEntryEmpty = 2u,
  kRawHeapSandboxTrustedPointerEntryZapped = 3u,
  kRawHeapSandboxTrustedPointerEntryUnpublished = 4u,
};

struct RawHeapSandboxTrustedPointerEntry {
  uint32_t handle_;
  uint32_t indirect_pointer_tag_;
  // Diagnostic-only candidate target. Translators must first prove that this
  // address belongs to the dumped rawheap object table before creating an edge.
  uint64_t trusted_object_address_;
  uint32_t state_;
  uint32_t flags_;
};

using RawHeapSandboxJSDispatchEntry = RawHeapJSDispatchEntry;
constexpr RawHeapJSDispatchEntryState kRawHeapSandboxJSDispatchEntryActive =
    kRawHeapJSDispatchEntryActive;
constexpr RawHeapJSDispatchEntryState kRawHeapSandboxJSDispatchEntryEmpty =
    kRawHeapJSDispatchEntryEmpty;

struct RawHeapSandboxCodePointerEntry {
  uint32_t handle_;
  uint32_t flags_;
  uint64_t code_object_address_;
  // CodePointerTable::IterateActiveEntriesIn exposes the Code object pointer.
  // It does not expose the CodeEntrypointTag required to read the entrypoint,
  // so this field is zero until producer-side code has an approved tag source.
  uint64_t entrypoint_;
};

static_assert(std::is_standard_layout<RawHeapSandboxTrustedPointerEntry>::value);
static_assert(
    std::is_trivially_copyable<RawHeapSandboxTrustedPointerEntry>::value);
static_assert(std::is_standard_layout<RawHeapSandboxCodePointerEntry>::value);
static_assert(
    std::is_trivially_copyable<RawHeapSandboxCodePointerEntry>::value);
#endif

struct RawHeapOffheapTableHeader {
  uint32_t kind_;
  uint32_t offset_;
  uint32_t size_;
  uint32_t count_;
};

constexpr uint32_t kRawHeapInvalidCanonicalIndex = 0xffffffffu;

enum RawHeapWasmCanonicalFieldFlags : uint32_t {
  kRawHeapWasmFieldIsRef = 1u << 0,
  kRawHeapWasmFieldIsNullable = 1u << 1,
  kRawHeapWasmFieldHasIndex = 1u << 2,
  kRawHeapWasmFieldIsShared = 1u << 3,
  kRawHeapWasmFieldIsExact = 1u << 4,
  kRawHeapWasmFieldIsMutable = 1u << 5,
};

enum RawHeapWasmCanonicalStructFlags : uint32_t {
  kRawHeapWasmStructIsDescriptor = 1u << 0,
  kRawHeapWasmStructIsShared = 1u << 1,
};

struct RawHeapWasmCanonicalStructRecord {
  uint32_t type_index_;
  uint32_t field_count_;
  // Offset relative to the start of the kWasmCanonicalTypeTable payload.
  uint32_t fields_offset_;
  uint32_t total_fields_size_;
  uint32_t flags_;
};

struct RawHeapWasmCanonicalStructField {
  // Offset relative to the start of the WasmStruct fields, not the object.
  uint32_t field_offset_;
  uint32_t value_kind_size_;
  uint32_t flags_;
  uint32_t ref_index_;
};

static_assert(std::is_standard_layout<RawHeapOffheapTableHeader>::value);
static_assert(std::is_trivially_copyable<RawHeapOffheapTableHeader>::value);
static_assert(std::is_standard_layout<RawHeapWasmCanonicalStructRecord>::value);
static_assert(
    std::is_trivially_copyable<RawHeapWasmCanonicalStructRecord>::value);
static_assert(std::is_standard_layout<RawHeapWasmCanonicalStructField>::value);
static_assert(
    std::is_trivially_copyable<RawHeapWasmCanonicalStructField>::value);

void PrintHead(const RawHeapHeader& head);
uint32_t GetCurrentRawHeapFlags();
bool HasRawHeapOffheapTableSection(const RawHeapHeader& header);
bool ValidateRawHeapOffheapTableSection(const RawHeapHeader& header,
                                        uint32_t binary_size,
                                        std::string* error = nullptr);
std::string GetRawHeapFlagsString(uint32_t flags);
std::string GetRawHeapFlagsString(const RawHeapHeader& header);
uint32_t GetEffectiveRawHeapFlags(const RawHeapHeader& header);
bool RawHeapFlagsMatchCurrent(const RawHeapHeader& header);
bool RawHeapFlagCompressedPointers(uint32_t flags);
bool RawHeapFlagSharedCage(uint32_t flags);
bool RawHeapFlagExternalCodeSpace(uint32_t flags);
bool RawHeapFlagSandbox(uint32_t flags);
bool RawHeapFlagSmi31BitOn64BitArch(uint32_t flags);
bool RawHeapFlagStaticRoots(uint32_t flags);
bool RawHeapFlagMapPacking(uint32_t flags);
bool RawHeapFlagI18nSupport(uint32_t flags);
bool RawHeapFlagI18nSupport(const RawHeapHeader& header);
uint32_t RawHeapFlagTaggedSize(uint32_t flags);
uint32_t RawHeapFlagHeapObjectTag(uint32_t flags);
uint32_t RawHeapFlagApiSystemPointerSize(uint32_t flags);
// dump map and real size only
bool SkipContentDump(i::Tagged<i::Map> map);
// Check if the rawheap file was created with sandbox enabled
bool IsRawHeapSandboxEnabled(const std::string& file_path);

}  // namespace dfx
}  // namespace v8
#endif
#endif
