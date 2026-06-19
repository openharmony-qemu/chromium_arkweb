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

#include <atomic>
#include <cinttypes>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "src/common/ptr-compr.h"

#ifdef OH_ENABLE_HEAP_DUMP_TEST
#include "src/heap/memory-chunk.h"
#endif
#include "src/codegen/reloc-info-inl.h"

// dump-objects-inl.h must be the last one
#include "dump-objects-inl.h"

namespace v8 {
namespace dfx {
namespace {
class LogObjectVisitor : public i::ObjectVisitorWithCageBases {
 public:
  explicit LogObjectVisitor(i::Isolate* isolate)
      : i::ObjectVisitorWithCageBases(isolate) {}

#define FieldLog                                             \
  if (i::v8_flags.log_heapdump) {                            \
    LogInfo(std::string("object visit:") + __func__ + "\n"); \
  }
  void VisitMapPointer(i::Tagged<i::HeapObject> host) override { FieldLog; }

  void VisitPointers(i::Tagged<i::HeapObject> host, i::ObjectSlot start,
                     i::ObjectSlot end) override {
    FieldLog;
  }

  void VisitPointers(i::Tagged<i::HeapObject> host, i::MaybeObjectSlot start,
                     i::MaybeObjectSlot end) override {
    FieldLog;
  }

  void VisitInstructionStreamPointer(i::Tagged<i::Code> host,
                                     i::InstructionStreamSlot slot) override {
    FieldLog;
  }

  void VisitCodeTarget(i::Tagged<i::InstructionStream> host,
                       i::RelocInfo* rinfo) override {
    FieldLog;
  }

  void VisitEmbeddedPointer(i::Tagged<i::InstructionStream> host,
                            i::RelocInfo* rinfo) override {
    FieldLog;
  }

  void VisitExternalReference(i::Tagged<i::InstructionStream> host,
                              i::RelocInfo* rinfo) override {
    FieldLog;
  }

  void VisitInternalReference(i::Tagged<i::InstructionStream> host,
                              i::RelocInfo* rinfo) override {
    FieldLog;
  }

  void VisitOffHeapTarget(i::Tagged<i::InstructionStream> host,
                          i::RelocInfo* rinfo) override {
    FieldLog;
  }

  void VisitIndirectPointer(i::Tagged<i::HeapObject> host,
                            i::IndirectPointerSlot slot,
                            i::IndirectPointerMode mode) override {
    FieldLog;
  }

  void VisitProtectedPointer(i::Tagged<i::TrustedObject> host,
                             i::ProtectedPointerSlot slot) override {
    FieldLog;
  }

  void VisitJSDispatchTableEntry(i::Tagged<i::HeapObject> host,
                                 i::JSDispatchHandle handle) override {
    FieldLog;
  }
};

void LogObject(i::Isolate* isolate, i::Tagged<i::HeapObject> object) {
  i::InstanceType instance_type = object->map()->instance_type();
  i::Address object_address = object->address();
  uint32_t dump_size = static_cast<uint32_t>(object->Size());

  LogInfo("log object:" + i::ToString(instance_type) + " address " +
          std::to_string(object_address) + " dump size " +
          std::to_string(dump_size) + "\n");

  switch (instance_type) {
    case i::InstanceType::CODE_WRAPPER_TYPE: {
      LogObjectVisitor log_visitor(isolate);
      i::VisitObject(isolate, object, &log_visitor);
      break;
    }
    default:
      break;
  }
}
}  // namespace

ObjectDumper::ObjectDumper(i::Heap* heap, BinaryWriterBase* writer)
    : heap_(heap), writer_(writer) {}

/* When dump Object without content, the dump format is
  type           |          description
  ------------------------------------------------
  Address        |          address of heapobject
  uint32_t       |          dump size
  tagged_t       |          map pointer
  uint32_t       |          real size
*/
void ObjectDumper::DumpObjectWithoutContent(i::Tagged<i::HeapObject> object) {
  // address
  i::Address object_address = object->address();
  writer_->WriteBinBlock(reinterpret_cast<uint8_t*>(&object_address),
                         sizeof(object_address));
  // dump size
  writer_->WriteBinBlock(
      reinterpret_cast<const uint8_t*>(&kNoContentObjectDumpSize),
      sizeof(kNoContentObjectDumpSize));
  // map pointer
  writer_->WriteBinBlock(reinterpret_cast<uint8_t*>(object_address),
                         sizeof(i::Tagged_t));
  // real size
  uint32_t real_size = static_cast<uint32_t>(object->Size());
  writer_->WriteBinBlock(reinterpret_cast<uint8_t*>(&real_size),
                         sizeof(real_size));
}

void ObjectDumper::WriteInstructionStreamInfo(i::Tagged<i::HeapObject> object) {
  DumpObjectVisitor visitor(
      i::PtrComprCageBase(heap_->isolate()->cage_base()),
      i::PtrComprCageBase(heap_->isolate()->code_cage_base()), this);
  i::Tagged<i::TrustedObject> host = TrustedCast<i::TrustedObject>(object);
  // same as PreVisitInstructionStream
  visitor.VisitProtectedPointer(
      host, host->RawProtectedPointerField(i::InstructionStream::kCodeOffset));
  visitor.VisitProtectedPointer(
      host, host->RawProtectedPointerField(
                i::InstructionStream::kRelocationInfoOffset));
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
  //  mapPointer + 2 * protectedPointer (objects-body-descriptors-inl.h: class
  //  InstructionStream::BodyDescripter)
  uint32_t prefix_size = sizeof(i::Tagged_t) + 2 * sizeof(i::Address);
  uint32_t objcet_size =
      static_cast<uint32_t>(instruction_stream_map_[object_address].size()) *
      (sizeof(i::Address) + sizeof(int8_t));
  objcet_size += prefix_size;
  writer_->WriteBinBlock(reinterpret_cast<uint8_t*>(&objcet_size),
                         sizeof(objcet_size));
  // map pointer
  writer_->WriteBinBlock(reinterpret_cast<uint8_t*>(object_address),
                         sizeof(i::Tagged_t));
  // content
  WriteInstructionStreamInfo(object);
}

#ifdef OH_ENABLE_HEAP_DUMP_TEST
#define CHECK_MAP(obj)                                                         \
  /*  map must be a heap object  */                                            \
  if (auto map_obj = obj->map(); !heap_objects_.count(map_obj.address()) ||    \
                                 !heap_objects_[map_obj.address()]) {          \
    i::Address object_address = obj->address();                                \
    std::stringstream ss;                                                      \
                                                                               \
    ss << "--------this map not in Combined heap.";                            \
    ss << "[HeapDump]object address:" << object_address << " ";                \
                                                                               \
    ss << "map.ptr():" << obj->map().ptr() << " "                              \
       << "instance type:" << i::ToString(obj->map()->instance_type()) << " "; \
    i::MemoryChunk* memory_chunk =                                             \
        i::MemoryChunk::FromAddress(map_obj.address());                        \
    i::AllocationSpace allocation_space =                                      \
        i::MutablePageMetadata::cast(memory_chunk->Metadata())                 \
            ->owner_identity();                                                \
    ss << " allocation space for map:" << i::ToString(allocation_space);       \
    LogInfo(ss.str());                                                         \
    UNREACHABLE();                                                             \
  }
#else  // OH_ENABLE_HEAP_DUMP_TEST
#define CHECK_MAP(obj)
#endif  // OH_ENABLE_HEAP_DUMP_TEST

void ObjectDumper::Dump() {
  v8::base::ElapsedTimer timer;
  timer.Start();

  i::CombinedHeapObjectIterator iterator(heap_);

  i::PtrComprCageBase cage_base(heap_->isolate());
  for (i::Tagged<i::HeapObject> obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    ////////// dump
    i::InstanceType instance_type = obj->map(cage_base)->instance_type();
    // i::VisitorId visitor_id = obj->map()->visitor_id();
    switch (instance_type) {
      case i::InstanceType::FREE_SPACE_TYPE:
      case i::InstanceType::FILLER_TYPE:
        // won't dump it
        break;
      case i::InstanceType::INSTRUCTION_STREAM_TYPE: {
        CHECK_MAP(obj);
        IterateInstructionStream(obj);
        break;
      }
      default: {
        CHECK_MAP(obj);
        if (SkipContentDump(obj->map())) {
          DumpObjectWithoutContent(obj);
        } else {
          if (i::v8_flags.log_heapdump) {
            LogObject(heap_->isolate(), obj);
          }
          ///// dump normal object
          // address
          i::Address object_address = obj->address();
          writer_->WriteBinBlock(reinterpret_cast<uint8_t*>(&object_address),
                                 sizeof(object_address));
          // dump size
          uint32_t dump_size = static_cast<uint32_t>(obj->Size());
          writer_->WriteBinBlock(reinterpret_cast<uint8_t*>(&dump_size),
                                 sizeof(dump_size));
          // object
          writer_->WriteBinBlock(reinterpret_cast<uint8_t*>(object_address),
                                 static_cast<uint32_t>(dump_size));
        }
      }  // end of default
    }
  }

  double elapsed = timer.Elapsed().InMillisecondsF();
  std::stringstream ss;
  ss << "[HeapDump]ObjectDumper used time: " << elapsed << " ms "
     << " object_cnt: " << object_cnt_
     << " total object size: " << ObjectDumpSize() << std::endl;
  LogInfo(ss.str());
  timer.Stop();
}

void ObjectDumper::AddInstructionStreamInfo(i::Address addr,
                                            InstructionStreamInfo info) {
  instruction_stream_map_[addr].push_back(info);
}

uint32_t ObjectDumper::PreVisitInstructionStream(
    i::Tagged<i::HeapObject> object) {
  // mapPointer + 2 * protectedPointer (objects-body-descriptors-inl.h: class
  // InstructionStream::BodyDescripter)
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
      i::RelocInfo::ModeMask(i::RelocInfo::WASM_STUB_CALL) |
      i::RelocInfo::ModeMask(i::RelocInfo::JS_DISPATCH_HANDLE);
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

// AddNode
void ObjectDumper::PreVisit() {
  i::CombinedHeapObjectIterator iterator(heap_);
  i::PtrComprCageBase cage_base(heap_->isolate());
  // NIY(hh):static_assert(map is first pointer)

  for (i::Tagged<i::HeapObject> obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    i::InstanceType instance_type = obj->map()->instance_type();
    // i::VisitorId visitor_id = obj->map()->visitor_id();

    switch (instance_type) {
      case i::InstanceType::FREE_SPACE_TYPE:
      case i::InstanceType::FILLER_TYPE:
        // won't dump it
        break;
      case i::InstanceType::INSTRUCTION_STREAM_TYPE: {
#ifdef OH_ENABLE_HEAP_DUMP_TEST
        heap_objects_[obj.address()] = true;
#endif
        total_object_size_ += PreVisitInstructionStream(obj);
        object_cnt_++;
        break;
      }
      default: {
#ifdef OH_ENABLE_HEAP_DUMP_TEST
        heap_objects_[obj.address()] = true;
#endif
        object_cnt_++;
        if (SkipContentDump(obj->map())) {
          total_object_size_ += kNoContentObjectDumpSize;
        } else {
          total_object_size_ += obj->Size();
        }
      }
    }  // end of switch
  }

  if (i::v8_flags.log_heapdump) {
    std::stringstream ss;
    ss << "ObjectDumper PreVisit: "
       << " object_cnt: " << object_cnt_
       << " total object size: " << ObjectDumpSize() << std::endl;
    LogInfo(ss.str());
  }
#ifdef OH_ENABLE_HEAP_DUMP_TEST
  CHECK(object_cnt_ == heap_objects_.size());
#endif
}

uint32_t ObjectDumper::ObjectCount() const {
  return object_cnt_;
}

uint32_t ObjectDumper::ObjectDumpSize() const {
  // every object has address and size field, size field should not be 0.
  return total_object_size_ +
         object_cnt_ * (sizeof(i::Address) + sizeof(uint32_t));
}

}  // namespace dfx
}  // namespace v8
#endif
