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

#if defined(OH_ENABLE_HEAP_DUMP) || defined(OH_ENABLE_HEAP_TRANSLATE)
#include "dump-format.h"

#include "dfx-visitor-list.h"
#include "heap-dump-log.h"
#include "include/v8-version.h"
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
constexpr uint32_t kOneBitMask = 0x1u;
constexpr uint32_t kTwoBitMask = 0x3u;

constexpr uint32_t EncodeField(uint32_t value, uint32_t shift, uint32_t mask) {
  return (value & mask) << shift;
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
  ss << ", tagged_size_log2:"
     << ((flags >> kFlagTaggedSizeLog2Shift) & kTwoBitMask);
  ss << ", heap_object_tag:"
     << ((flags >> kFlagHeapObjectTagShift) & kOneBitMask);
  ss << ", api_system_pointer_size_log2:"
     << ((flags >> kFlagApiSystemPointerSizeLog2Shift) & kTwoBitMask);
  ss << "}";
  return ss.str();
}

void PrintHead(const RawHeapHeader& head) {
  std::stringstream ss;
  ss << "print header_:  ";
  ss << "magic_: 0x" << std::hex << head.magic_ << std::dec;
  ss << ", dump_format_version_: " << head.dump_format_version_;
  ss << ", flags_: 0x" << std::hex << head.flags_ << std::dec << " "
     << GetRawHeapFlagsString(head.flags_);
  ss << ", timestamp_: " << head.timestamp_;
  ss << ", v8_version: " << head.v8_major_version_ << "."
     << head.v8_minor_version_ << "." << head.v8_build_number_ << "."
     << head.v8_patch_level_;
  ss << ", cage_base_: " << head.cage_base_
     << ", code_cage_base_: " << head.code_cage_base_
     << ", root_table_offset_: " << head.root_table_offset_
     << ", root_table_size_: " << head.root_table_size_
     << ", root_type_count_: " << head.root_type_count_
     << ", object_table_offset_: " << head.object_table_offset_
     << ", object_table_size_: " << head.object_table_size_
     << ", object_count_: " << head.object_count_ << "\n";

  LogInfo(ss.str());
}

uint32_t GetCurrentRawHeapFlags() {
  return BuildCurrentRawHeapFlags();
}

bool CheckRawHeapFlags(uint32_t flags) {
  return flags == BuildCurrentRawHeapFlags();
}

bool IsVersionCompatible(const RawHeapHeader& header) {
  if (header.magic_ != kRawHeapMagic) {
    return false;
  }

  if (header.dump_format_version_ != kDumpFormatVersion) {
    return false;
  }

  if (!CheckRawHeapFlags(header.flags_)) {
    return false;
  }

  if (header.timestamp_ == 0) {
    return false;
  }

  if (header.v8_major_version_ != V8_MAJOR_VERSION) {
    return false;
  }

  if (header.v8_minor_version_ != V8_MINOR_VERSION) {
    return false;
  }

  if (header.v8_build_number_ != V8_BUILD_NUMBER) {
    return false;
  }

  if (header.v8_patch_level_ != V8_PATCH_LEVEL) {
    return false;
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

}  // namespace dfx
}  // namespace v8
#endif
