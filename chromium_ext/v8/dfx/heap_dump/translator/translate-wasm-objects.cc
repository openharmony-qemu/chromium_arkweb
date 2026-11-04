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

#include <limits>
#include <memory>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-fake-heap-object.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-indirect-pointer-loader.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-objects.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-offheap-tables.h"
#include "src/objects/objects-body-descriptors-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace dfx {

#if V8_ENABLE_WEBASSEMBLY

void ObjectTranslator::SetWasmInternalReferenceIfExistingHeapObject(
    i::Address parent, const char* name, i::Tagged<i::HeapObject> object,
    int offset) {
  i::Tagged<i::Object> value =
      i::TaggedField<i::Tagged<i::Object>>::load(cage_base_, object, offset);
  if (!i::IsHeapObject(value)) {
    return;
  }
  i::Tagged<i::HeapObject> child = i::Cast<i::HeapObject>(value);
  if (FindObjectHead(child.address()) == nullptr) {
    ReportWarning("wasm_internal_reference_target_not_dumped",
                  "Wasm ordinary tagged-field target is not present in "
                  "rawheap; named edge suppressed",
                  "object_body", parent);
    return;
  }
  if (!generator_->GetNode(child.address())) {
    // Offline translate must skip missing dumped children before
    // SetInternalReference/visited-field marking; otherwise a missing node can
    // become a no-sandbox CHECK or a fabricated Wasm relationship.
    ReportWarning("wasm_internal_reference_target_node_missing",
                  "Wasm ordinary tagged-field target has no snapshot node; "
                  "named edge suppressed",
                  "object_body", parent);
    return;
  }
  generator_->SetInternalReference(parent, name, child.address(), offset);
}

void ObjectTranslator::ExtractWasmInstanceObjectReferences(
    i::Address real_address, i::Tagged<i::WasmInstanceObject> instance_object) {
  // Offline counterpart of
  // V8HeapExplorer::ExtractWasmInstanceObjectReferences in
  // src/profiler/heap-snapshot-generator.cc.

  // Ordinary WasmInstanceObject fields are real V8 heap-object edges only when
  // the decoded child was dumped and materialized as a snapshot node. Skip
  // before SetInternalReference if the child node is absent, matching native
  // no-entry behavior without fabricating placeholder Wasm edges.
  i::Address trusted_data_address = 0;
  if (GetWasmInstanceObjectTrustedData(real_address, instance_object,
                                       &trusted_data_address)) {
    generator_->SetInternalReference(real_address, "trusted_data",
                                     trusted_data_address,
                                     i::WasmInstanceObject::kTrustedDataOffset);
  }

  // module_object() and exports_object() is in wasm-objects-tq-inl.inc
  SetWasmInternalReferenceIfExistingHeapObject(
      real_address, "module_object", i::Cast<i::HeapObject>(instance_object),
      i::WasmInstanceObject::kModuleObjectOffset);
  SetWasmInternalReferenceIfExistingHeapObject(
      real_address, "exports", i::Cast<i::HeapObject>(instance_object),
      i::WasmInstanceObject::kExportsObjectOffset);
}

bool ObjectTranslator::GetWasmInstanceObjectTrustedData(
    i::Address real_address, i::Tagged<i::WasmInstanceObject> instance_object,
    i::Address* trusted_data_address) {
  if (trusted_data_address == nullptr) {
    return false;
  }
  *trusted_data_address = 0;

  // Offline equivalent of:
  //   WasmInstanceObject::trusted_data(isolate)
  // implemented by TRUSTED_POINTER_ACCESSORS in src/wasm/wasm-objects-inl.h,
  // then delegated through src/objects/trusted-pointer-inl.h.
  TrustedPointerField trusted_pointer_field(offheap_table_resolver_);
  OfflineIndirectPointerLoadResult trusted_data_result;
  OfflineIndirectPointerLoadStatus trusted_data_status =
      trusted_pointer_field.ReadTrustedPointerField(
          instance_object, i::WasmInstanceObject::kTrustedDataOffset,
          i::kWasmTrustedInstanceDataIndirectPointerTag, &trusted_data_result);
  if (trusted_data_status == OfflineIndirectPointerLoadStatus::kOk) {
    i::Address candidate = trusted_data_result.target_address_;
    if (FindObjectHead(candidate) == nullptr &&
        (candidate & i::kHeapObjectTag) == i::kHeapObjectTag) {
      const i::Address untagged_trusted_data_address =
          candidate - i::kHeapObjectTag;
      if (FindObjectHead(untagged_trusted_data_address) != nullptr) {
        candidate = untagged_trusted_data_address;
      }
    }

    if (FindObjectHead(candidate) != nullptr) {
      i::InstanceType target_type;
      if (!GetInstanceType(candidate, &target_type) ||
          target_type != i::WASM_TRUSTED_INSTANCE_DATA_TYPE) {
        ReportWarning("wasm_instance_trusted_data_wrong_type",
                      "WasmInstanceObject trusted_data target is not "
                      "WasmTrustedInstanceData",
                      "object_body", real_address);
        return false;
      }
      if (generator_->GetNode(candidate) == nullptr) {
        ReportWarning("wasm_instance_trusted_data_node_missing",
                      "WasmInstanceObject trusted_data target has no snapshot "
                      "node",
                      "object_body", real_address);
        return false;
      }
      *trusted_data_address = candidate;
      return true;
    }
  } else if (trusted_data_status ==
             OfflineIndirectPointerLoadStatus::kResolverMissing) {
    ReportUnsupported("wasm_instance_trusted_data_table_missing",
                      "WasmInstanceObject trusted_data cannot be resolved "
                      "without dumped sandbox trusted pointer table",
                      "object_body", real_address);
  }
  return false;
}

void ObjectTranslator::ExtractWasmModuleObjectReferences(
    i::Address real_address, i::Tagged<i::WasmModuleObject> module_object) {
  // Offline counterpart of
  // V8HeapExplorer::ExtractWasmModuleObjectReferences in
  // src/profiler/heap-snapshot-generator.cc.

  // Ordinary WasmModuleObject fields follow the same rawheap safety contract as
  // WasmInstanceObject fields: the decoded heap object must already be a
  // materialized snapshot node before translate emits the named internal edge.
  SetWasmInternalReferenceIfExistingHeapObject(
      real_address, "managed_native_module",
      i::Cast<i::HeapObject>(module_object),
      i::WasmModuleObject::kManagedNativeModuleOffset);
  SetWasmInternalReferenceIfExistingHeapObject(
      real_address, "script", i::Cast<i::HeapObject>(module_object),
      i::WasmModuleObject::kScriptOffset);
}

void ObjectTranslator::ExtractWasmStructReferences(
    i::Address real_address, i::Tagged<i::WasmStruct> obj,
    i::Tagged<i::Map> map, i::Address real_map_address) {
  CHECK(i::WASM_STRUCT_TYPE == map->instance_type());

  // Mirrors V8HeapExplorer::ExtractWasmStructReferences:
  //   info = obj->map()->wasm_type_info()
  //   type = LookupStruct(info->type_index())
  //   for each field: scalar fields are optional numeric values, ref fields
  //   become named property edges.
  //
  // Offline translate gets the type index through the dumped fake Map and the
  // field layout through RawHeapOffheapTableKind::kWasmCanonicalTypeTable,
  // because consulting this process's canonicalizer would be unsafe for a dump
  // produced by a different process/version.
  uint32_t type_index = 0;
  if (!GetWasmTypeIndexForMap(map, real_address, real_map_address,
                              &type_index)) {
    return;
  }

  const bool has_wasm_canonical_table =
      offheap_table_resolver_ != nullptr &&
      offheap_table_resolver_->IsValid() &&
      offheap_table_resolver_->HasTable(
          RawHeapOffheapTableKind::kWasmCanonicalTypeTable);

  // WasmStruct object bytes contain field payload, but the ref/scalar layout
  // comes from the producer's canonical type table. If this rawheap predates
  // the offheap kWasmCanonicalTypeTable, preserve the node/map edge and report
  // the missing capability instead of consulting this process's canonicalizer.
  if (!has_wasm_canonical_table) {
    std::string message =
        "WasmStruct body references require the offheap "
        "WasmCanonicalTypeTable; type_index=" +
        std::to_string(type_index);
    ReportUnsupported("wasm_struct_canonical_type_table_missing",
                      message.c_str(), "object_body", real_address,
                      real_map_address,
                      static_cast<uint32_t>(map->instance_type()),
                      static_cast<uint32_t>(map->visitor_id()));
    return;
  }

  const RawHeapWasmCanonicalStructRecord* record =
      offheap_table_resolver_->FindWasmCanonicalStruct(type_index);
  if (record == nullptr) {
    std::string message =
        "Wasm canonical struct layout was not found; type_index=" +
        std::to_string(type_index);
    ReportUnsupported("wasm_struct_type_layout_missing", message.c_str(),
                      "object_body", real_address, real_map_address,
                      static_cast<uint32_t>(map->instance_type()),
                      static_cast<uint32_t>(map->visitor_id()));
    return;
  }

  WasmCanonicalStructFieldsView fields;
  if (!offheap_table_resolver_->GetWasmCanonicalStructFields(*record,
                                                             &fields)) {
    std::string message =
        "Wasm canonical struct layout fields were invalid; type_index=" +
        std::to_string(type_index);
    ReportCorrupt("wasm_struct_canonical_type_fields_invalid", message.c_str(),
                  "object_body", real_address, real_map_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()));
    return;
  }

  const RawHeapObjectHead* object_head = FindObjectHead(real_address);
  if (object_head == nullptr) {
    ReportCorrupt("wasm_struct_object_missing",
                  "WasmStruct object was not present in rawheap", "object_body",
                  real_address, real_map_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()));
    return;
  }

  const uint32_t object_size = object_head->size_;
  if ((record->flags_ & kRawHeapWasmStructIsDescriptor) != 0) {
    if (object_size < static_cast<uint32_t>(i::WasmStruct::kHeaderSize) ||
        i::kTaggedSize >
            object_size - static_cast<uint32_t>(i::WasmStruct::kHeaderSize)) {
      std::string message =
          "WasmStruct descriptor RawField(0) is outside the dumped object "
          "payload; type_index=" +
          std::to_string(type_index) +
          " object_size=" + std::to_string(object_size);
      ReportCorrupt(
          "wasm_struct_descriptor_offset_out_of_range", message.c_str(),
          "object_body", real_address, real_map_address,
          static_cast<uint32_t>(map->instance_type()),
          static_cast<uint32_t>(map->visitor_id()), object_head->offset_);
      return;
    }

    i::Tagged<i::Object> descriptor_value = i::TaggedField<i::Object>::load(
        cage_base_, obj, i::WasmStruct::kHeaderSize);
    if (i::IsHeapObject(descriptor_value)) {
      i::Address child = i::Cast<i::HeapObject>(descriptor_value).address();
      if (generator_->GetNode(child)) {
        generator_->SetInternalReference(real_address, "described_rtt", child,
                                         i::WasmStruct::kHeaderSize);
      }
    }
  }

  for (uint32_t field_index = 0; field_index < fields.count_; ++field_index) {
    const RawHeapWasmCanonicalStructField& field = fields.data_[field_index];
    if ((field.flags_ & kRawHeapWasmFieldIsRef) == 0) {
      // Native may materialize scalar field values when numeric capture is
      // enabled. The translate snapshot generator does not support synthetic
      // scalar child entries yet, so scalar fields intentionally remain absent.
      continue;
    }

    // Native field names come from CanonicalTypeNamesProvider::PrintFieldName.
    // The rawheap canonical table currently exports layout, not those strings,
    // so translate uses a stable field-index name for the same property edge.
    std::string field_name = "wasm_struct_field_" + std::to_string(field_index);
    if (field.field_offset_ >
        std::numeric_limits<uint32_t>::max() - i::WasmStruct::kHeaderSize) {
      std::string message =
          "WasmStruct field offset overflows object offset; type_index=" +
          std::to_string(type_index) +
          " field_index=" + std::to_string(field_index) +
          " field_offset=" + std::to_string(field.field_offset_);
      ReportCorrupt("wasm_struct_field_offset_out_of_range", message.c_str(),
                    "object_body", real_address, real_map_address,
                    static_cast<uint32_t>(map->instance_type()),
                    static_cast<uint32_t>(map->visitor_id()));
      return;
    }

    const uint32_t object_field_offset =
        i::WasmStruct::kHeaderSize + field.field_offset_;
    if (object_field_offset % i::kTaggedSize != 0 ||
        object_field_offset > object_size ||
        i::kTaggedSize > object_size - object_field_offset) {
      std::string message =
          "WasmStruct ref field is outside the dumped object payload; "
          "type_index=" +
          std::to_string(type_index) +
          " field_index=" + std::to_string(field_index) +
          " field_offset=" + std::to_string(field.field_offset_) +
          " object_offset=" + std::to_string(object_field_offset) +
          " object_size=" + std::to_string(object_size);
      ReportCorrupt("wasm_struct_field_offset_out_of_range", message.c_str(),
                    "object_body", real_address, real_map_address,
                    static_cast<uint32_t>(map->instance_type()),
                    static_cast<uint32_t>(map->visitor_id()),
                    object_head->offset_);
      return;
    }

    i::Tagged<i::Object> value = i::TaggedField<i::Object>::load(
        cage_base_, obj, static_cast<int>(object_field_offset));
    if (!i::IsHeapObject(value)) {
      continue;
    }

    i::Address child = i::Cast<i::HeapObject>(value).address();
    if (!generator_->GetNode(child)) {
      continue;
    }

    generator_->SetPropertyReference(real_address, field_name, child,
                                     static_cast<int>(object_field_offset));
  }
}

bool ObjectTranslator::ExtractWasmArrayReferences(i::Address real_address,
                                                  i::Tagged<i::WasmArray> obj,
                                                  i::Tagged<i::Map> map,
                                                  i::Address real_map_address) {
  bool is_reference = false;
  if (!IsReferenceForWasmArray(map, real_address, real_map_address,
                               &is_reference)) {
    return false;
  }
  if (!is_reference) {
    return true;
  }

  const RawHeapObjectHead* object_head = FindObjectHead(real_address);
  if (object_head == nullptr) {
    ReportCorrupt("wasm_array_object_missing",
                  "WasmArray object was not present in rawheap", "object_body",
                  real_address, real_map_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()));
    return false;
  }

  const uint32_t object_size = object_head->size_;
  if (object_size < static_cast<uint32_t>(i::WasmArray::kHeaderSize)) {
    ReportCorrupt("wasm_array_object_too_small",
                  "WasmArray object is too small for its header", "object_body",
                  real_address, real_map_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  object_head->offset_);
    return false;
  }

  const uint32_t length = obj->length();
  constexpr uint32_t kElementSize = static_cast<uint32_t>(i::kTaggedSize);
  if (length > (std::numeric_limits<uint32_t>::max() -
                static_cast<uint32_t>(i::WasmArray::kHeaderSize)) /
                   kElementSize) {
    ReportCorrupt("wasm_array_elements_out_of_range",
                  "WasmArray element payload size overflows uint32_t",
                  "object_body", real_address, real_map_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  object_head->offset_);
    return false;
  }

  const uint32_t elements_end =
      static_cast<uint32_t>(i::WasmArray::kHeaderSize) + length * kElementSize;
  if (elements_end > object_size) {
    std::string message =
        "WasmArray ref elements are outside the dumped object payload; "
        "length=" +
        std::to_string(length) +
        " elements_end=" + std::to_string(elements_end) +
        " object_size=" + std::to_string(object_size);
    ReportCorrupt("wasm_array_elements_out_of_range", message.c_str(),
                  "object_body", real_address, real_map_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  object_head->offset_);
    return false;
  }

  for (uint32_t index = 0; index < length; ++index) {
    const uint32_t element_offset =
        static_cast<uint32_t>(i::WasmArray::kHeaderSize) + index * kElementSize;
    i::Tagged<i::Object> value = i::TaggedField<i::Object>::load(
        cage_base_, obj, static_cast<int>(element_offset));
    if (!i::IsHeapObject(value)) {
      continue;
    }

    i::Address child = i::Cast<i::HeapObject>(value).address();
    if (!generator_->GetNode(child)) {
      continue;
    }
    i::InstanceType child_type;
    // Do not call the online IsWasmNull(value) helper here: translate is
    // reading fake rawheap objects, and online Wasm null checks can dereference
    // real heap metadata. Resolve the child's dumped map instead.
    if (GetInstanceType(child, &child_type) && child_type == i::WASM_NULL_TYPE) {
      continue;
    }
    generator_->SetElementReference(real_address, index, child);
  }
  return true;
}

void ObjectTranslator::ExtractWasmTrustedInstanceDataReferences(
    i::Address real_address, i::Tagged<i::HeapObject> trusted_data) {
  const RawHeapObjectHead* object_head = FindObjectHead(real_address);
  if (object_head == nullptr) {
    ReportCorrupt("wasm_trusted_instance_data_object_missing",
                  "WasmTrustedInstanceData object was not present in rawheap",
                  "object_body", real_address);
    return;
  }

  auto field_in_range = [&](uint16_t offset, const char* edge_name) {
    if (object_head->size_ < static_cast<uint32_t>(offset) + i::kTaggedSize) {
      std::string message =
          "WasmTrustedInstanceData field is outside the dumped object payload; "
          "field=" +
          std::string(edge_name) + " offset=" + std::to_string(offset) +
          " object_size=" + std::to_string(object_head->size_);
      ReportCorrupt("wasm_trusted_instance_data_field_out_of_range",
                    message.c_str(), "object_body", real_address, 0, 0, 0,
                    object_head->offset_);
      return false;
    }
    return true;
  };

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

  // Native V8HeapExplorer::ExtractWasmTrustedInstanceDataReferences emits
  // named internal edges from these fixed field tables instead of relying on
  // generic hidden BodyDescriptor edges. Keep translate on the same path and
  // let ObjectTranslator::TranslateHeapObject skip the generic
  // WasmTrustedInstanceData::BodyDescriptor, which is not offline-safe because
  // it queries real heap page metadata through HeapLayout::InAnySharedSpace().
  for (size_t i = 0; i < i::WasmTrustedInstanceData::kTaggedFieldOffsets.size();
       ++i) {
    const uint16_t offset = i::WasmTrustedInstanceData::kTaggedFieldOffsets[i];
    if (!field_in_range(offset,
                        i::WasmTrustedInstanceData::kTaggedFieldNames[i])) {
      return;
    }
    i::Tagged<i::Object> value =
        i::TaggedField<i::Object>::load(cage_base_, trusted_data, offset);
    set_internal_if_heap_object(
        i::WasmTrustedInstanceData::kTaggedFieldNames[i], value, offset);
  }

  // Native then emits the protected pointer fields as named internal edges.
  // These are not indirect-pointer-table handles: even in sandbox builds
  // ProtectedPointerSlot is an OffHeapCompressedObjectSlot that decodes through
  // TrustedSpaceCompressionScheme. The rawheap v3 header initializes that base
  // before translate starts; legacy dumps without it cannot safely decode these
  // fields, so skip only in that case instead of falling back to the generic
  // BodyDescriptor.
#if defined(V8_ENABLE_SANDBOX)
  if (!has_trusted_cage_base_) {
    ReportWarning("sandbox_protected_pointer_missing_trusted_cage_base",
                  "WasmTrustedInstanceData protected fields require trusted "
                  "cage base from rawheap v3; skipping protected named edges",
                  "object_body", real_address, 0,
                  static_cast<uint32_t>(i::WASM_TRUSTED_INSTANCE_DATA_TYPE));
    return;
  }
#endif
  for (size_t i = 0;
       i < i::WasmTrustedInstanceData::kProtectedFieldOffsets.size(); ++i) {
    const uint16_t offset =
        i::WasmTrustedInstanceData::kProtectedFieldOffsets[i];
    if (!field_in_range(offset,
                        i::WasmTrustedInstanceData::kProtectedFieldNames[i])) {
      return;
    }
    i::Tagged<i::Object> value =
        i::TaggedField<i::Object, 0, i::TrustedSpaceCompressionScheme>::load(
            cage_base_, trusted_data, offset);
    set_internal_if_heap_object(
        i::WasmTrustedInstanceData::kProtectedFieldNames[i], value, offset);
  }
}

bool ObjectTranslator::GetWasmTypeInfoForMap(
    i::Tagged<i::Map> map, i::Address real_object_address,
    i::Address real_map_address,
    std::unique_ptr<FakeHeapObject>* fake_type_info,
    i::Address* real_type_info_address) {
  if (fake_type_info == nullptr || real_type_info_address == nullptr) {
    return false;
  }
  fake_type_info->reset();
  *real_type_info_address = 0;

  // Map::wasm_type_info() aliases the constructor/back-pointer/native-context
  // field for Wasm maps. Load the field directly from the fake map as the
  // WasmArray path does, then rebuild the referenced WasmTypeInfo from rawheap.
  i::Tagged<i::Object> value = i::TaggedField<i::Tagged<i::Object>>::load(
      cage_base_, map, i::Map::kConstructorOrBackPointerOrNativeContextOffset);
  if (!i::IsHeapObject(value)) {
    ReportCorrupt("wasm_type_info_not_heap_object",
                  "Wasm map type info field was not a heap object",
                  "object_body", real_object_address, real_map_address,
                  static_cast<uint32_t>(map->instance_type()));
    return false;
  }

  i::Tagged<i::HeapObject> raw_type_info = i::Cast<i::HeapObject>(value);
  *real_type_info_address = raw_type_info.address();
  std::unique_ptr<FakeHeapObject> type_info =
      MakeFakeHeapObject(raw_type_info.address());
  if (!type_info) {
    ReportCorrupt("wasm_type_info_missing",
                  "Wasm type info was not present in rawheap", "object_body",
                  real_object_address, real_map_address,
                  static_cast<uint32_t>(map->instance_type()));
    return false;
  }

  i::Address type_info_map_address =
      type_info->fake_object()->map(cage_base_).address();
  std::unique_ptr<FakeHeapObject> fake_type_info_map =
      MakeFakeHeapObject(type_info_map_address);
  if (!fake_type_info_map) {
    ReportCorrupt("wasm_type_info_map_missing",
                  "Wasm type info map was not present in rawheap",
                  "object_body", real_object_address, real_map_address,
                  static_cast<uint32_t>(map->instance_type()));
    return false;
  }
  i::Tagged<i::Map> type_info_map =
      i::UncheckedCast<i::Map>(fake_type_info_map->fake_object());
  if (type_info_map->instance_type() != i::WASM_TYPE_INFO_TYPE) {
    ReportCorrupt("wasm_type_info_wrong_instance_type",
                  "Wasm type info rawheap object did not have "
                  "WASM_TYPE_INFO_TYPE",
                  "object_body", real_object_address, real_map_address,
                  static_cast<uint32_t>(map->instance_type()));
    return false;
  }

  *fake_type_info = std::move(type_info);
  return true;
}

bool ObjectTranslator::ValidateWasmTypeInfoField(
    i::Address real_type_info_address, uint32_t field_offset,
    uint32_t field_size, i::Address real_object_address,
    i::Address real_map_address, i::Tagged<i::Map> map, const char* code,
    const char* message) {
  const RawHeapObjectHead* type_info_head =
      FindObjectHead(real_type_info_address);
  if (type_info_head == nullptr) {
    ReportCorrupt("wasm_type_info_missing",
                  "Wasm type info was not present in rawheap", "object_body",
                  real_object_address, real_map_address,
                  static_cast<uint32_t>(map->instance_type()));
    return false;
  }

  if (field_offset > type_info_head->size_ ||
      field_size > type_info_head->size_ - field_offset) {
    ReportCorrupt(code, message, "object_body", real_object_address,
                  real_map_address, static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()),
                  type_info_head->offset_);
    return false;
  }
  return true;
}

bool ObjectTranslator::GetWasmTypeIndexForMap(i::Tagged<i::Map> map,
                                              i::Address real_object_address,
                                              i::Address real_map_address,
                                              uint32_t* type_index) {
  if (type_index == nullptr) {
    return false;
  }
  *type_index = 0;

  std::unique_ptr<FakeHeapObject> fake_type_info;
  i::Address real_type_info_address = 0;
  if (!GetWasmTypeInfoForMap(map, real_object_address, real_map_address,
                             &fake_type_info, &real_type_info_address)) {
    return false;
  }
  if (!ValidateWasmTypeInfoField(
          real_type_info_address,
          static_cast<uint32_t>(i::WasmTypeInfo::kCanonicalTypeOffset),
          static_cast<uint32_t>(sizeof(uint32_t)), real_object_address,
          real_map_address, map, "wasm_type_info_canonical_type_truncated",
          "Wasm type info is too small for canonical_type")) {
    return false;
  }

  i::Tagged<i::WasmTypeInfo> type_info =
      i::UncheckedCast<i::WasmTypeInfo>(fake_type_info->fake_object());
  i::wasm::CanonicalTypeIndex canonical_type_index = type_info->type_index();
  if (!canonical_type_index.valid()) {
    ReportCorrupt("wasm_type_index_invalid",
                  "Wasm type info contained an invalid canonical type index",
                  "object_body", real_object_address, real_map_address,
                  static_cast<uint32_t>(map->instance_type()));
    return false;
  }
  *type_index = canonical_type_index.index;
  return true;
}

// see WasmArray::BodyDescriptor in objects-body-descriptors-inl.h
bool ObjectTranslator::IsReferenceForWasmArray(i::Tagged<i::Map> map,
                                               i::Address real_object_address,
                                               i::Address real_map_address,
                                               bool* is_reference) {
  if (is_reference == nullptr) {
    return false;
  }
  *is_reference = false;
  // WasmArray::GcSafeElementType
  CHECK(i::InstanceType::WASM_ARRAY_TYPE == map->instance_type());
  std::unique_ptr<FakeHeapObject> fake_type_info;
  i::Address real_type_info_address = 0;
  if (!GetWasmTypeInfoForMap(map, real_object_address, real_map_address,
                             &fake_type_info, &real_type_info_address)) {
    return false;
  }
  if (!ValidateWasmTypeInfoField(
          real_type_info_address,
          static_cast<uint32_t>(i::WasmTypeInfo::kCanonicalElementTypeOffset),
          static_cast<uint32_t>(sizeof(uint32_t)), real_object_address,
          real_map_address, map,
          "wasm_type_info_canonical_element_type_truncated",
          "Wasm type info is too small for canonical_element_type")) {
    return false;
  }
  i::Tagged<i::WasmTypeInfo> type_info =
      i::UncheckedCast<i::WasmTypeInfo>(fake_type_info->fake_object());
  i::wasm::CanonicalValueType element_type = type_info->element_type();
  if (!element_type.is_valid()) {
    ReportCorrupt("wasm_type_info_canonical_element_type_invalid",
                  "Wasm type info contained an invalid canonical_element_type",
                  "object_body", real_object_address, real_map_address,
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()));
    return false;
  }
  *is_reference = element_type.is_reference();
  return true;
}

// Breadcrumb: WasmDispatchTable::BodyDescriptor from
// objects-body-descriptors-inl.h.
void ObjectTranslator::WasmDispatchTableIterateBody(
    i::Tagged<i::Map> map, i::Tagged<i::HeapObject> obj, int object_size,
    i::ObjectVisitor* v) {
  // Self indirect pointers require offheap/indirect-pointer resolution that
  // this translator path does not support today, so only the protected pointer
  // fields are visited here.
  i::BodyDescriptorBase::IterateProtectedPointer(
      obj, i::WasmDispatchTable::kProtectedOffheapDataOffset, v);
  i::BodyDescriptorBase::IterateProtectedPointer(
      obj, i::WasmDispatchTable::kProtectedUsesOffset, v);
  int length =
      i::UncheckedCast<i::WasmDispatchTable>(obj)->length(kAcquireLoad);
  if (length < 0 || length > i::WasmDispatchTable::kMaxLength) {
    ReportCorrupt("wasm_dispatch_table_length_invalid",
                  "WasmDispatchTable length is invalid", "object_body",
                  obj.address(), map.address(),
                  static_cast<uint32_t>(map->instance_type()),
                  static_cast<uint32_t>(map->visitor_id()));
    return;
  }
  if (length > 0) {
    const int entries_end = static_cast<int>(
        i::WasmDispatchTable::OffsetOf(length - 1) +
        i::WasmDispatchTable::kImplicitArgBias + i::kTaggedSize);
    if (entries_end > object_size) {
      ReportCorrupt("wasm_dispatch_table_entries_out_of_range",
                    "WasmDispatchTable entries are outside the dumped object "
                    "payload",
                    "object_body", obj.address(), map.address(),
                    static_cast<uint32_t>(map->instance_type()),
                    static_cast<uint32_t>(map->visitor_id()));
      return;
    }
  }
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
