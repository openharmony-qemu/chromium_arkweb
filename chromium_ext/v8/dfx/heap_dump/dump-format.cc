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

#if defined(OH_ENABLE_HEAP_DUMP) || defined(OH_ENABLE_HEAP_TRANSLATE) || \
    defined(OH_ENABLE_HEAP_DUMP_TEST)
#include "dump-format.h"

#include <sstream>

#include "dfx-visitor-list.h"
#include "heap-dump-log.h"
#include "src/base/logging.h"
#include "src/objects/map-inl.h"
#include "src/objects/map.h"

namespace v8 {
namespace dfx {
namespace {

constexpr uint32_t kFlagCompressionPointerShift = 0;
constexpr uint32_t kFlagSharedCageShift = 1;
constexpr uint32_t kFlagExternalCodeSpaceShift = 2;
constexpr uint32_t kFlagSandboxShift = 3;
constexpr uint32_t kFlagSmi31BitOn64BitArchShift = 4;
constexpr uint32_t kFlagStaticRootsShift = 5;
constexpr uint32_t kFlagMapPackingShift = 6;
constexpr uint32_t kFlagTaggedSizeLog2Shift = 7;
constexpr uint32_t kFlagHeapObjectTagShift = 9;
constexpr uint32_t kFlagApiSystemPointerSizeLog2Shift = 10;
constexpr uint32_t kFlagI18nSupportShift = 12;
constexpr uint32_t kOneBitMask = 0x1u;
constexpr uint32_t kTwoBitMask = 0x3u;

constexpr uint32_t EncodeField(uint32_t value, uint32_t shift, uint32_t mask) {
  return (value & mask) << shift;
}

uint32_t DecodeField(uint32_t flags, uint32_t shift, uint32_t mask) {
  return (flags >> shift) & mask;
}

uint32_t SetFlagField(uint32_t flags, uint32_t value, uint32_t shift,
                      uint32_t mask) {
  flags &= ~(mask << shift);
  flags |= EncodeField(value, shift, mask);
  return flags;
}

uint32_t Log2OfPowerOfTwo(uint32_t value) {
  CHECK_NE(value, 0u);
  CHECK_EQ(value & (value - 1), 0u);
  uint32_t result = 0;
  while (value > 1) {
    value >>= 1;
    ++result;
  }
  return result;
}

bool FailWithError(std::string* error, const std::string& message) {
  if (error != nullptr) {
    *error = message;
  }
  return false;
}

uint32_t BuildCurrentRawHeapFlags() {
  uint32_t flags = 0;

  constexpr uint32_t kCompressionPointerEnabled =
#if defined(V8_COMPRESS_POINTERS)
      1;
#else
      0;
#endif
  constexpr uint32_t kSharedCageEnabled =
#if defined(V8_COMPRESS_POINTERS_IN_SHARED_CAGE)
      1;
#else
      0;
#endif
  constexpr uint32_t kExternalCodeSpaceEnabled =
#if defined(V8_EXTERNAL_CODE_SPACE)
      1;
#else
      0;
#endif
  constexpr uint32_t kSandboxEnabled =
#if defined(V8_ENABLE_SANDBOX)
      1;
#else
      0;
#endif
  constexpr uint32_t kSmi31BitEnabled =
#if defined(V8_31BIT_SMIS_ON_64BIT_ARCH)
      1;
#else
      0;
#endif
  constexpr uint32_t kStaticRootsEnabled =
#if defined(V8_STATIC_ROOTS)
      1;
#else
      0;
#endif
  constexpr uint32_t kMapPackingEnabled =
#if defined(V8_MAP_PACKING)
      1;
#else
      0;
#endif
  constexpr uint32_t kI18nSupportEnabled =
#if defined(V8_INTL_SUPPORT)
      1;
#else
      0;
#endif

  flags |= EncodeField(kCompressionPointerEnabled, kFlagCompressionPointerShift,
                       kOneBitMask);
  flags |= EncodeField(kSharedCageEnabled, kFlagSharedCageShift, kOneBitMask);
  flags |= EncodeField(kExternalCodeSpaceEnabled, kFlagExternalCodeSpaceShift,
                       kOneBitMask);
  flags |= EncodeField(kSandboxEnabled, kFlagSandboxShift, kOneBitMask);
  flags |=
      EncodeField(kSmi31BitEnabled, kFlagSmi31BitOn64BitArchShift, kOneBitMask);
  flags |= EncodeField(kStaticRootsEnabled, kFlagStaticRootsShift, kOneBitMask);
  flags |= EncodeField(kMapPackingEnabled, kFlagMapPackingShift, kOneBitMask);
  flags |= EncodeField(kI18nSupportEnabled, kFlagI18nSupportShift,
                       kOneBitMask);

  flags |= EncodeField(static_cast<uint32_t>(i::kTaggedSizeLog2),
                       kFlagTaggedSizeLog2Shift, kTwoBitMask);
  flags |= EncodeField(static_cast<uint32_t>(i::kHeapObjectTag),
                       kFlagHeapObjectTagShift, kOneBitMask);
  flags |= EncodeField(
      Log2OfPowerOfTwo(static_cast<uint32_t>(i::kApiSystemPointerSize)),
      kFlagApiSystemPointerSizeLog2Shift, kTwoBitMask);

  return flags;
}

}  // namespace

std::string GetRawHeapFlagsString(uint32_t flags) {
  std::stringstream ss;
  ss << "{";
  ss << "compression_pointer:"
     << ((flags >> kFlagCompressionPointerShift) & kOneBitMask);
  ss << ", shared_cage:" << ((flags >> kFlagSharedCageShift) & kOneBitMask);
  ss << ", external_code_space:"
     << ((flags >> kFlagExternalCodeSpaceShift) & kOneBitMask);
  ss << ", sandbox:" << ((flags >> kFlagSandboxShift) & kOneBitMask);
  ss << ", smi_31bit_on_64bit_arch:"
     << ((flags >> kFlagSmi31BitOn64BitArchShift) & kOneBitMask);
  ss << ", static_roots:" << ((flags >> kFlagStaticRootsShift) & kOneBitMask);
  ss << ", map_packing:" << ((flags >> kFlagMapPackingShift) & kOneBitMask);
  ss << ", i18n_support:" << ((flags >> kFlagI18nSupportShift) & kOneBitMask);
  ss << ", tagged_size_log2:"
     << ((flags >> kFlagTaggedSizeLog2Shift) & kTwoBitMask);
  ss << ", heap_object_tag:"
     << ((flags >> kFlagHeapObjectTagShift) & kOneBitMask);
  ss << ", api_system_pointer_size_log2:"
     << ((flags >> kFlagApiSystemPointerSizeLog2Shift) & kTwoBitMask);
  ss << "}";
  return ss.str();
}

uint32_t GetEffectiveRawHeapFlags(const RawHeapHeader& header) {
  uint32_t flags = header.flags_;
  if (header.dump_format_version_ == kDumpFormatVersionV1) {
    // Rawheap v1 did not export a dedicated i18n bit. Historical v1 dumps use
    // the sandbox bit as the effective i18n value, so normalize only for flag
    // interpretation/comparison and keep the stored header flags untouched.
    flags = SetFlagField(flags,
                         DecodeField(flags, kFlagSandboxShift, kOneBitMask),
                         kFlagI18nSupportShift, kOneBitMask);
  }
  return flags;
}

std::string GetRawHeapFlagsString(const RawHeapHeader& header) {
  return GetRawHeapFlagsString(GetEffectiveRawHeapFlags(header));
}

bool RawHeapFlagsMatchCurrent(const RawHeapHeader& header) {
  return GetEffectiveRawHeapFlags(header) == GetCurrentRawHeapFlags();
}

void PrintHead(const RawHeapHeader& head) {
  std::stringstream ss;
  ss << "print header_:  ";
  ss << "magic_: 0x" << std::hex << head.magic_ << std::dec;
  ss << ", dump_format_version_: " << head.dump_format_version_;
  ss << ", flags_: 0x" << std::hex << head.flags_ << std::dec << " "
     << GetRawHeapFlagsString(head);
  ss << ", timestamp_: " << head.timestamp_;
  ss << ", v8_version: " << head.v8_major_version_ << "."
     << head.v8_minor_version_ << "." << head.v8_build_number_ << "."
     << head.v8_patch_level_;
  ss << ", cage_base_: " << head.cage_base_
     << ", code_cage_base_: " << head.code_cage_base_
     << ", trusted_cage_base_: " << head.trusted_cage_base_
     << ", root_table_offset_: " << head.root_table_offset_
     << ", root_table_size_: " << head.root_table_size_
     << ", root_type_count_: " << head.root_type_count_
     << ", object_table_offset_: " << head.object_table_offset_
     << ", object_table_size_: " << head.object_table_size_
     << ", object_count_: " << head.object_count_
     << ", offheap_table_offset_: " << head.offheap_table_offset_
     << ", offheap_table_size_: " << head.offheap_table_size_
     << ", offheap_table_count_: " << head.offheap_table_count_
     << ", metadata_offset_: " << head.metadata_offset_
     << ", metadata_size_: " << head.metadata_size_ << "\n";

  LogInfo(ss.str());
}

uint32_t GetCurrentRawHeapFlags() { return BuildCurrentRawHeapFlags(); }

bool RawHeapFlagCompressedPointers(uint32_t flags) {
  return ((flags >> kFlagCompressionPointerShift) & kOneBitMask) != 0;
}

bool RawHeapFlagSharedCage(uint32_t flags) {
  return ((flags >> kFlagSharedCageShift) & kOneBitMask) != 0;
}

bool RawHeapFlagExternalCodeSpace(uint32_t flags) {
  return ((flags >> kFlagExternalCodeSpaceShift) & kOneBitMask) != 0;
}

bool RawHeapFlagSandbox(uint32_t flags) {
  return ((flags >> kFlagSandboxShift) & kOneBitMask) != 0;
}

bool RawHeapFlagSmi31BitOn64BitArch(uint32_t flags) {
  return ((flags >> kFlagSmi31BitOn64BitArchShift) & kOneBitMask) != 0;
}

bool RawHeapFlagStaticRoots(uint32_t flags) {
  return ((flags >> kFlagStaticRootsShift) & kOneBitMask) != 0;
}

bool RawHeapFlagMapPacking(uint32_t flags) {
  return ((flags >> kFlagMapPackingShift) & kOneBitMask) != 0;
}

bool RawHeapFlagI18nSupport(uint32_t flags) {
  return ((flags >> kFlagI18nSupportShift) & kOneBitMask) != 0;
}

bool RawHeapFlagI18nSupport(const RawHeapHeader& header) {
  return RawHeapFlagI18nSupport(GetEffectiveRawHeapFlags(header));
}

uint32_t RawHeapFlagTaggedSize(uint32_t flags) {
  return 1u << ((flags >> kFlagTaggedSizeLog2Shift) & kTwoBitMask);
}

uint32_t RawHeapFlagHeapObjectTag(uint32_t flags) {
  return (flags >> kFlagHeapObjectTagShift) & kOneBitMask;
}

uint32_t RawHeapFlagApiSystemPointerSize(uint32_t flags) {
  return 1u << ((flags >> kFlagApiSystemPointerSizeLog2Shift) & kTwoBitMask);
}

bool HasRawHeapOffheapTableSection(const RawHeapHeader& header) {
  return header.offheap_table_size_ != 0 || header.offheap_table_count_ != 0;
}

bool ValidateRawHeapOffheapTableSection(const RawHeapHeader& header,
                                        uint32_t binary_size,
                                        std::string* error) {
  if (!HasRawHeapOffheapTableSection(header)) {
    return true;
  }

  if (header.object_table_offset_ > binary_size ||
      header.object_table_size_ > binary_size - header.object_table_offset_) {
    return FailWithError(error, "object table extends beyond rawheap file");
  }

  const uint32_t object_table_end =
      header.object_table_offset_ + header.object_table_size_;

  if (header.offheap_table_offset_ == 0) {
    return FailWithError(
        error, "offheap table offset is zero while size or count is nonzero");
  }

  if (header.offheap_table_size_ == 0 || header.offheap_table_count_ == 0) {
    return FailWithError(
        error,
        "offheap table size and count must both be nonzero when present");
  }

  const uint64_t minimum_directory_size =
      static_cast<uint64_t>(header.offheap_table_count_) *
      sizeof(RawHeapOffheapTableHeader);
  if (header.offheap_table_size_ < minimum_directory_size) {
    return FailWithError(
        error, "offheap table cannot contain the declared table headers");
  }

  if (header.offheap_table_offset_ > binary_size ||
      header.offheap_table_size_ > binary_size - header.offheap_table_offset_) {
    return FailWithError(error, "offheap table extends beyond rawheap file");
  }

  if (object_table_end > header.offheap_table_offset_) {
    return FailWithError(error, "offheap table overlaps the object table");
  }

  const uint32_t offheap_table_end =
      header.offheap_table_offset_ + header.offheap_table_size_;
  if (header.metadata_offset_ != 0 &&
      offheap_table_end > header.metadata_offset_) {
    return FailWithError(
        error, "offheap table must appear before metadata without overlap");
  }

  return true;
}

constexpr int kDFXDataOnlyCount =
#define DFX_V(id) +1
    0 DFX_DATA_ONLY_VISITOR_ID_LIST(DFX_V)
        DFX_TORQUE_DATA_ONLY_VISITOR_ID_LIST(DFX_V);
#undef DFX_V

static_assert(kDFXDataOnlyCount == i::VisitorId::kDataOnlyVisitorIdCount);

bool SkipContentDump(i::Tagged<i::Map> map) {
  switch (map->visitor_id()) {
#define DFX_CASE(id) case i::VisitorId::kVisit##id:
    DFX_DATA_ONLY_VISITOR_ID_LIST(DFX_CASE)
    DFX_TORQUE_DATA_ONLY_VISITOR_ID_LIST(DFX_CASE)
    CHECK(i::Map::ObjectFieldsFrom(map->visitor_id()) ==
          i::ObjectFields::kDataOnly);
    return true;

    DFX_HEAP_OBJECT_NO_NEED_DUMP_ID_LIST(DFX_CASE)
    return true;
    default:
      return false;
  }
#undef DFX_CASE
}

bool IsRawHeapSandboxEnabled(const std::string& file_path) {
  FILE* file = fopen(file_path.c_str(), "rb");
  if (!file) {
    return false;
  }

  RawHeapHeader header;
  size_t bytes_read = fread(&header, sizeof(header), 1, file);
  fclose(file);

  if (bytes_read != 1) {
    return false;
  }

  if (header.magic_ != kRawHeapMagic) {
    return false;
  }

  uint32_t sandbox_flag = (header.flags_ >> kFlagSandboxShift) & kOneBitMask;
  return sandbox_flag == 1;
}

}  // namespace dfx
}  // namespace v8
#endif
