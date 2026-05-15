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

#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-objects.h"
#include "src/codegen/reloc-info-inl.h"

namespace v8 {
namespace dfx {
namespace {

// Rawheap InstructionStream object record:
//
//   Address object_address
//   uint32_t dump_size
//   payload[dump_size]:
//     Tagged_t map
//     Address code
//     Address relocation_info
//     repeated reloc records:
//       Address target_object_address
//       int8_t reloc_mode
//
// The fixed payload prefix mirrors InstructionStream::BodyDescriptor:
//
//   IterateProtectedPointer(obj, kCodeOffset, v);
//   IterateProtectedPointer(obj, kRelocationInfoOffset, v);
//
// If the InstructionStream is fully initialized, BodyDescriptor then iterates
// reloc info. You can see visitor.cc:ObjectVisitor::VisitRelocInfo and Visit in
// reloc-info-inl.h.

template <typename TSlot>
void WriteHeapObjectSlot(BinaryWriterBase* writer,
                         i::PtrComprCageBase cage_base, TSlot slot) {
  i::Tagged<i::HeapObject> heap_object;
  auto loaded_value = slot.load(cage_base);
  if (loaded_value.GetHeapObjectIfStrong(&heap_object)) {
    i::Address object_address = heap_object->address();
    writer->WriteBinBlock(reinterpret_cast<uint8_t*>(&object_address),
                          sizeof(object_address));
  } else if (loaded_value.GetHeapObjectIfWeak(&heap_object)) {
    i::Address object_address =
        heap_object->address() | dfx::kRawHeapWeakObjectTag;
    writer->WriteBinBlock(reinterpret_cast<uint8_t*>(&object_address),
                          sizeof(object_address));
  }
  // Smi and off-heap values are intentionally not persisted.
}

// see VisitProtectedPointer in heap-snapshot-generator.cc
void WriteInstructionStreamProtectedPointer(BinaryWriterBase* writer,
                                            i::Tagged<i::HeapObject> object,
                                            int offset) {
  i::Tagged<i::TrustedObject> host = TrustedCast<i::TrustedObject>(object);
  const i::PtrComprCageBase unused_cage_base(i::kNullAddress);
  WriteHeapObjectSlot(writer, unused_cage_base,
                      host->RawProtectedPointerField(offset));
}

class DumpObjectVisitor : public i::ObjectVisitorWithCageBases {
 public:
  explicit DumpObjectVisitor(i::PtrComprCageBase cage_base,
                             i::PtrComprCageBase code_cage_base,
                             dfx::ObjectDumper* dumper, bool pre_visit = false)
      : i::ObjectVisitorWithCageBases(cage_base, code_cage_base),
        cage_base_(cage_base),
        dumper_(dumper),
        pre_visit_(pre_visit) {}

  void VisitMapPointer(i::Tagged<i::HeapObject> host) override {
    // do nothing
  }

  void VisitPointers(i::Tagged<i::HeapObject> host, i::ObjectSlot start,
                     i::ObjectSlot end) override {
    // do nothing
  }

  void VisitPointers(i::Tagged<i::HeapObject> host, i::MaybeObjectSlot start,
                     i::MaybeObjectSlot end) override {
    // do nothing
  }

  void VisitInstructionStreamPointer(i::Tagged<i::Code> host,
                                     i::InstructionStreamSlot slot) override {
    // do nothing
  }

  void VisitCodeTarget(i::Tagged<i::InstructionStream> host,
                       i::RelocInfo* rinfo) override {
    if (pre_visit_) {
      count_++;
    }
    dfx::InstructionStreamInfo info;
    i::Tagged<i::InstructionStream> target =
        i::InstructionStream::FromTargetAddress(rinfo->target_address());
    i::Address object_address = target->address();
    info.object_addr_ = object_address;
    i::RelocInfo::Mode mode = rinfo->rmode();
    info.mode_ = static_cast<int8_t>(mode);
    dumper_->AddInstructionStreamInfo(host->address(), info);
  }

  void VisitEmbeddedPointer(i::Tagged<i::InstructionStream> host,
                            i::RelocInfo* rinfo) override {
    if (pre_visit_) {
      count_++;
    }
    dfx::InstructionStreamInfo info;
    i::Tagged<i::HeapObject> object = rinfo->target_object(cage_base_);
    i::Tagged<i::Code> code =
        UncheckedCast<i::Code>(host->raw_code(v8::kAcquireLoad));
    if (code->IsWeakObject(object)) {
      info.object_addr_ = object->address() | dfx::kRawHeapWeakObjectTag;
    } else {
      info.object_addr_ = object->address();
    }
    i::RelocInfo::Mode mode = rinfo->rmode();
    info.mode_ = static_cast<int8_t>(mode);
    dumper_->AddInstructionStreamInfo(host->address(), info);
  }

  void VisitExternalReference(i::Tagged<i::InstructionStream> host,
                              i::RelocInfo* rinfo) override {
    // do nothing for external
  }

  void VisitInternalReference(i::Tagged<i::InstructionStream> host,
                              i::RelocInfo* rinfo) override {
    // do nothing
  }

  void VisitOffHeapTarget(i::Tagged<i::InstructionStream> host,
                          i::RelocInfo* rinfo) override {
    // do nothing
  }

  void VisitIndirectPointer(i::Tagged<i::HeapObject> host,
                            i::IndirectPointerSlot slot,
                            i::IndirectPointerMode mode) override {
    // do nothing
  }

  void VisitJSDispatchTableEntry(i::Tagged<i::HeapObject> host,
                                 i::JSDispatchHandle handle) override {
    // do nothing
  }

  // copy from VisitRelocInfo
  uint32_t CountRelocInfo(i::Tagged<i::InstructionStream> host,
                          i::RelocIterator* it) {
    DCHECK(host->IsFullyInitialized());
    count_ = 0;
    for (; !it->done(); it->next()) {
      it->rinfo()->Visit(host, this);
    }
    return count_;
  }

 private:
  i::PtrComprCageBase cage_base_;
  dfx::ObjectDumper* dumper_;
  bool pre_visit_{false};
  uint32_t count_{0};
};

}  // namespace

void ObjectDumper::WriteInstructionStreamInfo(i::Tagged<i::HeapObject> object) {
  // same as PreVisitInstructionStream
  // see BodyDescriptorBase::IterateProtectedPointer and VisitProtectedPointer
  // in heap-snapshot-generator.cc
  WriteInstructionStreamProtectedPointer(writer_, object,
                                         i::InstructionStream::kCodeOffset);
  WriteInstructionStreamProtectedPointer(
      writer_, object, i::InstructionStream::kRelocationInfoOffset);
  i::Address object_address = object->address();
  auto it = instruction_stream_map_.find(object_address);
  if (it != instruction_stream_map_.end()) {
    const std::vector<InstructionStreamInfo>& vec = it->second;
    for (const auto& info : vec) {
      i::Address addr = info.object_addr_;
      int8_t mode = info.mode_;
      writer_->WriteBinBlock(reinterpret_cast<uint8_t*>(&addr), sizeof(addr));
      writer_->WriteBinBlock(reinterpret_cast<uint8_t*>(&mode), sizeof(mode));
    }
  }
}

void ObjectDumper::IterateInstructionStream(i::Tagged<i::HeapObject> object) {
  // address
  i::Address object_address = object->address();
  writer_->WriteBinBlock(reinterpret_cast<uint8_t*>(&object_address),
                         sizeof(object_address));
  // size
  // map pointer + code protected pointer + relocation_info protected pointer
  // + reloc heap-object records.
  uint32_t prefix_size = sizeof(i::Tagged_t) + 2 * sizeof(i::Address);
  uint32_t object_size =
      static_cast<uint32_t>(instruction_stream_map_[object_address].size()) *
      (sizeof(i::Address) + sizeof(int8_t));
  object_size += prefix_size;
  writer_->WriteBinBlock(reinterpret_cast<uint8_t*>(&object_size),
                         sizeof(object_size));
  // map pointer
  writer_->WriteBinBlock(reinterpret_cast<uint8_t*>(object_address),
                         sizeof(i::Tagged_t));
  // content
  WriteInstructionStreamInfo(object);
}

void ObjectDumper::AddInstructionStreamInfo(i::Address addr,
                                            InstructionStreamInfo info) {
  instruction_stream_map_[addr].push_back(info);
}

uint32_t ObjectDumper::PreVisitInstructionStream(
    i::Tagged<i::HeapObject> object) {
  // map pointer + code protected pointer + relocation_info protected pointer.
  uint32_t prefix_size = sizeof(i::Tagged_t) + 2 * sizeof(i::Address);
  int kRelocModeMask =
      i::RelocInfo::ModeMask(i::RelocInfo::CODE_TARGET) |
      i::RelocInfo::ModeMask(i::RelocInfo::RELATIVE_CODE_TARGET) |
      i::RelocInfo::ModeMask(i::RelocInfo::FULL_EMBEDDED_OBJECT) |
      i::RelocInfo::ModeMask(i::RelocInfo::COMPRESSED_EMBEDDED_OBJECT) |
      i::RelocInfo::ModeMask(i::RelocInfo::EXTERNAL_REFERENCE) |
      i::RelocInfo::ModeMask(i::RelocInfo::INTERNAL_REFERENCE) |
      i::RelocInfo::ModeMask(i::RelocInfo::INTERNAL_REFERENCE_ENCODED) |
      i::RelocInfo::ModeMask(i::RelocInfo::OFF_HEAP_TARGET) |
      i::RelocInfo::ModeMask(i::RelocInfo::WASM_STUB_CALL);
  i::Tagged<i::InstructionStream> istream =
      UncheckedCast<i::InstructionStream>(object);
  if (!istream->IsFullyInitialized()) {
    return prefix_size;
  }
  i::PtrComprCageBase cage_base(heap_->isolate());
  DumpObjectVisitor visitor(
      cage_base, i::PtrComprCageBase(heap_->isolate()->code_cage_base()), this,
      true);
  i::RelocIterator it(istream, kRelocModeMask);
  uint32_t cnt = visitor.CountRelocInfo(istream, &it);
  CHECK(instruction_stream_map_[object.address()].size() == cnt);
  uint32_t total_object_size = cnt * (sizeof(i::Address) + sizeof(int8_t));
  return prefix_size + total_object_size;
}

}  // namespace dfx
}  // namespace v8
#endif
