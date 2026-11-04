#include "arkweb/chromium_ext/v8/dfx/heap_dump/tests/cctest/heap-dump-test-utils.h"

#include <cstring>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-offheap-tables.h"
#include "src/debug/debug-wasm-objects.h"
#include "src/execution/execution.h"
#include "src/handles/handles.h"
#include "src/heap/factory-inl.h"
#include "src/objects/foreign.h"
#include "src/objects/script-inl.h"
#include "src/wasm/canonical-types.h"
#include "src/wasm/struct-types.h"
#include "src/wasm/wasm-arguments.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-result.h"
#include "src/wasm/wasm-value.h"
#include "test/common/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-module-runner.h"

namespace v8 {
namespace internal {
namespace heap {

#if defined(OH_ENABLE_HEAP_DUMP_TEST) && defined(OH_ENABLE_HEAP_DUMP) && \
    defined(OH_ENABLE_HEAP_TRANSLATE) && defined(V8_ENABLE_WEBASSEMBLY)

namespace {

using namespace heap_dump_test;
using wasm::TestSignatures;
using namespace wasm;

struct SyntheticOffheapTable {
  RawHeapHeader header;
  std::vector<uint8_t> bytes;
};

struct WasmStructRefFieldDump {
  std::vector<uint8_t> bytes;
  uint32_t type_index;
  Address struct_address;
  Address target_address;
};

struct VisitorIdExpectation {
  VisitorId id;
  const char* name;
};

constexpr VisitorIdExpectation kWasmStableVisitorExpectations[] = {
    {VisitorId::kVisitWasmArray, "WasmArray"},
    {VisitorId::kVisitWasmContinuationObject, "WasmContinuationObject"},
    {VisitorId::kVisitWasmGlobalObject, "WasmGlobalObject"},
#if V8_TARGET_OS_LINUX
    {VisitorId::kVisitWasmMemoryMapDescriptor, "WasmMemoryMapDescriptor"},
#endif
    {VisitorId::kVisitWasmResumeData, "WasmResumeData"},
    {VisitorId::kVisitWasmSuspendingObject, "WasmSuspendingObject"},
    {VisitorId::kVisitWasmImportData, "WasmImportData"},
    {VisitorId::kVisitWasmCapiFunctionData, "WasmCapiFunctionData"},
    {VisitorId::kVisitWasmJSFunctionData, "WasmJSFunctionData"},
};

bool HasDiagnostic(const TranslateDiagnostics& diagnostics,
                   TranslateDiagnostics::Severity severity, const char* code,
                   const char* section) {
  for (const TranslateDiagnostics::Entry& entry : diagnostics.entries()) {
    if (entry.severity == severity && entry.code == code &&
        entry.section == section) {
      return true;
    }
  }
  return false;
}

template <typename T>
void AppendPod(std::vector<uint8_t>* bytes, const T& value) {
  const size_t offset = bytes->size();
  bytes->resize(offset + sizeof(value));
  memcpy(bytes->data() + offset, &value, sizeof(value));
}

template <typename T>
void WritePodAt(std::vector<uint8_t>* bytes, uint32_t offset, const T& value) {
  CHECK_LE(static_cast<size_t>(offset) + sizeof(value), bytes->size());
  memcpy(bytes->data() + offset, &value, sizeof(value));
}

void WriteTaggedAtForWasmTest(std::vector<uint8_t>* bytes, uint32_t offset,
                              Tagged_t value) {
  WritePodAt(bytes, offset, value);
}

SyntheticOffheapTable BuildSyntheticWasmCanonicalStructTable(
    const std::vector<dfx::RawHeapWasmCanonicalStructRecord>& records,
    const std::vector<dfx::RawHeapWasmCanonicalStructField>& fields) {
  const uint32_t records_size =
      static_cast<uint32_t>(records.size() *
                            sizeof(dfx::RawHeapWasmCanonicalStructRecord));
  const uint32_t fields_size =
      static_cast<uint32_t>(fields.size() *
                            sizeof(dfx::RawHeapWasmCanonicalStructField));
  const uint32_t payload_size = records_size + fields_size;

  SyntheticOffheapTable table;
  table.header.object_table_offset_ = sizeof(RawHeapHeader);
  table.header.object_table_size_ = 0;
  table.header.object_count_ = 0;
  table.header.offheap_table_offset_ = sizeof(RawHeapHeader);
  table.header.offheap_table_size_ =
      sizeof(dfx::RawHeapOffheapTableHeader) + payload_size;
  table.header.offheap_table_count_ = 1;

  dfx::RawHeapOffheapTableHeader table_header{
      static_cast<uint32_t>(
          dfx::RawHeapOffheapTableKind::kWasmCanonicalTypeTable),
      static_cast<uint32_t>(sizeof(dfx::RawHeapOffheapTableHeader)),
      payload_size, static_cast<uint32_t>(records.size())};

  table.bytes.resize(sizeof(RawHeapHeader));
  WritePodAt(&table.bytes, 0, table.header);
  AppendPod(&table.bytes, table_header);
  for (const dfx::RawHeapWasmCanonicalStructRecord& record : records) {
    AppendPod(&table.bytes, record);
  }
  for (const dfx::RawHeapWasmCanonicalStructField& field : fields) {
    AppendPod(&table.bytes, field);
  }
  CHECK_EQ(table.header.offheap_table_offset_ + table.header.offheap_table_size_,
           table.bytes.size());
  return table;
}

dfx::OffheapTableResolver BuildResolver(
    const SyntheticOffheapTable& table,
    dfx::TranslateDiagnostics* diagnostics = nullptr) {
  dfx::BinaryReaderTest reader(const_cast<uint8_t*>(table.bytes.data()),
                               static_cast<uint32_t>(table.bytes.size()));
  return dfx::OffheapTableResolver(&reader, table.header, diagnostics);
}

struct TranslateResult {
  bool ok;
  std::unique_ptr<SnapshotGenerator> generator;
  dfx::TranslateDiagnostics diagnostics;
};

TranslateResult TranslateHeapBytesWithDiagnostics(
    const std::vector<uint8_t>& bytes) {
  BinaryReaderTest reader(const_cast<uint8_t*>(bytes.data()),
                          static_cast<uint32_t>(bytes.size()));
  auto generator = std::make_unique<SnapshotGenerator>();
  HeapTranslator translator(&reader, generator.get());
  bool ok = translator.TranslateHeap();
  return TranslateResult{ok, std::move(generator), translator.diagnostics()};
}

Tagged_t EncodeHeapObjectForRawHeapFieldForWasmTest(
    const std::vector<uint8_t>& bytes, Address address) {
#ifdef V8_COMPRESS_POINTERS
  RawHeapHeader header = ReadRawHeapHeader(bytes);
  const Address tagged_address = address + kHeapObjectTag;
  CHECK_GE(tagged_address, header.cage_base_);
  const uint64_t compressed =
      static_cast<uint64_t>(tagged_address - header.cage_base_);
  CHECK_LE(compressed,
           static_cast<uint64_t>(std::numeric_limits<Tagged_t>::max()));
  return static_cast<Tagged_t>(compressed);
#else
  USE(bytes);
  return static_cast<Tagged_t>(address + kHeapObjectTag);
#endif
}

template <typename T>
void WritePodAtForWasmTest(std::vector<uint8_t>* bytes, uint32_t offset,
                           const T& value) {
  CHECK_LE(static_cast<size_t>(offset) + sizeof(value), bytes->size());
  memcpy(bytes->data() + offset, &value, sizeof(value));
}

void RemoveRawObjectRecordForWasmTest(std::vector<uint8_t>* bytes,
                                      const RawObjectRecord& record) {
  const uint32_t record_offset = record.payload_offset -
                                 static_cast<uint32_t>(sizeof(Address)) -
                                 static_cast<uint32_t>(sizeof(uint32_t));
  const uint32_t record_size = static_cast<uint32_t>(sizeof(Address)) +
                               static_cast<uint32_t>(sizeof(uint32_t)) +
                               record.dump_size;
  const size_t erase_end = record_offset + record_size;
  CHECK_LE(erase_end, bytes->size());
  bytes->erase(bytes->begin() + record_offset, bytes->begin() + erase_end);

  RawHeapHeader header = ReadRawHeapHeader(*bytes);
  CHECK_GT(header.object_count_, 0u);
  CHECK_GE(header.object_table_size_, record_size);
  header.object_count_--;
  header.object_table_size_ -= record_size;
  if (header.offheap_table_offset_ != 0) {
    CHECK_GE(header.offheap_table_offset_, erase_end);
    header.offheap_table_offset_ -= record_size;
  }
  if (header.metadata_offset_ != 0) {
    CHECK_GE(header.metadata_offset_, erase_end);
    header.metadata_offset_ -= record_size;
  }
  WritePodAtForWasmTest(bytes, 0, header);
}

struct WasmStructDumpFixture {
  std::vector<uint8_t> bytes;
  Address struct_address = 0;
  Address target_address = 0;
  Address map_address = 0;
  uint32_t type_index = 0;
};

struct WasmStructDescriptorDumpFixture {
  std::vector<uint8_t> bytes;
  Address descriptor_address = 0;
  Address described_rtt_address = 0;
  Address map_address = 0;
  uint32_t type_index = 0;
};

struct WasmStringViewIterDumpFixture {
  std::vector<uint8_t> bytes;
  Address iterator_address = 0;
  Address iterator_map_address = 0;
  Address holder_address = 0;
  Address string_address = 0;
};

struct WasmArrayDumpFixture {
  std::vector<uint8_t> bytes;
  Address array_address = 0;
  Address target_address = 0;
  Address map_address = 0;
  Address type_info_address = 0;
  Address type_info_map_address = 0;
};

struct WasmTrustedInstanceDataDumpFixture {
  std::vector<uint8_t> bytes;
  Address trusted_data_address = 0;
  Address trusted_data_map_address = 0;
  Address shared_part_address = 0;
  Address memory_bases_and_sizes_address = 0;
  Address dispatch_table0_address = 0;
  Address dispatch_table0_map_address = 0;
  Address dispatch_tables_address = 0;
  Address dispatch_table_for_imports_address = 0;
  Address dispatch_table_for_imports_map_address = 0;
};

struct WasmMemoryMapDescriptorDumpFixture {
  std::vector<uint8_t> bytes;
  Address descriptor_address = 0;
  Address descriptor_map_address = 0;
};

struct WasmFunctionImportDataDumpFixture {
  std::vector<uint8_t> bytes;
  Address exported_function_data_address = 0;
  Address exported_function_data_map_address = 0;
  Address exported_function_data_func_ref_address = 0;
  Address exported_internal_function_address = 0;
  Address exported_internal_function_map_address = 0;
  Address wasm_js_function_data_address = 0;
  Address wasm_js_function_data_map_address = 0;
  Address wasm_js_function_data_func_ref_address = 0;
  Address wasm_js_function_data_internal_address = 0;
  Address wasm_import_data_address = 0;
  Address wasm_import_data_map_address = 0;
};

struct WasmCapiSuspenderContinuationDumpFixture {
  std::vector<uint8_t> bytes;
  Address wasm_capi_function_data_address = 0;
  Address wasm_capi_function_data_map_address = 0;
  Address wasm_capi_function_data_func_ref_address = 0;
  Address wasm_capi_function_data_internal_address = 0;
  Address suspender_address = 0;
  Address suspender_map_address = 0;
  Address continuation_address = 0;
  Address continuation_map_address = 0;
  Address resume_data_address = 0;
  Address resume_data_map_address = 0;
};

struct WasmJsApiObjectDumpFixture {
  std::vector<uint8_t> bytes;
  Address global_address = 0;
  Address global_prototype_address = 0;
  Address global_map_address = 0;
  Address instance_address = 0;
  Address instance_prototype_address = 0;
  Address instance_map_address = 0;
  Address instance_exports_address = 0;
  Address memory_address = 0;
  Address memory_prototype_address = 0;
  Address memory_map_address = 0;
  Address module_address = 0;
  Address module_prototype_address = 0;
  Address module_map_address = 0;
  Address module_managed_native_module_address = 0;
  Address module_script_address = 0;
  Address script_wasm_breakpoint_infos_address = 0;
  Address script_wasm_managed_native_module_address = 0;
  Address script_wasm_weak_instance_list_address = 0;
  Address table_address = 0;
  Address table_prototype_address = 0;
  Address table_map_address = 0;
};

struct WasmAssignedApiObjectSnapshot {
  Address object_address = 0;
  Address prototype_address = 0;
  Address map_address = 0;
};

struct WasmExceptionTagValueDumpFixture {
  std::vector<uint8_t> bytes;
  WasmAssignedApiObjectSnapshot exception_package;
  WasmAssignedApiObjectSnapshot suspending;
  WasmAssignedApiObjectSnapshot tag;
  WasmAssignedApiObjectSnapshot value;
};

struct WasmHardInternalTypesDumpFixture {
  std::vector<uint8_t> bytes;
  Address asm_wasm_data_address = 0;
  Address asm_wasm_data_map_address = 0;
  Address exception_tag_address = 0;
  Address exception_tag_map_address = 0;
  Address fast_api_call_data_address = 0;
  Address fast_api_call_data_map_address = 0;
  Address func_ref_address = 0;
  Address func_ref_map_address = 0;
  Address wasm_null_address = 0;
  Address wasm_null_map_address = 0;
};

struct WasmTranslateResult {
  std::unique_ptr<SnapshotGenerator> generator;
  TranslateDiagnostics diagnostics;
  bool ok = false;
};

struct WasmCanonicalTableInfo {
  RawHeapHeader header{};
  uint32_t directory_entry_offset = 0;
  dfx::RawHeapOffheapTableHeader table{};
  uint32_t payload_offset = 0;
};

struct WasmStructRecordInfo {
  dfx::RawHeapWasmCanonicalStructRecord record{};
  std::vector<dfx::RawHeapWasmCanonicalStructField> fields;
};

class WasmArrayTester {
 public:
  WasmArrayTester()
      : zone_(&allocator_, ZONE_NAME),
        builder_(&zone_),
        isolate_(CcTest::i_isolate()),
        thrower_(isolate_, "HeapDump wasm visitor coverage") {}

  DirectHandle<WasmArray> CreateArray() {
    const ModuleTypeIndex array_type_index =
        builder_.AddArrayType(zone_.New<ArrayType>(kWasmI32, true), true,
                              kNoSuperType);
    const HeapType array_heap_type =
        HeapType::Index(array_type_index, kNotShared, RefTypeKind::kArray);
    const ValueType array_value_type = ValueType::Ref(array_heap_type);
    FunctionSig sig(1, 0, &array_value_type);
    WasmFunctionBuilder* fun = builder_.AddFunction(&sig);
    fun->EmitCode(
        {WASM_ARRAY_NEW(array_type_index, WASM_I32V(4), WASM_I32V(33)),
         kExprEnd});
    builder_.AddExport(base::CStrVector("make_array"), fun);

    ZoneBuffer buffer(&zone_);
    builder_.WriteTo(&buffer);
    DirectHandle<WasmInstanceObject> instance =
        testing::CompileAndInstantiateForTesting(isolate_, &thrower_,
                                                 base::VectorOf(buffer))
            .ToHandleChecked();
    if (thrower_.error()) FATAL("%s", thrower_.error_msg());

    DirectHandle<WasmExportedFunction> exported =
        testing::GetExportedFunction(isolate_, instance, "make_array")
            .ToHandleChecked();
    MaybeDirectHandle<Object> maybe_result = Execution::Call(
        isolate_, exported, isolate_->factory()->undefined_value(), {});
    CHECK(!maybe_result.is_null());
    DirectHandle<Object> result = maybe_result.ToHandleChecked();
    CHECK(IsWasmArray(*result));

    keep_alive_.push_back(instance);
    keep_alive_.push_back(exported);
    keep_alive_.push_back(result);
    return Cast<WasmArray>(result);
  }

 private:
  AccountingAllocator allocator_;
  Zone zone_;
  WasmModuleBuilder builder_;
  Isolate* isolate_;
  wasm::ErrorThrower thrower_;
  std::vector<DirectHandle<Object>> keep_alive_;
};

class WasmStructRefFieldTester {
 public:
  WasmStructRefFieldTester()
      : zone_(&allocator_, ZONE_NAME),
        builder_(&zone_),
        isolate_(CcTest::i_isolate()),
        thrower_(isolate_, "HeapDump wasm struct ref field") {}

  DirectHandle<WasmStruct> CreateStructWithExternRefField(
      DirectHandle<JSObject>* target_out) {
    StructType::Builder type_builder(&zone_, 2, false, false);
    type_builder.AddField(kWasmExternRef, true);
    type_builder.AddField(kWasmI32, true);
    const ModuleTypeIndex struct_type_index =
        builder_.AddStructType(type_builder.Build(), true, kNoSuperType);
    const HeapType struct_heap_type =
        HeapType::Index(struct_type_index, kNotShared, RefTypeKind::kStruct);
    const ValueType struct_value_type = ValueType::Ref(struct_heap_type);
    ValueType sig_types[] = {struct_value_type, kWasmExternRef};
    FunctionSig sig(1, 1, sig_types);
    WasmFunctionBuilder* fun = builder_.AddFunction(&sig);
    fun->EmitCode({WASM_STRUCT_NEW(struct_type_index, WASM_LOCAL_GET(0),
                                   WASM_I32V(77)),
                   kExprEnd});
    builder_.AddExport(base::CStrVector("make_struct"), fun);

    ZoneBuffer buffer(&zone_);
    builder_.WriteTo(&buffer);
    DirectHandle<WasmInstanceObject> instance =
        testing::CompileAndInstantiateForTesting(isolate_, &thrower_,
                                                 base::VectorOf(buffer))
            .ToHandleChecked();
    if (thrower_.error()) FATAL("%s", thrower_.error_msg());

    DirectHandle<WasmExportedFunction> exported =
        testing::GetExportedFunction(isolate_, instance, "make_struct")
            .ToHandleChecked();
    DirectHandle<JSObject> target =
        isolate_->factory()->NewJSObject(isolate_->object_function());
    DirectHandle<Object> args[] = {Cast<Object>(target)};
    MaybeDirectHandle<Object> maybe_result = Execution::Call(
        isolate_, exported, isolate_->factory()->undefined_value(),
        base::VectorOf(args));
    CHECK(!maybe_result.is_null());
    DirectHandle<Object> result = maybe_result.ToHandleChecked();
    CHECK(IsWasmStruct(*result));
    CHECK_EQ(WASM_STRUCT_TYPE,
             Cast<WasmStruct>(result)->map()->instance_type());

    keep_alive_.push_back(Cast<Object>(instance));
    keep_alive_.push_back(Cast<Object>(exported));
    keep_alive_.push_back(Cast<Object>(target));
    keep_alive_.push_back(result);
    *target_out = target;
    return Cast<WasmStruct>(result);
  }

 private:
  AccountingAllocator allocator_;
  Zone zone_;
  WasmModuleBuilder builder_;
  Isolate* isolate_;
  wasm::ErrorThrower thrower_;
  std::vector<DirectHandle<Object>> keep_alive_;
};

template <typename T>
void KeepAlive(std::vector<DirectHandle<Object>>* keep_alive,
               DirectHandle<T> object) {
  keep_alive->push_back(Cast<Object>(object));
}

DirectHandle<WasmStruct> CreateWasmStructWithScalarAndRefField(
    Isolate* isolate, DirectHandle<Object> ref_target,
    DumpArtifacts* artifacts) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  WasmModuleBuilder builder(&zone);
  wasm::ErrorThrower thrower(isolate, "HeapDump wasm struct exact edge");

  StructType::Builder type_builder(&zone, 2, false, false);
  type_builder.AddField(kWasmI32, true);
  type_builder.AddField(kWasmExternRef, true);
  const ModuleTypeIndex struct_type_index =
      builder.AddStructType(type_builder.Build(), true, kNoSuperType);
  const HeapType struct_heap_type =
      HeapType::Index(struct_type_index, kNotShared, RefTypeKind::kStruct);
  const ValueType struct_value_type = ValueType::Ref(struct_heap_type);
  ValueType sig_types[] = {struct_value_type, kWasmExternRef};
  FunctionSig sig(1, 1, sig_types);

  WasmFunctionBuilder* fun = builder.AddFunction(&sig);
  fun->EmitCode({WASM_STRUCT_NEW(struct_type_index, WASM_I32V(77),
                                 WASM_LOCAL_GET(0)),
                 kExprEnd});
  builder.AddExport(base::CStrVector("make_struct"), fun);

  ZoneBuffer buffer(&zone);
  builder.WriteTo(&buffer);
  DirectHandle<WasmInstanceObject> instance =
      testing::CompileAndInstantiateForTesting(isolate, &thrower,
                                               base::VectorOf(buffer))
          .ToHandleChecked();
  if (thrower.error()) FATAL("%s", thrower.error_msg());

  DirectHandle<WasmExportedFunction> exported =
      testing::GetExportedFunction(isolate, instance, "make_struct")
          .ToHandleChecked();
  DirectHandle<Object> args[] = {ref_target};
  MaybeDirectHandle<Object> maybe_result =
      Execution::Call(isolate, exported, isolate->factory()->undefined_value(),
                      base::VectorOf(args));
  CHECK(!maybe_result.is_null());
  DirectHandle<Object> result = maybe_result.ToHandleChecked();
  CHECK(IsWasmStruct(*result));
  CHECK_EQ(WASM_STRUCT_TYPE, Cast<WasmStruct>(result)->map()->instance_type());

  KeepAlive(&artifacts->keep_alive, ref_target);
  KeepAlive(&artifacts->keep_alive, instance);
  KeepAlive(&artifacts->keep_alive, exported);
  KeepAlive(&artifacts->keep_alive, result);
  return Cast<WasmStruct>(result);
}

DirectHandle<WasmStruct> CreateWasmDescriptorStruct(Isolate* isolate,
                                                    DumpArtifacts* artifacts) {
  FlagScope<bool> custom_descriptors(
      &v8_flags.experimental_wasm_custom_descriptors, true);
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  WasmModuleBuilder builder(&zone);
  wasm::ErrorThrower thrower(isolate, "HeapDump wasm descriptor struct");

  builder.StartRecursiveTypeGroup();
  StructType::Builder described_type_builder(&zone, 0, false, false);
  const ModuleTypeIndex described_type_index =
      builder.AddStructType(described_type_builder.Build(), true,
                            kNoSuperType);
  StructType::Builder descriptor_type_builder(&zone, 0, true, false);
  const ModuleTypeIndex descriptor_type_index =
      builder.AddStructType(descriptor_type_builder.Build(), true,
                            kNoSuperType);
  builder.GetType_Unsafe(described_type_index).descriptor =
      descriptor_type_index;
  builder.GetType_Unsafe(descriptor_type_index).describes =
      described_type_index;
  builder.EndRecursiveTypeGroup();

  const HeapType descriptor_heap_type =
      HeapType::Index(descriptor_type_index, kNotShared, RefTypeKind::kStruct);
  const ValueType descriptor_value_type = ValueType::Ref(descriptor_heap_type);
  FunctionSig sig(1, 0, &descriptor_value_type);

  WasmFunctionBuilder* fun = builder.AddFunction(&sig);
  fun->EmitCode({WASM_STRUCT_NEW_DEFAULT(descriptor_type_index), kExprEnd});
  builder.AddExport(base::CStrVector("make_descriptor"), fun);

  ZoneBuffer buffer(&zone);
  builder.WriteTo(&buffer);
  DirectHandle<WasmInstanceObject> instance =
      testing::CompileAndInstantiateForTesting(isolate, &thrower,
                                               base::VectorOf(buffer))
          .ToHandleChecked();
  if (thrower.error()) FATAL("%s", thrower.error_msg());

  DirectHandle<WasmExportedFunction> exported =
      testing::GetExportedFunction(isolate, instance, "make_descriptor")
          .ToHandleChecked();
  MaybeDirectHandle<Object> maybe_result =
      Execution::Call(isolate, exported, isolate->factory()->undefined_value(),
                      {});
  CHECK(!maybe_result.is_null());
  DirectHandle<Object> result = maybe_result.ToHandleChecked();
  CHECK(IsWasmStruct(*result));
  CHECK_EQ(WASM_STRUCT_TYPE, Cast<WasmStruct>(result)->map()->instance_type());

  KeepAlive(&artifacts->keep_alive, instance);
  KeepAlive(&artifacts->keep_alive, exported);
  KeepAlive(&artifacts->keep_alive, result);
  return Cast<WasmStruct>(result);
}

DirectHandle<WasmStruct> CreateWasmStructHoldingStringViewIter(
    Isolate* isolate, DirectHandle<String> string, DumpArtifacts* artifacts) {
  FlagScope<bool> stringref(&v8_flags.experimental_wasm_stringref, true);
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  WasmModuleBuilder builder(&zone);
  wasm::ErrorThrower thrower(isolate, "HeapDump wasm string view iter");

  StructType::Builder type_builder(&zone, 1, false, false);
  type_builder.AddField(kWasmStringViewIter, true);
  const ModuleTypeIndex struct_type_index =
      builder.AddStructType(type_builder.Build(), true, kNoSuperType);
  const HeapType struct_heap_type =
      HeapType::Index(struct_type_index, kNotShared, RefTypeKind::kStruct);
  const uint32_t holder_global =
      builder.AddGlobal(ValueType::RefNull(struct_heap_type), true,
                        WasmInitExpr::RefNullConst(struct_heap_type));
  ValueType sig_types[] = {kWasmStringRef};
  FunctionSig sig(0, 1, sig_types);

  WasmFunctionBuilder* fun = builder.AddFunction(&sig);
  fun->EmitCode({WASM_GLOBAL_SET(holder_global,
                                 WASM_STRUCT_NEW(struct_type_index,
                                                 WASM_LOCAL_GET(0),
                                                 kGCPrefix,
                                                 U32V_2(ExtractPrefixedOpcodeBytes(
                                                     kExprStringAsIter)))),
                 kExprEnd});
  builder.AddExport(base::CStrVector("make_iter_holder"), fun);

  ZoneBuffer buffer(&zone);
  builder.WriteTo(&buffer);
  MaybeDirectHandle<WasmInstanceObject> maybe_instance =
      testing::CompileAndInstantiateForTesting(isolate, &thrower,
                                               base::VectorOf(buffer));
  if (maybe_instance.is_null()) {
    FATAL("failed to instantiate string view iter module: %s",
          thrower.error() ? thrower.error_msg() : "<no thrower message>");
  }
  DirectHandle<WasmInstanceObject> instance =
      maybe_instance.ToHandleChecked();
  if (thrower.error()) FATAL("%s", thrower.error_msg());

  MaybeDirectHandle<WasmExportedFunction> maybe_exported =
      testing::GetExportedFunction(isolate, instance, "make_iter_holder");
  if (maybe_exported.is_null()) {
    FATAL("failed to find string view iter wasm export");
  }
  DirectHandle<WasmExportedFunction> exported =
      maybe_exported.ToHandleChecked();
  DirectHandle<Object> args[] = {Cast<Object>(string)};
  MaybeDirectHandle<Object> maybe_result =
      Execution::Call(isolate, exported, isolate->factory()->undefined_value(),
                      base::VectorOf(args));
  if (maybe_result.is_null()) {
    FATAL("failed to call string view iter wasm export");
  }
  DirectHandle<WasmTrustedInstanceData> trusted_data(
      instance->trusted_data(isolate), isolate);
  const WasmGlobal& global = instance->module()->globals[holder_global];
  DirectHandle<FixedArray> tagged_globals(
      trusted_data->tagged_globals_buffer(), isolate);
  Tagged<Object> global_value = tagged_globals->get(global.offset);
  CHECK_NE(static_cast<Address>(0), global_value.ptr());
  DirectHandle<Object> result(global_value, isolate);
  CHECK(IsWasmStruct(*result));
  CHECK_EQ(WASM_STRUCT_TYPE, Cast<WasmStruct>(result)->map()->instance_type());

  KeepAlive(&artifacts->keep_alive, string);
  KeepAlive(&artifacts->keep_alive, instance);
  KeepAlive(&artifacts->keep_alive, exported);
  KeepAlive(&artifacts->keep_alive, result);
  return Cast<WasmStruct>(result);
}

DirectHandle<WasmArray> CreateWasmExternRefArrayWithTarget(
    Isolate* isolate, DirectHandle<Object> ref_target,
    DumpArtifacts* artifacts) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  WasmModuleBuilder builder(&zone);
  wasm::ErrorThrower thrower(isolate, "HeapDump wasm array exact edge");

  const ModuleTypeIndex array_type_index =
      builder.AddArrayType(zone.New<ArrayType>(kWasmExternRef, true), true,
                           kNoSuperType);
  const HeapType array_heap_type =
      HeapType::Index(array_type_index, kNotShared, RefTypeKind::kArray);
  const ValueType array_value_type = ValueType::Ref(array_heap_type);
  ValueType sig_types[] = {array_value_type, kWasmExternRef};
  FunctionSig sig(1, 1, sig_types);

  WasmFunctionBuilder* fun = builder.AddFunction(&sig);
  fun->EmitCode({WASM_ARRAY_NEW(array_type_index, WASM_LOCAL_GET(0),
                                WASM_I32V(3)),
                 kExprEnd});
  builder.AddExport(base::CStrVector("make_ref_array"), fun);

  ZoneBuffer buffer(&zone);
  builder.WriteTo(&buffer);
  DirectHandle<WasmInstanceObject> instance =
      testing::CompileAndInstantiateForTesting(isolate, &thrower,
                                               base::VectorOf(buffer))
          .ToHandleChecked();
  if (thrower.error()) FATAL("%s", thrower.error_msg());

  DirectHandle<WasmExportedFunction> exported =
      testing::GetExportedFunction(isolate, instance, "make_ref_array")
          .ToHandleChecked();
  DirectHandle<Object> args[] = {ref_target};
  MaybeDirectHandle<Object> maybe_result =
      Execution::Call(isolate, exported, isolate->factory()->undefined_value(),
                      base::VectorOf(args));
  CHECK(!maybe_result.is_null());
  DirectHandle<Object> result = maybe_result.ToHandleChecked();
  CHECK(IsWasmArray(*result));
  CHECK_EQ(WASM_ARRAY_TYPE, Cast<WasmArray>(result)->map()->instance_type());

  KeepAlive(&artifacts->keep_alive, ref_target);
  KeepAlive(&artifacts->keep_alive, instance);
  KeepAlive(&artifacts->keep_alive, exported);
  KeepAlive(&artifacts->keep_alive, result);
  return Cast<WasmArray>(result);
}

DirectHandle<WasmArray> CreateWasmScalarArray(Isolate* isolate,
                                              DumpArtifacts* artifacts) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  WasmModuleBuilder builder(&zone);
  wasm::ErrorThrower thrower(isolate, "HeapDump wasm scalar array");

  const ModuleTypeIndex array_type_index =
      builder.AddArrayType(zone.New<ArrayType>(kWasmI32, true), true,
                           kNoSuperType);
  const HeapType array_heap_type =
      HeapType::Index(array_type_index, kNotShared, RefTypeKind::kArray);
  const ValueType array_value_type = ValueType::Ref(array_heap_type);
  FunctionSig sig(1, 0, &array_value_type);

  WasmFunctionBuilder* fun = builder.AddFunction(&sig);
  fun->EmitCode(
      {WASM_ARRAY_NEW(array_type_index, WASM_I32V(0x1234567), WASM_I32V(4)),
       kExprEnd});
  builder.AddExport(base::CStrVector("make_scalar_array"), fun);

  ZoneBuffer buffer(&zone);
  builder.WriteTo(&buffer);
  DirectHandle<WasmInstanceObject> instance =
      testing::CompileAndInstantiateForTesting(isolate, &thrower,
                                               base::VectorOf(buffer))
          .ToHandleChecked();
  if (thrower.error()) FATAL("%s", thrower.error_msg());

  DirectHandle<WasmExportedFunction> exported =
      testing::GetExportedFunction(isolate, instance, "make_scalar_array")
          .ToHandleChecked();
  MaybeDirectHandle<Object> maybe_result =
      Execution::Call(isolate, exported, isolate->factory()->undefined_value(),
                      {});
  CHECK(!maybe_result.is_null());
  DirectHandle<Object> result = maybe_result.ToHandleChecked();
  CHECK(IsWasmArray(*result));
  CHECK_EQ(WASM_ARRAY_TYPE, Cast<WasmArray>(result)->map()->instance_type());

  KeepAlive(&artifacts->keep_alive, instance);
  KeepAlive(&artifacts->keep_alive, exported);
  KeepAlive(&artifacts->keep_alive, result);
  return Cast<WasmArray>(result);
}

DirectHandle<WasmTrustedInstanceData> CreateWasmTrustedInstanceDataForTest(
    Isolate* isolate, DumpArtifacts* artifacts) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  WasmModuleBuilder builder(&zone);
  wasm::ErrorThrower thrower(isolate,
                             "HeapDump wasm trusted protected fields");

  TestSignatures sigs;
  WasmFunctionBuilder* fun = builder.AddFunction(sigs.v_v());
  fun->EmitCode({kExprEnd});
  builder.AddExport(base::CStrVector("run"), fun);

  ZoneBuffer buffer(&zone);
  builder.WriteTo(&buffer);
  DirectHandle<WasmInstanceObject> instance =
      testing::CompileAndInstantiateForTesting(isolate, &thrower,
                                               base::VectorOf(buffer))
          .ToHandleChecked();
  if (thrower.error()) FATAL("%s", thrower.error_msg());

  DirectHandle<WasmTrustedInstanceData> trusted_data(
      instance->trusted_data(isolate), isolate);
  CHECK_EQ(WASM_TRUSTED_INSTANCE_DATA_TYPE,
           trusted_data->map()->instance_type());
  KeepAlive(&artifacts->keep_alive, instance);
  KeepAlive(&artifacts->keep_alive, trusted_data);
  return trusted_data;
}

DirectHandle<JSFunction> GetCallableForWasmTests(Isolate* isolate,
                                                 LocalContext* env) {
  RunScript(env->local(), R"(
    globalThis.__heap_dump_wasm_callable = function HeapDumpWasmCallable(x) {
      return x;
    };
  )");
  v8::Local<v8::Value> value =
      env->local()
          ->Global()
          ->Get(env->local(),
                NewApiString(CcTest::isolate(), "__heap_dump_wasm_callable"))
          .ToLocalChecked();
  return Cast<JSFunction>(v8::Utils::OpenDirectHandle(*value));
}

const wasm::CanonicalSig* CanonicalizeSig(const wasm::FunctionSig* sig) {
  wasm::CanonicalTypeIndex sig_id = GetTypeCanonicalizer()->AddRecursiveGroup(sig);
  return GetTypeCanonicalizer()->LookupFunctionSignature(sig_id);
}

int32_t DummyCapiTarget(int32_t value) { return value + 1; }

std::string MissingVisitorIdsMessage(const VisitorIdCounts& counts) {
  std::ostringstream out;
  bool first = true;
  for (const VisitorIdExpectation& item : kWasmStableVisitorExpectations) {
    if (counts.find(item.id) != counts.end()) continue;
    if (!first) out << ", ";
    out << item.name;
    first = false;
  }
  return out.str();
}

WasmStructRefFieldDump DumpWasmStructWithExternRefField() {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  v8::HandleScope scope(CcTest::isolate());
  LocalContext env(CcTest::isolate());
  USE(env);

  DirectHandle<JSObject> target;
  WasmStructRefFieldTester tester;
  DirectHandle<WasmStruct> wasm_struct =
      tester.CreateStructWithExternRefField(&target);

  InvokeAtomicMajorGC(isolate->heap());

  Tagged<WasmTypeInfo> type_info = wasm_struct->map()->wasm_type_info();
  wasm::CanonicalTypeIndex type_index = type_info->type_index();
  CHECK(type_index.valid());

  WasmStructRefFieldDump dump;
  dump.type_index = type_index.index;
  dump.struct_address = wasm_struct->address();
  dump.target_address = target->address();
  dump.bytes = DumpHeapBytes(isolate);
  return dump;
}

std::unique_ptr<SnapshotGenerator> CreateWasmCoverageSnapshot(
    VisitorIdCounts* counts_out) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  FlagScope<bool> memory_control(&v8_flags.experimental_wasm_memory_control,
                                 true);

  v8::HandleScope scope(CcTest::isolate());
  LocalContext env(CcTest::isolate());

  std::vector<DirectHandle<Object>> keep_alive;
  DirectHandle<JSFunction> callable = GetCallableForWasmTests(isolate, &env);
  KeepAlive(&keep_alive, callable);

  TestSignatures sigs;
  const wasm::CanonicalSig* canonical_sig = CanonicalizeSig(sigs.i_i());

  DirectHandle<WasmJSFunction> wasm_js_function =
      WasmJSFunction::New(isolate, sigs.i_i(), callable, wasm::kNoSuspend);
  KeepAlive(&keep_alive, wasm_js_function);

  DirectHandle<Foreign> embedder_data = CreateForeignForTest(isolate);
  KeepAlive(&keep_alive, embedder_data);
  DirectHandle<WasmCapiFunction> wasm_capi_function = WasmCapiFunction::New(
      isolate, reinterpret_cast<Address>(&DummyCapiTarget), embedder_data,
      canonical_sig);
  KeepAlive(&keep_alive, wasm_capi_function);

  DirectHandle<WasmImportData> wasm_import_data = isolate->factory()->NewWasmImportData(
      callable, wasm::kNoSuspend, MaybeDirectHandle<WasmTrustedInstanceData>(),
      canonical_sig, false);
  KeepAlive(&keep_alive, wasm_import_data);

  DirectHandle<WasmSuspenderObject> suspender =
      isolate->factory()->NewWasmSuspenderObjectInitialized();
  KeepAlive(&keep_alive, suspender);
  DirectHandle<WasmResumeData> wasm_resume_data =
      isolate->factory()->NewWasmResumeData(suspender, wasm::OnResume::kContinue);
  KeepAlive(&keep_alive, wasm_resume_data);

  DirectHandle<WasmContinuationObject> wasm_continuation =
      isolate->factory()->NewWasmContinuationObject(nullptr);
  KeepAlive(&keep_alive, wasm_continuation);

  DirectHandle<WasmSuspendingObject> wasm_suspending =
      WasmSuspendingObject::New(isolate, callable);
  KeepAlive(&keep_alive, wasm_suspending);

  RunScript(env.local(), R"(
    globalThis.__heap_dump_wasm_global =
        new WebAssembly.Global({value: 'i32', mutable: true}, 7);
  )");
  v8::Local<v8::Value> wasm_global_value =
      env.local()
          ->Global()
          ->Get(env.local(),
                NewApiString(CcTest::isolate(), "__heap_dump_wasm_global"))
          .ToLocalChecked();
  DirectHandle<WasmGlobalObject> wasm_global =
      Cast<WasmGlobalObject>(v8::Utils::OpenDirectHandle(*wasm_global_value));
  KeepAlive(&keep_alive, wasm_global);

#if V8_TARGET_OS_LINUX
  MaybeDirectHandle<WasmMemoryMapDescriptor> maybe_descriptor =
      WasmMemoryMapDescriptor::NewFromAnonymous(isolate, 4096);
  CHECK(!maybe_descriptor.is_null());
  DirectHandle<WasmMemoryMapDescriptor> wasm_memory_map_descriptor =
      maybe_descriptor.ToHandleChecked();
  KeepAlive(&keep_alive, wasm_memory_map_descriptor);
#endif

  WasmArrayTester array_tester;
  DirectHandle<WasmArray> wasm_array = array_tester.CreateArray();
  KeepAlive(&keep_alive, wasm_array);

  USE(keep_alive);
  InvokeAtomicMajorGC(isolate->heap());
  *counts_out = CollectVisitorIdCounts(isolate);
  return TranslateHeapBytes(DumpHeapBytes(isolate));
}

WasmTranslateResult TranslateHeapBytesWithDiagnosticsForWasmTest(
    const std::vector<uint8_t>& bytes) {
  BinaryReaderTest reader(const_cast<uint8_t*>(bytes.data()),
                          static_cast<uint32_t>(bytes.size()));
  WasmTranslateResult result;
  result.generator = std::make_unique<SnapshotGenerator>();
  HeapTranslator translator(&reader, result.generator.get());
  result.ok = translator.TranslateHeap();
  result.diagnostics = translator.diagnostics();
  return result;
}

void CheckWasmJsApiObjectSnapshot(SnapshotGenerator* generator,
                                  Address object_address,
                                  Address prototype_address,
                                  Address map_address) {
  Node* object = GetSnapshotNode(generator, object_address);
  Node* prototype = GetSnapshotNode(generator, prototype_address);
  Node* map = GetSnapshotNode(generator, map_address);
  CHECK_NOT_NULL(object);
  CHECK_NOT_NULL(prototype);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(object, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(generator, object,
                                                    "__proto__", prototype);
  CHECK(HasExactNamedEdge(generator, object, Edge::Type::kInternal, "map",
                          map));
  CHECK_EQ(1, CountExactNamedEdges(generator, object, Edge::Type::kInternal,
                                   "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(object, Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kProperty, map));
}

WasmJsApiObjectDumpFixture DumpWasmJsApiObjectFixture() {
  WasmJsApiObjectDumpFixture fixture;
  fixture.bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        RunScript(context, R"(
          (function() {
            const bytes = new Uint8Array([
              0x00, 0x61, 0x73, 0x6d,
              0x01, 0x00, 0x00, 0x00
            ]);
            const global =
                new WebAssembly.Global({value: 'i32', mutable: true}, 7);
            const module = new WebAssembly.Module(bytes);
            const instance = new WebAssembly.Instance(module, {});
            const memory = new WebAssembly.Memory({initial: 1, maximum: 1});
            const table =
                new WebAssembly.Table({initial: 1, element: 'anyfunc'});
            if (!(global instanceof WebAssembly.Global) ||
                global.value !== 7 ||
                !(module instanceof WebAssembly.Module) ||
                !(instance instanceof WebAssembly.Instance) ||
                instance.exports === undefined ||
                !(memory instanceof WebAssembly.Memory) ||
                memory.buffer.byteLength !== 65536 ||
                !(table instanceof WebAssembly.Table) ||
                table.length !== 1) {
              throw new Error('unexpected WebAssembly JS API fixture');
            }
            globalThis.__heap_dump_wasm_js_api_objects = {
              global,
              globalPrototype: Object.getPrototypeOf(global),
              instance,
              instancePrototype: Object.getPrototypeOf(instance),
              memory,
              memoryPrototype: Object.getPrototypeOf(memory),
              module,
              modulePrototype: Object.getPrototypeOf(module),
              table,
              tablePrototype: Object.getPrototypeOf(table)
            };
          })();
        )");

        v8::Local<v8::Object> holder = v8::Local<v8::Object>::Cast(
            context->Global()
                ->Get(context,
                      NewApiString(v8_isolate,
                                   "__heap_dump_wasm_js_api_objects"))
                .ToLocalChecked());
        auto get_property = [&](const char* name) {
          return holder->Get(context, NewApiString(v8_isolate, name))
              .ToLocalChecked();
        };
        auto check_object = [&](v8::Local<v8::Value> value,
                                v8::Local<v8::Value> prototype,
                                InstanceType expected) {
          CHECK(value->IsObject());
          CHECK(prototype->IsObject());
          CHECK_EQ(expected, LocalObjectInstanceType(value));
          v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
          CHECK(object->GetPrototypeV2()->StrictEquals(prototype));
        };

        v8::Local<v8::Value> global_value = get_property("global");
        v8::Local<v8::Value> global_prototype =
            get_property("globalPrototype");
        check_object(global_value, global_prototype, WASM_GLOBAL_OBJECT_TYPE);
        DirectHandle<WasmGlobalObject> global = Cast<WasmGlobalObject>(
            v8::Utils::OpenDirectHandle(*global_value));
        CHECK_EQ(WASM_GLOBAL_OBJECT_TYPE, global->map()->instance_type());

        v8::Local<v8::Value> instance_value = get_property("instance");
        v8::Local<v8::Value> instance_prototype =
            get_property("instancePrototype");
        check_object(instance_value, instance_prototype,
                     WASM_INSTANCE_OBJECT_TYPE);
        DirectHandle<WasmInstanceObject> instance =
            Cast<WasmInstanceObject>(
                v8::Utils::OpenDirectHandle(*instance_value));
        CHECK_EQ(WASM_INSTANCE_OBJECT_TYPE, instance->map()->instance_type());

        v8::Local<v8::Value> memory_value = get_property("memory");
        v8::Local<v8::Value> memory_prototype =
            get_property("memoryPrototype");
        check_object(memory_value, memory_prototype, WASM_MEMORY_OBJECT_TYPE);
        DirectHandle<WasmMemoryObject> memory = Cast<WasmMemoryObject>(
            v8::Utils::OpenDirectHandle(*memory_value));
        CHECK_EQ(WASM_MEMORY_OBJECT_TYPE, memory->map()->instance_type());

        v8::Local<v8::Value> module_value = get_property("module");
        v8::Local<v8::Value> module_prototype =
            get_property("modulePrototype");
        check_object(module_value, module_prototype, WASM_MODULE_OBJECT_TYPE);
        DirectHandle<WasmModuleObject> module = Cast<WasmModuleObject>(
            v8::Utils::OpenDirectHandle(*module_value));
        CHECK_EQ(WASM_MODULE_OBJECT_TYPE, module->map()->instance_type());

        v8::Local<v8::Value> table_value = get_property("table");
        v8::Local<v8::Value> table_prototype = get_property("tablePrototype");
        check_object(table_value, table_prototype, WASM_TABLE_OBJECT_TYPE);
        DirectHandle<WasmTableObject> table = Cast<WasmTableObject>(
            v8::Utils::OpenDirectHandle(*table_value));
        CHECK_EQ(WASM_TABLE_OBJECT_TYPE, table->map()->instance_type());

        InvokeAtomicMajorGC(isolate->heap());
        fixture.global_address = LocalObjectAddress(global_value);
        fixture.global_prototype_address = LocalObjectAddress(global_prototype);
        fixture.global_map_address = global->map()->address();
        fixture.instance_address = LocalObjectAddress(instance_value);
        fixture.instance_prototype_address =
            LocalObjectAddress(instance_prototype);
        fixture.instance_map_address = instance->map()->address();
        CHECK_EQ(module->address(), instance->module_object()->address());
        fixture.instance_exports_address =
            instance->exports_object()->address();
        fixture.memory_address = LocalObjectAddress(memory_value);
        fixture.memory_prototype_address = LocalObjectAddress(memory_prototype);
        fixture.memory_map_address = memory->map()->address();
        fixture.module_address = LocalObjectAddress(module_value);
        fixture.module_prototype_address = LocalObjectAddress(module_prototype);
        fixture.module_map_address = module->map()->address();
        fixture.module_managed_native_module_address =
            module->managed_native_module()->address();
        DirectHandle<Script> script(module->script(), isolate);
        CHECK_EQ(Script::Type::kWasm, script->type());
        fixture.module_script_address = script->address();
        fixture.script_wasm_breakpoint_infos_address =
            script->wasm_breakpoint_infos()->address();
        CHECK(IsHeapObject(script->wasm_managed_native_module()));
        fixture.script_wasm_managed_native_module_address =
            Cast<HeapObject>(script->wasm_managed_native_module())->address();
        fixture.script_wasm_weak_instance_list_address =
            script->wasm_weak_instance_list()->address();
        fixture.table_address = LocalObjectAddress(table_value);
        fixture.table_prototype_address = LocalObjectAddress(table_prototype);
        fixture.table_map_address = table->map()->address();
        return DumpArtifacts{};
      });
  CHECK_NE(static_cast<Address>(0), fixture.global_address);
  CHECK_NE(static_cast<Address>(0), fixture.instance_address);
  CHECK_NE(static_cast<Address>(0), fixture.instance_exports_address);
  CHECK_NE(static_cast<Address>(0), fixture.memory_address);
  CHECK_NE(static_cast<Address>(0), fixture.module_address);
  CHECK_NE(static_cast<Address>(0),
           fixture.module_managed_native_module_address);
  CHECK_NE(static_cast<Address>(0), fixture.module_script_address);
  CHECK_NE(static_cast<Address>(0),
           fixture.script_wasm_breakpoint_infos_address);
  CHECK_NE(static_cast<Address>(0),
           fixture.script_wasm_managed_native_module_address);
  CHECK_NE(static_cast<Address>(0),
           fixture.script_wasm_weak_instance_list_address);
  CHECK_NE(static_cast<Address>(0), fixture.table_address);
  return fixture;
}

bool HasWasmAssignedApiObject(const WasmAssignedApiObjectSnapshot& snapshot) {
  return snapshot.object_address != 0 && snapshot.prototype_address != 0 &&
         snapshot.map_address != 0;
}

void CheckWasmObjectSnapshotMapEdge(SnapshotGenerator* generator,
                                    Address object_address,
                                    Address map_address) {
  Node* object = GetSnapshotNode(generator, object_address);
  Node* map = GetSnapshotNode(generator, map_address);
  CHECK_NOT_NULL(object);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(object, Node::Type::kObject));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CHECK(HasExactNamedEdge(generator, object, Edge::Type::kInternal, "map",
                          map));
  CHECK_EQ(1, CountExactNamedEdges(generator, object, Edge::Type::kInternal,
                                   "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(object, Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kProperty, map));
}

void CheckWasmHeapObjectSnapshotMapEdge(SnapshotGenerator* generator,
                                        Address object_address,
                                        Address map_address) {
  Node* object = GetSnapshotNode(generator, object_address);
  Node* map = GetSnapshotNode(generator, map_address);
  CHECK_NOT_NULL(object);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CHECK(NodeHasType(object, Node::Type::kObject) ||
        NodeHasType(object, Node::Type::kHidden));
  CHECK(HasExactNamedEdge(generator, object, Edge::Type::kInternal, "map",
                          map));
  CHECK_EQ(1, CountExactNamedEdges(generator, object, Edge::Type::kInternal,
                                   "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(object, Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kProperty, map));
}

void CheckWasmFunctionDataBaseHiddenEdges(SnapshotGenerator* generator,
                                          Address function_data_address,
                                          Address func_ref_address,
                                          Address internal_address) {
  Node* function_data = GetSnapshotNode(generator, function_data_address);
  Node* func_ref = GetSnapshotNode(generator, func_ref_address);
  Node* internal = GetSnapshotNode(generator, internal_address);
  CHECK_NOT_NULL(function_data);
  CHECK_NOT_NULL(func_ref);
  CHECK_NOT_NULL(internal);
  CHECK(NodeHasType(function_data, Node::Type::kObject) ||
        NodeHasType(function_data, Node::Type::kHidden));
  CHECK(NodeHasType(func_ref, Node::Type::kObject) ||
        NodeHasType(func_ref, Node::Type::kHidden));
  CHECK(NodeHasType(internal, Node::Type::kObject) ||
        NodeHasType(internal, Node::Type::kHidden));

  CHECK_EQ(1,
           CountEdgesFromNodeToTarget(function_data, Edge::Type::kHidden,
                                      func_ref));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(function_data, Edge::Type::kInternal,
                                      func_ref));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(function_data, Edge::Type::kWeak,
                                         func_ref));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(function_data, Edge::Type::kProperty,
                                         func_ref));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(function_data, Edge::Type::kShortcut,
                                         func_ref));

  CHECK_EQ(1,
           CountEdgesFromNodeToTarget(function_data, Edge::Type::kHidden,
                                      internal));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(function_data, Edge::Type::kInternal,
                                      internal));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(function_data, Edge::Type::kWeak,
                                         internal));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(function_data, Edge::Type::kProperty,
                                         internal));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(function_data, Edge::Type::kShortcut,
                                         internal));
}

WasmExceptionTagValueDumpFixture DumpWasmExceptionTagValueFixture() {
  WasmExceptionTagValueDumpFixture fixture;
  fixture.bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        RunScript(context, R"(
          (function() {
            const holder = {};
            if (typeof WebAssembly.Tag === 'function') {
              const tag = new WebAssembly.Tag({parameters: ['i32']});
              if (!(tag instanceof WebAssembly.Tag)) {
                throw new Error('unexpected WebAssembly.Tag fixture');
              }
              holder.tag = tag;
              holder.tagPrototype = Object.getPrototypeOf(tag);

              if (typeof WebAssembly.Exception === 'function') {
                const exception = new WebAssembly.Exception(tag, [42]);
                if (!(exception instanceof WebAssembly.Exception) ||
                    !exception.is(tag) ||
                    exception.getArg(tag, 0) !== 42) {
                  throw new Error('unexpected WebAssembly.Exception fixture');
                }
                holder.exceptionPackage = exception;
                holder.exceptionPackagePrototype =
                    Object.getPrototypeOf(exception);
              } else {
                holder.exceptionPackageTodo =
                    'WebAssembly.Exception constructor unavailable';
              }
            } else {
              holder.tagTodo = 'WebAssembly.Tag constructor unavailable';
              holder.exceptionPackageTodo =
                  'WebAssembly.Exception requires WebAssembly.Tag';
            }

            if (typeof WebAssembly.Suspending === 'function') {
              const suspending =
                  new WebAssembly.Suspending(() => Promise.resolve(13));
              if (!(suspending instanceof WebAssembly.Suspending)) {
                throw new Error('unexpected WebAssembly.Suspending fixture');
              }
              holder.suspending = suspending;
              holder.suspendingPrototype = Object.getPrototypeOf(suspending);
            } else {
              holder.suspendingTodo =
                  'WebAssembly.Suspending constructor unavailable';
            }
            globalThis.__heap_dump_wasm_exception_tag_value_objects = holder;
          })();
        )");

        DumpArtifacts artifacts;
        v8::Local<v8::Object> holder = v8::Local<v8::Object>::Cast(
            context->Global()
                ->Get(context,
                      NewApiString(
                          v8_isolate,
                          "__heap_dump_wasm_exception_tag_value_objects"))
                .ToLocalChecked());
        auto get_property = [&](const char* name) {
          return holder->Get(context, NewApiString(v8_isolate, name))
              .ToLocalChecked();
        };
        DirectHandle<String> value_type =
            isolate->factory()->InternalizeString(base::StaticCharVector("i32"));
        DirectHandle<Object> value_payload = isolate->factory()->NewNumber(42);
        DirectHandle<WasmValueObject> value_object =
            WasmValueObject::New(isolate, value_type, value_payload);
        CHECK_EQ(WASM_VALUE_OBJECT_TYPE, value_object->map()->instance_type());
        KeepAlive(&artifacts.keep_alive, value_type);
        KeepAlive(&artifacts.keep_alive, value_payload);
        KeepAlive(&artifacts.keep_alive, value_object);

        InvokeAtomicMajorGC(isolate->heap());

        auto capture_optional_object =
            [&](const char* name, const char* prototype_name,
                InstanceType expected,
                WasmAssignedApiObjectSnapshot* snapshot) -> bool {
          v8::Local<v8::Value> value = get_property(name);
          if (value->IsUndefined()) return false;
          v8::Local<v8::Value> prototype = get_property(prototype_name);
          CHECK(value->IsObject());
          CHECK(prototype->IsObject());
          CHECK_EQ(expected, LocalObjectInstanceType(value));
          v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
          CHECK(object->GetPrototypeV2()->StrictEquals(prototype));
          DirectHandle<JSReceiver> receiver = LocalJSReceiver(value);
          CHECK_EQ(expected, receiver->map()->instance_type());
          snapshot->object_address = receiver->address();
          snapshot->prototype_address = LocalObjectAddress(prototype);
          snapshot->map_address = receiver->map()->address();
          artifacts.global_keep_alive.emplace_back(v8_isolate, value);
          artifacts.global_keep_alive.emplace_back(v8_isolate, prototype);
          return true;
        };

        if (!capture_optional_object(
                "exceptionPackage", "exceptionPackagePrototype",
                WASM_EXCEPTION_PACKAGE_TYPE, &fixture.exception_package)) {
          PrintF("[TODO] WASM_EXCEPTION_PACKAGE_TYPE is not constructible: %s\n",
                 *v8::String::Utf8Value(v8_isolate,
                                        get_property("exceptionPackageTodo")));
        }
        if (!capture_optional_object("suspending", "suspendingPrototype",
                                     WASM_SUSPENDING_OBJECT_TYPE,
                                     &fixture.suspending)) {
          PrintF("[TODO] WASM_SUSPENDING_OBJECT_TYPE is not constructible: %s\n",
                 *v8::String::Utf8Value(v8_isolate,
                                        get_property("suspendingTodo")));
        }
        if (!capture_optional_object("tag", "tagPrototype",
                                     WASM_TAG_OBJECT_TYPE, &fixture.tag)) {
          PrintF("[TODO] WASM_TAG_OBJECT_TYPE is not constructible: %s\n",
                 *v8::String::Utf8Value(v8_isolate, get_property("tagTodo")));
        }

        fixture.value.object_address = value_object->address();
        fixture.value.map_address = value_object->map()->address();
        return artifacts;
      });
  return fixture;
}

WasmStructDumpFixture DumpWasmStructFixture() {
  WasmStructDumpFixture fixture;
  fixture.bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        USE(context);
        DumpArtifacts artifacts;
        DirectHandle<JSObject> target =
            isolate->factory()->NewJSObject(isolate->object_function());
        DirectHandle<WasmStruct> wasm_struct =
            CreateWasmStructWithScalarAndRefField(isolate, target, &artifacts);

        InvokeAtomicMajorGC(isolate->heap());
        fixture.struct_address = wasm_struct->address();
        fixture.target_address = target->address();
        fixture.map_address = wasm_struct->map()->address();
        fixture.type_index =
            wasm_struct->map()->wasm_type_info()->type_index().index;
        return artifacts;
      });
  CHECK_NE(static_cast<Address>(0), fixture.struct_address);
  CHECK_NE(static_cast<Address>(0), fixture.target_address);
  CHECK_NE(static_cast<Address>(0), fixture.map_address);
  RawHeapHeader header = ReadRawHeapHeader(fixture.bytes);
  CHECK(dfx::HasRawHeapOffheapTableSection(header));
  return fixture;
}

WasmStructDescriptorDumpFixture DumpWasmStructDescriptorFixture() {
  WasmStructDescriptorDumpFixture fixture;
  fixture.bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        USE(context);
        DumpArtifacts artifacts;
        DirectHandle<WasmStruct> descriptor =
            CreateWasmDescriptorStruct(isolate, &artifacts);

        InvokeAtomicMajorGC(isolate->heap());
        fixture.descriptor_address = descriptor->address();
        fixture.described_rtt_address = descriptor->described_rtt()->address();
        fixture.map_address = descriptor->map()->address();
        fixture.type_index =
            descriptor->map()->wasm_type_info()->type_index().index;
        return artifacts;
      });
  CHECK_NE(static_cast<Address>(0), fixture.descriptor_address);
  CHECK_NE(static_cast<Address>(0), fixture.described_rtt_address);
  CHECK_NE(static_cast<Address>(0), fixture.map_address);
  RawHeapHeader header = ReadRawHeapHeader(fixture.bytes);
  CHECK(dfx::HasRawHeapOffheapTableSection(header));
  return fixture;
}

WasmStringViewIterDumpFixture DumpWasmStringViewIterFixture() {
  FlagScope<bool> stringref(&v8_flags.experimental_wasm_stringref, true);
  FlagScope<bool> wasm_staging(&v8_flags.wasm_staging, true);
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  v8::HandleScope scope(CcTest::isolate());
  LocalContext env(CcTest::isolate());
  USE(env);

  WasmStringViewIterDumpFixture fixture;
  DumpArtifacts artifacts;
  DirectHandle<String> string =
      isolate->factory()->InternalizeUtf8String("heap-dump-stringref");
  DirectHandle<WasmStruct> holder =
      CreateWasmStructHoldingStringViewIter(isolate, string, &artifacts);
  wasm::WasmValue field = holder->GetFieldValue(0);
  DirectHandle<Object> iterator_object = field.to_ref();
  CHECK(IsHeapObject(*iterator_object));
  DirectHandle<HeapObject> iterator = Cast<HeapObject>(iterator_object);
  CHECK_EQ(WASM_STRING_VIEW_ITER_TYPE, iterator->map()->instance_type());
  KeepAlive(&artifacts.keep_alive, iterator);

  InvokeAtomicMajorGC(isolate->heap());
  CHECK_EQ(WASM_STRING_VIEW_ITER_TYPE, iterator->map()->instance_type());
  fixture.iterator_address = iterator->address();
  fixture.iterator_map_address = iterator->map()->address();
  fixture.holder_address = holder->address();
  fixture.string_address = string->address();
  fixture.bytes = DumpHeapBytes(isolate);
  CHECK_NE(static_cast<Address>(0), fixture.iterator_address);
  CHECK_NE(static_cast<Address>(0), fixture.iterator_map_address);
  CHECK_NE(static_cast<Address>(0), fixture.holder_address);
  CHECK_NE(static_cast<Address>(0), fixture.string_address);
  return fixture;
}

WasmArrayDumpFixture DumpWasmRefArrayFixture() {
  WasmArrayDumpFixture fixture;
  fixture.bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        USE(context);
        DumpArtifacts artifacts;
        DirectHandle<JSObject> target =
            isolate->factory()->NewJSObject(isolate->object_function());
        DirectHandle<WasmArray> wasm_array =
            CreateWasmExternRefArrayWithTarget(isolate, target, &artifacts);

        InvokeAtomicMajorGC(isolate->heap());
        fixture.array_address = wasm_array->address();
        fixture.target_address = target->address();
        fixture.map_address = wasm_array->map()->address();
        DirectHandle<WasmTypeInfo> type_info(
            wasm_array->map()->wasm_type_info(), isolate);
        CHECK_EQ(WASM_TYPE_INFO_TYPE, type_info->map()->instance_type());
        KeepAlive(&artifacts.keep_alive, type_info);
        fixture.type_info_address = type_info->address();
        fixture.type_info_map_address = type_info->map()->address();
        return artifacts;
      });
  CHECK_NE(static_cast<Address>(0), fixture.array_address);
  CHECK_NE(static_cast<Address>(0), fixture.target_address);
  CHECK_NE(static_cast<Address>(0), fixture.map_address);
  CHECK_NE(static_cast<Address>(0), fixture.type_info_address);
  CHECK_NE(static_cast<Address>(0), fixture.type_info_map_address);
  return fixture;
}

WasmArrayDumpFixture DumpWasmScalarArrayFixture() {
  WasmArrayDumpFixture fixture;
  fixture.bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        USE(context);
        DumpArtifacts artifacts;
        DirectHandle<JSObject> target =
            isolate->factory()->NewJSObject(isolate->object_function());
        DirectHandle<WasmArray> wasm_array =
            CreateWasmScalarArray(isolate, &artifacts);

        KeepAlive(&artifacts.keep_alive, target);
        InvokeAtomicMajorGC(isolate->heap());
        fixture.array_address = wasm_array->address();
        fixture.target_address = target->address();
        fixture.map_address = wasm_array->map()->address();
        DirectHandle<WasmTypeInfo> type_info(
            wasm_array->map()->wasm_type_info(), isolate);
        CHECK_EQ(WASM_TYPE_INFO_TYPE, type_info->map()->instance_type());
        KeepAlive(&artifacts.keep_alive, type_info);
        fixture.type_info_address = type_info->address();
        fixture.type_info_map_address = type_info->map()->address();
        return artifacts;
      });
  CHECK_NE(static_cast<Address>(0), fixture.array_address);
  CHECK_NE(static_cast<Address>(0), fixture.target_address);
  CHECK_NE(static_cast<Address>(0), fixture.map_address);
  CHECK_NE(static_cast<Address>(0), fixture.type_info_address);
  CHECK_NE(static_cast<Address>(0), fixture.type_info_map_address);
  return fixture;
}

[[maybe_unused]] WasmTrustedInstanceDataDumpFixture
DumpWasmTrustedInstanceDataProtectedFieldsFixture() {
  WasmTrustedInstanceDataDumpFixture fixture;
  fixture.bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        USE(context);
        DumpArtifacts artifacts;
        DirectHandle<WasmTrustedInstanceData> trusted_data =
            CreateWasmTrustedInstanceDataForTest(isolate, &artifacts);
        DirectHandle<WasmDispatchTable> dispatch_table0(
            trusted_data->dispatch_table0(), isolate);
        CHECK_EQ(WASM_DISPATCH_TABLE_TYPE,
                 dispatch_table0->map()->instance_type());
        DirectHandle<WasmDispatchTableForImports> dispatch_table_for_imports(
            trusted_data->dispatch_table_for_imports(), isolate);
        CHECK_EQ(WASM_DISPATCH_TABLE_FOR_IMPORTS_TYPE,
                 dispatch_table_for_imports->map()->instance_type());
        CHECK_NE(dispatch_table0->address(),
                 dispatch_table_for_imports->address());
        KeepAlive(&artifacts.keep_alive, dispatch_table0);
        KeepAlive(&artifacts.keep_alive, dispatch_table_for_imports);

        InvokeAtomicMajorGC(isolate->heap());
        fixture.trusted_data_address = trusted_data->address();
        fixture.trusted_data_map_address = trusted_data->map()->address();
        fixture.shared_part_address = trusted_data->shared_part()->address();
        fixture.memory_bases_and_sizes_address =
            trusted_data->memory_bases_and_sizes()->address();
        fixture.dispatch_table0_address = dispatch_table0->address();
        fixture.dispatch_table0_map_address =
            dispatch_table0->map()->address();
        fixture.dispatch_tables_address =
            trusted_data->dispatch_tables()->address();
        fixture.dispatch_table_for_imports_address =
            dispatch_table_for_imports->address();
        fixture.dispatch_table_for_imports_map_address =
            dispatch_table_for_imports->map()->address();
        return artifacts;
      });
  CHECK_NE(static_cast<Address>(0), fixture.trusted_data_address);
  CHECK_NE(static_cast<Address>(0), fixture.shared_part_address);
  CHECK_NE(static_cast<Address>(0), fixture.memory_bases_and_sizes_address);
  CHECK_NE(static_cast<Address>(0), fixture.dispatch_table0_address);
  CHECK_NE(static_cast<Address>(0), fixture.dispatch_table0_map_address);
  CHECK_NE(static_cast<Address>(0), fixture.dispatch_tables_address);
  CHECK_NE(static_cast<Address>(0), fixture.dispatch_table_for_imports_address);
  CHECK_NE(static_cast<Address>(0),
           fixture.dispatch_table_for_imports_map_address);
  return fixture;
}

#if V8_TARGET_OS_LINUX
[[maybe_unused]] WasmMemoryMapDescriptorDumpFixture DumpWasmMemoryMapDescriptorFixture() {
  WasmMemoryMapDescriptorDumpFixture fixture;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* v8_isolate = v8::Isolate::New(create_params);
  Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);

  v8_isolate->Enter();
  {
    v8::HandleScope scope(v8_isolate);
    FlagScope<bool> memory_control(
        &v8_flags.experimental_wasm_memory_control, true);
    LocalContext env(v8_isolate);
    USE(env);
    std::vector<DirectHandle<Object>> keep_alive;
    MaybeDirectHandle<WasmMemoryMapDescriptor> maybe_descriptor =
        WasmMemoryMapDescriptor::NewFromAnonymous(isolate, 4096);
    CHECK(!maybe_descriptor.is_null());
    DirectHandle<WasmMemoryMapDescriptor> descriptor =
        maybe_descriptor.ToHandleChecked();
    CHECK_EQ(WASM_MEMORY_MAP_DESCRIPTOR_TYPE,
             descriptor->map()->instance_type());
    KeepAlive(&keep_alive, descriptor);

    InvokeAtomicMajorGC(isolate->heap());
    fixture.descriptor_address = descriptor->address();
    fixture.descriptor_map_address = descriptor->map()->address();
    fixture.bytes = DumpHeapBytes(isolate);
    USE(keep_alive);
  }
  v8_isolate->Exit();
  v8_isolate->Dispose();
  CHECK_NE(static_cast<Address>(0), fixture.descriptor_address);
  CHECK_NE(static_cast<Address>(0), fixture.descriptor_map_address);
  return fixture;
}
#endif

WasmFunctionImportDataDumpFixture DumpWasmFunctionImportDataFixture() {
  WasmFunctionImportDataDumpFixture fixture;
  fixture.bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        DumpArtifacts artifacts;
        RunScript(context, R"(
          globalThis.__heap_dump_wasm_function_imports = {
            m: {
              imported: function HeapDumpWasmImportedFunction(x) {
                return x + 1;
              }
            }
          };
          globalThis.__heap_dump_wasm_js_function_callable =
              function HeapDumpWasmJSFunctionCallable(x) {
                return x;
              };
        )");
        v8::Local<v8::Value> imports_value =
            context->Global()
                ->Get(context,
                      NewApiString(v8_isolate,
                                   "__heap_dump_wasm_function_imports"))
                .ToLocalChecked();
        CHECK(imports_value->IsObject());
        DirectHandle<JSReceiver> imports = LocalJSReceiver(imports_value);

        AccountingAllocator allocator;
        Zone zone(&allocator, ZONE_NAME);
        WasmModuleBuilder builder(&zone);
        TestSignatures sigs;
        const uint32_t import_index =
            builder.AddImport(base::CStrVector("imported"), sigs.i_i(),
                              base::CStrVector("m"));
        WasmFunctionBuilder* exported_fun = builder.AddFunction(sigs.i_i());
        exported_fun->EmitCode(
            {WASM_CALL_FUNCTION(import_index, WASM_LOCAL_GET(0)), WASM_END});
        builder.AddExport(base::CStrVector("call_import"), exported_fun);

        ZoneBuffer buffer(&zone);
        builder.WriteTo(&buffer);
        wasm::ErrorThrower thrower(isolate, "HeapDump wasm function data");
        MaybeDirectHandle<WasmModuleObject> maybe_module =
            testing::CompileForTesting(
                isolate, &thrower,
                base::VectorOf(buffer.begin(), buffer.size()));
        if (thrower.error()) FATAL("%s", thrower.error_msg());
        DirectHandle<WasmModuleObject> module = maybe_module.ToHandleChecked();
        MaybeDirectHandle<WasmInstanceObject> maybe_instance =
            GetWasmEngine()->SyncInstantiate(
                isolate, &thrower, module, imports,
                DirectHandle<JSArrayBuffer>::null());
        if (thrower.error()) FATAL("%s", thrower.error_msg());
        DirectHandle<WasmInstanceObject> instance =
            maybe_instance.ToHandleChecked();
        DirectHandle<WasmExportedFunction> exported =
            testing::GetExportedFunction(isolate, instance, "call_import")
                .ToHandleChecked();
        DirectHandle<Object> arg = handle(Smi::FromInt(41), isolate);
        DirectHandle<Object> result =
            Execution::Call(isolate, exported,
                            isolate->factory()->undefined_value(),
                            base::VectorOf(&arg, 1))
                .ToHandleChecked();
        CHECK_EQ(42, Object::NumberValue(*result));

        DirectHandle<WasmExportedFunctionData> exported_data(
            exported->shared()->wasm_exported_function_data(), isolate);
        CHECK_EQ(WASM_EXPORTED_FUNCTION_DATA_TYPE,
                 exported_data->map()->instance_type());
        DirectHandle<WasmFuncRef> exported_func_ref(exported_data->func_ref(),
                                                    isolate);
        CHECK_EQ(WASM_FUNC_REF_TYPE, exported_func_ref->map()->instance_type());
        DirectHandle<WasmInternalFunction> exported_internal(
            exported_data->internal(), isolate);
        CHECK_EQ(WASM_INTERNAL_FUNCTION_TYPE,
                 exported_internal->map()->instance_type());

        DirectHandle<WasmTrustedInstanceData> trusted_data(
            instance->trusted_data(isolate), isolate);
        DirectHandle<WasmImportData> import_data(
            TrustedCast<WasmImportData>(
                trusted_data->dispatch_table_for_imports()->implicit_arg(
                    import_index)),
            isolate);
        CHECK_EQ(WASM_IMPORT_DATA_TYPE, import_data->map()->instance_type());

        v8::Local<v8::Value> callable_value =
            context->Global()
                ->Get(context,
                      NewApiString(v8_isolate,
                                   "__heap_dump_wasm_js_function_callable"))
                .ToLocalChecked();
        CHECK(callable_value->IsFunction());
        DirectHandle<JSFunction> callable =
            Cast<JSFunction>(v8::Utils::OpenDirectHandle(*callable_value));
        DirectHandle<WasmJSFunction> wasm_js_function =
            WasmJSFunction::New(isolate, sigs.i_i(), callable,
                                wasm::kNoSuspend);
        DirectHandle<WasmJSFunctionData> js_function_data(
            wasm_js_function->shared()->wasm_js_function_data(), isolate);
        CHECK_EQ(WASM_JS_FUNCTION_DATA_TYPE,
                 js_function_data->map()->instance_type());
        DirectHandle<WasmFuncRef> js_function_func_ref(
            js_function_data->func_ref(), isolate);
        CHECK_EQ(WASM_FUNC_REF_TYPE,
                 js_function_func_ref->map()->instance_type());
        DirectHandle<WasmInternalFunction> js_function_internal(
            js_function_data->internal(), isolate);
        CHECK_EQ(WASM_INTERNAL_FUNCTION_TYPE,
                 js_function_internal->map()->instance_type());

        KeepAlive(&artifacts.keep_alive, imports);
        KeepAlive(&artifacts.keep_alive, module);
        KeepAlive(&artifacts.keep_alive, instance);
        KeepAlive(&artifacts.keep_alive, exported);
        KeepAlive(&artifacts.keep_alive, exported_data);
        KeepAlive(&artifacts.keep_alive, exported_func_ref);
        KeepAlive(&artifacts.keep_alive, exported_internal);
        KeepAlive(&artifacts.keep_alive, import_data);
        KeepAlive(&artifacts.keep_alive, callable);
        KeepAlive(&artifacts.keep_alive, wasm_js_function);
        KeepAlive(&artifacts.keep_alive, js_function_data);
        KeepAlive(&artifacts.keep_alive, js_function_func_ref);
        KeepAlive(&artifacts.keep_alive, js_function_internal);

        InvokeAtomicMajorGC(isolate->heap());
        fixture.exported_function_data_address = exported_data->address();
        fixture.exported_function_data_map_address =
            exported_data->map()->address();
        fixture.exported_function_data_func_ref_address =
            exported_func_ref->address();
        fixture.exported_internal_function_address =
            exported_internal->address();
        fixture.exported_internal_function_map_address =
            exported_internal->map()->address();
        fixture.wasm_js_function_data_address = js_function_data->address();
        fixture.wasm_js_function_data_map_address =
            js_function_data->map()->address();
        fixture.wasm_js_function_data_func_ref_address =
            js_function_func_ref->address();
        fixture.wasm_js_function_data_internal_address =
            js_function_internal->address();
        fixture.wasm_import_data_address = import_data->address();
        fixture.wasm_import_data_map_address = import_data->map()->address();
        return artifacts;
      });
  CHECK_NE(static_cast<Address>(0), fixture.exported_function_data_address);
  CHECK_NE(static_cast<Address>(0),
           fixture.exported_function_data_func_ref_address);
  CHECK_NE(static_cast<Address>(0), fixture.exported_internal_function_address);
  CHECK_NE(static_cast<Address>(0), fixture.wasm_js_function_data_address);
  CHECK_NE(static_cast<Address>(0),
           fixture.wasm_js_function_data_func_ref_address);
  CHECK_NE(static_cast<Address>(0),
           fixture.wasm_js_function_data_internal_address);
  CHECK_NE(static_cast<Address>(0), fixture.wasm_import_data_address);
  return fixture;
}

WasmHardInternalTypesDumpFixture DumpWasmHardInternalTypesFixture() {
  WasmHardInternalTypesDumpFixture fixture;
  fixture.bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        USE(context);
        DumpArtifacts artifacts;

        AccountingAllocator allocator;
        Zone zone(&allocator, ZONE_NAME);
        WasmModuleBuilder builder(&zone);
        TestSignatures sigs;
        WasmFunctionBuilder* fun = builder.AddFunction(sigs.v_v());
        fun->EmitCode({kExprEnd});
        builder.AddExport(base::CStrVector("run"), fun);

        ZoneBuffer buffer(&zone);
        builder.WriteTo(&buffer);
        wasm::ErrorThrower thrower(isolate, "HeapDump wasm hard internals");
        MaybeDirectHandle<WasmModuleObject> maybe_module =
            testing::CompileForTesting(isolate, &thrower,
                                       base::VectorOf(buffer));
        if (thrower.error()) FATAL("%s", thrower.error_msg());
        DirectHandle<WasmModuleObject> module = maybe_module.ToHandleChecked();

        DirectHandle<HeapNumber> uses_bitset =
            isolate->factory()->NewHeapNumberFromBits(0);
        DirectHandle<AsmWasmData> asm_wasm_data =
            AsmWasmData::New(isolate, module->shared_native_module(),
                             uses_bitset);
        CHECK_EQ(ASM_WASM_DATA_TYPE, asm_wasm_data->map()->instance_type());

        DirectHandle<WasmExceptionTag> exception_tag =
            WasmExceptionTag::New(isolate, 17);
        CHECK_EQ(WASM_EXCEPTION_TAG_TYPE,
                 exception_tag->map()->instance_type());

        DirectHandle<Foreign> signature = CreateForeignForTest(isolate);
        DirectHandle<Object> callback_data =
            isolate->factory()->NewNumberFromInt(23);
        DirectHandle<WasmFastApiCallData> fast_api_call_data =
            isolate->factory()->NewWasmFastApiCallData(signature,
                                                       callback_data);
        CHECK_EQ(WASM_FAST_API_CALL_DATA_TYPE,
                 fast_api_call_data->map()->instance_type());

        MaybeDirectHandle<WasmInstanceObject> maybe_instance =
            testing::CompileAndInstantiateForTesting(
                isolate, &thrower, base::VectorOf(buffer));
        if (thrower.error()) FATAL("%s", thrower.error_msg());
        DirectHandle<WasmInstanceObject> instance =
            maybe_instance.ToHandleChecked();
        DirectHandle<WasmExportedFunction> exported =
            testing::GetExportedFunction(isolate, instance, "run")
                .ToHandleChecked();
        DirectHandle<WasmExportedFunctionData> exported_data(
            exported->shared()->wasm_exported_function_data(), isolate);
        DirectHandle<WasmFuncRef> func_ref(exported_data->func_ref(), isolate);
        CHECK_EQ(WASM_FUNC_REF_TYPE, func_ref->map()->instance_type());

        DirectHandle<WasmNull> wasm_null = isolate->factory()->wasm_null();
        CHECK_EQ(WASM_NULL_TYPE, wasm_null->map()->instance_type());

        KeepAlive(&artifacts.keep_alive, module);
        KeepAlive(&artifacts.keep_alive, uses_bitset);
        KeepAlive(&artifacts.keep_alive, asm_wasm_data);
        KeepAlive(&artifacts.keep_alive, exception_tag);
        KeepAlive(&artifacts.keep_alive, signature);
        KeepAlive(&artifacts.keep_alive, callback_data);
        KeepAlive(&artifacts.keep_alive, fast_api_call_data);
        KeepAlive(&artifacts.keep_alive, instance);
        KeepAlive(&artifacts.keep_alive, exported);
        KeepAlive(&artifacts.keep_alive, exported_data);
        KeepAlive(&artifacts.keep_alive, func_ref);
        KeepAlive(&artifacts.keep_alive, wasm_null);

        InvokeAtomicMajorGC(isolate->heap());
        fixture.asm_wasm_data_address = asm_wasm_data->address();
        fixture.asm_wasm_data_map_address = asm_wasm_data->map()->address();
        fixture.exception_tag_address = exception_tag->address();
        fixture.exception_tag_map_address = exception_tag->map()->address();
        fixture.fast_api_call_data_address = fast_api_call_data->address();
        fixture.fast_api_call_data_map_address =
            fast_api_call_data->map()->address();
        fixture.func_ref_address = func_ref->address();
        fixture.func_ref_map_address = func_ref->map()->address();
        fixture.wasm_null_address = wasm_null->address();
        fixture.wasm_null_map_address = wasm_null->map()->address();
        return artifacts;
      });
  CHECK_NE(static_cast<Address>(0), fixture.asm_wasm_data_address);
  CHECK_NE(static_cast<Address>(0), fixture.exception_tag_address);
  CHECK_NE(static_cast<Address>(0), fixture.fast_api_call_data_address);
  CHECK_NE(static_cast<Address>(0), fixture.func_ref_address);
  CHECK_NE(static_cast<Address>(0), fixture.wasm_null_address);
  return fixture;
}

WasmCapiSuspenderContinuationDumpFixture
DumpWasmCapiSuspenderContinuationFixture() {
  WasmCapiSuspenderContinuationDumpFixture fixture;
  fixture.bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        USE(context);
        DumpArtifacts artifacts;

        TestSignatures sigs;
        const wasm::CanonicalSig* canonical_sig = CanonicalizeSig(sigs.i_i());

        DirectHandle<Foreign> embedder_data = CreateForeignForTest(isolate);
        DirectHandle<WasmCapiFunction> wasm_capi_function =
            WasmCapiFunction::New(
                isolate, reinterpret_cast<Address>(&DummyCapiTarget),
                embedder_data, canonical_sig);
        DirectHandle<WasmCapiFunctionData> wasm_capi_function_data(
            wasm_capi_function->shared()->wasm_capi_function_data(), isolate);
        CHECK_EQ(WASM_CAPI_FUNCTION_DATA_TYPE,
                 wasm_capi_function_data->map()->instance_type());
        DirectHandle<WasmFuncRef> wasm_capi_function_data_func_ref(
            wasm_capi_function_data->func_ref(), isolate);
        CHECK_EQ(WASM_FUNC_REF_TYPE,
                 wasm_capi_function_data_func_ref->map()->instance_type());
        DirectHandle<WasmInternalFunction> wasm_capi_function_data_internal(
            wasm_capi_function_data->internal(), isolate);
        CHECK_EQ(WASM_INTERNAL_FUNCTION_TYPE,
                 wasm_capi_function_data_internal->map()->instance_type());

        DirectHandle<WasmSuspenderObject> suspender =
            isolate->factory()->NewWasmSuspenderObjectInitialized();
        CHECK_EQ(WASM_SUSPENDER_OBJECT_TYPE,
                 suspender->map()->instance_type());

        DirectHandle<WasmContinuationObject> continuation =
            isolate->factory()->NewWasmContinuationObject(nullptr);
        CHECK_EQ(WASM_CONTINUATION_OBJECT_TYPE,
                 continuation->map()->instance_type());

        DirectHandle<WasmResumeData> resume_data =
            isolate->factory()->NewWasmResumeData(
                suspender, wasm::OnResume::kContinue);
        CHECK_EQ(WASM_RESUME_DATA_TYPE,
                 resume_data->map()->instance_type());

        KeepAlive(&artifacts.keep_alive, embedder_data);
        KeepAlive(&artifacts.keep_alive, wasm_capi_function);
        KeepAlive(&artifacts.keep_alive, wasm_capi_function_data);
        KeepAlive(&artifacts.keep_alive, wasm_capi_function_data_func_ref);
        KeepAlive(&artifacts.keep_alive, wasm_capi_function_data_internal);
        KeepAlive(&artifacts.keep_alive, suspender);
        KeepAlive(&artifacts.keep_alive, continuation);
        KeepAlive(&artifacts.keep_alive, resume_data);

        InvokeAtomicMajorGC(isolate->heap());
        fixture.wasm_capi_function_data_address =
            wasm_capi_function_data->address();
        fixture.wasm_capi_function_data_map_address =
            wasm_capi_function_data->map()->address();
        fixture.wasm_capi_function_data_func_ref_address =
            wasm_capi_function_data_func_ref->address();
        fixture.wasm_capi_function_data_internal_address =
            wasm_capi_function_data_internal->address();
        fixture.suspender_address = suspender->address();
        fixture.suspender_map_address = suspender->map()->address();
        fixture.continuation_address = continuation->address();
        fixture.continuation_map_address = continuation->map()->address();
        fixture.resume_data_address = resume_data->address();
        fixture.resume_data_map_address = resume_data->map()->address();
        return artifacts;
      });
  CHECK_NE(static_cast<Address>(0),
           fixture.wasm_capi_function_data_address);
  CHECK_NE(static_cast<Address>(0),
           fixture.wasm_capi_function_data_map_address);
  CHECK_NE(static_cast<Address>(0),
           fixture.wasm_capi_function_data_func_ref_address);
  CHECK_NE(static_cast<Address>(0),
           fixture.wasm_capi_function_data_internal_address);
  CHECK_NE(static_cast<Address>(0), fixture.suspender_address);
  CHECK_NE(static_cast<Address>(0), fixture.suspender_map_address);
  CHECK_NE(static_cast<Address>(0), fixture.continuation_address);
  CHECK_NE(static_cast<Address>(0), fixture.continuation_map_address);
  CHECK_NE(static_cast<Address>(0), fixture.resume_data_address);
  CHECK_NE(static_cast<Address>(0), fixture.resume_data_map_address);
  return fixture;
}

WasmCanonicalTableInfo FindWasmCanonicalTableInfo(
    const std::vector<uint8_t>& bytes) {
  WasmCanonicalTableInfo info;
  info.header = ReadRawHeapHeader(bytes);
  CHECK(dfx::HasRawHeapOffheapTableSection(info.header));
  const uint64_t directory_size =
      static_cast<uint64_t>(info.header.offheap_table_count_) *
      sizeof(dfx::RawHeapOffheapTableHeader);
  CHECK_LE(directory_size, info.header.offheap_table_size_);
  CHECK_LE(static_cast<uint64_t>(info.header.offheap_table_offset_) +
               info.header.offheap_table_size_,
           bytes.size());
  for (uint32_t index = 0; index < info.header.offheap_table_count_; ++index) {
    const uint32_t entry_offset =
        info.header.offheap_table_offset_ +
        index * static_cast<uint32_t>(sizeof(dfx::RawHeapOffheapTableHeader));
    dfx::RawHeapOffheapTableHeader table =
        ReadPod<dfx::RawHeapOffheapTableHeader>(bytes, entry_offset);
    CHECK_LE(table.offset_, info.header.offheap_table_size_);
    CHECK_LE(table.size_, info.header.offheap_table_size_ - table.offset_);
    if (table.kind_ == static_cast<uint32_t>(
                           dfx::RawHeapOffheapTableKind::
                               kWasmCanonicalTypeTable)) {
      info.directory_entry_offset = entry_offset;
      info.table = table;
      info.payload_offset = info.header.offheap_table_offset_ + table.offset_;
      return info;
    }
  }
  UNREACHABLE();
}

WasmStructRecordInfo ReadWasmStructRecordInfo(
    const std::vector<uint8_t>& bytes, uint32_t type_index) {
  WasmCanonicalTableInfo table_info = FindWasmCanonicalTableInfo(bytes);
  for (uint32_t index = 0; index < table_info.table.count_; ++index) {
    const uint32_t record_offset =
        table_info.payload_offset +
        index *
            static_cast<uint32_t>(
                sizeof(dfx::RawHeapWasmCanonicalStructRecord));
    dfx::RawHeapWasmCanonicalStructRecord record =
        ReadPod<dfx::RawHeapWasmCanonicalStructRecord>(bytes, record_offset);
    if (record.type_index_ != type_index) {
      continue;
    }

    WasmStructRecordInfo info;
    info.record = record;
    for (uint32_t field_index = 0; field_index < record.field_count_;
         ++field_index) {
      const uint32_t field_offset =
          table_info.payload_offset + record.fields_offset_ +
          field_index *
              static_cast<uint32_t>(
                  sizeof(dfx::RawHeapWasmCanonicalStructField));
      info.fields.push_back(
          ReadPod<dfx::RawHeapWasmCanonicalStructField>(bytes, field_offset));
    }
    return info;
  }
  UNREACHABLE();
}

std::vector<uint8_t> BuildWasmCanonicalPayloadForTest(
    const std::vector<dfx::RawHeapWasmCanonicalStructRecord>& records,
    const std::vector<dfx::RawHeapWasmCanonicalStructField>& fields) {
  std::vector<uint8_t> payload(
      records.size() * sizeof(dfx::RawHeapWasmCanonicalStructRecord) +
      fields.size() * sizeof(dfx::RawHeapWasmCanonicalStructField));
  uint32_t offset = 0;
  if (!records.empty()) {
    const uint32_t records_size = static_cast<uint32_t>(
        records.size() * sizeof(dfx::RawHeapWasmCanonicalStructRecord));
    memcpy(payload.data(), records.data(), records_size);
    offset += records_size;
  }
  if (!fields.empty()) {
    const uint32_t fields_size = static_cast<uint32_t>(
        fields.size() * sizeof(dfx::RawHeapWasmCanonicalStructField));
    memcpy(payload.data() + offset, fields.data(), fields_size);
  }
  return payload;
}

void ReplaceWasmCanonicalPayloadForTest(std::vector<uint8_t>* bytes,
                                        const std::vector<uint8_t>& payload,
                                        uint32_t record_count) {
  WasmCanonicalTableInfo table_info = FindWasmCanonicalTableInfo(*bytes);
  const uint64_t directory_end =
      static_cast<uint64_t>(table_info.header.offheap_table_offset_) +
      static_cast<uint64_t>(table_info.header.offheap_table_count_) *
          sizeof(dfx::RawHeapOffheapTableHeader);
  CHECK_LE(directory_end, table_info.payload_offset);
  const uint32_t old_size = table_info.table.size_;
  CHECK_LE(static_cast<size_t>(table_info.payload_offset) + old_size,
           bytes->size());
  CHECK_LE(payload.size(),
           static_cast<size_t>(std::numeric_limits<uint32_t>::max()));

  bytes->erase(bytes->begin() + table_info.payload_offset,
               bytes->begin() + table_info.payload_offset + old_size);
  bytes->insert(bytes->begin() + table_info.payload_offset, payload.begin(),
                payload.end());

  const int64_t delta =
      static_cast<int64_t>(payload.size()) - static_cast<int64_t>(old_size);
  RawHeapHeader header = table_info.header;
  CHECK(delta >= -static_cast<int64_t>(header.offheap_table_size_));
  header.offheap_table_size_ =
      static_cast<uint32_t>(static_cast<int64_t>(header.offheap_table_size_) +
                            delta);
  if (header.metadata_offset_ != 0) {
    CHECK(delta >= -static_cast<int64_t>(header.metadata_offset_));
    header.metadata_offset_ =
        static_cast<uint32_t>(static_cast<int64_t>(header.metadata_offset_) +
                              delta);
  }

  table_info.table.size_ = static_cast<uint32_t>(payload.size());
  table_info.table.count_ = record_count;
  WritePodAtForWasmTest(bytes, 0, header);
  WritePodAtForWasmTest(bytes, table_info.directory_entry_offset,
                        table_info.table);

  for (uint32_t index = 0; index < header.offheap_table_count_; ++index) {
    const uint32_t entry_offset =
        header.offheap_table_offset_ +
        index * static_cast<uint32_t>(sizeof(dfx::RawHeapOffheapTableHeader));
    if (entry_offset == table_info.directory_entry_offset) {
      continue;
    }
    dfx::RawHeapOffheapTableHeader table =
        ReadPod<dfx::RawHeapOffheapTableHeader>(*bytes, entry_offset);
    if (table.offset_ <= table_info.table.offset_) {
      continue;
    }
    CHECK(delta >= -static_cast<int64_t>(table.offset_));
    table.offset_ =
        static_cast<uint32_t>(static_cast<int64_t>(table.offset_) + delta);
    WritePodAtForWasmTest(bytes, entry_offset, table);
  }
}

void RemoveOffheapSectionForWasmTest(std::vector<uint8_t>* bytes) {
  RawHeapHeader header = ReadRawHeapHeader(*bytes);
  CHECK(dfx::HasRawHeapOffheapTableSection(header));
  CHECK_LE(static_cast<size_t>(header.offheap_table_offset_) +
               header.offheap_table_size_,
           bytes->size());
  bytes->erase(bytes->begin() + header.offheap_table_offset_,
               bytes->begin() + header.offheap_table_offset_ +
                   header.offheap_table_size_);
  if (header.metadata_offset_ != 0) {
    CHECK_GE(header.metadata_offset_, header.offheap_table_size_);
    header.metadata_offset_ -= header.offheap_table_size_;
  }
  header.offheap_table_offset_ = 0;
  header.offheap_table_size_ = 0;
  header.offheap_table_count_ = 0;
  WritePodAtForWasmTest(bytes, 0, header);
}

void ReplaceWasmCanonicalTableWithEmptyForWasmTest(
    std::vector<uint8_t>* bytes) {
  ReplaceWasmCanonicalPayloadForTest(bytes, {}, 0);
}

void ReplaceWasmCanonicalTableWithSingleRecordForWasmTest(
    std::vector<uint8_t>* bytes, const WasmStructDumpFixture& fixture,
    std::vector<dfx::RawHeapWasmCanonicalStructField> fields) {
  WasmStructRecordInfo record_info =
      ReadWasmStructRecordInfo(*bytes, fixture.type_index);
  dfx::RawHeapWasmCanonicalStructRecord record = record_info.record;
  record.field_count_ = static_cast<uint32_t>(fields.size());
  record.fields_offset_ =
      static_cast<uint32_t>(sizeof(dfx::RawHeapWasmCanonicalStructRecord));
  std::vector<uint8_t> payload =
      BuildWasmCanonicalPayloadForTest({record}, fields);
  ReplaceWasmCanonicalPayloadForTest(bytes, payload, 1);
}

void MutateWasmCanonicalTableDuplicateTypeIndexForWasmTest(
    std::vector<uint8_t>* bytes, const WasmStructDumpFixture& fixture) {
  WasmStructRecordInfo record_info =
      ReadWasmStructRecordInfo(*bytes, fixture.type_index);
  CHECK(!record_info.fields.empty());
  dfx::RawHeapWasmCanonicalStructRecord record = record_info.record;
  record.fields_offset_ = 2 * static_cast<uint32_t>(
                                  sizeof(dfx::RawHeapWasmCanonicalStructRecord));
  std::vector<uint8_t> payload =
      BuildWasmCanonicalPayloadForTest({record, record}, record_info.fields);
  ReplaceWasmCanonicalPayloadForTest(bytes, payload, 2);
}

void MutateWasmCanonicalTableTruncatedFieldsForWasmTest(
    std::vector<uint8_t>* bytes, const WasmStructDumpFixture& fixture) {
  WasmStructRecordInfo record_info =
      ReadWasmStructRecordInfo(*bytes, fixture.type_index);
  CHECK(!record_info.fields.empty());
  dfx::RawHeapWasmCanonicalStructRecord record = record_info.record;
  record.fields_offset_ =
      static_cast<uint32_t>(sizeof(dfx::RawHeapWasmCanonicalStructRecord));
  std::vector<uint8_t> payload =
      BuildWasmCanonicalPayloadForTest({record}, record_info.fields);
  CHECK(!payload.empty());
  payload.pop_back();
  ReplaceWasmCanonicalPayloadForTest(bytes, payload, 1);
}

void MutateWasmCanonicalTableFieldsOverlapRecordsForWasmTest(
    std::vector<uint8_t>* bytes, const WasmStructDumpFixture& fixture) {
  WasmStructRecordInfo record_info =
      ReadWasmStructRecordInfo(*bytes, fixture.type_index);
  CHECK(!record_info.fields.empty());
  dfx::RawHeapWasmCanonicalStructRecord record = record_info.record;
  record.field_count_ = 1;
  record.fields_offset_ = 0;
  std::vector<uint8_t> payload =
      BuildWasmCanonicalPayloadForTest({record}, {record_info.fields[0]});
  ReplaceWasmCanonicalPayloadForTest(bytes, payload, 1);
}

void MutateWasmStructRefFieldOffsetOutOfBoundsForWasmTest(
    std::vector<uint8_t>* bytes, const WasmStructDumpFixture& fixture) {
  WasmStructRecordInfo record_info =
      ReadWasmStructRecordInfo(*bytes, fixture.type_index);
  bool found_ref_field = false;
  for (dfx::RawHeapWasmCanonicalStructField& field : record_info.fields) {
    if ((field.flags_ & dfx::kRawHeapWasmFieldIsRef) == 0) {
      continue;
    }
    field.field_offset_ = std::numeric_limits<uint32_t>::max();
    found_ref_field = true;
    break;
  }
  CHECK(found_ref_field);
  ReplaceWasmCanonicalTableWithSingleRecordForWasmTest(bytes, fixture,
                                                       record_info.fields);
}

void MutateWasmArrayTypeInfoToWrongInstanceTypeForWasmTest(
    std::vector<uint8_t>* bytes, const WasmArrayDumpFixture& fixture) {
  RawObjectRecord map_record = FindRawObject(*bytes, fixture.map_address);
  const uint32_t type_info_field_offset =
      map_record.payload_offset +
      static_cast<uint32_t>(
          Map::kConstructorOrBackPointerOrNativeContextOffset);
  WriteTaggedAtForWasmTest(bytes, type_info_field_offset,
                           EncodeHeapObjectForRawHeapFieldForWasmTest(
                               *bytes, fixture.target_address));
}

void MutateWasmArrayElementTypeToInvalidForWasmTest(
    std::vector<uint8_t>* bytes, const WasmArrayDumpFixture& fixture) {
  RawObjectRecord type_info_record =
      FindRawObject(*bytes, fixture.type_info_address);
  const uint32_t element_type_offset =
      type_info_record.payload_offset +
      static_cast<uint32_t>(WasmTypeInfo::kCanonicalElementTypeOffset);
  WritePodAtForWasmTest(bytes, element_type_offset,
                        std::numeric_limits<uint32_t>::max());
}

void CheckWasmStructMapEdgePreserved(const WasmTranslateResult& result,
                                     const WasmStructDumpFixture& fixture) {
  Node* struct_node = result.generator->GetNode(fixture.struct_address);
  CHECK_NOT_NULL(struct_node);
  Node* map_node = result.generator->GetNode(fixture.map_address);
  CHECK_NOT_NULL(map_node);
  CHECK(HasEdgeByNameFromNode(
      result.generator.get(), struct_node, Edge::Type::kInternal, "map",
      [map_node](const Node* target) { return target == map_node; }));
}

int CountWasmStructFieldEdges(const WasmTranslateResult& result,
                              const WasmStructDumpFixture& fixture,
                              const char* edge_name) {
  Node* struct_node = result.generator->GetNode(fixture.struct_address);
  CHECK_NOT_NULL(struct_node);
  return CountEdgesByNameFromNode(
      result.generator.get(), struct_node, Edge::Type::kProperty, edge_name,
      [](const Node*) { return true; });
}

void CheckNoWasmStructRefFieldEdge(const WasmTranslateResult& result,
                                   const WasmStructDumpFixture& fixture) {
  CHECK_EQ(0, CountWasmStructFieldEdges(result, fixture,
                                        "wasm_struct_field_1"));
}

template <typename TargetPredicate>
int CountEdgesByIndexFromNodeForWasmTest(SnapshotGenerator* generator,
                                         const Node* source, Edge::Type type,
                                         uint32_t edge_index,
                                         TargetPredicate target_predicate) {
  USE(generator);
  CHECK_NOT_NULL(source);
  int count = 0;
  for (const Edge* edge : source->childs_) {
    if (edge->type_ == static_cast<uint32_t>(type) &&
        edge->name_or_index_ == edge_index &&
        target_predicate(edge->to_node_)) {
      ++count;
    }
  }
  return count;
}

template <typename TargetPredicate>
bool HasEdgeByIndexFromNodeForWasmTest(SnapshotGenerator* generator,
                                       const Node* source, Edge::Type type,
                                       uint32_t edge_index,
                                       TargetPredicate target_predicate) {
  return CountEdgesByIndexFromNodeForWasmTest(generator, source, type,
                                              edge_index,
                                              target_predicate) > 0;
}

int CountEdgesFromNodeToTargetForWasmTest(const Node* source, Edge::Type type,
                                          const Node* target) {
  CHECK_NOT_NULL(source);
  int count = 0;
  for (const Edge* edge : source->childs_) {
    if (edge->type_ == static_cast<uint32_t>(type) &&
        edge->to_node_ == target) {
      ++count;
    }
  }
  return count;
}

int CountEdgesFromNodeByTypeForWasmTest(const Node* source, Edge::Type type) {
  CHECK_NOT_NULL(source);
  int count = 0;
  for (const Edge* edge : source->childs_) {
    if (edge->type_ == static_cast<uint32_t>(type)) {
      ++count;
    }
  }
  return count;
}

void CheckNoWasmArrayElementEdgeToTarget(const WasmTranslateResult& result,
                                         const WasmArrayDumpFixture& fixture) {
  Node* array_node = result.generator->GetNode(fixture.array_address);
  CHECK_NOT_NULL(array_node);
  Node* target_node = result.generator->GetNode(fixture.target_address);
  CHECK_NOT_NULL(target_node);
  CHECK_EQ(0, CountEdgesFromNodeToTargetForWasmTest(
                  array_node, Edge::Type::kElement, target_node));
  CHECK_EQ(0, CountEdgesFromNodeToTargetForWasmTest(
                  array_node, Edge::Type::kHidden, target_node));
}

[[maybe_unused]] void CheckNamedInternalEdgeToTargetForWasmTest(
    const WasmTranslateResult& result, Address source_address,
    const char* edge_name, Address target_address) {
  Node* source_node = result.generator->GetNode(source_address);
  CHECK_NOT_NULL(source_node);
  Node* target_node = result.generator->GetNode(target_address);
  CHECK_NOT_NULL(target_node);
  CHECK(NodeHasType(source_node, Node::Type::kObject) ||
        NodeHasType(source_node, Node::Type::kHidden));
  CHECK(NodeHasType(target_node, Node::Type::kObject) ||
        NodeHasType(target_node, Node::Type::kHidden) ||
        NodeHasType(target_node, Node::Type::kArray));
  CHECK_EQ(1, CountEdgesByNameFromNode(
                  result.generator.get(), source_node, Edge::Type::kInternal,
                  edge_name,
                  [target_node](const Node* target) {
                    return target == target_node;
                  }));
  CHECK_EQ(0, CountEdgesFromNodeToTargetForWasmTest(
                  source_node, Edge::Type::kHidden, target_node));
  CHECK_EQ(0, CountEdgesFromNodeToTargetForWasmTest(
                  source_node, Edge::Type::kWeak, target_node));
  CHECK_EQ(0, CountEdgesFromNodeToTargetForWasmTest(
                  source_node, Edge::Type::kProperty, target_node));
  CHECK_EQ(0, CountEdgesFromNodeToTargetForWasmTest(
                  source_node, Edge::Type::kShortcut, target_node));
}

}  // namespace

TEST(HeapDumpVisitorIdsWasmCoverage) {
  VisitorIdCounts counts;
  std::unique_ptr<SnapshotGenerator> generator =
      CreateWasmCoverageSnapshot(&counts);
  CHECK_GT(generator->nodes().size(), 0u);

  std::string missing = MissingVisitorIdsMessage(counts);
  if (!missing.empty()) {
    std::cout << "HeapDump missing wasm visitor ids: " << missing << std::endl;
  }
  CHECK(missing.empty());
}

TEST(HeapDumpVisitorIdsWasmCoverageReport) {
  VisitorIdCounts counts;
  std::unique_ptr<SnapshotGenerator> generator =
      CreateWasmCoverageSnapshot(&counts);
  USE(generator);
  std::string missing = MissingVisitorIdsMessage(counts);
  std::cout << "HeapDump wasm visitor-id coverage missing: "
            << (missing.empty() ? "<none>" : missing) << std::endl;
}

TEST(HeapDumpOffheapResolverParsesWasmCanonicalStructTable) {
  constexpr uint32_t kTypeIndex = 123;
  constexpr uint32_t kFieldsOffset =
      sizeof(dfx::RawHeapWasmCanonicalStructRecord);
  std::vector<dfx::RawHeapWasmCanonicalStructRecord> records = {
      {kTypeIndex, 2, kFieldsOffset, 2 * i::kTaggedSize, 0}};
  std::vector<dfx::RawHeapWasmCanonicalStructField> fields = {
      {0, 0, dfx::kRawHeapWasmFieldIsRef | dfx::kRawHeapWasmFieldIsNullable,
       dfx::kRawHeapInvalidCanonicalIndex},
      {static_cast<uint32_t>(i::kTaggedSize), 0,
       dfx::kRawHeapWasmFieldIsRef | dfx::kRawHeapWasmFieldHasIndex, 7}};
  SyntheticOffheapTable table =
      BuildSyntheticWasmCanonicalStructTable(records, fields);

  dfx::TranslateDiagnostics diagnostics;
  dfx::OffheapTableResolver resolver = BuildResolver(table, &diagnostics);

  CHECK(resolver.IsValid());
  CHECK(!diagnostics.has_errors());
  CHECK(resolver.HasTable(
      dfx::RawHeapOffheapTableKind::kWasmCanonicalTypeTable));
  const dfx::RawHeapWasmCanonicalStructRecord* record =
      resolver.FindWasmCanonicalStruct(kTypeIndex);
  CHECK_NOT_NULL(record);
  CHECK_EQ(kTypeIndex, record->type_index_);
  CHECK_EQ(2u, record->field_count_);
  CHECK_EQ(kFieldsOffset, record->fields_offset_);
  CHECK_EQ(2u * i::kTaggedSize, record->total_fields_size_);

  dfx::WasmCanonicalStructFieldsView view;
  CHECK(resolver.GetWasmCanonicalStructFields(*record, &view));
  CHECK_EQ(2u, view.count_);
  CHECK_NOT_NULL(view.data_);
  CHECK_EQ(0u, view.data_[0].field_offset_);
  CHECK_EQ(static_cast<uint32_t>(dfx::kRawHeapWasmFieldIsRef |
                                 dfx::kRawHeapWasmFieldIsNullable),
           view.data_[0].flags_);
  CHECK_EQ(static_cast<uint32_t>(i::kTaggedSize),
           view.data_[1].field_offset_);
  CHECK_EQ(static_cast<uint32_t>(dfx::kRawHeapWasmFieldIsRef |
                                 dfx::kRawHeapWasmFieldHasIndex),
           view.data_[1].flags_);
  CHECK_EQ(7u, view.data_[1].ref_index_);
}

TEST(HeapDumpOffheapResolverRejectsMalformedWasmCanonicalStructTable) {
  constexpr uint32_t kTypeIndex = 456;
  constexpr uint32_t kRecordsSize =
      2 * sizeof(dfx::RawHeapWasmCanonicalStructRecord);
  std::vector<dfx::RawHeapWasmCanonicalStructRecord> duplicate_records = {
      {kTypeIndex, 0, kRecordsSize, 0, 0},
      {kTypeIndex, 0, kRecordsSize, 0, 0}};
  SyntheticOffheapTable duplicate_table =
      BuildSyntheticWasmCanonicalStructTable(duplicate_records, {});
  dfx::TranslateDiagnostics duplicate_diagnostics;
  dfx::OffheapTableResolver duplicate_resolver =
      BuildResolver(duplicate_table, &duplicate_diagnostics);
  CHECK(!duplicate_resolver.IsValid());
  CHECK(duplicate_diagnostics.has_errors());
  CHECK(HasDiagnostic(duplicate_diagnostics,
                      dfx::TranslateDiagnostics::Severity::kCorruptInput,
                      "wasm_canonical_struct_duplicate_type_index",
                      "offheap_table"));

  constexpr uint32_t kOneRecordFieldsOffset =
      sizeof(dfx::RawHeapWasmCanonicalStructRecord);
  std::vector<dfx::RawHeapWasmCanonicalStructRecord> truncated_records = {
      {kTypeIndex + 1, 2, kOneRecordFieldsOffset, 2 * i::kTaggedSize, 0}};
  std::vector<dfx::RawHeapWasmCanonicalStructField> one_field = {
      {0, 0, dfx::kRawHeapWasmFieldIsRef,
       dfx::kRawHeapInvalidCanonicalIndex}};
  SyntheticOffheapTable truncated_table =
      BuildSyntheticWasmCanonicalStructTable(truncated_records, one_field);
  dfx::TranslateDiagnostics truncated_diagnostics;
  dfx::OffheapTableResolver truncated_resolver =
      BuildResolver(truncated_table, &truncated_diagnostics);
  CHECK(!truncated_resolver.IsValid());
  CHECK(truncated_diagnostics.has_errors());
  CHECK(HasDiagnostic(truncated_diagnostics,
                      dfx::TranslateDiagnostics::Severity::kCorruptInput,
                      "wasm_canonical_struct_fields_truncated",
                      "offheap_table"));
}

TEST(HeapDumpOffheapResolverAcceptsMissingSection) {
  RawHeapHeader header{};
  std::vector<uint8_t> bytes(sizeof(RawHeapHeader), 0);
  WritePodAt(&bytes, 0, header);
  BinaryReaderTest reader(bytes.data(), static_cast<uint32_t>(bytes.size()));

  dfx::OffheapTableResolver resolver(&reader, header);

  CHECK(resolver.IsValid());
  CHECK(!dfx::HasRawHeapOffheapTableSection(header));
  CHECK(!resolver.HasTable(
      dfx::RawHeapOffheapTableKind::kWasmCanonicalTypeTable));
}

TEST(HeapDumpWasmCanonicalTypeTableSectionIsDumped) {
  WasmStructRefFieldDump dump = DumpWasmStructWithExternRefField();
  RawHeapHeader header = ReadRawHeapHeader(dump.bytes);

  CHECK(dfx::HasRawHeapOffheapTableSection(header));
  CHECK_GT(header.offheap_table_offset_, header.object_table_offset_);
  CHECK_EQ(header.object_table_offset_ + header.object_table_size_,
           header.offheap_table_offset_);
  if (header.metadata_offset_ != 0) {
    CHECK_LE(header.offheap_table_offset_ + header.offheap_table_size_,
             header.metadata_offset_);
  }

  std::string error;
  CHECK(dfx::ValidateRawHeapOffheapTableSection(
      header, static_cast<uint32_t>(dump.bytes.size()), &error));
  CHECK(error.empty());

  BinaryReaderTest reader(dump.bytes.data(),
                          static_cast<uint32_t>(dump.bytes.size()));
  dfx::OffheapTableResolver resolver(&reader, header);
  CHECK(resolver.IsValid());
  CHECK(resolver.HasTable(
      dfx::RawHeapOffheapTableKind::kWasmCanonicalTypeTable));
  const dfx::RawHeapWasmCanonicalStructRecord* record =
      resolver.FindWasmCanonicalStruct(dump.type_index);
  CHECK_NOT_NULL(record);
  CHECK_EQ(dump.type_index, record->type_index_);
}

TEST(HeapDumpWasmStructRefFieldTranslatesToEdge) {
  WasmStructRefFieldDump dump = DumpWasmStructWithExternRefField();

  TranslateResult translated = TranslateHeapBytesWithDiagnostics(dump.bytes);

  CHECK(translated.ok);
  CHECK(!translated.diagnostics.has_errors());
  Node* struct_node =
      GetSnapshotNode(translated.generator.get(), dump.struct_address);
  Node* target_node =
      GetSnapshotNode(translated.generator.get(), dump.target_address);
  CHECK_NOT_NULL(struct_node);
  CHECK_NOT_NULL(target_node);
  CHECK(HasEdgeByNameFromNode(
      translated.generator.get(), struct_node, Edge::Type::kProperty,
      "wasm_struct_field_0",
      [target_node](const Node* node) { return node == target_node; }));
  MarkCoveredInstanceType(WASM_STRUCT_TYPE);
}

TEST(HeapDumpWasmJsApiObjectsExactPrototypeAndMapEdges) {
  WasmJsApiObjectDumpFixture fixture = DumpWasmJsApiObjectFixture();
  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(result.ok);
  CHECK(!result.diagnostics.has_errors());

  CheckWasmJsApiObjectSnapshot(
      result.generator.get(), fixture.global_address,
      fixture.global_prototype_address, fixture.global_map_address);
  MarkCoveredInstanceType(WASM_GLOBAL_OBJECT_TYPE);

  CheckWasmJsApiObjectSnapshot(
      result.generator.get(), fixture.instance_address,
      fixture.instance_prototype_address, fixture.instance_map_address);
  MarkCoveredInstanceType(WASM_INSTANCE_OBJECT_TYPE);

  CheckWasmJsApiObjectSnapshot(
      result.generator.get(), fixture.memory_address,
      fixture.memory_prototype_address, fixture.memory_map_address);
  MarkCoveredInstanceType(WASM_MEMORY_OBJECT_TYPE);

  CheckWasmJsApiObjectSnapshot(
      result.generator.get(), fixture.module_address,
      fixture.module_prototype_address, fixture.module_map_address);
  MarkCoveredInstanceType(WASM_MODULE_OBJECT_TYPE);

  CheckWasmJsApiObjectSnapshot(
      result.generator.get(), fixture.table_address,
      fixture.table_prototype_address, fixture.table_map_address);
  MarkCoveredInstanceType(WASM_TABLE_OBJECT_TYPE);
}

TEST(HeapDumpWasmInstanceAndModuleObjectExactNamedEdges) {
  WasmJsApiObjectDumpFixture fixture = DumpWasmJsApiObjectFixture();
  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(result.ok);
  CHECK(!result.diagnostics.has_errors());

  SnapshotGenerator* generator = result.generator.get();
  Node* instance = GetSnapshotNode(generator, fixture.instance_address);
  Node* module = GetSnapshotNode(generator, fixture.module_address);
  Node* exports = GetSnapshotNode(generator, fixture.instance_exports_address);
  Node* managed_native_module =
      GetSnapshotNode(generator, fixture.module_managed_native_module_address);
  Node* script = GetSnapshotNode(generator, fixture.module_script_address);

  CHECK_NOT_NULL(instance);
  CHECK_NOT_NULL(module);
  CHECK_NOT_NULL(exports);
  CHECK_NOT_NULL(managed_native_module);
  CHECK_NOT_NULL(script);
  CHECK(NodeHasType(instance, Node::Type::kObject));
  CHECK(NodeHasType(module, Node::Type::kObject));
  CHECK(NodeHasType(exports, Node::Type::kObject));
  CHECK(NodeHasType(managed_native_module, Node::Type::kHidden));
  CHECK(NodeHasType(script, Node::Type::kCode) ||
        NodeHasType(script, Node::Type::kObject));

  // V8HeapExplorer::ExtractWasmInstanceObjectReferences emits three named
  // internal WasmInstanceObject edges: "trusted_data", "module_object", and
  // "exports". "module_object" and "exports" are ordinary tagged fields in
  // both sandbox and no-sandbox builds. "trusted_data" is represented as a
  // normal tagged trusted pointer field in no-sandbox, but as an indirect
  // trusted-pointer handle resolved through the sandbox offheap table when
  // V8_ENABLE_SANDBOX is enabled.
  CHECK(HasExactNamedEdge(generator, instance, Edge::Type::kInternal,
                          "module_object", module));
  CHECK_EQ(1, CountExactNamedEdges(generator, instance, Edge::Type::kInternal,
                                   "module_object", module));
  CHECK_EQ(0, CountExactNamedEdges(generator, instance, Edge::Type::kHidden,
                                   "module_object", module));
  CHECK_EQ(0, CountExactNamedEdges(generator, instance, Edge::Type::kWeak,
                                   "module_object", module));

  CHECK(HasExactNamedEdge(generator, instance, Edge::Type::kInternal,
                          "exports", exports));
  CHECK_EQ(1, CountExactNamedEdges(generator, instance, Edge::Type::kInternal,
                                   "exports", exports));
  CHECK_EQ(0, CountExactNamedEdges(generator, instance, Edge::Type::kHidden,
                                   "exports", exports));
  CHECK_EQ(0, CountExactNamedEdges(generator, instance, Edge::Type::kWeak,
                                   "exports", exports));

  // V8HeapExplorer::ExtractWasmModuleObjectReferences emits two named internal
  // WasmModuleObject edges: "managed_native_module" and "script". Both fields
  // are ordinary tagged object references in this snapshot path, so the exact
  // edge expectations are the same in sandbox and no-sandbox builds.
  CHECK(HasExactNamedEdge(generator, module, Edge::Type::kInternal,
                          "managed_native_module", managed_native_module));
  CHECK_EQ(1, CountExactNamedEdges(generator, module, Edge::Type::kInternal,
                                   "managed_native_module",
                                   managed_native_module));
  CHECK_EQ(0, CountExactNamedEdges(generator, module, Edge::Type::kHidden,
                                   "managed_native_module",
                                   managed_native_module));
  CHECK_EQ(0, CountExactNamedEdges(generator, module, Edge::Type::kWeak,
                                   "managed_native_module",
                                   managed_native_module));

  CHECK(HasExactNamedEdge(generator, module, Edge::Type::kInternal, "script",
                          script));
  CHECK_EQ(1, CountExactNamedEdges(generator, module, Edge::Type::kInternal,
                                   "script", script));
  CHECK_EQ(0, CountExactNamedEdges(generator, module, Edge::Type::kHidden,
                                   "script", script));
  CHECK_EQ(0, CountExactNamedEdges(generator, module, Edge::Type::kWeak,
                                   "script", script));

  MarkCoveredInstanceType(WASM_INSTANCE_OBJECT_TYPE);
  MarkCoveredInstanceType(WASM_MODULE_OBJECT_TYPE);
}

TEST(HeapDumpWasmInstanceMissingModuleObjectDoesNotFabricateEdge) {
  WasmJsApiObjectDumpFixture fixture = DumpWasmJsApiObjectFixture();
  RawObjectRecord module_record =
      FindRawObject(fixture.bytes, fixture.module_address);
  RemoveRawObjectRecordForWasmTest(&fixture.bytes, module_record);

  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(result.ok);
  CHECK(!result.diagnostics.has_errors());
  CHECK(HasDiagnostic(result.diagnostics,
                      TranslateDiagnostics::Severity::kWarning,
                      "wasm_internal_reference_target_not_dumped",
                      "object_body"));

  SnapshotGenerator* generator = result.generator.get();
  Node* instance = GetSnapshotNode(generator, fixture.instance_address);
  CHECK_NOT_NULL(instance);

  Node* module = GetSnapshotNode(generator, fixture.module_address);
  if (module != nullptr) {
    CHECK_EQ(0, CountExactNamedEdges(generator, instance,
                                     Edge::Type::kInternal, "module_object",
                                     module));
    CHECK_EQ(0, CountExactNamedEdges(generator, instance, Edge::Type::kHidden,
                                     "module_object", module));
    CHECK_EQ(0, CountExactNamedEdges(generator, instance, Edge::Type::kWeak,
                                     "module_object", module));
  }
}

TEST(HeapDumpWasmScriptExactNamedEdges) {
  WasmJsApiObjectDumpFixture fixture = DumpWasmJsApiObjectFixture();
  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(result.ok);
  CHECK(!result.diagnostics.has_errors());

  SnapshotGenerator* generator = result.generator.get();
  Node* script = GetSnapshotNode(generator, fixture.module_script_address);
  Node* breakpoint_infos =
      GetSnapshotNode(generator, fixture.script_wasm_breakpoint_infos_address);
  Node* managed_native_module = GetSnapshotNode(
      generator, fixture.script_wasm_managed_native_module_address);
  Node* weak_instance_list =
      GetSnapshotNode(generator, fixture.script_wasm_weak_instance_list_address);

  CHECK_NOT_NULL(script);
  CHECK_NOT_NULL(breakpoint_infos);
  CHECK_NOT_NULL(managed_native_module);
  CHECK_NOT_NULL(weak_instance_list);

  // White-box coverage for the Wasm-specific tail of
  // V8HeapExplorer::ExtractScriptReferences. Wasm reuses ordinary Script
  // fields, but native emits these named internal edges after the unconditional
  // Script TagObject calls.
  struct ExpectedWasmScriptEdge {
    const char* name;
    Node* target;
  };
  ExpectedWasmScriptEdge expected_edges[] = {
      {"wasm_breakpoint_infos", breakpoint_infos},
      {"wasm_managed_native_module", managed_native_module},
      {"wasm_weak_instance_list", weak_instance_list},
  };
  for (const ExpectedWasmScriptEdge& expected : expected_edges) {
    CHECK(HasExactNamedEdge(generator, script, Edge::Type::kInternal,
                            expected.name, expected.target));
    CHECK_EQ(1, CountExactNamedEdges(generator, script, Edge::Type::kInternal,
                                     expected.name, expected.target));
    CHECK_EQ(0, CountExactNamedEdges(generator, script, Edge::Type::kHidden,
                                     expected.name, expected.target));
    CHECK_EQ(0, CountExactNamedEdges(generator, script, Edge::Type::kWeak,
                                     expected.name, expected.target));
    CHECK_EQ(0, CountExactNamedEdges(generator, script, Edge::Type::kProperty,
                                     expected.name, expected.target));
    CHECK_EQ(0, CountExactNamedEdges(generator, script, Edge::Type::kShortcut,
                                     expected.name, expected.target));
  }

  // The raw kInfosOffset target is named "wasm_weak_instance_list" for Wasm
  // scripts. Native script->infos() returns the dumped empty_weak_fixed_array()
  // for the tag-only "(infos)" path, so the WeakArrayList must not be retagged
  // or exposed as an "(infos)" named edge.
  CHECK_EQ(0, CountExactNamedEdges(generator, script, Edge::Type::kInternal,
                                   "(infos)", weak_instance_list));
  CHECK_EQ(0, CountExactNamedEdges(generator, script, Edge::Type::kHidden,
                                   "(infos)", weak_instance_list));
  CHECK_EQ(0, CountExactNamedEdges(generator, script, Edge::Type::kWeak,
                                   "(infos)", weak_instance_list));
  CHECK_EQ(0, CountExactNamedEdges(generator, script, Edge::Type::kProperty,
                                   "(infos)", weak_instance_list));
  CHECK_EQ(0, CountExactNamedEdges(generator, script, Edge::Type::kShortcut,
                                   "(infos)", weak_instance_list));

  MarkCoveredInstanceType(SCRIPT_TYPE);
  MarkCoveredInstanceType(FIXED_ARRAY_TYPE);
  MarkCoveredInstanceType(WEAK_ARRAY_LIST_TYPE);
}

TEST(HeapDumpWasmScriptMissingManagedNativeModuleDoesNotFabricateEdge) {
  WasmJsApiObjectDumpFixture fixture = DumpWasmJsApiObjectFixture();
  RawObjectRecord managed_native_module_record =
      FindRawObject(fixture.bytes,
                    fixture.script_wasm_managed_native_module_address);
  RemoveRawObjectRecordForWasmTest(&fixture.bytes,
                                   managed_native_module_record);

  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(result.ok);
  CHECK(!result.diagnostics.has_errors());
  CHECK(HasDiagnostic(result.diagnostics,
                      TranslateDiagnostics::Severity::kWarning,
                      "wasm_internal_reference_target_not_dumped",
                      "object_body"));

  SnapshotGenerator* generator = result.generator.get();
  Node* script = GetSnapshotNode(generator, fixture.module_script_address);
  CHECK_NOT_NULL(script);

  Node* managed_native_module =
      GetSnapshotNode(generator,
                      fixture.script_wasm_managed_native_module_address);
  if (managed_native_module != nullptr) {
    CHECK_EQ(0, CountExactNamedEdges(generator, script,
                                     Edge::Type::kInternal,
                                     "wasm_managed_native_module",
                                     managed_native_module));
    CHECK_EQ(0, CountExactNamedEdges(generator, script, Edge::Type::kHidden,
                                     "wasm_managed_native_module",
                                     managed_native_module));
    CHECK_EQ(0, CountExactNamedEdges(generator, script, Edge::Type::kWeak,
                                     "wasm_managed_native_module",
                                     managed_native_module));
  }
}

TEST(HeapDumpWasmExceptionTagValueObjectsExactPrototypeAndMapEdges) {
  WasmExceptionTagValueDumpFixture fixture =
      DumpWasmExceptionTagValueFixture();
  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(result.ok);
  CHECK(!result.diagnostics.has_errors());

  if (HasWasmAssignedApiObject(fixture.exception_package)) {
    CheckWasmJsApiObjectSnapshot(
        result.generator.get(), fixture.exception_package.object_address,
        fixture.exception_package.prototype_address,
        fixture.exception_package.map_address);
    MarkCoveredInstanceType(WASM_EXCEPTION_PACKAGE_TYPE);
  }

  if (HasWasmAssignedApiObject(fixture.suspending)) {
    CheckWasmJsApiObjectSnapshot(
        result.generator.get(), fixture.suspending.object_address,
        fixture.suspending.prototype_address, fixture.suspending.map_address);
    MarkCoveredInstanceType(WASM_SUSPENDING_OBJECT_TYPE);
  }

  if (HasWasmAssignedApiObject(fixture.tag)) {
    CheckWasmJsApiObjectSnapshot(result.generator.get(),
                                 fixture.tag.object_address,
                                 fixture.tag.prototype_address,
                                 fixture.tag.map_address);
    MarkCoveredInstanceType(WASM_TAG_OBJECT_TYPE);
  }

  CheckWasmObjectSnapshotMapEdge(result.generator.get(),
                                 fixture.value.object_address,
                                 fixture.value.map_address);
  MarkCoveredInstanceType(WASM_VALUE_OBJECT_TYPE);
}

TEST(HeapDumpWasmFunctionImportDataExactMapEdges) {
  WasmFunctionImportDataDumpFixture fixture =
      DumpWasmFunctionImportDataFixture();
  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(result.ok);
  CHECK(!result.diagnostics.has_errors());

  CheckWasmHeapObjectSnapshotMapEdge(
      result.generator.get(), fixture.exported_function_data_address,
      fixture.exported_function_data_map_address);
  CheckWasmFunctionDataBaseHiddenEdges(
      result.generator.get(), fixture.exported_function_data_address,
      fixture.exported_function_data_func_ref_address,
      fixture.exported_internal_function_address);
  MarkCoveredInstanceType(WASM_EXPORTED_FUNCTION_DATA_TYPE);

  CheckWasmHeapObjectSnapshotMapEdge(
      result.generator.get(), fixture.exported_internal_function_address,
      fixture.exported_internal_function_map_address);
  MarkCoveredInstanceType(WASM_INTERNAL_FUNCTION_TYPE);

  CheckWasmHeapObjectSnapshotMapEdge(result.generator.get(),
                                     fixture.wasm_js_function_data_address,
                                     fixture.wasm_js_function_data_map_address);
  CheckWasmFunctionDataBaseHiddenEdges(
      result.generator.get(), fixture.wasm_js_function_data_address,
      fixture.wasm_js_function_data_func_ref_address,
      fixture.wasm_js_function_data_internal_address);
  MarkCoveredInstanceType(WASM_JS_FUNCTION_DATA_TYPE);

  CheckWasmHeapObjectSnapshotMapEdge(result.generator.get(),
                                     fixture.wasm_import_data_address,
                                     fixture.wasm_import_data_map_address);
  MarkCoveredInstanceType(WASM_IMPORT_DATA_TYPE);
}

TEST(HeapDumpWasmHardInternalTypesExactMapEdges) {
  WasmHardInternalTypesDumpFixture fixture =
      DumpWasmHardInternalTypesFixture();
  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(result.ok);
  CHECK(!result.diagnostics.has_errors());

  CheckWasmHeapObjectSnapshotMapEdge(
      result.generator.get(), fixture.asm_wasm_data_address,
      fixture.asm_wasm_data_map_address);
  MarkCoveredInstanceType(ASM_WASM_DATA_TYPE);

  CheckWasmHeapObjectSnapshotMapEdge(
      result.generator.get(), fixture.exception_tag_address,
      fixture.exception_tag_map_address);
  MarkCoveredInstanceType(WASM_EXCEPTION_TAG_TYPE);

  CheckWasmHeapObjectSnapshotMapEdge(
      result.generator.get(), fixture.fast_api_call_data_address,
      fixture.fast_api_call_data_map_address);
  MarkCoveredInstanceType(WASM_FAST_API_CALL_DATA_TYPE);

  CheckWasmHeapObjectSnapshotMapEdge(result.generator.get(),
                                     fixture.func_ref_address,
                                     fixture.func_ref_map_address);
  MarkCoveredInstanceType(WASM_FUNC_REF_TYPE);

  CheckWasmHeapObjectSnapshotMapEdge(result.generator.get(),
                                     fixture.wasm_null_address,
                                     fixture.wasm_null_map_address);
  MarkCoveredInstanceType(WASM_NULL_TYPE);
}

TEST(HeapDumpWasmCapiSuspenderContinuationExactMapEdges) {
  WasmCapiSuspenderContinuationDumpFixture fixture =
      DumpWasmCapiSuspenderContinuationFixture();
  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(result.ok);
  CHECK(!result.diagnostics.has_errors());

  CheckWasmHeapObjectSnapshotMapEdge(
      result.generator.get(), fixture.wasm_capi_function_data_address,
      fixture.wasm_capi_function_data_map_address);
  CheckWasmFunctionDataBaseHiddenEdges(
      result.generator.get(), fixture.wasm_capi_function_data_address,
      fixture.wasm_capi_function_data_func_ref_address,
      fixture.wasm_capi_function_data_internal_address);
  MarkCoveredInstanceType(WASM_CAPI_FUNCTION_DATA_TYPE);

  CheckWasmHeapObjectSnapshotMapEdge(result.generator.get(),
                                     fixture.suspender_address,
                                     fixture.suspender_map_address);
  MarkCoveredInstanceType(WASM_SUSPENDER_OBJECT_TYPE);

  CheckWasmHeapObjectSnapshotMapEdge(result.generator.get(),
                                     fixture.continuation_address,
                                     fixture.continuation_map_address);
  MarkCoveredInstanceType(WASM_CONTINUATION_OBJECT_TYPE);

  CheckWasmHeapObjectSnapshotMapEdge(result.generator.get(),
                                     fixture.resume_data_address,
                                     fixture.resume_data_map_address);
  MarkCoveredInstanceType(WASM_RESUME_DATA_TYPE);
}

TEST(HeapDumpWasmArrayRejectsCorruptTypeInfoFieldGracefully) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  v8::HandleScope scope(CcTest::isolate());
  LocalContext env(CcTest::isolate());
  USE(env);

  WasmArrayTester array_tester;
  DirectHandle<WasmArray> wasm_array = array_tester.CreateArray();
  InvokeAtomicMajorGC(isolate->heap());

  const Address array_map_address = wasm_array->map()->address();
  std::vector<uint8_t> bytes = DumpHeapBytes(isolate);
  RawObjectRecord map_record = FindRawObject(bytes, array_map_address);
  const uint32_t type_info_field_offset =
      map_record.payload_offset +
      static_cast<uint32_t>(
          Map::kConstructorOrBackPointerOrNativeContextOffset);
  WriteTaggedAtForWasmTest(&bytes, type_info_field_offset,
                           static_cast<Tagged_t>(0));

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_errors());
  CHECK(HasDiagnostic(diagnostics,
                      TranslateDiagnostics::Severity::kCorruptInput,
                      "wasm_type_info_not_heap_object", "object_body"));
}

TEST(HeapDumpWasmTrustedInstanceDataProtectedFieldsTranslateToNamedEdges) {
#if defined(V8_ENABLE_SANDBOX)
  return;
#else
  WasmTrustedInstanceDataDumpFixture fixture =
      DumpWasmTrustedInstanceDataProtectedFieldsFixture();
  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(result.ok);
  CHECK(!result.diagnostics.has_errors());

  CheckNamedInternalEdgeToTargetForWasmTest(
      result, fixture.trusted_data_address, "shared_part",
      fixture.shared_part_address);
  CheckNamedInternalEdgeToTargetForWasmTest(
      result, fixture.trusted_data_address, "memory_bases_and_sizes",
      fixture.memory_bases_and_sizes_address);
  CheckNamedInternalEdgeToTargetForWasmTest(
      result, fixture.trusted_data_address, "dispatch_table0",
      fixture.dispatch_table0_address);
  CheckNamedInternalEdgeToTargetForWasmTest(
      result, fixture.trusted_data_address, "dispatch_tables",
      fixture.dispatch_tables_address);
  CheckNamedInternalEdgeToTargetForWasmTest(
      result, fixture.trusted_data_address, "dispatch_table_for_imports",
      fixture.dispatch_table_for_imports_address);
  MarkCoveredInstanceType(WASM_TRUSTED_INSTANCE_DATA_TYPE);
#endif
}

TEST(HeapDumpWasmDispatchTypeInfoMemoryMapExactMapEdges) {
#if defined(V8_ENABLE_SANDBOX)
#if V8_TARGET_OS_LINUX
  WasmMemoryMapDescriptorDumpFixture memory_map_fixture =
      DumpWasmMemoryMapDescriptorFixture();
  WasmTranslateResult memory_map_result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(memory_map_fixture.bytes);
  CHECK(memory_map_result.ok);
  CHECK(!memory_map_result.diagnostics.has_errors());
  CheckWasmHeapObjectSnapshotMapEdge(
      memory_map_result.generator.get(), memory_map_fixture.descriptor_address,
      memory_map_fixture.descriptor_map_address);
  MarkCoveredInstanceType(WASM_MEMORY_MAP_DESCRIPTOR_TYPE);
#endif

  WasmTrustedInstanceDataDumpFixture trusted_fixture =
      DumpWasmTrustedInstanceDataProtectedFieldsFixture();
  WasmTranslateResult trusted_result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(trusted_fixture.bytes);
  CHECK(trusted_result.ok);
  CHECK(!trusted_result.diagnostics.has_errors());

  CheckWasmHeapObjectSnapshotMapEdge(
      trusted_result.generator.get(), trusted_fixture.trusted_data_address,
      trusted_fixture.trusted_data_map_address);
  MarkCoveredInstanceType(WASM_TRUSTED_INSTANCE_DATA_TYPE);

  CheckWasmHeapObjectSnapshotMapEdge(
      trusted_result.generator.get(), trusted_fixture.dispatch_table0_address,
      trusted_fixture.dispatch_table0_map_address);
  MarkCoveredInstanceType(WASM_DISPATCH_TABLE_TYPE);

  CheckWasmHeapObjectSnapshotMapEdge(
      trusted_result.generator.get(),
      trusted_fixture.dispatch_table_for_imports_address,
      trusted_fixture.dispatch_table_for_imports_map_address);
  MarkCoveredInstanceType(WASM_DISPATCH_TABLE_FOR_IMPORTS_TYPE);
  return;
#endif

#if !defined(V8_ENABLE_SANDBOX)
  WasmTrustedInstanceDataDumpFixture trusted_fixture =
      DumpWasmTrustedInstanceDataProtectedFieldsFixture();
  WasmTranslateResult trusted_result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(trusted_fixture.bytes);
  CHECK(trusted_result.ok);
  CHECK(!trusted_result.diagnostics.has_errors());

  CheckWasmHeapObjectSnapshotMapEdge(
      trusted_result.generator.get(), trusted_fixture.dispatch_table0_address,
      trusted_fixture.dispatch_table0_map_address);
  MarkCoveredInstanceType(WASM_DISPATCH_TABLE_TYPE);

  CheckWasmHeapObjectSnapshotMapEdge(
      trusted_result.generator.get(),
      trusted_fixture.dispatch_table_for_imports_address,
      trusted_fixture.dispatch_table_for_imports_map_address);
  MarkCoveredInstanceType(WASM_DISPATCH_TABLE_FOR_IMPORTS_TYPE);

  WasmArrayDumpFixture array_fixture = DumpWasmRefArrayFixture();
  WasmTranslateResult array_result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(array_fixture.bytes);
  CHECK(array_result.ok);
  CHECK(!array_result.diagnostics.has_errors());
  CheckWasmHeapObjectSnapshotMapEdge(
      array_result.generator.get(), array_fixture.type_info_address,
      array_fixture.type_info_map_address);
  MarkCoveredInstanceType(WASM_TYPE_INFO_TYPE);

#if V8_TARGET_OS_LINUX
  WasmMemoryMapDescriptorDumpFixture memory_map_fixture =
      DumpWasmMemoryMapDescriptorFixture();
  WasmTranslateResult memory_map_result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(memory_map_fixture.bytes);
  CHECK(memory_map_result.ok);
  CHECK(!memory_map_result.diagnostics.has_errors());
  CheckWasmHeapObjectSnapshotMapEdge(
      memory_map_result.generator.get(), memory_map_fixture.descriptor_address,
      memory_map_fixture.descriptor_map_address);
  MarkCoveredInstanceType(WASM_MEMORY_MAP_DESCRIPTOR_TYPE);
#endif
#endif
}

TEST(HeapDumpWasmTypeInfoExactMapEdge) {
#if defined(V8_ENABLE_SANDBOX)
  WasmArrayDumpFixture array_fixture = DumpWasmRefArrayFixture();
  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(array_fixture.bytes);
  CHECK(result.ok);
  CHECK(!result.diagnostics.has_errors());
  CheckWasmHeapObjectSnapshotMapEdge(result.generator.get(),
                                     array_fixture.type_info_address,
                                     array_fixture.type_info_map_address);
  MarkCoveredInstanceType(WASM_TYPE_INFO_TYPE);
#endif
}

TEST(HeapDumpWasmArrayRefElementsTranslateToIndexedEdges) {
  WasmArrayDumpFixture fixture = DumpWasmRefArrayFixture();
  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(result.ok);
  CHECK(!result.diagnostics.has_errors());

  Node* array_node = result.generator->GetNode(fixture.array_address);
  CHECK_NOT_NULL(array_node);
  Node* target_node = result.generator->GetNode(fixture.target_address);
  CHECK_NOT_NULL(target_node);

  CHECK(HasEdgeByIndexFromNodeForWasmTest(
      result.generator.get(), array_node, Edge::Type::kElement, 0,
      [target_node](const Node* target) { return target == target_node; }));
  CHECK_EQ(0, CountEdgesFromNodeToTargetForWasmTest(
                  array_node, Edge::Type::kHidden, target_node));
  MarkCoveredInstanceType(WASM_ARRAY_TYPE);
}

TEST(HeapDumpWasmArrayScalarElementsDoNotCreateObjectEdges) {
  WasmArrayDumpFixture fixture = DumpWasmScalarArrayFixture();
  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(result.ok);
  CHECK(!result.diagnostics.has_errors());

  Node* array_node = result.generator->GetNode(fixture.array_address);
  CHECK_NOT_NULL(array_node);
  Node* target_node = result.generator->GetNode(fixture.target_address);
  CHECK_NOT_NULL(target_node);
  USE(target_node);

  CHECK_EQ(0, CountEdgesFromNodeByTypeForWasmTest(array_node,
                                                  Edge::Type::kElement));
  CHECK_EQ(0,
           CountEdgesFromNodeByTypeForWasmTest(array_node, Edge::Type::kHidden));
  MarkCoveredInstanceType(WASM_ARRAY_TYPE);
}

TEST(HeapDumpWasmArrayRejectsWrongTypeInfoObjectGracefully) {
  WasmArrayDumpFixture fixture = DumpWasmRefArrayFixture();
  MutateWasmArrayTypeInfoToWrongInstanceTypeForWasmTest(&fixture.bytes,
                                                        fixture);

  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(!result.ok);
  CHECK(result.diagnostics.has_errors());
  CHECK(HasDiagnostic(result.diagnostics,
                      TranslateDiagnostics::Severity::kCorruptInput,
                      "wasm_type_info_wrong_instance_type", "object_body"));
  CheckNoWasmArrayElementEdgeToTarget(result, fixture);
}

TEST(HeapDumpWasmArrayRejectsInvalidElementTypeGracefully) {
  WasmArrayDumpFixture fixture = DumpWasmRefArrayFixture();
  MutateWasmArrayElementTypeToInvalidForWasmTest(&fixture.bytes, fixture);

  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(!result.ok);
  CHECK(result.diagnostics.has_errors());
  CHECK(HasDiagnostic(result.diagnostics,
                      TranslateDiagnostics::Severity::kCorruptInput,
                      "wasm_type_info_canonical_element_type_invalid",
                      "object_body"));
  CheckNoWasmArrayElementEdgeToTarget(result, fixture);
}

TEST(HeapDumpWasmStructExactRefFieldTranslatesToEdge) {
  WasmStructDumpFixture fixture = DumpWasmStructFixture();
  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(result.ok);
  CHECK(!result.diagnostics.has_errors());

  Node* struct_node = result.generator->GetNode(fixture.struct_address);
  CHECK_NOT_NULL(struct_node);
  Node* target_node = result.generator->GetNode(fixture.target_address);
  CHECK_NOT_NULL(target_node);

  CHECK(HasEdgeByNameFromNode(
      result.generator.get(), struct_node, Edge::Type::kProperty,
      "wasm_struct_field_1",
      [target_node](const Node* target) { return target == target_node; }));
  CHECK_EQ(0, CountWasmStructFieldEdges(result, fixture,
                                        "wasm_struct_field_0"));
  MarkCoveredInstanceType(WASM_STRUCT_TYPE);
}

TEST(HeapDumpWasmStructDescriptorRawFieldZeroTranslatesToEdge) {
  WasmStructDescriptorDumpFixture fixture = DumpWasmStructDescriptorFixture();
  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(result.ok);
  CHECK(!result.diagnostics.has_errors());

  Node* descriptor_node = result.generator->GetNode(fixture.descriptor_address);
  CHECK_NOT_NULL(descriptor_node);
  Node* rtt_node = result.generator->GetNode(fixture.described_rtt_address);
  CHECK_NOT_NULL(rtt_node);

  CHECK(HasEdgeByNameFromNode(
      result.generator.get(), descriptor_node, Edge::Type::kInternal,
      "described_rtt", [rtt_node](const Node* target) {
        return target == rtt_node;
      }));
  MarkCoveredInstanceType(WASM_STRUCT_TYPE);
}

TEST(HeapDumpWasmStringViewIterExactMapAndStringEdge) {
  WasmStringViewIterDumpFixture fixture = DumpWasmStringViewIterFixture();
  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(result.ok);
  CHECK(!result.diagnostics.has_errors());

  Node* iterator_node = result.generator->GetNode(fixture.iterator_address);
  CHECK_NOT_NULL(iterator_node);
  Node* iterator_map_node =
      result.generator->GetNode(fixture.iterator_map_address);
  CHECK_NOT_NULL(iterator_map_node);
  Node* holder_node = result.generator->GetNode(fixture.holder_address);
  CHECK_NOT_NULL(holder_node);
  Node* string_node = result.generator->GetNode(fixture.string_address);
  CHECK_NOT_NULL(string_node);
  USE(string_node);

  CHECK(NodeHasType(iterator_map_node, Node::Type::kObjectShape));
  CHECK(HasExactNamedEdge(result.generator.get(), iterator_node,
                          Edge::Type::kInternal, "map", iterator_map_node));
  CHECK_EQ(1, CountExactNamedEdges(result.generator.get(), iterator_node,
                                   Edge::Type::kInternal, "map",
                                   iterator_map_node));
  CHECK_EQ(1, CountEdgesFromNodeToTargetForWasmTest(
                  iterator_node, Edge::Type::kInternal, iterator_map_node));
  CHECK_EQ(0, CountEdgesFromNodeToTargetForWasmTest(
                  iterator_node, Edge::Type::kWeak, iterator_map_node));
  CHECK_EQ(0, CountEdgesFromNodeToTargetForWasmTest(
                  iterator_node, Edge::Type::kProperty, iterator_map_node));
  CHECK_EQ(0, CountEdgesFromNodeToTargetForWasmTest(
                  iterator_node, Edge::Type::kHidden, iterator_map_node));
  CHECK(HasEdgeByNameFromNode(
      result.generator.get(), holder_node, Edge::Type::kProperty,
      "wasm_struct_field_0", [iterator_node](const Node* target) {
        return target == iterator_node;
      }));

  MarkCoveredInstanceType(WASM_STRING_VIEW_ITER_TYPE);
}

TEST(HeapDumpWasmStructMissingCanonicalTablePreservesNodeAndMap) {
  WasmStructDumpFixture fixture = DumpWasmStructFixture();
  RemoveOffheapSectionForWasmTest(&fixture.bytes);

  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(result.ok);
  CHECK(!result.diagnostics.has_errors());
  CHECK(HasDiagnostic(result.diagnostics,
                      TranslateDiagnostics::Severity::kUnsupported,
                      "wasm_struct_canonical_type_table_missing",
                      "object_body"));
  CheckWasmStructMapEdgePreserved(result, fixture);
  CheckNoWasmStructRefFieldEdge(result, fixture);
}

TEST(HeapDumpWasmStructMissingCanonicalRecordPreservesNodeAndMap) {
  WasmStructDumpFixture fixture = DumpWasmStructFixture();
  ReplaceWasmCanonicalTableWithEmptyForWasmTest(&fixture.bytes);

  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(result.ok);
  CHECK(!result.diagnostics.has_errors());
  CHECK(HasDiagnostic(result.diagnostics,
                      TranslateDiagnostics::Severity::kUnsupported,
                      "wasm_struct_type_layout_missing", "object_body"));
  CheckWasmStructMapEdgePreserved(result, fixture);
  CheckNoWasmStructRefFieldEdge(result, fixture);
}

TEST(HeapDumpWasmStructRejectsFieldOffsetOutOfBoundsGracefully) {
  WasmStructDumpFixture fixture = DumpWasmStructFixture();
  MutateWasmStructRefFieldOffsetOutOfBoundsForWasmTest(&fixture.bytes,
                                                       fixture);

  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(!result.ok);
  CHECK(result.diagnostics.has_errors());
  CHECK(HasDiagnostic(result.diagnostics,
                      TranslateDiagnostics::Severity::kCorruptInput,
                      "wasm_struct_field_offset_out_of_range",
                      "object_body"));
  CheckWasmStructMapEdgePreserved(result, fixture);
  CheckNoWasmStructRefFieldEdge(result, fixture);
}

TEST(HeapDumpWasmStructRejectsDuplicateCanonicalTypeIndexGracefully) {
  WasmStructDumpFixture fixture = DumpWasmStructFixture();
  MutateWasmCanonicalTableDuplicateTypeIndexForWasmTest(&fixture.bytes,
                                                       fixture);

  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(!result.ok);
  CHECK(result.diagnostics.has_errors());
  CHECK(HasDiagnostic(result.diagnostics,
                      TranslateDiagnostics::Severity::kCorruptInput,
                      "wasm_canonical_struct_duplicate_type_index",
                      "offheap_table"));
}

TEST(HeapDumpWasmStructRejectsTruncatedCanonicalFieldsGracefully) {
  WasmStructDumpFixture fixture = DumpWasmStructFixture();
  MutateWasmCanonicalTableTruncatedFieldsForWasmTest(&fixture.bytes, fixture);

  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(!result.ok);
  CHECK(result.diagnostics.has_errors());
  CHECK(HasDiagnostic(result.diagnostics,
                      TranslateDiagnostics::Severity::kCorruptInput,
                      "wasm_canonical_struct_fields_truncated",
                      "offheap_table"));
}

TEST(HeapDumpWasmStructRejectsCanonicalFieldsOverlapGracefully) {
  WasmStructDumpFixture fixture = DumpWasmStructFixture();
  MutateWasmCanonicalTableFieldsOverlapRecordsForWasmTest(&fixture.bytes,
                                                          fixture);

  WasmTranslateResult result =
      TranslateHeapBytesWithDiagnosticsForWasmTest(fixture.bytes);
  CHECK(!result.ok);
  CHECK(result.diagnostics.has_errors());
  CHECK(HasDiagnostic(result.diagnostics,
                      TranslateDiagnostics::Severity::kCorruptInput,
                      "wasm_canonical_struct_fields_overlap_records",
                      "offheap_table"));
}

#else

TEST(HeapDumpVisitorIdsWasmCoverage) {}
TEST(HeapDumpVisitorIdsWasmCoverageReport) {}
TEST(HeapDumpOffheapResolverParsesWasmCanonicalStructTable) {}
TEST(HeapDumpOffheapResolverRejectsMalformedWasmCanonicalStructTable) {}
TEST(HeapDumpOffheapResolverAcceptsMissingSection) {}
TEST(HeapDumpWasmCanonicalTypeTableSectionIsDumped) {}
TEST(HeapDumpWasmStructRefFieldTranslatesToEdge) {}
TEST(HeapDumpWasmJsApiObjectsExactPrototypeAndMapEdges) {}
TEST(HeapDumpWasmInstanceAndModuleObjectExactNamedEdges) {}
TEST(HeapDumpWasmInstanceMissingModuleObjectDoesNotFabricateEdge) {}
TEST(HeapDumpWasmScriptExactNamedEdges) {}
TEST(HeapDumpWasmScriptMissingManagedNativeModuleDoesNotFabricateEdge) {}
TEST(HeapDumpWasmExceptionTagValueObjectsExactPrototypeAndMapEdges) {}
TEST(HeapDumpWasmFunctionImportDataExactMapEdges) {}
TEST(HeapDumpWasmHardInternalTypesExactMapEdges) {}
TEST(HeapDumpWasmCapiSuspenderContinuationExactMapEdges) {}
TEST(HeapDumpWasmArrayRejectsCorruptTypeInfoFieldGracefully) {}
TEST(HeapDumpWasmTrustedInstanceDataProtectedFieldsTranslateToNamedEdges) {}
TEST(HeapDumpWasmDispatchTypeInfoMemoryMapExactMapEdges) {}
TEST(HeapDumpWasmTypeInfoExactMapEdge) {}
TEST(HeapDumpWasmArrayRefElementsTranslateToIndexedEdges) {}
TEST(HeapDumpWasmArrayScalarElementsDoNotCreateObjectEdges) {}
TEST(HeapDumpWasmArrayRejectsWrongTypeInfoObjectGracefully) {}
TEST(HeapDumpWasmArrayRejectsInvalidElementTypeGracefully) {}
TEST(HeapDumpWasmStructExactRefFieldTranslatesToEdge) {}
TEST(HeapDumpWasmStructDescriptorRawFieldZeroTranslatesToEdge) {}
TEST(HeapDumpWasmStringViewIterExactMapAndStringEdge) {}
TEST(HeapDumpWasmStructMissingCanonicalTablePreservesNodeAndMap) {}
TEST(HeapDumpWasmStructMissingCanonicalRecordPreservesNodeAndMap) {}
TEST(HeapDumpWasmStructRejectsFieldOffsetOutOfBoundsGracefully) {}
TEST(HeapDumpWasmStructRejectsDuplicateCanonicalTypeIndexGracefully) {}
TEST(HeapDumpWasmStructRejectsTruncatedCanonicalFieldsGracefully) {}
TEST(HeapDumpWasmStructRejectsCanonicalFieldsOverlapGracefully) {}

#endif

}  // namespace heap
}  // namespace internal
}  // namespace v8
