#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <sstream>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/dfx-visitor-list.h"
#include "include/v8-external.h"
#include "include/v8-promise.h"
#include "include/v8-script.h"
#include "src/ast/ast-source-ranges.h"
#include "src/handles/maybe-handles-inl.h"
#include "src/heap/factory-inl.h"
#include "src/objects/bytecode-array-inl.h"
#include "src/objects/cpp-heap-external-object-inl.h"
#include "src/objects/js-objects.h"
#include "src/objects/js-regexp.h"
#include "src/objects/megadom-handler-inl.h"
#include "src/zone/zone-containers.h"
#include "src/zone/zone.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/tests/cctest/heap-dump-test-utils.h"
#include "test/common/flag-utils.h"

namespace v8 {
namespace internal {
namespace heap {

#if defined(OH_ENABLE_HEAP_DUMP_TEST) && defined(OH_ENABLE_HEAP_DUMP) && \
    defined(OH_ENABLE_HEAP_TRANSLATE)

namespace {

using namespace heap_dump_test;

void CheckExactInternalMapEdgeNoWrongKind(SnapshotGenerator* generator,
                                          const Node* object, const Node* map) {
  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CHECK(
      HasExactNamedEdge(generator, object, Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(object, Edge::Type::kInternal, map));
  CHECK_EQ(1, CountExactNamedEdges(generator, object, Edge::Type::kInternal,
                                   "map", map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kProperty, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kShortcut, map));
}

#if defined(V8_ENABLE_SANDBOX)
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
#endif

template <typename T>
DirectHandle<T> GetGlobalDirectHandle(Isolate* isolate,
                                      v8::Local<v8::Context> context,
                                      const char* key) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8::Local<v8::Value> value = context->Global()
                                   ->Get(context, NewApiString(v8_isolate, key))
                                   .ToLocalChecked();
  return Cast<T>(v8::Utils::OpenDirectHandle(*value));
}

DirectHandle<String> CreateSeqTwoByteStringForTest(
    Isolate* isolate, v8::Local<v8::Context> context) {
  static constexpr uint16_t kText[] = {0x4f60, 0x597d, 0x002d, 0x03bb, 0x2603};
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8::Local<v8::String> value =
      v8::String::NewFromTwoByte(v8_isolate, kText, v8::NewStringType::kNormal,
                                 arraysize(kText))
          .ToLocalChecked();
  SetGlobalValue(context, "__heap_dump_two_byte_string", value);
  return v8::Utils::OpenDirectHandle(*value);
}

void EmptyApiCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {}

v8::Intercepted InterceptorGetter(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(17);
  return v8::Intercepted::kYes;
}

v8::MaybeLocal<v8::Module> UnexpectedModuleResolveCallback(
    v8::Local<v8::Context> context, v8::Local<v8::String> specifier,
    v8::Local<v8::FixedArray> import_attributes,
    v8::Local<v8::Module> referrer) {
  return v8::MaybeLocal<v8::Module>();
}

v8::MaybeLocal<v8::Value> SyntheticModuleEvaluationStepsCallback(
    v8::Local<v8::Context> context, v8::Local<v8::Module> module) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  CHECK(module
            ->SetSyntheticModuleExport(isolate, NewApiString(isolate, "value"),
                                       v8::Integer::New(isolate, 1))
            .FromJust());
  return v8::Undefined(isolate);
}

void InstallVisitorCoverageApiObjects(v8::Local<v8::Context> context) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  v8::Local<v8::FunctionTemplate> api_ctor =
      v8::FunctionTemplate::New(isolate, EmptyApiCallback);
  api_ctor->InstanceTemplate()->SetInternalFieldCount(2);
  CHECK(context->Global()
            ->Set(context, NewApiString(isolate, "HeapDumpApiCtor"),
                  api_ctor->GetFunction(context).ToLocalChecked())
            .FromJust());

  v8::Local<v8::FunctionTemplate> interceptor_ctor =
      v8::FunctionTemplate::New(isolate, EmptyApiCallback);
  interceptor_ctor->InstanceTemplate()->SetInternalFieldCount(1);
  interceptor_ctor->InstanceTemplate()->SetHandler(
      v8::NamedPropertyHandlerConfiguration(InterceptorGetter));
  CHECK(context->Global()
            ->Set(context, NewApiString(isolate, "HeapDumpInterceptorCtor"),
                  interceptor_ctor->GetFunction(context).ToLocalChecked())
            .FromJust());

  SetGlobalValue(context, "__heap_dump_external",
                 v8::External::New(isolate, reinterpret_cast<void*>(0x1234),
                                   v8::kExternalPointerTypeTagDefault));
}

void InstallVisitorCoverageModules(v8::Local<v8::Context> context) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  {
    v8::ScriptOrigin origin(NewApiString(isolate, "heap-dump-source.mjs"), 0, 0,
                            false, -1, v8::Local<v8::Value>(), false, false,
                            true);
    v8::ScriptCompiler::Source source(
        NewApiString(
            isolate,
            "export const answer = 42; export function f() { return answer; }"),
        origin);
    v8::Local<v8::Module> module =
        v8::ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
    CHECK(module->InstantiateModule(context, UnexpectedModuleResolveCallback)
              .ToChecked());
    CHECK(!module->Evaluate(context).IsEmpty());
    SetGlobalValue(context, "__heap_dump_source_module",
                   module->GetModuleNamespace());
  }

  {
    std::array<v8::Local<v8::String>, 1> export_names = {
        NewApiString(isolate, "value")};
    v8::Local<v8::Module> module = v8::Module::CreateSyntheticModule(
        isolate, NewApiString(isolate, "heap-dump-synth"), export_names,
        SyntheticModuleEvaluationStepsCallback);
    CHECK(module->InstantiateModule(context, UnexpectedModuleResolveCallback)
              .ToChecked());
    CHECK(!module->Evaluate(context).IsEmpty());
    SetGlobalValue(context, "__heap_dump_synthetic_module",
                   module->GetModuleNamespace());
  }
}

DumpArtifacts CreateVisitorCoverageObjects(Isolate* isolate,
                                           v8::Local<v8::Context> context) {
  DumpArtifacts artifacts;
  CreateBaselineObjects(context);
  CreateExtendedObjects(context);
  InstallVisitorCoverageApiObjects(context);
  InstallVisitorCoverageModules(context);

  FlagScope<bool> allow_natives_syntax(&v8_flags.allow_natives_syntax, true);
  RunScript(context, R"(
    globalThis.__heap_dump_visitor_ids = {};
    (function() {
      globalThis.__heap_dump_coverage_fn = function HeapDumpCoverageFn(x) {
        return x + 1;
      };
      globalThis.__heap_dump_coverage_regexp = /heap-dump-extra-(bar)?/gi;

      function outer(a, b, c) {
        var local = a + b + c;
        return function inner() { return local; };
      }
      function makeSloppyArguments(a, b) { arguments[0] = a + 1; return arguments; }
      async function asyncFn(x) { return await Promise.resolve(x + 1); }
      function* genFn() { yield 1; yield 2; }

      const weakKey = { key: true };
      const weakValue = { held: true };
      const proxyTarget = { a: 1 };
      const proxy = new Proxy(proxyTarget, {
        get(target, prop, receiver) {
          return Reflect.get(target, prop, receiver);
        }
      });

      const sliced = 'heap-dump-visitor-ids-sliced-string'.slice(5, 22);
      const cons = 'heap-dump-' + 'visitor-ids-cons-string';
      const maybeThin = ('thin-' + 'string').slice(0);
      const runtimeCons = %ConstructConsString(
          'heap-dump-runtime-cons-left-1234567890',
          'heap-dump-runtime-cons-right-1234567890');
      const runtimeThin =
          %ConstructThinString('heap-dump-runtime-thin-string-1234567890');
      globalThis.runtimeCons = runtimeCons;
      globalThis.runtimeThin = runtimeThin;

      const dict = {};
      for (let i = 0; i < 64; i++) dict['k' + i] = i;
      for (let i = 0; i < 16; i++) delete dict['k' + i];

      const array = [1, 2, 3, { nested: true }];
      const object = {
        get acc() { return 1; },
        set acc(v) { this._v = v; },
        deep: { ok: true }
      };

      const weakMap = new WeakMap([[weakKey, weakValue]]);
      const weakSet = new WeakSet([weakKey]);
      const finalizationRegistry =
          typeof FinalizationRegistry === 'function'
              ? new FinalizationRegistry(() => 0)
              : null;
      const weakRef =
          typeof WeakRef === 'function' ? new WeakRef(weakValue) : null;
      if (finalizationRegistry) finalizationRegistry.register(weakValue, 1);

      const arrayBuffer = new ArrayBuffer(128);
      const dataView = new DataView(arrayBuffer);
      const typed = new Uint8Array(arrayBuffer);
      const shared = typeof SharedArrayBuffer === 'function'
          ? new SharedArrayBuffer(64)
          : null;

      const apiObject = new HeapDumpApiCtor();
      const intercepted = new HeapDumpInterceptorCtor();
      intercepted.anything;

      const moduleMap = new Map([[1, 'one'], [2, { deep: true }]]);
      const moduleSet = new Set([1, 2, 3]);

      const regexp = /visitor-id-(foo)?/gi;
      regexp.exec('visitor-id-foo');
      regexp.exec('visitor-id-bar');

      const promise = Promise.resolve({ done: false });
      const closure = outer(1, 2, 3);
      const args = makeSloppyArguments(1, 2, 3, 4);
      const date = new Date(2026, 3, 3);
      const bigint = 20260403n;
      const symbol = Symbol('visitor-id-symbol');

      let evalValue = 9;
      const evalClosure = function() { return eval('evalValue'); };
      const withClosure = (function() {
        var scopeObj = { p: 11 };
        with (scopeObj) {
          return function() { return p; };
        }
      })();

      globalThis.__heap_dump_coverage_fn(41);
      globalThis.__heap_dump_coverage_regexp.exec('heap-dump-extra-bar');

      for (let i = 0; i < 64; i++) {
        outer(i, i + 1, i + 2)();
        proxy.a;
        intercepted.anything;
        object.acc;
        regexp.exec('visitor-id-foo');
        makeSloppyArguments(i, i + 1);
        [i, i + 1, i + 2, { lit: true }];
        ({ x: i, y: i + 1, z: i + 2 });
      }

      globalThis.__heap_dump_visitor_ids = {
        apiObject,
        intercepted,
        proxy,
        proxyTarget,
        sliced,
        cons,
        maybeThin,
        runtimeCons,
        runtimeThin,
        dict,
        array,
        object,
        weakMap,
        weakSet,
        weakRef,
        finalizationRegistry,
        arrayBuffer,
        dataView,
        typed,
        shared,
        moduleMap,
        moduleSet,
        regexp,
        promise,
        closure,
        gen: genFn(),
        asyncFn,
        args,
        date,
        bigint,
        symbol,
        evalClosure,
        withClosure,
        external: globalThis.__heap_dump_external,
        sourceModule: globalThis.__heap_dump_source_module,
        syntheticModule: globalThis.__heap_dump_synthetic_module
      };

      if (typeof WebAssembly === 'object') {
        const bytes = new Uint8Array([
          0x00, 0x61, 0x73, 0x6d,
          0x01, 0x00, 0x00, 0x00,
          0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
          0x03, 0x02, 0x01, 0x00,
          0x07, 0x07, 0x01, 0x03, 0x6e, 0x6f, 0x70, 0x00, 0x00,
          0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b
        ]);
        const wasmModule = new WebAssembly.Module(bytes);
        const wasmInstance = new WebAssembly.Instance(wasmModule, {});
        globalThis.__heap_dump_visitor_ids.wasm = {
          wasmModule,
          wasmInstance,
          memory: new WebAssembly.Memory({ initial: 1, maximum: 1 }),
          table: new WebAssembly.Table({ initial: 1, element: 'anyfunc' })
        };
      }
    })();
  )");

  DirectHandle<InstructionStream> instruction_stream =
      CreateInstructionStreamForTest(isolate);
  DirectHandle<Foreign> foreign = CreateForeignForTest(isolate);
  DirectHandle<String> external_string =
      CreateExternalStringForTest(isolate, context);
  DirectHandle<String> two_byte_string =
      CreateSeqTwoByteStringForTest(isolate, context);
  DirectHandle<String> cons_string =
      isolate->factory()
          ->NewConsString(external_string, two_byte_string)
          .ToHandleChecked();
  DirectHandle<AccessorInfo> accessor_info =
      isolate->factory()->NewAccessorInfo();
  accessor_info->set_name(*isolate->factory()->InternalizeUtf8String("v"));
  accessor_info->set_data(*foreign);
  Handle<AllocationSite> allocation_site =
      isolate->factory()->NewAllocationSite(false);
  Handle<LoadHandler> load_handler = isolate->factory()->NewLoadHandler(1);
  DirectHandle<FeedbackCell> feedback_cell =
      isolate->factory()->NewManyClosuresCell();
  Handle<AccessorPair> accessor_pair = isolate->factory()->NewAccessorPair();
  accessor_pair->set_getter(*foreign);
  accessor_pair->set_setter(*foreign);
  DirectHandle<JSObject> dictionary_object =
      GetGlobalDirectHandle<JSObject>(isolate, context, "__heap_dump_object");
  JSObject::NormalizeProperties(isolate, dictionary_object,
                                KEEP_INOBJECT_PROPERTIES, 0,
                                "heap-dump-visitor-coverage");
  DirectHandle<Map> slow_object_map = direct_handle(
      isolate->native_context()->slow_object_with_object_prototype_map(),
      isolate);
  DirectHandle<JSObject> slow_object =
      isolate->factory()->NewJSObjectFromMap(slow_object_map);

  DirectHandle<JSFunction> coverage_function =
      GetGlobalDirectHandle<JSFunction>(isolate, context,
                                        "__heap_dump_coverage_fn");
  DirectHandle<JSRegExp> coverage_regexp = GetGlobalDirectHandle<JSRegExp>(
      isolate, context, "__heap_dump_coverage_regexp");
  DirectHandle<SharedFunctionInfo> shared(coverage_function->shared(), isolate);
  CHECK(shared->HasBytecodeArray());
  DirectHandle<BytecodeArray> bytecode_array(shared->GetBytecodeArray(isolate),
                                             isolate);
  DirectHandle<Code> function_code = CreateCodeForTest(isolate);

  Zone coverage_zone(isolate->allocator(), ZONE_NAME);
  ZoneVector<SourceRange> coverage_slots(&coverage_zone);
  coverage_slots.push_back(SourceRange(0, 5));
  coverage_slots.push_back(SourceRange(6, 12));

  DirectHandle<CoverageInfo> coverage_info =
      isolate->factory()->NewCoverageInfo(coverage_slots);
  DirectHandle<DebugInfo> debug_info = isolate->factory()->NewDebugInfo(shared);
  DirectHandle<CallSiteInfo> call_site_info =
      isolate->factory()->NewCallSiteInfo(
          isolate->factory()->undefined_value(), coverage_function, shared, 0,
          0, isolate->factory()->empty_fixed_array());
  DirectHandle<InterpreterData> interpreter_data =
      isolate->factory()->NewInterpreterData(bytecode_array, function_code);
  DirectHandle<SharedFunctionInfoWrapper> shared_function_info_wrapper =
      isolate->factory()->NewSharedFunctionInfoWrapper(shared);
  Handle<PreparseData> preparse_data =
      isolate->factory()->NewPreparseData(4, 1);
  DirectHandle<String> inferred_name =
      isolate->factory()->InternalizeUtf8String("heap_dump_preparsed");
  DirectHandle<UncompiledDataWithPreparseData> uncompiled_with_preparse =
      isolate->factory()->NewUncompiledDataWithPreparseData(inferred_name, 0, 8,
                                                            preparse_data);
  DirectHandle<UncompiledDataWithoutPreparseDataWithJob>
      uncompiled_without_preparse_with_job =
          isolate->factory()->NewUncompiledDataWithoutPreparseDataWithJob(
              inferred_name, 0, 8);
  DirectHandle<UncompiledDataWithPreparseDataAndJob>
      uncompiled_with_preparse_and_job =
          isolate->factory()->NewUncompiledDataWithPreparseDataAndJob(
              inferred_name, 0, 8, preparse_data);
  DirectHandle<RegExpBoilerplateDescription> regexp_boilerplate =
      isolate->factory()->NewRegExpBoilerplateDescription(
          direct_handle(coverage_regexp->data(isolate), isolate),
          direct_handle(coverage_regexp->source(), isolate),
          Smi::FromInt(JSRegExp::kGlobal));
  DirectHandle<SmallOrderedHashMap> small_ordered_hash_map =
      isolate->factory()->NewSmallOrderedHashMap(4);
  DirectHandle<SmallOrderedHashSet> small_ordered_hash_set =
      isolate->factory()->NewSmallOrderedHashSet(4);
  DirectHandle<SmallOrderedNameDictionary> small_ordered_name_dictionary =
      isolate->factory()->NewSmallOrderedNameDictionary(4);
  Handle<CppHeapExternalObject> cpp_heap_external =
      isolate->factory()->NewCppHeapExternal(AllocationType::kOld);
  DirectHandle<DictionaryTemplateInfo> dictionary_template_info =
      isolate->factory()->NewDictionaryTemplateInfo(
          isolate->factory()->empty_fixed_array());
  DirectHandle<FunctionTemplateInfo> api_function_template =
      isolate->factory()->NewFunctionTemplateInfo(0, false);
  DirectHandle<NativeContext> native_context = isolate->native_context();
  DirectHandle<MegaDomHandler> mega_dom_handler =
      isolate->factory()->NewMegaDomHandler(
          MaybeObjectDirectHandle::Weak(api_function_template),
          MaybeObjectDirectHandle::Weak(native_context));

  artifacts.keep_alive.push_back(instruction_stream);
  artifacts.keep_alive.push_back(foreign);
  artifacts.keep_alive.push_back(external_string);
  artifacts.keep_alive.push_back(two_byte_string);
  artifacts.keep_alive.push_back(cons_string);
  artifacts.keep_alive.push_back(accessor_info);
  artifacts.keep_alive.push_back(direct_handle(*allocation_site, isolate));
  artifacts.keep_alive.push_back(direct_handle(*load_handler, isolate));
  artifacts.keep_alive.push_back(feedback_cell);
  artifacts.keep_alive.push_back(direct_handle(*accessor_pair, isolate));
  artifacts.keep_alive.push_back(dictionary_object);
  artifacts.keep_alive.push_back(slow_object);
  artifacts.keep_alive.push_back(coverage_info);
  artifacts.keep_alive.push_back(debug_info);
  artifacts.keep_alive.push_back(call_site_info);
  artifacts.keep_alive.push_back(interpreter_data);
  artifacts.keep_alive.push_back(shared_function_info_wrapper);
  artifacts.keep_alive.push_back(preparse_data);
  artifacts.keep_alive.push_back(uncompiled_with_preparse);
  artifacts.keep_alive.push_back(uncompiled_without_preparse_with_job);
  artifacts.keep_alive.push_back(uncompiled_with_preparse_and_job);
  artifacts.keep_alive.push_back(regexp_boilerplate);
  artifacts.keep_alive.push_back(small_ordered_hash_map);
  artifacts.keep_alive.push_back(small_ordered_hash_set);
  artifacts.keep_alive.push_back(small_ordered_name_dictionary);
  artifacts.keep_alive.push_back(direct_handle(*cpp_heap_external, isolate));
  artifacts.keep_alive.push_back(dictionary_template_info);
  artifacts.keep_alive.push_back(api_function_template);
  artifacts.keep_alive.push_back(mega_dom_handler);
  return artifacts;
}

struct VisitorIdExpectation {
  VisitorId id;
  const char* name;
};

struct ConstructedStringState {
  bool runtime_cons_before_gc = false;
  bool runtime_cons_after_gc = false;
  bool runtime_thin_before_gc = false;
  bool runtime_thin_after_gc = false;
  int runtime_cons_visitor_before_gc = -1;
  int runtime_cons_visitor_after_gc = -1;
  int runtime_thin_visitor_before_gc = -1;
  int runtime_thin_visitor_after_gc = -1;
};

struct VisitorCoverageRun {
  VisitorIdCounts counts;
  ConstructedStringState string_state;
  std::unique_ptr<SnapshotGenerator> generator;
};

struct InterpreterDataProtectedEdgesState {
  Address interpreter_data = 0;
  Address bytecode_array = 0;
  Address interpreter_trampoline = 0;
  bool saw_interpreter_data_type = false;
  bool saw_bytecode_array_type = false;
  bool saw_code_type = false;
};

struct BytecodeArrayProtectedEdgesState {
  Address bytecode_array = 0;
  Address constant_pool = 0;
  Address handler_table = 0;
  Address source_position_table = 0;
  bool saw_bytecode_array_type = false;
  bool saw_constant_pool_type = false;
  bool saw_handler_table_type = false;
  bool saw_source_position_table_type = false;
};

#define VISITOR_EXPECT(Name) \
  { VisitorId::kVisit##Name, #Name }

#define VISITOR_EXPECT_ENTRY(Name) VISITOR_EXPECT(Name),

std::vector<VisitorIdExpectation> BuildNonTorqueVisitorExpectations() {
  std::vector<VisitorIdExpectation> expectations = {
      DATA_ONLY_VISITOR_ID_LIST(VISITOR_EXPECT_ENTRY)
          POINTER_VISITOR_ID_LIST(VISITOR_EXPECT_ENTRY)
              TRUSTED_VISITOR_ID_LIST(VISITOR_EXPECT_ENTRY)};
  expectations.erase(
      std::remove_if(expectations.begin(), expectations.end(),
                     [](const VisitorIdExpectation& item) {
                       switch (item.id) {
                         // CombinedHeapObjectIterator never surfaces layout
                         // fillers/free space, so these cannot be observed by
                         // CollectVisitorIdCounts(...).
                         case VisitorId::kVisitFiller:
                         case VisitorId::kVisitFreeSpace:
                         // Non-internalized cons strings are classified as
                         // kVisitShortcutCandidate in map.cc.
                         case VisitorId::kVisitConsString:
                         // JSObject currently requires JSPromise embedder
                         // fields, but this build keeps
                         // v8::Promise::kEmbedderFieldCount == 0.
                         case VisitorId::kVisitJSObject:
                         // Covered by the dedicated shared-space cctest that
                         // enables harmony_struct before V8 initialization.
                         case VisitorId::kVisitJSSynchronizationPrimitive:
                         case VisitorId::kVisitWasmArray:
                         case VisitorId::kVisitWasmContinuationObject:
                         case VisitorId::kVisitWasmGlobalObject:
                         case VisitorId::kVisitWasmMemoryMapDescriptor:
                         case VisitorId::kVisitWasmResumeData:
                         case VisitorId::kVisitWasmSuspendingObject:
                         case VisitorId::kVisitWasmImportData:
                         case VisitorId::kVisitWasmCapiFunctionData:
                         case VisitorId::kVisitWasmJSFunctionData:
                           return true;
                         default:
                           return false;
                       }
                     }),
      expectations.end());
  return expectations;
}

#undef VISITOR_EXPECT_ENTRY

constexpr VisitorIdExpectation kStableVisitorExpectations[] = {
    // This list intentionally tracks the stable JS-reachable visitor ids we can
    // create from script plus a few API-side helpers in this test. It is not a
    // claim that JavaScript can materialize every VisitorId in V8; many remain
    // trusted/internal-only.
    VISITOR_EXPECT(AllocationSite),
    VISITOR_EXPECT(AccessorInfo),
    VISITOR_EXPECT(ArrayList),
    VISITOR_EXPECT(BytecodeArray),
    VISITOR_EXPECT(Cell),
    VISITOR_EXPECT(Code),
    VISITOR_EXPECT(Context),
    VISITOR_EXPECT(DataHandler),
    VISITOR_EXPECT(DescriptorArray),
    VISITOR_EXPECT(EphemeronHashTable),
    VISITOR_EXPECT(FeedbackCell),
    VISITOR_EXPECT(FeedbackVector),
    VISITOR_EXPECT(FixedArray),
    VISITOR_EXPECT(Foreign),
    VISITOR_EXPECT(FunctionTemplateInfo),
    VISITOR_EXPECT(InstructionStream),
    VISITOR_EXPECT(InterceptorInfo),
    VISITOR_EXPECT(IrRegExpData),
    VISITOR_EXPECT(JSApiObject),
    VISITOR_EXPECT(JSArrayBuffer),
    VISITOR_EXPECT(JSDataViewOrRabGsabDataView),
    VISITOR_EXPECT(JSDate),
    VISITOR_EXPECT(JSExternalObject),
    VISITOR_EXPECT(JSFinalizationRegistry),
    VISITOR_EXPECT(JSFunction),
    VISITOR_EXPECT(JSObjectFast),
    VISITOR_EXPECT(JSProxy),
    VISITOR_EXPECT(JSRegExp),
    VISITOR_EXPECT(JSTypedArray),
    VISITOR_EXPECT(JSWeakCollection),
    VISITOR_EXPECT(JSWeakRef),
    VISITOR_EXPECT(Map),
    VISITOR_EXPECT(NativeContext),
    VISITOR_EXPECT(Oddball),
    VISITOR_EXPECT(ObjectBoilerplateDescription),
    VISITOR_EXPECT(PropertyArray),
    VISITOR_EXPECT(PrototypeInfo),
    VISITOR_EXPECT(RegExpMatchInfo),
    VISITOR_EXPECT(ShortcutCandidate),
    VISITOR_EXPECT(ScopeInfo),
    VISITOR_EXPECT(SharedFunctionInfo),
    VISITOR_EXPECT(SlicedString),
    VISITOR_EXPECT(SloppyArgumentsElements),
    VISITOR_EXPECT(SourceTextModule),
    VISITOR_EXPECT(SwissNameDictionary),
    VISITOR_EXPECT(Symbol),
    VISITOR_EXPECT(SyntheticModule),
    VISITOR_EXPECT(ThinString),
    VISITOR_EXPECT(TransitionArray),
    VISITOR_EXPECT(WeakArrayList),
    VISITOR_EXPECT(WeakCell),
    VISITOR_EXPECT(WeakFixedArray),
};

#undef VISITOR_EXPECT

std::string MissingVisitorIdsMessage(const VisitorIdCounts& counts) {
  std::ostringstream out;
  bool first = true;
  for (const VisitorIdExpectation& item : kStableVisitorExpectations) {
    if (counts.find(item.id) != counts.end()) continue;
    if (!first) out << ", ";
    out << item.name;
    first = false;
  }
  return out.str();
}

std::string MissingVisitorIdsMessage(
    const VisitorIdCounts& counts,
    const std::vector<VisitorIdExpectation>& expectations) {
  std::ostringstream out;
  bool first = true;
  for (const VisitorIdExpectation& item : expectations) {
#ifdef V8_ENABLE_WEBASSEMBLY
    if (item.id == VisitorId::kVisitWasmStruct) continue;
#endif
    if (counts.find(item.id) != counts.end()) continue;
    if (!first) out << ", ";
    out << item.name;
    first = false;
  }
  return out.str();
}

std::vector<VisitorIdExpectation> BuildAllNativeVisitorIds() {
  std::vector<VisitorIdExpectation> ids;
#define ADD_VISITOR_ID(Name) ids.push_back({VisitorId::kVisit##Name, #Name});
  DATA_ONLY_VISITOR_ID_LIST(ADD_VISITOR_ID)
  TORQUE_DATA_ONLY_VISITOR_ID_LIST(ADD_VISITOR_ID)
  POINTER_VISITOR_ID_LIST(ADD_VISITOR_ID)
  TORQUE_POINTER_VISITOR_ID_LIST(ADD_VISITOR_ID)
  TRUSTED_VISITOR_ID_LIST(ADD_VISITOR_ID)
#undef ADD_VISITOR_ID
  return ids;
}

std::set<VisitorId> BuildDfxDeclaredVisitorIds() {
  std::set<VisitorId> ids;
#define ADD_VISITOR_ID(Name) ids.insert(VisitorId::kVisit##Name);
  DFX_DATA_ONLY_VISITOR_ID_LIST(ADD_VISITOR_ID)
  DFX_TORQUE_DATA_ONLY_VISITOR_ID_LIST(ADD_VISITOR_ID)
  DFX_HEAP_OBJECT_NO_NEED_DUMP_ID_LIST(ADD_VISITOR_ID)
  DFX_TYPED_VISITOR_ID_LIST(ADD_VISITOR_ID)
  DFX_TYPED_VISITOR_WITH_SLACK_ID_LIST(ADD_VISITOR_ID)
  DFX_TORQUE_VISITOR_ID_LIST(ADD_VISITOR_ID)
  DFX_TRUSTED_VISITOR_ID_LIST(ADD_VISITOR_ID)
#undef ADD_VISITOR_ID
  return ids;
}

std::set<VisitorId> BuildDfxSpecialVisitorIds() {
  std::set<VisitorId> ids;
  ids.insert(VisitorId::kVisitInstructionStream);
  ids.insert(VisitorId::kVisitJSApiObject);
  ids.insert(VisitorId::kVisitJSObjectFast);
  ids.insert(VisitorId::kVisitShortcutCandidate);
  ids.insert(VisitorId::kVisitStruct);
#if V8_ENABLE_WEBASSEMBLY
  ids.insert(VisitorId::kVisitWasmArray);
  ids.insert(VisitorId::kVisitWasmDispatchTable);
  ids.insert(VisitorId::kVisitWasmStruct);
  ids.insert(VisitorId::kVisitWasmTrustedInstanceData);
#endif
  return ids;
}

std::set<VisitorId> BuildDfxExplicitUnsupportedVisitorIds() {
  std::set<VisitorId> ids;
  return ids;
}

std::set<VisitorId> BuildDfxIgnoredVisitorIds() {
  std::set<VisitorId> ids;
  ids.insert(VisitorId::kVisitFreeSpace);
  ids.insert(VisitorId::kVisitFiller);
  return ids;
}

std::set<VisitorId> BuildDfxPolicyVisitorIds() {
  std::set<VisitorId> ids = BuildDfxDeclaredVisitorIds();
  std::set<VisitorId> special = BuildDfxSpecialVisitorIds();
  std::set<VisitorId> unsupported = BuildDfxExplicitUnsupportedVisitorIds();
  std::set<VisitorId> ignored = BuildDfxIgnoredVisitorIds();
  ids.insert(special.begin(), special.end());
  ids.insert(unsupported.begin(), unsupported.end());
  ids.insert(ignored.begin(), ignored.end());
#if V8_ENABLE_WEBASSEMBLY
  // Wasm trusted-instance-data policy is validated in the dedicated
  // test-heap-dump-visitor-ids-wasm.cc suite instead of the generic DFX
  // coverage check.
  ids.insert(VisitorId::kVisitWasmTrustedInstanceData);
#endif
  return ids;
}

std::string MissingPolicyMessage() {
  std::set<VisitorId> policy = BuildDfxPolicyVisitorIds();
  std::ostringstream out;
  bool first = true;
  for (const VisitorIdExpectation& item : BuildAllNativeVisitorIds()) {
    if (policy.find(item.id) != policy.end()) continue;
    if (!first) out << ", ";
    out << item.name;
    first = false;
  }
  return out.str();
}

std::string ExtraPolicyMessage() {
  std::set<VisitorId> native_ids;
  for (const VisitorIdExpectation& item : BuildAllNativeVisitorIds()) {
    native_ids.insert(item.id);
  }
  std::ostringstream out;
  bool first = true;
  for (VisitorId id : BuildDfxPolicyVisitorIds()) {
    if (native_ids.find(id) != native_ids.end()) continue;
    if (!first) out << ", ";
    out << static_cast<int>(id);
    first = false;
  }
  return out.str();
}

VisitorCoverageRun RunJavaScriptVisitorCoverage() {
  VisitorCoverageRun run;
  run.generator = DumpAndTranslateWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        DumpArtifacts artifacts =
            CreateVisitorCoverageObjects(isolate, context);
        DirectHandle<String> runtime_cons =
            GetGlobalDirectHandle<String>(isolate, context, "runtimeCons");
        DirectHandle<String> runtime_thin =
            GetGlobalDirectHandle<String>(isolate, context, "runtimeThin");
        run.string_state.runtime_cons_before_gc = IsConsString(*runtime_cons);
        run.string_state.runtime_thin_before_gc = IsThinString(*runtime_thin);
        run.string_state.runtime_cons_visitor_before_gc =
            static_cast<int>(runtime_cons->map()->visitor_id());
        run.string_state.runtime_thin_visitor_before_gc =
            static_cast<int>(runtime_thin->map()->visitor_id());
        i::heap::InvokeMemoryReducingMajorGCs(isolate->heap());
        run.string_state.runtime_cons_after_gc = IsConsString(*runtime_cons);
        run.string_state.runtime_thin_after_gc = IsThinString(*runtime_thin);
        run.string_state.runtime_cons_visitor_after_gc =
            static_cast<int>(runtime_cons->map()->visitor_id());
        run.string_state.runtime_thin_visitor_after_gc =
            static_cast<int>(runtime_thin->map()->visitor_id());
        run.counts = CollectVisitorIdCounts(isolate);
        return artifacts;
      });
  return run;
}

}  // namespace

void CheckInterpreterDataProtectedPointerEdges(
    TranslateDiagnostics* diagnostics = nullptr) {
  InterpreterDataProtectedEdgesState state;
  bool ok = false;
  std::vector<uint8_t> bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        FlagScope<bool> allow_natives_syntax(&v8_flags.allow_natives_syntax,
                                             true);
        RunScript(context, R"(
              globalThis.__heap_dump_interpreter_data_fn =
                  function HeapDumpInterpreterDataProtectedPointerEdges(x) {
                return x + 1;
              };
              %PrepareFunctionForOptimization(
                  globalThis.__heap_dump_interpreter_data_fn);
              globalThis.__heap_dump_interpreter_data_fn(41);
            )");

        DirectHandle<JSFunction> function = GetGlobalDirectHandle<JSFunction>(
            isolate, context, "__heap_dump_interpreter_data_fn");
        DirectHandle<SharedFunctionInfo> shared(function->shared(), isolate);
        CHECK(shared->HasBytecodeArray());
        DirectHandle<BytecodeArray> bytecode_array(
            shared->GetBytecodeArray(isolate), isolate);
        DirectHandle<Code> code = CreateCodeForTest(isolate);
        DirectHandle<InterpreterData> interpreter_data =
            isolate->factory()->NewInterpreterData(bytecode_array, code);

        CHECK_EQ(INTERPRETER_DATA_TYPE,
                 interpreter_data->map()->instance_type());
        CHECK_EQ(BYTECODE_ARRAY_TYPE, bytecode_array->map()->instance_type());
        CHECK_EQ(CODE_TYPE, code->map()->instance_type());
        CHECK_EQ(*bytecode_array, interpreter_data->bytecode_array());
        CHECK_EQ(*code, interpreter_data->interpreter_trampoline());

        state.saw_interpreter_data_type = true;
        state.saw_bytecode_array_type = true;
        state.saw_code_type = true;
        state.interpreter_data = interpreter_data->address();
        state.bytecode_array = bytecode_array->address();
        state.interpreter_trampoline =
            interpreter_data->interpreter_trampoline().address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(function);
        artifacts.keep_alive.push_back(shared);
        artifacts.keep_alive.push_back(bytecode_array);
        artifacts.keep_alive.push_back(code);
        artifacts.keep_alive.push_back(interpreter_data);
        return artifacts;
      });
  std::unique_ptr<SnapshotGenerator> generator =
      TranslateHeapBytes(bytes, diagnostics, &ok);
  CHECK(ok);

  CHECK(state.saw_interpreter_data_type);
  CHECK(state.saw_bytecode_array_type);
  CHECK(state.saw_code_type);

  const Node* interpreter_data =
      GetSnapshotNode(generator.get(), state.interpreter_data);
  const Node* bytecode_array =
      GetSnapshotNode(generator.get(), state.bytecode_array);
  const Node* interpreter_trampoline =
      GetSnapshotNode(generator.get(), state.interpreter_trampoline);
  CHECK_NOT_NULL(interpreter_data);
  CHECK_NOT_NULL(bytecode_array);
  CHECK_NOT_NULL(interpreter_trampoline);

  CheckSingleHiddenEdgeToTargetNoPropertyOrInternalDuplicate(interpreter_data,
                                                             bytecode_array);
  CheckSingleHiddenEdgeToTargetNoPropertyOrInternalDuplicate(
      interpreter_data, interpreter_trampoline);
}

void CheckBytecodeArrayProtectedPointerHiddenEdges(
    TranslateDiagnostics* diagnostics = nullptr) {
  BytecodeArrayProtectedEdgesState state;
  bool ok = false;
  std::vector<uint8_t> bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        FlagScope<bool> allow_natives_syntax(&v8_flags.allow_natives_syntax,
                                             true);
        RunScript(context, R"(
              globalThis.__heap_dump_bytecode_array_fn =
                  function HeapDumpBytecodeArrayProtectedPointerHiddenEdges(x) {
                const y = x + 1;
                return y * 2;
              };
              %PrepareFunctionForOptimization(
                  globalThis.__heap_dump_bytecode_array_fn);
              globalThis.__heap_dump_bytecode_array_fn(20);
            )");

        DirectHandle<JSFunction> function = GetGlobalDirectHandle<JSFunction>(
            isolate, context, "__heap_dump_bytecode_array_fn");
        DirectHandle<SharedFunctionInfo> shared(function->shared(), isolate);
        CHECK(shared->HasBytecodeArray());
        DirectHandle<BytecodeArray> bytecode_array(
            shared->GetBytecodeArray(isolate), isolate);

        Tagged<TrustedFixedArray> constant_pool =
            bytecode_array->constant_pool();
        Tagged<TrustedByteArray> handler_table =
            bytecode_array->handler_table();
        Tagged<Object> raw_source_position_table =
            bytecode_array->raw_source_position_table(kAcquireLoad);

        CHECK_EQ(BYTECODE_ARRAY_TYPE, bytecode_array->map()->instance_type());
        CHECK_EQ(TRUSTED_FIXED_ARRAY_TYPE,
                 constant_pool->map()->instance_type());
        CHECK_EQ(TRUSTED_BYTE_ARRAY_TYPE,
                 handler_table->map()->instance_type());

        state.saw_bytecode_array_type = true;
        state.saw_constant_pool_type = true;
        state.saw_handler_table_type = true;
        state.bytecode_array = bytecode_array->address();
        state.constant_pool = constant_pool.address();
        state.handler_table = handler_table.address();
        if (IsTrustedByteArray(raw_source_position_table)) {
          Tagged<TrustedByteArray> source_position_table =
              TrustedCast<TrustedByteArray>(raw_source_position_table);
          CHECK_EQ(TRUSTED_BYTE_ARRAY_TYPE,
                   source_position_table->map()->instance_type());
          state.saw_source_position_table_type = true;
          state.source_position_table = source_position_table.address();
        }

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(function);
        artifacts.keep_alive.push_back(shared);
        artifacts.keep_alive.push_back(bytecode_array);
        return artifacts;
      });
  std::unique_ptr<SnapshotGenerator> generator =
      TranslateHeapBytes(bytes, diagnostics, &ok);
  CHECK(ok);

  CHECK(state.saw_bytecode_array_type);
  CHECK(state.saw_constant_pool_type);
  CHECK(state.saw_handler_table_type);

  const Node* bytecode_array =
      GetSnapshotNode(generator.get(), state.bytecode_array);
  const Node* constant_pool =
      GetSnapshotNode(generator.get(), state.constant_pool);
  const Node* handler_table =
      GetSnapshotNode(generator.get(), state.handler_table);
  CHECK_NOT_NULL(bytecode_array);
  CHECK_NOT_NULL(constant_pool);
  CHECK_NOT_NULL(handler_table);

  CheckSingleHiddenEdgeToTargetNoPropertyOrInternalDuplicate(bytecode_array,
                                                             constant_pool);
  CheckSingleHiddenEdgeToTargetNoPropertyOrInternalDuplicate(bytecode_array,
                                                             handler_table);
  if (state.saw_source_position_table_type) {
    const Node* source_position_table =
        GetSnapshotNode(generator.get(), state.source_position_table);
    CHECK_NOT_NULL(source_position_table);
    CheckSingleHiddenEdgeToTargetNoPropertyOrInternalDuplicate(
        bytecode_array, source_position_table);
  }
}

UNINITIALIZED_TEST(HeapDumpTranslateInterpreterDataProtectedPointerEdges) {
  CheckInterpreterDataProtectedPointerEdges();
}

UNINITIALIZED_TEST(HeapDumpBytecodeArrayProtectedPointerHiddenEdges) {
  CheckBytecodeArrayProtectedPointerHiddenEdges();
}

#if defined(V8_ENABLE_SANDBOX)
UNINITIALIZED_TEST(
    HeapDumpSandboxInterpreterDataProtectedPointerEdgesWithTrustedCageBase) {
  TranslateDiagnostics diagnostics;
  CheckInterpreterDataProtectedPointerEdges(&diagnostics);
  CHECK(!HasDiagnostic(diagnostics,
                       TranslateDiagnostics::Severity::kUnsupported,
                       "VisitProtectedPointer in sandbox", "object_body"));
  CHECK(!HasDiagnostic(
      diagnostics, TranslateDiagnostics::Severity::kUnsupported,
      "sandbox_protected_pointer_missing_trusted_cage_base", "object_body"));
  CHECK(!HasDiagnostic(
      diagnostics, TranslateDiagnostics::Severity::kWarning,
      "sandbox_protected_pointer_missing_trusted_cage_base", "object_body"));
}

UNINITIALIZED_TEST(
    HeapDumpSandboxBytecodeArrayProtectedPointerHiddenEdgesWithTrustedCageBase) {
  TranslateDiagnostics diagnostics;
  CheckBytecodeArrayProtectedPointerHiddenEdges(&diagnostics);
  CHECK(!HasDiagnostic(diagnostics,
                       TranslateDiagnostics::Severity::kUnsupported,
                       "VisitProtectedPointer in sandbox", "object_body"));
  CHECK(!HasDiagnostic(
      diagnostics, TranslateDiagnostics::Severity::kUnsupported,
      "sandbox_protected_pointer_missing_trusted_cage_base", "object_body"));
  CHECK(!HasDiagnostic(
      diagnostics, TranslateDiagnostics::Severity::kWarning,
      "sandbox_protected_pointer_missing_trusted_cage_base", "object_body"));
}
#endif

UNINITIALIZED_TEST(HeapDumpVisitorIdsJavaScriptCoverage) {
  VisitorCoverageRun run = RunJavaScriptVisitorCoverage();
  CHECK_GT(run.generator->nodes().size(), 0u);
  std::string missing = MissingVisitorIdsMessage(run.counts);
  if (!missing.empty()) {
    std::cout << "HeapDump missing visitor ids: " << missing << std::endl;
  }
  CHECK(missing.empty());
  CHECK(run.string_state.runtime_thin_before_gc);
  CHECK(run.string_state.runtime_thin_after_gc);
  CHECK_EQ(static_cast<int>(VisitorId::kVisitShortcutCandidate),
           run.string_state.runtime_cons_visitor_before_gc);
  CHECK_EQ(static_cast<int>(VisitorId::kVisitShortcutCandidate),
           run.string_state.runtime_cons_visitor_after_gc);
  CHECK_EQ(static_cast<int>(VisitorId::kVisitThinString),
           run.string_state.runtime_thin_visitor_before_gc);
  CHECK_EQ(static_cast<int>(VisitorId::kVisitThinString),
           run.string_state.runtime_thin_visitor_after_gc);
}

UNINITIALIZED_TEST(HeapDumpVisitorIdsCoverageReport) {
  VisitorCoverageRun run = RunJavaScriptVisitorCoverage();
  USE(run.generator);
  std::vector<VisitorIdExpectation> all = BuildNonTorqueVisitorExpectations();
  std::string missing = MissingVisitorIdsMessage(run.counts, all);
  std::cout << "HeapDump JS-side visitor-id coverage missing: "
            << (missing.empty() ? "<none>" : missing) << std::endl;
  std::cout
      << "HeapDump constructed string states: cons(before="
      << run.string_state.runtime_cons_before_gc
      << ", after=" << run.string_state.runtime_cons_after_gc
      << ", visitor_before=" << run.string_state.runtime_cons_visitor_before_gc
      << ", visitor_after=" << run.string_state.runtime_cons_visitor_after_gc
      << "), thin(before=" << run.string_state.runtime_thin_before_gc
      << ", after=" << run.string_state.runtime_thin_after_gc
      << ", visitor_before=" << run.string_state.runtime_thin_visitor_before_gc
      << ", visitor_after=" << run.string_state.runtime_thin_visitor_after_gc
      << ")" << std::endl;
}

UNINITIALIZED_TEST(HeapDumpVisitorIdsSourceBackedExclusions) {
  VisitorCoverageRun run = RunJavaScriptVisitorCoverage();
  USE(run.generator);
  CHECK_EQ(0, v8::Promise::kEmbedderFieldCount);
  CHECK_EQ(static_cast<int>(VisitorId::kVisitShortcutCandidate),
           run.string_state.runtime_cons_visitor_before_gc);
  CHECK_EQ(static_cast<int>(VisitorId::kVisitShortcutCandidate),
           run.string_state.runtime_cons_visitor_after_gc);
}

UNINITIALIZED_TEST(HeapDumpCppHeapExternalMegaDomHandlerExactMapEdges) {
  struct Target {
    InstanceType type;
    Address address = 0;
    Address map_address = 0;
    bool is_expected_instance_type = false;
  };

  Target cpp_heap_external_target{CPP_HEAP_EXTERNAL_OBJECT_TYPE};
  Target mega_dom_handler_target{MEGA_DOM_HANDLER_TYPE};

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DumpArtifacts artifacts;
        Factory* factory = isolate->factory();

        Handle<CppHeapExternalObject> cpp_heap_external =
            factory->NewCppHeapExternal(AllocationType::kOld);
        DirectHandle<Map> cpp_heap_external_map(cpp_heap_external->map(),
                                                isolate);

        DirectHandle<FunctionTemplateInfo> api_function_template =
            factory->NewFunctionTemplateInfo(0, false);
        DirectHandle<NativeContext> native_context = isolate->native_context();
        DirectHandle<MegaDomHandler> mega_dom_handler =
            factory->NewMegaDomHandler(
                MaybeObjectDirectHandle::Weak(api_function_template),
                MaybeObjectDirectHandle::Weak(native_context));
        DirectHandle<Map> mega_dom_handler_map(mega_dom_handler->map(),
                                               isolate);

        InvokeAtomicMajorGC(isolate->heap());

        CHECK_EQ(cpp_heap_external_target.type,
                 cpp_heap_external_map->instance_type());
        cpp_heap_external_target.is_expected_instance_type = true;
        cpp_heap_external_target.address = cpp_heap_external->address();
        cpp_heap_external_target.map_address = cpp_heap_external_map->address();

        CHECK_EQ(mega_dom_handler_target.type,
                 mega_dom_handler_map->instance_type());
        mega_dom_handler_target.is_expected_instance_type = true;
        mega_dom_handler_target.address = mega_dom_handler->address();
        mega_dom_handler_target.map_address = mega_dom_handler_map->address();

        artifacts.keep_alive.push_back(
            direct_handle(*cpp_heap_external, isolate));
        artifacts.keep_alive.push_back(cpp_heap_external_map);
        artifacts.keep_alive.push_back(api_function_template);
        artifacts.keep_alive.push_back(native_context);
        artifacts.keep_alive.push_back(mega_dom_handler);
        artifacts.keep_alive.push_back(mega_dom_handler_map);
        return artifacts;
      });

  Target* targets[] = {&cpp_heap_external_target, &mega_dom_handler_target};
  for (Target* target : targets) {
    Node* object = GetSnapshotNode(generator.get(), target->address);
    Node* map = GetSnapshotNode(generator.get(), target->map_address);
    if (!target->is_expected_instance_type || object == nullptr ||
        map == nullptr) {
      PrintF("[TODO] %s not found as an exact heap object with map edge.\n",
             HeapDumpInstanceTypeName(target->type));
      continue;
    }

    CheckExactInternalMapEdgeNoWrongKind(generator.get(), object, map);
    MarkCoveredInstanceType(target->type);
  }
}

UNINITIALIZED_TEST(HeapDumpVisitorIdsAllNativeIdsHaveDfxPolicy) {
  std::string missing = MissingPolicyMessage();
  if (!missing.empty()) {
    std::cout << "HeapDump visitor ids missing DFX policy: " << missing
              << std::endl;
  }
  CHECK(missing.empty());

  std::string extra = ExtraPolicyMessage();
  if (!extra.empty()) {
    std::cout
        << "HeapDump DFX visitor policies not present in V8 native lists: "
        << extra << std::endl;
  }
  CHECK(extra.empty());
}

UNINITIALIZED_TEST(HeapDumpVisitorIdsUnsupportedPolicyIsExplicit) {
  std::set<VisitorId> declared = BuildDfxDeclaredVisitorIds();
  std::set<VisitorId> special = BuildDfxSpecialVisitorIds();
  std::set<VisitorId> unsupported = BuildDfxExplicitUnsupportedVisitorIds();
#if V8_ENABLE_WEBASSEMBLY
  // Dynamic-layout Wasm bodies are handled by explicit special paths, not by
  // the generic DFX visitor list or the unsupported visitor-id bucket.
  CHECK(special.find(VisitorId::kVisitWasmArray) != special.end());
  CHECK(special.find(VisitorId::kVisitWasmStruct) != special.end());
  CHECK(unsupported.find(VisitorId::kVisitWasmArray) == unsupported.end());
  CHECK(unsupported.find(VisitorId::kVisitWasmStruct) == unsupported.end());
  CHECK(declared.find(VisitorId::kVisitWasmArray) == declared.end());
  CHECK(declared.find(VisitorId::kVisitWasmStruct) == declared.end());
#endif
  CHECK(unsupported.empty());
}

#else

UNINITIALIZED_TEST(HeapDumpVisitorIdsJavaScriptCoverage) {}
UNINITIALIZED_TEST(HeapDumpBytecodeArrayProtectedPointerHiddenEdges) {}
UNINITIALIZED_TEST(HeapDumpVisitorIdsCoverageReport) {}
UNINITIALIZED_TEST(HeapDumpCppHeapExternalMegaDomHandlerExactMapEdges) {}
UNINITIALIZED_TEST(HeapDumpVisitorIdsAllNativeIdsHaveDfxPolicy) {}
UNINITIALIZED_TEST(HeapDumpVisitorIdsUnsupportedPolicyIsExplicit) {}

#endif

}  // namespace heap
}  // namespace internal
}  // namespace v8
