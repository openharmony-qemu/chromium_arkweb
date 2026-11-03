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

#ifndef TRANSLATE_OBJECT_LIST_H
#define TRANSLATE_OBJECT_LIST_H

#include "torque-generated/visitor-lists.h"

// Defines DFX_IF_WASM, to be used in macro lists for elements that should only
// be there if WebAssembly is enabled.
#if V8_ENABLE_WEBASSEMBLY
// EXPAND is needed to work around MSVC's broken __VA_ARGS__ expansion.
#define DFX_IF_WASM(V, ...) EXPAND(V(__VA_ARGS__))
#else
#define DFX_IF_WASM(V, ...)
#endif  // V8_ENABLE_WEBASSEMBLY

// ---------- Data Only related ----------
// copy from map.h
#define DFX_DATA_ONLY_VISITOR_ID_LIST(V) \
  V(BigInt)                              \
  V(CoverageInfo)                        \
  V(FeedbackMetadata)                    \
  V(Filler)                              \
  V(HeapNumber)                          \
  V(Hole)                                \
  V(SeqOneByteString)                    \
  V(SeqTwoByteString)                    \
  DFX_IF_WASM(V, WasmNull)

// copy from src/out/musl_64/gen/v8/torque-generated/visitor-lists.h
#define DFX_TORQUE_DATA_ONLY_VISITOR_ID_LIST(V) \
  TORQUE_DATA_ONLY_VISITOR_ID_LIST(V)
// V(TurbofanBitsetType)                         \
// V(TurbofanRangeType)                          \
// V(TurbofanOtherNumberConstantType)            \
// V(TurboshaftWord32Type)                       \
// V(TurboshaftWord32RangeType)                  \
// V(TurboshaftWord32SetType)                    \
// V(TurboshaftWord64Type)                       \
// V(TurboshaftWord64RangeType)                  \
// V(TurboshaftWord64SetType)                    \
// V(TurboshaftFloat64Type)                      \
// V(TurboshaftFloat64RangeType)                 \
// V(TurboshaftFloat64SetType)                   \
// V(AbstractInternalClassSubclass1)             \
// V(AbstractInternalClassSubclass2)

// self defined
#define DFX_HEAP_OBJECT_NO_NEED_DUMP_ID_LIST(V) \
  V(ByteArray)                                  \
  V(FixedDoubleArray)                           \
  V(CppHeapExternalObject)                      \
  V(TrustedByteArray)                           \
  V(ExternalString)

// ---------- TYPED_VISITOR_ID_LIST related ----------

// copy from object-list-macros.h
#define DFX_SIMPLE_HEAP_OBJECT_LIST_GENERATOR(APPLY, V)                   \
  APPLY(V, ArrayList, ARRAY_LIST)                                         \
  /* APPLY(V, ByteArray, BYTE_ARRAY)  should be DataOnly */               \
  APPLY(V, ClosureFeedbackCellArray, CLOSURE_FEEDBACK_CELL_ARRAY)         \
  APPLY(V, FixedArray, FIXED_ARRAY)                                       \
  /* APPLY(V, FixedDoubleArray, FIXED_DOUBLE_ARRAY)  should be DataOnly*/ \
  APPLY(V, ObjectBoilerplateDescription, OBJECT_BOILERPLATE_DESCRIPTION)  \
  APPLY(V, RegExpMatchInfo, REG_EXP_MATCH_INFO)                           \
  APPLY(V, ScriptContextTable, SCRIPT_CONTEXT_TABLE)                      \
  APPLY(V, WeakFixedArray, WEAK_FIXED_ARRAY)

// The DFX_SIMPLE_HEAP_OBJECT_LIST1 format is:
//   V(TypeCamelCase)
//
#define DFX_SIMPLE_HEAP_OBJECT_LIST1_ADAPTER(V, Name, NAME) V(Name)
#define DFX_SIMPLE_HEAP_OBJECT_LIST1(V) \
  DFX_SIMPLE_HEAP_OBJECT_LIST_GENERATOR(DFX_SIMPLE_HEAP_OBJECT_LIST1_ADAPTER, V)

// copy from heap-visitor.h
#define DFX_TYPED_VISITOR_ID_LIST(V)                                     \
  V(AccessorInfo)                                                        \
  V(AllocationSite)                                                      \
  /* V(BigInt)  DataOnly*/                                               \
  /* todo(hh):arkweb IterateTrustedPointer*/                             \
  V(BytecodeWrapper)                                                     \
  /* todo(hh):see call-site-info.h,arkweb IterateTrustedPointer  */      \
  V(CallSiteInfo)                                                        \
  V(Cell) /* cell.h */                                                   \
  /* todo(hh):arkweb IterateTrustedPointer */                            \
  V(CodeWrapper)                                                         \
  V(ConsString)  /* FixedBodyDescriptor in string.h */                   \
  V(ContextCell) /* FixedBodyDescriptor in contexts.h */                 \
  /* V(CoverageInfo)  DataOnly*/                                         \
  V(DataHandler)                                                         \
  /* todo(hh):IterateTrustedPointer */                                   \
  V(DebugInfo)                                                           \
  V(DoubleStringCache)                                                   \
  V(EmbedderDataArray)                                                   \
  V(EphemeronHashTable)                                                  \
  /* V(ExternalString) no need dump */                                   \
  V(FeedbackCell)                                                        \
  /* V(FeedbackMetadata)  DataOnly */                                    \
  V(Foreign)                                                             \
  V(FunctionTemplateInfo)                                                \
  /* V(HeapNumber)  DataOnly */                                          \
  V(InterceptorInfo)                                                     \
  /* V(Hole)  DataOnly */                                                \
  V(Map)                                                                 \
  V(NativeContext)                                                       \
  V(Oddball) /* FixedBodyDescriptor in oddball.h */                      \
  V(PreparseData)                                                        \
  V(PropertyArray) /* FlexibleBodyDescriptor in property-array.h */      \
  V(PropertyCell)  /* FixedBodyDescriptor in property-cell.h */          \
  V(PrototypeInfo)                                                       \
  V(RegExpBoilerplateDescription) /* StructBodyDescriptor in             \
                                     literal-objects.h/struct.h */       \
  /* todo(hh):IterateTrustedPointer */                                   \
  V(RegExpDataWrapper)                                                   \
  /* V(SeqOneByteString)  DataOnly */                                    \
  /* V(SeqTwoByteString)  DataOnly */                                    \
  /* todo(hh):IterateTrustedPointer */                                   \
  V(SharedFunctionInfo)                                                  \
  V(SlicedString) /* FixedBodyDescriptor in string.h */                  \
  V(SloppyArgumentsElements)                                             \
  V(SmallOrderedHashMap)        /* SmallOrderedHashTable */              \
  V(SmallOrderedHashSet)        /* SmallOrderedHashTable */              \
  V(SmallOrderedNameDictionary) /* SmallOrderedHashTable */              \
  V(SourceTextModule) /* ParentBodyDescriptor in source-text-module.h */ \
  V(SwissNameDictionary)                                                 \
  V(Symbol)          /* FixedBodyDescriptor in name.h */                 \
  V(SyntheticModule) /* SubclassBodyDescriptor in synthetic-module.h */  \
  V(ThinString)      /* FixedBodyDescriptor in string.h */               \
  V(TransitionArray) /* transitions.h */                                 \
  V(WeakCell)                                                            \
  /* WasmArray intentionally stays out of the generic visitor lists;     \
   * translate-objects.cc special handling emits ref element edges and   \
   * avoids scanning scalar payload as tagged slots. */                  \
  /* todo:IterateTrustedPointer */                                       \
  DFX_IF_WASM(V, WasmFuncRef)                                            \
  DFX_IF_WASM(V, WasmMemoryMapDescriptor)                                \
  /* DFX_IF_WASM(V, WasmNull)  DataOnly */                               \
  /* todo:IterateTrustedPointer */                                       \
  DFX_IF_WASM(V, WasmResumeData)                                         \
  /* WasmStruct intentionally stays out of the generic visitor lists;    \
   * translate-objects.cc uses the dumped offheap canonical type table   \
   * instead of generic body-descriptor traversal. */                    \
  /* DFX_IF_WASM(V, WasmStruct) */                                       \
  DFX_IF_WASM(V, WasmContinuationObject) /* wasm-objects.h */            \
  DFX_IF_WASM(V, WasmTypeInfo)                                           \
  DFX_SIMPLE_HEAP_OBJECT_LIST1(V)

// ---------- TYPED_VISITOR_WITH_SLACK_ID_LIST related ----------
// copy from heap-visitor.h
#define DFX_TYPED_VISITOR_WITH_SLACK_ID_LIST(V)                   \
  V(JSArrayBuffer)                                                \
  V(JSDataViewOrRabGsabDataView)                                  \
  V(JSDate)                                                       \
  V(JSExternalObject)                                             \
  V(JSFinalizationRegistry)                                       \
  V(JSFunction)                                                   \
  V(JSObject)                                                     \
  /* todo(hh): IterateTrustedPointer */                           \
  V(JSRegExp)                                                     \
  V(JSSynchronizationPrimitive)                                   \
  V(JSTypedArray)                                                 \
  V(JSWeakCollection)                                             \
  V(JSWeakRef)                                                    \
  /*todo:IterateTrustedPointer*/                                  \
  DFX_IF_WASM(V, WasmGlobalObject)                                \
  DFX_IF_WASM(V, WasmInstanceObject)                              \
  DFX_IF_WASM(V, WasmMemoryObject)                                \
  DFX_IF_WASM(V, WasmSuspendingObject) /* subclass of JSObject */ \
  DFX_IF_WASM(V, WasmTableObject)                                 \
  DFX_IF_WASM(V, WasmTagObject)

// ---------- TORQUE_VISITOR_ID_LIST related ----------
// copy from map.h
#define DFX_TORQUE_POINTER_VISITOR_ID_LIST(V) TORQUE_POINTER_VISITOR_ID_LIST(V)
// the following objects descriptor all in objects-body-descriptors-inl.inc
//   V(Context)                              \
//   V(JSProxy)                              \
//   V(WeakArrayList)                        \
//   V(ScopeInfo)                            \
//   V(DescriptorArray)                      \
//   V(StrongDescriptorArray)                \
//   V(FeedbackVector)                       \
//   V(MegaDomHandler)                       \
//   V(OnHeapBasicBlockProfilerData)         \
//   V(DictionaryTemplateInfo)               \
//   V(TurbofanUnionType)                    \
//   V(TurbofanHeapConstantType)             \
//   V(InternalClass)                        \
//   V(SmiPair)                              \
//   V(SmiBox)                               \
//   V(ExportedSubClassBase)                 \
//   V(ExportedSubClass)                     \
//   V(InternalClassWithStructElements)      \
//   V(ExportedSubClass2)                    \
//   V(SortState)                            \
//   V(WasmFastApiCallData)                  \
//   V(WasmStringViewIter)

#define DFX_TORQUE_VISITOR_ID_LIST(V) DFX_TORQUE_POINTER_VISITOR_ID_LIST(V)

// ---------- TRUSTED_VISITOR_ID_LIST related ----------

// copy from map.h
// Concrete trusted objects. These must:
// - (Transitively) inherit from TrustedObject
// - Have a unique instance type
// - Define a custom body descriptor
#define DFX_CONCRETE_TRUSTED_OBJECT_LIST_GENERATOR(APPLY, V)                   \
  APPLY(V, BytecodeArray, BYTECODE_ARRAY)                                      \
  /* todo: IterateSelfIndirectPointer for arkweb */                            \
  APPLY(V, Code, CODE)                                                         \
  /* APPLY(V, InstructionStream, INSTRUCTION_STREAM) deal it specially */      \
  /* todo: IterateSelfIndirectPointer for arkweb */                            \
  APPLY(V, InterpreterData, INTERPRETER_DATA)                                  \
  APPLY(V, UncompiledDataWithPreparseData, UNCOMPILED_DATA_WITH_PREPARSE_DATA) \
  APPLY(V, UncompiledDataWithoutPreparseData,                                  \
        UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA)                                 \
  APPLY(V, UncompiledDataWithPreparseDataAndJob,                               \
        UNCOMPILED_DATA_WITH_PREPARSE_DATA_AND_JOB)                            \
  APPLY(V, UncompiledDataWithoutPreparseDataWithJob,                           \
        UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_WITH_JOB)                        \
  APPLY(V, SharedFunctionInfoWrapper, SHARED_FUNCTION_INFO_WRAPPER)            \
  APPLY(V, ProtectedFixedArray, PROTECTED_FIXED_ARRAY)                         \
  APPLY(V, ProtectedWeakFixedArray, PROTECTED_WEAK_FIXED_ARRAY)                \
  /* APPLY(V, TrustedByteArray, TRUSTED_BYTE_ARRAY)  Data Only */              \
  APPLY(V, TrustedFixedArray, TRUSTED_FIXED_ARRAY)                             \
  APPLY(V, TrustedForeign, TRUSTED_FOREIGN) /* foreign.h */                    \
  APPLY(V, TrustedWeakFixedArray, TRUSTED_WEAK_FIXED_ARRAY)                    \
  APPLY(V, AtomRegExpData, ATOM_REG_EXP_DATA)                                  \
  APPLY(V, IrRegExpData, IR_REG_EXP_DATA)                                      \
  APPLY(V, RegExpData, REG_EXP_DATA)                                           \
  DFX_IF_WASM(APPLY, V, WasmImportData, WASM_IMPORT_DATA) /* wasm-objects.h */ \
  DFX_IF_WASM(APPLY, V, WasmCapiFunctionData, WASM_CAPI_FUNCTION_DATA)         \
  /* deal specially:InAnySharedSpace in body desc */                           \
  /*DFX_IF_WASM(APPLY, V, WasmDispatchTable, WASM_DISPATCH_TABLE)*/            \
  DFX_IF_WASM(APPLY, V, WasmDispatchTableForImports,                           \
              WASM_DISPATCH_TABLE_FOR_IMPORTS)                                 \
  DFX_IF_WASM(APPLY, V, WasmExportedFunctionData, WASM_EXPORTED_FUNCTION_DATA) \
  DFX_IF_WASM(APPLY, V, WasmJSFunctionData, WASM_JS_FUNCTION_DATA)             \
  DFX_IF_WASM(APPLY, V, WasmInternalFunction, WASM_INTERNAL_FUNCTION)          \
  /* deal specially:InAnySharedSpace in body desc */                           \
  /*DFX_IF_WASM(APPLY, V, WasmTrustedInstanceData,                             \
                WASM_TRUSTED_INSTANCE_DATA)*/                                  \
  DFX_IF_WASM(APPLY, V, WasmSuspenderObject, WASM_SUSPENDER_OBJECT)

#define DFX_TRUSTED_OBJECT_LIST1_ADAPTER(V, Name, NAME) V(Name)
#define DFX_CONCRETE_TRUSTED_OBJECT_TYPE_LIST1(V)                              \
  DFX_CONCRETE_TRUSTED_OBJECT_LIST_GENERATOR(DFX_TRUSTED_OBJECT_LIST1_ADAPTER, \
                                             V)

#define DFX_TRUSTED_VISITOR_ID_LIST(V) DFX_CONCRETE_TRUSTED_OBJECT_TYPE_LIST1(V)

#endif
