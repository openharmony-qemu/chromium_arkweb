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

#include <iostream>
#include <limits>
#include <memory>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/dfx-visitor-list.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-format.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/heap-dump-log.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/dfx-heap-visitor-inl.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/dfx-heap-visitor.h"
#include "src/base/logging.h"
#include "src/codegen/reloc-info.h"
#include "src/common/globals.h"
#include "src/common/ptr-compr.h"
#include "src/init/heap-symbols.h"
#include "src/objects/contexts.h"
#include "src/objects/elements-kind.h"
#include "src/objects/heap-object.h"
#include "src/objects/hole.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/instruction-stream.h"
#include "src/objects/js-disposable-stack.h"
#include "src/objects/js-regexp.h"
#include "src/objects/number-string-cache.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/objects/oddball.h"
#include "src/objects/slots.h"
#include "src/objects/visitors.h"
#include "src/roots/roots.h"

namespace v8 {
namespace dfx {
// see ObjectVisitorForwarder in visit-object.cc for more info
class HeapObjectVisitor final : public DfxHeapVisitor<HeapObjectVisitor> {
 public:
  explicit HeapObjectVisitor(i::ObjectVisitor* visitor,
                             i::PtrComprCageBase cage_base,
                             i::PtrComprCageBase code_cage_base)
      : DfxHeapVisitor(cage_base, code_cage_base), visitor_(visitor) {}

  // the rest is copied from ObjectVisitorForwarder
  static constexpr bool ShouldVisitMapPointer() { return false; }
  static constexpr bool ShouldUseUncheckedCast() { return true; }
  static constexpr bool ShouldVisitFullJSObject() { return true; }

  void VisitPointers(i::Tagged<i::HeapObject> host, i::ObjectSlot start,
                     i::ObjectSlot end) override {
    visitor_->VisitPointers(host, start, end);
  }

  void VisitPointers(i::Tagged<i::HeapObject> host, i::MaybeObjectSlot start,
                     i::MaybeObjectSlot end) override {
    visitor_->VisitPointers(host, start, end);
  }

  void VisitInstructionStreamPointer(i::Tagged<i::Code> host,
                                     i::InstructionStreamSlot slot) override {
    visitor_->VisitInstructionStreamPointer(host, slot);
  }

  void VisitCustomWeakPointers(i::Tagged<i::HeapObject> host,
                               i::ObjectSlot start,
                               i::ObjectSlot end) override {
    visitor_->VisitCustomWeakPointers(host, start, end);
  }

  void VisitPointer(i::Tagged<i::HeapObject> host,
                    i::ObjectSlot slot) override {
    visitor_->VisitPointers(host, slot, slot + 1);
  }

  void VisitPointer(i::Tagged<i::HeapObject> host,
                    i::MaybeObjectSlot slot) override {
    visitor_->VisitPointers(host, slot, slot + 1);
  }

  void VisitCustomWeakPointer(i::Tagged<i::HeapObject> host,
                              i::ObjectSlot slot) override {
    visitor_->VisitCustomWeakPointer(host, slot);
  }

  void VisitCodeTarget(i::Tagged<i::InstructionStream> host,
                       i::RelocInfo* rinfo) override {
    visitor_->VisitCodeTarget(host, rinfo);
  }

  void VisitEmbeddedPointer(i::Tagged<i::InstructionStream> host,
                            i::RelocInfo* rinfo) override {
    visitor_->VisitEmbeddedPointer(host, rinfo);
  }

  void VisitExternalReference(i::Tagged<i::InstructionStream> host,
                              i::RelocInfo* rinfo) override {
    visitor_->VisitExternalReference(host, rinfo);
  }

  void VisitInternalReference(i::Tagged<i::InstructionStream> host,
                              i::RelocInfo* rinfo) override {
    visitor_->VisitInternalReference(host, rinfo);
  }

  void VisitOffHeapTarget(i::Tagged<i::InstructionStream> host,
                          i::RelocInfo* rinfo) override {
    visitor_->VisitOffHeapTarget(host, rinfo);
  }

  void VisitExternalPointer(i::Tagged<i::HeapObject> host,
                            i::ExternalPointerSlot slot) override {
    visitor_->VisitExternalPointer(host, slot);
  }

  void VisitCppHeapPointer(i::Tagged<i::HeapObject> host,
                           i::CppHeapPointerSlot slot) override {
    visitor_->VisitCppHeapPointer(host, slot);
  }

  void VisitEphemeron(i::Tagged<i::HeapObject> host, int index,
                      i::ObjectSlot key, i::ObjectSlot value) override {
    visitor_->VisitEphemeron(host, index, key, value);
  }

  void VisitIndirectPointer(i::Tagged<i::HeapObject> host,
                            i::IndirectPointerSlot slot,
                            i::IndirectPointerMode mode) override {
    visitor_->VisitIndirectPointer(host, slot, mode);
  }

  void VisitProtectedPointer(i::Tagged<i::TrustedObject> host,
                             i::ProtectedPointerSlot slot) override {
    visitor_->VisitProtectedPointer(host, slot);
  }

  void VisitProtectedPointer(i::Tagged<i::TrustedObject> host,
                             i::ProtectedMaybeObjectSlot slot) override {
    visitor_->VisitProtectedPointer(host, slot);
  }

  void VisitTrustedPointerTableEntry(i::Tagged<i::HeapObject> host,
                                     i::IndirectPointerSlot slot) override {
    visitor_->VisitTrustedPointerTableEntry(host, slot);
  }

  void VisitJSDispatchTableEntry(i::Tagged<i::HeapObject> host,
                                 i::JSDispatchHandle handle) override {
    visitor_->VisitJSDispatchTableEntry(host, handle);
  }

  void VisitMapPointer(i::Tagged<i::HeapObject> host) override {
    UNREACHABLE();
  }

#define STRING_METHOD(_, name, value) \
  std::string name() const { return value; }

  INTERNALIZED_STRING_LIST_GENERATOR(STRING_METHOD, _)
#undef STRING_METHOD

 private:
  ObjectVisitor* const visitor_;
};

//////////////////// ObjectDetailVisitor
// see IndexedReferencesExtractor in heap-snapshot-generator.cc
class ObjectDetailVisitor : public i::ObjectVisitorWithCageBases {
 public:
  explicit ObjectDetailVisitor(ObjectTranslator* translator,
                               i::Address parent_address,
                               i::PtrComprCageBase cage_base,
                               i::PtrComprCageBase code_cage_base,
                               i::InstanceType instance_type)
      : i::ObjectVisitorWithCageBases(cage_base, code_cage_base),
        translator_(translator),
        parent_address_(parent_address),
        cage_base_(cage_base),
        code_cage_base_(code_cage_base),
        instance_type_(instance_type) {}
#define FieldLog                                             \
  if (i::v8_flags.log_heapdump) {                            \
    LogInfo(std::string("object visit:") + __func__ + "\n"); \
  }
  void VisitMapPointer(i::Tagged<i::HeapObject> host) override {
    FieldLog;
    VisitSlotImpl(host, cage_base_, host->map_slot());
  }

  void VisitPointers(i::Tagged<i::HeapObject> host, i::ObjectSlot start,
                     i::ObjectSlot end) override {
    FieldLog;
    VisitPointers(host, i::MaybeObjectSlot(start), i::MaybeObjectSlot(end));
  }

  void VisitPointers(i::Tagged<i::HeapObject> host, i::MaybeObjectSlot start,
                     i::MaybeObjectSlot end) override {
    FieldLog;
    for (i::MaybeObjectSlot slot = start; slot < end; ++slot) {
      VisitSlotImpl(host, cage_base_, slot);
    }
  }

  // code.h
  void VisitInstructionStreamPointer(i::Tagged<i::Code> host,
                                     i::InstructionStreamSlot slot) override {
    // slot maybe off heap, in heap-snapshot-generator.cc, VisitSlotImpl will
    // ignore the off-heap object
    FieldLog;
    VisitSlotImpl(host, code_cage_base_, slot);
  }

  // class InstructionStream::BodyDescriptor in objects-body-descriptors-inl.h
  // will call it.
  // NIY: if use DCHECK, it will fail before this visit
  void VisitCodeTarget(i::Tagged<i::InstructionStream> host,
                       i::RelocInfo* rinfo) override {
    UNREACHABLE();
  }
  // same as VisitCodeTarget
  void VisitEmbeddedPointer(i::Tagged<i::InstructionStream> host,
                            i::RelocInfo* rinfo) override {
    UNREACHABLE();
  }

  // IterateTrustedPointer will call this. IndirectPointerSlot is a index to an
  // entry in a pointer table.
  void VisitIndirectPointer(i::Tagged<i::HeapObject> host,
                            i::IndirectPointerSlot slot,
                            i::IndirectPointerMode mode) override {
    FieldLog;
    // handle is a uint32_t type index, see IndirectPointerSlot::load
    i::IndirectPointerHandle handle = slot.Relaxed_LoadHandle();
    ResolveHandle(handle, slot.tag());
  }

  // a trusted object can use protected pointer find another trusted object
  void VisitProtectedPointer(i::Tagged<i::TrustedObject> host,
                             i::ProtectedPointerSlot slot) override {
    FieldLog;
    // TODO(saelo): the cage base doesn't currently matter as it isn't used,
    // but technically we should either use the trusted cage base here or
    // remove the cage_base parameter.
    const i::PtrComprCageBase unused_cage_base(i::kNullAddress);
    VisitSlotImpl(host, unused_cage_base, slot);
  }
  void VisitProtectedPointer(i::Tagged<i::TrustedObject> host,
                             i::ProtectedMaybeObjectSlot slot) override {
    FieldLog;
    // TODO(saelo): the cage base doesn't currently matter as it
    // isn't used,
    // but technically we should either use the trusted cage base here or
    // remove the cage_base parameter.
    const i::PtrComprCageBase unused_cage_base(i::kNullAddress);
    VisitSlotImpl(host, unused_cage_base, slot);
  }

  void VisitJSDispatchTableEntry(i::Tagged<i::HeapObject> host,
                                 i::JSDispatchHandle handle) override {
    FieldLog;
    if (i::InstanceTypeChecker::IsJSFunction(instance_type_)) {
      int field_index = i::JSFunction::kDispatchHandleOffset / i::kTaggedSize;
      std::vector<bool>& visited = translator_->generator()->visited_fields_;
      if (visited[field_index]) {
        visited[field_index] = false;
      }
    }
  }

 private:
  // see heap-snapshot-generator.cc IndexedReferencesExtractor
  template <typename TSlot>
  void VisitSlotImpl(i::Tagged<i::HeapObject> host,
                     i::PtrComprCageBase cage_base, TSlot slot) {
    int field_index = static_cast<int>(slot.address() -
                                       host->RawMaybeWeakField(0).address()) /
                      TSlot::kSlotDataSize;
    std::vector<bool>& visited = translator_->generator()->visited_fields_;
    if (visited[field_index]) {
      visited[field_index] = false;
      return;
    }
    FieldLog;
    // NIY : if we want to get field_index, we must pass the
    // fake_object_addr
    i::Tagged<i::HeapObject> heap_object;
    // load the pointer
    auto loaded_value = slot.load(cage_base);
    if (loaded_value.GetHeapObjectIfStrong(&heap_object)) {
      VisitHeapObjectImpl(heap_object.address());
    } else if (loaded_value.GetHeapObjectIfWeak(&heap_object)) {
      translator_->generator()->SetWeakReference(parent_address_, next_index_++,
                                                 heap_object.address(), {});
    }
    // here we do nothing for off-heap object and smi
  }

  void VisitHeapObjectImpl(i::Address child) {
    translator_->generator()->SetHiddenReference(parent_address_, next_index_++,
                                                 child);
  }

  // see ResolveHandle in slots-inl.h
  void ResolveHandle(i::IndirectPointerHandle handle,
                     i::IndirectPointerTag tag) {}

  ObjectTranslator* translator_;
  i::Address parent_address_;
  i::PtrComprCageBase cage_base_;
  i::PtrComprCageBase code_cage_base_;
  i::InstanceType instance_type_;

  uint32_t next_index_{0};
};  // namespace dfx

//////////////////// FakeHeapObject
class FakeHeapObject {
 public:
  explicit FakeHeapObject(BinaryReaderBase* reader, uint32_t offset,
                          uint32_t size)
      : size_(size) {
    CHECK(offset && size);
    data_ =
        reinterpret_cast<uint8_t*>(::operator new(size, std::align_val_t{8}));
    CHECK(data_);
    CHECK(reader->ReadDataAt(offset, size, data_, size));
  }
  explicit FakeHeapObject(BinaryReaderBase* reader, uint32_t size)
      : size_(size) {
    data_ =
        reinterpret_cast<uint8_t*>(::operator new(size, std::align_val_t{8}));
    CHECK(data_);
    CHECK(reader->ReadData(size, data_, size));
  }
  explicit FakeHeapObject(BinaryReaderBase* reader,
                          ObjectTranslator::RawHeapObjectHead& head)
      : FakeHeapObject(reader, head.offset_, head.size_) {
    CHECK(head.offset_ && head.size_);
  }

  ~FakeHeapObject() {
    if (data_ != nullptr) {
      ::operator delete(data_, std::align_val_t{8});
      data_ = nullptr;
    }
  }
  i::Address fake_address() {
    i::Address addr = reinterpret_cast<i::Address>(data_);
    CHECK(!(addr & 0x7));
    return addr;
  }
  i::Tagged<i::HeapObject> fake_object() {
    return i::HeapObject::FromAddress(fake_address());
  }

  uint32_t real_size() {
    CHECK(size_ == kNoContentObjectDumpSize);
    return *reinterpret_cast<uint32_t*>(data_ + sizeof(i::Tagged_t));
  }

 private:
  uint8_t* data_{nullptr};
  const uint32_t size_;
};

//////////////////// ObjectTranslator
ObjectTranslator::ObjectTranslator(BinaryReaderBase* reader, uint32_t offset,
                                   uint32_t size, uint32_t count,
                                   i::PtrComprCageBase cage_base,
                                   i::PtrComprCageBase code_cage_base,
                                   SnapshotGenerator* generator)
    : reader_(reader),
      table_offset_(offset),
      table_size_(size),
      object_count_(count),
      cage_base_(cage_base),
      code_cage_base_(code_cage_base),
      generator_(generator) {
  CHECK(reader->CurrentPosition() == offset);
  if (!(size < std::numeric_limits<uint32_t>::max() - offset &&
        size + offset <= reader->BinarySize())) {
    LogInfo("size " + std::to_string(size) + " offset " +
            std::to_string(offset) + " reader size " +
            std::to_string(reader->BinarySize()));
    CHECK(false);
  }
}

void ObjectTranslator::PreVisit() {
  if (i::v8_flags.log_heapdump) {
    std::cout << "PreVisit\n";
  }
  // a raw object = address + size + map_pointer + [payload]
  const uint32_t MinSizeOfRawObject =
      sizeof(i::Address) + sizeof(uint32_t) + sizeof(i::Tagged_t);
  CHECK(object_count_ == 0 || table_size_ >= MinSizeOfRawObject);
  uint32_t table_end = table_size_ + table_offset_;

  // objects
  for (uint32_t i = 0; i < object_count_; i++) {
    CHECK(reader_->CurrentPosition() <= table_end - MinSizeOfRawObject);
    // read address and size
    i::Address address{0};
    CHECK(reader_->ReadData(sizeof(address),
                            reinterpret_cast<uint8_t*>(&address),
                            sizeof(address)));
    uint32_t dump_size{0};
    CHECK(reader_->ReadData(sizeof(dump_size),
                            reinterpret_cast<uint8_t*>(&dump_size),
                            sizeof(dump_size)));
    if (auto [it, inserted] = objects_head_.try_emplace(
            address,
            RawHeapObjectHead{static_cast<uint32_t>(reader_->CurrentPosition()),
                              dump_size
#ifdef OH_ENABLE_HEAP_DUMP_TEST
                              ,
                              false
#endif
            });
        !inserted) {
      std::cout << "HeapDump: same address insert twice addr " << address
                << " size " << dump_size << " offset "
                << reader_->CurrentPosition() << std::endl;
      UNREACHABLE();
    }
    CHECK(dump_size <= table_end - reader_->CurrentPosition());
    reader_->SetPosition(reader_->CurrentPosition() + dump_size);
  }

#if defined(OH_ENABLE_HEAP_DUMP_TEST)
  if (i::v8_flags.log_heapdump) {
    std::cout << "HeapDump after PreVisit: objects_head_.size"
              << objects_head_.size() << "\n";
    std::cout << "HeapDump after PreVisit: object_count_" << object_count_
              << "\n";
    std::cout << "HeapDump after PreVisit: table_end" << table_end << "\n";
    std::cout << "HeapDump after PreVisit: reader_->CurrentPosition()"
              << reader_->CurrentPosition() << "\n";
  }
#endif

  CHECK(objects_head_.size() == object_count_);
  CHECK(table_end == reader_->CurrentPosition());

  // maps and AddNode for every heap object
  reader_->SetPosition(table_offset_);
  for (uint32_t i = 0; i < object_count_; i++) {
    // read address and size
    i::Address address{0};
    uint32_t dump_size{0};
    CHECK(reader_->ReadData(sizeof(address),
                            reinterpret_cast<uint8_t*>(&address),
                            sizeof(address)));
    CHECK(reader_->ReadData(sizeof(dump_size),
                            reinterpret_cast<uint8_t*>(&dump_size),
                            sizeof(dump_size)));

    CHECK(dump_size <= table_end - reader_->CurrentPosition());
    FakeHeapObject fake_ho(reader_, dump_size);
    i::Tagged<i::HeapObject> fake_heap_object = fake_ho.fake_object();

    ////////// map
    i::Tagged<i::Map> map = fake_heap_object->map(cage_base_);

#ifdef OH_ENABLE_HEAP_DUMP_TEST
    if (auto [it, inserted] = objects_head_.try_emplace(
            map.address(), RawHeapObjectHead{0, 0, true});
        !inserted) {
      it->second.is_map_ = true;
    } else {
      std::cout << "HeapDump map not in heap: address " << map.address()
                << std::endl;
      UNREACHABLE();
    }
#endif
    FakeHeapObject fake_map_ho(reader_, objects_head_[map.address()].offset_,
                               objects_head_[map.address()].size_);
    i::Tagged<i::Map> fake_map =
        UncheckedCast<i::Map>(fake_map_ho.fake_object());

    uint32_t real_size = dump_size;
    if (SkipContentDump(fake_map)) {
      real_size = fake_ho.real_size();
    }
    if (!AddNode(fake_map, fake_heap_object, address, real_size)) {
      generator_->AddNode(address, Node::Type::kObject, "<dummy>", real_size);
    }
  }
}

void ObjectTranslator::Translate() {
  PreVisit();
  reader_->SetPosition(table_offset_);

  if (i::v8_flags.log_heapdump) {
    std::cout << "Translate\n";
  }
  for (uint32_t i = 0; i < object_count_; i++) {
    // has beed checked in PreVisit()
    i::Address address{0};
    uint32_t dump_size{0};
    CHECK(reader_->ReadData(sizeof(address),
                            reinterpret_cast<uint8_t*>(&address),
                            sizeof(address)));
    CHECK(reader_->ReadData(sizeof(dump_size),
                            reinterpret_cast<uint8_t*>(&dump_size),
                            sizeof(dump_size)));
#if defined(OH_ENABLE_HEAP_DUMP_TEST)
    if (i::v8_flags.log_heapdump) {
      std::cout << "address:" << address << " dump_size:" << dump_size
                << " object end:" << (address + dump_size) << std::endl;
    }
#endif
    CHECK(objects_head_.count(address));
    CHECK(objects_head_[address].offset_ == reader_->CurrentPosition());
    CHECK(objects_head_[address].size_ == dump_size);

    TranslateHeapObject(address, dump_size);
  }
}

// Add Edges
void ObjectTranslator::TranslateHeapObject(i::Address addr,
                                           uint32_t dump_size) {
  FakeHeapObject fake_ho(reader_, dump_size);
  i::Tagged<i::HeapObject> fake_heap_object = fake_ho.fake_object();

  ////////// map
  i::Tagged<i::Map> map = fake_heap_object->map(cage_base_);

  CHECK(objects_head_.count(map.address()));
  RawHeapObjectHead map_head = objects_head_[map.address()];

#if defined(OH_ENABLE_HEAP_DUMP_TEST)
  CHECK(map_head.is_map_);
#endif

  FakeHeapObject fake_map_obj(reader_, map_head.offset_, map_head.size_);
  i::Tagged<i::Map> fake_map =
      UncheckedCast<i::Map>(fake_map_obj.fake_object());
#if defined(OH_ENABLE_HEAP_DUMP_TEST)
  if (i::v8_flags.log_heapdump) {
    std::cout << "instance type:" << i::ToString(fake_map->instance_type())
              << " " << std::endl;
  }
#endif

  // copy from ObjectTranslator::IterateAndExtractReferences
  generator_->max_pointers_ = dump_size / i::kTaggedSize;
  if (generator_->max_pointers_ > generator_->visited_fields_.size()) {
    // Reallocate to right size.
    generator_->visited_fields_.resize(generator_->max_pointers_, false);
  }
  generator_->visited_fields_.assign(generator_->max_pointers_, false);

  // NIY:some instance may have different format

  ////////// visit object
  ObjectDetailVisitor visitor(this, addr, cage_base_, code_cage_base_,
                              fake_map->instance_type());

  if (SkipContentDump(fake_map)) {
    generator_->SetInternalReference(addr, "map", map.address(),
                                     i::HeapObject::kMapOffset);
    return;
  }
  // NIY: call ExtractReferences like heap snaphsot
  ExtractReferences(fake_heap_object, fake_map, addr);
  generator_->SetInternalReference(addr, "map", map.address(),
                                   i::HeapObject::kMapOffset);

  i::InstanceType instance_type = fake_map->instance_type();

  // see HeapVisitor<ConcreteVisitor>::Visit in heap-visitor-inl.h
  /*
  TYPED_VISITOR_WITH_SLACK_ID_LIST--->VisitJSObjectSubclass
  TYPED_VISITOR_ID_LIST
  TORQUE_VISITOR_ID_LIST
  TRUSTED_VISITOR_ID_LIST--->VisitWithBodyDescriptor
  */
  i::VisitorId visitor_id = fake_map->visitor_id();
  // white list
  switch (visitor_id) {
    case i::VisitorId::kVisitorIdCount:
    case i::VisitorId::kDataOnlyVisitorIdCount:
    case i::VisitorId::kVisitFreeSpace:
    case i::VisitorId::kVisitFiller:
      UNREACHABLE();
    case i::VisitorId::kVisitInstructionStream: {
      TranslateInstructionStream(addr, dump_size);
      return;
    }
#ifdef V8_ENABLE_WEBASSEMBLY
    case i::VisitorId::kVisitWasmArray: {
      // NIY:should we move it to dfx-heap-visitor-inl.h
      // see WasmArray::BodyDescriptor in objects-body-descriptors-inl.h
      if (!IsReferenceForWasmArray(fake_map)) {
        return;
      }
      i::BodyDescriptorBase::IteratePointers(
          fake_heap_object, i::WasmArray::kHeaderSize,
          i::WasmArray::BodyDescriptor::SizeOf(fake_map, fake_heap_object),
          &visitor);
      return;
    }
    case i::VisitorId::kVisitWasmStruct: {
      // NIY(hh):CanonicalStructType should dump
      i::InstanceType wasm_struct_type = fake_map->instance_type();
      std::string name = i::ToString(wasm_struct_type);
      LogInfo("Details in " + name + " won't show yet");
      return;
    }
    case i::VisitorId::kVisitWasmDispatchTable: {
      return WasmDispatchTableIterateBody(
          fake_map, fake_heap_object,
          i::WasmDispatchTable::BodyDescriptor::SizeOf(
              fake_map,
              i::UncheckedCast<i::WasmDispatchTable>(fake_heap_object)),
          &visitor);
    }
#endif
// copy from HeapVisitor<ConcreteVisitor>::Visit
#define CASE(TypeName) case i::VisitorId::kVisit##TypeName:
      DFX_TYPED_VISITOR_ID_LIST(CASE)
      DFX_TYPED_VISITOR_WITH_SLACK_ID_LIST(CASE)
      DFX_TORQUE_VISITOR_ID_LIST(CASE)
      DFX_TRUSTED_VISITOR_ID_LIST(CASE)
#undef CASE

    case i::VisitorId::kVisitShortcutCandidate:
      // VisitShortcutCandidate in heap-visitor-inl.h
    case i::VisitorId::kVisitJSObjectFast:
      // VisitJSApiObject in heap-visitor-inl.h, JSAPIObjectWithEmbedderSlots
    case i::VisitorId::kVisitJSApiObject:
    // case kVisitCppHeapExternalObject:DataOnly
    case i::VisitorId::kVisitStruct: {
      // Note: not all heap object can use Visit
      HeapObjectVisitor dfx_heap_object_visitor(&visitor, cage_base_,
                                                code_cage_base_);
      dfx_heap_object_visitor.Visit(fake_map, fake_heap_object);
      break;
    }
    default:
      if (!SkipContentDump(fake_map)) {
        std::string name = i::ToString(instance_type);
        LogInfo("[HeapDump]Instance type" + std::string(name) +
                "  not supported.");
        UNREACHABLE();
      }
      return;
  }
}

/////////////// InstructionStream

bool ObjectTranslator::IsStrong(i::Address addr) {
  if (addr & dfx::kRawHeapWeakObjectTag) {
    return false;
  }
  return true;
}

void ObjectTranslator::TranslateInstructionStream(i::Address addr,
                                                  uint32_t size) {
  uint32_t next_index = 0;
  // accroding to objects-body-descriptors-inl.h: class
  // InstructionStream::BodyDescripter total size: mapPointer + 2 *
  // protectedPointer + n * relocInfo (addr + mode)
  RawHeapObjectHead object_head = objects_head_[addr];
  i::Address protect_pointer_addr{0};
  uint32_t offset = object_head.offset_ + sizeof(i::Tagged_t);
  CHECK(reader_->ReadDataAt(offset, sizeof(i::Address),
                            reinterpret_cast<uint8_t*>(&protect_pointer_addr),
                            sizeof(i::Address)));
  this->generator()->SetHiddenReference(addr, next_index++,
                                        protect_pointer_addr);

  protect_pointer_addr = 0;
  offset += sizeof(i::Address);
  CHECK(reader_->ReadDataAt(offset, sizeof(i::Address),
                            reinterpret_cast<uint8_t*>(&protect_pointer_addr),
                            sizeof(i::Address)));
  this->generator()->SetHiddenReference(addr, next_index++,
                                        protect_pointer_addr);

  offset += sizeof(i::Address);
  int reloc_info_cnt = (size - sizeof(i::Tagged_t) - 2 * sizeof(i::Address)) /
                       (sizeof(i::Address) + sizeof(int8_t));
  for (int i = 0; i < reloc_info_cnt; i++) {
    InstructionStreamInfo info;
    reader_->ReadDataAt(offset, sizeof(i::Address),
                        reinterpret_cast<uint8_t*>(&info.object_addr_),
                        sizeof(i::Address));
    if (IsStrong(info.object_addr_)) {
      this->generator()->SetHiddenReference(addr, next_index++,
                                            info.object_addr_);
    } else {
      // NIY (weak reference)
      this->generator()->SetHiddenReference(
          addr, next_index++, info.object_addr_ ^ dfx::kRawHeapWeakObjectTag);
    }

    offset += sizeof(i::Address);
    reader_->ReadDataAt(offset, sizeof(info.mode_),
                        reinterpret_cast<uint8_t*>(&info.mode_),
                        sizeof(info.mode_));
    offset += sizeof(int8_t);
  }
}

////////// AddNode

// NIY(hh):see ObjectTranslator::AddEntry
Node* ObjectTranslator::AddNode(i::Tagged<i::Map> map,            // fake map
                                i::Tagged<i::HeapObject> object,  // fake object
                                i::Address address,  // real address
                                uint32_t size) {
  i::InstanceType instance_type = map->instance_type();
  if (i::InstanceTypeChecker::IsJSObject(instance_type)) {
    if (i::InstanceTypeChecker::IsJSFunction(instance_type)) {
      i::Tagged<i::JSFunction> func = i::UncheckedCast<i::JSFunction>(object);
      std::string name = get_name(func);
      if (name.empty()) {
        name = "JSFunction";
      }
      return generator_->AddNode(address, Node::Type::kClosure, name, size);

    } else if (i::InstanceTypeChecker::IsJSBoundFunction(instance_type)) {
      return generator_->AddNode(address, Node::Type::kClosure, "native_bind",
                                 size);
    }
    if (i::InstanceTypeChecker::IsJSRegExp(instance_type)) {
      i::Tagged<i::JSRegExp> re = i::UncheckedCast<i::JSRegExp>(object);
      // Get source string from JSRegExp
      // JSRegExp::source() is stored at kSourceOffset
      i::Tagged<i::Object> source_value =
          i::TaggedField<i::Tagged<i::Object>>::load(
              cage_base_, re, i::JSRegExp::kSourceOffset);
      std::string name = "(regexp)";
      if (i::IsHeapObject(source_value)) {
        i::Address source_addr = i::Cast<i::HeapObject>(source_value).address();
        name = get_string_content(source_addr);
        if (name.empty()) {
          name = "(regexp)";
        }
      }
      return generator_->AddNode(address, Node::Type::kRegExp, name, size);
    }
    // TODO(v8:12674) Fix and run full gcmole.
    // NIY:GetConstructorName
    i::DisableGCMole no_gcmole;
    std::string name = GetClassName(map, object, address);
    if (name.empty()) {
      UNREACHABLE();
    }
    if (i::InstanceTypeChecker::IsJSGlobalObject(instance_type)) {
      // NIY(hh): native_context_tag_map_ not available in translate-objects.cc
      // In heap-snapshot-generator.cc: uses native_context_tag_map_ to get
      // context tag
      name = name + "(global*)";
    } else if (i::InstanceTypeChecker::IsJSGlobalProxy(instance_type)) {
      // NIY(hh): native_context_tag_map_ not available - cannot get creation
      // context In heap-snapshot-generator.cc: uses GetCreationContext() and
      // native_context_tag_map_
      name = name + "(global)";
    }
    // NIY(hh): native_context_tag_map_ not available - cannot tag individual
    // objects In heap-snapshot-generator.cc: checks native_context_tag_map_ for
    // object-specific tags
    return generator_->AddNode(address, Node::Type::kObject, name, size);

  } else if (i::InstanceTypeChecker::IsString(instance_type)) {
    if (i::InstanceTypeChecker::IsConsString(instance_type)) {
      return generator_->AddNode(address, Node::Type::kConsString,
                                 "(concatenated string)", size);
    } else if (i::InstanceTypeChecker::IsSlicedString(instance_type)) {
      return generator_->AddNode(address, Node::Type::kSlicedString,
                                 "(sliced string)", size);
    } else if (i::InstanceTypeChecker::IsThinString(instance_type)) {
      return generator_->AddNode(address, Node::Type::kString, "(thin string)",
                                 size);
    } else if (i::InstanceTypeChecker::IsExternalString(instance_type)) {
      return generator_->AddNode(address, Node::Type::kString,
                                 "(external string)", size);
    } else {
      return generator_->AddNode(address, Node::Type::kString, "(string)",
                                 size);
    }
  } else if (i::InstanceTypeChecker::IsSymbol(instance_type)) {
    if (UncheckedCast<i::Symbol>(object)->is_private()) {
      return generator_->AddNode(address, Node::Type::kHidden, "private symbol",
                                 size);
    } else {
      return generator_->AddNode(address, Node::Type::kSymbol, "symbol", size);
    }

  } else if (i::InstanceTypeChecker::IsBigInt(instance_type)) {
    return generator_->AddNode(address, Node::Type::kBigInt, "bigint", size);
  } else if (i::InstanceTypeChecker::IsInstructionStream(instance_type) ||
             i::InstanceTypeChecker::IsCode(instance_type)) {
    return generator_->AddNode(address, Node::Type::kCode, "", size);
  } else if (i::InstanceTypeChecker::IsSharedFunctionInfo(instance_type)) {
    i::Tagged<i::SharedFunctionInfo> sfi =
        i::UncheckedCast<i::SharedFunctionInfo>(object);
    std::string name = get_name(sfi);
    if (name.empty()) {
      name = "shared function info";
    }
    return generator_->AddNode(address, Node::Type::kCode, name, size);

  } else if (i::InstanceTypeChecker::IsScript(instance_type)) {
    // Try to get script name from script->name() field
    // In heap-snapshot-generator.cc:
    // names_->GetName(Cast<Script>(object)->name())
    i::Tagged<i::Script> script = i::UncheckedCast<i::Script>(object);
    i::Tagged<i::Object> name_value =
        i::TaggedField<i::Tagged<i::Object>>::load(cage_base_, script,
                                                   i::Script::kNameOffset);
    std::string script_name = "script";
    if (i::IsHeapObject(name_value)) {
      i::Address name_addr = i::Cast<i::HeapObject>(name_value).address();
      // Try to get string content - may return empty due to no actual content
      script_name = get_string_content(name_addr);
      if (script_name.empty()) {
        script_name = "script";
      }
    }
    return generator_->AddNode(address, Node::Type::kCode, script_name, size);
  } else if (i::InstanceTypeChecker::IsNativeContext(instance_type)) {
    // NIY(hh): native_context_tag_map_ not available - cannot get context tag
    // In heap-snapshot-generator.cc: uses native_context_tag_map_ to get tag
    return generator_->AddNode(address, Node::Type::kHidden,
                               "system / NativeContext", size);
  } else if (i::InstanceTypeChecker::IsContext(instance_type)) {
    return generator_->AddNode(address, Node::Type::kObject, "system / Context",
                               size);
  } else if (i::InstanceTypeChecker::IsHeapNumber(instance_type)) {
    return generator_->AddNode(address, Node::Type::kHeapNumber, "heap number",
                               size);
  } else if (i::InstanceTypeChecker::IsOddball(instance_type)) {
    // Get to_string from Oddball
    i::Tagged<i::Oddball> oddball = i::UncheckedCast<i::Oddball>(object);
    // to_string_ is a TaggedMember<String>, see oddball.h and oddball-inl.h
    i::Tagged<i::Object> to_string_value = oddball->to_string();
    std::string name = "oddball";
    if (i::IsHeapObject(to_string_value)) {
      i::Address to_string_addr =
          i::Cast<i::HeapObject>(to_string_value).address();
      name = get_string_content(to_string_addr);
      if (name.empty()) {
        name = "oddball";
      }
    }
    return generator_->AddNode(address, Node::Type::kHidden, name, size);
  } else if (i::InstanceTypeChecker::IsCppHeapExternalObject(instance_type)) {
    return generator_->AddNode(address, Node::Type::kObject,
                               "system / CppHeapExternal", size);
  }
#if V8_ENABLE_WEBASSEMBLY
  if (i::InstanceTypeChecker::IsWasmObject(instance_type)) {
    // NIY(hh): Get wasm type info from object->map()->wasm_type_info()
    // In heap-snapshot-generator.cc: uses
    // GetCanonicalTypeNamesProvider()->PrintTypeName
    return generator_->AddNode(address, Node::Type::kObject, "(wasm)", size);
  }
  if (i::InstanceTypeChecker::IsWasmNull(instance_type)) {
    // Inlined copies of {GetSystemEntryType}, {GetSystemEntryName}, and
    // {AddEntry}, allowing us to override the size.
    // The actual object's size is fairly large (at the time of this writing,
    // just over 64 KB) and mostly includes a guard region. We report it as
    // much smaller to avoid confusion.
    static constexpr size_t kSize = i::WasmNull::kHeaderSize;
    return generator_->AddNode(address, Node::Type::kHidden,
                               "system / WasmNull", kSize);
  }
#endif  // V8_ENABLE_WEBASSEMBLY

  if (i::InstanceTypeChecker::IsForeign(instance_type)) {
    uint32_t node_size = size;
    std::string name =
        GetForeignName(i::UncheckedCast<i::Foreign>(object), &node_size);
    if (!name.empty()) {
      return generator_->AddNode(address, Node::Type::kHidden, name, node_size);
    }
    return generator_->AddNode(address, GetSystemEntryType(instance_type),
                               GetSystemEntryName(instance_type), node_size);
  }

  // NIY
  return generator_->AddNode(address, GetSystemEntryType(instance_type),
                             GetSystemEntryName(instance_type), size);
}

// copy from heap-snapshot-generator.cc:GetSystemEntryType
Node::Type ObjectTranslator::GetSystemEntryType(i::InstanceType type) {
  if (i::InstanceTypeChecker::IsAllocationSite(type) ||
      i::InstanceTypeChecker::IsArrayBoilerplateDescription(type) ||
      i::InstanceTypeChecker::IsBytecodeArray(type) ||
      i::InstanceTypeChecker::IsBytecodeWrapper(type) ||
      i::InstanceTypeChecker::IsClosureFeedbackCellArray(type) ||
      i::InstanceTypeChecker::IsCode(type) ||
      i::InstanceTypeChecker::IsCodeWrapper(type) ||
      i::InstanceTypeChecker::IsFeedbackCell(type) ||
      i::InstanceTypeChecker::IsFeedbackMetadata(type) ||
      i::InstanceTypeChecker::IsFeedbackVector(type) ||
      i::InstanceTypeChecker::IsInstructionStream(type) ||
      i::InstanceTypeChecker::IsInterpreterData(type) ||
      i::InstanceTypeChecker::IsLoadHandler(type) ||
      i::InstanceTypeChecker::IsObjectBoilerplateDescription(type) ||
      i::InstanceTypeChecker::IsPreparseData(type) ||
      i::InstanceTypeChecker::IsRegExpBoilerplateDescription(type) ||
      i::InstanceTypeChecker::IsScopeInfo(type) ||
      i::InstanceTypeChecker::IsStoreHandler(type) ||
      i::InstanceTypeChecker::IsTemplateObjectDescription(type) ||
      i::InstanceTypeChecker::IsTurbofanType(type) ||
      i::InstanceTypeChecker::IsUncompiledData(type)) {
    return Node::Type::kCode;
  }

  // This check must come second, because some subtypes of FixedArray are
  // determined above to represent code content.
  if (i::InstanceTypeChecker::IsFixedArray(type) ||
      i::InstanceTypeChecker::IsFixedDoubleArray(type) ||
      i::InstanceTypeChecker::IsByteArray(type)) {
    return Node::Type::kArray;
  }

  // NIY:Maps in read-only space are for internal V8 data, not user-defined
  // object shapes.
  if ((i::InstanceTypeChecker::IsMap(
          type)  // NIY: && !MemoryChunk::FromHeapObject(object)->InReadOnlySpace()
       ) ||
      i::InstanceTypeChecker::IsDescriptorArray(type) ||
      i::InstanceTypeChecker::IsTransitionArray(type) ||
      i::InstanceTypeChecker::IsPrototypeInfo(type) ||
      i::InstanceTypeChecker::IsEnumCache(type)) {
    return Node::Type::kObjectShape;
  }

  return Node::Type::kHidden;
}

// copy from heap-snapshot-generator.cc:GetSystemEntryName
const char* ObjectTranslator::GetSystemEntryName(i::InstanceType type) {
  if (i::InstanceTypeChecker::IsMap(type)) {
    switch (type) {
#define MAKE_STRING_MAP_CASE(instance_type, size, name, Name) \
  case i::InstanceType::instance_type:                        \
    return "system / Map (" #Name ")";
      STRING_TYPE_LIST(MAKE_STRING_MAP_CASE)
#undef MAKE_STRING_MAP_CASE
      default:
        return "system / Map";
    }
  }

  // Empty string names are special: TagObject can overwrite them, and devtools
  // will report them as "(internal array)".
  if (i::InstanceTypeChecker::IsFixedArray(type) ||
      i::InstanceTypeChecker::IsFixedDoubleArray(type) ||
      i::InstanceTypeChecker::IsByteArray(type)) {
    return "";
  }

  switch (type) {
#define MAKE_TORQUE_CASE(Name, TYPE) \
  case i::InstanceType::TYPE:        \
    return "system / " #Name;
    // The following lists include every non-String instance type.
    // This includes a few types that already have non-"system" names assigned
    // by AddEntry, but this is a convenient way to avoid manual upkeep here.
    TORQUE_INSTANCE_CHECKERS_SINGLE_FULLY_DEFINED(MAKE_TORQUE_CASE)
    TORQUE_INSTANCE_CHECKERS_MULTIPLE_FULLY_DEFINED(MAKE_TORQUE_CASE)
    TORQUE_INSTANCE_CHECKERS_SINGLE_ONLY_DECLARED(MAKE_TORQUE_CASE)
    TORQUE_INSTANCE_CHECKERS_MULTIPLE_ONLY_DECLARED(MAKE_TORQUE_CASE)
#undef MAKE_TORQUE_CASE

    // Strings were already handled by AddEntry.
#define MAKE_STRING_CASE(instance_type, size, name, Name) \
  case i::InstanceType::instance_type:                    \
    UNREACHABLE();
    STRING_TYPE_LIST(MAKE_STRING_CASE)
#undef MAKE_STRING_CASE

    case i::InstanceType::HOLE_TYPE:
      return "system / Hole";
  }

  // Avoid undefined behavior for enum values not handled by the exhaustive
  // switch, since they're read from inside the sandbox.
  CHECK(false);
}

std::string ObjectTranslator::GetForeignName(i::Tagged<i::Foreign> foreign,
                                             uint32_t* node_size) {
  USE(foreign);
  USE(node_size);
  // Upstream uses Foreign::GetTag() to recover managed external-pointer types.
  // Translator only has the dumped object bytes, which preserve the external
  // pointer handle but not the external pointer table entry that stores the
  // concrete tag. After isolate teardown that tag is no longer reconstructable
  // offline, so fall back to the generic system entry name instead of creating
  // a dummy node.
  return "";
}

// According to file src\objects\js-objects.cc:JSReceiver::class_name()
// NIY: GetConstructorHelper()?
std::string ObjectTranslator::GetClassName(i::Tagged<i::Map> map,
                                           i::Tagged<i::HeapObject> object,
                                           i::Address address) {
  i::InstanceType instance_type = map->instance_type();
  ObjectDetailVisitor visitor(this, address, cage_base_, code_cage_base_,
                              instance_type);
  // INTERNALIZED_STRING_LIST_GENERATOR in HeapObjectVisitor
  HeapObjectVisitor heap_object_visitor_tmp(&visitor, cage_base_,
                                            code_cage_base_);
  if (i::InstanceTypeChecker::IsJSFunctionOrBoundFunctionOrWrappedFunction(
          instance_type)) {
    return heap_object_visitor_tmp.Function_string();
  }
  if (i::InstanceTypeChecker::IsJSArgumentsObject(instance_type)) {
    return heap_object_visitor_tmp.Arguments_string();
  }
  if (i::InstanceTypeChecker::IsJSArray(instance_type)) {
    return heap_object_visitor_tmp.Array_string();
  }
  if (i::InstanceTypeChecker::IsJSArrayBuffer(instance_type)) {
    if (UncheckedCast<i::JSArrayBuffer>(object)->is_shared()) {
      return heap_object_visitor_tmp.SharedArrayBuffer_string();
    }
    return heap_object_visitor_tmp.ArrayBuffer_string();
  }
  if (i::InstanceTypeChecker::IsJSArrayIterator(instance_type)) {
    return heap_object_visitor_tmp.ArrayIterator_string();
  }
  if (i::InstanceTypeChecker::IsJSDate(instance_type)) {
    return heap_object_visitor_tmp.Date_string();
  }
  if (i::InstanceTypeChecker::IsJSError(instance_type)) {
    return heap_object_visitor_tmp.Error_string();
  }
  if (i::InstanceTypeChecker::IsJSGeneratorObject(instance_type)) {
    return heap_object_visitor_tmp.Generator_string();
  }
  if (i::InstanceTypeChecker::IsJSMap(instance_type)) {
    return heap_object_visitor_tmp.Map_string();
  }
  if (i::InstanceTypeChecker::IsJSMapIterator(instance_type)) {
    return heap_object_visitor_tmp.MapIterator_string();
  }
  if (i::InstanceTypeChecker::IsJSProxy(instance_type)) {
    return map->is_callable() ? heap_object_visitor_tmp.Function_string()
                              : heap_object_visitor_tmp.Object_string();
  }
  if (i::InstanceTypeChecker::IsJSRegExp(instance_type)) {
    return heap_object_visitor_tmp.RegExp_string();
  }
  if (i::InstanceTypeChecker::IsJSSet(instance_type)) {
    return heap_object_visitor_tmp.Set_string();
  }
  if (i::InstanceTypeChecker::IsJSSetIterator(instance_type)) {
    return heap_object_visitor_tmp.SetIterator_string();
  }
  if (i::InstanceTypeChecker::IsJSTypedArray(instance_type)) {
#define SWITCH_STRING(Type, type, TYPE, ctype)           \
  if (map->elements_kind() == i::TYPE##_ELEMENTS) {      \
    return heap_object_visitor_tmp.Type##Array_string(); \
  }
    TYPED_ARRAYS(SWITCH_STRING)
#undef SWITCH_STRING
  }

  if (i::InstanceTypeChecker::IsJSPrimitiveWrapper(instance_type)) {
    i::Tagged<i::Object> value =
        i::TaggedField<i::Tagged<i::JSPrimitiveWrapper>>::load(
            cage_base_, object, i::JSPrimitiveWrapper::kValueOffset);
    if (i::IsSmi(value)) {
      return heap_object_visitor_tmp.Number_string();
    }
    if (i::IsHeapObject(value)) {
      i::Tagged<i::HeapObject> value_heap_object = Cast<i::HeapObject>(value);
      FakeHeapObject fObject(reader_,
                             objects_head_[value_heap_object.address()]);
      i::Tagged<i::HeapObject> fake_object = fObject.fake_object();

      FakeHeapObject fake_map_ho(
          reader_, objects_head_[fake_object->map(cage_base_).address()]);
      i::Tagged<i::Map> fake_map =
          UncheckedCast<i::Map>(fake_map_ho.fake_object());

      i::InstanceType type = fake_map->instance_type();
      if (IsBoolean(type, fake_object)) {
        return heap_object_visitor_tmp.Boolean_string();
      }
      if (i::InstanceTypeChecker::IsString(type)) {
        return heap_object_visitor_tmp.String_string();
      }
      if (i::InstanceTypeChecker::IsHeapNumber(type)) {
        return heap_object_visitor_tmp.Number_string();
      }
      if (i::InstanceTypeChecker::IsBigInt(type)) {
        return heap_object_visitor_tmp.BigInt_string();
      }
      if (i::InstanceTypeChecker::IsSymbol(type)) {
        return heap_object_visitor_tmp.Symbol_string();
      }
      if (i::InstanceTypeChecker::IsScript(type)) {
        return heap_object_visitor_tmp.Script_string();
      }
    }
    UNREACHABLE();
  }

  if (i::InstanceTypeChecker::IsJSWeakMap(instance_type)) {
    return heap_object_visitor_tmp.WeakMap_string();
  }
  if (i::InstanceTypeChecker::IsJSWeakSet(instance_type)) {
    return heap_object_visitor_tmp.WeakSet_string();
  }
  if (i::InstanceTypeChecker::IsJSGlobalProxy(instance_type)) {
    return heap_object_visitor_tmp.global_string();
  }
  if (IsShared(instance_type)) {
    if (i::InstanceTypeChecker::IsJSSharedStruct(instance_type)) {
      return heap_object_visitor_tmp.SharedStruct_string();
    }
    if (i::InstanceTypeChecker::IsJSSharedArray(instance_type)) {
      return heap_object_visitor_tmp.SharedArray_string();
    }
    if (i::InstanceTypeChecker::IsJSAtomicsMutex(instance_type)) {
      return heap_object_visitor_tmp.AtomicsMutex_string();
    }
    if (i::InstanceTypeChecker::IsJSAtomicsCondition(instance_type)) {
      return heap_object_visitor_tmp.AtomicsCondition_string();
    }
    UNREACHABLE();
  }
  return heap_object_visitor_tmp.Object_string();
}

// copy from DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsBoolean) in oddball-inl.h
bool ObjectTranslator::IsBoolean(i::InstanceType instance_type,
                                 i::Tagged<i::Object> obj) {
  return i::InstanceTypeChecker::IsOddball(instance_type) &&
         ((UncheckedCast<i::Oddball>(obj)->kind() &
           i::Oddball::kNotBooleanMask) == 0);
}

// NIY objects-inl.h
bool ObjectTranslator::IsShared(i::InstanceType instance_type) {
  // NIY IsReadOnlySpaceShared

  if (i::InstanceTypeChecker::IsAlwaysSharedSpaceJSObject(instance_type)) {
    return true;
  }
  switch (instance_type) {
    case i::SHARED_SEQ_TWO_BYTE_STRING_TYPE:
    case i::SHARED_SEQ_ONE_BYTE_STRING_TYPE:
    case i::SHARED_EXTERNAL_TWO_BYTE_STRING_TYPE:
    case i::SHARED_EXTERNAL_ONE_BYTE_STRING_TYPE:
    case i::SHARED_UNCACHED_EXTERNAL_TWO_BYTE_STRING_TYPE:
    case i::SHARED_UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE:
      return true;
    case i::INTERNALIZED_TWO_BYTE_STRING_TYPE:
    case i::INTERNALIZED_ONE_BYTE_STRING_TYPE:
    case i::EXTERNAL_INTERNALIZED_TWO_BYTE_STRING_TYPE:
    case i::EXTERNAL_INTERNALIZED_ONE_BYTE_STRING_TYPE:
    case i::UNCACHED_EXTERNAL_INTERNALIZED_TWO_BYTE_STRING_TYPE:
    case i::UNCACHED_EXTERNAL_INTERNALIZED_ONE_BYTE_STRING_TYPE:
      if (i::v8_flags.shared_string_table) {
        return true;
      }
      return false;
    // NIY HEAP_NUMBER_TYPE-InWritableSharedSpace
    default:
      return false;
  }
}

std::string ObjectTranslator::get_name(i::Tagged<i::JSFunction> func) {
  // Get SharedFunctionInfo from JSFunction
  // Pattern for fake objects: load field, get address, create FakeHeapObject
  i::Tagged<i::Object> sfi_value = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, func, i::JSFunction::kSharedFunctionInfoOffset);

  if (!i::IsHeapObject(sfi_value)) {
    return "";
  }

  i::Address sfi_addr = i::Cast<i::HeapObject>(sfi_value).address();
  auto it = objects_head_.find(sfi_addr);
  CHECK(it != objects_head_.end());

  // Create FakeHeapObject to read SharedFunctionInfo
  FakeHeapObject fake_sfi_ho(reader_, it->second.offset_, it->second.size_);
  i::Tagged<i::SharedFunctionInfo> fake_sfi =
      i::UncheckedCast<i::SharedFunctionInfo>(fake_sfi_ho.fake_object());

  return get_name(fake_sfi);
}

// See SharedFunctionInfo::Name() in shared-function-info-inl.h
std::string ObjectTranslator::get_name(i::Tagged<i::SharedFunctionInfo> sfi) {
  // Get name_or_scope_info field from SharedFunctionInfo
  i::Tagged<i::Object> name_or_scope_info =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, sfi, i::SharedFunctionInfo::kNameOrScopeInfoOffset);

  if (!i::IsHeapObject(name_or_scope_info)) {
    // Could be kNoSharedNameSentinel (Smi)
    return "";
  }

  i::Tagged<i::HeapObject> name_obj =
      i::Cast<i::HeapObject>(name_or_scope_info);
  i::Address name_addr = name_obj.address();

  auto it = objects_head_.find(name_addr);
  CHECK(it != objects_head_.end());

  // Create FakeHeapObject to read the name object
  FakeHeapObject fake_name_ho(reader_, it->second.offset_, it->second.size_);
  i::Tagged<i::HeapObject> fake_name_obj = fake_name_ho.fake_object();

  // Get map to check the instance type
  i::Tagged<i::Map> name_map = fake_name_obj->map(cage_base_);
  auto map_it = objects_head_.find(name_map.address());

  CHECK(map_it != objects_head_.end());

  FakeHeapObject fake_map_ho(reader_, map_it->second.offset_,
                             map_it->second.size_);
  i::Tagged<i::Map> fake_name_map =
      i::UncheckedCast<i::Map>(fake_map_ho.fake_object());

  i::InstanceType name_type = fake_name_map->instance_type();

  // Could be a ScopeInfo - would need to extract function name from it
  if (i::InstanceTypeChecker::IsScopeInfo(name_type)) {
    // NIY: Extract function name from ScopeInfo
    // NIY get_name(UncheckedCast<i::ScopeInfo>(fake_name_obj));
    return "(function name in scope)";
  }

  // Check if it's a String - if so, try to read its content
  if (i::InstanceTypeChecker::IsString(name_type)) {
    return get_string_content(name_addr);
  }

  return "";
}

std::string ObjectTranslator::get_string_content(i::Address string_addr) {
  USE(string_addr);
  // Heap dump intentionally strips string payload. Translator can only recover
  // type-level placeholders in AddNode, so content lookup always stays empty.
  return "";
}

std::string ObjectTranslator::get_string_content(
    i::Tagged<i::String> string, i::InstanceType instance_type) {
  USE(string);
  USE(instance_type);
  return "";
}

/////////////// ExtractReference

// copy from heap-snapshot-generator.cc
void ObjectTranslator::ExtractReferences(
    i::Tagged<i::HeapObject> obj,  // fake_obj
    i::Tagged<i::Map> map,         // fake_map
    i::Address real_address) {
  i::InstanceType instance_type = map->instance_type();
  if (i::InstanceTypeChecker::IsJSGlobalProxy(instance_type)) {
    ExtractJSGlobalProxyReferences(real_address,
                                   UncheckedCast<i::JSGlobalProxy>(obj), map);
  } else if (i::InstanceTypeChecker::IsJSArrayBuffer(instance_type)) {
    ExtractJSArrayBufferReferences(real_address,
                                   UncheckedCast<i::JSArrayBuffer>(obj));
  } else if (i::InstanceTypeChecker::IsJSObject(instance_type)) {
    if (i::InstanceTypeChecker::IsJSWeakSet(instance_type)) {
      ExtractJSWeakCollectionReferences(real_address,
                                        UncheckedCast<i::JSWeakSet>(obj));
    } else if (i::InstanceTypeChecker::IsJSWeakMap(instance_type)) {
      ExtractJSWeakCollectionReferences(real_address,
                                        UncheckedCast<i::JSWeakMap>(obj));

    } else if (i::InstanceTypeChecker::IsJSSet(instance_type)) {
      ExtractJSCollectionReferences(real_address, UncheckedCast<i::JSSet>(obj));
    } else if (i::InstanceTypeChecker::IsJSMap(instance_type)) {
      ExtractJSCollectionReferences(real_address, UncheckedCast<i::JSMap>(obj));
    } else if (i::InstanceTypeChecker::IsJSPromise(instance_type)) {
      ExtractJSPromiseReferences(real_address,
                                 UncheckedCast<i::JSPromise>(obj));
    } else if (i::InstanceTypeChecker::IsJSGeneratorObject(instance_type)) {
      ExtractJSGeneratorObjectReferences(
          real_address, UncheckedCast<i::JSGeneratorObject>(obj));
    } else if (i::InstanceTypeChecker::IsJSWeakRef(instance_type)) {
      ExtractJSWeakRefReferences(real_address,
                                 UncheckedCast<i::JSWeakRef>(obj));
    } else if (i::InstanceTypeChecker::IsJSDisposableStackBase(instance_type)) {
      ExtractJSDisposableStackReferences(
          real_address, UncheckedCast<i::JSDisposableStackBase>(obj));
#if V8_ENABLE_WEBASSEMBLY
    } else if (i::InstanceTypeChecker::IsWasmInstanceObject(instance_type)) {
      ExtractWasmInstanceObjectReferences(
          real_address, UncheckedCast<i::WasmInstanceObject>(obj));
    } else if (i::InstanceTypeChecker::IsWasmModuleObject(instance_type)) {
      ExtractWasmModuleObjectReferences(
          real_address, UncheckedCast<i::WasmModuleObject>(obj));
#endif  // V8_ENABLE_WEBASSEMBLY
    }
    // Extract basic JSObject references: properties, elements, prototype
    ExtractJSObjectReferences(real_address, i::UncheckedCast<i::JSObject>(obj),
                              map);
  } else if (i::InstanceTypeChecker::IsString(instance_type)) {
    ExtractStringReferences(real_address, Cast<i::String>(obj), instance_type);
  } else if (i::InstanceTypeChecker::IsSymbol(instance_type)) {
    ExtractSymbolReferences(real_address, i::Cast<i::Symbol>(obj));
  } else if (i::InstanceTypeChecker::IsMap(instance_type)) {
    ExtractMapReferences(real_address, i::UncheckedCast<i::Map>(obj));
  } else if (i::InstanceTypeChecker::IsSharedFunctionInfo(instance_type)) {
    ExtractSharedFunctionInfoReferences(
        real_address, i::UncheckedCast<i::SharedFunctionInfo>(obj), map);
  } else if (i::InstanceTypeChecker::IsScript(instance_type)) {
    ExtractScriptReferences(real_address, i::UncheckedCast<i::Script>(obj),
                            map);
  } else if (i::InstanceTypeChecker::IsAccessorInfo(instance_type)) {
    ExtractAccessorInfoReferences(real_address,
                                  i::UncheckedCast<i::AccessorInfo>(obj));
  } else if (i::InstanceTypeChecker::IsAccessorPair(instance_type)) {
    ExtractAccessorPairReferences(real_address,
                                  i::UncheckedCast<i::AccessorPair>(obj));
  } else if (i::Tagged<i::Code> code; TryCast(obj, &code)) {
    ExtractCodeReferences(real_address, code);
  } else if (i::Tagged<i::InstructionStream> istream; TryCast(obj, &istream)) {
    ExtractInstructionStreamReferences(real_address, istream);
  } else if (i::InstanceTypeChecker::IsCell(instance_type)) {
    ExtractCellReferences(real_address, i::UncheckedCast<i::Cell>(obj));
  } else if (i::InstanceTypeChecker::IsFeedbackCell(instance_type)) {
    ExtractFeedbackCellReferences(real_address,
                                  i::UncheckedCast<i::FeedbackCell>(obj));
  } else if (i::InstanceTypeChecker::IsPropertyCell(instance_type)) {
    ExtractPropertyCellReferences(real_address,
                                  i::UncheckedCast<i::PropertyCell>(obj));
  } else if (i::InstanceTypeChecker::IsPrototypeInfo(instance_type)) {
    ExtractPrototypeInfoReferences(real_address,
                                   i::UncheckedCast<i::PrototypeInfo>(obj));
  } else if (i::InstanceTypeChecker::IsAllocationSite(instance_type)) {
    ExtractAllocationSiteReferences(real_address,
                                    i::UncheckedCast<i::AllocationSite>(obj));
  } else if (i::InstanceTypeChecker::IsArrayBoilerplateDescription(
                 instance_type)) {
    ExtractArrayBoilerplateDescriptionReferences(
        real_address, i::UncheckedCast<i::ArrayBoilerplateDescription>(obj));
  } else if (i::InstanceTypeChecker::IsRegExpBoilerplateDescription(
                 instance_type)) {
    // Tag-only path in heap-snapshot-generator.cc. Keep current behavior.
  } else if (i::InstanceTypeChecker::IsFeedbackVector(instance_type)) {
    ExtractFeedbackVectorReferences(real_address,
                                    i::UncheckedCast<i::FeedbackVector>(obj));
  } else if (i::InstanceTypeChecker::IsDescriptorArray(instance_type)) {
    ExtractDescriptorArrayReferences(real_address,
                                     i::UncheckedCast<i::DescriptorArray>(obj));
  } else if (i::InstanceTypeChecker::IsEnumCache(instance_type)) {
    ExtractEnumCacheReferences(real_address,
                               i::UncheckedCast<i::EnumCache>(obj));
  } else if (i::InstanceTypeChecker::IsTransitionArray(instance_type)) {
    ExtractTransitionArrayReferences(real_address,
                                     i::UncheckedCast<i::TransitionArray>(obj));
  } else if (i::InstanceTypeChecker::IsWeakFixedArray(instance_type)) {
    ExtractWeakArrayReferences(OFFSET_OF_DATA_START(i::WeakFixedArray),
                               real_address,
                               i::UncheckedCast<i::WeakFixedArray>(obj));
  } else if (i::InstanceTypeChecker::IsWeakArrayList(instance_type)) {
    ExtractWeakArrayReferences(i::WeakArrayList::kHeaderSize, real_address,
                               i::UncheckedCast<i::WeakArrayList>(obj));
  } else if (i::InstanceTypeChecker::IsContext(instance_type)) {
    ExtractContextReferences(real_address, i::UncheckedCast<i::Context>(obj),
                             instance_type);
  } else if (i::InstanceTypeChecker::IsEphemeronHashTable(instance_type)) {
    ExtractEphemeronHashTableReferences(
        real_address, i::UncheckedCast<i::EphemeronHashTable>(obj));
  } else if (i::InstanceTypeChecker::IsFixedArray(instance_type)) {
    ExtractFixedArrayReferences(real_address,
                                i::UncheckedCast<i::FixedArray>(obj));
  } else if (i::InstanceTypeChecker::IsWeakCell(instance_type)) {
    ExtractWeakCellReferences(real_address, i::UncheckedCast<i::WeakCell>(obj));
  } else if (i::InstanceTypeChecker::IsHeapNumber(instance_type)) {
    // NIY:if (snapshot_->capture_numeric_value()) { }
  } else if (i::Tagged<i::BytecodeArray> bytecode_array;
             TryCast(obj, &bytecode_array)) {
    ExtractBytecodeArrayReferences(real_address, bytecode_array);
  } else if (i::InstanceTypeChecker::IsScopeInfo(instance_type)) {
    ExtractScopeInfoReferences(real_address,
                               i::UncheckedCast<i::ScopeInfo>(obj));
  } else if (i::InstanceTypeChecker::IsCppHeapExternalObject(instance_type)) {
    ExtractCppHeapExternalReferences(
        real_address, i::UncheckedCast<i::CppHeapExternalObject>(obj));
#if V8_ENABLE_WEBASSEMBLY
  } else if (i::InstanceTypeChecker::IsWasmStruct(instance_type)) {
    ExtractWasmStructReferences(real_address, i::Cast<i::WasmStruct>(obj));
  } else if (i::InstanceTypeChecker::IsWasmArray(instance_type)) {
    ExtractWasmArrayReferences(real_address, i::Cast<i::WasmArray>(obj), map);
  } else if (i::InstanceTypeChecker::IsWasmTrustedInstanceData(instance_type)) {
    ExtractWasmTrustedInstanceDataReferences(real_address, obj);
#endif  // V8_ENABLE_WEBASSEMBLY
  }
}

void ObjectTranslator::ExtractJSGlobalProxyReferences(
    i::Address real_address, i::Tagged<i::JSGlobalProxy> proxy,
    i::Tagged<i::Map> map) {
  USE(proxy);
  // see JSReceiver::GetCreationContext
  FakeHeapObject fake_meta_map_obj(
      reader_, objects_head_[map->map(cage_base_).address()]);
  i::Tagged<i::Map> fake_meta_map =
      UncheckedCast<i::Map>(fake_meta_map_obj.fake_object());
  i::Tagged<i::Object> maybe_native_context =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, fake_meta_map,
          i::Map::kConstructorOrBackPointerOrNativeContextOffset);
  if (!i::IsHeapObject(maybe_native_context)) {
    return;
  }
  i::Address native_context_addr =
      i::Cast<i::HeapObject>(maybe_native_context).address();
  auto native_context_it = objects_head_.find(native_context_addr);
  if (native_context_it == objects_head_.end()) {
    return;
  }

  generator_->SetShortcutReference(real_address, "native_context",
                                   native_context_addr);

  // native_context->global_object()
  FakeHeapObject fake_native_context(reader_, native_context_it->second);
  i::Tagged<i::NativeContext> fake_context =
      UncheckedCast<i::NativeContext>(fake_native_context.fake_object());
  i::Tagged<i::Object> global_obj = i::TaggedField<i::Object>::Relaxed_Load(
      cage_base_, fake_context,
      i::Context::OffsetOfElementAt(i::Context::Field::EXTENSION_INDEX));
  if (i::IsHeapObject(global_obj)) {
    i::Tagged<i::HeapObject> heap_object = Cast<i::HeapObject>(global_obj);
    generator_->SetShortcutReference(real_address, "global_object",
                                     heap_object.address());
  }
}

void ObjectTranslator::ExtractPropertyReferences(i::Address real_address,
                                                 i::Tagged<i::JSObject> js_obj,
                                                 i::Tagged<i::Map> map) {
  return;
}

void ObjectTranslator::ExtractElementReferences(i::Address real_address,
                                                i::Tagged<i::JSObject> js_obj,
                                                i::Tagged<i::Map> map) {}

void ObjectTranslator::ExtractInternalReferences(i::Address real_address,
                                                 i::Tagged<i::JSObject> js_obj,
                                                 i::Tagged<i::Map> map) {}

// NIY TagObject
void ObjectTranslator::ExtractJSObjectReferences(i::Address real_address,
                                                 i::Tagged<i::JSObject> js_obj,
                                                 i::Tagged<i::Map> map) {
  i::InstanceType instance_type = map->instance_type();
  ExtractPropertyReferences(real_address, js_obj, map);
  ExtractElementReferences(real_address, js_obj, map);
  ExtractInternalReferences(real_address, js_obj, map);
  // NIY  SetPropertyReference(entry, roots.proto_string(), iter.GetCurrent());

  if (i::InstanceTypeChecker::IsJSBoundFunction(instance_type)) {
    // Handle JSBoundFunction references
    i::Tagged<i::JSBoundFunction> bound_fun =
        i::UncheckedCast<i::JSBoundFunction>(js_obj);

    // bound_arguments
    i::Tagged<i::Object> bound_args =
        i::TaggedField<i::Tagged<i::Object>>::load(
            cage_base_, bound_fun, i::JSBoundFunction::kBoundArgumentsOffset);
    if (i::IsHeapObject(bound_args)) {
      i::Tagged<i::HeapObject> args_heap = i::Cast<i::HeapObject>(bound_args);
      generator_->SetInternalReference(
          real_address, "bindings", args_heap.address(),
          i::JSBoundFunction::kBoundArgumentsOffset);
    }

    // bound_this
    i::Tagged<i::Object> bound_this =
        i::TaggedField<i::Tagged<i::Object>>::load(
            cage_base_, bound_fun, i::JSBoundFunction::kBoundThisOffset);
    if (i::IsHeapObject(bound_this)) {
      i::Tagged<i::HeapObject> this_heap = i::Cast<i::HeapObject>(bound_this);
      generator_->SetInternalReference(real_address, "bound_this",
                                       this_heap.address(),
                                       i::JSBoundFunction::kBoundThisOffset);
    }

    // bound_target_function
    i::Tagged<i::Object> bound_target =
        i::TaggedField<i::Tagged<i::Object>>::load(
            cage_base_, bound_fun,
            i::JSBoundFunction::kBoundTargetFunctionOffset);
    if (i::IsHeapObject(bound_target)) {
      i::Tagged<i::HeapObject> target_heap =
          i::Cast<i::HeapObject>(bound_target);
      generator_->SetInternalReference(
          real_address, "bound_function", target_heap.address(),
          i::JSBoundFunction::kBoundTargetFunctionOffset);
    }
  } else if (i::InstanceTypeChecker::IsJSFunction(instance_type)) {
    // Handle JSFunction-specific references
    i::Tagged<i::JSFunction> js_fun = i::UncheckedCast<i::JSFunction>(js_obj);

    // prototype_or_initial_map
    // if (js_fun->has_prototype_slot()) {
    // NIY
    // }

    // raw_feedback_cell
    i::Tagged<i::Object> feedback_cell =
        i::TaggedField<i::Tagged<i::Object>>::load(
            cage_base_, js_fun, i::JSFunction::kFeedbackCellOffset);
    if (i::IsHeapObject(feedback_cell)) {
      i::Tagged<i::HeapObject> cell_heap =
          i::Cast<i::HeapObject>(feedback_cell);
      generator_->SetInternalReference(real_address, "feedback_cell",
                                       cell_heap.address(),
                                       i::JSFunction::kFeedbackCellOffset);
    }

    // SharedFunctionInfo
    i::Tagged<i::Object> shared_value =
        i::TaggedField<i::Tagged<i::Object>>::load(
            cage_base_, js_fun, i::JSFunction::kSharedFunctionInfoOffset);
    if (i::IsHeapObject(shared_value)) {
      i::Tagged<i::HeapObject> shared_heap =
          i::Cast<i::HeapObject>(shared_value);
      generator_->SetInternalReference(
          real_address, "shared", shared_heap.address(),
          i::JSFunction::kSharedFunctionInfoOffset);
    }

    // Context
    i::Tagged<i::Object> context_value =
        i::TaggedField<i::Tagged<i::Object>>::load(
            cage_base_, js_fun, i::JSFunction::kContextOffset);
    if (i::IsHeapObject(context_value)) {
      i::Tagged<i::HeapObject> context_heap =
          i::Cast<i::HeapObject>(context_value);
      generator_->SetInternalReference(real_address, "context",
                                       context_heap.address(),
                                       i::JSFunction::kContextOffset);
    }

    // NIY: code (dispatch handle)
    // generator_->SetInternalReference(real_address, "code", 0,
    //                                  i::JSFunction::kDispatchHandleOffset);
  } else if (i::InstanceTypeChecker::IsJSGlobalObject(instance_type)) {
    // Handle JSGlobalObject references
    i::Tagged<i::JSGlobalObject> global_obj =
        i::UncheckedCast<i::JSGlobalObject>(js_obj);
    // global_proxy
    i::Tagged<i::Object> global_proxy =
        i::TaggedField<i::Tagged<i::Object>>::load(
            cage_base_, global_obj, i::JSGlobalObject::kGlobalProxyOffset);
    if (i::IsHeapObject(global_proxy)) {
      i::Tagged<i::HeapObject> proxy_heap =
          i::Cast<i::HeapObject>(global_proxy);
      generator_->SetInternalReference(real_address, "global_proxy",
                                       proxy_heap.address(),
                                       i::JSGlobalObject::kGlobalProxyOffset);
    }
  } else if (i::InstanceTypeChecker::IsJSArrayBufferView(instance_type)) {
    // Handle JSArrayBufferView references
    i::Tagged<i::JSArrayBufferView> view =
        i::UncheckedCast<i::JSArrayBufferView>(js_obj);
    // buffer
    i::Tagged<i::Object> buffer = i::TaggedField<i::Tagged<i::Object>>::load(
        cage_base_, view, i::JSArrayBufferView::kBufferOffset);
    if (i::IsHeapObject(buffer)) {
      i::Tagged<i::HeapObject> buffer_heap = i::Cast<i::HeapObject>(buffer);
      generator_->SetInternalReference(real_address, "buffer",
                                       buffer_heap.address(),
                                       i::JSArrayBufferView::kBufferOffset);
    }
  }

  // Extract properties (stored in raw_properties_or_hash field)
  i::Tagged<i::Object> properties_or_hash =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, js_obj, i::JSObject::kPropertiesOrHashOffset);
  if (i::IsHeapObject(properties_or_hash)) {
    i::Tagged<i::HeapObject> props_heap =
        i::Cast<i::HeapObject>(properties_or_hash);
    generator_->SetInternalReference(real_address, "properties",
                                     props_heap.address(),
                                     i::JSObject::kPropertiesOrHashOffset);
  }

  // Extract elements
  i::Tagged<i::Object> elements = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, js_obj, i::JSObject::kElementsOffset);
  if (i::IsHeapObject(elements)) {
    i::Tagged<i::HeapObject> elements_heap = i::Cast<i::HeapObject>(elements);
    generator_->SetInternalReference(real_address, "elements",
                                     elements_heap.address(),
                                     i::JSObject::kElementsOffset);
  }
}

void ObjectTranslator::ExtractJSWeakCollectionReferences(
    i::Address real_address, i::Tagged<i::JSWeakCollection> obj) {
  // js-collection-tq-inl.inc
  i::Tagged<i::Object> value = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, obj, i::JSWeakCollection::kTableOffset);

  if (i::IsHeapObject(value)) {
    i::Tagged<i::HeapObject> heap_object = Cast<i::HeapObject>(value);
    generator_->SetInternalReference(real_address, "table",
                                     heap_object.address(),
                                     i::JSWeakCollection::kTableOffset);
  }
}

void ObjectTranslator::ExtractJSCollectionReferences(
    i::Address real_address, i::Tagged<i::JSCollection> collection) {
  // JSCollection has a table field at kTableOffset.
  // Use TaggedField::load to read the pointer field without dereferencing
  // since FakeHeapObject doesn't have valid memory at pointer targets.
  i::Tagged<i::Object> value = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, collection, i::JSCollection::kTableOffset);

  if (i::IsHeapObject(value)) {
    i::Tagged<i::HeapObject> heap_object = Cast<i::HeapObject>(value);
    generator_->SetInternalReference(real_address, "table",
                                     heap_object.address(),
                                     i::JSCollection::kTableOffset);
  }
}

void ObjectTranslator::ExtractJSPromiseReferences(
    i::Address real_address, i::Tagged<i::JSPromise> promise) {
  // JSPromise has a reactions_or_result field at kReactionsOrResultOffset.
  // Use TaggedField::load to read the pointer field without dereferencing
  // since FakeHeapObject doesn't have valid memory at pointer targets.
  i::Tagged<i::Object> value = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, promise, i::JSPromise::kReactionsOrResultOffset);

  if (i::IsHeapObject(value)) {
    i::Tagged<i::HeapObject> heap_object = Cast<i::HeapObject>(value);
    generator_->SetInternalReference(real_address, "reactions_or_result",
                                     heap_object.address(),
                                     i::JSPromise::kReactionsOrResultOffset);
  }
}

void ObjectTranslator::ExtractJSArrayBufferReferences(
    i::Address real_address, i::Tagged<i::JSArrayBuffer> buffer) {
  // Setup a reference to a native memory backing_store object.
  // backing_store is an external pointer (off-heap), not a HeapObject.
  // Note: detach_key will be handled by the normal object visitor during
  // dfx_heap_object_visitor.Visit(), so we only need to handle backing_store
  // here.
  i::Address backing_store_raw =
      buffer->ReadSandboxedPointerField(i::JSArrayBuffer::kBackingStoreOffset,
                                        cage_base_);
  void* backing_store = reinterpret_cast<void*>(backing_store_raw);
  if (!backing_store) {
    return;
  }

  // Get byte_length for the size of the backing store
  size_t byte_length = buffer->byte_length();

  // Create a synthetic node for the backing store native memory.
  // Use the backing_store address as the unique identifier for the node.
  i::Address backing_store_addr = reinterpret_cast<i::Address>(backing_store);
  generator_->AddNode(backing_store_addr, Node::Type::kNative,
                      "system / JSArrayBufferData",
                      static_cast<uint32_t>(byte_length));

  // Add an internal reference from the JSArrayBuffer to the backing store.
  // Note: backing_store is at kBackingStoreOffset, but it's an external pointer
  // field, not a tagged field, so we don't mark it as visited.
  generator_->SetInternalReference(real_address, "backing_store",
                                   backing_store_addr,
                                   i::JSArrayBuffer::kBackingStoreOffset);
}

// copy from heap-snapshot-generator.cc ExtractStringReferences
void ObjectTranslator::ExtractStringReferences(i::Address real_address,
                                               i::Tagged<i::String> string,
                                               i::InstanceType instance_type) {
  // Handle different string types - ConsString, SlicedString, ThinString
  // Note: We cannot call methods like cs->first(cage_base_) because for
  // FakeHeapObject, the pointer fields don't point to valid memory. Instead,
  // use TaggedField::load to read the raw tagged value, then get the address.

  if (i::InstanceTypeChecker::IsConsString(instance_type)) {
    // ConsString has first and second parts
    // Use TaggedField::load to read the pointer field without dereferencing
    i::Tagged<i::Object> first_value =
        i::TaggedField<i::Tagged<i::Object>>::load(cage_base_, string,
                                                   i::ConsString::kFirstOffset);
    i::Tagged<i::Object> second_value =
        i::TaggedField<i::Tagged<i::Object>>::load(
            cage_base_, string, i::ConsString::kSecondOffset);

    if (i::IsHeapObject(first_value)) {
      i::Tagged<i::HeapObject> heap_object = Cast<i::HeapObject>(first_value);
      generator_->SetInternalReference(real_address, "first",
                                       heap_object.address(),
                                       i::ConsString::kFirstOffset);
    }
    if (i::IsHeapObject(second_value)) {
      i::Tagged<i::HeapObject> heap_object = Cast<i::HeapObject>(second_value);
      generator_->SetInternalReference(real_address, "second",
                                       heap_object.address(),
                                       i::ConsString::kSecondOffset);
    }
  } else if (i::InstanceTypeChecker::IsSlicedString(instance_type)) {
    // SlicedString has a parent string
    // Use offsetof since SlicedString doesn't have kParentOffset constant
    constexpr int kParentOffset = offsetof(i::SlicedString, parent_);
    i::Tagged<i::Object> parent_value =
        i::TaggedField<i::Tagged<i::Object>>::load(cage_base_, string,
                                                   kParentOffset);

    if (i::IsHeapObject(parent_value)) {
      i::Tagged<i::HeapObject> heap_object = Cast<i::HeapObject>(parent_value);
      generator_->SetInternalReference(real_address, "parent",
                                       heap_object.address(), kParentOffset);
    }
  } else if (i::InstanceTypeChecker::IsThinString(instance_type)) {
    // ThinString has an actual string
    // Use offsetof since ThinString doesn't have kActualOffset constant
    constexpr int kActualOffset = offsetof(i::ThinString, actual_);
    i::Tagged<i::Object> actual_value =
        i::TaggedField<i::Tagged<i::Object>>::load(cage_base_, string,
                                                   kActualOffset);

    if (i::IsHeapObject(actual_value)) {
      i::Tagged<i::HeapObject> heap_object = Cast<i::HeapObject>(actual_value);
      generator_->SetInternalReference(real_address, "actual",
                                       heap_object.address(), kActualOffset);
    }
  }
  // Note: ExternalString doesn't have heap references to add
}

void ObjectTranslator::ExtractJSGeneratorObjectReferences(
    i::Address real_address, i::Tagged<i::JSGeneratorObject> generator) {
  // JSGeneratorObject has the following reference fields:
  // - function (at kFunctionOffset)
  // - context (at kContextOffset)
  // - receiver (at kReceiverOffset)
  // - parameters_and_registers (at kParametersAndRegistersOffset)
  // Use TaggedField::load to read the pointer field without dereferencing
  // since FakeHeapObject doesn't have valid memory at pointer targets.

  // function field
  i::Tagged<i::Object> function_value =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, generator, i::JSGeneratorObject::kFunctionOffset);
  if (i::IsHeapObject(function_value)) {
    i::Tagged<i::HeapObject> heap_object = Cast<i::HeapObject>(function_value);
    generator_->SetInternalReference(real_address, "function",
                                     heap_object.address(),
                                     i::JSGeneratorObject::kFunctionOffset);
  }

  // context field
  i::Tagged<i::Object> context_value =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, generator, i::JSGeneratorObject::kContextOffset);
  if (i::IsHeapObject(context_value)) {
    i::Tagged<i::HeapObject> heap_object = Cast<i::HeapObject>(context_value);
    generator_->SetInternalReference(real_address, "context",
                                     heap_object.address(),
                                     i::JSGeneratorObject::kContextOffset);
  }

  // receiver field
  i::Tagged<i::Object> receiver_value =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, generator, i::JSGeneratorObject::kReceiverOffset);
  if (i::IsHeapObject(receiver_value)) {
    i::Tagged<i::HeapObject> heap_object = Cast<i::HeapObject>(receiver_value);
    generator_->SetInternalReference(real_address, "receiver",
                                     heap_object.address(),
                                     i::JSGeneratorObject::kReceiverOffset);
  }

  // parameters_and_registers field
  i::Tagged<i::Object> params_value =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, generator,
          i::JSGeneratorObject::kParametersAndRegistersOffset);
  if (i::IsHeapObject(params_value)) {
    i::Tagged<i::HeapObject> heap_object = Cast<i::HeapObject>(params_value);
    generator_->SetInternalReference(
        real_address, "parameters_and_registers", heap_object.address(),
        i::JSGeneratorObject::kParametersAndRegistersOffset);
  }
}

void ObjectTranslator::ExtractJSWeakRefReferences(
    i::Address real_address, i::Tagged<i::JSWeakRef> js_weak_ref) {
  // JSWeakRef has a single weak reference field: target.
  // Use TaggedField::load to read the pointer field without dereferencing
  // since FakeHeapObject doesn't have valid memory at pointer targets.
  i::Tagged<i::Object> target_value =
      i::TaggedField<i::Tagged<i::Object>>::load(cage_base_, js_weak_ref,
                                                 i::JSWeakRef::kTargetOffset);
  if (i::IsHeapObject(target_value)) {
    i::Tagged<i::HeapObject> heap_object = Cast<i::HeapObject>(target_value);
    generator_->SetWeakReference(real_address, "target", heap_object.address(),
                                 i::JSWeakRef::kTargetOffset);
  }
}

void ObjectTranslator::ExtractJSDisposableStackReferences(
    i::Address real_address,
    i::Tagged<i::JSDisposableStackBase> disposable_stack) {
  // Extract references from JSDisposableStackBase
  // See V8HeapExplorer::ExtractJSDisposableStackReferences in
  // heap-snapshot-generator.cc The object has three fields: stack, error, and
  // error_message

  // stack field
  i::Tagged<i::Object> stack_value = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, disposable_stack, i::JSDisposableStackBase::kStackOffset);
  if (i::IsHeapObject(stack_value)) {
    i::Tagged<i::HeapObject> heap_object = i::Cast<i::HeapObject>(stack_value);
    generator_->SetInternalReference(real_address, "stack",
                                     heap_object.address(),
                                     i::JSDisposableStackBase::kStackOffset);
  }

  // error field
  i::Tagged<i::Object> error_value = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, disposable_stack, i::JSDisposableStackBase::kErrorOffset);
  if (i::IsHeapObject(error_value)) {
    i::Tagged<i::HeapObject> heap_object = i::Cast<i::HeapObject>(error_value);
    generator_->SetInternalReference(real_address, "error",
                                     heap_object.address(),
                                     i::JSDisposableStackBase::kErrorOffset);
  }

  // error_message field
  i::Tagged<i::Object> error_message_value =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, disposable_stack,
          i::JSDisposableStackBase::kErrorMessageOffset);
  if (i::IsHeapObject(error_message_value)) {
    i::Tagged<i::HeapObject> heap_object =
        i::Cast<i::HeapObject>(error_message_value);
    generator_->SetInternalReference(
        real_address, "error_message", heap_object.address(),
        i::JSDisposableStackBase::kErrorMessageOffset);
  }
}

void ObjectTranslator::ExtractWasmInstanceObjectReferences(
    i::Address real_address, i::Tagged<i::WasmInstanceObject> instance_object) {
  // Extract references from WasmInstanceObject
  // See V8HeapExplorer::ExtractWasmInstanceObjectReferences in
  // heap-snapshot-generator.cc
  // The object has fields: trusted_data, module_object, exports_object

  // NIY: trusted_data field - this is a trusted pointer, use special handling
  // WasmInstanceObject::BodyDescriptor uses IterateTrustedPointer for this
  // field We cannot directly dereference, so skip for now

  // module_object field
  i::Tagged<i::Object> module_object_value =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, instance_object,
          i::WasmInstanceObject::kModuleObjectOffset);
  if (i::IsHeapObject(module_object_value)) {
    i::Tagged<i::HeapObject> heap_object =
        i::Cast<i::HeapObject>(module_object_value);
    generator_->SetInternalReference(
        real_address, "module_object", heap_object.address(),
        i::WasmInstanceObject::kModuleObjectOffset);
  }

  // exports_object field
  i::Tagged<i::Object> exports_object_value =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, instance_object,
          i::WasmInstanceObject::kExportsObjectOffset);
  if (i::IsHeapObject(exports_object_value)) {
    i::Tagged<i::HeapObject> heap_object =
        i::Cast<i::HeapObject>(exports_object_value);
    generator_->SetInternalReference(
        real_address, "exports", heap_object.address(),
        i::WasmInstanceObject::kExportsObjectOffset);
  }
}

void ObjectTranslator::ExtractWasmModuleObjectReferences(
    i::Address real_address, i::Tagged<i::WasmModuleObject> module_object) {
  // Extract references from WasmModuleObject
  // See V8HeapExplorer::ExtractWasmModuleObjectReferences in
  // heap-snapshot-generator.cc
  // The object has fields: managed_native_module, script

  // managed_native_module field
  i::Tagged<i::Object> managed_native_module_value =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, module_object,
          i::WasmModuleObject::kManagedNativeModuleOffset);
  if (i::IsHeapObject(managed_native_module_value)) {
    i::Tagged<i::HeapObject> heap_object =
        i::Cast<i::HeapObject>(managed_native_module_value);
    generator_->SetInternalReference(
        real_address, "managed_native_module", heap_object.address(),
        i::WasmModuleObject::kManagedNativeModuleOffset);
  }

  // script field
  i::Tagged<i::Object> script_value =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, module_object, i::WasmModuleObject::kScriptOffset);
  if (i::IsHeapObject(script_value)) {
    i::Tagged<i::HeapObject> heap_object = i::Cast<i::HeapObject>(script_value);
    generator_->SetInternalReference(real_address, "script",
                                     heap_object.address(),
                                     i::WasmModuleObject::kScriptOffset);
  }
}

void ObjectTranslator::ExtractMapReferences(i::Address real_address,
                                            i::Tagged<i::Map> map) {
  // Extract references from Map
  // See V8HeapExplorer::ExtractMapReferences in heap-snapshot-generator.cc

  // transitions or prototype_info field
  i::Tagged<i::MaybeObject> maybe_transitions_or_prototype_info =
      i::TaggedField<i::Tagged<i::MaybeObject>>::load(
          cage_base_, map, i::Map::kTransitionsOrPrototypeInfoOffset);
  i::Tagged<i::HeapObject> transitions_or_prototype_info;
  if (maybe_transitions_or_prototype_info.GetHeapObjectIfWeak(
          &transitions_or_prototype_info)) {
    generator_->SetWeakReference(real_address, "transition",
                                 transitions_or_prototype_info.address(),
                                 i::Map::kTransitionsOrPrototypeInfoOffset);
  } else if (maybe_transitions_or_prototype_info.GetHeapObjectIfStrong(
                 &transitions_or_prototype_info)) {
    // Use FakeHeapObject to check the type of transitions_or_prototype_info
    i::InstanceType type =
        GetInstanceType(transitions_or_prototype_info.address());

    if (i::InstanceTypeChecker::IsTransitionArray(type)) {
      generator_->SetInternalReference(
          real_address, "transitions", transitions_or_prototype_info.address(),
          i::Map::kTransitionsOrPrototypeInfoOffset);
    } else if (i::InstanceTypeChecker::IsFixedArray(type)) {
      generator_->SetInternalReference(
          real_address, "transition", transitions_or_prototype_info.address(),
          i::Map::kTransitionsOrPrototypeInfoOffset);
    } else if (map->is_prototype_map()) {
      generator_->SetInternalReference(
          real_address, "prototype_info",
          transitions_or_prototype_info.address(),
          i::Map::kTransitionsOrPrototypeInfoOffset);
    }
  }

  // descriptors field (for JSObject maps)
  if (i::InstanceTypeChecker::IsJSObject(map->instance_type())) {
    i::Tagged<i::Object> descriptors_value =
        i::TaggedField<i::Tagged<i::Object>>::load(
            cage_base_, map, i::Map::kInstanceDescriptorsOffset);
    if (i::IsHeapObject(descriptors_value)) {
      i::Tagged<i::HeapObject> heap_object =
          i::Cast<i::HeapObject>(descriptors_value);
      generator_->SetInternalReference(real_address, "descriptors",
                                       heap_object.address(),
                                       i::Map::kInstanceDescriptorsOffset);
    }
  }

  // prototype field
  i::Tagged<i::Object> prototype_value =
      i::TaggedField<i::Tagged<i::Object>>::load(cage_base_, map,
                                                 i::Map::kPrototypeOffset);
  if (i::IsHeapObject(prototype_value)) {
    i::Tagged<i::HeapObject> heap_object =
        i::Cast<i::HeapObject>(prototype_value);
    generator_->SetInternalReference(real_address, "prototype",
                                     heap_object.address(),
                                     i::Map::kPrototypeOffset);
  }

  // constructor_or_back_pointer_or_native_context field
  i::Tagged<i::Object> value = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, map, i::Map::kConstructorOrBackPointerOrNativeContextOffset);
  if (i::IsHeapObject(value)) {
    i::Tagged<i::HeapObject> heap_object = i::Cast<i::HeapObject>(value);
    // if (IsContextMap(map) || IsMapMap(map))
    if (i::InstanceTypeChecker::IsContext(map->instance_type()) ||
        i::InstanceTypeChecker::IsMap(map->instance_type())) {
      generator_->SetInternalReference(
          real_address, "native_context", heap_object.address(),
          i::Map::kConstructorOrBackPointerOrNativeContextOffset);
    } else {
      // heap_object is map->constructor_or_back_pointer() as well, get map and
      // instance type
      auto it = objects_head_.find(heap_object.address());
      CHECK(it != objects_head_.end());
      FakeHeapObject fake_constructor_or_back_pointer(
          reader_, it->second.offset_, it->second.size_);
      i::Tagged<i::HeapObject> fake_obj =
          fake_constructor_or_back_pointer.fake_object();
      FakeHeapObject fake_obj_map(
          reader_, objects_head_[fake_obj->map(cage_base_).address()]);
      i::Tagged<i::Map> constructor_or_back_pointer_map =
          i::UncheckedCast<i::Map>(fake_obj_map.fake_object());
      auto constructor_or_back_pointer_type =
          constructor_or_back_pointer_map->instance_type();

      if (i::InstanceTypeChecker::IsMap(constructor_or_back_pointer_type)) {
        generator_->SetInternalReference(
            real_address, "back_pointer", heap_object.address(),
            i::Map::kConstructorOrBackPointerOrNativeContextOffset);
      } else if (i::InstanceTypeChecker::IsFunctionTemplateInfo(
                     constructor_or_back_pointer_type)) {
        generator_->SetInternalReference(
            real_address, "constructor_function_data", heap_object.address(),
            i::Map::kConstructorOrBackPointerOrNativeContextOffset);
      } else {
        generator_->SetInternalReference(
            real_address, "constructor", heap_object.address(),
            i::Map::kConstructorOrBackPointerOrNativeContextOffset);
      }
    }
  }

  // dependent_code field
  i::Tagged<i::Object> dependent_code_value =
      i::TaggedField<i::Tagged<i::Object>>::load(cage_base_, map,
                                                 i::Map::kDependentCodeOffset);
  if (i::IsHeapObject(dependent_code_value)) {
    i::Tagged<i::HeapObject> heap_object =
        i::Cast<i::HeapObject>(dependent_code_value);
    generator_->SetInternalReference(real_address, "dependent_code",
                                     heap_object.address(),
                                     i::Map::kDependentCodeOffset);
  }
}

// Keep this synced with NATIVE_CONTEXT_FIELDS in contexts.h.
static const struct {
  int index;
  const char* name;
} native_context_names[] = {
#define CONTEXT_FIELD_INDEX_NAME(index, _, name) {i::Context::index, #name},
    NATIVE_CONTEXT_FIELDS(CONTEXT_FIELD_INDEX_NAME)
#undef CONTEXT_FIELD_INDEX_NAME
};

void ObjectTranslator::ExtractContextReferences(i::Address real_address,
                                                i::Tagged<i::Context> context,
                                                i::InstanceType context_type) {
  const bool is_native_context =
      context_type == i::InstanceType::NATIVE_CONTEXT_TYPE;
  auto set_internal_context_slot = [&](const char* edge_name, int index) {
    int field_offset = i::Context::OffsetOfElementAt(index);
    if (field_offset < 0) {
      return;
    }
    if (static_cast<size_t>(field_offset / i::kTaggedSize) >=
        generator_->max_pointers_) {
      return;
    }
    i::Tagged<i::Object> value = i::TaggedField<i::Object>::Relaxed_Load(
        cage_base_, context, field_offset);
    if (!i::IsHeapObject(value)) {
      return;
    }
    i::Address child = i::Cast<i::HeapObject>(value).address();
    if (!generator_->GetNode(child)) {
      return;
    }
    generator_->SetInternalReference(real_address, edge_name, child,
                                     field_offset);
  };

  i::Tagged<i::Object> scope_info_value =
      i::TaggedField<i::Object>::Relaxed_Load(
          cage_base_, context,
          i::Context::OffsetOfElementAt(i::Context::SCOPE_INFO_INDEX));
  std::unique_ptr<FakeHeapObject> fake_scope_info_ho;
  i::Tagged<i::ScopeInfo> fake_scope_info;
  // Translator runs on dumped bytes. To inspect ScopeInfo internals safely, we
  // must dereference through objects_head_ and rebuild a child FakeHeapObject.
  if (i::IsHeapObject(scope_info_value)) {
    i::Address scope_info_addr =
        i::Cast<i::HeapObject>(scope_info_value).address();
    auto it = objects_head_.find(scope_info_addr);
    if (it != objects_head_.end()) {
      fake_scope_info_ho =
          std::make_unique<FakeHeapObject>(reader_, it->second);
      fake_scope_info =
          i::UncheckedCast<i::ScopeInfo>(fake_scope_info_ho->fake_object());
    }
  }

  // copy from Context::is_declaration_context
  auto is_declaration_context = [&]() {
    if (is_native_context) {
      return true;
    }
    // Do not call Context::is_declaration_context() here: that helper may walk
    // map/heap state not available in translate mode.
    switch (context_type) {
      case i::InstanceType::FUNCTION_CONTEXT_TYPE:
      case i::InstanceType::SCRIPT_CONTEXT_TYPE:
      case i::InstanceType::MODULE_CONTEXT_TYPE:
        return true;
      case i::InstanceType::EVAL_CONTEXT_TYPE:
        return fake_scope_info_ho &&
               fake_scope_info->language_mode() == i::LanguageMode::kStrict;
      case i::InstanceType::BLOCK_CONTEXT_TYPE:
        return fake_scope_info_ho && fake_scope_info->is_declaration_scope();
      default:
        return false;
    }
  };

  if (!is_native_context && fake_scope_info_ho && is_declaration_context()) {
    // NIY:  if (!IsNativeContext(context) &&
    // context->is_declaration_context()) {
  }

  set_internal_context_slot("scope_info", i::Context::SCOPE_INFO_INDEX);
  set_internal_context_slot("previous", i::Context::PREVIOUS_INDEX);

  if (is_native_context) {
    set_internal_context_slot("global_object", i::Context::EXTENSION_INDEX);
    for (const auto& field : native_context_names) {
      set_internal_context_slot(field.name, field.index);
    }
  } else {  // NIY: context->has_extension()
  }
}

// ExtractSymbolReferences with real_address parameter
void ObjectTranslator::ExtractSymbolReferences(i::Address real_address,
                                               i::Tagged<i::Symbol> symbol) {
  // Get description field
  i::Tagged<i::Object> description = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, symbol, offsetof(i::Symbol, description_));
  if (i::IsHeapObject(description)) {
    i::Tagged<i::HeapObject> desc_heap = i::Cast<i::HeapObject>(description);
    generator_->SetInternalReference(real_address, "name", desc_heap.address(),
                                     offsetof(i::Symbol, description_));
  }
}

// ExtractSharedFunctionInfoReferences
void ObjectTranslator::ExtractSharedFunctionInfoReferences(
    i::Address real_address, i::Tagged<i::SharedFunctionInfo> shared,
    i::Tagged<i::Map> map) {
  // NIY: TagObject(shared, "(shared function info)");

  // name_or_scope_info field
  i::Tagged<i::Object> name_or_scope_info =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, shared, i::SharedFunctionInfo::kNameOrScopeInfoOffset);
  if (i::IsHeapObject(name_or_scope_info)) {
    i::Tagged<i::HeapObject> heap_object =
        i::Cast<i::HeapObject>(name_or_scope_info);
    // Check if it's ScopeInfo
    // NIY: TagObject(name_or_scope_info, "(function scope info)");

    generator_->SetInternalReference(
        real_address, "name_or_scope_info", heap_object.address(),
        i::SharedFunctionInfo::kNameOrScopeInfoOffset);
  }

  // script field
  i::Tagged<i::Object> script = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, shared, i::SharedFunctionInfo::kScriptOffset);
  if (i::IsHeapObject(script)) {
    i::Tagged<i::HeapObject> heap_object = i::Cast<i::HeapObject>(script);
    generator_->SetInternalReference(real_address, "script",
                                     heap_object.address(),
                                     i::SharedFunctionInfo::kScriptOffset);
  }

  // trusted_function_data field
  i::Tagged<i::Object> trusted_function_data =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, shared,
          i::SharedFunctionInfo::kTrustedFunctionDataOffset);
  if (i::IsHeapObject(trusted_function_data)) {
    i::Tagged<i::HeapObject> heap_object =
        i::Cast<i::HeapObject>(trusted_function_data);
    generator_->SetInternalReference(
        real_address, "trusted_function_data", heap_object.address(),
        i::SharedFunctionInfo::kTrustedFunctionDataOffset);
  }

  // untrusted_function_data field
  i::Tagged<i::Object> untrusted_function_data =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, shared,
          i::SharedFunctionInfo::kUntrustedFunctionDataOffset);
  if (i::IsHeapObject(untrusted_function_data)) {
    i::Tagged<i::HeapObject> heap_object =
        i::Cast<i::HeapObject>(untrusted_function_data);
    generator_->SetInternalReference(
        real_address, "untrusted_function_data", heap_object.address(),
        i::SharedFunctionInfo::kUntrustedFunctionDataOffset);
  }

  // raw_outer_scope_info_or_feedback_metadata field
  i::Tagged<i::Object> outer_scope_info_or_feedback_metadata =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, shared,
          i::SharedFunctionInfo::kOuterScopeInfoOrFeedbackMetadataOffset);
  if (i::IsHeapObject(outer_scope_info_or_feedback_metadata)) {
    i::Tagged<i::HeapObject> heap_object =
        i::Cast<i::HeapObject>(outer_scope_info_or_feedback_metadata);
    generator_->SetInternalReference(
        real_address, "raw_outer_scope_info_or_feedback_metadata",
        heap_object.address(),
        i::SharedFunctionInfo::kOuterScopeInfoOrFeedbackMetadataOffset);
  }
}

void ObjectTranslator::ExtractCodeReferences(i::Address real_address,
                                             i::Tagged<i::Code> code) {
  auto set_internal_if_heap_object = [&](const char* name, int offset) {
    i::Tagged<i::Object> value =
        i::TaggedField<i::Tagged<i::Object>>::load(cage_base_, code, offset);
    if (!i::IsHeapObject(value)) {
      return;
    }
    i::Address child = i::Cast<i::HeapObject>(value).address();
    if (!generator_->GetNode(child)) {
      return;
    }
    generator_->SetInternalReference(real_address, name, child, offset);
  };

  if (!code->has_instruction_stream()) {
    return;
  }

  i::Tagged<i::Object> instruction_stream_value =
      i::TaggedField<i::Tagged<i::Object>>::load(
          code_cage_base_, code, i::Code::kInstructionStreamOffset);
  if (i::IsHeapObject(instruction_stream_value)) {
    i::Address child =
        i::Cast<i::HeapObject>(instruction_stream_value).address();
    if (generator_->GetNode(child)) {
      generator_->SetInternalReference(real_address, "instruction_stream",
                                       child,
                                       i::Code::kInstructionStreamOffset);
    }
  }

  if (code->kind() == i::CodeKind::BASELINE) {
    set_internal_if_heap_object(
        "interpreter_data",
        i::Code::kDeoptimizationDataOrInterpreterDataOffset);
    set_internal_if_heap_object("bytecode_offset_table",
                                i::Code::kPositionTableOffset);
  } else if (code->uses_deoptimization_data()) {
    set_internal_if_heap_object(
        "deoptimization_data",
        i::Code::kDeoptimizationDataOrInterpreterDataOffset);
    set_internal_if_heap_object("source_position_table",
                                i::Code::kPositionTableOffset);
  }
}

// ExtractInstructionStreamReferences with real_address parameter
void ObjectTranslator::ExtractInstructionStreamReferences(
    i::Address real_address, i::Tagged<i::InstructionStream> istream) {
  // InstructionStream dump is custom (see
  // dump-objects.cc::WriteInstructionStreamInfo): [map][code protected
  // pointer][relocation_info protected pointer][reloc entries...]
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
    generator_->SetInternalReference(real_address, "code", code_addr,
                                     i::InstructionStream::kCodeOffset);
  }

  offset += sizeof(i::Address);
  i::Address relocation_info_addr{0};
  if (reader_->ReadDataAt(offset, sizeof(relocation_info_addr),
                          reinterpret_cast<uint8_t*>(&relocation_info_addr),
                          sizeof(relocation_info_addr)) &&
      generator_->GetNode(relocation_info_addr)) {
    generator_->SetInternalReference(
        real_address, "relocation_info", relocation_info_addr,
        i::InstructionStream::kRelocationInfoOffset);
  }
}

// ExtractCellReferences with real_address parameter
void ObjectTranslator::ExtractCellReferences(i::Address real_address,
                                             i::Tagged<i::Cell> cell) {
  // Get maybe_value field
  i::Tagged<i::MaybeObject> maybe_value =
      i::TaggedField<i::Tagged<i::Object>>::load(cage_base_, cell,
                                                 i::Cell::kMaybeValueOffset);

  i::Tagged<i::HeapObject> heap_object;
  if (maybe_value.GetHeapObjectIfStrong(&heap_object)) {
    generator_->SetInternalReference(real_address, "value",
                                     heap_object.address(),
                                     i::Cell::kMaybeValueOffset);
  } else if (maybe_value.GetHeapObjectIfWeak(&heap_object)) {
    generator_->SetWeakReference(real_address, "value", heap_object.address(),
                                 i::Cell::kMaybeValueOffset);
  }
}

// ExtractFeedbackCellReferences with real_address parameter
void ObjectTranslator::ExtractFeedbackCellReferences(
    i::Address real_address, i::Tagged<i::FeedbackCell> feedback_cell) {
  // Get value field
  i::Tagged<i::Object> value = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, feedback_cell, i::FeedbackCell::kValueOffset);
  if (i::IsHeapObject(value)) {
    i::Tagged<i::HeapObject> value_heap = i::Cast<i::HeapObject>(value);
    generator_->SetInternalReference(real_address, "value",
                                     value_heap.address(),
                                     i::FeedbackCell::kValueOffset);
  }
}

// ExtractPropertyCellReferences with real_address parameter
void ObjectTranslator::ExtractPropertyCellReferences(
    i::Address real_address, i::Tagged<i::PropertyCell> cell) {
  // Get value field
  i::Tagged<i::Object> value = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, cell, i::PropertyCell::kValueOffset);
  if (i::IsHeapObject(value)) {
    i::Tagged<i::HeapObject> value_heap = i::Cast<i::HeapObject>(value);
    generator_->SetInternalReference(real_address, "value",
                                     value_heap.address(),
                                     i::PropertyCell::kValueOffset);
  }

  // Get dependent_code
  i::Tagged<i::Object> dependent_code =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, cell, i::PropertyCell::kDependentCodeOffset);
  if (i::IsHeapObject(dependent_code)) {
    i::Tagged<i::HeapObject> dep_heap = i::Cast<i::HeapObject>(dependent_code);
    generator_->SetInternalReference(real_address, "dependent_code",
                                     dep_heap.address(),
                                     i::PropertyCell::kDependentCodeOffset);
  }
}

void ObjectTranslator::ExtractWeakCellReferences(
    i::Address real_address, i::Tagged<i::WeakCell> weak_cell) {
  auto set_weak_if_heap_object = [&](const char* edge_name,
                                     i::Tagged<i::Object> value, int offset) {
    if (!i::IsHeapObject(value)) {
      return;
    }
    i::Address child = i::Cast<i::HeapObject>(value).address();
    if (!generator_->GetNode(child)) {
      return;
    }
    generator_->SetWeakReference(real_address, edge_name, child, offset);
  };

  set_weak_if_heap_object(
      "target",
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, weak_cell, offsetof(i::WeakCell, target_)),
      offsetof(i::WeakCell, target_));
  set_weak_if_heap_object(
      "unregister_token",
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, weak_cell, offsetof(i::WeakCell, unregister_token_)),
      offsetof(i::WeakCell, unregister_token_));
}

// ExtractPrototypeInfoReferences with real_address parameter
void ObjectTranslator::ExtractPrototypeInfoReferences(
    i::Address real_address, i::Tagged<i::PrototypeInfo> info) {
  // Keep alignment with heap-snapshot-generator.cc:
  // this path is tag-only there, while actual field edges are collected by the
  // generic post-extract visitor pass.
  (void)real_address;
  (void)info;
}

// ExtractAllocationSiteReferences with real_address parameter
void ObjectTranslator::ExtractAllocationSiteReferences(
    i::Address real_address, i::Tagged<i::AllocationSite> site) {
  // Get transition_info_or_boilerplate
  i::Tagged<i::Object> transition_info =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, site,
          offsetof(i::AllocationSite, transition_info_or_boilerplate_));
  if (i::IsHeapObject(transition_info)) {
    i::Tagged<i::HeapObject> trans_heap =
        i::Cast<i::HeapObject>(transition_info);
    generator_->SetInternalReference(
        real_address, "transition_info", trans_heap.address(),
        offsetof(i::AllocationSite, transition_info_or_boilerplate_));
  }

  // Get nested_site
  i::Tagged<i::Object> nested_site = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, site, offsetof(i::AllocationSite, nested_site_));
  if (i::IsHeapObject(nested_site)) {
    i::Tagged<i::HeapObject> nested_heap = i::Cast<i::HeapObject>(nested_site);
    generator_->SetInternalReference(real_address, "nested_site",
                                     nested_heap.address(),
                                     offsetof(i::AllocationSite, nested_site_));
  }

  // NIY:  TagObject(site->dependent_code(), "(dependent code)",
  // HeapEntry::kCode);

  // Get dependent_code
  i::Tagged<i::Object> dependent_code =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, site, offsetof(i::AllocationSite, dependent_code_));

  if (i::IsHeapObject(dependent_code)) {
    i::Tagged<i::HeapObject> dep_heap = i::Cast<i::HeapObject>(dependent_code);
    generator_->SetInternalReference(
        real_address, "dependent_code", dep_heap.address(),
        offsetof(i::AllocationSite, dependent_code_));
  }
}

// ExtractArrayBoilerplateDescriptionReferences with real_address parameter
void ObjectTranslator::ExtractArrayBoilerplateDescriptionReferences(
    i::Address real_address, i::Tagged<i::ArrayBoilerplateDescription> value) {
  // Get constant_elements
  i::Tagged<i::FixedArrayBase> constant_elements =
      i::TaggedField<i::Tagged<i::FixedArrayBase>>::load(
          cage_base_, value,
          offsetof(i::ArrayBoilerplateDescription, constant_elements_));
  if (i::IsHeapObject(constant_elements)) {
    i::Tagged<i::HeapObject> constant_elements_heap =
        i::Cast<i::HeapObject>(constant_elements);
    generator_->SetInternalReference(
        real_address, "constant_elements", constant_elements_heap.address(),
        offsetof(i::ArrayBoilerplateDescription, constant_elements_));
    // NIY: TagObject(constant_elements, "(constant elements)",
    // HeapEntry::kCode);
  }
}

// ExtractFeedbackVectorReferences with real_address parameter
void ObjectTranslator::ExtractFeedbackVectorReferences(
    i::Address real_address, i::Tagged<i::FeedbackVector> feedback_vector) {}

// ExtractDescriptorArrayReferences with real_address parameter
void ObjectTranslator::ExtractDescriptorArrayReferences(
    i::Address real_address, i::Tagged<i::DescriptorArray> array) {
  // Get enum_cache
  i::Tagged<i::Object> enum_cache = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, array, i::DescriptorArray::kEnumCacheOffset);
  if (i::IsHeapObject(enum_cache)) {
    i::Tagged<i::HeapObject> cache_heap = i::Cast<i::HeapObject>(enum_cache);
    generator_->SetInternalReference(real_address, "enum_cache",
                                     cache_heap.address(),
                                     i::DescriptorArray::kEnumCacheOffset);
  }

  // Iterate through descriptors
  i::MaybeObjectSlot start = i::MaybeObjectSlot(array->GetDescriptorSlot(0));
  i::MaybeObjectSlot end = i::MaybeObjectSlot(
      array->GetDescriptorSlot(array->number_of_all_descriptors()));
  for (int i = 0; start + i < end; ++i) {
    i::MaybeObjectSlot slot = start + i;
    int offset = static_cast<int>(slot.address() - array.address());
    i::Tagged<i::MaybeObject> object = *slot;
    i::Tagged<i::HeapObject> heap_object;

    if (object.GetHeapObjectIfWeak(&heap_object)) {
      generator_->SetWeakReference(real_address, i, heap_object.address(),
                                   offset);
    } else if (object.GetHeapObjectIfStrong(&heap_object)) {
      generator_->SetInternalReference(real_address, std::to_string(i),
                                       heap_object.address(), offset);
    }
  }
}

// ExtractEnumCacheReferences with real_address parameter
void ObjectTranslator::ExtractEnumCacheReferences(
    i::Address real_address, i::Tagged<i::EnumCache> cache) {
  // Keep alignment with heap-snapshot-generator.cc:
  // this path is tag-only there, while generic visitor traversal collects
  // object fields.
  (void)real_address;
  (void)cache;
}

// ExtractTransitionArrayReferences with real_address parameter
void ObjectTranslator::ExtractTransitionArrayReferences(
    i::Address real_address, i::Tagged<i::TransitionArray> transitions) {
  // Keep alignment with heap-snapshot-generator.cc:
  // this path is tag-only there, while generic visitor traversal collects
  // object fields.
  (void)real_address;
  (void)transitions;
}

// ExtractWeakArrayReferences with real_address parameter (template)
template <typename T>
void ObjectTranslator::ExtractWeakArrayReferences(int header_size,
                                                  i::Address real_address,
                                                  i::Tagged<T> array) {
  int length = array->length();
  for (int i = 0; i < length; ++i) {
    int offset = header_size + i * i::kTaggedSize;
    // i::Tagged<i::MaybeObject> object = array->get(i);
    i::Tagged<i::MaybeObject> object =
        i::TaggedField<i::Tagged<i::MaybeObject>>::Relaxed_Load(cage_base_,
                                                                array, offset);
    i::Tagged<i::HeapObject> heap_object;

    if (object.GetHeapObjectIfWeak(&heap_object)) {
      generator_->SetWeakReference(real_address, i, heap_object.address(),
                                   offset);
    } else if (object.GetHeapObjectIfStrong(&heap_object)) {
      generator_->SetInternalReference(real_address, std::to_string(i),
                                       heap_object.address(), offset);
    }
  }
}

void ObjectTranslator::ExtractEphemeronHashTableReferences(
    i::Address real_address, i::Tagged<i::EphemeronHashTable> table) {
  // NIY
}

// ExtractFixedArrayReferences with real_address parameter
void ObjectTranslator::ExtractFixedArrayReferences(
    i::Address real_address, i::Tagged<i::FixedArray> array) {
  // NIY:
}

// ExtractScriptReferences with real_address parameter
void ObjectTranslator::ExtractScriptReferences(i::Address real_address,
                                               i::Tagged<i::Script> script,
                                               i::Tagged<i::Map> map) {
  // Reference: heap-snapshot-generator.cc ExtractScriptReferences

  // source field (String) - note: string data may not exist, only map field
  // NIY: TagObject(script->source(), "(script source)");
  i::Tagged<i::Object> source = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, script, i::Script::kSourceOffset);
  if (i::IsHeapObject(source)) {
    i::Tagged<i::HeapObject> heap_object = i::Cast<i::HeapObject>(source);
    generator_->SetInternalReference(real_address, "source",
                                     heap_object.address(),
                                     i::Script::kSourceOffset);
  }

  // name field (Object - typically String or undefined)
  // NIY: TagObject(script->name(), "(script name)");
  i::Tagged<i::Object> name = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, script, i::Script::kNameOffset);
  if (i::IsHeapObject(name)) {
    i::Tagged<i::HeapObject> heap_object = i::Cast<i::HeapObject>(name);
    generator_->SetInternalReference(
        real_address, "name", heap_object.address(), i::Script::kNameOffset);
  }

  // context_data field
  i::Tagged<i::Object> context_data =
      i::TaggedField<i::Tagged<i::Object>>::load(cage_base_, script,
                                                 i::Script::kContextDataOffset);
  if (i::IsHeapObject(context_data)) {
    i::Tagged<i::HeapObject> heap_object = i::Cast<i::HeapObject>(context_data);
    generator_->SetInternalReference(real_address, "context_data",
                                     heap_object.address(),
                                     i::Script::kContextDataOffset);
  }

  // line_ends field (FixedArray)
  // NIY: TagObject(script->line_ends(), "(script line ends)",
  // HeapEntry::kCode);
  i::Tagged<i::Object> line_ends = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, script, i::Script::kLineEndsOffset);
  if (i::IsHeapObject(line_ends)) {
    i::Tagged<i::HeapObject> heap_object = i::Cast<i::HeapObject>(line_ends);
    generator_->SetInternalReference(real_address, "line_ends",
                                     heap_object.address(),
                                     i::Script::kLineEndsOffset);
  }

  // infos field (WeakFixedArray)
  // NIY: TagObject(script->infos(), "(infos)", HeapEntry::kCode);

  // host_defined_options field (FixedArray)
  // NIY: TagObject(script->host_defined_options(), "(host-defined options)",
  // HeapEntry::kCode);

#if V8_ENABLE_WEBASSEMBLY
  // Wasm reuses some otherwise unused fields for wasm-specific information.
  if (script->type() == i::Script::Type::kWasm) {
    // wasm_breakpoint_infos
    i::Tagged<i::Object> wasm_breakpoint_infos =
        i::TaggedField<i::Tagged<i::Object>>::load(
            cage_base_, script,
            i::Script::kEvalFromSharedOrWrappedArgumentsOffset);
    if (i::IsHeapObject(wasm_breakpoint_infos)) {
      i::Tagged<i::HeapObject> heap_object =
          i::Cast<i::HeapObject>(wasm_breakpoint_infos);
      generator_->SetInternalReference(
          real_address, "wasm_breakpoint_infos", heap_object.address(),
          i::Script::kEvalFromSharedOrWrappedArgumentsOffset);
    }

    // wasm_managed_native_module
    i::Tagged<i::Object> wasm_managed_native_module =
        i::TaggedField<i::Tagged<i::Object>>::load(
            cage_base_, script, i::Script::kEvalFromPositionOffset);
    if (i::IsHeapObject(wasm_managed_native_module)) {
      i::Tagged<i::HeapObject> heap_object =
          i::Cast<i::HeapObject>(wasm_managed_native_module);
      generator_->SetInternalReference(
          real_address, "wasm_managed_native_module", heap_object.address(),
          i::Script::kEvalFromPositionOffset);
    }

    // wasm_weak_instance_list
    i::Tagged<i::Object> wasm_weak_instance_list =
        i::TaggedField<i::Tagged<i::Object>>::load(cage_base_, script,
                                                   i::Script::kInfosOffset);
    if (i::IsHeapObject(wasm_weak_instance_list)) {
      i::Tagged<i::HeapObject> heap_object =
          i::Cast<i::HeapObject>(wasm_weak_instance_list);
      generator_->SetInternalReference(real_address, "wasm_weak_instance_list",
                                       heap_object.address(),
                                       i::Script::kInfosOffset);
    }
  }
#endif
}

// ExtractAccessorInfoReferences
// Reference: heap-snapshot-generator.cc ExtractAccessorInfoReferences
void ObjectTranslator::ExtractAccessorInfoReferences(
    i::Address real_address, i::Tagged<i::AccessorInfo> accessor_info) {
  // name field
  i::Tagged<i::Object> name = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, accessor_info, i::AccessorInfo::kNameOffset);
  if (i::IsHeapObject(name)) {
    i::Tagged<i::HeapObject> heap_object = i::Cast<i::HeapObject>(name);
    generator_->SetInternalReference(real_address, "name",
                                     heap_object.address(),
                                     i::AccessorInfo::kNameOffset);
  }
  // NIY: TagObject(accessor_info->name(), "(accessor name)");

  // data field
  i::Tagged<i::Object> data = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, accessor_info, i::AccessorInfo::kDataOffset);
  if (i::IsHeapObject(data)) {
    i::Tagged<i::HeapObject> heap_object = i::Cast<i::HeapObject>(data);
    generator_->SetInternalReference(real_address, "data",
                                     heap_object.address(),
                                     i::AccessorInfo::kDataOffset);
  }
  // NIY: TagObject(accessor_info->data(), "(accessor data)");
}

// ExtractAccessorPairReferences
// Reference: heap-snapshot-generator.cc ExtractAccessorPairReferences
void ObjectTranslator::ExtractAccessorPairReferences(
    i::Address real_address, i::Tagged<i::AccessorPair> accessors) {
  // getter field
  i::Tagged<i::Object> getter = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, accessors, offsetof(i::AccessorPair, getter_));
  if (i::IsHeapObject(getter)) {
    i::Tagged<i::HeapObject> heap_object = i::Cast<i::HeapObject>(getter);
    generator_->SetInternalReference(real_address, "getter",
                                     heap_object.address(),
                                     offsetof(i::AccessorPair, getter_));
  }

  // setter field
  i::Tagged<i::Object> setter = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, accessors, offsetof(i::AccessorPair, setter_));
  if (i::IsHeapObject(setter)) {
    i::Tagged<i::HeapObject> heap_object = i::Cast<i::HeapObject>(setter);
    generator_->SetInternalReference(real_address, "setter",
                                     heap_object.address(),
                                     offsetof(i::AccessorPair, setter_));
  }
}

// ExtractBytecodeArrayReferences with real_address parameter
void ObjectTranslator::ExtractBytecodeArrayReferences(
    i::Address real_address, i::Tagged<i::BytecodeArray> bytecode) {
  // Keep alignment with heap-snapshot-generator.cc:
  // this path is tag-only there, while generic visitor traversal collects
  // object fields.
  (void)real_address;
  (void)bytecode;
}

// ExtractScopeInfoReferences with real_address parameter
void ObjectTranslator::ExtractScopeInfoReferences(
    i::Address real_address, i::Tagged<i::ScopeInfo> info) {
  // Keep alignment with heap-snapshot-generator.cc:
  // this path is tag-only there, while generic visitor traversal collects
  // object fields.
  (void)real_address;
  (void)info;
}

// ExtractCppHeapExternalReferences with real_address parameter
void ObjectTranslator::ExtractCppHeapExternalReferences(
    i::Address real_address, i::Tagged<i::CppHeapExternalObject> obj) {
  // This is for tracking external C++ heap objects
  // We don't add references here, just mark the object as external
}

#if V8_ENABLE_WEBASSEMBLY
void ObjectTranslator::ExtractWasmStructReferences(
    i::Address real_address, i::Tagged<i::WasmStruct> obj) {
  // WasmStruct field decoding needs canonical type metadata and full wasm
  // support; keep the current behavior (node exists, no detailed edges).
  (void)real_address;
  (void)obj;
}

void ObjectTranslator::ExtractWasmArrayReferences(i::Address real_address,
                                                  i::Tagged<i::WasmArray> obj,
                                                  i::Tagged<i::Map> map) {
  if (!IsReferenceForWasmArray(map)) return;
  // NIY:
}

void ObjectTranslator::ExtractWasmTrustedInstanceDataReferences(
    i::Address real_address, i::Tagged<i::HeapObject> trusted_data) {
  auto set_internal_if_heap_object = [&](const char* edge_name,
                                         i::Tagged<i::Object> value,
                                         int offset) {
    if (!i::IsHeapObject(value)) {
      return;
    }
    i::Address child = i::Cast<i::HeapObject>(value).address();
    if (!generator_->GetNode(child)) {
      return;
    }
    generator_->SetInternalReference(real_address, edge_name, child, offset);
  };

  for (size_t i = 0; i < i::WasmTrustedInstanceData::kTaggedFieldOffsets.size();
       ++i) {
    const uint16_t offset = i::WasmTrustedInstanceData::kTaggedFieldOffsets[i];
    i::Tagged<i::Object> value =
        i::TaggedField<i::Object>::load(cage_base_, trusted_data, offset);
    set_internal_if_heap_object(
        i::WasmTrustedInstanceData::kTaggedFieldNames[i], value, offset);
  }
  // NIY  for (size_t i = 0; i <
  // WasmTrustedInstanceData::kProtectedFieldNames.size();
}
#endif

/////////////// helper
i::InstanceType ObjectTranslator::GetInstanceType(i::Address real_address) {
  auto it = objects_head_.find(real_address);
  CHECK(it != objects_head_.end());
  FakeHeapObject fake_ho(reader_, it->second.offset_, it->second.size_);
  i::Tagged<i::HeapObject> heap_object = fake_ho.fake_object();

  auto map_it = objects_head_.find(heap_object->map(cage_base_).address());
  CHECK(map_it != objects_head_.end());
  FakeHeapObject fake_map(reader_, map_it->second.offset_,
                          map_it->second.size_);
  i::Tagged<i::Map> map = i::UncheckedCast<i::Map>(fake_map.fake_object());
  return map->instance_type();
}

#if V8_ENABLE_WEBASSEMBLY
// see WasmArray::BodyDescriptor in objects-body-descriptors-inl.h
bool ObjectTranslator::IsReferenceForWasmArray(i::Tagged<i::Map> map) {
  // WasmArray::GcSafeElementType
  CHECK(i::InstanceType::WASM_ARRAY_TYPE == map->instance_type());
  i::Tagged<i::Object> value = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, map, i::Map::kConstructorOrBackPointerOrNativeContextOffset);
  i::Tagged<i::HeapObject> raw = i::Cast<i::HeapObject>(value);
  CHECK(objects_head_.count(raw.address()));
  FakeHeapObject fake_raw_ho(reader_, objects_head_[raw.address()]);
  i::Tagged<i::WasmTypeInfo> type_info =
      i::UncheckedCast<i::WasmTypeInfo>(fake_raw_ho.fake_object());
  return type_info->element_type().is_reference();
}

// copy WasmDispatchTable::BodyDescriptor from objects-body-descriptors-inl.h
void ObjectTranslator::WasmDispatchTableIterateBody(
    i::Tagged<i::Map> map, i::Tagged<i::HeapObject> obj, int object_size,
    i::ObjectVisitor* v) {
  // NIY
  // bool shared = HeapLayout::InAnySharedSpace(obj);
  // IndirectPointerTag tag = shared ?
  // kSharedWasmDispatchTableIndirectPointerTag
  //                                 : kWasmDispatchTableIndirectPointerTag;
  // IterateSelfIndirectPointer(obj, tag, v);
  i::BodyDescriptorBase::IterateProtectedPointer(
      obj, i::WasmDispatchTable::kProtectedOffheapDataOffset, v);
  i::BodyDescriptorBase::IterateProtectedPointer(
      obj, i::WasmDispatchTable::kProtectedUsesOffset, v);
  int length =
      i::UncheckedCast<i::WasmDispatchTable>(obj)->length(kAcquireLoad);
  for (int i = 0; i < length; ++i) {
    i::BodyDescriptorBase::IterateProtectedPointer(
        obj,
        i::WasmDispatchTable::OffsetOf(i) +
            i::WasmDispatchTable::kImplicitArgBias,
        v);
  }
}
#endif

}  // namespace dfx
}  // namespace v8
#endif
