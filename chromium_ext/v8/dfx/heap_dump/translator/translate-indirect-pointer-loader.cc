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

#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-indirect-pointer-loader.h"

#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-format.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-offheap-tables.h"
#include "src/objects/objects-inl.h"
#include "src/objects/tagged-field.h"

namespace v8 {
namespace dfx {

OfflineIndirectPointerLoadStatus
TrustedPointerField::ReadTrustedPointerField(
    i::Tagged<i::HeapObject> host, size_t offset, i::IndirectPointerTag tag,
    OfflineIndirectPointerLoadResult* result) const {
  // Implements the offline equivalent of:
  //   src/objects/trusted-pointer-inl.h
  //     TrustedPointerField::ReadTrustedPointerField()
  return ReadMaybeEmptyTrustedPointerField(host, offset, tag, result);
}

OfflineIndirectPointerLoadStatus
TrustedPointerField::ReadMaybeEmptyTrustedPointerField(
    i::Tagged<i::HeapObject> host, size_t offset, i::IndirectPointerTag tag,
    OfflineIndirectPointerLoadResult* result) const {
  // Implements the offline equivalent of:
  //   src/objects/trusted-pointer-inl.h
  //     TrustedPointerField::ReadMaybeEmptyTrustedPointerField()
  if (result != nullptr) {
    *result = OfflineIndirectPointerLoadResult{};
  }
#if defined(V8_ENABLE_SANDBOX)
  // Mirrors the sandbox branch:
  //   field_address = host->ptr() + offset - kHeapObjectTag
  //   ReadIndirectPointerField<tag>(field_address, isolate, kAcquireLoad)
  i::Address field_address = host->ptr() + offset - i::kHeapObjectTag;
  return indirect_pointer_field_.ReadIndirectPointerField(field_address, tag,
                                                         result);
#else
  USE(tag);
  if (result == nullptr) {
    return OfflineIndirectPointerLoadStatus::kEntryMissing;
  }
  i::Tagged<i::Object> value =
      i::TaggedField<i::Object>::Acquire_Load(host, static_cast<int>(offset));
  i::Tagged<i::HeapObject> heap_object;
  if (!value.GetHeapObject(&heap_object)) {
    return OfflineIndirectPointerLoadStatus::kEntryMissing;
  }
  result->target_address_ = heap_object.address();
  result->is_code_pointer_ = false;
  return OfflineIndirectPointerLoadStatus::kOk;
#endif
}

OfflineIndirectPointerLoadStatus
IndirectPointerField::ReadIndirectPointerField(
    i::Address field_address, i::IndirectPointerTag tag,
    OfflineIndirectPointerLoadResult* result) const {
  // Implements the offline equivalent of:
  //   src/sandbox/indirect-pointer-inl.h
  //     ReadIndirectPointerField()
  if (result != nullptr) {
    *result = OfflineIndirectPointerLoadResult{};
  }
#if defined(V8_ENABLE_SANDBOX)
  if (result == nullptr) {
    return OfflineIndirectPointerLoadStatus::kEntryMissing;
  }
  i::IndirectPointerHandle handle =
      *reinterpret_cast<i::IndirectPointerHandle*>(field_address);
  return ReadIndirectPointerHandle(handle, tag, result);
#else
  USE(field_address);
  USE(tag);
  USE(result);
  return OfflineIndirectPointerLoadStatus::kSandboxDisabled;
#endif
}

OfflineIndirectPointerLoadStatus
IndirectPointerField::ReadIndirectPointerHandle(
    i::IndirectPointerHandle handle, i::IndirectPointerTag tag,
    OfflineIndirectPointerLoadResult* result) const {
  // Implements the offline equivalent of:
  //   src/sandbox/indirect-pointer-inl.h
  //     ReadIndirectPointerHandle()
  if (result != nullptr) {
    *result = OfflineIndirectPointerLoadResult{};
  }
#if defined(V8_ENABLE_SANDBOX)
  if (result == nullptr) {
    return OfflineIndirectPointerLoadStatus::kEntryMissing;
  }
  if (!handle) {
    return OfflineIndirectPointerLoadStatus::kEntryMissing;
  }
  if (offheap_table_resolver_ == nullptr ||
      !offheap_table_resolver_->IsValid()) {
    return OfflineIndirectPointerLoadStatus::kResolverMissing;
  }

  if (tag == i::kUnknownIndirectPointerTag) {
    if (handle & i::kCodePointerHandleMarker) {
      return ResolveCodePointerHandle(handle, result);
    }
    return ResolveTrustedPointerHandle(handle, tag, result);
  }
  if (tag == i::kCodeIndirectPointerTag) {
    return ResolveCodePointerHandle(handle, result);
  }
  return ResolveTrustedPointerHandle(handle, tag, result);
#else
  USE(handle);
  USE(tag);
  USE(result);
  return OfflineIndirectPointerLoadStatus::kSandboxDisabled;
#endif
}

OfflineIndirectPointerLoadStatus
IndirectPointerField::ResolveCodePointerHandle(
    i::IndirectPointerHandle handle,
    OfflineIndirectPointerLoadResult* result) const {
  // Implements the offline equivalent of:
  //   src/sandbox/indirect-pointer-inl.h
  //     ResolveCodePointerHandle()
#if defined(V8_ENABLE_SANDBOX)
  const RawHeapSandboxCodePointerEntry* entry =
      offheap_table_resolver_->FindCodePointer(handle);
  if (entry == nullptr) {
    return OfflineIndirectPointerLoadStatus::kEntryMissing;
  }
  result->target_address_ = static_cast<i::Address>(entry->code_object_address_);
  result->is_code_pointer_ = true;
  return OfflineIndirectPointerLoadStatus::kOk;
#else
  USE(handle);
  USE(result);
  return OfflineIndirectPointerLoadStatus::kSandboxDisabled;
#endif
}

OfflineIndirectPointerLoadStatus
IndirectPointerField::ResolveTrustedPointerHandle(
    i::IndirectPointerHandle handle, i::IndirectPointerTag tag,
    OfflineIndirectPointerLoadResult* result) const {
  // Implements the offline equivalent of:
  //   src/sandbox/indirect-pointer-inl.h
  //     ResolveTrustedPointerHandle()
#if defined(V8_ENABLE_SANDBOX)
  const RawHeapSandboxTrustedPointerEntry* entry =
      offheap_table_resolver_->FindTrustedPointer(handle,
                                                  static_cast<uint32_t>(tag));
  if (entry == nullptr) {
    return OfflineIndirectPointerLoadStatus::kEntryMissing;
  }
  result->target_address_ =
      static_cast<i::Address>(entry->trusted_object_address_);
  result->is_code_pointer_ = false;
  return OfflineIndirectPointerLoadStatus::kOk;
#else
  USE(handle);
  USE(tag);
  USE(result);
  return OfflineIndirectPointerLoadStatus::kSandboxDisabled;
#endif
}

}  // namespace dfx
}  // namespace v8

#endif  // OH_ENABLE_HEAP_TRANSLATE || OH_ENABLE_HEAP_DUMP_TEST
