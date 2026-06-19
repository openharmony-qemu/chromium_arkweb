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
#include <set>
#include <unordered_map>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/binary-reader-base.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-format.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/snapshot-generator.h"
#include "include/v8-internal.h"
#include "src/common/ptr-compr.h"
#include "src/objects/foreign.h"
#include "src/objects/prototype-info.h"

namespace v8 {
namespace dfx {

class SnapshotGenerator;

class ObjectTranslator {
 public:
  explicit ObjectTranslator(BinaryReaderBase* reader, uint32_t offset,
                            uint32_t size, uint32_t count,
                            i::PtrComprCageBase cage_base,
                            i::PtrComprCageBase code_cage_base,
                            SnapshotGenerator* generator);
  ~ObjectTranslator() = default;
  void Translate();

  SnapshotGenerator* generator() { return generator_; }

  struct RawHeapObjectHead {
    uint32_t offset_;     // offset in raw heap
    uint32_t size_ : 31;  // size in raw heap
#if defined(OH_ENABLE_HEAP_DUMP_TEST)
    uint32_t is_map_ : 1;  // indicate this heap object a map or not
#endif
  };

 private:
  void PreVisit();
  void TranslateHeapObject(i::Address addr, uint32_t size);
  void TranslateInstructionStream(i::Address addr, uint32_t size);
  bool IsStrong(i::Address addr);

  ////////// AddNode
  Node* AddNode(i::Tagged<i::Map> map, i::Tagged<i::HeapObject> object,
                i::Address address, uint32_t size);
  std::string GetForeignName(i::Tagged<i::Foreign> foreign,
                             uint32_t* node_size);
  std::string GetClassName(i::Tagged<i::Map> map,
                           i::Tagged<i::HeapObject> object, i::Address address);
  bool IsShared(i::InstanceType instance_type);
  bool IsBoolean(i::InstanceType instance_type, i::Tagged<i::Object> obj);
  Node::Type GetSystemEntryType(i::InstanceType type);
  const char* GetSystemEntryName(i::InstanceType type);

  inline std::string get_name(i::Tagged<i::JSFunction> func);
  inline std::string get_name(i::Tagged<i::SharedFunctionInfo> sfi);
  // Helper to read string content from a fake String object
  std::string get_string_content(i::Address string_addr);
  std::string get_string_content(i::Tagged<i::String> string,
                                 i::InstanceType instance_type);

  ////////// ExtractReference
  void ExtractReferences(i::Tagged<i::HeapObject> obj, i::Tagged<i::Map> map,
                         i::Address real_address);
  void ExtractJSGlobalProxyReferences(i::Address real_address,
                                      i::Tagged<i::JSGlobalProxy> proxy,
                                      i::Tagged<i::Map> map);
  void ExtractJSObjectReferences(i::Address real_address,
                                 i::Tagged<i::JSObject> js_obj,
                                 i::Tagged<i::Map> map);
  void ExtractPropertyReferences(i::Address real_address,
                                 i::Tagged<i::JSObject> js_obj,
                                 i::Tagged<i::Map> map);
  void ExtractElementReferences(i::Address real_address,
                                i::Tagged<i::JSObject> js_obj,
                                i::Tagged<i::Map> map);
  void ExtractInternalReferences(i::Address real_address,
                                 i::Tagged<i::JSObject> js_obj,
                                 i::Tagged<i::Map> map);
  void ExtractJSWeakCollectionReferences(i::Address real_address,
                                         i::Tagged<i::JSWeakCollection> obj);
  void ExtractJSArrayBufferReferences(i::Address real_address,
                                      i::Tagged<i::JSArrayBuffer> buffer);
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
  void ExtractMapReferences(i::Address real_address, i::Tagged<i::Map> map);
  void ExtractSharedFunctionInfoReferences(
      i::Address real_address, i::Tagged<i::SharedFunctionInfo> shared,
      i::Tagged<i::Map> map);
  void ExtractScriptReferences(i::Address real_address,
                               i::Tagged<i::Script> script,
                               i::Tagged<i::Map> map);
  void ExtractAccessorInfoReferences(i::Address real_address,
                                     i::Tagged<i::AccessorInfo> accessor_info);
  void ExtractAccessorPairReferences(i::Address real_address,
                                     i::Tagged<i::AccessorPair> accessors);
  void ExtractCodeReferences(i::Address real_address, i::Tagged<i::Code> code);
  void ExtractInstructionStreamReferences(
      i::Address real_address, i::Tagged<i::InstructionStream> istream);
  void ExtractCellReferences(i::Address real_address, i::Tagged<i::Cell> cell);
  void ExtractFeedbackCellReferences(i::Address real_address,
                                     i::Tagged<i::FeedbackCell> feedback_cell);
  void ExtractPropertyCellReferences(i::Address real_address,
                                     i::Tagged<i::PropertyCell> cell);
  void ExtractPrototypeInfoReferences(i::Address real_address,
                                      i::Tagged<i::PrototypeInfo> info);
  void ExtractAllocationSiteReferences(i::Address real_address,
                                       i::Tagged<i::AllocationSite> site);
  void ExtractArrayBoilerplateDescriptionReferences(
      i::Address real_address, i::Tagged<i::ArrayBoilerplateDescription> value);
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
  void ExtractWasmModuleObjectReferences(
      i::Address real_address, i::Tagged<i::WasmModuleObject> module_object);
  void ExtractWasmStructReferences(i::Address real_address,
                                   i::Tagged<i::WasmStruct> obj);
  void ExtractWasmArrayReferences(i::Address real_address,
                                  i::Tagged<i::WasmArray> obj,
                                  i::Tagged<i::Map> map);
  void ExtractWasmTrustedInstanceDataReferences(
      i::Address real_address, i::Tagged<i::HeapObject> trusted_data);
#endif

  ////////// helper
  i::InstanceType GetInstanceType(i::Address real_address);
#ifdef V8_ENABLE_WEBASSEMBLY
  bool IsReferenceForWasmArray(i::Tagged<i::Map> map);
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

  // record related
  // record all address
  std::unordered_map<i::Address, RawHeapObjectHead> objects_head_;
  SnapshotGenerator* generator_;
};
}  // namespace dfx
}  // namespace v8
#endif
#endif
