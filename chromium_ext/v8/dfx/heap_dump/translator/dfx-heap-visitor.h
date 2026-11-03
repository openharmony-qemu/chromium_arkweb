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

// This is a copy of heap-visitor.h with modifications for DFX heap translate.

#ifndef DFX_HEAP_VISITOR_H_
#define DFX_HEAP_VISITOR_H_

#include "arkweb/chromium_ext/v8/dfx/heap_dump/dfx-visitor-list.h"

#include "src/base/logging.h"
#include "src/execution/local-isolate.h"
#include "src/objects/bytecode-array.h"
#include "src/objects/contexts.h"
#include "src/objects/fixed-array.h"
#include "src/objects/js-weak-refs.h"
#include "src/objects/map.h"
#include "src/objects/object-list-macros.h"
#include "src/objects/objects.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/string.h"
#include "src/objects/visitors.h"

namespace v8 {
namespace dfx {

// This class is used as argument to the DfxHeapVisitor::Visit() method as a
// cheaper alternative to std::optional<size_t>.
class DfxMaybeObjectSize final {
 public:
  explicit DfxMaybeObjectSize(size_t size) : raw_size_(size) {}
  explicit DfxMaybeObjectSize(i::SafeHeapObjectSize size) : raw_size_(size.value()) {}
  DfxMaybeObjectSize() : raw_size_(0) {}

  size_t AssumeSize() const {
    return raw_size_;
  }

  bool IsNone() const { return raw_size_ == 0; }

 private:
  size_t raw_size_;
};

template <typename ConcreteVisitor>
class DfxHeapVisitor : public i::ObjectVisitorWithCageBases {
 public:
  inline explicit DfxHeapVisitor(i::PtrComprCageBase cage_base,
                                 i::PtrComprCageBase code_cage_base);

  V8_INLINE size_t Visit(i::Tagged<i::HeapObject> object)
    requires(!ConcreteVisitor::UsePrecomputedObjectSize());

  V8_INLINE size_t Visit(i::Tagged<i::Map> map, i::Tagged<i::HeapObject> object)
    requires(!ConcreteVisitor::UsePrecomputedObjectSize());

  V8_INLINE size_t Visit(i::Tagged<i::Map> map, i::Tagged<i::HeapObject> object,
                         int object_size)
    requires(ConcreteVisitor::UsePrecomputedObjectSize());
  V8_INLINE size_t Visit(i::Tagged<i::Map> map, i::Tagged<i::HeapObject> object,
                         i::SafeHeapObjectSize object_size)
    requires(ConcreteVisitor::UsePrecomputedObjectSize());

 protected:
  V8_INLINE size_t Visit(i::Tagged<i::Map> map, i::Tagged<i::HeapObject> object,
                         DfxMaybeObjectSize maybe_object_size);

  V8_INLINE static constexpr bool ShouldVisitMapPointer() { return true; }
  V8_INLINE static constexpr bool ShouldVisitReadOnlyMapPointer() { return true; }
  V8_INLINE static constexpr bool CanEncounterFillerOrFreeSpace() { return true; }
  V8_INLINE static constexpr bool ShouldUseUncheckedCast() { return false; }
  V8_INLINE static constexpr bool EnableConcurrentVisitation() { return false; }
  V8_INLINE static constexpr bool UsePrecomputedObjectSize() { return false; }

  template <i::VisitorId visitor_id>
  V8_INLINE void VisitMapPointerIfNeeded(i::Tagged<i::HeapObject> host);

  V8_INLINE static constexpr bool ShouldVisitFullJSObject() { return false; }

  ConcreteVisitor* concrete_visitor() {
    return static_cast<ConcreteVisitor*>(this);
  }

  const ConcreteVisitor* concrete_visitor() const {
    return static_cast<const ConcreteVisitor*>(this);
  }

  // For visiting types, use VisitStruct for unsupported types
  V8_INLINE size_t VisitDataOnlyObject(i::Tagged<i::Map> map,
                                        i::Tagged<i::HeapObject> object,
                                        DfxMaybeObjectSize maybe_object_size);

#define VISIT(TypeName)                                                      \
  V8_INLINE size_t Visit##TypeName(i::Tagged<i::Map> map,                   \
                                   i::Tagged<i::TypeName> object,            \
                                   DfxMaybeObjectSize maybe_object_size);
  DFX_TYPED_VISITOR_ID_LIST(VISIT)
  DFX_TYPED_VISITOR_WITH_SLACK_ID_LIST(VISIT)
  DFX_TORQUE_VISITOR_ID_LIST(VISIT)
  DFX_TRUSTED_VISITOR_ID_LIST(VISIT)
#undef VISIT
  V8_INLINE size_t VisitShortcutCandidate(i::Tagged<i::Map> map,
                                          i::Tagged<i::ConsString> object,
                                          DfxMaybeObjectSize maybe_object_size);
  V8_INLINE size_t VisitJSObjectFast(i::Tagged<i::Map> map,
                                     i::Tagged<i::JSObject> object,
                                     DfxMaybeObjectSize maybe_object_size);
  V8_INLINE size_t VisitJSApiObject(i::Tagged<i::Map> map,
                                    i::Tagged<i::JSObject> object,
                                    DfxMaybeObjectSize maybe_object_size);
  V8_INLINE size_t VisitCppHeapExternalObject(
      i::Tagged<i::Map> map, i::Tagged<i::CppHeapExternalObject> object,
      DfxMaybeObjectSize maybe_object_size);
  V8_INLINE size_t VisitStruct(i::Tagged<i::Map> map,
                               i::Tagged<i::HeapObject> object,
                               DfxMaybeObjectSize maybe_object_size);
  V8_INLINE size_t VisitFiller(i::Tagged<i::Map> map,
                               i::Tagged<i::HeapObject> object,
                               DfxMaybeObjectSize maybe_object_size);
  V8_INLINE size_t VisitFreeSpace(i::Tagged<i::Map> map,
                                  i::Tagged<i::FreeSpace> object,
                                  DfxMaybeObjectSize maybe_object_size);

  template <typename T, typename TBodyDescriptor = typename T::BodyDescriptor>
  V8_INLINE size_t VisitJSObjectSubclass(i::Tagged<i::Map> map,
                                         i::Tagged<T> object,
                                         DfxMaybeObjectSize maybe_object_size);

  template <i::VisitorId visitor_id, typename T,
            typename TBodyDescriptor = typename T::BodyDescriptor>
  V8_INLINE size_t VisitWithBodyDescriptor(i::Tagged<i::Map> map,
                                           i::Tagged<T> object,
                                           DfxMaybeObjectSize maybe_object_size);

  template <typename T>
  static V8_INLINE i::Tagged<T> Cast(i::Tagged<i::HeapObject> object, const i::Heap* heap);
};

}  // namespace dfx
}  // namespace v8

#endif  // DFX_HEAP_VISITOR_H_
