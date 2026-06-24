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

#if defined(OH_ENABLE_HEAP_TRANSLATE) || defined(OH_ENABLE_HEAP_DUMP_TEST)

#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-header.h"

#include <cstddef>
#include <utility>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/binary-reader-base.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-diagnostics.h"
#include "include/v8-version.h"
#include "src/common/ptr-compr-inl.h"
#include "src/common/ptr-compr.h"

namespace v8 {
namespace dfx {
namespace {

struct RawHeapHeaderPrefix {
  uint64_t magic_;
  uint32_t dump_format_version_;
};

struct RawHeapHeaderV1 {
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
};

struct RawHeapHeaderV2 {
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

  uint32_t offheap_table_offset_;
  uint32_t offheap_table_size_;
  uint32_t offheap_table_count_;

  uint32_t metadata_offset_;
  uint32_t metadata_size_;
};

static_assert(sizeof(RawHeapHeaderV1) < sizeof(RawHeapHeaderV2));
static_assert(sizeof(RawHeapHeaderV2) < sizeof(RawHeapHeader));
static_assert(sizeof(RawHeapHeaderPrefix) < sizeof(RawHeapHeaderV1));
static_assert(offsetof(RawHeapHeaderPrefix, magic_) ==
              offsetof(RawHeapHeader, magic_));
static_assert(offsetof(RawHeapHeaderPrefix, dump_format_version_) ==
              offsetof(RawHeapHeader, dump_format_version_));

bool FailWithError(std::string* error, const std::string& message) {
  if (error != nullptr) {
    *error = message;
  }
  return false;
}

void Fatal(TranslateDiagnostics* diagnostics, std::string code,
           std::string message, std::string section,
           uint32_t rawheap_offset = 0) {
  if (diagnostics != nullptr) {
    diagnostics->Fatal(std::move(code), std::move(message), std::move(section),
                       0, 0, 0, 0, rawheap_offset);
  }
}

std::string FormatMagicMismatch(uint64_t actual) {
  return "rawheap magic mismatch: actual " + std::to_string(actual) +
         ", expected " + std::to_string(kRawHeapMagic);
}

std::string FormatUnsupportedDumpVersion(uint32_t actual) {
  return "unsupported rawheap dump format version " + std::to_string(actual) +
         ", supported versions are " +
         GetRawHeapDumpFormatVersionsForTranslate();
}

uint32_t SectionEnd(uint32_t offset, uint32_t size) { return offset + size; }

void MigrateRawHeapHeaderV1(const RawHeapHeaderV1& header_v1,
                            RawHeapHeader* header) {
  *header = {};
  header->magic_ = header_v1.magic_;
  header->dump_format_version_ = header_v1.dump_format_version_;
  header->flags_ = header_v1.flags_;
  header->timestamp_ = header_v1.timestamp_;
  header->v8_major_version_ = header_v1.v8_major_version_;
  header->v8_minor_version_ = header_v1.v8_minor_version_;
  header->v8_build_number_ = header_v1.v8_build_number_;
  header->v8_patch_level_ = header_v1.v8_patch_level_;
  header->cage_base_ = header_v1.cage_base_;
  header->code_cage_base_ = header_v1.code_cage_base_;
  header->trusted_cage_base_ = 0;
  header->root_table_offset_ = header_v1.root_table_offset_;
  header->root_table_size_ = header_v1.root_table_size_;
  header->root_type_count_ = header_v1.root_type_count_;
  header->string_table_offset_ = header_v1.string_table_offset_;
  header->string_table_size_ = header_v1.string_table_size_;
  header->string_count_ = header_v1.string_count_;
  header->object_table_offset_ = header_v1.object_table_offset_;
  header->object_table_size_ = header_v1.object_table_size_;
  header->object_count_ = header_v1.object_count_;

  header->offheap_table_offset_ =
      SectionEnd(header_v1.object_table_offset_, header_v1.object_table_size_);
  header->offheap_table_size_ = 0;
  header->offheap_table_count_ = 0;
  header->metadata_size_ = header_v1.metadata_size_;
  header->metadata_offset_ = header_v1.metadata_offset_;
  if (header->metadata_size_ == 0) {
    header->metadata_offset_ = header->offheap_table_offset_;
  }
}

void MigrateRawHeapHeaderV2(const RawHeapHeaderV2& header_v2,
                            RawHeapHeader* header) {
  *header = {};
  header->magic_ = header_v2.magic_;
  header->dump_format_version_ = header_v2.dump_format_version_;
  header->flags_ = header_v2.flags_;
  header->timestamp_ = header_v2.timestamp_;
  header->v8_major_version_ = header_v2.v8_major_version_;
  header->v8_minor_version_ = header_v2.v8_minor_version_;
  header->v8_build_number_ = header_v2.v8_build_number_;
  header->v8_patch_level_ = header_v2.v8_patch_level_;
  header->cage_base_ = header_v2.cage_base_;
  header->code_cage_base_ = header_v2.code_cage_base_;
  header->trusted_cage_base_ = 0;
  header->root_table_offset_ = header_v2.root_table_offset_;
  header->root_table_size_ = header_v2.root_table_size_;
  header->root_type_count_ = header_v2.root_type_count_;
  header->string_table_offset_ = header_v2.string_table_offset_;
  header->string_table_size_ = header_v2.string_table_size_;
  header->string_count_ = header_v2.string_count_;
  header->object_table_offset_ = header_v2.object_table_offset_;
  header->object_table_size_ = header_v2.object_table_size_;
  header->object_count_ = header_v2.object_count_;
  header->offheap_table_offset_ = header_v2.offheap_table_offset_;
  header->offheap_table_size_ = header_v2.offheap_table_size_;
  header->offheap_table_count_ = header_v2.offheap_table_count_;
  header->metadata_offset_ = header_v2.metadata_offset_;
  header->metadata_size_ = header_v2.metadata_size_;
}

bool ReadRawHeapHeaderV1(BinaryReaderBase* reader, RawHeapHeader* header,
                         std::string* error) {
  RawHeapHeaderV1 header_v1{};
  if (!reader->ReadDataAt(0, sizeof(header_v1),
                          reinterpret_cast<uint8_t*>(&header_v1),
                          sizeof(header_v1))) {
    return FailWithError(error, "failed to read rawheap v1 header");
  }

  MigrateRawHeapHeaderV1(header_v1, header);
  reader->SetPosition(sizeof(RawHeapHeaderV1));
  return true;
}

bool ReadRawHeapHeaderV2(BinaryReaderBase* reader, RawHeapHeader* header,
                         std::string* error) {
  RawHeapHeaderV2 header_v2{};
  if (!reader->ReadDataAt(0, sizeof(header_v2),
                          reinterpret_cast<uint8_t*>(&header_v2),
                          sizeof(header_v2))) {
    return FailWithError(error, "failed to read rawheap v2 header");
  }

  MigrateRawHeapHeaderV2(header_v2, header);
  reader->SetPosition(sizeof(RawHeapHeaderV2));
  return true;
}

bool ReadLatestRawHeapHeaderForTranslate(BinaryReaderBase* reader,
                                         RawHeapHeader* header,
                                         std::string* error) {
  if (!reader->ReadDataAt(0, sizeof(*header),
                          reinterpret_cast<uint8_t*>(header),
                          sizeof(*header))) {
    return FailWithError(error, "failed to read rawheap header");
  }

  reader->SetPosition(sizeof(RawHeapHeader));
  return true;
}

}  // namespace

bool ReadRawHeapHeaderForTranslate(BinaryReaderBase* reader,
                                   RawHeapHeader* header, std::string* error) {
  if (reader == nullptr) {
    return FailWithError(error, "rawheap reader is null");
  }
  if (header == nullptr) {
    return FailWithError(error, "rawheap header output is null");
  }

  RawHeapHeaderPrefix prefix{};
  if (!reader->ReadDataAt(0, sizeof(prefix),
                          reinterpret_cast<uint8_t*>(&prefix),
                          sizeof(prefix))) {
    return FailWithError(error, "failed to read rawheap header prefix");
  }

  if (prefix.magic_ != kRawHeapMagic) {
    return FailWithError(error, FormatMagicMismatch(prefix.magic_));
  }

  if (prefix.dump_format_version_ == kDumpFormatVersionV1) {
    return ReadRawHeapHeaderV1(reader, header, error);
  }
  if (prefix.dump_format_version_ == kDumpFormatVersionWithOffheapTables) {
    return ReadRawHeapHeaderV2(reader, header, error);
  }
  if (prefix.dump_format_version_ == kDumpFormatVersionCurrent) {
    return ReadLatestRawHeapHeaderForTranslate(reader, header, error);
  }
  return FailWithError(
      error, FormatUnsupportedDumpVersion(prefix.dump_format_version_));
}

bool ValidateRawHeapHeaderForTranslate(BinaryReaderBase* reader,
                                       const RawHeapHeader& header,
                                       TranslateDiagnostics* diagnostics) {
  if (reader == nullptr) {
    Fatal(diagnostics, "reader_missing", "rawheap reader is null", "header");
    return false;
  }
  if (header.magic_ != kRawHeapMagic) {
    Fatal(diagnostics, "magic_mismatch", "rawheap magic is not V8HD", "header");
    return false;
  }
  if (!IsRawHeapDumpFormatVersionSupportedForTranslate(
          header.dump_format_version_)) {
    Fatal(diagnostics, "dump_format_version_mismatch",
          "unsupported dump format version " +
              std::to_string(header.dump_format_version_) +
              ", supported versions are " +
              GetRawHeapDumpFormatVersionsForTranslate(),
          "header");
    return false;
  }
  if (!RawHeapFlagsMatchCurrent(header)) {
    Fatal(diagnostics, "rawheap_flags_mismatch",
          "rawheap flags " + GetRawHeapFlagsString(header) +
              " do not match translator binary flags " +
              GetRawHeapFlagsString(GetCurrentRawHeapFlags()),
          "header");
    return false;
  }
  if (header.timestamp_ == 0) {
    Fatal(diagnostics, "timestamp_missing", "rawheap timestamp is zero",
          "header");
    return false;
  }
  if (header.v8_major_version_ != V8_MAJOR_VERSION ||
      header.v8_minor_version_ != V8_MINOR_VERSION ||
      header.v8_build_number_ != V8_BUILD_NUMBER ||
      header.v8_patch_level_ != V8_PATCH_LEVEL) {
    Fatal(diagnostics, "v8_version_mismatch",
          "rawheap V8 version " + std::to_string(header.v8_major_version_) +
              "." + std::to_string(header.v8_minor_version_) + "." +
              std::to_string(header.v8_build_number_) + "." +
              std::to_string(header.v8_patch_level_) +
              " does not match translator binary version " +
              std::to_string(V8_MAJOR_VERSION) + "." +
              std::to_string(V8_MINOR_VERSION) + "." +
              std::to_string(V8_BUILD_NUMBER) + "." +
              std::to_string(V8_PATCH_LEVEL),
          "header");
    return false;
  }
  if (i::V8HeapCompressionScheme::GetPtrComprCageBaseAddress(
          header.cage_base_) != header.cage_base_) {
    Fatal(diagnostics, "cage_base_mismatch",
          "rawheap cage base is not aligned for the current compression scheme",
          "header");
    return false;
  }
#if defined(V8_ENABLE_SANDBOX)
  if (header.dump_format_version_ >= kDumpFormatVersionWithTrustedCageBase) {
    if (header.trusted_cage_base_ == 0) {
      Fatal(diagnostics, "trusted_cage_base_missing",
            "rawheap trusted cage base is missing", "header");
      return false;
    }
    if (i::TrustedSpaceCompressionScheme::GetPtrComprCageBaseAddress(
            header.trusted_cage_base_) != header.trusted_cage_base_) {
      Fatal(diagnostics, "trusted_cage_base_mismatch",
            "rawheap trusted cage base is not aligned for the current "
            "compression scheme",
            "header");
      return false;
    }
  }
#endif

  std::string offheap_error;
  if (!ValidateRawHeapOffheapTableSection(header, reader->BinarySize(),
                                          &offheap_error)) {
    Fatal(diagnostics, "offheap_table_invalid", offheap_error, "offheap_table",
          header.offheap_table_offset_);
    return false;
  }

  return true;
}

bool IsRawHeapDumpFormatVersionSupportedForTranslate(
    uint32_t dump_format_version) {
  return dump_format_version == kDumpFormatVersionV1 ||
         dump_format_version == kDumpFormatVersionWithOffheapTables ||
         dump_format_version == kDumpFormatVersionCurrent;
}

std::string GetRawHeapDumpFormatVersionsForTranslate() {
  return std::to_string(kDumpFormatVersionV1) + ", " +
         std::to_string(kDumpFormatVersionWithOffheapTables) + " and " +
         std::to_string(kDumpFormatVersionCurrent);
}

}  // namespace dfx
}  // namespace v8

#endif
