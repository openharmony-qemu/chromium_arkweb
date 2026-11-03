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

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/dfx-visitor-list.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-format.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/heap-dump-log.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/dfx-heap-visitor-inl.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/dfx-heap-visitor.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-diagnostics.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-fake-heap-object.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-indirect-pointer-loader.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-offheap-tables.h"
#include "src/base/logging.h"
#include "src/codegen/reloc-info.h"
#include "src/common/globals.h"
#include "src/common/ptr-compr.h"
#include "src/init/heap-symbols.h"
#include "src/objects/contexts.h"
#include "src/objects/deoptimization-data.h"
#include "src/objects/dictionary-inl.h"
#include "src/objects/elements-kind.h"
#include "src/objects/embedder-data-slot.h"
#include "src/objects/field-index-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/hole.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/instruction-stream.h"
#include "src/objects/js-array.h"
#include "src/objects/js-disposable-stack.h"
#include "src/objects/js-regexp.h"
#include "src/objects/literal-objects.h"
#include "src/objects/number-string-cache.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/objects/oddball.h"
#include "src/objects/property-array-inl.h"
#include "src/objects/prototype-info-inl.h"
#include "src/objects/scope-info-inl.h"
#include "src/objects/slots.h"
#include "src/objects/swiss-name-dictionary-inl.h"
#include "src/objects/transitions-inl.h"
#include "src/objects/visitors.h"

namespace v8 {
namespace dfx {
namespace {

constexpr uint32_t kMaxObjectDetailVisitorSize =
    static_cast<uint32_t>(std::numeric_limits<int>::max());

class InternalizedStringNames {
 public:
#define STRING_METHOD(_, name, value) \
  static constexpr const char* name() { return value; }

  INTERNALIZED_STRING_LIST_GENERATOR(STRING_METHOD, _)
#undef STRING_METHOD
};

// Validates that a rawheap value names a declared V8 InstanceType. This is an
// input-integrity guard, not the DFX VisitorId support policy from
// dfx-visitor-list.h.
bool IsDeclaredV8InstanceType(i::InstanceType type) {
  switch (type) {
#define MAKE_STRING_CASE(instance_type, size, name, Name) \
  case i::InstanceType::instance_type:
    STRING_TYPE_LIST(MAKE_STRING_CASE)
#undef MAKE_STRING_CASE
#define MAKE_TORQUE_CASE(Name, TYPE) \
  case i::InstanceType::TYPE:
    TORQUE_INSTANCE_CHECKERS_SINGLE_FULLY_DEFINED(MAKE_TORQUE_CASE)
    TORQUE_INSTANCE_CHECKERS_MULTIPLE_FULLY_DEFINED(MAKE_TORQUE_CASE)
    TORQUE_INSTANCE_CHECKERS_SINGLE_ONLY_DECLARED(MAKE_TORQUE_CASE)
    TORQUE_INSTANCE_CHECKERS_MULTIPLE_ONLY_DECLARED(MAKE_TORQUE_CASE)
#undef MAKE_TORQUE_CASE
    case i::InstanceType::HOLE_TYPE:
      return true;
  }
  return false;
}

bool IsSkipContentDumpInstanceType(i::InstanceType type) {
  if (i::InstanceTypeChecker::IsBigInt(type) ||
      i::InstanceTypeChecker::IsCoverageInfo(type) ||
      i::InstanceTypeChecker::IsFeedbackMetadata(type) ||
      i::InstanceTypeChecker::IsFreeSpaceOrFiller(type) ||
      i::InstanceTypeChecker::IsHeapNumber(type) ||
      i::InstanceTypeChecker::IsHole(type) ||
      i::InstanceTypeChecker::IsSeqString(type) ||
      i::InstanceTypeChecker::IsByteArray(type) ||
      i::InstanceTypeChecker::IsFixedDoubleArray(type) ||
      i::InstanceTypeChecker::IsCppHeapExternalObject(type) ||
      i::InstanceTypeChecker::IsTrustedByteArray(type) ||
      i::InstanceTypeChecker::IsExternalString(type)) {
    return true;
  }
#if V8_ENABLE_WEBASSEMBLY
  if (i::InstanceTypeChecker::IsWasmNull(type)) {
    return true;
  }
#endif
#define DFX_CASE(Name)                         \
  if (i::InstanceTypeChecker::Is##Name(type)) { \
    return true;                               \
  }
  DFX_TORQUE_DATA_ONLY_VISITOR_ID_LIST(DFX_CASE)
#undef DFX_CASE
  return false;
}

template <typename TObject>
bool LoadHeapObjectAddress(i::PtrComprCageBase cage_base,
                           i::Tagged<TObject> object, int offset,
                           i::Address* address) {
  if (address == nullptr) {
    return false;
  }
  i::Tagged<i::Object> value =
      i::TaggedField<i::Tagged<i::Object>>::load(cage_base, object, offset);
  if (!i::IsHeapObject(value)) {
    return false;
  }
  *address = i::Cast<i::HeapObject>(value).address();
  return true;
}

inline bool TaggedFieldFits(uint32_t object_size, int offset) {
  if (offset < 0) {
    return false;
  }
  const size_t field_offset = static_cast<size_t>(offset);
  const size_t size = static_cast<size_t>(object_size);
  return field_offset <= size &&
         static_cast<size_t>(i::kTaggedSize) <= size - field_offset;
}

bool IsRegExpDataInstanceType(i::InstanceType type) {
  return type == i::REG_EXP_DATA_TYPE || type == i::ATOM_REG_EXP_DATA_TYPE ||
         type == i::IR_REG_EXP_DATA_TYPE;
}

template <typename TName, typename TObject>
void SetInternalReferenceIfHeapObject(SnapshotGenerator* generator,
                                      i::Address parent, TName name,
                                      i::PtrComprCageBase cage_base,
                                      i::Tagged<TObject> object, int offset) {
  i::Address child;
  if (LoadHeapObjectAddress(cage_base, object, offset, &child)) {
    generator->SetInternalReference(parent, name, child, offset);
  }
}

template <typename TName, typename TObject>
void SetInternalReferenceIfExistingHeapObject(SnapshotGenerator* generator,
                                              i::Address parent, TName name,
                                              i::PtrComprCageBase cage_base,
                                              i::Tagged<TObject> object,
                                              int offset) {
  i::Address child;
  if (!LoadHeapObjectAddress(cage_base, object, offset, &child)) {
    return;
  }
  if (!generator->GetNode(child)) {
    // Offline translate must not let a missing dumped child reach the
    // edge writer, and must not mark this field visited when no edge is
    // emitted. This preserves native SetInternalReference skip behavior for
    // non-entry children without fabricating a placeholder edge.
    return;
  }
  generator->SetInternalReference(parent, name, child, offset);
}

bool GetFixedArrayElementAddress(i::PtrComprCageBase cage_base,
                                 i::Tagged<i::FixedArray> array, int index,
                                 i::Address* address) {
  // Mirrors FixedArray::get(index), used by
  // V8HeapExplorer::ExtractFixedArrayReferences.
  DCHECK_NOT_NULL(address);
  *address = 0;
  DCHECK_GE(index, 0);
  DCHECK_LT(index, array->length());
  i::Tagged<i::Object> value = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base, array, i::FixedArray::OffsetOfElementAt(index));
  DCHECK(!i::HasWeakHeapObjectTag(value));
  if (!i::IsHeapObject(value)) {
    return false;
  }
  *address = i::Cast<i::HeapObject>(value).address();
  return true;
}

bool LoadEphemeronHashTableElementAddress(
    i::PtrComprCageBase cage_base, i::Tagged<i::EphemeronHashTable> table,
    int index, i::Address* address) {
  // Mirrors EphemeronHashTable::get(index), used by
  // V8HeapExplorer::ExtractEphemeronHashTableReferences.
  DCHECK_NOT_NULL(address);
  *address = 0;
  DCHECK_GE(index, 0);
  i::Tagged<i::Object> value = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base, table, i::EphemeronHashTable::OffsetOfElementAt(index));
  if (!i::IsHeapObject(value)) {
    return false;
  }
  *address = i::Cast<i::HeapObject>(value).address();
  return true;
}

template <typename TName, typename TObject>
void SetWeakReferenceIfHeapObject(SnapshotGenerator* generator,
                                  i::Address parent, TName name,
                                  i::PtrComprCageBase cage_base,
                                  i::Tagged<TObject> object, int offset) {
  i::Address child;
  if (LoadHeapObjectAddress(cage_base, object, offset, &child)) {
    generator->SetWeakReference(parent, name, child, offset);
  }
}

template <typename TName, typename TObject>
void SetWeakReferenceIfExistingHeapObject(SnapshotGenerator* generator,
                                          i::Address parent, TName name,
                                          i::PtrComprCageBase cage_base,
                                          i::Tagged<TObject> object,
                                          int offset) {
  i::Address child;
  if (!LoadHeapObjectAddress(cage_base, object, offset, &child)) {
    return;
  }
  if (!generator->GetNode(child)) {
    // Native SetWeakReference skips non-entry targets before marking the field
    // visited. Offline translate must do the same so absent dumped children do
    // not produce fabricated edges.
    return;
  }
  generator->SetWeakReference(parent, name, child, offset);
}

std::string SnapshotNodeName(SnapshotGenerator* generator, const Node* node) {
  if (node == nullptr) {
    return "";
  }
  for (const auto& entry : generator->strings()) {
    if (entry.second == node->name_) {
      return entry.first;
    }
  }
  return "";
}

std::string FormatEphemeronAnnotationName(SnapshotGenerator* generator,
                                          const Node* key_node,
                                          const Node* value_node,
                                          const Node* table_node) {
  return "part of key (" + SnapshotNodeName(generator, key_node) + " @" +
         std::to_string(key_node->id_) + ") -> value (" +
         SnapshotNodeName(generator, value_node) + " @" +
         std::to_string(value_node->id_) + ") pair in WeakMap (table @" +
         std::to_string(table_node->id_) + ")";
}

}  // namespace

void ObjectTranslator::ReportFatal(const char* code, const char* message,
                                   const char* section) {
  if (diagnostics_) {
    diagnostics_->Fatal(code, message, section);
  }
}

void ObjectTranslator::ReportCorrupt(
    const char* code, const char* message, const char* section,
    i::Address object_address, i::Address map_address, uint32_t instance_type,
    uint32_t visitor_id, uint32_t rawheap_offset) {
  if (diagnostics_) {
    diagnostics_->CorruptInput(code, message, section, object_address,
                               map_address, instance_type, visitor_id,
                               rawheap_offset);
  }
}

void ObjectTranslator::ReportWarning(
    const char* code, const char* message, const char* section,
    i::Address object_address, i::Address map_address, uint32_t instance_type,
    uint32_t visitor_id, uint32_t rawheap_offset) {
  if (diagnostics_) {
    diagnostics_->Warning(code, message, section, object_address, map_address,
                          instance_type, visitor_id, rawheap_offset);
  }
}

void ObjectTranslator::ReportUnsupported(
    const char* code, const char* message, const char* section,
    i::Address object_address, i::Address map_address, uint32_t instance_type,
    uint32_t visitor_id, uint32_t rawheap_offset) {
  if (diagnostics_) {
    diagnostics_->Unsupported(code, message, section, object_address,
                              map_address, instance_type, visitor_id,
                              rawheap_offset);
  }
}

void ObjectTranslator::SetInternalReferenceFromJSDispatchEntry(
    i::Address real_address, std::string reference_name,
    i::Tagged<i::HeapObject> object, int offset) {
  i::JSDispatchHandle handle(
      object->Relaxed_ReadField<i::JSDispatchHandle::underlying_type>(offset));
  if (offheap_table_resolver_ == nullptr ||
      !offheap_table_resolver_->IsValid()) {
    ReportWarning("js_dispatch_table_missing",
                  "JSDispatch table unavailable; cannot emit JSFunction "
                  "code edge",
                  "object_body", real_address);
    return;
  }
  const RawHeapJSDispatchEntry* entry =
      offheap_table_resolver_->FindJSDispatchCode(
          static_cast<uint32_t>(handle.value()));
  if (entry == nullptr) {
    // Warning already emitted by FindJSDispatchCode.
    return;
  }
  i::Address code_addr = static_cast<i::Address>(entry->code_object_address_);
  if (FindObjectHead(code_addr) == nullptr) {
    ReportWarning("js_dispatch_code_not_dumped",
                  "JSDispatch code target is not in dumped object table",
                  "object_body", real_address, code_addr);
    return;
  }
  i::InstanceType target_type;
  const bool has_target_type = GetInstanceType(code_addr, &target_type);
  if (!has_target_type || target_type != i::CODE_TYPE) {
    ReportWarning("js_dispatch_code_wrong_type",
                  "JSDispatch code target is not a Code object", "object_body",
                  real_address, code_addr,
                  has_target_type ? static_cast<uint32_t>(target_type) : 0);
    return;
  }
  if (generator_->GetNode(code_addr) == nullptr) {
    ReportWarning("js_dispatch_code_node_missing",
                  "JSDispatch Code target has no snapshot node; edge "
                  "suppressed",
                  "object_body", real_address, code_addr);
    return;
  }
  generator_->SetInternalReference(real_address, reference_name, code_addr,
                                   offset);
}

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
                               i::Tagged<i::HeapObject> parent_fake_object,
                               int parent_dump_size,
                               i::PtrComprCageBase cage_base,
                               i::PtrComprCageBase code_cage_base,
                               i::InstanceType instance_type)
      : i::ObjectVisitorWithCageBases(cage_base, code_cage_base),
        translator_(translator),
        parent_address_(parent_address),
        parent_fake_object_(parent_fake_object),
        parent_start_(parent_fake_object->RawMaybeWeakField(0)),
        parent_end_(parent_fake_object->RawMaybeWeakField(parent_dump_size)),
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
    if (has_corrupt_input_) {
      return;
    }
    if (!CheckSlotRange(start.address(), end.address())) {
      return;
    }
    for (i::MaybeObjectSlot slot = start; slot < end; ++slot) {
      VisitSlotImpl(host, cage_base_, slot);
      if (has_corrupt_input_) {
        return;
      }
    }
  }

  // code.h
  void VisitInstructionStreamPointer(i::Tagged<i::Code> host,
                                     i::InstructionStreamSlot slot) override {
    // The instruction_stream edge is handled by the object-specific path in
    // ObjectTranslator::ExtractCodeReferences (code_cage_base_ decode +
    // FindObjectHead membership check + IsInstructionStream type guard).
    // Emitting via VisitSlotImpl here would either bypass the cage/type
    // guardrail or produce a duplicate edge; suppress in both variants.
    FieldLog;
    USE(host);
    USE(slot);
  }

  // InstructionStream::BodyDescriptor iterates reloc infos and dispatches
  // CODE_TARGET / EMBEDDED_OBJECT entries here. The supported offline extraction
  // for instruction streams is handled by TranslateInstructionStream and
  // ExtractInstructionStreamReferences. If V8 starts feeding these callbacks here
  // through a future/corrupt layout, they are treated as unsupported and skipped
  // instead of terminating translation.
  // TODO(sandbox-kpi): add explicit offline relocation-kind routing when
  // available in the rawheap payload.
  void VisitCodeTarget(i::Tagged<i::InstructionStream> host,
                       i::RelocInfo* rinfo) override {
    FieldLog;
    USE(host);
    USE(rinfo);
    ReportUnsupported("instruction_stream_code_target_reloc_unsupported",
                      "InstructionStream CODE_TARGET relocation callback reached "
                      "in offline translation; skipping");
  }
  // same as VisitCodeTarget
  void VisitEmbeddedPointer(i::Tagged<i::InstructionStream> host,
                            i::RelocInfo* rinfo) override {
    FieldLog;
    USE(host);
    USE(rinfo);
    ReportUnsupported("instruction_stream_embedded_object_reloc_unsupported",
                      "InstructionStream EMBEDDED_OBJECT relocation callback "
                      "reached in offline translation; skipping");
  }

  // IterateTrustedPointer will call this. IndirectPointerSlot is a index to an
  // entry in a pointer table.
  void VisitIndirectPointer(i::Tagged<i::HeapObject> host,
                            i::IndirectPointerSlot slot,
                            i::IndirectPointerMode mode) override {
    FieldLog;
#if defined(V8_ENABLE_SANDBOX)
    USE(host);
    USE(mode);
    if (has_corrupt_input_) {
      return;
    }
    if (!CheckSlotRange(slot.address(), slot.address() + i::kTaggedSize)) {
      return;
    }
    // visited_fields_ is indexed like SnapshotGenerator::MarkVisitedField:
    // offsets are divided by kTaggedSize, even when the descriptor visits an
    // indirect-pointer slot.
    const size_t field_index =
        (slot.address() - parent_start_.address()) / i::kTaggedSize;
    std::vector<bool>& visited = translator_->generator()->visited_fields_;
    if (field_index >= visited.size()) {
      ReportCorrupt(
          "object body descriptor indirect pointer field index is outside the "
          "dumped object payload");
      return;
    }
    if (visited[field_index]) {
      visited[field_index] = false;
      return;
    }
    // handle is a uint32_t type index, see IndirectPointerSlot::load
    i::IndirectPointerHandle handle = slot.Relaxed_LoadHandle();
    ResolveHandle(handle, slot.tag(), slot.address());
#else
    USE(host);
    USE(slot);
    USE(mode);
#endif
  }

  // a trusted object can use protected pointer find another trusted object
  void VisitProtectedPointer(i::Tagged<i::TrustedObject> host,
                             i::ProtectedPointerSlot slot) override {
    FieldLog;
#ifdef V8_ENABLE_SANDBOX
    if (!translator_->has_trusted_cage_base_) {
      ReportWarning("sandbox_protected_pointer_missing_trusted_cage_base",
                    "ProtectedPointerSlot requires trusted cage base from "
                    "rawheap v3; skipping protected pointer edge");
      return;
    }
    // Sandbox builds alias ProtectedPointerSlot to
    // OffHeapCompressedObjectSlot<TrustedSpaceCompressionScheme> in globals.h.
    // VisitSlotImpl still passes cage_base_ to keep the shared slot/range
    // bookkeeping path, but OffHeapCompressedObjectSlotBase::load(cage_base) in
    // compressed-slots-inl.h ignores that parameter and decodes with its
    // template CompressionScheme, i.e. TrustedSpaceCompressionScheme. The
    // scheme base is initialized from rawheap v3 trusted_cage_base_ in
    // HeapTranslator::TranslateHeader before object traversal.
    VisitSlotImpl(host, cage_base_, slot);
#else
    // No-sandbox builds typedef ProtectedPointerSlot to a normal object slot.
    // Reuse the generic visitor path so range checks/visited_fields_ logic and
    // weak/strong handling remain identical to ordinary slots.
    VisitSlotImpl(host, cage_base_, slot);
#endif
  }
  void VisitProtectedPointer(i::Tagged<i::TrustedObject> host,
                             i::ProtectedMaybeObjectSlot slot) override {
    FieldLog;
#ifdef V8_ENABLE_SANDBOX
    if (!translator_->has_trusted_cage_base_) {
      ReportWarning("sandbox_protected_pointer_missing_trusted_cage_base",
                    "ProtectedMaybeObjectSlot requires trusted cage base "
                    "from rawheap v3; skipping protected pointer edge");
      return;
    }
    // Same as ProtectedPointerSlot: the sandbox slot type is
    // OffHeapCompressedMaybeObjectSlot<TrustedSpaceCompressionScheme>. The
    // cage_base_ argument is not used by compressed-slots-inl.h for this slot;
    // decoding comes from TrustedSpaceCompressionScheme::base().
    VisitSlotImpl(host, cage_base_, slot);
#else
    // No-sandbox builds typedef ProtectedMaybeObjectSlot to a normal maybe
    // slot. Reuse the generic visitor path so compressed decode and
    // duplicate-suppression semantics stay centralized in VisitSlotImpl().
    VisitSlotImpl(host, cage_base_, slot);
#endif
  }

  void VisitJSDispatchTableEntry(i::Tagged<i::HeapObject> host,
                                 i::JSDispatchHandle handle) override {
    FieldLog;
    USE(host);
    USE(handle);
    // Native IndexedReferencesExtractor does not emit edges from this hook. It
    // only consumes the JSFunction dispatch field that the object-specific
    // extractor already marked for the "code" edge, and ignores FeedbackCell,
    // Code, and InstructionStream dispatch handles.
    if (instance_type_ >= i::FIRST_JS_FUNCTION_TYPE &&
        instance_type_ <= i::LAST_JS_FUNCTION_TYPE) {
      const size_t field_index =
          i::JSFunction::kDispatchHandleOffset / i::ObjectSlot::kSlotDataSize;
      std::vector<bool>& visited = translator_->generator()->visited_fields_;
      if (field_index < visited.size() && visited[field_index]) {
        visited[field_index] = false;
      }
    }
  }

  bool has_corrupt_input() const { return has_corrupt_input_; }

 private:
  bool CheckSlotRange(i::Address start, i::Address end) {
    i::Address parent_start = parent_start_.address();
    i::Address parent_end = parent_end_.address();
    if (start < parent_start || end < start || end > parent_end) {
      ReportCorrupt(
          "object body descriptor visited a slot range outside the "
          "dumped object payload");
      return false;
    }
    return true;
  }

  void ReportCorrupt(const char* message) {
    has_corrupt_input_ = true;
    translator_->ReportCorrupt("object_slot_out_of_bounds", message,
                               "object_body", parent_address_, 0,
                               static_cast<uint32_t>(instance_type_));
  }

  void ReportWarning(const char* code, const char* message) {
    translator_->ReportWarning(code, message, "object_body", parent_address_, 0,
                               static_cast<uint32_t>(instance_type_));
  }

  void ReportUnsupported(const char* code, const char* message) {
    translator_->ReportUnsupported(code, message, "object_body",
                                   parent_address_, 0,
                                   static_cast<uint32_t>(instance_type_));
  }

  // see heap-snapshot-generator.cc IndexedReferencesExtractor
  template <typename TSlot>
  void VisitSlotImpl(i::Tagged<i::HeapObject> host,
                     i::PtrComprCageBase cage_base, TSlot slot) {
    USE(host);
    if (has_corrupt_input_) {
      return;
    }
    i::Address slot_address = slot.address();
    i::Address parent_start = parent_start_.address();
    i::Address parent_end = parent_end_.address();
    if (slot_address < parent_start || slot_address > parent_end ||
        TSlot::kSlotDataSize > parent_end - slot_address) {
      ReportCorrupt(
          "object body descriptor visited a slot outside the dumped "
          "object payload");
      return;
    }
    size_t field_index = (slot_address - parent_start) / TSlot::kSlotDataSize;
    std::vector<bool>& visited = translator_->generator()->visited_fields_;
    if (field_index >= visited.size()) {
      ReportCorrupt(
          "object body descriptor field index is outside the dumped "
          "object payload");
      return;
    }
    if (visited[field_index]) {
      visited[field_index] = false;
      return;
    }
    FieldLog;
    int field_offset = static_cast<int>(field_index) * TSlot::kSlotDataSize;
    i::Tagged<i::HeapObject> heap_object;
    auto loaded_value = slot.load(cage_base);
    if (loaded_value.GetHeapObjectIfStrong(&heap_object)) {
      VisitHeapObjectImpl(heap_object.address(), field_offset);
    } else if (loaded_value.GetHeapObjectIfWeak(&heap_object)) {
      if (translator_->FindObjectHead(heap_object.address()) == nullptr) {
        return;
      }
      if (translator_->generator()->GetNode(heap_object.address()) == nullptr) {
        ReportWarning("object_weak_target_node_missing",
                      "weak HeapObject target is in rawheap object table but "
                      "has no snapshot node");
        return;
      }
      translator_->generator()->SetWeakReference(parent_address_, next_index_++,
                                                 heap_object.address(), {});
    }
    // Smi/non-HeapObject slots are not V8 heap-object edges.
  }

  bool IsEssentialHiddenReference(int field_offset) {
    if (instance_type_ == i::InstanceType::ALLOCATION_SITE_TYPE &&
        field_offset == offsetof(i::AllocationSiteWithWeakNext, weak_next_))
      return false;
    if (instance_type_ >= i::FIRST_CONTEXT_TYPE &&
        instance_type_ <= i::LAST_CONTEXT_TYPE &&
        field_offset ==
            i::Context::OffsetOfElementAt(i::Context::NEXT_CONTEXT_LINK))
      return false;
    if (instance_type_ == i::InstanceType::JS_FINALIZATION_REGISTRY_TYPE &&
        field_offset == i::JSFinalizationRegistry::kNextDirtyOffset)
      return false;
    return true;
  }

  void VisitHeapObjectImpl(i::Address child, int field_offset) {
    if (translator_->FindObjectHead(child) == nullptr) {
      return;
    }
    if (translator_->generator()->GetNode(child) == nullptr) {
      ReportWarning("object_hidden_target_node_missing",
                    "HeapObject target is in rawheap object table but has no "
                    "snapshot node");
      return;
    }
    if (!IsEssentialHiddenReference(field_offset)) {
      return;
    }
    translator_->generator()->SetHiddenReference(parent_address_, next_index_++,
                                                 child);
  }

  // see ResolveHandle in slots-inl.h
  void ResolveHandle(i::IndirectPointerHandle handle, i::IndirectPointerTag tag,
                     i::Address slot_address) {
#if defined(V8_ENABLE_SANDBOX)
    // Match V8 slot resolution semantics: null indirect pointer handles resolve to
    // Smi::zero() and should be dropped without diagnostics.
    if (handle == i::kNullIndirectPointerHandle) {
      return;
    }
    if (translator_->offheap_table_resolver_ == nullptr ||
        !translator_->offheap_table_resolver_->IsValid()) {
      ReportUnsupported("sandbox_indirect_pointer_table_missing",
                        "indirect pointer slot cannot be resolved without a "
                        "dumped sandbox pointer table");
      return;
    }

    if (tag == i::kUnknownIndirectPointerTag) {
      if (handle & i::kCodePointerHandleMarker) {
        ResolveCodePointerHandle(handle);
      } else {
        ResolveTrustedPointerHandle(handle, tag, slot_address);
      }
    } else if (tag == i::kCodeIndirectPointerTag) {
      ResolveCodePointerHandle(handle);
    } else {
      ResolveTrustedPointerHandle(handle, tag, slot_address);
    }
#else
    USE(handle);
    USE(tag);
    USE(slot_address);
#endif
  }

#if defined(V8_ENABLE_SANDBOX)
  void ResolveCodePointerHandle(i::IndirectPointerHandle handle) {
    const RawHeapSandboxCodePointerEntry* entry =
        translator_->offheap_table_resolver_->FindCodePointer(handle);
    if (entry == nullptr) {
      return;
    }
    i::Address code_addr = static_cast<i::Address>(entry->code_object_address_);
    // CodePointerTable::GetCodeObject stores the tagged Object value used by
    // IndirectPointerSlot::ResolveCodePointerHandle(). Rawheap object-table
    // keys are untagged HeapObject::address() values, so normalize here instead
    // of dereferencing the offline pointer or changing the producer schema for
    // this consumer-only KPI slice.
    if (translator_->FindObjectHead(code_addr) == nullptr &&
        (code_addr & i::kHeapObjectTag) == i::kHeapObjectTag) {
      const i::Address untagged_code_addr = code_addr - i::kHeapObjectTag;
      if (translator_->FindObjectHead(untagged_code_addr) != nullptr) {
        code_addr = untagged_code_addr;
      }
    }
    if (translator_->FindObjectHead(code_addr) == nullptr) {
      ReportWarning("sandbox_code_pointer_target_not_dumped",
                    "CodePointer target is not in dumped object table");
      return;
    }
    // CodePointer slots are native generic references. Only emit the
    // hidden/indexed edge after proving the dumped target is a Code object.
    i::InstanceType target_type;
    if (!translator_->GetInstanceType(code_addr, &target_type) ||
        target_type != i::CODE_TYPE) {
      ReportWarning("sandbox_code_pointer_target_wrong_type",
                    "CodePointer target is not a Code object");
      return;
    }
    // HeapObject membership is proven via FindObjectHead above; CODE_TYPE is
    // confirmed. Node absence here means the Code object was not included in
    // the snapshot — not that the target is native/offheap. Emit a diagnostic
    // explicitly rather than relying on SetReference to silently drop the edge
    // inside its sandbox log path.
    if (translator_->generator()->GetNode(code_addr) == nullptr) {
      ReportWarning("sandbox_code_pointer_code_node_missing",
                    "CodePointer Code target has no snapshot node; edge "
                    "suppressed");
      return;
    }
    translator_->generator()->SetHiddenReference(parent_address_, next_index_++,
                                                 code_addr);
  }

  void ResolveTrustedPointerHandle(i::IndirectPointerHandle handle,
                                   i::IndirectPointerTag tag,
                                   i::Address slot_address) {
    USE(slot_address);
    const RawHeapSandboxTrustedPointerEntry* entry =
        translator_->offheap_table_resolver_->FindTrustedPointer(
            handle, static_cast<uint32_t>(tag));
    if (entry == nullptr) {
      return;
    }
    // TrustedPointer/IndirectPointer targets may also be native/off-V8-heap
    // resources. Only rawheap object-table membership proves a dumped V8
    // HeapObject target; once proven, generic traversal should emit the hidden
    // edge without owner/type special cases.
    i::Address target = static_cast<i::Address>(entry->trusted_object_address_);
    if (translator_->FindObjectHead(target) == nullptr &&
        (target & i::kHeapObjectTag) == i::kHeapObjectTag) {
      const i::Address untagged_target = target - i::kHeapObjectTag;
      if (translator_->FindObjectHead(untagged_target) != nullptr) {
        target = untagged_target;
      }
    }
    if (translator_->FindObjectHead(target) == nullptr) {
      return;
    }
    if (translator_->generator()->GetNode(target) == nullptr) {
      ReportWarning("sandbox_trusted_pointer_target_node_missing",
                    "TrustedPointer target is in rawheap object table but has "
                    "no snapshot node");
      return;
    }
    translator_->generator()->SetHiddenReference(parent_address_, next_index_++,
                                                 target);
  }
#endif  // V8_ENABLE_SANDBOX

  ObjectTranslator* translator_;
  i::Address parent_address_;
  i::Tagged<i::HeapObject> parent_fake_object_;
  i::MaybeObjectSlot parent_start_;
  i::MaybeObjectSlot parent_end_;
  i::PtrComprCageBase cage_base_;
  i::PtrComprCageBase code_cage_base_;
  i::InstanceType instance_type_;

  uint32_t next_index_{0};
  bool has_corrupt_input_{false};
};  // namespace dfx

//////////////////// ObjectTranslator
const ObjectTranslator::RawHeapObjectHead* ObjectTranslator::FindObjectHead(
    i::Address real_address) const {
  auto it = objects_head_.find(real_address);
  if (it == objects_head_.end()) {
    return nullptr;
  }
  return &it->second;
}

std::unique_ptr<FakeHeapObject> ObjectTranslator::MakeFakeHeapObject(
    i::Address real_address) {
  const RawHeapObjectHead* head = FindObjectHead(real_address);
  if (head == nullptr) {
    return nullptr;
  }
  auto fake_object = std::make_unique<FakeHeapObject>(reader_, head->offset_,
                                                     head->size_);
  if (!fake_object->is_valid()) {
    return nullptr;
  }
  return fake_object;
}

#if defined(OH_ENABLE_HEAP_DUMP_TEST)
bool ObjectTranslator::TestLookupFakeObjectMapAddress(i::Address real_address,
                                                      i::Address* map_address) {
  if (map_address == nullptr) {
    return false;
  }
  *map_address = 0;

  std::unique_ptr<FakeHeapObject> fake_object =
      MakeFakeHeapObject(real_address);
  if (!fake_object) {
    return false;
  }
  *map_address = fake_object->fake_object()->map(cage_base_).address();
  return true;
}
#endif

ObjectTranslator::ObjectTranslator(
    BinaryReaderBase* reader, uint32_t offset, uint32_t size, uint32_t count,
    i::PtrComprCageBase cage_base, i::PtrComprCageBase code_cage_base,
    bool has_trusted_cage_base, SnapshotGenerator* generator,
    TranslateDiagnostics* diagnostics,
    const OffheapTableResolver* offheap_table_resolver)
    : reader_(reader),
      table_offset_(offset),
      table_size_(size),
      object_count_(count),
      cage_base_(cage_base),
      code_cage_base_(code_cage_base),
      has_trusted_cage_base_(has_trusted_cage_base),
      generator_(generator),
      diagnostics_(diagnostics),
      offheap_table_resolver_(offheap_table_resolver) {
  if (reader_ == nullptr || generator_ == nullptr) {
    is_valid_ = false;
    ReportFatal("object_null_dependency",
                "ObjectTranslator missing reader or generator", "object_table");
    return;
  }
  if (reader->CurrentPosition() != offset) {
    is_valid_ = false;
    ReportCorrupt("object_offset_mismatch",
                  "reader is not positioned at object table", "object_table", 0,
                  0, 0, 0, reader->CurrentPosition());
    return;
  }
  if (!(size < std::numeric_limits<uint32_t>::max() - offset &&
        size + offset <= reader->BinarySize())) {
    if (i::v8_flags.log_heapdump) {
      LogInfo("size " + std::to_string(size) + " offset " +
              std::to_string(offset) + " reader size " +
              std::to_string(reader->BinarySize()));
    }
    is_valid_ = false;
    ReportCorrupt("object_table_out_of_bounds",
                  "object table extends beyond rawheap file", "object_table", 0,
                  0, 0, 0, offset);
  }
}

bool ObjectTranslator::PreVisit() {
  if (!is_valid_) {
    return false;
  }
  if (i::v8_flags.log_heapdump) {
    std::cout << "PreVisit\n";
  }
  // a raw object = address + size + map_pointer + [payload]
  const uint32_t MinSizeOfRawObject =
      sizeof(i::Address) + sizeof(uint32_t) + sizeof(i::Tagged_t);
  if (object_count_ != 0 && table_size_ < MinSizeOfRawObject) {
    ReportCorrupt("object_table_too_small",
                  "object table is smaller than one record", "object_table", 0,
                  0, 0, 0, table_offset_);
    return false;
  }
  uint32_t table_end = table_size_ + table_offset_;

  // Pass 1: index the object table without constructing edges. Later fake
  // object reads use this non-inserting address -> payload lookup; rawheap
  // addresses are identities and must not be dereferenced in this process.
  for (uint32_t i = 0; i < object_count_; i++) {
    if (reader_->CurrentPosition() > table_end ||
        MinSizeOfRawObject > table_end - reader_->CurrentPosition()) {
      ReportCorrupt("object_record_truncated",
                    "object record header is truncated", "object_table", 0, 0,
                    0, 0, reader_->CurrentPosition());
      return false;
    }
    // read address and size
    i::Address address{0};
    if (!reader_->ReadData(sizeof(address),
                           reinterpret_cast<uint8_t*>(&address),
                           sizeof(address))) {
      ReportCorrupt("object_address_read_failed",
                    "failed to read object address", "object_table", 0, 0, 0, 0,
                    reader_->CurrentPosition());
      return false;
    }
    uint32_t dump_size{0};
    if (!reader_->ReadData(sizeof(dump_size),
                           reinterpret_cast<uint8_t*>(&dump_size),
                           sizeof(dump_size))) {
      ReportCorrupt("object_size_read_failed",
                    "failed to read object dump size", "object_table", address,
                    0, 0, 0, reader_->CurrentPosition());
      return false;
    }
    if (dump_size < sizeof(i::Tagged_t)) {
      ReportCorrupt("object_payload_too_small",
                    "object payload is smaller than the heap object map field",
                    "object_table", address, 0, 0, 0,
                    reader_->CurrentPosition());
      return false;
    }
    if (dump_size > kMaxObjectDetailVisitorSize) {
      ReportCorrupt("object_payload_too_large",
                    "object payload exceeds the translator visitor size limit",
                    "object_table", address, 0, 0, 0,
                    reader_->CurrentPosition());
      return false;
    }
    if (auto [it, inserted] = objects_head_.try_emplace(
            address,
            RawHeapObjectHead{static_cast<uint32_t>(reader_->CurrentPosition()),
                              dump_size, false});
        !inserted) {
      if (i::v8_flags.log_heapdump) {
        std::cout << "HeapDump: same address insert twice addr " << address
                  << " size " << dump_size << " offset "
                  << reader_->CurrentPosition() << std::endl;
      }
      ReportCorrupt("duplicate_object_address",
                    "object address appears more than once", "object_table",
                    address, 0, 0, 0, reader_->CurrentPosition());
      return false;
    }
    if (dump_size > table_end - reader_->CurrentPosition()) {
      ReportCorrupt("object_payload_truncated",
                    "object payload exceeds object table", "object_table",
                    address, 0, 0, 0, reader_->CurrentPosition());
      return false;
    }
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

  if (objects_head_.size() != object_count_ ||
      table_end != reader_->CurrentPosition()) {
    ReportCorrupt("object_table_size_mismatch",
                  "object table did not end at declared size", "object_table",
                  0, 0, 0, 0, reader_->CurrentPosition());
    return false;
  }

  // Pass 2: create all snapshot nodes before any references are emitted. This
  // matches V8HeapExplorer::IterateAndExtractReferences, where every object has
  // an entry before SetInternalReference/SetPropertyReference looks up targets.
  reader_->SetPosition(table_offset_);
  for (uint32_t i = 0; i < object_count_; i++) {
    // read address and size
    i::Address address{0};
    uint32_t dump_size{0};
    if (!reader_->ReadData(sizeof(address),
                           reinterpret_cast<uint8_t*>(&address),
                           sizeof(address)) ||
        !reader_->ReadData(sizeof(dump_size),
                           reinterpret_cast<uint8_t*>(&dump_size),
                           sizeof(dump_size))) {
      ReportCorrupt("object_record_reread_failed",
                    "failed to reread object record", "object_table", 0, 0, 0,
                    0, reader_->CurrentPosition());
      return false;
    }

    if (dump_size > table_end - reader_->CurrentPosition()) {
      ReportCorrupt("object_payload_reread_truncated",
                    "object payload exceeds object table", "object_table",
                    address, 0, 0, 0, reader_->CurrentPosition());
      return false;
    }
    FakeHeapObject fake_ho(reader_, dump_size);
    if (!fake_ho.is_valid()) {
      ReportCorrupt("object_payload_read_failed",
                    "failed to read object payload", "object_table", address, 0,
                    0, 0, reader_->CurrentPosition());
      return false;
    }
    i::Tagged<i::HeapObject> fake_heap_object = fake_ho.fake_object();

    ////////// map
    i::Tagged<i::Map> map = fake_heap_object->map(cage_base_);

    auto map_it = objects_head_.find(map.address());
    if (map_it == objects_head_.end()) {
      ReportCorrupt("object_map_missing",
                    "object map was not present in rawheap", "object_table",
                    address, map.address(), 0, 0, reader_->CurrentPosition());
      generator_->AddNode(address, Node::Type::kObject, "<missing map>",
                          dump_size);
      continue;
    } else {
      map_it->second.is_map_ = true;
    }
    FakeHeapObject fake_map_ho(reader_, map_it->second.offset_,
                               map_it->second.size_);
    if (!fake_map_ho.is_valid()) {
      ReportCorrupt("object_map_read_failed",
                    "failed to read object map payload", "object_table",
                    address, map.address(), 0, 0, map_it->second.offset_);
      return false;
    }
    i::Tagged<i::Map> fake_map =
        UncheckedCast<i::Map>(fake_map_ho.fake_object());

    uint32_t real_size = dump_size;
    if (SkipContentDump(fake_map)) {
      if (IsDeclaredV8InstanceType(fake_map->instance_type()) &&
          !IsSkipContentDumpInstanceType(fake_map->instance_type())) {
        ReportCorrupt("map_only_instance_type_mismatch",
                      "map-only object visitor id does not match the map "
                      "instance type",
                      "object_table", address, map.address(),
                      static_cast<uint32_t>(fake_map->instance_type()),
                      static_cast<uint32_t>(fake_map->visitor_id()),
                      reader_->CurrentPosition());
        return false;
      }
      if (!fake_ho.TryReadNoContentRealSize(&real_size)) {
        ReportCorrupt("map_only_payload_size_invalid",
                      "map-only object payload does not match no-content "
                      "object dump shape",
                      "object_table", address, map.address(),
                      static_cast<uint32_t>(fake_map->instance_type()),
                      static_cast<uint32_t>(fake_map->visitor_id()),
                      reader_->CurrentPosition());
        return false;
      }
    }
    if (!AddNode(fake_map, fake_heap_object, address, real_size)) {
      generator_->AddNode(address, Node::Type::kObject, "<dummy>", real_size);
    }
  }
  if (diagnostics_ != nullptr && diagnostics_->has_errors()) {
    return false;
  }
  return true;
}

bool ObjectTranslator::Translate() {
  if (!PreVisit()) {
    return false;
  }
  reader_->SetPosition(table_offset_);

  if (i::v8_flags.log_heapdump) {
    std::cout << "Translate\n";
  }
  for (uint32_t i = 0; i < object_count_; i++) {
    // The table shape was validated in PreVisit(); this pass rebuilds fake
    // objects from the same payload offsets and emits edges.
    i::Address address{0};
    uint32_t dump_size{0};
    if (!reader_->ReadData(sizeof(address),
                           reinterpret_cast<uint8_t*>(&address),
                           sizeof(address)) ||
        !reader_->ReadData(sizeof(dump_size),
                           reinterpret_cast<uint8_t*>(&dump_size),
                           sizeof(dump_size))) {
      ReportCorrupt("object_record_translate_read_failed",
                    "failed to read object record for translate",
                    "object_table", 0, 0, 0, 0, reader_->CurrentPosition());
      return false;
    }
#if defined(OH_ENABLE_HEAP_DUMP_TEST)
    if (i::v8_flags.log_heapdump) {
      std::cout << "address:" << address << " dump_size:" << dump_size
                << " object end:" << (address + dump_size) << std::endl;
    }
#endif
    auto it = objects_head_.find(address);
    if (it == objects_head_.end() ||
        it->second.offset_ != reader_->CurrentPosition() ||
        it->second.size_ != dump_size) {
      ReportCorrupt("object_index_mismatch",
                    "object table index changed after previsit", "object_table",
                    address, 0, 0, 0, reader_->CurrentPosition());
      return false;
    }

    uint32_t payload_offset = reader_->CurrentPosition();
    if (!TranslateHeapObject(address, dump_size)) {
      return false;
    }
    reader_->SetPosition(payload_offset + dump_size);
  }
  return true;
}

// Add Edges
bool ObjectTranslator::TranslateHeapObject(i::Address addr,
                                           uint32_t dump_size) {
  const RawHeapObjectHead* object_head = FindObjectHead(addr);
  if (object_head == nullptr || object_head->size_ != dump_size) {
    ReportCorrupt("translate_object_table_lookup_failed",
                  "object was not found in the rawheap object table",
                  "object_table", addr, 0, 0, 0, reader_->CurrentPosition());
    return false;
  }

  std::unique_ptr<FakeHeapObject> fake_ho = MakeFakeHeapObject(addr);
  if (!fake_ho) {
    ReportCorrupt("translate_object_payload_read_failed",
                  "failed to read object payload", "object_table", addr, 0, 0,
                  0, object_head->offset_);
    return false;
  }
  i::Tagged<i::HeapObject> fake_heap_object = fake_ho->fake_object();

  ////////// map
  i::Tagged<i::Map> map = fake_heap_object->map(cage_base_);

  const RawHeapObjectHead* map_head = FindObjectHead(map.address());
  if (map_head == nullptr) {
    ReportCorrupt("translate_object_map_missing",
                  "object map was not present in rawheap", "object_table", addr,
                  map.address(), 0, 0, reader_->CurrentPosition());
    return false;
  }

#if defined(OH_ENABLE_HEAP_DUMP_TEST)
  if (!map_head->is_map_) {
    ReportWarning("translate_object_map_not_marked",
                  "object map was present but not marked as map in "
                  "test previsit bookkeeping",
                  "object_table", addr, map.address());
  }
#endif

  std::unique_ptr<FakeHeapObject> fake_map_obj =
      MakeFakeHeapObject(map.address());
  if (!fake_map_obj) {
    ReportCorrupt("translate_object_map_read_failed",
                  "failed to read object map payload", "object_table", addr,
                  map.address(), 0, 0, map_head->offset_);
    return false;
  }
  i::Tagged<i::Map> fake_map =
      UncheckedCast<i::Map>(fake_map_obj->fake_object());
#if defined(OH_ENABLE_HEAP_DUMP_TEST)
  if (i::v8_flags.log_heapdump) {
    std::cout << "instance type:" << i::ToString(fake_map->instance_type())
              << " " << std::endl;
  }
#endif

  // visited_fields_ mirrors heap-snapshot-generator.cc
  // IndexedReferencesExtractor. Specialized extraction marks named fields so
  // the generic body visitor does not also emit hidden edges for them.
  generator_->max_pointers_ = dump_size / i::kTaggedSize;
  if (generator_->max_pointers_ > generator_->visited_fields_.size()) {
    // Reallocate to right size.
    generator_->visited_fields_.resize(generator_->max_pointers_, false);
  }
  generator_->visited_fields_.assign(generator_->max_pointers_, false);

  // Some layouts require snapshot-aligned special extraction before the generic
  // body visitor sees remaining fields.
  ////////// visit object
  if (dump_size > kMaxObjectDetailVisitorSize) {
    ReportCorrupt("object_payload_too_large",
                  "object payload exceeds the translator visitor size limit",
                  "object_body", addr, map.address(),
                  static_cast<uint32_t>(fake_map->instance_type()),
                  static_cast<uint32_t>(fake_map->visitor_id()),
                  reader_->CurrentPosition());
    return false;
  }
  ObjectDetailVisitor visitor(this, addr, fake_heap_object,
                              static_cast<int>(dump_size), cage_base_,
                              code_cage_base_, fake_map->instance_type());

  if (SkipContentDump(fake_map)) {
    if (IsDeclaredV8InstanceType(fake_map->instance_type()) &&
        !IsSkipContentDumpInstanceType(fake_map->instance_type())) {
      ReportCorrupt("map_only_instance_type_mismatch",
                    "map-only object visitor id does not match the map "
                    "instance type",
                    "object_body", addr, map.address(),
                    static_cast<uint32_t>(fake_map->instance_type()),
                    static_cast<uint32_t>(fake_map->visitor_id()),
                    reader_->CurrentPosition());
      return false;
    }
    generator_->SetInternalReference(addr, "map", map.address(),
                                     i::HeapObject::kMapOffset);
    return true;
  }
  // Add snapshot-aligned specialized edges before the generic visitor handles
  // any remaining unvisited fields. The function below follows
  // V8HeapExplorer::ExtractReferences, but every field is read from fake
  // rawheap bytes and every child is resolved through the rawheap object table.
  if (!ExtractReferences(fake_heap_object, fake_map, addr, map.address())) {
    return false;
  }
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
      ReportCorrupt("invalid_visitor_id_for_object",
                    "object has a non-translatable visitor id", "object_body",
                    addr, map.address(), static_cast<uint32_t>(instance_type),
                    static_cast<uint32_t>(visitor_id));
      return false;
    case i::VisitorId::kVisitInstructionStream: {
      return TranslateInstructionStream(addr, dump_size);
    }
#ifdef V8_ENABLE_WEBASSEMBLY
    case i::VisitorId::kVisitWasmArray: {
      // ExtractWasmArrayReferences already emitted snapshot-aligned element
      // edges from fake rawheap bytes. Do not run the generic body visitor,
      // which would turn array elements into hidden edges.
      return true;
    }
    case i::VisitorId::kVisitWasmStruct: {
      // WasmStruct has a dynamic body layout. ExtractReferences reads and
      // validates map->wasm_type_info()->type_index(), then uses the offheap
      // canonical type table when present. Keep it out of generic body
      // traversal so field bytes are interpreted only with that layout.
      return true;
    }
    case i::VisitorId::kVisitWasmTrustedInstanceData: {
      // ExtractWasmTrustedInstanceDataReferences covers the native
      // heapsnapshot named-field extraction for the fixed tagged/protected
      // field lists. Do not run the generic BodyDescriptor here:
      // WasmTrustedInstanceData::BodyDescriptor calls HeapLayout helpers such
      // as InAnySharedSpace(), which require a real V8 heap page and are not
      // valid for offline fake heap objects.
      return true;
    }
    case i::VisitorId::kVisitWasmDispatchTable: {
      WasmDispatchTableIterateBody(
          fake_map, fake_heap_object,
          i::WasmDispatchTable::BodyDescriptor::SizeOf(
              fake_map,
              i::UncheckedCast<i::WasmDispatchTable>(fake_heap_object)),
          &visitor);
      return !visitor.has_corrupt_input();
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
        if (i::v8_flags.log_heapdump) {
          LogInfo("[HeapDump]Instance type" + std::string(name) +
                  "  not supported.");
        }
        ReportUnsupported("visitor_id_unsupported",
                          "object body visitor is not supported by this "
                          "translator",
                          "object_body", addr, map.address(),
                          static_cast<uint32_t>(instance_type),
                          static_cast<uint32_t>(visitor_id));
        return false;
      }
      return true;
  }
  return !visitor.has_corrupt_input();
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
    std::string name = GetClassName(map, object, address, size);
    if (name.empty()) {
      UNREACHABLE();
    }
    if (i::InstanceTypeChecker::IsJSGlobalObject(instance_type)) {
      // NIY(hh): native_context_tag_map_ not available in translate-objects.cc
      // In heap-snapshot-generator.cc: uses native_context_tag_map_ to get
      // context tag
      name = name + " (global*)";
    } else if (i::InstanceTypeChecker::IsJSGlobalProxy(instance_type)) {
      // NIY(hh): native_context_tag_map_ not available - cannot get creation
      // context In heap-snapshot-generator.cc: uses GetCreationContext() and
      // native_context_tag_map_
      name = name + " (global)";
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
    const char* system_entry_name = GetSystemEntryName(instance_type);
    if (system_entry_name == nullptr) {
      ReportWarning("unknown_instance_type",
                    "object map has an unknown InstanceType; using fallback "
                    "system entry name",
                    "object_table", address, map.address(),
                    static_cast<uint32_t>(instance_type));
      system_entry_name = "system / Unknown";
    }
    return generator_->AddNode(address, GetSystemEntryType(instance_type),
                               system_entry_name, node_size);
  }

  // NIY
  const char* system_entry_name = GetSystemEntryName(instance_type);
  if (system_entry_name == nullptr) {
    ReportWarning("unknown_instance_type",
                  "object map has an unknown InstanceType; using fallback "
                  "system entry name",
                  "object_table", address, map.address(),
                  static_cast<uint32_t>(instance_type));
    system_entry_name = "system / Unknown";
  }
  return generator_->AddNode(address, GetSystemEntryType(instance_type),
                             system_entry_name, size);
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
          type)  // NIY: &&
                 // !MemoryChunk::FromHeapObject(object)->InReadOnlySpace()
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
  if (!IsDeclaredV8InstanceType(type)) {
    return nullptr;
  }

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

  // Avoid terminating on malformed enum values read from untrusted rawheap
  // bytes. Callers emit a diagnostic and use a generic system node name.
  return nullptr;
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
                                           i::Address address, uint32_t size) {
  i::InstanceType instance_type = map->instance_type();
  CHECK_LE(size, static_cast<uint32_t>(std::numeric_limits<int>::max()));
  ObjectDetailVisitor visitor(this, address, object, static_cast<int>(size),
                              cage_base_, code_cage_base_, instance_type);
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
      std::unique_ptr<FakeHeapObject> fake_value =
          MakeFakeHeapObject(value_heap_object.address());
      if (!fake_value) {
        ReportCorrupt("primitive_wrapper_value_missing",
                      "JSPrimitiveWrapper value was not present in rawheap",
                      "object_body", address, 0,
                      static_cast<uint32_t>(instance_type));
        return heap_object_visitor_tmp.Object_string();
      }
      i::Tagged<i::HeapObject> fake_object = fake_value->fake_object();

      i::Address value_map_address = fake_object->map(cage_base_).address();
      std::unique_ptr<FakeHeapObject> fake_map_ho =
          MakeFakeHeapObject(value_map_address);
      if (!fake_map_ho) {
        ReportCorrupt("primitive_wrapper_value_map_missing",
                      "JSPrimitiveWrapper value map was not present in rawheap",
                      "object_body", address, value_map_address,
                      static_cast<uint32_t>(instance_type));
        return heap_object_visitor_tmp.Object_string();
      }
      i::Tagged<i::Map> fake_map =
          UncheckedCast<i::Map>(fake_map_ho->fake_object());

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
      ReportWarning("primitive_wrapper_value_unexpected_type",
                    "JSPrimitiveWrapper value target is not a supported "
                    "primitive wrapper value type; using Object fallback name",
                    "object_table", address, value_map_address,
                    static_cast<uint32_t>(instance_type),
                    static_cast<uint32_t>(fake_map->visitor_id()));
      return heap_object_visitor_tmp.Object_string();
    }
    ReportWarning("primitive_wrapper_value_unexpected_tag",
                  "JSPrimitiveWrapper value is neither Smi nor a heap object; "
                  "using Object fallback name",
                  "object_table", address, 0,
                  static_cast<uint32_t>(instance_type));
    return heap_object_visitor_tmp.Object_string();
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

bool ObjectTranslator::IsUndefinedOddballOffline(i::Address address) {
  i::InstanceType instance_type;
  if (!GetInstanceType(address, &instance_type) ||
      !i::InstanceTypeChecker::IsOddball(instance_type)) {
    return false;
  }
  std::unique_ptr<FakeHeapObject> fake_object = MakeFakeHeapObject(address);
  if (!fake_object) {
    const RawHeapObjectHead* object_head = FindObjectHead(address);
    ReportCorrupt("undefined_oddball_unreadable",
                  "Oddball payload could not be read from rawheap",
                  "object_body", address, 0, 0, 0,
                  object_head == nullptr ? 0 : object_head->offset_);
    return false;
  }
  i::Tagged<i::Oddball> oddball =
      i::UncheckedCast<i::Oddball>(fake_object->fake_object());
  return oddball->kind() == i::Oddball::kUndefined;
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
  std::unique_ptr<FakeHeapObject> fake_sfi_ho = MakeFakeHeapObject(sfi_addr);
  if (!fake_sfi_ho) {
    ReportCorrupt("js_function_sfi_missing",
                  "JSFunction SharedFunctionInfo was not present in rawheap",
                  "object_body", sfi_addr);
    return "";
  }
  i::Tagged<i::SharedFunctionInfo> fake_sfi =
      i::UncheckedCast<i::SharedFunctionInfo>(fake_sfi_ho->fake_object());

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

  std::unique_ptr<FakeHeapObject> fake_name_ho = MakeFakeHeapObject(name_addr);
  if (!fake_name_ho) {
    ReportCorrupt("shared_function_info_name_missing",
                  "SharedFunctionInfo name object was not present in rawheap",
                  "object_body", name_addr);
    return "";
  }
  i::Tagged<i::HeapObject> fake_name_obj = fake_name_ho->fake_object();

  // Get map to check the instance type
  i::Tagged<i::Map> name_map = fake_name_obj->map(cage_base_);
  std::unique_ptr<FakeHeapObject> fake_map_ho =
      MakeFakeHeapObject(name_map.address());
  if (!fake_map_ho) {
    ReportCorrupt("shared_function_info_name_map_missing",
                  "SharedFunctionInfo name map was not present in rawheap",
                  "object_body", name_addr, name_map.address());
    return "";
  }
  i::Tagged<i::Map> fake_name_map =
      i::UncheckedCast<i::Map>(fake_map_ho->fake_object());

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

bool ObjectTranslator::ExtractReferences(
    i::Tagged<i::HeapObject> obj,  // fake_obj
    i::Tagged<i::Map> map,         // fake_map
    i::Address real_address, i::Address real_map_address) {
  // Keep this dispatch list aligned with V8HeapExplorer::ExtractReferences in
  // src/profiler/heap-snapshot-generator.cc. The goal is the same DevTools
  // graph shape, but with corrupt/missing rawheap input reported gracefully
  // instead of assuming a live, internally consistent isolate.
  i::InstanceType instance_type = map->instance_type();
  if (i::InstanceTypeChecker::IsJSGlobalProxy(instance_type)) {
    if (!ExtractJSGlobalProxyReferences(real_address,
                                        UncheckedCast<i::JSGlobalProxy>(obj),
                                        map, real_map_address)) {
      return false;
    }
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
    if (!ExtractMapReferences(real_address, i::UncheckedCast<i::Map>(obj))) {
      return false;
    }
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
  } else if (i::InstanceTypeChecker::IsCode(instance_type)) {
    ExtractCodeReferences(real_address, i::UncheckedCast<i::Code>(obj));
  } else if (i::InstanceTypeChecker::IsInstructionStream(instance_type)) {
    ExtractInstructionStreamReferences(
        real_address, i::UncheckedCast<i::InstructionStream>(obj));
  } else if (i::InstanceTypeChecker::IsCell(instance_type)) {
    ExtractCellReferences(real_address, i::UncheckedCast<i::Cell>(obj));
  } else if (i::InstanceTypeChecker::IsFeedbackCell(instance_type)) {
    ExtractFeedbackCellReferences(real_address, instance_type,
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
    ExtractRegExpBoilerplateDescriptionReferences(
        i::UncheckedCast<i::RegExpBoilerplateDescription>(obj));
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
  } else if (i::InstanceTypeChecker::IsBytecodeArray(instance_type)) {
    ExtractBytecodeArrayReferences(real_address,
                                   i::UncheckedCast<i::BytecodeArray>(obj));
  } else if (i::InstanceTypeChecker::IsScopeInfo(instance_type)) {
    ExtractScopeInfoReferences(real_address,
                               i::UncheckedCast<i::ScopeInfo>(obj));
  } else if (i::InstanceTypeChecker::IsCppHeapExternalObject(instance_type)) {
    ExtractCppHeapExternalReferences(
        real_address, i::UncheckedCast<i::CppHeapExternalObject>(obj));
#if V8_ENABLE_WEBASSEMBLY
  } else if (i::InstanceTypeChecker::IsWasmStruct(instance_type)) {
    ExtractWasmStructReferences(real_address, i::Cast<i::WasmStruct>(obj), map,
                                real_map_address);
  } else if (i::InstanceTypeChecker::IsWasmArray(instance_type)) {
    if (!ExtractWasmArrayReferences(real_address, i::Cast<i::WasmArray>(obj),
                                    map, real_map_address)) {
      return false;
    }
  } else if (i::InstanceTypeChecker::IsWasmTrustedInstanceData(instance_type)) {
    ExtractWasmTrustedInstanceDataReferences(real_address, obj);
#endif  // V8_ENABLE_WEBASSEMBLY
  }
  return true;
}

bool ObjectTranslator::GetCreationContextOffline(
    i::Address real_address, i::Tagged<i::Map> map, i::Address real_map_address,
    i::Address* native_context_addr) {
  DCHECK_NOT_NULL(native_context_addr);
  *native_context_addr = 0;

  // Offline copy of the source logic in:
  //   src/objects/js-objects-inl.h
  //     JSReceiver::GetCreationContext()
  //       -> map()->map()->native_context_or_null()
  // Native V8 returns only the NativeContext here. Offline translate
  // reconstructs the same value from rawheap bytes and still requires rawheap
  // object-table membership before exposing it as a V8 heap object.
  i::Address meta_map_address = map->map(cage_base_).address();
  std::unique_ptr<FakeHeapObject> fake_meta_map_obj =
      MakeFakeHeapObject(meta_map_address);
  if (!fake_meta_map_obj) {
    ReportCorrupt("global_proxy_meta_map_missing",
                  "JSGlobalProxy meta map was not present in rawheap",
                  "object_body", real_address, real_map_address,
                  static_cast<uint32_t>(map->instance_type()));
    return false;
  }
  i::Tagged<i::Map> fake_meta_map =
      UncheckedCast<i::Map>(fake_meta_map_obj->fake_object());
  i::Tagged<i::Object> maybe_native_context =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, fake_meta_map,
          i::Map::kConstructorOrBackPointerOrNativeContextOffset);
  if (!i::IsHeapObject(maybe_native_context)) {
    return true;
  }
  i::Address native_context_address =
      i::Cast<i::HeapObject>(maybe_native_context).address();
  auto native_context_it = objects_head_.find(native_context_address);
  if (native_context_it == objects_head_.end()) {
    return true;
  }
  i::InstanceType native_context_type;
  if (!GetInstanceType(native_context_address, &native_context_type) ||
      !i::InstanceTypeChecker::IsNativeContext(native_context_type)) {
    ReportWarning("global_proxy_native_context_type_mismatch",
                  "JSGlobalProxy creation context target is not a "
                  "NativeContext; shortcut edges suppressed",
                  "object_body", real_address, real_map_address,
                  static_cast<uint32_t>(map->instance_type()));
    return true;
  }
  *native_context_addr = native_context_address;
  return true;
}

void ObjectTranslator::GetNativeContextGlobalObjectOffline(
    i::Address native_context_addr, i::Address* global_object_addr) {
  DCHECK_NOT_NULL(global_object_addr);
  *global_object_addr = 0;

  // Mirrors the second half of:
  //   src/profiler/heap-snapshot-generator.cc
  //     V8HeapExplorer::ExtractJSGlobalProxyReferences()
  //       -> native_context->global_object()
  // after GetCreationContext() has already recovered the NativeContext.
  auto native_context_it = objects_head_.find(native_context_addr);
  if (native_context_it == objects_head_.end()) {
    return;
  }
  FakeHeapObject fake_native_context(reader_, native_context_it->second.offset_,
                                     native_context_it->second.size_);
  i::Tagged<i::NativeContext> fake_context =
      UncheckedCast<i::NativeContext>(fake_native_context.fake_object());
  i::Tagged<i::Object> global_obj = i::TaggedField<i::Object>::Relaxed_Load(
      cage_base_, fake_context,
      i::Context::OffsetOfElementAt(i::Context::Field::EXTENSION_INDEX));
  if (i::IsHeapObject(global_obj)) {
    i::Address global_obj_addr = i::Cast<i::HeapObject>(global_obj).address();
    i::InstanceType global_obj_type;
    // Treat the recovered slot as a candidate edge target only after rawheap
    // membership and JSGlobalObject type validation confirm it is the dumped V8
    // heap object native snapshot would have used. The caller applies the
    // snapshot-node gate before SetShortcutReference.
    if (objects_head_.count(global_obj_addr) != 0 &&
        GetInstanceType(global_obj_addr, &global_obj_type) &&
        i::InstanceTypeChecker::IsJSGlobalObject(global_obj_type)) {
      *global_object_addr = global_obj_addr;
    }
  }
}

bool ObjectTranslator::ExtractJSGlobalProxyReferences(
    i::Address real_address, i::Tagged<i::JSGlobalProxy> proxy,
    i::Tagged<i::Map> map, i::Address real_map_address) {
  USE(proxy);
  i::Address native_context_addr = 0;
  i::Address global_object_addr = 0;
  if (!GetCreationContextOffline(real_address, map, real_map_address,
                                 &native_context_addr)) {
    return false;
  }
  if (native_context_addr == 0 ||
      generator_->GetNode(native_context_addr) == nullptr) {
    return true;
  }

  generator_->SetShortcutReference(real_address, "native_context",
                                   native_context_addr);
  GetNativeContextGlobalObjectOffline(native_context_addr, &global_object_addr);
  if (global_object_addr != 0 &&
      generator_->GetNode(global_object_addr) != nullptr) {
    generator_->SetShortcutReference(real_address, "global_object",
                                     global_object_addr);
  }
  return true;
}

std::unique_ptr<FakeHeapObject> ObjectTranslator::GetJSObjectOwnDescriptors(
    i::Address real_address, i::Tagged<i::Map> map) {
  i::Address descriptors_address;
  if (!LoadHeapObjectAddress(cage_base_, map,
                             i::Map::kInstanceDescriptorsOffset,
                             &descriptors_address)) {
    return nullptr;
  }

  const RawHeapObjectHead* descriptors_head =
      FindObjectHead(descriptors_address);
  if (descriptors_head == nullptr) {
    ReportWarning("js_object_descriptors_missing",
                  "JSObject descriptor array was not present in rawheap; "
                  "own property edges suppressed",
                  "object_body", real_address, descriptors_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()));
    return nullptr;
  }

  i::InstanceType descriptors_type;
  if (!GetInstanceType(descriptors_address, &descriptors_type) ||
      descriptors_type != i::DESCRIPTOR_ARRAY_TYPE) {
    ReportWarning("js_object_descriptors_wrong_type",
                  "JSObject instance descriptors target is not a "
                  "DescriptorArray; own property edges suppressed",
                  "object_body", real_address, descriptors_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  descriptors_head->offset_);
    return nullptr;
  }

  std::unique_ptr<FakeHeapObject> fake_descriptors =
      MakeFakeHeapObject(descriptors_address);
  if (!fake_descriptors) {
    ReportWarning("js_object_descriptors_unreadable",
                  "JSObject descriptor array could not be read from rawheap; "
                  "own property edges suppressed",
                  "object_body", real_address, descriptors_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  descriptors_head->offset_);
    return nullptr;
  }

  i::Tagged<i::DescriptorArray> descs =
      i::UncheckedCast<i::DescriptorArray>(fake_descriptors->fake_object());
  const int descriptor_count = map->NumberOfOwnDescriptors();
  if (descriptor_count > descs->number_of_descriptors()) {
    ReportWarning("js_object_descriptors_count_invalid",
                  "JSObject own descriptor count is invalid for the dumped "
                  "DescriptorArray; own property edges suppressed",
                  "object_body", real_address, descriptors_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  descriptors_head->offset_);
    return nullptr;
  }

  const int descriptors_end =
      i::DescriptorArray::OffsetOfDescriptorAt(descriptor_count);
  if (descriptors_end < 0 ||
      static_cast<uint32_t>(descriptors_end) > descriptors_head->size_) {
    ReportWarning("js_object_descriptors_out_of_range",
                  "JSObject own descriptors are outside the dumped "
                  "DescriptorArray payload; own property edges suppressed",
                  "object_body", real_address, descriptors_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  descriptors_head->offset_);
    return nullptr;
  }

  return fake_descriptors;
}

std::string ObjectTranslator::GetPropertyNameFromKey(i::Tagged<i::Name> key) {
  i::Address key_address = key.address();
  if (FindObjectHead(key_address) == nullptr) {
    return "";
  }
  i::InstanceType key_type;
  if (!GetInstanceType(key_address, &key_type)) {
    return "";
  }
  if (i::InstanceTypeChecker::IsString(key_type)) {
    return get_string_content(key_address);
  }
  if (i::InstanceTypeChecker::IsSymbol(key_type)) {
    std::unique_ptr<FakeHeapObject> fake_symbol =
        MakeFakeHeapObject(key_address);
    if (!fake_symbol) {
      return "";
    }
    i::Tagged<i::Symbol> symbol =
        i::UncheckedCast<i::Symbol>(fake_symbol->fake_object());
    i::Address description_address;
    if (LoadHeapObjectAddress(cage_base_, symbol,
                              offsetof(i::Symbol, description_),
                              &description_address)) {
      return get_string_content(description_address);
    }
  }
  return "";
}

std::unique_ptr<FakeHeapObject>
ObjectTranslator::GetJSGlobalObjectGlobalDictionary(
    i::Address real_address, i::Tagged<i::Map> map,
    i::Tagged<i::JSGlobalObject> global_object) {
  i::Address dictionary_address;
  if (!LoadHeapObjectAddress(cage_base_, global_object,
                             i::JSObject::kPropertiesOrHashOffset,
                             &dictionary_address)) {
    return nullptr;
  }

  const RawHeapObjectHead* dictionary_head = FindObjectHead(dictionary_address);
  i::InstanceType dictionary_type;
  std::unique_ptr<FakeHeapObject> fake_dictionary =
      MakeFakeHeapObject(dictionary_address);
  if (dictionary_head == nullptr ||
      !GetInstanceType(dictionary_address, &dictionary_type) ||
      dictionary_type != i::GLOBAL_DICTIONARY_TYPE || !fake_dictionary) {
    ReportWarning("js_global_object_dictionary_unavailable",
                  "JSGlobalObject global dictionary was missing, unreadable, "
                  "or not a GlobalDictionary; global property edges suppressed",
                  "object_body", real_address, dictionary_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()));
    return nullptr;
  }

  i::Tagged<i::GlobalDictionary> dictionary =
      i::UncheckedCast<i::GlobalDictionary>(fake_dictionary->fake_object());
  const uint32_t capacity = dictionary->Capacity();
  if (capacity > i::GlobalDictionary::kMaxCapacity) {
    ReportWarning("js_global_object_dictionary_capacity_invalid",
                  "JSGlobalObject global dictionary capacity is invalid for "
                  "the dumped payload; global property edges suppressed",
                  "object_body", real_address, dictionary_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  dictionary_head->offset_);
    return nullptr;
  }

  const int entries_end =
      i::FixedArray::OffsetOfElementAt(i::GlobalDictionary::EntryToIndex(
          i::InternalIndex(static_cast<int>(capacity))));
  if (entries_end < 0 ||
      static_cast<uint32_t>(entries_end) > dictionary_head->size_) {
    ReportWarning("js_global_object_dictionary_out_of_range",
                  "JSGlobalObject global dictionary entries are outside the "
                  "dumped payload; global property edges suppressed",
                  "object_body", real_address, dictionary_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  dictionary_head->offset_);
    return nullptr;
  }

  return fake_dictionary;
}

std::unique_ptr<FakeHeapObject> ObjectTranslator::GetGlobalDictionaryCellAt(
    i::Address real_address, i::Tagged<i::Map> map,
    i::Tagged<i::GlobalDictionary> dictionary, i::InternalIndex entry) {
  i::Tagged<i::Object> key = dictionary->KeyAt(cage_base_, entry);
  if (!i::IsHeapObject(key)) {
    return nullptr;
  }

  i::Address key_address = i::Cast<i::HeapObject>(key).address();
  i::InstanceType cell_type;
  if (FindObjectHead(key_address) == nullptr ||
      !GetInstanceType(key_address, &cell_type) ||
      cell_type != i::PROPERTY_CELL_TYPE) {
    return nullptr;
  }

  // Offline equivalent of GlobalDictionary::CellAt(): the key is the
  // PropertyCell. Avoid returning the live tagged value as dereferenceable.
  std::unique_ptr<FakeHeapObject> fake_cell = MakeFakeHeapObject(key_address);
  if (!fake_cell) {
    ReportWarning("js_global_object_property_cell_unavailable",
                  "JSGlobalObject global dictionary entry is not readable as a "
                  "PropertyCell; global property edge suppressed",
                  "object_body", real_address, key_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()));
    return nullptr;
  }
  return fake_cell;
}

std::unique_ptr<FakeHeapObject>
ObjectTranslator::GetJSObjectSwissNameDictionary(
    i::Address real_address, i::Tagged<i::Map> map,
    i::Tagged<i::JSObject> js_obj) {
  i::Address dictionary_address;
  if (!LoadHeapObjectAddress(cage_base_, js_obj,
                             i::JSObject::kPropertiesOrHashOffset,
                             &dictionary_address)) {
    return nullptr;
  }

  const RawHeapObjectHead* dictionary_head = FindObjectHead(dictionary_address);
  i::InstanceType dictionary_type;
  std::unique_ptr<FakeHeapObject> fake_dictionary =
      MakeFakeHeapObject(dictionary_address);
  if (dictionary_head == nullptr ||
      !GetInstanceType(dictionary_address, &dictionary_type) ||
      dictionary_type != i::SWISS_NAME_DICTIONARY_TYPE || !fake_dictionary) {
    ReportWarning("js_object_swiss_dictionary_unavailable",
                  "JSObject properties were missing, unreadable, or not a "
                  "SwissNameDictionary; slow property edges suppressed",
                  "object_body", real_address, dictionary_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()));
    return nullptr;
  }

  i::Tagged<i::SwissNameDictionary> dictionary =
      i::UncheckedCast<i::SwissNameDictionary>(fake_dictionary->fake_object());
  const int capacity = dictionary->Capacity();
  if (!i::SwissNameDictionary::IsValidCapacity(capacity) ||
      capacity > i::SwissNameDictionary::MaxCapacity()) {
    ReportWarning("js_object_swiss_dictionary_capacity_invalid",
                  "JSObject SwissNameDictionary capacity is invalid for the "
                  "dumped payload; slow property edges suppressed",
                  "object_body", real_address, dictionary_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  dictionary_head->offset_);
    return nullptr;
  }

  const int dictionary_size = i::SwissNameDictionary::SizeFor(capacity);
  if (dictionary_size < 0 ||
      static_cast<uint32_t>(dictionary_size) > dictionary_head->size_) {
    ReportWarning("js_object_swiss_dictionary_out_of_range",
                  "JSObject SwissNameDictionary payload is outside the dumped "
                  "object; slow property edges suppressed",
                  "object_body", real_address, dictionary_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  dictionary_head->offset_);
    return nullptr;
  }

  return fake_dictionary;
}

bool ObjectTranslator::LoadSwissNameDictionaryProperty(
    i::Address real_address, i::Tagged<i::Map> map,
    i::Tagged<i::SwissNameDictionary> dictionary, i::InternalIndex entry,
    std::string* name, bool* name_unresolved, i::Address* value_address,
    i::PropertyKind* kind) {
  DCHECK_NOT_NULL(name);
  DCHECK_NOT_NULL(name_unresolved);
  DCHECK_NOT_NULL(value_address);
  DCHECK_NOT_NULL(kind);
  *name_unresolved = false;

  const int entry_index = entry.as_int();
  const int key_offset =
      i::SwissNameDictionary::DataTableStartOffset() +
      (entry_index * i::SwissNameDictionary::kDataTableEntryCount +
       i::SwissNameDictionary::kDataTableKeyEntryIndex) *
          i::kTaggedSize;
  const int value_offset =
      i::SwissNameDictionary::DataTableStartOffset() +
      (entry_index * i::SwissNameDictionary::kDataTableEntryCount +
       i::SwissNameDictionary::kDataTableValueEntryIndex) *
          i::kTaggedSize;

  // Offline SwissNameDictionary::KeyAt/ValueAt with explicit cage base.
  i::Tagged<i::Object> key = i::TaggedField<i::Tagged<i::Object>>::Relaxed_Load(
      cage_base_, dictionary, key_offset);
  if (!i::IsHeapObject(key)) {
    return false;
  }
  i::Address key_address = i::Cast<i::HeapObject>(key).address();
  i::InstanceType key_type;
  if (FindObjectHead(key_address) == nullptr ||
      !GetInstanceType(key_address, &key_type) ||
      !i::InstanceTypeChecker::IsName(key_type)) {
    return false;
  }

  i::Tagged<i::Object> value =
      i::TaggedField<i::Tagged<i::Object>>::Relaxed_Load(cage_base_, dictionary,
                                                         value_offset);
  if (!i::IsHeapObject(value)) {
    return false;
  }

  *name = GetPropertyNameFromKey(i::UncheckedCast<i::Name>(key));
  if (name->empty()) {
    name->assign("(property #" + std::to_string(entry.as_int()) + ")");
    *name_unresolved = true;
  }
  *value_address = i::Cast<i::HeapObject>(value).address();
  *kind = dictionary->DetailsAt(entry).kind();
  return true;
}

std::unique_ptr<FakeHeapObject> ObjectTranslator::GetJSObjectNameDictionary(
    i::Address real_address, i::Tagged<i::Map> map,
    i::Tagged<i::JSObject> js_obj) {
  i::Address dictionary_address;
  if (!LoadHeapObjectAddress(cage_base_, js_obj,
                             i::JSObject::kPropertiesOrHashOffset,
                             &dictionary_address)) {
    return nullptr;
  }

  const RawHeapObjectHead* dictionary_head = FindObjectHead(dictionary_address);
  i::InstanceType dictionary_type;
  std::unique_ptr<FakeHeapObject> fake_dictionary =
      MakeFakeHeapObject(dictionary_address);
  if (dictionary_head == nullptr ||
      !GetInstanceType(dictionary_address, &dictionary_type) ||
      dictionary_type != i::NAME_DICTIONARY_TYPE || !fake_dictionary) {
    ReportWarning("js_object_name_dictionary_unavailable",
                  "JSObject properties were missing, unreadable, or not a "
                  "NameDictionary; slow property edges suppressed",
                  "object_body", real_address, dictionary_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()));
    return nullptr;
  }

  i::Tagged<i::NameDictionary> dictionary =
      i::UncheckedCast<i::NameDictionary>(fake_dictionary->fake_object());
  const uint32_t capacity = dictionary->Capacity();
  if (capacity > i::NameDictionary::kMaxCapacity) {
    ReportWarning("js_object_name_dictionary_capacity_invalid",
                  "JSObject NameDictionary capacity is invalid for the dumped "
                  "payload; slow property edges suppressed",
                  "object_body", real_address, dictionary_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  dictionary_head->offset_);
    return nullptr;
  }

  const int entries_end =
      i::FixedArray::OffsetOfElementAt(i::NameDictionary::EntryToIndex(
          i::InternalIndex(static_cast<int>(capacity))));
  if (entries_end < 0 ||
      static_cast<uint32_t>(entries_end) > dictionary_head->size_) {
    ReportWarning("js_object_name_dictionary_out_of_range",
                  "JSObject NameDictionary entries are outside the dumped "
                  "payload; slow property edges suppressed",
                  "object_body", real_address, dictionary_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  dictionary_head->offset_);
    return nullptr;
  }

  return fake_dictionary;
}

bool ObjectTranslator::LoadNameDictionaryProperty(
    i::Address real_address, i::Tagged<i::Map> map,
    i::Tagged<i::NameDictionary> dictionary, i::InternalIndex entry,
    std::string* name, bool* name_unresolved, i::Address* value_address,
    i::PropertyKind* kind) {
  DCHECK_NOT_NULL(name);
  DCHECK_NOT_NULL(name_unresolved);
  DCHECK_NOT_NULL(value_address);
  DCHECK_NOT_NULL(kind);
  *name_unresolved = false;

  i::Tagged<i::Object> key = dictionary->KeyAt(cage_base_, entry);
  if (!i::IsHeapObject(key)) {
    return false;
  }
  i::Address key_address = i::Cast<i::HeapObject>(key).address();
  i::InstanceType key_type;
  if (FindObjectHead(key_address) == nullptr ||
      !GetInstanceType(key_address, &key_type) ||
      !i::InstanceTypeChecker::IsName(key_type)) {
    return false;
  }

  i::Tagged<i::Object> value = dictionary->ValueAt(cage_base_, entry);
  if (!i::IsHeapObject(value)) {
    return false;
  }

  *name = GetPropertyNameFromKey(i::UncheckedCast<i::Name>(key));
  if (name->empty()) {
    name->assign("(property #" + std::to_string(entry.as_int()) + ")");
    *name_unresolved = true;
  }
  *value_address = i::Cast<i::HeapObject>(value).address();
  *kind = dictionary->DetailsAt(entry).kind();
  return true;
}

void ObjectTranslator::SetDataOrAccessorPropertyReference(
    i::Address real_address, i::PropertyKind kind,
    const std::string& reference_name, i::Address child_address,
    int field_offset, bool name_is_unresolved) {
  if (kind == i::PropertyKind::kAccessor) {
    ExtractAccessorPairProperty(real_address, reference_name, child_address,
                               field_offset, name_is_unresolved);
    return;
  }

  if (reference_name.empty()) {
    return;
  }

  if (FindObjectHead(child_address) == nullptr ||
      generator_->GetNode(child_address) == nullptr) {
    return;
  }

  generator_->SetPropertyReference(real_address, reference_name, child_address,
                                  field_offset, name_is_unresolved);
}

void ObjectTranslator::ExtractAccessorPairProperty(
    i::Address real_address, const std::string& reference_name,
    i::Address child_address, int field_offset, bool name_is_unresolved) {
  if (reference_name.empty()) {
    return;
  }

  if (FindObjectHead(child_address) == nullptr ||
      generator_->GetNode(child_address) == nullptr) {
    return;
  }
  i::InstanceType child_type;
  if (!GetInstanceType(child_address, &child_type) ||
      child_type != i::ACCESSOR_PAIR_TYPE) {
    return;
  }

  generator_->SetPropertyReference(real_address, reference_name, child_address,
                                  field_offset, name_is_unresolved);

  std::unique_ptr<FakeHeapObject> fake_accessors =
      MakeFakeHeapObject(child_address);
  if (!fake_accessors) {
    return;
  }
  i::Tagged<i::AccessorPair> accessors =
      i::UncheckedCast<i::AccessorPair>(fake_accessors->fake_object());
  // accersors->getter()/setter()
  auto set_accessor_component = [&](const std::string& name, int offset) {
    i::Address component_address;
    if (!LoadHeapObjectAddress(cage_base_, accessors, offset,
                               &component_address)) {
      return;
    }
    i::InstanceType component_type;
    if (!GetInstanceType(component_address, &component_type) ||
        i::InstanceTypeChecker::IsOddball(component_type) ||
        generator_->GetNode(component_address) == nullptr) {
      return;
    }
    generator_->SetPropertyReference(real_address, name, component_address,
                                    -1, name_is_unresolved);
  };

  set_accessor_component("get " + reference_name,
                         offsetof(i::AccessorPair, getter_));
  set_accessor_component("set " + reference_name,
                         offsetof(i::AccessorPair, setter_));
}

bool ObjectTranslator::LoadRawFastPropertyAt(i::Address real_address,
                                             i::Tagged<i::JSObject> js_obj,
                                             i::Tagged<i::Map> map,
                                             i::FieldIndex field_index,
                                             i::Address* value_address) {
  DCHECK_NOT_NULL(value_address);
  auto set_heap_object_address = [&](i::Tagged<i::Object> value) {
    if (!i::IsHeapObject(value)) {
      return false;
    }
    *value_address = i::Cast<i::HeapObject>(value).address();
    return true;
  };

  if (field_index.is_inobject()) {
    const RawHeapObjectHead* object_head = FindObjectHead(real_address);
    if (object_head == nullptr) {
      ReportWarning("js_object_property_source_missing",
                    "JSObject source object was not present in rawheap; own "
                    "property edges suppressed",
                    "object_body", real_address, map.address(),
                    static_cast<uint32_t>(map->instance_type()),
                    static_cast<uint32_t>(map->visitor_id()));
      return false;
    }

    const int offset = field_index.offset();
    if (!TaggedFieldFits(object_head->size_, offset)) {
      ReportWarning("js_object_fast_property_field_out_of_range",
                    "JSObject in-object fast property field is outside "
                    "the dumped object payload",
                    "object_body", real_address, map.address(),
                    static_cast<uint32_t>(map->instance_type()),
                    static_cast<uint32_t>(map->visitor_id()),
                    object_head->offset_);
      return false;
    }
    // Offline rawheap reads immutable dumped bytes, so mirror the normal
    // JSObject::RawFastPropertyAt path instead of the shared-struct SeqCst one.
    i::Tagged<i::Object> value =
        i::TaggedField<i::Tagged<i::Object>>::Relaxed_Load(cage_base_, js_obj,
                                                           offset);
    return set_heap_object_address(value);
  }

  // property_array(cage_base)
  i::Address properties_address;
  if (!LoadHeapObjectAddress(cage_base_, js_obj,
                             i::JSObject::kPropertiesOrHashOffset,
                             &properties_address)) {
    return false;
  }
  const RawHeapObjectHead* properties_head = FindObjectHead(properties_address);
  i::InstanceType properties_type;
  std::unique_ptr<FakeHeapObject> fake_properties =
      MakeFakeHeapObject(properties_address);
  if (properties_head == nullptr ||
      !GetInstanceType(properties_address, &properties_type) ||
      properties_type != i::PROPERTY_ARRAY_TYPE || !fake_properties) {
    ReportWarning("js_object_fast_properties_array_unavailable",
                  "JSObject fast properties array was missing, unreadable, or "
                  "not a PropertyArray; fast property edge suppressed",
                  "object_body", real_address, properties_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()));
    return false;
  }

  i::Tagged<i::PropertyArray> properties =
      i::UncheckedCast<i::PropertyArray>(fake_properties->fake_object());

  const int property_index = field_index.outobject_array_index();
  if (property_index < 0 || property_index >= properties->length()) {
    return false;
  }
  const int offset = i::PropertyArray::OffsetOfElementAt(property_index);
  if (!TaggedFieldFits(properties_head->size_, offset)) {
    ReportWarning("js_object_fast_properties_array_out_of_range",
                  "JSObject fast properties array entry is outside the "
                  "dumped payload",
                  "object_body", real_address, properties_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  properties_head->offset_);
    return false;
  }
  // PropertyArray::get
  i::Tagged<i::Object> value =
      i::TaggedField<i::Tagged<i::Object>>::Relaxed_Load(cage_base_, properties,
                                                         offset);
  return set_heap_object_address(value);
}

void ObjectTranslator::ExtractPropertyReferences(i::Address real_address,
                                                 i::Tagged<i::JSObject> js_obj,
                                                 i::Tagged<i::Map> map) {
  if (!map->is_dictionary_map()) {  // same as HasFastProperties
    // Fast properties: mirror V8's own descriptor walk.
    std::unique_ptr<FakeHeapObject> fake_descriptors =
        GetJSObjectOwnDescriptors(real_address, map);
    if (!fake_descriptors) {
      return;
    }

    i::Tagged<i::DescriptorArray> descs =
        i::UncheckedCast<i::DescriptorArray>(fake_descriptors->fake_object());

    for (i::InternalIndex i : map->IterateOwnDescriptors()) {
      i::PropertyDetails details = descs->GetDetails(i);
      i::Tagged<i::Name> key = descs->GetKey(cage_base_, i);
      std::string name = GetPropertyNameFromKey(key);
      bool name_unresolved = false;
      if (name.empty()) {
        // Current rawheap strips string payloads for data-only string objects.
        // Keep a property edge for retention analysis even when the exact
        // JavaScript property name cannot be recovered offline.
        name = "(property #" + std::to_string(i.as_int()) + ")";
        name_unresolved = true;
      }

      switch (details.location()) {
        case i::PropertyLocation::kField: {
          // Field location: mirror RawFastPropertyAt, but return an address.
          if (details.representation().IsSmi() ||
              details.representation().IsDouble()) {
            break;
          }
          i::FieldIndex field_index = i::FieldIndex::ForDetails(map, details);
          i::Address value_address;
          if (!LoadRawFastPropertyAt(real_address, js_obj, map, field_index,
                                     &value_address)) {
            break;
          }
          int field_offset =
              field_index.is_inobject() ? field_index.offset() : -1;
          SetDataOrAccessorPropertyReference(real_address, details.kind(), name,
                                            value_address, field_offset,
                                            name_unresolved);
          break;
        }
        case i::PropertyLocation::kDescriptor: {
          // Descriptor location: value is stored in the DescriptorArray.
          i::Tagged<i::Object> value = descs->GetStrongValue(cage_base_, i);
          if (!i::IsHeapObject(value)) {
            break;
          }
          i::Address value_address = i::Cast<i::HeapObject>(value).address();
          SetDataOrAccessorPropertyReference(real_address, details.kind(), name,
                                            value_address, -1, name_unresolved);
          break;
        }
      }
    }
  } else if (i::InstanceTypeChecker::IsJSGlobalObject(map->instance_type())) {
    // We assume that global objects can only have slow properties.
    i::Tagged<i::JSGlobalObject> global_object =
        i::UncheckedCast<i::JSGlobalObject>(js_obj);
    std::unique_ptr<FakeHeapObject> fake_dictionary =
        GetJSGlobalObjectGlobalDictionary(real_address, map, global_object);
    if (!fake_dictionary) {
      return;
    }

    i::Tagged<i::GlobalDictionary> dictionary =
        i::UncheckedCast<i::GlobalDictionary>(fake_dictionary->fake_object());
    for (i::InternalIndex i : dictionary->IterateEntries()) {
      // GlobalDictionary stores PropertyCell entries in its key slots.
      std::unique_ptr<FakeHeapObject> fake_cell =
          GetGlobalDictionaryCellAt(real_address, map, dictionary, i);
      if (!fake_cell) {
        continue;
      }
      i::Tagged<i::PropertyCell> cell =
          i::UncheckedCast<i::PropertyCell>(fake_cell->fake_object());
      std::string name = GetPropertyNameFromKey(cell->name(cage_base_));
      bool name_unresolved = false;
      if (name.empty()) {
        name = "(global property #" + std::to_string(i.as_int()) + ")";
        name_unresolved = true;
      }

      i::Tagged<i::Object> value = cell->value(cage_base_);
      if (!i::IsHeapObject(value)) {
        continue;
      }
      i::Address value_address = i::Cast<i::HeapObject>(value).address();
      i::PropertyDetails details = cell->property_details();
      SetDataOrAccessorPropertyReference(real_address, details.kind(), name,
                                         value_address, -1, name_unresolved);
    }
    return;
  } else if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    std::unique_ptr<FakeHeapObject> fake_dictionary =
        GetJSObjectSwissNameDictionary(real_address, map, js_obj);
    if (!fake_dictionary) {
      return;
    }

    i::Tagged<i::SwissNameDictionary> dictionary =
        i::UncheckedCast<i::SwissNameDictionary>(
            fake_dictionary->fake_object());
    // Avoid SwissNameDictionary::IterateEntries(); it creates Handles.
    for (int entry = 0; entry < dictionary->Capacity(); ++entry) {
      std::string name;
      i::Address value_address;
      i::PropertyKind kind;
      bool name_unresolved = false;
      if (!LoadSwissNameDictionaryProperty(real_address, map, dictionary,
                                          i::InternalIndex(entry), &name,
                                          &name_unresolved, &value_address,
                                          &kind)) {
        continue;
      }
      SetDataOrAccessorPropertyReference(real_address, kind, name,
                                         value_address, -1, name_unresolved);
    }
    return;
  } else {
    std::unique_ptr<FakeHeapObject> fake_dictionary =
        GetJSObjectNameDictionary(real_address, map, js_obj);
    if (!fake_dictionary) {
      return;
    }

    i::Tagged<i::NameDictionary> dictionary =
        i::UncheckedCast<i::NameDictionary>(fake_dictionary->fake_object());
    // NameDictionary::IterateEntries is a plain HashTable range.
    for (i::InternalIndex i : dictionary->IterateEntries()) {
      std::string name;
      i::Address value_address;
      i::PropertyKind kind;
      bool name_unresolved = false;
      if (!LoadNameDictionaryProperty(real_address, map, dictionary, i, &name,
                                     &name_unresolved, &value_address, &kind)) {
        continue;
      }
      SetDataOrAccessorPropertyReference(real_address, kind, name,
                                         value_address, -1, name_unresolved);
    }
    return;
  }
}

std::unique_ptr<FakeHeapObject> ObjectTranslator::GetJSObjectElements(
    i::Address real_address, i::Tagged<i::JSObject> js_obj,
    i::Tagged<i::Map> map) {
  // Source: V8HeapExplorer::ExtractElementReferences -> js_obj->elements().
  // Offline equivalent of js_obj->elements() in ExtractElementReferences.
  auto report_elements_unavailable = [&](const char* message,
                                         uint32_t rawheap_offset) {
    ReportCorrupt("js_object_elements_unavailable", message, "object_body",
                  real_address, map.address(),
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()), rawheap_offset);
  };

  const RawHeapObjectHead* object_head = FindObjectHead(real_address);
  if (object_head == nullptr ||
      !TaggedFieldFits(object_head->size_, i::JSObject::kElementsOffset)) {
    report_elements_unavailable(
        "JSObject elements slot is unavailable in the dumped object payload",
        object_head == nullptr ? 0 : object_head->offset_);
    return nullptr;
  }

  i::Tagged<i::Object> elements_value =
      i::TaggedField<i::Tagged<i::Object>>::load(cage_base_, js_obj,
                                                 i::JSObject::kElementsOffset);
  if (!i::IsHeapObject(elements_value)) {
    return nullptr;
  }

  const i::Address elements_address =
      i::Cast<i::HeapObject>(elements_value).address();
  const RawHeapObjectHead* elements_head = FindObjectHead(elements_address);
  if (elements_head == nullptr) {
    report_elements_unavailable(
        "JSObject elements backing store is unavailable in rawheap",
        object_head->offset_);
    return nullptr;
  }

  std::unique_ptr<FakeHeapObject> fake_elements =
      MakeFakeHeapObject(elements_address);
  if (!fake_elements) {
    report_elements_unavailable(
        "JSObject elements backing store could not be read from rawheap",
        elements_head->offset_);
    return nullptr;
  }

  constexpr int kFixedArrayHeaderSize = i::FixedArray::OffsetOfElementAt(0);
  if (elements_head->size_ < static_cast<uint32_t>(kFixedArrayHeaderSize)) {
    report_elements_unavailable(
        "JSObject elements backing store header is outside the dumped object "
        "payload",
        elements_head->offset_);
    return nullptr;
  }
  return fake_elements;
}

std::unique_ptr<FakeHeapObject>
ObjectTranslator::GetJSObjectNumberDictionaryElements(
    i::Address real_address, i::Tagged<i::JSObject> js_obj,
    i::Tagged<i::Map> map) {
  // Source: V8HeapExplorer::ExtractElementReferences ->
  // js_obj->element_dictionary().
  i::Address dictionary_address;
  if (!LoadHeapObjectAddress(cage_base_, js_obj, i::JSObject::kElementsOffset,
                             &dictionary_address)) {
    return nullptr;
  }

  const RawHeapObjectHead* dictionary_head = FindObjectHead(dictionary_address);
  i::InstanceType dictionary_type;
  std::unique_ptr<FakeHeapObject> fake_dictionary =
      MakeFakeHeapObject(dictionary_address);
  if (dictionary_head == nullptr ||
      !GetInstanceType(dictionary_address, &dictionary_type) ||
      dictionary_type != i::NUMBER_DICTIONARY_TYPE || !fake_dictionary) {
    ReportWarning("js_object_number_dictionary_unavailable",
                  "JSObject elements were missing, unreadable, or not a "
                  "NumberDictionary; dictionary element edges suppressed",
                  "object_body", real_address, dictionary_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()));
    return nullptr;
  }

  constexpr int kFixedArrayHeaderSize = i::FixedArray::OffsetOfElementAt(0);
  if (dictionary_head->size_ < static_cast<uint32_t>(kFixedArrayHeaderSize)) {
    ReportWarning("js_object_number_dictionary_header_out_of_range",
                  "JSObject NumberDictionary header is outside the dumped "
                  "payload; dictionary element edges suppressed",
                  "object_body", real_address, dictionary_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  dictionary_head->offset_);
    return nullptr;
  }

  i::Tagged<i::NumberDictionary> dictionary =
      i::UncheckedCast<i::NumberDictionary>(fake_dictionary->fake_object());
  const uint32_t capacity = dictionary->Capacity();
  if (capacity > i::NumberDictionary::kMaxCapacity ||
      capacity > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
    ReportWarning("js_object_number_dictionary_capacity_invalid",
                  "JSObject NumberDictionary capacity is invalid for the "
                  "dumped payload; dictionary element edges suppressed",
                  "object_body", real_address, dictionary_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  dictionary_head->offset_);
    return nullptr;
  }

  const int entries_end =
      i::FixedArray::OffsetOfElementAt(i::NumberDictionary::EntryToIndex(
          i::InternalIndex(static_cast<int>(capacity))));
  if (entries_end < 0 ||
      static_cast<uint32_t>(entries_end) > dictionary_head->size_) {
    ReportWarning("js_object_number_dictionary_out_of_range",
                  "JSObject NumberDictionary entries are outside the dumped "
                  "payload; dictionary element edges suppressed",
                  "object_body", real_address, dictionary_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  dictionary_head->offset_);
    return nullptr;
  }

  return fake_dictionary;
}

bool ObjectTranslator::LoadObjectElementsLength(
    i::Address real_address, i::Tagged<i::JSObject> js_obj,
    i::Tagged<i::Map> map, i::Tagged<i::FixedArray> elements, int* length) {
  DCHECK_NOT_NULL(length);
  if (!i::InstanceTypeChecker::IsJSArray(map->instance_type())) {
    *length = elements->length();
    return true;
  }

  // Mirrors IsJSArray(js_obj) ? Smi::ToInt(Cast<JSArray>(js_obj)->length())
  // in V8HeapExplorer::ExtractElementReferences, but reads the length slot
  // from the dumped JSArray payload instead of dereferencing the live object.
  const RawHeapObjectHead* object_head = FindObjectHead(real_address);
  if (object_head == nullptr ||
      !TaggedFieldFits(object_head->size_, i::JSArray::kLengthOffset)) {
    ReportCorrupt("js_array_length_unavailable",
                  "JSArray length slot is unavailable in the dumped object "
                  "payload; element edges suppressed",
                  "object_body", real_address, map.address(),
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  object_head == nullptr ? 0 : object_head->offset_);
    return false;
  }

  i::Tagged<i::Object> length_value =
      i::TaggedField<i::Tagged<i::Object>>::load(cage_base_, js_obj,
                                                 i::JSArray::kLengthOffset);
  if (!i::IsSmi(length_value)) {
    ReportCorrupt("js_array_length_not_smi",
                  "JSArray length is not a Smi in the dumped object payload; "
                  "element edges suppressed",
                  "object_body", real_address, map.address(),
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  object_head->offset_);
    return false;
  }

  *length = i::Smi::ToInt(length_value);
  return true;
}

bool ObjectTranslator::LoadNotHoleElement(i::Tagged<i::FixedArray> elements,
                                          int index, i::Address* child) {
  // Matches FixedArray::get(index) plus the IsTheHole skip.
  DCHECK_NOT_NULL(child);
  if (index < 0 || index >= elements->length()) {
    return false;
  }
  const int offset = i::FixedArray::OffsetOfElementAt(index);
  i::Tagged<i::Object> value =
      i::TaggedField<i::Tagged<i::Object>>::load(cage_base_, elements, offset);
  if (!i::IsHeapObject(value)) {
    return false;
  }
  *child = i::Cast<i::HeapObject>(value).address();
  i::InstanceType child_type;
  return FindObjectHead(*child) != nullptr &&
         GetInstanceType(*child, &child_type) && child_type != i::HOLE_TYPE &&
         generator_->GetNode(*child) != nullptr;
}

bool ObjectTranslator::LoadNumberDictionaryElement(
    i::Address real_address, i::Tagged<i::Map> map,
    i::Tagged<i::NumberDictionary> dictionary, i::InternalIndex entry,
    uint32_t* index, i::Address* child) {
  // Source: V8HeapExplorer::ExtractElementReferences dictionary loop:
  // KeyAt/NumberValue/ValueAt.
  // Matches KeyAt/IsKey/NumberValue/ValueAt for NumberDictionary entries.
  DCHECK_NOT_NULL(index);
  DCHECK_NOT_NULL(child);
  const int entry_index = i::NumberDictionary::EntryToIndex(entry);
  i::Tagged<i::Object> key = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, dictionary,
      i::FixedArray::OffsetOfElementAt(entry_index +
                                       i::NumberDictionary::kEntryKeyIndex));
  if (i::IsSmi(key)) {
    const int smi_index = i::Smi::ToInt(key);
    if (smi_index < 0) {
      return false;
    }
    *index = static_cast<uint32_t>(smi_index);
  } else {
    if (!i::IsHeapObject(key)) {
      return false;
    }
    const i::Address key_address = i::Cast<i::HeapObject>(key).address();
    i::InstanceType key_type;
    if (FindObjectHead(key_address) == nullptr ||
        !GetInstanceType(key_address, &key_type) ||
        key_type != i::HEAP_NUMBER_TYPE) {
      return false;
    }

    std::unique_ptr<FakeHeapObject> fake_key = MakeFakeHeapObject(key_address);
    if (!fake_key) {
      ReportWarning("js_object_dictionary_element_key_unavailable",
                    "JSObject dictionary element key could not be read as a "
                    "HeapNumber; element edge suppressed",
                    "object_body", real_address, key_address,
                    static_cast<uint32_t>(map->instance_type()),
                    static_cast<uint32_t>(map->visitor_id()));
      return false;
    }

    i::Tagged<i::HeapNumber> heap_number =
        i::UncheckedCast<i::HeapNumber>(fake_key->fake_object());
    const double number = heap_number->value();
    if (!std::isfinite(number) || number < 0 ||
        number > static_cast<double>(std::numeric_limits<uint32_t>::max())) {
      return false;
    }
    *index = static_cast<uint32_t>(number);
  }

  i::Tagged<i::Object> value = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, dictionary,
      i::FixedArray::OffsetOfElementAt(entry_index +
                                       i::NumberDictionary::kEntryValueIndex));
  if (!i::IsHeapObject(value)) {
    return false;
  }
  *child = i::Cast<i::HeapObject>(value).address();
  i::InstanceType child_type;
  return FindObjectHead(*child) != nullptr &&
         GetInstanceType(*child, &child_type) && child_type != i::HOLE_TYPE &&
         generator_->GetNode(*child) != nullptr;
}

void ObjectTranslator::ExtractElementReferences(i::Address real_address,
                                                i::Tagged<i::JSObject> js_obj,
                                                i::Tagged<i::Map> map) {
  // Same branch order and loop shape as
  // V8HeapExplorer::ExtractElementReferences.
  const i::ElementsKind elements_kind = map->elements_kind();
  if (i::IsObjectElementsKind(elements_kind)) {
    // Mirrors the HasObjectElements branch in heap-snapshot-generator.cc.
    std::unique_ptr<FakeHeapObject> fake_elements =
        GetJSObjectElements(real_address, js_obj, map);

    if (!fake_elements) {
      return;
    }
    i::Tagged<i::FixedArray> elements =
        i::UncheckedCast<i::FixedArray>(fake_elements->fake_object());

    int length;
    if (!LoadObjectElementsLength(real_address, js_obj, map, elements,
                                  &length)) {
      return;
    }

    for (int index = 0; index < length; ++index) {
      i::Address child;
      if (!LoadNotHoleElement(elements, index, &child)) {
        continue;
      }
      generator_->SetElementReference(real_address, index, child);
    }
  } else if (i::IsDictionaryElementsKind(elements_kind)) {
    // Mirrors the HasDictionaryElements branch in heap-snapshot-generator.cc.
    std::unique_ptr<FakeHeapObject> fake_elements =
        GetJSObjectNumberDictionaryElements(real_address, js_obj, map);
    if (!fake_elements) {
      return;
    }
    i::Tagged<i::NumberDictionary> dictionary =
        i::UncheckedCast<i::NumberDictionary>(fake_elements->fake_object());

    for (i::InternalIndex entry : dictionary->IterateEntries()) {
      uint32_t index;
      i::Address child;
      if (!LoadNumberDictionaryElement(real_address, map, dictionary, entry,
                                       &index, &child)) {
        continue;
      }
      generator_->SetElementReference(real_address, index, child);
    }
  }
}

bool ObjectTranslator::LoadEmbedderField(i::Address real_address,
                                         i::Tagged<i::JSObject> js_obj,
                                         i::Tagged<i::Map> map, int index,
                                         i::Address* child, int* field_offset) {
  DCHECK_NOT_NULL(child);
  DCHECK_NOT_NULL(field_offset);
  *child = 0;
  *field_offset = -1;

  // Offline equivalent of JSObject::GetEmbedderField(i) and
  // JSObject::GetEmbedderFieldOffset(i). The native accessor reads the tagged
  // payload inside the EmbedderDataSlot while the edge field offset remains the
  // start of the embedder slot.
  const RawHeapObjectHead* object_head = FindObjectHead(real_address);
  auto report_out_of_range = [&]() {
    is_valid_ = false;
    ReportCorrupt("js_object_embedder_field_out_of_range",
                  "JSObject embedder field tagged payload is outside the "
                  "dumped object payload",
                  "object_body", real_address, map.address(),
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  object_head == nullptr ? 0 : object_head->offset_);
  };
  if (object_head == nullptr) {
    report_out_of_range();
    return false;
  }

  const int embedder_fields_start =
      i::JSObject::GetEmbedderFieldsStartOffset(map);
  if (embedder_fields_start < 0) {
    report_out_of_range();
    return false;
  }

  const int64_t slot_offset =
      static_cast<int64_t>(embedder_fields_start) +
      static_cast<int64_t>(i::kEmbedderDataSlotSize) * index;
  const int64_t tagged_offset =
      slot_offset + i::EmbedderDataSlot::kTaggedPayloadOffset;
  if (slot_offset > std::numeric_limits<int>::max() ||
      tagged_offset > std::numeric_limits<int>::max() ||
      !TaggedFieldFits(object_head->size_, static_cast<int>(tagged_offset))) {
    report_out_of_range();
    return false;
  }

  *field_offset = static_cast<int>(slot_offset);
  i::Tagged<i::Object> value = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, js_obj, static_cast<int>(tagged_offset));
  if (!i::IsHeapObject(value)) {
    return false;
  }
  *child = i::Cast<i::HeapObject>(value).address();
  if (generator_->GetNode(*child) == nullptr) {
    ReportWarning("js_object_embedder_field_target_node_missing",
                  "JSObject embedder field target has no snapshot node; "
                  "internal edge suppressed",
                  "object_body", real_address, *child,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  object_head->offset_);
    return false;
  }
  return true;
}

void ObjectTranslator::ExtractInternalReferences(i::Address real_address,
                                                 i::Tagged<i::JSObject> js_obj,
                                                 i::Tagged<i::Map> map) {
  // Same loop shape as V8HeapExplorer::ExtractInternalReferences.
  int length = i::JSObject::GetEmbedderFieldCount(map);
  for (int i = 0; i < length; ++i) {
    i::Address child;
    int field_offset;
    if (!LoadEmbedderField(real_address, js_obj, map, i, &child,
                           &field_offset)) {
      if (!is_valid_) {
        return;
      }
      continue;
    }
    generator_->SetInternalReference(real_address, std::to_string(i), child,
                                     field_offset);
  }
}

bool ObjectTranslator::LoadJSObjectPrototype(i::Address real_address,
                                             i::Tagged<i::JSObject> js_obj,
                                             i::Tagged<i::Map> map,
                                             i::Address* prototype_address) {
  DCHECK_NOT_NULL(prototype_address);
  *prototype_address = 0;

  i::Address real_map_address = js_obj->map(cage_base_).address();
  const RawHeapObjectHead* map_head = FindObjectHead(real_map_address);
  if (map_head == nullptr ||
      !TaggedFieldFits(map_head->size_, i::Map::kPrototypeOffset)) {
    ReportCorrupt("js_object_map_prototype_unavailable",
                  "JSObject map or prototype slot is unavailable in rawheap",
                  "object_body", real_address, real_map_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  map_head == nullptr ? 0 : map_head->offset_);
    return false;
  }

  // Mirrors PrototypeIterator(isolate, js_obj).GetCurrent(): the default
  // iterator starts at the direct prototype, implemented as map->prototype().
  i::Tagged<i::Object> prototype = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, map, i::Map::kPrototypeOffset);
  if (!i::IsHeapObject(prototype)) {
    return false;
  }

  *prototype_address = i::Cast<i::HeapObject>(prototype).address();
  i::InstanceType prototype_type;
  if (FindObjectHead(*prototype_address) == nullptr ||
      !GetInstanceType(*prototype_address, &prototype_type) ||
      !i::InstanceTypeChecker::IsJSReceiver(prototype_type)) {
    return false;
  }

  if (generator_->GetNode(*prototype_address) == nullptr) {
    ReportWarning("js_object_prototype_node_missing",
                  "JSObject prototype target has no snapshot node; __proto__ "
                  "edge suppressed",
                  "object_body", real_address, *prototype_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()), map_head->offset_);
    return false;
  }
  return true;
}

bool ObjectTranslator::LoadJSFunctionPrototypeOrInitialMapIfNotHole(
    i::Address real_address, i::Tagged<i::Map> map,
    i::Tagged<i::JSFunction> js_fun, i::Address* proto_or_map_address,
    i::InstanceType* proto_or_map_type) {
  DCHECK_NOT_NULL(proto_or_map_address);
  DCHECK_NOT_NULL(proto_or_map_type);
  *proto_or_map_address = 0;
  *proto_or_map_type = i::ODDBALL_TYPE;

  const RawHeapObjectHead* object_head = FindObjectHead(real_address);
  if (object_head == nullptr ||
      !TaggedFieldFits(object_head->size_,
                       i::JSFunction::kPrototypeOrInitialMapOffset)) {
    ReportCorrupt("js_function_prototype_or_initial_map_unavailable",
                  "JSFunction prototype_or_initial_map slot is unavailable "
                  "in rawheap",
                  "object_body", real_address, map.address(),
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  object_head == nullptr ? 0 : object_head->offset_);
    return false;
  }

  // Mirrors heap-snapshot-generator.cc's
  // `if (!IsTheHole(js_fun->prototype_or_initial_map(kAcquireLoad)))`: first
  // load the Torque-generated prototype_or_initial_map(cage_base) slot from
  // gen/torque-generated/src/objects/js-function-tq-inl.inc. Native emits a
  // "prototype" property edge for any non-Map heap object here, including
  // non-JSReceiver values stored by functions with non-instance prototypes.
  i::Tagged<i::Object> proto_or_map =
      i::TaggedField<i::Tagged<i::Object>>::load(
          cage_base_, js_fun, i::JSFunction::kPrototypeOrInitialMapOffset);
  if (!i::IsHeapObject(proto_or_map)) {
    return false;
  }

  *proto_or_map_address = i::Cast<i::HeapObject>(proto_or_map).address();
  if (FindObjectHead(*proto_or_map_address) == nullptr ||
      !GetInstanceType(*proto_or_map_address, proto_or_map_type)) {
    return false;
  }
  return *proto_or_map_type != i::HOLE_TYPE;
}

bool ObjectTranslator::LoadJSFunctionPrototype(i::Address real_address,
                                               i::Tagged<i::Map> map,
                                               i::Address initial_map_address,
                                               i::Address* prototype_address) {
  DCHECK_NOT_NULL(prototype_address);
  *prototype_address = 0;

  // Mirrors JSFunction::prototype() in js-function-inl.h: if the function map
  // has a non-instance prototype, Map::GetNonInstancePrototype reads it from
  // the constructor Tuple2 instead of the initial map.
  if (map->has_non_instance_prototype()) {
    const RawHeapObjectHead* map_head = FindObjectHead(map.address());
    if (map_head == nullptr ||
        !TaggedFieldFits(
            map_head->size_,
            i::Map::kConstructorOrBackPointerOrNativeContextOffset)) {
      ReportCorrupt("js_function_map_constructor_unavailable",
                    "JSFunction map constructor slot is unavailable in rawheap",
                    "object_body", real_address, map.address(),
                    static_cast<uint32_t>(map->instance_type()),
                    static_cast<uint32_t>(map->visitor_id()),
                    map_head == nullptr ? 0 : map_head->offset_);
      return false;
    }

    i::Address tuple_address;
    if (!LoadHeapObjectAddress(
            cage_base_, map,
            i::Map::kConstructorOrBackPointerOrNativeContextOffset,
            &tuple_address)) {
      return false;
    }

    const RawHeapObjectHead* tuple_head = FindObjectHead(tuple_address);
    i::InstanceType tuple_type;
    constexpr int kTuple2Value2Offset =
        i::HeapObject::kHeaderSize + i::kTaggedSize;
    if (tuple_head == nullptr || !GetInstanceType(tuple_address, &tuple_type) ||
        tuple_type != i::TUPLE2_TYPE ||
        !TaggedFieldFits(tuple_head->size_, kTuple2Value2Offset)) {
      ReportCorrupt("js_function_non_instance_prototype_tuple_unavailable",
                    "JSFunction non-instance prototype Tuple2 is unavailable "
                    "in rawheap",
                    "object_body", real_address, tuple_address,
                    static_cast<uint32_t>(map->instance_type()),
                    static_cast<uint32_t>(map->visitor_id()),
                    tuple_head == nullptr ? 0 : tuple_head->offset_);
      return false;
    }

    std::unique_ptr<FakeHeapObject> fake_tuple =
        MakeFakeHeapObject(tuple_address);
    if (!fake_tuple) {
      ReportCorrupt("js_function_non_instance_prototype_tuple_unreadable",
                    "JSFunction non-instance prototype Tuple2 could not be "
                    "read from rawheap",
                    "object_body", real_address, tuple_address,
                    static_cast<uint32_t>(map->instance_type()),
                    static_cast<uint32_t>(map->visitor_id()),
                    tuple_head->offset_);
      return false;
    }

    i::Tagged<i::Object> prototype = i::TaggedField<i::Tagged<i::Object>>::load(
        cage_base_, fake_tuple->fake_object(), kTuple2Value2Offset);
    if (!i::IsHeapObject(prototype)) {
      return false;
    }

    *prototype_address = i::Cast<i::HeapObject>(prototype).address();
    return FindObjectHead(*prototype_address) != nullptr;
  }

  const RawHeapObjectHead* initial_map_head =
      FindObjectHead(initial_map_address);
  if (initial_map_head == nullptr ||
      !TaggedFieldFits(initial_map_head->size_, i::Map::kPrototypeOffset)) {
    ReportCorrupt("js_function_initial_map_prototype_unavailable",
                  "JSFunction initial map prototype slot is unavailable "
                  "in rawheap",
                  "object_body", real_address, initial_map_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  initial_map_head == nullptr ? 0 : initial_map_head->offset_);
    return false;
  }

  std::unique_ptr<FakeHeapObject> fake_initial_map =
      MakeFakeHeapObject(initial_map_address);
  if (!fake_initial_map) {
    ReportCorrupt("js_function_initial_map_unreadable",
                  "JSFunction initial map could not be read from rawheap",
                  "object_body", real_address, initial_map_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  initial_map_head->offset_);
    return false;
  }

  // Mirrors JSFunction::instance_prototype(): for the normal initial-map case,
  // the prototype value is stored on the initial map. Native
  // ExtractJSObjectReferences emits js_fun->prototype() without re-checking
  // JSReceiver-ness, so offline translation only proves rawheap membership.
  i::Tagged<i::Map> initial_map =
      i::UncheckedCast<i::Map>(fake_initial_map->fake_object());
  i::Tagged<i::Object> prototype = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, initial_map, i::Map::kPrototypeOffset);
  if (!i::IsHeapObject(prototype)) {
    return false;
  }

  *prototype_address = i::Cast<i::HeapObject>(prototype).address();
  return FindObjectHead(*prototype_address) != nullptr;
}

bool ObjectTranslator::LoadJSBoundFunctionReference(
    i::Address real_address, i::Tagged<i::Map> map,
    i::Tagged<i::JSBoundFunction> bound_fun, int offset,
    i::Address* target_address) {
  DCHECK_NOT_NULL(target_address);
  *target_address = 0;

  if (!LoadHeapObjectAddress(cage_base_, bound_fun, offset, target_address)) {
    return false;
  }
  // These are tagged JSBoundFunction fields, not native pointers. Prove
  // rawheap membership before the snapshot-node gate.
  if (FindObjectHead(*target_address) == nullptr) {
    return false;
  }
  if (generator_->GetNode(*target_address) == nullptr) {
    ReportWarning("js_bound_function_target_node_missing",
                  "JSBoundFunction field target has no snapshot node; "
                  "internal edge suppressed",
                  "object_body", real_address, *target_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()));
    return false;
  }
  return true;
}

std::unique_ptr<FakeHeapObject>
ObjectTranslator::GetJSBoundFunctionBoundArguments(
    i::Address real_address, i::Tagged<i::Map> map,
    i::Tagged<i::JSBoundFunction> bound_fun) {
  i::Address bound_arguments_address;
  if (!LoadHeapObjectAddress(cage_base_, bound_fun,
                             i::JSBoundFunction::kBoundArgumentsOffset,
                             &bound_arguments_address)) {
    return nullptr;
  }

  // Mirrors JSBoundFunction::bound_arguments(): this slot is the FixedArray
  // later iterated by heap-snapshot-generator.cc as bound_argument_%d edges.
  // `bound_arguments` is itself a tagged HeapObject slot, not a native
  // pointer. Classify it from dumped rawheap bytes before any snapshot-node
  // lookup, then use the FixedArray payload only after range checks.
  const RawHeapObjectHead* bound_arguments_head =
      FindObjectHead(bound_arguments_address);
  if (bound_arguments_head == nullptr) {
    ReportCorrupt("js_bound_function_bound_arguments_missing",
                  "JSBoundFunction bound arguments object was not present "
                  "in rawheap",
                  "object_body", real_address, bound_arguments_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()));
    return nullptr;
  }

  i::InstanceType bound_arguments_type;
  if (!GetInstanceType(bound_arguments_address, &bound_arguments_type) ||
      bound_arguments_type != i::FIXED_ARRAY_TYPE) {
    ReportCorrupt("js_bound_function_bound_arguments_wrong_type",
                  "JSBoundFunction bound arguments object is not a FixedArray",
                  "object_body", real_address, bound_arguments_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()));
    return nullptr;
  }

  std::unique_ptr<FakeHeapObject> fake_bound_arguments =
      MakeFakeHeapObject(bound_arguments_address);
  if (!fake_bound_arguments) {
    ReportCorrupt("js_bound_function_bound_arguments_unreadable",
                  "JSBoundFunction bound arguments object could not be read "
                  "from rawheap",
                  "object_body", real_address, bound_arguments_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  bound_arguments_head->offset_);
    return nullptr;
  }

  constexpr int kFixedArrayHeaderSize = i::FixedArray::OffsetOfElementAt(0);
  if (bound_arguments_head->size_ <
      static_cast<uint32_t>(kFixedArrayHeaderSize)) {
    ReportCorrupt("js_bound_function_bound_arguments_header_out_of_range",
                  "JSBoundFunction bound arguments FixedArray header is "
                  "outside the dumped object payload",
                  "object_body", real_address, bound_arguments_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  bound_arguments_head->offset_);
    return nullptr;
  }

  i::Tagged<i::FixedArray> bound_arguments =
      i::UncheckedCast<i::FixedArray>(fake_bound_arguments->fake_object());
  const int length = bound_arguments->length();
  if (length < 0 ||
      length > (std::numeric_limits<int>::max() - kFixedArrayHeaderSize) /
                   i::kTaggedSize) {
    ReportCorrupt("js_bound_function_bound_arguments_length_invalid",
                  "JSBoundFunction bound arguments FixedArray length is "
                  "invalid",
                  "object_body", real_address, bound_arguments_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  bound_arguments_head->offset_);
    return nullptr;
  }

  const int elements_end = i::FixedArray::OffsetOfElementAt(length);
  if (elements_end < 0 ||
      static_cast<uint32_t>(elements_end) > bound_arguments_head->size_) {
    ReportCorrupt("js_bound_function_bound_arguments_elements_out_of_range",
                  "JSBoundFunction bound arguments FixedArray elements are "
                  "outside the dumped object payload",
                  "object_body", real_address, bound_arguments_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  bound_arguments_head->offset_);
    return nullptr;
  }
  return fake_bound_arguments;
}

bool ObjectTranslator::LoadJSGlobalObjectGlobalProxy(
    i::Address real_address, i::Tagged<i::Map> map,
    i::Tagged<i::JSGlobalObject> global_obj, i::Address* global_proxy_address) {
  DCHECK_NOT_NULL(global_proxy_address);
  *global_proxy_address = 0;

  // Mirrors the Torque-generated JSGlobalObject::global_proxy(cage_base)
  // accessor in gen/torque-generated/src/objects/js-objects-tq-inl.inc. Keep
  // type policy aligned with native SetInternalReference: after proving the
  // tagged target exists in rawheap, node presence is checked by the caller.
  if (!LoadHeapObjectAddress(cage_base_, global_obj,
                             i::JSGlobalObject::kGlobalProxyOffset,
                             global_proxy_address)) {
    return false;
  }

  const RawHeapObjectHead* global_proxy_head =
      FindObjectHead(*global_proxy_address);
  if (global_proxy_head == nullptr) {
    ReportCorrupt("js_global_object_global_proxy_missing",
                  "JSGlobalObject global_proxy was not present in rawheap",
                  "object_body", real_address, *global_proxy_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()));
    return false;
  }
  return true;
}

bool ObjectTranslator::LoadJSArrayBufferViewBuffer(
    i::Address real_address, i::Tagged<i::Map> map,
    i::Tagged<i::JSArrayBufferView> view, i::Address* buffer_address) {
  DCHECK_NOT_NULL(buffer_address);
  *buffer_address = 0;

  // Mirrors the Torque-generated JSArrayBufferView::buffer(cage_base) accessor
  // in gen/torque-generated/src/objects/js-array-buffer-tq-inl.inc. Keep type
  // policy aligned with native SetInternalReference: after proving the tagged
  // target exists in rawheap, node presence is checked by the caller.
  if (!LoadHeapObjectAddress(cage_base_, view,
                             i::JSArrayBufferView::kBufferOffset,
                             buffer_address)) {
    return false;
  }

  const RawHeapObjectHead* buffer_head = FindObjectHead(*buffer_address);
  if (buffer_head == nullptr) {
    ReportCorrupt("js_array_buffer_view_buffer_missing",
                  "JSArrayBufferView buffer was not present in rawheap",
                  "object_body", real_address, *buffer_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()));
    return false;
  }
  return true;
}

bool ObjectTranslator::LoadJSObjectRawPropertiesOrHash(
    i::Address real_address, i::Tagged<i::Map> map,
    i::Tagged<i::JSObject> js_obj, i::Address* properties_address) {
  DCHECK_NOT_NULL(properties_address);
  *properties_address = 0;

  // Mirrors JSReceiver::raw_properties_or_hash() / the Torque-generated
  // properties_or_hash(cage_base) accessor in js-objects-tq-inl.inc. A Smi hash
  // has no heap edge; heap-object backing stores are node-gated by the caller.
  if (!LoadHeapObjectAddress(cage_base_, js_obj,
                             i::JSObject::kPropertiesOrHashOffset,
                             properties_address)) {
    return false;
  }

  const RawHeapObjectHead* properties_head =
      FindObjectHead(*properties_address);
  if (properties_head == nullptr) {
    ReportCorrupt("js_object_properties_missing",
                  "JSObject properties backing store was not present in "
                  "rawheap",
                  "object_body", real_address, *properties_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()));
    return false;
  }
  return true;
}

bool ObjectTranslator::LoadJSObjectElements(i::Address real_address,
                                            i::Tagged<i::Map> map,
                                            i::Tagged<i::JSObject> js_obj,
                                            i::Address* elements_address) {
  DCHECK_NOT_NULL(elements_address);
  *elements_address = 0;

  // Mirrors JSObject::elements(cage_base) / the Torque-generated
  // elements(cage_base) accessor in js-objects-tq-inl.inc. It is a tagged
  // FixedArrayBase edge; snapshot-node presence is checked by the caller.
  if (!LoadHeapObjectAddress(cage_base_, js_obj, i::JSObject::kElementsOffset,
                             elements_address)) {
    return false;
  }

  const RawHeapObjectHead* elements_head = FindObjectHead(*elements_address);
  if (elements_head == nullptr) {
    ReportCorrupt("js_object_elements_missing",
                  "JSObject elements backing store was not present in rawheap",
                  "object_body", real_address, *elements_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()));
    return false;
  }
  return true;
}

// TagObject parity policy: full V8HeapExplorer::TagObject name/type override
// parity is intentionally not implemented inline here. It is a low-priority
// readability bucket tracked centrally in
// doc/internal/niy_translate_triage_plan.md. Keep graph edges from dumped bytes
// here; add future structural tags only as small, testable slices instead of
// scattering per-call-site NIY comments.
void ObjectTranslator::ExtractJSObjectReferences(i::Address real_address,
                                                 i::Tagged<i::JSObject> js_obj,
                                                 i::Tagged<i::Map> map) {
  i::InstanceType instance_type = map->instance_type();
  ExtractPropertyReferences(real_address, js_obj, map);
  ExtractElementReferences(real_address, js_obj, map);
  ExtractInternalReferences(real_address, js_obj, map);
  i::Address js_object_prototype_address;
  if (LoadJSObjectPrototype(real_address, js_obj, map,
                            &js_object_prototype_address)) {
    generator_->SetPropertyReference(real_address,
                                     InternalizedStringNames::proto_string(),
                                     js_object_prototype_address);
  }

  if (i::InstanceTypeChecker::IsJSBoundFunction(instance_type)) {
    i::Tagged<i::JSBoundFunction> js_fun =
        i::UncheckedCast<i::JSBoundFunction>(js_obj);

    i::Address bindings_address;
    if (LoadJSBoundFunctionReference(real_address, map, js_fun,
                                     i::JSBoundFunction::kBoundArgumentsOffset,
                                     &bindings_address)) {
      generator_->SetInternalReference(
          real_address, "bindings", bindings_address,
          i::JSBoundFunction::kBoundArgumentsOffset);
    }

    i::Address bound_this_address;
    if (LoadJSBoundFunctionReference(real_address, map, js_fun,
                                     i::JSBoundFunction::kBoundThisOffset,
                                     &bound_this_address)) {
      generator_->SetInternalReference(real_address, "bound_this",
                                       bound_this_address,
                                       i::JSBoundFunction::kBoundThisOffset);
    }

    i::Address bound_function_address;
    if (LoadJSBoundFunctionReference(
            real_address, map, js_fun,
            i::JSBoundFunction::kBoundTargetFunctionOffset,
            &bound_function_address)) {
      generator_->SetInternalReference(
          real_address, "bound_function", bound_function_address,
          i::JSBoundFunction::kBoundTargetFunctionOffset);
    }

    std::unique_ptr<FakeHeapObject> fake_bindings =
        GetJSBoundFunctionBoundArguments(real_address, map, js_fun);
    if (!fake_bindings) {
      return;
    }
    i::Tagged<i::FixedArray> bindings =
        i::UncheckedCast<i::FixedArray>(fake_bindings->fake_object());
    for (int i = 0; i < bindings->length(); i++) {
      const int offset = i::FixedArray::OffsetOfElementAt(i);
      i::Address argument_address;
      // Each argument can be any tagged HeapObject. Emit the native shortcut
      // only after the target is present in rawheap and has a translated
      // snapshot node; Smi/primitives/missing nodes are safe skips.
      if (!LoadHeapObjectAddress(cage_base_, bindings, offset,
                                 &argument_address) ||
          FindObjectHead(argument_address) == nullptr ||
          !generator_->GetNode(argument_address)) {
        continue;
      }
      generator_->SetShortcutReference(real_address,
                                       "bound_argument_" + std::to_string(i),
                                       argument_address);
    }
  } else if (i::InstanceTypeChecker::IsJSFunction(instance_type)) {
    // Handle JSFunction-specific references
    i::Tagged<i::JSFunction> js_fun = i::UncheckedCast<i::JSFunction>(js_obj);

    if (map->has_prototype_slot()) {
      i::Address proto_or_map_address;
      i::InstanceType proto_or_map_type;
      if (LoadJSFunctionPrototypeOrInitialMapIfNotHole(
              real_address, map, js_fun, &proto_or_map_address,
              &proto_or_map_type)) {
        if (proto_or_map_type != i::MAP_TYPE) {
          if (generator_->GetNode(proto_or_map_address)) {
            generator_->SetPropertyReference(
                real_address, InternalizedStringNames::prototype_string(),
                proto_or_map_address,
                i::JSFunction::kPrototypeOrInitialMapOffset);
          }
        } else {
          i::Address prototype_address;
          if (LoadJSFunctionPrototype(real_address, map, proto_or_map_address,
                                      &prototype_address) &&
              generator_->GetNode(prototype_address)) {
            generator_->SetPropertyReference(
                real_address, InternalizedStringNames::prototype_string(),
                prototype_address);
          }
          if (generator_->GetNode(proto_or_map_address)) {
            generator_->SetInternalReference(
                real_address, "initial_map", proto_or_map_address,
                i::JSFunction::kPrototypeOrInitialMapOffset);
          }
        }
      }
    }

    // These JSFunction fields are ordinary tagged HeapObjects. Classify them
    // from dumped bytes first, then require a translated snapshot node before
    // emitting the native-shaped internal edge.
    SetInternalReferenceIfExistingHeapObject(
        generator_, real_address, "feedback_cell", cage_base_, js_fun,
        i::JSFunction::kFeedbackCellOffset);
    SetInternalReferenceIfExistingHeapObject(
        generator_, real_address, "shared", cage_base_, js_fun,
        i::JSFunction::kSharedFunctionInfoOffset);
    SetInternalReferenceIfExistingHeapObject(generator_, real_address,
                                             "context", cage_base_, js_fun,
                                             i::JSFunction::kContextOffset);
    SetInternalReferenceFromJSDispatchEntry(
        real_address, "code", js_fun, i::JSFunction::kDispatchHandleOffset);
  } else if (i::InstanceTypeChecker::IsJSGlobalObject(instance_type)) {
    // Handle JSGlobalObject references
    i::Tagged<i::JSGlobalObject> global_obj =
        i::UncheckedCast<i::JSGlobalObject>(js_obj);
    i::Address global_proxy_address;
    if (LoadJSGlobalObjectGlobalProxy(real_address, map, global_obj,
                                      &global_proxy_address)) {
      if (generator_->GetNode(global_proxy_address)) {
        generator_->SetInternalReference(real_address, "global_proxy",
                                         global_proxy_address,
                                         i::JSGlobalObject::kGlobalProxyOffset);
      }
    }
  } else if (i::InstanceTypeChecker::IsJSArrayBufferView(instance_type)) {
    // Handle JSArrayBufferView references
    i::Tagged<i::JSArrayBufferView> view =
        i::UncheckedCast<i::JSArrayBufferView>(js_obj);
    i::Address buffer_address;
    if (LoadJSArrayBufferViewBuffer(real_address, map, view, &buffer_address)) {
      if (generator_->GetNode(buffer_address)) {
        generator_->SetInternalReference(real_address, "buffer", buffer_address,
                                         i::JSArrayBufferView::kBufferOffset);
      }
    }
  }

  i::Address properties_address;
  if (LoadJSObjectRawPropertiesOrHash(real_address, map, js_obj,
                                      &properties_address)) {
    if (generator_->GetNode(properties_address)) {
      generator_->SetInternalReference(real_address, "properties",
                                       properties_address,
                                       i::JSObject::kPropertiesOrHashOffset);
    }
  }

  i::Address elements_address;
  if (LoadJSObjectElements(real_address, map, js_obj, &elements_address)) {
    if (generator_->GetNode(elements_address)) {
      generator_->SetInternalReference(real_address, "elements",
                                       elements_address,
                                       i::JSObject::kElementsOffset);
    }
  }
}

void ObjectTranslator::ExtractJSWeakCollectionReferences(
    i::Address real_address, i::Tagged<i::JSWeakCollection> obj) {
  // JSWeakCollection::table() is a Torque-generated load from the ordinary
  // tagged field at kTableOffset. See the generated accessor in:
  //   gen/torque-generated/src/objects/js-collection-tq-inl.inc
  SetInternalReferenceIfExistingHeapObject(generator_, real_address, "table",
                                           cage_base_, obj,
                                           i::JSWeakCollection::kTableOffset);
}

void ObjectTranslator::ExtractJSCollectionReferences(
    i::Address real_address, i::Tagged<i::JSCollection> collection) {
  // JSCollection::table() is a Torque-generated load from the ordinary tagged
  // field at kTableOffset. See the generated accessor in:
  //   gen/torque-generated/src/objects/js-collection-tq-inl.inc
  SetInternalReferenceIfExistingHeapObject(generator_, real_address, "table",
                                           cage_base_, collection,
                                           i::JSCollection::kTableOffset);
}

void ObjectTranslator::ExtractJSPromiseReferences(
    i::Address real_address, i::Tagged<i::JSPromise> promise) {
  // JSPromise::reactions_or_result() is a Torque-generated load from the
  // ordinary tagged field at kReactionsOrResultOffset. See the generated
  // accessor in:
  //   gen/torque-generated/src/objects/js-promise-tq-inl.inc
  SetInternalReferenceIfExistingHeapObject(
      generator_, real_address, "reactions_or_result", cage_base_, promise,
      i::JSPromise::kReactionsOrResultOffset);
}

void ObjectTranslator::ExtractJSArrayBufferReferences(
    i::Address real_address, i::Tagged<i::JSArrayBuffer> buffer) {
  // Setup a reference to a native memory backing_store object.
  // backing_store is an external pointer (off-heap), not a HeapObject.
  // Note: detach_key will be handled by the normal object visitor during
  // dfx_heap_object_visitor.Visit(), so we only need to handle backing_store
  // here.
  // src/objects/js-array-buffer-inl.h
  i::Address backing_store_raw = buffer->ReadSandboxedPointerField(
      i::JSArrayBuffer::kBackingStoreOffset, cage_base_);
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

void ObjectTranslator::SetStringInternalReference(i::Address real_address,
                                                  i::Tagged<i::String> string,
                                                  i::InstanceType instance_type,
                                                  const char* name,
                                                  int offset) {
  // Offline counterpart of V8HeapExplorer::SetInternalReference for
  // ExtractStringReferences' named string component fields.
  i::Address child;
  if (!LoadHeapObjectAddress(cage_base_, string, offset, &child)) {
    // This is an object-specific named field in the native extractor; suppress
    // generic traversal rather than fabricating a hidden edge for corrupt data.
    generator_->MarkVisitedField(offset);
    return;
  }

  i::InstanceType child_type;
  // String component edges are real V8 heap-object edges only when the exact
  // dumped child has a snapshot node and string-family type. Never pass raw
  // corrupt addresses to SetInternalReference.
  if (FindObjectHead(child) == nullptr ||
      generator_->GetNode(child) == nullptr ||
      !GetInstanceType(child, &child_type) ||
      !i::InstanceTypeChecker::IsString(child_type)) {
    ReportWarning("string_reference_target_invalid",
                  "String internal reference target was missing or not a "
                  "dumped string object",
                  "object_body", real_address, 0,
                  static_cast<uint32_t>(instance_type), 0,
                  static_cast<uint32_t>(offset));
    generator_->MarkVisitedField(offset);
    return;
  }

  generator_->SetInternalReference(real_address, name, child, offset);
}

void ObjectTranslator::ExtractStringReferences(i::Address real_address,
                                               i::Tagged<i::String> string,
                                               i::InstanceType instance_type) {
  // Mirrors heap-snapshot-generator.cc ExtractStringReferences for
  // ConsString/SlicedString/ThinString named internal edges.
  // Note: We cannot call methods like cs->first(cage_base_) because for
  // FakeHeapObject, the pointer fields don't point to valid memory. Instead,
  // use TaggedField::load to read the raw tagged value, then get the address.
  if (i::InstanceTypeChecker::IsConsString(instance_type)) {
    SetStringInternalReference(real_address, string, instance_type, "first",
                               i::ConsString::kFirstOffset);
    SetStringInternalReference(real_address, string, instance_type, "second",
                               i::ConsString::kSecondOffset);
  } else if (i::InstanceTypeChecker::IsSlicedString(instance_type)) {
    // Use offsetof since SlicedString doesn't have kParentOffset constant
    constexpr int kParentOffset = offsetof(i::SlicedString, parent_);
    SetStringInternalReference(real_address, string, instance_type, "parent",
                               kParentOffset);
  } else if (i::InstanceTypeChecker::IsThinString(instance_type)) {
    // Use offsetof since ThinString doesn't have kActualOffset constant
    constexpr int kActualOffset = offsetof(i::ThinString, actual_);
    SetStringInternalReference(real_address, string, instance_type, "actual",
                               kActualOffset);
  }
  // Note: ExternalString resource accounting is native state, not a V8 heap
  // object edge.
}

void ObjectTranslator::ExtractJSGeneratorObjectReferences(
    i::Address real_address, i::Tagged<i::JSGeneratorObject> generator) {
  // JSGeneratorObject reference accessors are Torque-generated loads from
  // ordinary tagged fields. See the generated accessors in:
  //   gen/torque-generated/src/objects/js-generator-tq-inl.inc
  SetInternalReferenceIfExistingHeapObject(
      generator_, real_address, "function", cage_base_, generator,
      i::JSGeneratorObject::kFunctionOffset);
  SetInternalReferenceIfExistingHeapObject(
      generator_, real_address, "context", cage_base_, generator,
      i::JSGeneratorObject::kContextOffset);
  SetInternalReferenceIfExistingHeapObject(
      generator_, real_address, "receiver", cage_base_, generator,
      i::JSGeneratorObject::kReceiverOffset);
  SetInternalReferenceIfExistingHeapObject(
      generator_, real_address, "parameters_and_registers", cage_base_,
      generator, i::JSGeneratorObject::kParametersAndRegistersOffset);
}

void ObjectTranslator::ExtractJSWeakRefReferences(
    i::Address real_address, i::Tagged<i::JSWeakRef> js_weak_ref) {
  // JSWeakRef::target() is a Torque-generated load from the ordinary tagged
  // field at kTargetOffset. See the generated accessor in:
  //   gen/torque-generated/src/objects/js-weak-refs-tq-inl.inc
  SetWeakReferenceIfExistingHeapObject(generator_, real_address, "target",
                                       cage_base_, js_weak_ref,
                                       i::JSWeakRef::kTargetOffset);
}

void ObjectTranslator::ExtractJSDisposableStackReferences(
    i::Address real_address,
    i::Tagged<i::JSDisposableStackBase> disposable_stack) {
  // JSDisposableStackBase stack/error/error_message accessors are
  // Torque-generated loads from ordinary tagged fields. See the generated
  // accessors in:
  //   gen/torque-generated/src/objects/js-disposable-stack-tq-inl.inc
  SetInternalReferenceIfExistingHeapObject(
      generator_, real_address, "stack", cage_base_, disposable_stack,
      i::JSDisposableStackBase::kStackOffset);
  SetInternalReferenceIfExistingHeapObject(
      generator_, real_address, "error", cage_base_, disposable_stack,
      i::JSDisposableStackBase::kErrorOffset);
  SetInternalReferenceIfExistingHeapObject(
      generator_, real_address, "error_message", cage_base_, disposable_stack,
      i::JSDisposableStackBase::kErrorMessageOffset);
}

bool ObjectTranslator::LoadMapConstructorOrBackPointer(
    i::Address real_address, i::Tagged<i::Map> map, i::Address* value_address,
    i::InstanceType* value_type, bool* has_value) {
  DCHECK_NOT_NULL(value_address);
  DCHECK_NOT_NULL(value_type);
  DCHECK_NOT_NULL(has_value);
  *value_address = 0;
  *value_type = i::ODDBALL_TYPE;
  *has_value = false;

  // Offline counterpart of map->constructor_or_back_pointer(): read the shared
  // constructor/back-pointer/native-context slot, then inspect the target map
  // because ExtractMapReferences branches on the target's instance type.
  i::Tagged<i::Object> value = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, map, i::Map::kConstructorOrBackPointerOrNativeContextOffset);
  if (!i::IsHeapObject(value)) {
    return true;
  }

  *has_value = true;
  *value_address = i::Cast<i::HeapObject>(value).address();
  std::unique_ptr<FakeHeapObject> fake_value =
      MakeFakeHeapObject(*value_address);
  if (!fake_value) {
    ReportCorrupt("map_constructor_back_pointer_missing",
                  "Map constructor/back pointer was not present in rawheap",
                  "object_body", real_address, *value_address,
                  static_cast<uint32_t>(map->instance_type()));
    return false;
  }

  i::Tagged<i::HeapObject> fake_obj = fake_value->fake_object();
  i::Address fake_obj_map_address = fake_obj->map(cage_base_).address();
  std::unique_ptr<FakeHeapObject> fake_obj_map =
      MakeFakeHeapObject(fake_obj_map_address);
  if (!fake_obj_map) {
    ReportCorrupt("map_constructor_back_pointer_map_missing",
                  "Map constructor/back pointer map was not present in rawheap",
                  "object_body", real_address, fake_obj_map_address,
                  static_cast<uint32_t>(map->instance_type()));
    return false;
  }

  i::Tagged<i::Map> value_map =
      i::UncheckedCast<i::Map>(fake_obj_map->fake_object());
  *value_type = value_map->instance_type();
  return true;
}

bool ObjectTranslator::LoadMapDependentCode(
    i::Address real_address, i::Tagged<i::Map> map,
    i::Address* dependent_code_address, i::InstanceType* dependent_code_type) {
  USE(real_address);
  DCHECK_NOT_NULL(dependent_code_address);
  DCHECK_NOT_NULL(dependent_code_type);
  *dependent_code_address = 0;
  *dependent_code_type = i::ODDBALL_TYPE;

  // Mirrors Map::dependent_code() (generated accessor in map-inl.h).
  if (!LoadHeapObjectAddress(cage_base_, map, i::Map::kDependentCodeOffset,
                             dependent_code_address)) {
    return false;
  }
  return GetInstanceType(*dependent_code_address, dependent_code_type);
}

bool ObjectTranslator::LoadMapPrototypeValidityCell(
    i::Address real_address, i::Tagged<i::Map> map,
    i::Address* prototype_validity_cell_address,
    i::InstanceType* prototype_validity_cell_type) {
  USE(real_address);
  DCHECK_NOT_NULL(prototype_validity_cell_address);
  DCHECK_NOT_NULL(prototype_validity_cell_type);
  *prototype_validity_cell_address = 0;
  *prototype_validity_cell_type = i::ODDBALL_TYPE;

  // Mirrors Map::prototype_validity_cell(kRelaxedLoad) (generated accessor
  // in map-inl.h): the slot can hold Smi sentinel, and only HeapObject values
  // can be used for downstream snapshot tagging.
  i::Tagged<i::Object> value =
      i::TaggedField<i::Tagged<i::Object>>::Relaxed_Load(
          cage_base_, map, i::Map::kPrototypeValidityCellOffset);
  if (!i::IsHeapObject(value)) {
    return false;
  }
  *prototype_validity_cell_address = i::Cast<i::HeapObject>(value).address();
  return GetInstanceType(*prototype_validity_cell_address,
                         prototype_validity_cell_type);
}

bool ObjectTranslator::ExtractMapReferences(i::Address real_address,
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
    // A weak back-pointer to a sibling Map: prove the MaybeObject target from
    // rawheap first, then require a translated node before emitting the edge.
    i::InstanceType weak_target_type;
    if (GetInstanceType(transitions_or_prototype_info.address(),
                        &weak_target_type) &&
        i::InstanceTypeChecker::IsMap(weak_target_type) &&
        generator_->GetNode(transitions_or_prototype_info.address())) {
      generator_->SetWeakReference(real_address, "transition",
                                   transitions_or_prototype_info.address(),
                                   i::Map::kTransitionsOrPrototypeInfoOffset);
    }
  } else if (maybe_transitions_or_prototype_info.GetHeapObjectIfStrong(
                 &transitions_or_prototype_info)) {
    // Use FakeHeapObject to check the type of transitions_or_prototype_info
    i::InstanceType type;
    if (!GetInstanceType(transitions_or_prototype_info.address(), &type)) {
      return false;
    }

    if (!generator_->GetNode(transitions_or_prototype_info.address())) {
      // Native SetInternalReference skips children without snapshot entries; do
      // the same before SetReference can mark this field visited.
    } else if (i::InstanceTypeChecker::IsTransitionArray(type)) {
      // Native also TagObject()s the TransitionArray and optional prototype
      // transitions here; translated retagging is handled by the target
      // extractors, while this Map field only emits the "transitions" edge.
      generator_->SetInternalReference(
          real_address, "transitions", transitions_or_prototype_info.address(),
          i::Map::kTransitionsOrPrototypeInfoOffset);
    } else if (i::InstanceTypeChecker::IsFixedArray(type)) {
      // Mirrors the legacy strong FixedArray transition branch in native
      // ExtractMapReferences.
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
    SetInternalReferenceIfExistingHeapObject(
        generator_, real_address, "descriptors", cage_base_, map,
        i::Map::kInstanceDescriptorsOffset);
  }

  // prototype field
  SetInternalReferenceIfExistingHeapObject(generator_, real_address,
                                           "prototype", cage_base_, map,
                                           i::Map::kPrototypeOffset);

  // Mirrors heap-snapshot-generator.cc: IsContextMap/IsMapMap read
  // native_context_or_null(), other maps read constructor_or_back_pointer().
  if (i::InstanceTypeChecker::IsContext(map->instance_type()) ||
      i::InstanceTypeChecker::IsMap(map->instance_type())) {
    SetInternalReferenceIfExistingHeapObject(
        generator_, real_address, "native_context", cage_base_, map,
        i::Map::kConstructorOrBackPointerOrNativeContextOffset);
  } else {
    i::Address constructor_or_back_pointer_address;
    i::InstanceType constructor_or_back_pointer_type;
    bool has_constructor_or_back_pointer;
    if (!LoadMapConstructorOrBackPointer(real_address, map,
                                         &constructor_or_back_pointer_address,
                                         &constructor_or_back_pointer_type,
                                         &has_constructor_or_back_pointer)) {
      return false;
    }
    if (has_constructor_or_back_pointer &&
        generator_->GetNode(constructor_or_back_pointer_address)) {
      if (i::InstanceTypeChecker::IsMap(constructor_or_back_pointer_type)) {
        generator_->SetInternalReference(
            real_address, "back_pointer", constructor_or_back_pointer_address,
            i::Map::kConstructorOrBackPointerOrNativeContextOffset);
      } else if (i::InstanceTypeChecker::IsFunctionTemplateInfo(
                     constructor_or_back_pointer_type)) {
        generator_->SetInternalReference(
            real_address, "constructor_function_data",
            constructor_or_back_pointer_address,
            i::Map::kConstructorOrBackPointerOrNativeContextOffset);
      } else {
        generator_->SetInternalReference(
            real_address, "constructor", constructor_or_back_pointer_address,
            i::Map::kConstructorOrBackPointerOrNativeContextOffset);
      }
    }
  }

  i::Address dependent_code_address;
  i::InstanceType dependent_code_type;
  if (LoadMapDependentCode(real_address, map, &dependent_code_address,
                           &dependent_code_type)) {
    if (dependent_code_type == i::WEAK_ARRAY_LIST_TYPE &&
        generator_->GetNode(dependent_code_address) != nullptr) {
      // Native TagObject(map->dependent_code(), "(dependent code)") is
      // label-only. DependentCode currently aliases WeakArrayList, so keep the
      // translated node category and only rename an existing dumped node after
      // the exact type gate.
      generator_->RetagNodeIfExisting(dependent_code_address,
                                      "(dependent code)", Node::Type::kHidden,
                                      /*update_type=*/false,
                                      /*overwrite_system_name=*/true);
      generator_->SetInternalReference(real_address, "dependent_code",
                                       dependent_code_address,
                                       i::Map::kDependentCodeOffset);
    }
  }

  i::Address prototype_validity_cell_address;
  i::InstanceType prototype_validity_cell_type;
  if (LoadMapPrototypeValidityCell(real_address, map,
                                   &prototype_validity_cell_address,
                                   &prototype_validity_cell_type)) {
    if (i::InstanceTypeChecker::IsCell(prototype_validity_cell_type) &&
        generator_->GetNode(prototype_validity_cell_address) != nullptr) {
      // The native helper tags an existing Cell as ObjectShape and deliberately
      // does not emit an edge from the Map field.
      generator_->RetagNodeIfExisting(prototype_validity_cell_address,
                                      "(prototype validity cell)",
                                      Node::Type::kObjectShape,
                                      /*update_type=*/true,
                                      /*overwrite_system_name=*/true);
    }
  }
  return true;
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
  std::unique_ptr<FakeHeapObject> fake_scope_info_ho =
      GetContextScopeInfo(context);
  const bool has_scope_info = fake_scope_info_ho != nullptr;
  i::Tagged<i::ScopeInfo> fake_scope_info;
  if (has_scope_info) {
    fake_scope_info =
        i::UncheckedCast<i::ScopeInfo>(fake_scope_info_ho->fake_object());
  }

  if (!is_native_context && has_scope_info &&
      IsDeclarationContextOffline(context_type, has_scope_info,
                                  fake_scope_info)) {
    // Source: heap-snapshot-generator.cc:
    // V8HeapExplorer::ExtractContextReferences enumerates
    // ScopeInfo::IterateLocalNames(scope_info, no_gc). Keep offline layout
    // details inside GetScopeInfoContextLocalNames().
    const int header = fake_scope_info->ContextHeaderLength();
    for (const ContextLocalName& local_name :
         GetScopeInfoContextLocalNames(fake_scope_info)) {
      int idx = header + local_name.index;
      SetContextReferenceIfExisting(real_address, context, local_name.name,
                                    idx);
    }
    if (fake_scope_info->HasContextAllocatedFunctionName()) {
      SetContextFunctionNameReferenceIfExisting(real_address, context,
                                                fake_scope_info);
    }
  }

  i::Address scope_info_address = 0;
  if (GetContextSlotAddress(context, i::Context::SCOPE_INFO_INDEX,
                            &scope_info_address)) {
    SetScopeInfoContextReference(real_address, context, scope_info_address);
  }
  SetInternalContextSlotIfExisting(real_address, context, "previous",
                                   i::Context::PREVIOUS_INDEX);

  if (is_native_context) {
    SetInternalContextSlotIfExisting(real_address, context, "global_object",
                                     i::Context::EXTENSION_INDEX);
    for (const auto& field : native_context_names) {
      SetInternalContextSlotIfExisting(real_address, context, field.name,
                                       field.index);
    }
  } else if (has_scope_info &&
             ContextHasExtensionOffline(context, has_scope_info,
                                        fake_scope_info)) {
    SetInternalContextSlotIfExisting(real_address, context, "extension",
                                     i::Context::EXTENSION_INDEX);
  }
}

bool ObjectTranslator::GetContextSlotAddress(i::Tagged<i::Context> context,
                                             int index, i::Address* address) {
  DCHECK_NOT_NULL(address);
  *address = 0;
  i::Tagged<i::Object> value = i::TaggedField<i::Object>::Relaxed_Load(
      cage_base_, context, i::Context::OffsetOfElementAt(index));
  if (!i::IsHeapObject(value)) {
    return false;
  }
  *address = i::Cast<i::HeapObject>(value).address();
  return true;
}

std::unique_ptr<FakeHeapObject> ObjectTranslator::GetContextScopeInfo(
    i::Tagged<i::Context> context) {
  i::Address scope_info_address = 0;
  if (!GetContextSlotAddress(context, i::Context::SCOPE_INFO_INDEX,
                             &scope_info_address)) {
    return nullptr;
  }
  return MakeFakeHeapObject(scope_info_address);
}

bool ObjectTranslator::LoadScopeInfoFunctionName(
    i::Tagged<i::ScopeInfo> scope_info, i::Address* name_address) {
  if (name_address == nullptr) {
    return false;
  }
  *name_address = 0;
  const int offset = scope_info->FunctionVariableInfoOffset();
  if (offset < 0) {
    return false;
  }
  i::Tagged<i::Object> name = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, scope_info, offset);
  if (!i::IsHeapObject(name)) {
    return false;
  }
  *name_address = i::Cast<i::HeapObject>(name).address();
  return true;
}

std::vector<ObjectTranslator::ContextLocalName>
ObjectTranslator::GetScopeInfoContextLocalNames(
    i::Tagged<i::ScopeInfo> scope_info) {
  std::vector<ContextLocalName> names;
  // Source: ScopeInfo::IterateLocalNames(scope_info, no_gc).
  //
  // Offline translate must not follow raw tagged pointers directly. For
  // inlined local names, ScopeInfo::ContextInlinedLocalName(cage_base_, i)
  // reads the name slot from the rebuilt fake ScopeInfo payload and returns the
  // original string address; GetPropertyNameFromKey then validates that string
  // through the rawheap object table before using it as an edge name.
  //
  // The non-inlined layout stores names in a secondary NameToIndexHashTable.
  // Supporting that safely belongs in this helper: rebuild the table as a
  // FakeHeapObject, validate NAME_TO_INDEX_HASH_TABLE_TYPE, then enumerate
  // KeyAt/IndexAt from the fake table. Until then, return an empty list rather
  // than letting ScopeInfo::IterateLocalNames dereference the producer address.
  if (!scope_info->HasInlinedLocalNames()) {
    return names;
  }
  const int count = scope_info->ContextLocalCount();
  names.reserve(count);
  for (int local_index = 0; local_index < count; ++local_index) {
    i::Tagged<i::String> name =
        scope_info->ContextInlinedLocalName(cage_base_, local_index);
    std::string decoded_name = GetPropertyNameFromKey(name);
    if (!decoded_name.empty()) {
      names.push_back({decoded_name, local_index});
    }
  }
  return names;
}

bool ObjectTranslator::SetContextReferenceIfExisting(
    i::Address real_address, i::Tagged<i::Context> context,
    std::string reference_name, int index) {
  if (reference_name.empty()) {
    return false;
  }
  int field_offset = i::Context::OffsetOfElementAt(index);
  if (field_offset < 0) {
    return false;
  }
  if (static_cast<size_t>(field_offset / i::kTaggedSize) >=
      generator_->max_pointers_) {
    return false;
  }
  i::Address child = 0;
  if (!GetContextSlotAddress(context, index, &child)) {
    return false;
  }
  if (!generator_->GetNode(child)) {
    return false;
  }
  generator_->SetContextReference(real_address, reference_name, child,
                                  field_offset);
  return true;
}

bool ObjectTranslator::SetContextFunctionNameReferenceIfExisting(
    i::Address real_address, i::Tagged<i::Context> context,
    i::Tagged<i::ScopeInfo> scope_info) {
  i::Address name_address = 0;
  i::InstanceType name_type;
  if (!LoadScopeInfoFunctionName(scope_info, &name_address) ||
      !GetInstanceType(name_address, &name_type) ||
      !i::InstanceTypeChecker::IsString(name_type)) {
    return false;
  }

  i::Tagged<i::String> name =
      i::UncheckedCast<i::String>(i::HeapObject::FromAddress(name_address));
  // Offline equivalent of:
  //   Tagged<String> name = Cast<String>(scope_info->FunctionName());
  //   int idx = scope_info->FunctionContextSlotIndex(name);
  // Do not call FunctionName() or FunctionContextSlotIndex(): their debug
  // string checks may dereference the producer heap address. The comparison
  // below is raw address equality, matching FunctionName() == name without
  // touching the target object.
  if (name_address != name.address()) {
    return false;
  }
  const int idx =
      scope_info->function_variable_info_context_or_stack_slot_index();
  if (idx < 0) {
    return false;
  }
  return SetContextReferenceIfExisting(real_address, context,
                                       GetPropertyNameFromKey(name), idx);
}

bool ObjectTranslator::SetInternalContextSlotIfExisting(
    i::Address real_address, i::Tagged<i::Context> context,
    const char* edge_name, int index) {
  int field_offset = i::Context::OffsetOfElementAt(index);
  if (field_offset < 0) {
    return false;
  }
  if (static_cast<size_t>(field_offset / i::kTaggedSize) >=
      generator_->max_pointers_) {
    return false;
  }
  i::Address child = 0;
  if (!GetContextSlotAddress(context, index, &child)) {
    return false;
  }
  if (!generator_->GetNode(child)) {
    return false;
  }
  generator_->SetInternalReference(real_address, edge_name, child,
                                   field_offset);
  return true;
}

bool ObjectTranslator::SetScopeInfoContextReference(
    i::Address real_address, i::Tagged<i::Context> context,
    i::Address scope_info_child) {
  USE(context);
  // "scope_info" edge: IsHeapObject + snapshot GetNode membership are
  // necessary but not sufficient. The target must also have ScopeInfo type
  // family; a corrupt or mistyped SCOPE_INFO_INDEX slot must be silently
  // dropped, not emitted as a "scope_info" edge to a non-ScopeInfo object.
  if (scope_info_child == 0) {
    return false;
  }
  int scope_info_offset =
      i::Context::OffsetOfElementAt(i::Context::SCOPE_INFO_INDEX);
  if (scope_info_offset < 0 ||
      static_cast<size_t>(scope_info_offset / i::kTaggedSize) >=
          generator_->max_pointers_ ||
      !generator_->GetNode(scope_info_child)) {
    return false;
  }
  i::InstanceType scope_info_type;
  if (!GetInstanceType(scope_info_child, &scope_info_type) ||
      !i::InstanceTypeChecker::IsScopeInfo(scope_info_type)) {
    return false;
  }
  generator_->SetInternalReference(real_address, "scope_info",
                                   scope_info_child, scope_info_offset);
  return true;
}

bool ObjectTranslator::IsDeclarationContextOffline(
    i::InstanceType context_type, bool has_scope_info,
    i::Tagged<i::ScopeInfo> scope_info) {
  // Offline copy of Context::is_declaration_context(). Do not call the helper
  // directly: it may walk map/heap state not available in translate mode.
  switch (context_type) {
    case i::InstanceType::FUNCTION_CONTEXT_TYPE:
    case i::InstanceType::SCRIPT_CONTEXT_TYPE:
    case i::InstanceType::MODULE_CONTEXT_TYPE:
      return true;
    case i::InstanceType::EVAL_CONTEXT_TYPE:
      return has_scope_info &&
             scope_info->language_mode() == i::LanguageMode::kStrict;
    case i::InstanceType::BLOCK_CONTEXT_TYPE:
      return has_scope_info && scope_info->is_declaration_scope();
    default:
      return false;
  }
}

bool ObjectTranslator::ContextHasExtensionOffline(
    i::Tagged<i::Context> context, bool has_scope_info,
    i::Tagged<i::ScopeInfo> scope_info) {
  if (!has_scope_info || !scope_info->HasContextExtensionSlot()) {
    return false;
  }
  int field_offset =
      i::Context::OffsetOfElementAt(i::Context::EXTENSION_INDEX);
  if (field_offset < 0 ||
      static_cast<size_t>(field_offset / i::kTaggedSize) >=
          generator_->max_pointers_) {
    return false;
  }
  i::Address value_address = 0;
  if (!GetContextSlotAddress(context, i::Context::EXTENSION_INDEX,
                             &value_address)) {
    return false;
  }
  return !IsUndefinedOddballOffline(value_address);
}

// ExtractSymbolReferences with real_address parameter
void ObjectTranslator::ExtractSymbolReferences(i::Address real_address,
                                               i::Tagged<i::Symbol> symbol) {
  SetInternalReferenceIfExistingHeapObject(generator_, real_address, "name",
                                           cage_base_, symbol,
                                           offsetof(i::Symbol, description_));
}

// ExtractSharedFunctionInfoReferences
void ObjectTranslator::ExtractSharedFunctionInfoReferences(
    i::Address real_address, i::Tagged<i::SharedFunctionInfo> shared,
    i::Tagged<i::Map> map) {
  SetInternalReferenceIfExistingHeapObject(
      generator_, real_address, "name_or_scope_info", cage_base_, shared,
      i::SharedFunctionInfo::kNameOrScopeInfoOffset);
  SetInternalReferenceIfExistingHeapObject(
      generator_, real_address, "script", cage_base_, shared,
      i::SharedFunctionInfo::kScriptOffset);

  // trusted_function_data is a TrustedPointer field (IndirectPointer in
  // sandbox). Use TrustedPointerField to correctly resolve the indirect handle,
  // matching native SharedFunctionInfo::GetTrustedData() which calls
  // ReadMaybeEmptyTrustedPointerField<kUnknownIndirectPointerTag>.
  TrustedPointerField trusted_field(offheap_table_resolver_);
  OfflineIndirectPointerLoadResult result;
  OfflineIndirectPointerLoadStatus status =
      trusted_field.ReadTrustedPointerField(
          shared, i::SharedFunctionInfo::kTrustedFunctionDataOffset,
          i::kUnknownIndirectPointerTag, &result);
  if (status == OfflineIndirectPointerLoadStatus::kOk &&
      result.target_address_ != 0) {
    i::Address trusted_data_address = result.target_address_;
    if (FindObjectHead(trusted_data_address) == nullptr &&
        (trusted_data_address & i::kHeapObjectTag) == i::kHeapObjectTag) {
      const i::Address untagged_trusted_data_address =
          trusted_data_address - i::kHeapObjectTag;
      if (FindObjectHead(untagged_trusted_data_address) != nullptr) {
        trusted_data_address = untagged_trusted_data_address;
      }
    }
    if (FindObjectHead(trusted_data_address) != nullptr &&
        generator_->GetNode(trusted_data_address) != nullptr) {
      generator_->SetInternalReference(
          real_address, "trusted_function_data", trusted_data_address,
          i::SharedFunctionInfo::kTrustedFunctionDataOffset);
    }
  }

  SetInternalReferenceIfExistingHeapObject(
      generator_, real_address, "untrusted_function_data", cage_base_, shared,
      i::SharedFunctionInfo::kUntrustedFunctionDataOffset);
  SetInternalReferenceIfExistingHeapObject(
      generator_, real_address, "raw_outer_scope_info_or_feedback_metadata",
      cage_base_, shared,
      i::SharedFunctionInfo::kOuterScopeInfoOrFeedbackMetadataOffset);
}

// Offline counterpart of V8HeapExplorer::ExtractCodeReferences. The live V8
// version reads Code fields directly; here the Code object is a FakeHeapObject
// and the emitted child addresses must already exist as translated nodes.
bool ObjectTranslator::LoadCodeInstructionStream(
    i::Address real_address, i::Tagged<i::Code> code,
    i::Address* instruction_stream_address) {
  DCHECK_NOT_NULL(instruction_stream_address);
  *instruction_stream_address = 0;
  // V8_EXTERNAL_CODE_SPACE: the InstructionStreamSlot is encoded relative to
  // code_cage_base_, not the main cage. A generic tagged-field decode with
  // cage_base_ would produce a wrong address. Membership and instance-type
  // checks are mandatory before emitting the edge so a misread slot never
  // silently creates a dangling reference.
  i::Tagged<i::Object> instruction_stream_value =
      code->raw_instruction_stream(code_cage_base_);
  if (!i::IsHeapObject(instruction_stream_value)) {
    return false;
  }

  i::Address child = i::Cast<i::HeapObject>(instruction_stream_value).address();
  // Membership check: child must be present in the rawheap object table.
  std::unique_ptr<FakeHeapObject> fake_child = MakeFakeHeapObject(child);
  if (!fake_child) {
    ReportWarning("instruction_stream_not_in_rawheap",
                  "Code::instruction_stream target not found in rawheap "
                  "objects; skipping edge",
                  "object_body", real_address, 0,
                  static_cast<uint32_t>(i::CODE_TYPE));
    return false;
  }

  // Type check via rawheap-backed fake objects. The decoded child and its map
  // must both be present in the dump before the edge is trusted.
  i::Address child_map_address =
      fake_child->fake_object()->map(cage_base_).address();
  std::unique_ptr<FakeHeapObject> fake_child_map =
      MakeFakeHeapObject(child_map_address);
  if (!fake_child_map) {
    ReportWarning("instruction_stream_map_not_in_rawheap",
                  "Code::instruction_stream target map not found in "
                  "rawheap objects; skipping edge",
                  "object_body", real_address, child_map_address,
                  static_cast<uint32_t>(i::CODE_TYPE));
    return false;
  }
  i::Tagged<i::Map> child_map =
      i::UncheckedCast<i::Map>(fake_child_map->fake_object());
  i::InstanceType child_type = child_map->instance_type();
  if (!i::InstanceTypeChecker::IsInstructionStream(child_type)) {
    ReportWarning("instruction_stream_type_mismatch",
                  "Code::instruction_stream target has unexpected instance "
                  "type; skipping edge",
                  "object_body", real_address, 0,
                  static_cast<uint32_t>(child_type));
    return false;
  }
  if (!generator_->GetNode(child)) {
    return false;
  }
  *instruction_stream_address = child;
  return true;
}

bool ObjectTranslator::LoadCodeProtectedFieldIfExpectedHeapObject(
    i::Address real_address, i::Tagged<i::Code> code, int offset,
    i::Address* child_address, i::InstanceType* child_type) {
  // Source: V8HeapExplorer::ExtractCodeReferences support-field accessors:
  // bytecode_or_interpreter_data(), deoptimization_data(), and table getters.
  DCHECK_NOT_NULL(child_address);
  DCHECK_NOT_NULL(child_type);
  *child_address = 0;
  *child_type = i::FIRST_TYPE;

#if defined(V8_ENABLE_SANDBOX)
  if (!has_trusted_cage_base_) {
    ReportWarning("sandbox_protected_pointer_missing_trusted_cage_base",
                  "Code protected field requires trusted cage base from "
                  "rawheap v3; skipping protected Code edge for legacy dump",
                  "object_body", real_address, 0,
                  static_cast<uint32_t>(i::CODE_TYPE), 0,
                  static_cast<uint32_t>(offset));
    return false;
  }
#else
  USE(real_address);
#endif

  // Code::bytecode_or_interpreter_data(), deoptimization_data(),
  // bytecode_offset_table(), and source_position_table() all use
  // TrustedObject::ReadProtectedPointerField(), which decodes with
  // TrustedSpaceCompressionScheme rather than the main cage.
  i::Tagged<i::Object> value =
      i::TaggedField<i::Object, 0, i::TrustedSpaceCompressionScheme>::load(
          code, offset);
  if (!i::IsHeapObject(value)) {
    return false;
  }
  i::Address child = i::Cast<i::HeapObject>(value).address();
  // Keep HeapObject/rawheap classification separate from the snapshot-node
  // emission gate: a decoded child can be a real dumped V8 heap object with a
  // valid family yet still be absent from the translated graph. In that case
  // native would skip through GetEntry, and offline translate must not
  // fabricate a native/offheap edge.
  if (FindObjectHead(child) == nullptr) {
    return false;
  }
  if (!GetInstanceType(child, child_type)) {
    return false;
  }
  if (!generator_->GetNode(child)) {
    return false;
  }
  *child_address = child;
  return true;
}

bool ObjectTranslator::LoadCodeBytecodeOrInterpreterData(
    i::Address real_address, i::Tagged<i::Code> code,
    i::Address* child_address) {
  // Code::bytecode_or_interpreter_data() reads the protected
  // kDeoptimizationDataOrInterpreterDataOffset slot for BASELINE code.
  i::InstanceType child_type;
  return LoadCodeProtectedFieldIfExpectedHeapObject(
      real_address, code, i::Code::kDeoptimizationDataOrInterpreterDataOffset,
      child_address, &child_type) &&
         (i::InstanceTypeChecker::IsBytecodeArray(child_type) ||
          child_type == i::INTERPRETER_DATA_TYPE);
}

bool ObjectTranslator::LoadCodeBytecodeOffsetTable(
    i::Address real_address, i::Tagged<i::Code> code,
    i::Address* child_address) {
  // Code::bytecode_offset_table() reads the protected kPositionTableOffset
  // slot for BASELINE code.
  i::InstanceType child_type;
  return LoadCodeProtectedFieldIfExpectedHeapObject(
      real_address, code, i::Code::kPositionTableOffset, child_address,
      &child_type) &&
         child_type == i::TRUSTED_BYTE_ARRAY_TYPE;
}

bool ObjectTranslator::LoadCodeDeoptimizationData(
    i::Address real_address, i::Tagged<i::Code> code,
    i::Address* child_address) {
  // Code::deoptimization_data() reads the protected
  // kDeoptimizationDataOrInterpreterDataOffset slot for optimized code.
  i::InstanceType child_type;
  return LoadCodeProtectedFieldIfExpectedHeapObject(
      real_address, code, i::Code::kDeoptimizationDataOrInterpreterDataOffset,
      child_address, &child_type) &&
         child_type == i::PROTECTED_FIXED_ARRAY_TYPE;
}

bool ObjectTranslator::LoadCodeSourcePositionTable(
    i::Address real_address, i::Tagged<i::Code> code,
    i::Address* child_address) {
  // Code::source_position_table() reads the protected kPositionTableOffset slot
  // for non-baseline code with deoptimization data.
  i::InstanceType child_type;
  return LoadCodeProtectedFieldIfExpectedHeapObject(
      real_address, code, i::Code::kPositionTableOffset, child_address,
      &child_type) &&
         child_type == i::TRUSTED_BYTE_ARRAY_TYPE;
}

void ObjectTranslator::RetagDeoptimizationDataSubtables(
    i::Address deoptimization_data_address) {
  const RawHeapObjectHead* deopt_data_head =
      FindObjectHead(deoptimization_data_address);
  if (deopt_data_head == nullptr) {
    return;
  }
  constexpr int kFixedArrayHeaderSize =
      i::DeoptimizationData::OffsetOfElementAt(0);
  if (deopt_data_head->size_ < static_cast<uint32_t>(kFixedArrayHeaderSize)) {
    return;
  }
  std::unique_ptr<FakeHeapObject> fake_deopt_data =
      MakeFakeHeapObject(deoptimization_data_address);
  if (!fake_deopt_data) {
    return;
  }
  i::Tagged<i::DeoptimizationData> deopt_data =
      i::UncheckedCast<i::DeoptimizationData>(fake_deopt_data->fake_object());
  const int deopt_data_length = deopt_data->length();
  if (deopt_data_length <= i::DeoptimizationData::kFrameTranslationIndex ||
      deopt_data_length >
          (std::numeric_limits<int>::max() - kFixedArrayHeaderSize) /
              i::kTaggedSize) {
    return;
  }
  const int deopt_data_payload_end =
      i::DeoptimizationData::OffsetOfElementAt(deopt_data_length);
  const int frame_translation_offset = i::DeoptimizationData::OffsetOfElementAt(
      i::DeoptimizationData::kFrameTranslationIndex);
  if (deopt_data_payload_end < 0 || frame_translation_offset < 0 ||
      static_cast<uint32_t>(deopt_data_payload_end) > deopt_data_head->size_) {
    return;
  }

  auto retag_deopt_subtable_if_expected = [this, deopt_data, deopt_data_head](
                                              int index,
                                              i::InstanceType expected_type) {
    const int offset = i::DeoptimizationData::OffsetOfElementAt(index);
    if (!TaggedFieldFits(deopt_data_head->size_, offset)) {
      return;
    }
    i::Tagged<i::Object> subtable = deopt_data->get(index);
    if (!i::IsHeapObject(subtable)) {
      return;
    }
    i::Address subtable_address = i::Cast<i::HeapObject>(subtable).address();
    i::InstanceType subtable_type;
    if (FindObjectHead(subtable_address) == nullptr ||
        !GetInstanceType(subtable_address, &subtable_type) ||
        subtable_type != expected_type ||
        generator_->GetNode(subtable_address) == nullptr) {
      return;
    }
    generator_->RetagNodeIfExisting(subtable_address, "(code deopt data)",
                                    Node::Type::kCode, true, true);
  };
  // Native TagObject labels these subtables but does not create
  // Code-to-subtable edges.
  retag_deopt_subtable_if_expected(
      i::DeoptimizationData::kFrameTranslationIndex,
      i::TRUSTED_BYTE_ARRAY_TYPE);
  if (deopt_data_length > i::DeoptimizationData::kLiteralArrayIndex) {
    // DeoptimizationLiteralArray is a TrustedWeakFixedArray subclass with no
    // dedicated instance type.
    retag_deopt_subtable_if_expected(i::DeoptimizationData::kLiteralArrayIndex,
                                     i::TRUSTED_WEAK_FIXED_ARRAY_TYPE);
  }
  if (deopt_data_length > i::DeoptimizationData::kInliningPositionsIndex) {
    // TrustedPodArray<InliningPosition> is TrustedByteArray-castable; native
    // tags it for display only and emits no Code edge.
    retag_deopt_subtable_if_expected(
        i::DeoptimizationData::kInliningPositionsIndex,
        i::TRUSTED_BYTE_ARRAY_TYPE);
  }
}

void ObjectTranslator::ExtractCodeReferences(i::Address real_address,
                                             i::Tagged<i::Code> code) {
  if (!code->has_instruction_stream()) {
    return;
  }

  i::Address instruction_stream_address;
  if (LoadCodeInstructionStream(real_address, code,
                                &instruction_stream_address)) {
    generator_->SetInternalReference(real_address, "instruction_stream",
                                     instruction_stream_address,
                                     i::Code::kInstructionStreamOffset);
  }

  if (code->kind() == i::CodeKind::BASELINE) {
    i::Address interpreter_data_address;
    if (LoadCodeBytecodeOrInterpreterData(real_address, code,
                                          &interpreter_data_address)) {
      // Native TagObject only supplies a display label for this support object;
      // it must not force the BytecodeArray/InterpreterData target into the
      // Code category.
      generator_->RetagNodeIfExisting(interpreter_data_address,
                                      "(interpreter data)", Node::Type::kCode,
                                      false, true);
      generator_->SetInternalReference(
          real_address, "interpreter_data", interpreter_data_address,
          i::Code::kDeoptimizationDataOrInterpreterDataOffset);
    }

    i::Address bytecode_offset_table_address;
    if (LoadCodeBytecodeOffsetTable(real_address, code,
                                    &bytecode_offset_table_address)) {
      generator_->RetagNodeIfExisting(bytecode_offset_table_address,
                                      "(bytecode offset table)",
                                      Node::Type::kCode, true, true);
      generator_->SetInternalReference(real_address, "bytecode_offset_table",
                                       bytecode_offset_table_address,
                                       i::Code::kPositionTableOffset);
    }
  } else if (code->uses_deoptimization_data()) {
    i::Address deoptimization_data_address;
    if (LoadCodeDeoptimizationData(real_address, code,
                                   &deoptimization_data_address)) {
      generator_->RetagNodeIfExisting(deoptimization_data_address,
                                      "(code deopt data)", Node::Type::kCode,
                                      true, true);
      RetagDeoptimizationDataSubtables(deoptimization_data_address);
      generator_->SetInternalReference(
          real_address, "deoptimization_data", deoptimization_data_address,
          i::Code::kDeoptimizationDataOrInterpreterDataOffset);
    }

    i::Address source_position_table_address;
    if (LoadCodeSourcePositionTable(real_address, code,
                                    &source_position_table_address)) {
      generator_->RetagNodeIfExisting(source_position_table_address,
                                      "(source position table)",
                                      Node::Type::kCode, true, true);
      generator_->SetInternalReference(real_address, "source_position_table",
                                       source_position_table_address,
                                       i::Code::kPositionTableOffset);
    }
  }
}

// ExtractCellReferences with real_address parameter
void ObjectTranslator::ExtractCellReferences(i::Address real_address,
                                             i::Tagged<i::Cell> cell) {
  // Reference: heap-snapshot-generator.cc ExtractCellReferences.
  // Cell::maybe_value() is a MaybeObject slot; preserve weak-vs-strong tagging
  // before choosing SetWeakReference or SetInternalReference.
  i::Tagged<i::MaybeObject> maybe_value =
      i::TaggedField<i::MaybeObject, i::Cell::kMaybeValueOffset>::Relaxed_Load(
          cage_base_, cell);

  i::Tagged<i::HeapObject> heap_object;
  if (maybe_value.GetHeapObjectIfStrong(&heap_object)) {
    i::Address child = heap_object.address();
    if (generator_->GetNode(child)) {
      generator_->SetInternalReference(real_address, "value", child,
                                       i::Cell::kMaybeValueOffset);
    }
  } else if (maybe_value.GetHeapObjectIfWeak(&heap_object)) {
    i::Address child = heap_object.address();
    if (generator_->GetNode(child)) {
      generator_->SetWeakReference(real_address, "value", child,
                                   i::Cell::kMaybeValueOffset);
    }
  }
}

// ExtractFeedbackCellReferences with real_address parameter
void ObjectTranslator::ExtractFeedbackCellReferences(
    i::Address real_address, i::InstanceType instance_type,
    i::Tagged<i::FeedbackCell> feedback_cell) {
  if (instance_type == i::FEEDBACK_CELL_TYPE) {
    // Native TagObject(feedback_cell, "(feedback cell)") is label-only:
    // it does not pass HeapEntry::Type and must not override node category.
    generator_->RetagNodeIfExisting(real_address, "(feedback cell)",
                                    Node::Type::kObject,
                                    /*update_type=*/false,
                                    /*overwrite_system_name=*/true);
  }
  SetInternalReferenceIfExistingHeapObject(generator_, real_address, "value",
                                           cage_base_, feedback_cell,
                                           i::FeedbackCell::kValueOffset);
}

bool ObjectTranslator::LoadPropertyCellDependentCode(
    i::Address real_address, i::Tagged<i::PropertyCell> cell,
    i::Address* dependent_code_address, i::InstanceType* dependent_code_type) {
  USE(real_address);
  DCHECK_NOT_NULL(dependent_code_address);
  DCHECK_NOT_NULL(dependent_code_type);
  *dependent_code_address = 0;
  *dependent_code_type = i::FIRST_TYPE;

  // PropertyCell::dependent_code() is an ordinary tagged field. DependentCode
  // is represented as WeakArrayList in this branch; return the type to keep the
  // ExtractPropertyCellReferences control flow close to the native extractor.
  if (!LoadHeapObjectAddress(cage_base_, cell,
                             i::PropertyCell::kDependentCodeOffset,
                             dependent_code_address)) {
    return false;
  }
  if (FindObjectHead(*dependent_code_address) == nullptr) {
    return false;
  }
  if (!GetInstanceType(*dependent_code_address, dependent_code_type)) {
    return false;
  }
  return generator_->GetNode(*dependent_code_address) != nullptr;
}

// ExtractPropertyCellReferences with real_address parameter
void ObjectTranslator::ExtractPropertyCellReferences(
    i::Address real_address, i::Tagged<i::PropertyCell> cell) {
  // Reference: heap-snapshot-generator.cc ExtractPropertyCellReferences.
  SetInternalReferenceIfExistingHeapObject(generator_, real_address, "value",
                                           cage_base_, cell,
                                           i::PropertyCell::kValueOffset);

  i::Address dependent_code_address;
  i::InstanceType dependent_code_type;
  if (!LoadPropertyCellDependentCode(real_address, cell,
                                     &dependent_code_address,
                                     &dependent_code_type)) {
    return;
  }
  if (dependent_code_type != i::WEAK_ARRAY_LIST_TYPE) {
    return;
  }

  // Native TagObject(cell->dependent_code(), "(dependent code)") is
  // label-only; DependentCode is currently represented as WeakArrayList, so
  // keep the original node category and only retag an already dumped node.
  generator_->RetagNodeIfExisting(dependent_code_address, "(dependent code)",
                                  Node::Type::kHidden,
                                  /*update_type=*/false,
                                  /*overwrite_system_name=*/true);
  generator_->SetInternalReference(real_address, "dependent_code",
                                   dependent_code_address,
                                   i::PropertyCell::kDependentCodeOffset);
}

void ObjectTranslator::ExtractWeakCellReferences(
    i::Address real_address, i::Tagged<i::WeakCell> weak_cell) {
  SetWeakReferenceIfExistingHeapObject(generator_, real_address, "target",
                                       cage_base_, weak_cell,
                                       offsetof(i::WeakCell, target_));
  SetWeakReferenceIfExistingHeapObject(
      generator_, real_address, "unregister_token", cage_base_, weak_cell,
      offsetof(i::WeakCell, unregister_token_));
}

// ExtractPrototypeInfoReferences with real_address parameter
void ObjectTranslator::ExtractPrototypeInfoReferences(
    i::Address real_address, i::Tagged<i::PrototypeInfo> info) {
  i::Address enum_cache_address;
  if (LoadHeapObjectAddress(cage_base_, info,
                            i::PrototypeInfo::kPrototypeChainEnumCacheOffset,
                            &enum_cache_address) &&
      FindObjectHead(enum_cache_address) != nullptr) {
    i::InstanceType enum_cache_type;
    if (GetInstanceType(enum_cache_address, &enum_cache_type) &&
        i::InstanceTypeChecker::IsFixedArray(enum_cache_type) &&
        generator_->GetNode(enum_cache_address) != nullptr) {
      // Native only TagObject-labels this child as ObjectShape. It does not
      // mark the field visited or synthesize a named edge from PrototypeInfo.
      generator_->RetagNodeIfExisting(enum_cache_address,
                                      "(prototype chain enum cache)",
                                      Node::Type::kObjectShape,
                                      /*update_type=*/true,
                                      /*overwrite_system_name=*/true);
    }
  }

  i::Address prototype_users_address;
  if (LoadHeapObjectAddress(cage_base_, info,
                            i::PrototypeInfo::kPrototypeUsersOffset,
                            &prototype_users_address) &&
      FindObjectHead(prototype_users_address) != nullptr) {
    i::InstanceType prototype_users_type;
    if (GetInstanceType(prototype_users_address, &prototype_users_type) &&
        prototype_users_type == i::WEAK_ARRAY_LIST_TYPE &&
        generator_->GetNode(prototype_users_address) != nullptr) {
      // Torque defines this field as WeakArrayList|Zero. Only a proven dumped
      // WeakArrayList receives the native fixed TagObject label/category.
      generator_->RetagNodeIfExisting(prototype_users_address,
                                      "(prototype users)",
                                      Node::Type::kObjectShape,
                                      /*update_type=*/true,
                                      /*overwrite_system_name=*/true);
    }
  }
  (void)real_address;
}

void ObjectTranslator::ExtractAllocationSiteReferences(
    i::Address real_address, i::Tagged<i::AllocationSite> site) {
  // Reference: heap-snapshot-generator.cc ExtractAllocationSiteReferences.
  SetInternalReferenceIfExistingHeapObject(
      generator_, real_address, "transition_info", cage_base_, site,
      offsetof(i::AllocationSite, transition_info_or_boilerplate_));
  SetInternalReferenceIfExistingHeapObject(
      generator_, real_address, "nested_site", cage_base_, site,
      offsetof(i::AllocationSite, nested_site_));
  SetInternalReferenceIfExistingHeapObject(
      generator_, real_address, "dependent_code", cage_base_, site,
      offsetof(i::AllocationSite, dependent_code_));

  i::Address dependent_code_address;
  if (!LoadHeapObjectAddress(cage_base_, site,
                             offsetof(i::AllocationSite, dependent_code_),
                             &dependent_code_address)) {
    return;
  }
  if (FindObjectHead(dependent_code_address) == nullptr) {
    return;
  }
  i::InstanceType dependent_code_type;
  if (!GetInstanceType(dependent_code_address, &dependent_code_type) ||
      dependent_code_type != i::WEAK_ARRAY_LIST_TYPE) {
    return;
  }
  if (generator_->GetNode(dependent_code_address) == nullptr) {
    return;
  }

  // Native TagObject(site->dependent_code(), "(dependent code)", kCode)
  // retags only an existing DependentCode/WeakArrayList node; the real
  // "dependent_code" edge above remains independent.
  generator_->RetagNodeIfExisting(dependent_code_address, "(dependent code)",
                                  Node::Type::kCode,
                                  /*update_type=*/true,
                                  /*overwrite_system_name=*/true);
}

// ExtractArrayBoilerplateDescriptionReferences with real_address parameter
void ObjectTranslator::ExtractArrayBoilerplateDescriptionReferences(
    i::Address real_address, i::Tagged<i::ArrayBoilerplateDescription> value) {
  // constant_elements is an ordinary dumped tagged field. Skip if the child was
  // not materialized as a snapshot node, matching native non-entry behavior.
  constexpr int kConstantElementsOffset =
      offsetof(i::ArrayBoilerplateDescription, constant_elements_);
  i::Address constant_elements_address = i::kNullAddress;
  SetInternalReferenceIfExistingHeapObject(generator_, real_address,
                                           "constant_elements", cage_base_,
                                           value, kConstantElementsOffset);
  if (!LoadHeapObjectAddress(cage_base_, value, kConstantElementsOffset,
                             &constant_elements_address) ||
      generator_->GetNode(constant_elements_address) == nullptr) {
    return;
  }
  // Native TagObject(constant_elements, "(constant elements)", kCode) retags
  // only the existing child node. Do not emit an extra edge for this metadata.
  generator_->RetagNodeIfExisting(constant_elements_address,
                                  "(constant elements)", Node::Type::kCode,
                                  /*update_type=*/true,
                                  /*overwrite_system_name=*/true);
}

void ObjectTranslator::ExtractRegExpBoilerplateDescriptionReferences(
    i::Tagged<i::RegExpBoilerplateDescription> value) {
  // Native V8HeapExplorer::ExtractRegExpBoilerplateDescriptionReferences is
  // tag-only: it retags value->data(isolate()) as code metadata and emits no
  // named edge. BodyDescriptor traversal owns the hidden source_/data_ edges.
  static constexpr size_t kDataOffset = sizeof(i::StructLayout);
  TrustedPointerField trusted_field(offheap_table_resolver_);
  OfflineIndirectPointerLoadResult result;
  if (trusted_field.ReadTrustedPointerField(
          value, kDataOffset, i::kRegExpDataIndirectPointerTag, &result) !=
      OfflineIndirectPointerLoadStatus::kOk) {
    return;
  }

  i::Address data_address = result.target_address_;
  if (FindObjectHead(data_address) == nullptr &&
      (data_address & i::kHeapObjectTag) == i::kHeapObjectTag) {
    const i::Address untagged_data_address = data_address - i::kHeapObjectTag;
    if (FindObjectHead(untagged_data_address) != nullptr) {
      data_address = untagged_data_address;
    }
  }
  if (FindObjectHead(data_address) == nullptr) {
    return;
  }

  i::InstanceType data_type;
  if (!GetInstanceType(data_address, &data_type) ||
      !IsRegExpDataInstanceType(data_type)) {
    return;
  }
  generator_->RetagNodeIfExisting(data_address, "(RegExpData)",
                                  Node::Type::kCode,
                                  /*update_type=*/true,
                                  /*overwrite_system_name=*/false);
}

// ExtractFeedbackVectorReferences with real_address parameter
void ObjectTranslator::ExtractFeedbackVectorReferences(
    i::Address real_address, i::Tagged<i::FeedbackVector> feedback_vector) {
  // Native ExtractFeedbackVectorReferences only retags selected strong slot
  // targets as "(feedback)"/kCode; it does not emit object-specific edges or
  // mark visited fields. Preserve that shape here: read dumped slots, prove the
  // child is a dumped FixedArray/WeakFixedArray node, then retag only.
  constexpr int kSlotsOffset = i::FeedbackVector::kRawFeedbackSlotsOffset;
  const RawHeapObjectHead* head = FindObjectHead(real_address);
  const int length = feedback_vector->length();
  if (head == nullptr || length < 0 ||
      head->size_ < static_cast<uint32_t>(kSlotsOffset)) {
    return;
  }
  const uint32_t max_slots =
      (head->size_ - static_cast<uint32_t>(kSlotsOffset)) / i::kTaggedSize;
  if (static_cast<uint32_t>(length) > max_slots) {
    ReportCorrupt("feedback_vector_slots_out_of_bounds",
                  "FeedbackVector length exceeds dumped object payload",
                  "object_references", real_address);
    return;
  }

  for (int i = 0; i < length; ++i) {
    i::Tagged<i::MaybeObject> maybe_entry =
        *(feedback_vector->slots_start() + i);
    i::Tagged<i::HeapObject> entry_obj;
    if (!maybe_entry.GetHeapObjectIfStrong(&entry_obj)) {
      continue;
    }

    i::Address entry_address = entry_obj.address();
    if (FindObjectHead(entry_address) == nullptr ||
        generator_->GetNode(entry_address) == nullptr) {
      continue;
    }

    i::InstanceType entry_type;
    if (!GetInstanceType(entry_address, &entry_type) ||
        (entry_type != i::WEAK_FIXED_ARRAY_TYPE &&
         entry_type != i::FIXED_ARRAY_TYPE)) {
      continue;
    }

    generator_->RetagNodeIfExisting(entry_address, "(feedback)",
                                    Node::Type::kCode,
                                    /*update_type=*/true,
                                    /*overwrite_system_name=*/true);
  }
}

// ExtractDescriptorArrayReferences with real_address parameter
void ObjectTranslator::ExtractDescriptorArrayReferences(
    i::Address real_address, i::Tagged<i::DescriptorArray> array) {
  // Skip enum_cache edge when the child has no translated snapshot node.
  SetInternalReferenceIfExistingHeapObject(
      generator_, real_address, "enum_cache", cage_base_, array,
      i::DescriptorArray::kEnumCacheOffset);

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
      if (!generator_->GetNode(heap_object.address())) continue;
      generator_->SetWeakReference(real_address, i, heap_object.address(),
                                   offset);
    } else if (object.GetHeapObjectIfStrong(&heap_object)) {
      if (!generator_->GetNode(heap_object.address())) continue;
      generator_->SetInternalReference(real_address, std::to_string(i),
                                       heap_object.address(), offset);
    }
  }
}

// ExtractEnumCacheReferences with real_address parameter
void ObjectTranslator::ExtractEnumCacheReferences(
    i::Address real_address, i::Tagged<i::EnumCache> cache) {
  auto retag_fixed_array_child = [&](i::Tagged<i::FixedArray> child) {
    i::Address child_address = child.address();
    if (FindObjectHead(child_address) == nullptr) {
      return;
    }
    i::InstanceType child_type;
    if (!GetInstanceType(child_address, &child_type) ||
        !i::InstanceTypeChecker::IsFixedArray(child_type) ||
        generator_->GetNode(child_address) == nullptr) {
      return;
    }
    // Native TagObject mutates an already-created target node only; EnumCache
    // keys/indices are fixed FixedArray fields and do not create helper edges.
    generator_->RetagNodeIfExisting(child_address, "(enum cache)",
                                    Node::Type::kObjectShape,
                                    /*update_type=*/true,
                                    /*overwrite_system_name=*/true);
  };
  retag_fixed_array_child(cache->keys());
  retag_fixed_array_child(cache->indices());
  (void)real_address;
}

// ExtractTransitionArrayReferences with real_address parameter
void ObjectTranslator::ExtractTransitionArrayReferences(
    i::Address real_address, i::Tagged<i::TransitionArray> transitions) {
  const int offset = i::WeakFixedArray::OffsetOfElementAt(
      i::TransitionArray::kPrototypeTransitionsIndex);
  i::Tagged<i::Object> value = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, transitions, offset);
  if (!i::IsHeapObject(value)) {
    return;
  }

  i::Address prototype_transitions_address =
      i::Cast<i::HeapObject>(value).address();
  if (FindObjectHead(prototype_transitions_address) == nullptr) {
    return;
  }

  i::InstanceType prototype_transitions_type;
  if (!GetInstanceType(prototype_transitions_address,
                       &prototype_transitions_type) ||
      prototype_transitions_type != i::WEAK_FIXED_ARRAY_TYPE ||
      generator_->GetNode(prototype_transitions_address) == nullptr) {
    return;
  }

  // Native TagObject only retags the existing WeakFixedArray node. It does not
  // mark the field visited, so the generic body visitor still emits the
  // structural indexed edge for this slot.
  generator_->RetagNodeIfExisting(prototype_transitions_address,
                                  "(prototype transitions)",
                                  Node::Type::kObjectShape,
                                  /*update_type=*/true,
                                  /*overwrite_system_name=*/true);
  (void)real_address;
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
      if (!generator_->GetNode(heap_object.address())) continue;
      generator_->SetWeakReference(real_address, i, heap_object.address(),
                                   offset);
    } else if (object.GetHeapObjectIfStrong(&heap_object)) {
      if (!generator_->GetNode(heap_object.address())) continue;
      generator_->SetInternalReference(real_address, std::to_string(i),
                                       heap_object.address(), offset);
    }
  }
}

bool ObjectTranslator::LoadEphemeronHashTableLengthIfValid(
    i::Address real_address, i::Tagged<i::EphemeronHashTable> table,
    int* length) {
  DCHECK_NOT_NULL(length);
  *length = 0;
  auto report = [&](const char* code, const char* message,
                    uint32_t rawheap_offset = 0) {
    ReportCorrupt(code, message, "object_body", real_address, 0, 0, 0,
                  rawheap_offset);
  };
  const RawHeapObjectHead* object_head = FindObjectHead(real_address);
  if (object_head == nullptr) {
    report("ephemeron_hash_table_object_missing",
           "EphemeronHashTable object was not present in rawheap");
    return false;
  }

  constexpr int kFixedArrayHeaderSize = i::FixedArray::OffsetOfElementAt(0);
  if (object_head->size_ < static_cast<uint32_t>(kFixedArrayHeaderSize)) {
    report("ephemeron_hash_table_header_out_of_range",
           "EphemeronHashTable header is outside the dumped object payload",
           object_head->offset_);
    return false;
  }
  const int table_length = table->length();
  if (table_length < i::EphemeronHashTable::kElementsStartIndex ||
      table_length > (std::numeric_limits<int>::max() - kFixedArrayHeaderSize) /
                         i::kTaggedSize) {
    report("ephemeron_hash_table_length_invalid",
           "EphemeronHashTable length is invalid", object_head->offset_);
    return false;
  }
  const int elements_end = i::FixedArray::OffsetOfElementAt(table_length);
  if (elements_end < 0 ||
      static_cast<uint32_t>(elements_end) > object_head->size_) {
    report("ephemeron_hash_table_elements_out_of_range",
           "EphemeronHashTable elements are outside the dumped object payload",
           object_head->offset_);
    return false;
  }

  *length = table_length;
  return true;
}

bool ObjectTranslator::GetEphemeronHashTableElement(
    i::Tagged<i::EphemeronHashTable> table, int index,
    i::Address* element_address) {
  DCHECK_NOT_NULL(element_address);
  *element_address = 0;

  if (!LoadEphemeronHashTableElementAddress(cage_base_, table, index,
                                            element_address)) {
    return false;
  }

  const RawHeapObjectHead* element_head = FindObjectHead(*element_address);
  if (element_head == nullptr) {
    return false;
  }

  // PreVisit creates all nodes before reference extraction. Once object-table
  // membership is proven, missing node state is an internal translator bug.
  CHECK_NOT_NULL(generator_->GetNode(*element_address));
  return true;
}

void ObjectTranslator::ExtractEphemeronHashTableReferences(
    i::Address real_address, i::Tagged<i::EphemeronHashTable> table) {
  int length;
  if (!LoadEphemeronHashTableLengthIfValid(real_address, table, &length)) {
    return;
  }

  Node* table_node = generator_->GetNode(real_address);
  CHECK_NOT_NULL(table_node);
  for (i::InternalIndex entry : table->IterateEntries()) {
    const int key_index = i::EphemeronHashTable::EntryToIndex(entry) +
                          i::EphemeronHashTable::kEntryKeyIndex;
    const int value_index = i::EphemeronHashTable::EntryToValueIndex(entry);
    if (key_index < 0 || value_index < 0 || key_index >= length ||
        value_index >= length) {
      ReportCorrupt(
          "ephemeron_hash_table_entry_out_of_range",
          "EphemeronHashTable entry index is outside the dumped object payload",
          "object_body", real_address);
      return;
    }

    const int key_offset = i::EphemeronHashTable::OffsetOfElementAt(key_index);
    const int value_offset =
        i::EphemeronHashTable::OffsetOfElementAt(value_index);

    // Mirrors heap-snapshot-generator.cc:
    //   key = table->get(key_index); value = table->get(value_index);
    //   SetWeakReference(... key_index/key ...);
    //   SetWeakReference(... value_index/value ...);
    //   key_entry/value_entry/table_entry gate ephemeron annotations.
    i::Address key_address = 0;
    const bool has_key =
        GetEphemeronHashTableElement(table, key_index, &key_address);
    if (has_key) {
      generator_->SetWeakReference(real_address, key_index, key_address,
                                   key_offset);
    }

    i::Address value_address = 0;
    const bool has_value =
        GetEphemeronHashTableElement(table, value_index, &value_address);
    if (has_value) {
      generator_->SetWeakReference(real_address, value_index, value_address,
                                   value_offset);
    }

    if (!has_key || !has_value) {
      continue;
    }
    Node* key_node = generator_->GetNode(key_address);
    Node* value_node = generator_->GetNode(value_address);
    CHECK_NOT_NULL(key_node);
    CHECK_NOT_NULL(value_node);
    if (IsUndefinedOddballOffline(key_address)) {
      continue;
    }

    // Native heap snapshots add synthetic ephemeron annotations in addition to
    // the weak table slots. They are real V8 heap-object edges only after all
    // participating dumped objects already have snapshot nodes; the -1 offset
    // deliberately avoids marking any key/value table slot as visited twice.
    std::string annotation_name = FormatEphemeronAnnotationName(
        generator_, key_node, value_node, table_node);
    generator_->SetInternalReference(key_address, annotation_name,
                                     value_address, -1);
    generator_->SetInternalReference(real_address, annotation_name,
                                     value_address, -1);
  }
}

bool ObjectTranslator::LoadFixedArrayLengthIfValid(
    i::Address real_address, i::Tagged<i::FixedArray> array, int* length) {
  DCHECK_NOT_NULL(length);
  *length = 0;
  auto report = [&](const char* code, const char* message,
                    uint32_t rawheap_offset = 0) {
    ReportCorrupt(code, message, "object_body", real_address, 0, 0, 0,
                  rawheap_offset);
  };
  const RawHeapObjectHead* object_head = FindObjectHead(real_address);
  if (object_head == nullptr) {
    report("fixed_array_object_missing",
           "FixedArray object was not present in rawheap");
    return false;
  }

  constexpr int kFixedArrayHeaderSize = i::FixedArray::OffsetOfElementAt(0);
  if (object_head->size_ < static_cast<uint32_t>(kFixedArrayHeaderSize)) {
    report("fixed_array_header_out_of_range",
           "FixedArray header is outside the dumped object payload",
           object_head->offset_);
    return false;
  }
  const int array_length = array->length();
  if (array_length < 0 ||
      array_length > (std::numeric_limits<int>::max() - kFixedArrayHeaderSize) /
                   i::kTaggedSize) {
    report("fixed_array_length_invalid", "FixedArray length is invalid",
           object_head->offset_);
    return false;
  }
  const int elements_end = i::FixedArray::OffsetOfElementAt(array_length);
  if (elements_end < 0 ||
      static_cast<uint32_t>(elements_end) > object_head->size_) {
    report("fixed_array_elements_out_of_range",
           "FixedArray elements are outside the dumped object payload",
           object_head->offset_);
    return false;
  }

  *length = array_length;
  return true;
}

void ObjectTranslator::ExtractFixedArrayReferences(
    i::Address real_address, i::Tagged<i::FixedArray> array) {
  int length = 0;
  if (!LoadFixedArrayLengthIfValid(real_address, array, &length)) {
    return;
  }

  // Mirrors V8HeapExplorer::ExtractFixedArrayReferences:
  // SetInternalReference(entry, i, array->get(i), OffsetOfElementAt(i)).
  for (int index = 0; index < length; ++index) {
    i::Address child = 0;
    if (!GetFixedArrayElementAddress(cage_base_, array, index, &child) ||
        !generator_->GetNode(child)) {
      continue;
    }
    generator_->SetInternalReference(real_address, std::to_string(index), child,
                                     i::FixedArray::OffsetOfElementAt(index));
  }
}

bool ObjectTranslator::LoadScriptLineEnds(i::Address real_address,
                                          i::Tagged<i::Script> script,
                                          i::Address* line_ends_address) {
  USE(real_address);
  DCHECK_NOT_NULL(line_ends_address);
  *line_ends_address = 0;

  // Native ExtractScriptReferences calls script->line_ends(), whose Torque
  // accessor reads this slot and DCHECKs Smi|FixedArray. Only the FixedArray
  // case can produce a snapshot edge/tag.
  i::Tagged<i::Object> value = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, script, i::Script::kLineEndsOffset);
  if (!i::IsHeapObject(value)) {
    return false;
  }

  i::Address address = i::Cast<i::HeapObject>(value).address();
  i::InstanceType type;
  if (!GetInstanceType(address, &type) || type != i::FIXED_ARRAY_TYPE) {
    return false;
  }
  *line_ends_address = address;
  return true;
}

bool ObjectTranslator::LoadScriptHostDefinedOptions(
    i::Address real_address, i::Tagged<i::Script> script,
    i::Address* host_defined_options_address) {
  USE(real_address);
  DCHECK_NOT_NULL(host_defined_options_address);
  *host_defined_options_address = 0;

  // Native calls script->host_defined_options(); the Torque accessor is a raw
  // slot load with a FixedArray DCHECK. Gate the offline value the same way
  // before any retagging.
  i::Address address;
  if (!LoadHeapObjectAddress(cage_base_, script,
                             i::Script::kHostDefinedOptionsOffset, &address)) {
    return false;
  }
  i::InstanceType type;
  if (!GetInstanceType(address, &type) || type != i::FIXED_ARRAY_TYPE) {
    return false;
  }
  *host_defined_options_address = address;
  return true;
}

// ExtractScriptReferences with real_address parameter
void ObjectTranslator::ExtractScriptReferences(i::Address real_address,
                                               i::Tagged<i::Script> script,
                                               i::Tagged<i::Map> map) {
  // Reference: heap-snapshot-generator.cc ExtractScriptReferences

  // source field (String) - note: string data may not exist, only map field
  SetInternalReferenceIfExistingHeapObject(generator_, real_address, "source",
                                           cage_base_, script,
                                           i::Script::kSourceOffset);

  // name field (Object - typically String or undefined)
  SetInternalReferenceIfExistingHeapObject(generator_, real_address, "name",
                                           cage_base_, script,
                                           i::Script::kNameOffset);

  // context_data field
  SetInternalReferenceIfExistingHeapObject(generator_, real_address,
                                           "context_data", cage_base_, script,
                                           i::Script::kContextDataOffset);

  i::Address line_ends_address;
  if (LoadScriptLineEnds(real_address, script, &line_ends_address) &&
      generator_->GetNode(line_ends_address) != nullptr) {
    generator_->SetInternalReference(real_address, "line_ends",
                                     line_ends_address,
                                     i::Script::kLineEndsOffset);
    // Native TagObject changes the existing FixedArray node metadata, while
    // the real Script -> line_ends internal edge above remains independent.
    generator_->RetagNodeIfExisting(line_ends_address, "(script line ends)",
                                    Node::Type::kCode,
                                    /*update_type=*/true,
                                    /*overwrite_system_name=*/true);
  }

  // TODO(heapdump): Native TagObject(script->infos(), "(infos)", kCode)
  // calls the hand-written Script::infos() getter. For Wasm scripts that getter
  // returns ReadOnlyRoots::empty_weak_fixed_array(), whose dump-time address is
  // not recoverable from the current rawheap format. Do not approximate it by
  // scanning for an empty WEAK_FIXED_ARRAY_TYPE object; that can retag the
  // wrong dumped object. Revisit after the dump preserves root-table
  // RootIndex -> address mapping or records this root during previsit.

  // Native TagObject(script->host_defined_options(),
  // "(host-defined options)", kCode) is also unconditional and only retags;
  // it does not mark the slot visited or create a named Script edge.
  i::Address host_defined_options_address;
  if (LoadScriptHostDefinedOptions(real_address, script,
                                   &host_defined_options_address) &&
      generator_->GetNode(host_defined_options_address) != nullptr) {
    generator_->RetagNodeIfExisting(host_defined_options_address,
                                    "(host-defined options)", Node::Type::kCode,
                                    /*update_type=*/true,
                                    /*overwrite_system_name=*/true);
  }

#if V8_ENABLE_WEBASSEMBLY
  // Wasm reuses some otherwise unused fields for wasm-specific information.
  if (script->type() == i::Script::Type::kWasm) {
    SetWasmInternalReferenceIfExistingHeapObject(
        real_address, "wasm_breakpoint_infos", i::Cast<i::HeapObject>(script),
        i::Script::kEvalFromSharedOrWrappedArgumentsOffset);
    SetWasmInternalReferenceIfExistingHeapObject(
        real_address, "wasm_managed_native_module",
        i::Cast<i::HeapObject>(script), i::Script::kEvalFromPositionOffset);
    SetWasmInternalReferenceIfExistingHeapObject(
        real_address, "wasm_weak_instance_list",
        i::Cast<i::HeapObject>(script),
        i::Script::kInfosOffset);
  }
#endif
}

// ExtractAccessorInfoReferences
// Reference: heap-snapshot-generator.cc ExtractAccessorInfoReferences
void ObjectTranslator::ExtractAccessorInfoReferences(
    i::Address real_address, i::Tagged<i::AccessorInfo> accessor_info) {
  // name field
  SetInternalReferenceIfExistingHeapObject(generator_, real_address, "name",
                                           cage_base_, accessor_info,
                                           i::AccessorInfo::kNameOffset);
  // data field
  SetInternalReferenceIfExistingHeapObject(generator_, real_address, "data",
                                           cage_base_, accessor_info,
                                           i::AccessorInfo::kDataOffset);
}

// ExtractAccessorPairReferences
// Reference: heap-snapshot-generator.cc ExtractAccessorPairReferences
void ObjectTranslator::ExtractAccessorPairReferences(
    i::Address real_address, i::Tagged<i::AccessorPair> accessors) {
  // getter field
  SetInternalReferenceIfExistingHeapObject(generator_, real_address, "getter",
                                           cage_base_, accessors,
                                           offsetof(i::AccessorPair, getter_));

  // setter field
  SetInternalReferenceIfExistingHeapObject(generator_, real_address, "setter",
                                           cage_base_, accessors,
                                           offsetof(i::AccessorPair, setter_));
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

/////////////// helper
bool ObjectTranslator::GetInstanceType(i::Address real_address,
                                       i::InstanceType* instance_type) {
  if (instance_type == nullptr) {
    return false;
  }
  std::unique_ptr<FakeHeapObject> fake_ho = MakeFakeHeapObject(real_address);
  if (!fake_ho) {
    ReportCorrupt("instance_type_object_missing",
                  "object for instance type lookup was not present in rawheap",
                  "object_body", real_address);
    return false;
  }
  i::Tagged<i::HeapObject> heap_object = fake_ho->fake_object();

  i::Address map_address = heap_object->map(cage_base_).address();
  std::unique_ptr<FakeHeapObject> fake_map = MakeFakeHeapObject(map_address);
  if (!fake_map) {
    ReportCorrupt("instance_type_map_missing",
                  "object map for instance type lookup was not present in "
                  "rawheap",
                  "object_body", real_address, map_address);
    return false;
  }
  i::Tagged<i::Map> map = i::UncheckedCast<i::Map>(fake_map->fake_object());
  *instance_type = map->instance_type();
  return true;
}

}  // namespace dfx
}  // namespace v8
#endif
