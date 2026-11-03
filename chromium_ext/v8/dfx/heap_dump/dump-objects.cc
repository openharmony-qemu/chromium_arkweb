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

/* Object table records are:
  Address        | real HeapObject::address(), used only as identity
  uint32_t       | payload size
  uint8_t[]      | payload bytes

  For normal objects, payload is a raw copy of HeapObject bytes. For selected
  data-only/no-content objects, the payload is the compact map-only form below.
  This keeps graph identity and self size while avoiding large or irrelevant
  scalar/external contents.

  When dump Object without content, the dump format is
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
        // Free/filler cells are allocator bookkeeping, not heap snapshot nodes.
        break;
      case i::InstanceType::INSTRUCTION_STREAM_TYPE: {
        CHECK_MAP(obj);
        // InstructionStream is encoded as a special payload because copying
        // executable memory and relocation state verbatim is neither necessary
        // nor sufficient for offline edge reconstruction.
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
          // Normal object: persist the raw object bytes. Translate will copy
          // them into FakeHeapObject storage and use V8 field accessors/body
          // descriptors against that fake address.
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

void ObjectDumper::PreVisit() {
  i::CombinedHeapObjectIterator iterator(heap_);
  i::PtrComprCageBase cage_base(heap_->isolate());
  // This pass mirrors Dump() without writing. It produces object_count_ and the
  // exact object_table_size_ recorded in RawHeapHeader.

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
    ss << "ObjectDumper PreVisit: " << " object_cnt: " << object_cnt_
       << " total object size: " << ObjectDumpSize() << std::endl;
    LogInfo(ss.str());
  }
#ifdef OH_ENABLE_HEAP_DUMP_TEST
  CHECK(object_cnt_ == heap_objects_.size());
#endif
}

uint32_t ObjectDumper::ObjectCount() const { return object_cnt_; }

uint32_t ObjectDumper::ObjectDumpSize() const {
  // every object has address and size field, size field should not be 0.
  return total_object_size_ +
         object_cnt_ * (sizeof(i::Address) + sizeof(uint32_t));
}

}  // namespace dfx
}  // namespace v8
#endif
