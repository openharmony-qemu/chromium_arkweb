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

#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-objects.h"
#include "src/objects/instruction-stream.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace dfx {

bool ObjectTranslator::IsStrong(i::Address addr) {
  if (addr & dfx::kRawHeapWeakObjectTag) {
    return false;
  }
  return true;
}

bool ObjectTranslator::TranslateInstructionStream(i::Address addr,
                                                  uint32_t size) {
  uint32_t next_index = 0;
  // According to objects-body-descriptors-inl.h:
  // InstructionStream::BodyDescriptor fixed prefix is mapPointer + 2 *
  // protectedPointer. The fixed code / relocation_info fields are emitted by
  // ExtractInstructionStreamReferences. Remaining entries are rawheap custom
  // payload reloc info encoded as addr + mode and emitted as hidden/weak
  // snapshot edges below.
  const RawHeapObjectHead* object_head = FindObjectHead(addr);
  if (object_head == nullptr) {
    ReportCorrupt("instruction_stream_object_missing",
                  "InstructionStream object was not present in rawheap",
                  "object_body", addr);
    return false;
  }
  if (size < sizeof(i::Tagged_t) + 2 * sizeof(i::Address)) {
    ReportCorrupt("instruction_stream_size_invalid",
                  "InstructionStream dump payload is smaller than its fixed "
                  "prefix",
                  "object_body", addr, 0, 0, 0, object_head->offset_);
    return false;
  }

  // Only validate and skip the fixed protected pointers here; do not emit
  // hidden edges for them, otherwise they duplicate the named edges emitted by
  // ExtractInstructionStreamReferences.
  uint32_t offset = object_head->offset_ + sizeof(i::Tagged_t);
  uint8_t fixed_protected_pointers[2 * sizeof(i::Address)];
  if (!reader_->ReadDataAt(offset, sizeof(fixed_protected_pointers),
                           fixed_protected_pointers,
                           sizeof(fixed_protected_pointers))) {
    ReportCorrupt("instruction_stream_fixed_prefix_read_failed",
                  "failed to read InstructionStream fixed protected pointers",
                  "object_body", addr, 0, 0, 0, offset);
    return false;
  }

  offset += sizeof(fixed_protected_pointers);
  int reloc_info_cnt = (size - sizeof(i::Tagged_t) - 2 * sizeof(i::Address)) /
                       (sizeof(i::Address) + sizeof(int8_t));
  for (int i = 0; i < reloc_info_cnt; i++) {
    i::Address object_addr{0};
    if (!reader_->ReadDataAt(offset, sizeof(i::Address),
                             reinterpret_cast<uint8_t*>(&object_addr),
                             sizeof(i::Address))) {
      ReportCorrupt("instruction_stream_reloc_target_failed",
                    "failed to read InstructionStream reloc target",
                    "object_body", addr, 0, 0, 0, offset);
      return false;
    }
    offset += sizeof(i::Address);
    int8_t mode{0};
    if (!reader_->ReadDataAt(offset, sizeof(mode),
                             reinterpret_cast<uint8_t*>(&mode),
                             sizeof(mode))) {
      ReportCorrupt("instruction_stream_reloc_mode_failed",
                    "failed to read InstructionStream reloc mode",
                    "object_body", addr, 0, 0, 0, offset);
      return false;
    }

    const bool is_strong = IsStrong(object_addr);
    // InstructionStream custom payload stores untagged rawheap object
    // identities, so kRawHeapWeakObjectTag is a rawheap serialization marker,
    // not the native V8 MaybeObject weak tag. Strip it before creating the
    // snapshot weak edge.
    const i::Address target_addr =
        is_strong ? object_addr : object_addr ^ dfx::kRawHeapWeakObjectTag;
    if (FindObjectHead(target_addr) == nullptr) {
      ReportWarning("instruction_stream_reloc_target_not_dumped",
                    "InstructionStream relocation target is not in dumped "
                    "object table",
                    "object_body", addr, target_addr, 0, 0, offset);
      offset += sizeof(int8_t);
      continue;
    }
    if (generator()->GetNode(target_addr) == nullptr) {
      ReportWarning("instruction_stream_reloc_target_node_missing",
                    "InstructionStream relocation target has no snapshot "
                    "node; edge suppressed",
                    "object_body", addr, target_addr, 0, 0, offset);
      offset += sizeof(int8_t);
      continue;
    }
    if (is_strong) {
      generator()->SetHiddenReference(addr, next_index++, target_addr);
    } else {
      generator()->SetWeakReference(addr, next_index++, target_addr, {});
    }
    offset += sizeof(int8_t);
  }
  return true;
}

void ObjectTranslator::ExtractInstructionStreamReferences(
    i::Address real_address, i::Tagged<i::InstructionStream> istream) {
  // Breadcrumb: V8HeapExplorer::ExtractInstructionStreamReferences mirrors
  // InstructionStream::BodyDescriptor for heap-snapshot named edges. Emit only
  // the named code / relocation_info edges here; rawheap custom payload
  // relocation entries stay in TranslateInstructionStream as hidden/weak edges.
  auto it = objects_head_.find(real_address);
  if (it == objects_head_.end()) {
    return;
  }

  uint32_t offset = it->second.offset_ + sizeof(i::Tagged_t);
  i::Address code_addr{0};
  if (reader_->ReadDataAt(offset, sizeof(code_addr),
                          reinterpret_cast<uint8_t*>(&code_addr),
                          sizeof(code_addr)) &&
      generator_->GetNode(code_addr)) {
    generator_->SetInternalReference(
        real_address, "code", code_addr, -1
        /* no need use i::InstructionStream::kCodeOffset */);
  }

  offset += sizeof(i::Address);
  i::Address relocation_info_addr{0};
  if (reader_->ReadDataAt(offset, sizeof(relocation_info_addr),
                          reinterpret_cast<uint8_t*>(&relocation_info_addr),
                          sizeof(relocation_info_addr)) &&
      generator_->GetNode(relocation_info_addr)) {
    generator_->SetInternalReference(
        real_address, "relocation_info", relocation_info_addr, -1
        /* i::InstructionStream::kRelocationInfoOffset */);
  }
}

}  // namespace dfx
}  // namespace v8
#endif
