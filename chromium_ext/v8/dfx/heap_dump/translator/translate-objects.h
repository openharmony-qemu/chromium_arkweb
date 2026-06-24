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

#ifndef TRANSLATE_OBJECTS_H
#define TRANSLATE_OBJECTS_H
#if (defined(OH_ENABLE_HEAP_TRANSLATE) || defined(OH_ENABLE_HEAP_DUMP_TEST))
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/binary-reader-base.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-format.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/snapshot-generator.h"
#include "include/v8-internal.h"
#include "src/common/ptr-compr.h"
#include "src/objects/dictionary.h"
#include "src/objects/field-index.h"
#include "src/objects/foreign.h"
#include "src/objects/property-details.h"
#include "src/objects/prototype-info.h"
#include "src/objects/swiss-name-dictionary.h"

namespace v8 {
namespace internal {
class RegExpBoilerplateDescription;
}
namespace dfx {

class SnapshotGenerator;
class TranslateDiagnostics;
class FakeHeapObject;
class OffheapTableResolver;

class ObjectTranslator {
 public:
  explicit ObjectTranslator(
      BinaryReaderBase* reader, uint32_t offset, uint32_t size, uint32_t count,
      i::PtrComprCageBase cage_base, i::PtrComprCageBase code_cage_base,
      bool has_trusted_cage_base, SnapshotGenerator* generator,
      TranslateDiagnostics* diagnostics = nullptr,
      const OffheapTableResolver* offheap_table_resolver = nullptr);
  ~ObjectTranslator() = default;
  bool Translate();

  SnapshotGenerator* generator() { return generator_; }

#if defined(OH_ENABLE_HEAP_DUMP_TEST)
  bool TestLookupFakeObjectMapAddress(i::Address real_address,
                                      i::Address* map_address);
  const char* TestGetSystemEntryName(i::InstanceType type) {
    return GetSystemEntryName(type);
  }
#endif

  struct RawHeapObjectHead {
    uint32_t offset_;      // offset in raw heap
    uint32_t size_ : 31;   // size in raw heap
    uint32_t is_map_ : 1;  // indicate this heap object a map or not
  };

 private:
  struct ContextLocalName {
    std::string name;
    int index;
  };

  friend class ObjectDetailVisitor;

  bool PreVisit();
  bool TranslateHeapObject(i::Address addr, uint32_t size);
  bool TranslateInstructionStream(i::Address addr, uint32_t size);
  bool IsStrong(i::Address addr);
  const RawHeapObjectHead* FindObjectHead(i::Address real_address) const;
  std::unique_ptr<FakeHeapObject> MakeFakeHeapObject(i::Address real_address);
  void ReportFatal(const char* code, const char* message, const char* section);
  void ReportCorrupt(const char* code, const char* message, const char* section,
                     i::Address object_address = 0, i::Address map_address = 0,
                     uint32_t instance_type = 0, uint32_t visitor_id = 0,
                     uint32_t rawheap_offset = 0);
  void ReportWarning(const char* code, const char* message, const char* section,
                     i::Address object_address = 0, i::Address map_address = 0,
                     uint32_t instance_type = 0, uint32_t visitor_id = 0,
                     uint32_t rawheap_offset = 0);
  void ReportUnsupported(const char* code, const char* message,
                         const char* section, i::Address object_address = 0,
                         i::Address map_address = 0, uint32_t instance_type = 0,
                         uint32_t visitor_id = 0, uint32_t rawheap_offset = 0);

  ////////// AddNode
  Node* AddNode(i::Tagged<i::Map> map, i::Tagged<i::HeapObject> object,
                i::Address address, uint32_t size);
  std::string GetForeignName(i::Tagged<i::Foreign> foreign,
                             uint32_t* node_size);
  std::string GetClassName(i::Tagged<i::Map> map,
                           i::Tagged<i::HeapObject> object, i::Address address,
                           uint32_t size);
  bool IsShared(i::InstanceType instance_type);
  bool IsBoolean(i::InstanceType instance_type, i::Tagged<i::Object> obj);
  bool IsUndefinedOddballOffline(i::Address address);
  Node::Type GetSystemEntryType(i::InstanceType type);
  // Returns nullptr for malformed/rawheap-only InstanceType values that have no
  // safe system-entry name.
  const char* GetSystemEntryName(i::InstanceType type);

  inline std::string get_name(i::Tagged<i::JSFunction> func);
  inline std::string get_name(i::Tagged<i::SharedFunctionInfo> sfi);
  // Helper to read string content from a fake String object
  std::string get_string_content(i::Address string_addr);
  std::string get_string_content(i::Tagged<i::String> string,
                                 i::InstanceType instance_type);

  ////////// ExtractReference
  bool ExtractReferences(i::Tagged<i::HeapObject> obj, i::Tagged<i::Map> map,
                         i::Address real_address, i::Address real_map_address);
  bool ExtractJSGlobalProxyReferences(i::Address real_address,
                                      i::Tagged<i::JSGlobalProxy> proxy,
                                      i::Tagged<i::Map> map,
                                      i::Address real_map_address);
  bool GetCreationContextOffline(i::Address real_address, i::Tagged<i::Map> map,
                                 i::Address real_map_address,
                                 i::Address* native_context_addr);
  void GetNativeContextGlobalObjectOffline(i::Address native_context_addr,
                                           i::Address* global_object_addr);
  void ExtractJSObjectReferences(i::Address real_address,
                                 i::Tagged<i::JSObject> js_obj,
                                 i::Tagged<i::Map> map);
  void SetInternalReferenceFromJSDispatchEntry(i::Address real_address,
                                               std::string reference_name,
                                               i::Tagged<i::HeapObject> object,
                                               int offset);
  std::unique_ptr<FakeHeapObject> GetJSObjectOwnDescriptors(
      i::Address real_address, i::Tagged<i::Map> map);
  std::string GetPropertyNameFromKey(i::Tagged<i::Name> key);
  std::unique_ptr<FakeHeapObject> GetJSGlobalObjectGlobalDictionary(
      i::Address real_address, i::Tagged<i::Map> map,
      i::Tagged<i::JSGlobalObject> global_object);
  std::unique_ptr<FakeHeapObject> GetGlobalDictionaryCellAt(
      i::Address real_address, i::Tagged<i::Map> map,
      i::Tagged<i::GlobalDictionary> dictionary, i::InternalIndex entry);
  std::unique_ptr<FakeHeapObject> GetJSObjectSwissNameDictionary(
      i::Address real_address, i::Tagged<i::Map> map,
      i::Tagged<i::JSObject> js_obj);
  bool LoadSwissNameDictionaryProperty(
      i::Address real_address, i::Tagged<i::Map> map,
      i::Tagged<i::SwissNameDictionary> dictionary, i::InternalIndex entry,
      std::string* name, bool* name_unresolved, i::Address* value_address,
      i::PropertyKind* kind);
  std::unique_ptr<FakeHeapObject> GetJSObjectNameDictionary(
      i::Address real_address, i::Tagged<i::Map> map,
      i::Tagged<i::JSObject> js_obj);
  bool LoadNameDictionaryProperty(i::Address real_address,
                                  i::Tagged<i::Map> map,
                                  i::Tagged<i::NameDictionary> dictionary,
                                  i::InternalIndex entry, std::string* name,
                                  bool* name_unresolved,
                                  i::Address* value_address,
                                  i::PropertyKind* kind);
  void SetDataOrAccessorPropertyReference(i::Address real_address,
                                          i::PropertyKind kind,
                                          const std::string& reference_name,
                                          i::Address child_address,
                                          int field_offset,
                                          bool name_is_unresolved = false);
  void ExtractAccessorPairProperty(i::Address real_address,
                                   const std::string& reference_name,
                                   i::Address child_address, int field_offset,
                                   bool name_is_unresolved = false);
  bool LoadRawFastPropertyAt(i::Address real_address,
                             i::Tagged<i::JSObject> js_obj,
                             i::Tagged<i::Map> map, i::FieldIndex field_index,
                             i::Address* value_address);
  void ExtractPropertyReferences(i::Address real_address,
                                 i::Tagged<i::JSObject> js_obj,
                                 i::Tagged<i::Map> map);
  std::unique_ptr<FakeHeapObject> GetJSObjectElements(
      i::Address real_address, i::Tagged<i::JSObject> js_obj,
      i::Tagged<i::Map> map);
  std::unique_ptr<FakeHeapObject> GetJSObjectNumberDictionaryElements(
      i::Address real_address, i::Tagged<i::JSObject> js_obj,
      i::Tagged<i::Map> map);
  bool LoadObjectElementsLength(i::Address real_address,
                                i::Tagged<i::JSObject> js_obj,
                                i::Tagged<i::Map> map,
                                i::Tagged<i::FixedArray> elements, int* length);
  bool LoadNotHoleElement(i::Tagged<i::FixedArray> elements, int index,
                          i::Address* child);
  bool LoadNumberDictionaryElement(i::Address real_address,
                                   i::Tagged<i::Map> map,
                                   i::Tagged<i::NumberDictionary> dictionary,
                                   i::InternalIndex entry, uint32_t* index,
                                   i::Address* child);
  void ExtractElementReferences(i::Address real_address,
                                i::Tagged<i::JSObject> js_obj,
                                i::Tagged<i::Map> map);
  bool LoadEmbedderField(i::Address real_address, i::Tagged<i::JSObject> js_obj,
                         i::Tagged<i::Map> map, int index, i::Address* child,
                         int* field_offset);
  void ExtractInternalReferences(i::Address real_address,
                                 i::Tagged<i::JSObject> js_obj,
                                 i::Tagged<i::Map> map);
  bool LoadJSObjectPrototype(i::Address real_address,
                             i::Tagged<i::JSObject> js_obj,
                             i::Tagged<i::Map> map,
                             i::Address* prototype_address);
  bool LoadJSFunctionPrototypeOrInitialMapIfNotHole(
      i::Address real_address, i::Tagged<i::Map> map,
      i::Tagged<i::JSFunction> js_fun, i::Address* proto_or_map_address,
      i::InstanceType* proto_or_map_type);
  bool LoadJSFunctionPrototype(i::Address real_address, i::Tagged<i::Map> map,
                               i::Address initial_map_address,
                               i::Address* prototype_address);
  bool LoadJSBoundFunctionReference(i::Address real_address,
                                    i::Tagged<i::Map> map,
                                    i::Tagged<i::JSBoundFunction> bound_fun,
                                    int offset, i::Address* target_address);
  std::unique_ptr<FakeHeapObject> GetJSBoundFunctionBoundArguments(
      i::Address real_address, i::Tagged<i::Map> map,
      i::Tagged<i::JSBoundFunction> bound_fun);
  bool LoadJSGlobalObjectGlobalProxy(i::Address real_address,
                                     i::Tagged<i::Map> map,
                                     i::Tagged<i::JSGlobalObject> global_obj,
                                     i::Address* global_proxy_address);
  bool LoadJSArrayBufferViewBuffer(i::Address real_address,
                                   i::Tagged<i::Map> map,
                                   i::Tagged<i::JSArrayBufferView> view,
                                   i::Address* buffer_address);
  bool LoadJSObjectRawPropertiesOrHash(i::Address real_address,
                                       i::Tagged<i::Map> map,
                                       i::Tagged<i::JSObject> js_obj,
                                       i::Address* properties_address);
  bool LoadJSObjectElements(i::Address real_address, i::Tagged<i::Map> map,
                            i::Tagged<i::JSObject> js_obj,
                            i::Address* elements_address);
  bool LoadMapDependentCode(i::Address real_address, i::Tagged<i::Map> map,
                            i::Address* dependent_code_address,
                            i::InstanceType* dependent_code_type);
  bool LoadMapPrototypeValidityCell(
      i::Address real_address, i::Tagged<i::Map> map,
      i::Address* prototype_validity_cell_address,
      i::InstanceType* prototype_validity_cell_type);
  void ExtractJSWeakCollectionReferences(i::Address real_address,
                                         i::Tagged<i::JSWeakCollection> obj);
  void ExtractJSArrayBufferReferences(i::Address real_address,
                                      i::Tagged<i::JSArrayBuffer> buffer);
  void SetStringInternalReference(i::Address real_address,
                                  i::Tagged<i::String> string,
                                  i::InstanceType instance_type,
                                  const char* name, int offset);
  void ExtractStringReferences(i::Address real_address,
                               i::Tagged<i::String> string,
                               i::InstanceType instance_type);
  void ExtractSymbolReferences(i::Address real_address,
                               i::Tagged<i::Symbol> symbol);
  void ExtractJSCollectionReferences(i::Address real_address,
                                     i::Tagged<i::JSCollection> collection);
  void ExtractJSPromiseReferences(i::Address real_address,
                                  i::Tagged<i::JSPromise> promise);
  void ExtractJSGeneratorObjectReferences(
      i::Address real_address, i::Tagged<i::JSGeneratorObject> generator);
  void ExtractJSWeakRefReferences(i::Address real_address,
                                  i::Tagged<i::JSWeakRef> js_weak_ref);
  void ExtractJSDisposableStackReferences(
      i::Address real_address,
      i::Tagged<i::JSDisposableStackBase> disposable_stack);
  bool LoadMapConstructorOrBackPointer(i::Address real_address,
                                       i::Tagged<i::Map> map,
                                       i::Address* value_address,
                                       i::InstanceType* value_type,
                                       bool* has_value);
  bool ExtractMapReferences(i::Address real_address, i::Tagged<i::Map> map);
  void ExtractSharedFunctionInfoReferences(
      i::Address real_address, i::Tagged<i::SharedFunctionInfo> shared,
      i::Tagged<i::Map> map);
  bool LoadScriptLineEnds(i::Address real_address, i::Tagged<i::Script> script,
                          i::Address* line_ends_address);
  bool LoadScriptHostDefinedOptions(i::Address real_address,
                                    i::Tagged<i::Script> script,
                                    i::Address* host_defined_options_address);
  void ExtractScriptReferences(i::Address real_address,
                               i::Tagged<i::Script> script,
                               i::Tagged<i::Map> map);
  void ExtractAccessorInfoReferences(i::Address real_address,
                                     i::Tagged<i::AccessorInfo> accessor_info);
  void ExtractAccessorPairReferences(i::Address real_address,
                                     i::Tagged<i::AccessorPair> accessors);
  bool LoadCodeInstructionStream(i::Address real_address,
                                 i::Tagged<i::Code> code,
                                 i::Address* instruction_stream_address);
  bool LoadCodeProtectedFieldIfExpectedHeapObject(
      i::Address real_address, i::Tagged<i::Code> code, int offset,
      i::Address* child_address, i::InstanceType* child_type);
  bool LoadCodeBytecodeOrInterpreterData(i::Address real_address,
                                         i::Tagged<i::Code> code,
                                         i::Address* child_address);
  bool LoadCodeBytecodeOffsetTable(i::Address real_address,
                                   i::Tagged<i::Code> code,
                                   i::Address* child_address);
  bool LoadCodeDeoptimizationData(i::Address real_address,
                                  i::Tagged<i::Code> code,
                                  i::Address* child_address);
  bool LoadCodeSourcePositionTable(i::Address real_address,
                                   i::Tagged<i::Code> code,
                                   i::Address* child_address);
  void RetagDeoptimizationDataSubtables(i::Address deoptimization_data_address);
  void ExtractCodeReferences(i::Address real_address, i::Tagged<i::Code> code);
  void ExtractInstructionStreamReferences(
      i::Address real_address, i::Tagged<i::InstructionStream> istream);
  void ExtractCellReferences(i::Address real_address, i::Tagged<i::Cell> cell);
  void ExtractFeedbackCellReferences(i::Address real_address,
                                     i::InstanceType instance_type,
                                     i::Tagged<i::FeedbackCell> feedback_cell);
  bool LoadPropertyCellDependentCode(i::Address real_address,
                                     i::Tagged<i::PropertyCell> cell,
                                     i::Address* dependent_code_address,
                                     i::InstanceType* dependent_code_type);
  void ExtractPropertyCellReferences(i::Address real_address,
                                     i::Tagged<i::PropertyCell> cell);
  void ExtractPrototypeInfoReferences(i::Address real_address,
                                      i::Tagged<i::PrototypeInfo> info);
  void ExtractAllocationSiteReferences(i::Address real_address,
                                       i::Tagged<i::AllocationSite> site);
  void ExtractArrayBoilerplateDescriptionReferences(
      i::Address real_address, i::Tagged<i::ArrayBoilerplateDescription> value);
  void ExtractRegExpBoilerplateDescriptionReferences(
      i::Tagged<i::RegExpBoilerplateDescription> value);
  void ExtractFeedbackVectorReferences(
      i::Address real_address, i::Tagged<i::FeedbackVector> feedback_vector);
  void ExtractDescriptorArrayReferences(i::Address real_address,
                                        i::Tagged<i::DescriptorArray> array);
  void ExtractEnumCacheReferences(i::Address real_address,
                                  i::Tagged<i::EnumCache> cache);
  void ExtractTransitionArrayReferences(
      i::Address real_address, i::Tagged<i::TransitionArray> transitions);
  template <typename T>
  void ExtractWeakArrayReferences(int header_size, i::Address real_address,
                                  i::Tagged<T> array);
  void ExtractBytecodeArrayReferences(i::Address real_address,
                                      i::Tagged<i::BytecodeArray> bytecode);
  void ExtractScopeInfoReferences(i::Address real_address,
                                  i::Tagged<i::ScopeInfo> info);
  void ExtractCppHeapExternalReferences(
      i::Address real_address, i::Tagged<i::CppHeapExternalObject> obj);
  void ExtractContextReferences(i::Address real_address,
                                i::Tagged<i::Context> context,
                                i::InstanceType context_type);
  bool GetContextSlotAddress(i::Tagged<i::Context> context, int index,
                             i::Address* address);
  std::unique_ptr<FakeHeapObject> GetContextScopeInfo(
      i::Tagged<i::Context> context);
  bool LoadScopeInfoFunctionName(i::Tagged<i::ScopeInfo> scope_info,
                                 i::Address* name_address);
  std::vector<ContextLocalName> GetScopeInfoContextLocalNames(
      i::Tagged<i::ScopeInfo> scope_info);
  bool SetContextReferenceIfExisting(i::Address real_address,
                                     i::Tagged<i::Context> context,
                                     std::string reference_name, int index);
  bool SetContextFunctionNameReferenceIfExisting(
      i::Address real_address, i::Tagged<i::Context> context,
      i::Tagged<i::ScopeInfo> scope_info);
  bool SetInternalContextSlotIfExisting(i::Address real_address,
                                        i::Tagged<i::Context> context,
                                        const char* edge_name, int index);
  bool SetScopeInfoContextReference(i::Address real_address,
                                    i::Tagged<i::Context> context,
                                    i::Address scope_info_address);
  bool IsDeclarationContextOffline(i::InstanceType context_type,
                                   bool has_scope_info,
                                   i::Tagged<i::ScopeInfo> scope_info);
  bool ContextHasExtensionOffline(i::Tagged<i::Context> context,
                                  bool has_scope_info,
                                  i::Tagged<i::ScopeInfo> scope_info);
  bool LoadFixedArrayLengthIfValid(i::Address real_address,
                                   i::Tagged<i::FixedArray> array,
                                   int* length);
  bool LoadEphemeronHashTableLengthIfValid(
      i::Address real_address, i::Tagged<i::EphemeronHashTable> table,
      int* length);
  bool GetEphemeronHashTableElement(
      i::Tagged<i::EphemeronHashTable> table, int index,
      i::Address* element_address);
  void ExtractFixedArrayReferences(i::Address real_address,
                                   i::Tagged<i::FixedArray> array);
  void ExtractWeakCellReferences(i::Address real_address,
                                 i::Tagged<i::WeakCell> weak_cell);
  void ExtractEphemeronHashTableReferences(
      i::Address real_address, i::Tagged<i::EphemeronHashTable> table);
#if V8_ENABLE_WEBASSEMBLY
  void ExtractWasmInstanceObjectReferences(
      i::Address real_address,
      i::Tagged<i::WasmInstanceObject> instance_object);
  bool GetWasmInstanceObjectTrustedData(
      i::Address real_address, i::Tagged<i::WasmInstanceObject> instance_object,
      i::Address* trusted_data_address);
  void ExtractWasmModuleObjectReferences(
      i::Address real_address, i::Tagged<i::WasmModuleObject> module_object);
  void ExtractWasmStructReferences(i::Address real_address,
                                   i::Tagged<i::WasmStruct> obj,
                                   i::Tagged<i::Map> map,
                                   i::Address real_map_address);
  bool ExtractWasmArrayReferences(i::Address real_address,
                                  i::Tagged<i::WasmArray> obj,
                                  i::Tagged<i::Map> map,
                                  i::Address real_map_address);
  void ExtractWasmTrustedInstanceDataReferences(
      i::Address real_address, i::Tagged<i::HeapObject> trusted_data);
#endif

  ////////// helper
  bool GetInstanceType(i::Address real_address, i::InstanceType* instance_type);
#ifdef V8_ENABLE_WEBASSEMBLY
  bool GetWasmTypeInfoForMap(i::Tagged<i::Map> map,
                             i::Address real_object_address,
                             i::Address real_map_address,
                             std::unique_ptr<FakeHeapObject>* fake_type_info,
                             i::Address* real_type_info_address);
  bool ValidateWasmTypeInfoField(i::Address real_type_info_address,
                                 uint32_t field_offset, uint32_t field_size,
                                 i::Address real_object_address,
                                 i::Address real_map_address,
                                 i::Tagged<i::Map> map, const char* code,
                                 const char* message);
  bool GetWasmTypeIndexForMap(i::Tagged<i::Map> map,
                              i::Address real_object_address,
                              i::Address real_map_address,
                              uint32_t* type_index);
  bool IsReferenceForWasmArray(i::Tagged<i::Map> map,
                               i::Address real_object_address,
                               i::Address real_map_address, bool* is_reference);
  void SetWasmInternalReferenceIfExistingHeapObject(
      i::Address parent, const char* name, i::Tagged<i::HeapObject> object,
      int offset);
  void WasmDispatchTableIterateBody(i::Tagged<i::Map> map,
                                    i::Tagged<i::HeapObject> obj,
                                    int object_size, i::ObjectVisitor* v);
#endif

  BinaryReaderBase* reader_;
  const uint32_t table_offset_;
  const uint32_t table_size_;
  const uint32_t object_count_;
  i::PtrComprCageBase cage_base_;
  i::PtrComprCageBase code_cage_base_;
  bool has_trusted_cage_base_;

  // record related
  // record all address
  std::unordered_map<i::Address, RawHeapObjectHead> objects_head_;
  SnapshotGenerator* generator_;
  TranslateDiagnostics* diagnostics_{nullptr};
  const OffheapTableResolver* offheap_table_resolver_{nullptr};
  bool is_valid_{true};
};
}  // namespace dfx
}  // namespace v8
#endif
#endif
