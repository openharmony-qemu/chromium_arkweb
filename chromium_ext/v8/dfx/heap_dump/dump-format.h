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
#if defined(OH_ENABLE_HEAP_DUMP) || defined(OH_ENABLE_HEAP_TRANSLATE)

#include <string>

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
constexpr uint32_t kDumpFormatVersion = 1;

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

  uint32_t root_table_offset_;
  uint32_t root_table_size_;
  uint32_t root_type_count_;

  uint32_t string_table_offset_;
  uint32_t string_table_size_;
  uint32_t string_count_;

  uint32_t object_table_offset_;
  uint32_t object_table_size_;
  uint32_t object_count_;

  uint32_t metadata_offset_;
  uint32_t metadata_size_;
  // ...
};

struct InstructionStreamInfo {
  i::Address object_addr_;
  int8_t mode_;
};

struct StringTableElementInfo {
  i::Address string_address_;
  int len_;
};

void PrintHead(const RawHeapHeader& head);
uint32_t GetCurrentRawHeapFlags();
bool CheckRawHeapFlags(uint32_t flags);
bool IsVersionCompatible(const RawHeapHeader& header);
std::string GetRawHeapFlagsString(uint32_t flags);
// dump map and real size only
bool SkipContentDump(i::Tagged<i::Map> map);

}  // namespace dfx
}  // namespace v8
#endif
#endif
