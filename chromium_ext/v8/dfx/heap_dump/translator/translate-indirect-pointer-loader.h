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

#ifndef TRANSLATE_INDIRECT_POINTER_LOADER_H
#define TRANSLATE_INDIRECT_POINTER_LOADER_H

#if (defined(OH_ENABLE_HEAP_TRANSLATE) || defined(OH_ENABLE_HEAP_DUMP_TEST))

#include "src/common/globals.h"
#include "src/objects/heap-object.h"
#include "src/sandbox/indirect-pointer-tag.h"

namespace v8 {
namespace dfx {

class OffheapTableResolver;

enum class OfflineIndirectPointerLoadStatus {
  kOk,
  kSandboxDisabled,
  kResolverMissing,
  kEntryMissing,
};

struct OfflineIndirectPointerLoadResult {
  i::Address target_address_{0};
  bool is_code_pointer_{false};
};

class IndirectPointerField {
 public:
  explicit IndirectPointerField(
      const OffheapTableResolver* offheap_table_resolver)
      : offheap_table_resolver_(offheap_table_resolver) {}

  // Offline counterpart of V8's sandbox field load in:
  //   src/sandbox/indirect-pointer-inl.h
  //     ReadIndirectPointerField()
  //
  // It loads the IndirectPointerHandle stored in the dumped object field and
  // delegates handle resolution to ReadIndirectPointerHandle().
  OfflineIndirectPointerLoadStatus ReadIndirectPointerField(
      i::Address field_address, i::IndirectPointerTag tag,
      OfflineIndirectPointerLoadResult* result) const;

 private:
  OfflineIndirectPointerLoadStatus ReadIndirectPointerHandle(
      i::IndirectPointerHandle handle, i::IndirectPointerTag tag,
      OfflineIndirectPointerLoadResult* result) const;
  OfflineIndirectPointerLoadStatus ResolveCodePointerHandle(
      i::IndirectPointerHandle handle,
      OfflineIndirectPointerLoadResult* result) const;
  OfflineIndirectPointerLoadStatus ResolveTrustedPointerHandle(
      i::IndirectPointerHandle handle, i::IndirectPointerTag tag,
      OfflineIndirectPointerLoadResult* result) const;

  [[maybe_unused]] const OffheapTableResolver* offheap_table_resolver_{nullptr};
};

class TrustedPointerField {
 public:
  explicit TrustedPointerField(const OffheapTableResolver* offheap_table_resolver)
      : indirect_pointer_field_(offheap_table_resolver) {}

  // Offline counterpart of V8's trusted pointer entry point in:
  //   src/objects/trusted-pointer-inl.h
  //     TrustedPointerField::ReadTrustedPointerField()
  //
  // The V8 implementation immediately delegates to
  // ReadMaybeEmptyTrustedPointerField(), then either reads an indirect handle
  // from a sandbox field or performs a normal tagged-field load in no-sandbox
  // builds. This helper preserves that layering; callers still own rawheap
  // membership, type, and snapshot-node checks before edge emission.
  OfflineIndirectPointerLoadStatus ReadTrustedPointerField(
      i::Tagged<i::HeapObject> host, size_t offset, i::IndirectPointerTag tag,
      OfflineIndirectPointerLoadResult* result) const;

 private:
  OfflineIndirectPointerLoadStatus ReadMaybeEmptyTrustedPointerField(
      i::Tagged<i::HeapObject> host, size_t offset, i::IndirectPointerTag tag,
      OfflineIndirectPointerLoadResult* result) const;

  IndirectPointerField indirect_pointer_field_;
};

}  // namespace dfx
}  // namespace v8

#endif  // OH_ENABLE_HEAP_TRANSLATE || OH_ENABLE_HEAP_DUMP_TEST
#endif  // TRANSLATE_INDIRECT_POINTER_LOADER_H
