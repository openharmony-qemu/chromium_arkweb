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

// This is a copy of heap-visitor-inl.h with modifications for DFX heap
// translate.

#ifndef DFX_HEAP_VISITOR_INL_H_
#define DFX_HEAP_VISITOR_INL_H_

#include <optional>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/dfx-heap-visitor.h"
#include "src/base/logging.h"
#include "src/execution/local-isolate.h"
#include "src/heap/heap-layout-inl.h"
#include "src/heap/mark-compact.h"
#include "src/heap/object-lock-inl.h"
#include "src/objects/arguments.h"
#include "src/objects/data-handler-inl.h"
#include "src/objects/free-space-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-objects.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/map.h"
#include "src/objects/module-inl.h"
#include "src/objects/objects-body-descriptors-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/ordered-hash-table.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/synthetic-module-inl.h"
#include "src/objects/torque-defined-classes.h"
#include "src/objects/visitors.h"

namespace v8 {
namespace dfx {

template <typename ConcreteVisitor>
DfxHeapVisitor<ConcreteVisitor>::DfxHeapVisitor(
    i::PtrComprCageBase cage_base, i::PtrComprCageBase code_cage_base)
    : i::ObjectVisitorWithCageBases(cage_base, code_cage_base) {}

template <typename ConcreteVisitor>
template <typename T>
i::Tagged<T> DfxHeapVisitor<ConcreteVisitor>::Cast(
    i::Tagged<i::HeapObject> object, const i::Heap* heap) {
  if constexpr (ConcreteVisitor::ShouldUseUncheckedCast()) {
    return i::UncheckedCast<T>(object);
  }
  return i::TrustedCast<T>(object);
}

template <typename ConcreteVisitor>
size_t DfxHeapVisitor<ConcreteVisitor>::Visit(i::Tagged<i::HeapObject> object)
  requires(!ConcreteVisitor::UsePrecomputedObjectSize())
{
  // For translate, we assume the map is already available from the caller
  // The original was: return Visit(object->map(cage_base()), object);
  // But for translate, we always call Visit(Map, HeapObject) directly
  UNREACHABLE();
}

template <typename ConcreteVisitor>
size_t DfxHeapVisitor<ConcreteVisitor>::Visit(i::Tagged<i::Map> map,
                                              i::Tagged<i::HeapObject> object)
  requires(!ConcreteVisitor::UsePrecomputedObjectSize())
{
  return Visit(map, object, DfxMaybeObjectSize());
}

template <typename ConcreteVisitor>
size_t DfxHeapVisitor<ConcreteVisitor>::Visit(i::Tagged<i::Map> map,
                                              i::Tagged<i::HeapObject> object,
                                              int object_size)
  requires(ConcreteVisitor::UsePrecomputedObjectSize())
{
  return Visit(map, object, DfxMaybeObjectSize(object_size));
}

template <typename ConcreteVisitor>
size_t DfxHeapVisitor<ConcreteVisitor>::Visit(i::Tagged<i::Map> map,
                                              i::Tagged<i::HeapObject> object,
                                              i::SafeHeapObjectSize object_size)
  requires(ConcreteVisitor::UsePrecomputedObjectSize())
{
  return Visit(map, object, DfxMaybeObjectSize(object_size));
}

template <typename ConcreteVisitor>
size_t DfxHeapVisitor<ConcreteVisitor>::Visit(
    i::Tagged<i::Map> map, i::Tagged<i::HeapObject> object,
    DfxMaybeObjectSize maybe_object_size) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  i::VisitorId id = map->visitor_id();
  switch (id) {
#define CASE(TypeName)                                                     \
  case i::VisitorId::kVisit##TypeName:                                     \
    return visitor->Visit##TypeName(                                       \
        map, ConcreteVisitor::template Cast<i::TypeName>(object, nullptr), \
        maybe_object_size);
    DFX_TYPED_VISITOR_ID_LIST(CASE)
    DFX_TYPED_VISITOR_WITH_SLACK_ID_LIST(CASE)
    DFX_TORQUE_VISITOR_ID_LIST(CASE)
#undef CASE
#define CASE(TypeName)                                                     \
  case i::VisitorId::kVisit##TypeName:                                     \
    return visitor->Visit##TypeName(                                       \
        map, ConcreteVisitor::template Cast<i::TypeName>(object, nullptr), \
        maybe_object_size);
    DFX_TRUSTED_VISITOR_ID_LIST(CASE)
#undef CASE
    case i::VisitorId::kVisitShortcutCandidate:
      return visitor->VisitShortcutCandidate(
          map, ConcreteVisitor::template Cast<i::ConsString>(object, nullptr),
          maybe_object_size);
    case i::VisitorId::kVisitJSObjectFast:
      return visitor->VisitJSObjectFast(
          map, ConcreteVisitor::template Cast<i::JSObject>(object, nullptr),
          maybe_object_size);
    case i::VisitorId::kVisitJSApiObject:
      return visitor->VisitJSApiObject(
          map, ConcreteVisitor::template Cast<i::JSObject>(object, nullptr),
          maybe_object_size);
    case i::VisitorId::kVisitCppHeapExternalObject:
      return visitor->VisitCppHeapExternalObject(
          map,
          ConcreteVisitor::template Cast<i::CppHeapExternalObject>(object,
                                                                   nullptr),
          maybe_object_size);
    case i::VisitorId::kVisitStruct:
      return visitor->VisitStruct(map, object, maybe_object_size);
    case i::VisitorId::kVisitFiller:
      return visitor->VisitFiller(map, object, maybe_object_size);
    case i::VisitorId::kVisitFreeSpace:
      return visitor->VisitFreeSpace(map, i::Cast<i::FreeSpace>(object),
                                     maybe_object_size);
    case i::VisitorId::kDataOnlyVisitorIdCount:
    case i::VisitorId::kVisitorIdCount:
    default:
      // NIY:For visitor IDs not in DFX list

      UNREACHABLE();
  }
}

template <typename ConcreteVisitor>
template <i::VisitorId visitor_id>
void DfxHeapVisitor<ConcreteVisitor>::VisitMapPointerIfNeeded(
    i::Tagged<i::HeapObject> host) {
  if constexpr (!ConcreteVisitor::ShouldVisitMapPointer()) {
    return;
  }
  static_cast<ConcreteVisitor*>(this)->VisitMapPointer(host);
}

#define VISIT(TypeName)                                                       \
  template <typename ConcreteVisitor>                                         \
  size_t DfxHeapVisitor<ConcreteVisitor>::Visit##TypeName(                    \
      i::Tagged<i::Map> map, i::Tagged<i::TypeName> object,                   \
      DfxMaybeObjectSize maybe_object_size) {                                 \
    return static_cast<ConcreteVisitor*>(this)                                \
        ->template VisitWithBodyDescriptor<                                   \
            i::VisitorId::kVisit##TypeName, i::TypeName,                      \
            i::ObjectTraits<i::TypeName>::BodyDescriptor>(map, object,        \
                                                          maybe_object_size); \
  }

DFX_TYPED_VISITOR_ID_LIST(VISIT)
DFX_TORQUE_VISITOR_ID_LIST(VISIT)
DFX_TRUSTED_VISITOR_ID_LIST(VISIT)
#undef VISIT

#define VISIT_WITH_SLACK(TypeName)                                     \
  template <typename ConcreteVisitor>                                  \
  size_t DfxHeapVisitor<ConcreteVisitor>::Visit##TypeName(             \
      i::Tagged<i::Map> map, i::Tagged<i::TypeName> object,            \
      DfxMaybeObjectSize maybe_object_size) {                          \
    return static_cast<ConcreteVisitor*>(this)                         \
        ->template VisitJSObjectSubclass<i::TypeName,                  \
                                         i::TypeName::BodyDescriptor>( \
            map, object, maybe_object_size);                           \
  }

DFX_TYPED_VISITOR_WITH_SLACK_ID_LIST(VISIT_WITH_SLACK)
#undef VISIT_WITH_SLACK

template <typename ConcreteVisitor>
size_t DfxHeapVisitor<ConcreteVisitor>::VisitShortcutCandidate(
    i::Tagged<i::Map> map, i::Tagged<i::ConsString> object,
    DfxMaybeObjectSize maybe_object_size) {
  return static_cast<ConcreteVisitor*>(this)->VisitConsString(
      map, object, maybe_object_size);
}

template <typename ConcreteVisitor>
size_t DfxHeapVisitor<ConcreteVisitor>::VisitFiller(
    i::Tagged<i::Map> map, i::Tagged<i::HeapObject> object,
    DfxMaybeObjectSize maybe_object_size) {
  if constexpr (!ConcreteVisitor::CanEncounterFillerOrFreeSpace()) {
    UNREACHABLE();
  }
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  visitor->template VisitMapPointerIfNeeded<i::VisitorId::kVisitFiller>(object);
  return ConcreteVisitor::UsePrecomputedObjectSize()
             ? maybe_object_size.AssumeSize()
             : map->instance_size();
}

template <typename ConcreteVisitor>
size_t DfxHeapVisitor<ConcreteVisitor>::VisitFreeSpace(
    i::Tagged<i::Map> map, i::Tagged<i::FreeSpace> object,
    DfxMaybeObjectSize maybe_object_size) {
  if constexpr (!ConcreteVisitor::CanEncounterFillerOrFreeSpace()) {
    UNREACHABLE();
  }
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  visitor->template VisitMapPointerIfNeeded<i::VisitorId::kVisitFreeSpace>(
      object);
  return object->size(kRelaxedLoad);
}

template <typename ConcreteVisitor>
size_t DfxHeapVisitor<ConcreteVisitor>::VisitJSObjectFast(
    i::Tagged<i::Map> map, i::Tagged<i::JSObject> object,
    DfxMaybeObjectSize maybe_object_size) {
  return static_cast<ConcreteVisitor*>(this)
      ->template VisitJSObjectSubclass<i::JSObject,
                                       i::JSObject::FastBodyDescriptor>(
          map, object, maybe_object_size);
}

template <typename ConcreteVisitor>
size_t DfxHeapVisitor<ConcreteVisitor>::VisitJSApiObject(
    i::Tagged<i::Map> map, i::Tagged<i::JSObject> object,
    DfxMaybeObjectSize maybe_object_size) {
  return static_cast<ConcreteVisitor*>(this)
      ->template VisitJSObjectSubclass<
          i::JSObject, i::JSAPIObjectWithEmbedderSlots::BodyDescriptor>(
          map, object, maybe_object_size);
}

template <typename ConcreteVisitor>
size_t DfxHeapVisitor<ConcreteVisitor>::VisitCppHeapExternalObject(
    i::Tagged<i::Map> map, i::Tagged<i::CppHeapExternalObject> object,
    DfxMaybeObjectSize maybe_object_size) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  const size_t size =
      ConcreteVisitor::UsePrecomputedObjectSize()
          ? maybe_object_size.AssumeSize()
          : i::CppHeapExternalObject::BodyDescriptor::SizeOf(map, object);
  visitor->template VisitMapPointerIfNeeded<
      i::VisitorId::kVisitCppHeapExternalObject>(object);
  i::CppHeapExternalObject::BodyDescriptor::IterateBody(
      map, object, static_cast<int>(size), visitor);
  return size;
}

template <typename ConcreteVisitor>
size_t DfxHeapVisitor<ConcreteVisitor>::VisitStruct(
    i::Tagged<i::Map> map, i::Tagged<i::HeapObject> object,
    DfxMaybeObjectSize maybe_object_size) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  int size = ConcreteVisitor::UsePrecomputedObjectSize()
                 ? static_cast<int>(maybe_object_size.AssumeSize())
                 : map->instance_size();
  visitor->template VisitMapPointerIfNeeded<i::VisitorId::kVisitStruct>(object);
  i::StructBodyDescriptor::IterateBody(map, object, size, visitor);
  return size;
}

template <typename ConcreteVisitor>
template <typename T, typename TBodyDescriptor>
size_t DfxHeapVisitor<ConcreteVisitor>::VisitJSObjectSubclass(
    i::Tagged<i::Map> map, i::Tagged<T> object,
    DfxMaybeObjectSize maybe_object_size) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  visitor->template VisitMapPointerIfNeeded<i::VisitorId::kVisitJSObject>(
      object);

  const size_t size = ConcreteVisitor::UsePrecomputedObjectSize()
                          ? maybe_object_size.AssumeSize()
                          : TBodyDescriptor::SizeOf(map, object);

  int visitation_size = static_cast<int>(size);

  if (!ConcreteVisitor::ShouldVisitFullJSObject()) {
    const int used_size = map->UsedInstanceSize();
    visitation_size = used_size;
  }

  TBodyDescriptor::IterateBody(map, object, visitation_size, visitor);
  return size;
}

template <typename ConcreteVisitor>
template <i::VisitorId visitor_id, typename T, typename TBodyDescriptor>
size_t DfxHeapVisitor<ConcreteVisitor>::VisitWithBodyDescriptor(
    i::Tagged<i::Map> map, i::Tagged<T> object,
    DfxMaybeObjectSize maybe_object_size) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  visitor->template VisitMapPointerIfNeeded<visitor_id>(object);
  const int size = ConcreteVisitor::UsePrecomputedObjectSize()
                       ? static_cast<int>(maybe_object_size.AssumeSize())
                       : TBodyDescriptor::SizeOf(map, object);
  TBodyDescriptor::IterateBody(map, object, size, visitor);
  return size;
}

}  // namespace dfx
}  // namespace v8

#endif  // DFX_HEAP_VISITOR_INL_H_
