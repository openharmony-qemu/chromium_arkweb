#include "arkweb/chromium_ext/v8/dfx/heap_dump/tests/cctest/heap-dump-test-utils.h"

#include "src/objects/api-callbacks-inl.h"
#include "src/objects/code-inl.h"
#include "src/objects/deoptimization-data.h"
#include "src/objects/deoptimization-data-inl.h"
#include "src/objects/js-regexp.h"
#include "src/objects/literal-objects.h"
#include "src/objects/property-details.h"
#include "src/objects/struct-inl.h"

namespace v8 {
namespace internal {
namespace heap {

#if defined(OH_ENABLE_HEAP_DUMP_TEST) && defined(OH_ENABLE_HEAP_DUMP) && \
    defined(OH_ENABLE_HEAP_TRANSLATE)

namespace {

using namespace heap_dump_test;

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

size_t CountPresentStrings(SnapshotGenerator* generator,
                           std::initializer_list<const char*> names) {
  size_t present = 0;
  for (const char* name : names) {
    if (HasString(generator, name)) {
      ++present;
    }
  }
  return present;
}

DirectHandle<Code> CreateCodeForInterpreterDataTest(Isolate* isolate) {
#define __ assm.
  Assembler assm(isolate->allocator(), AssemblerOptions{});
  __ nop();
  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  return Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#undef __
}

DirectHandle<Code> CreateBaselineCodeForTest(
    Isolate* isolate, Handle<TrustedObject> interpreter_data,
    int parameter_count, DirectHandle<TrustedByteArray> bytecode_offset_table) {
#define __ assm.
  Assembler assm(isolate->allocator(), AssemblerOptions{});
  __ nop();
  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<TrustedByteArray> table = handle(*bytecode_offset_table, isolate);
  return Factory::CodeBuilder(isolate, desc, CodeKind::BASELINE)
      .set_interpreter_data(interpreter_data)
      .set_bytecode_offset_table(table)
      .set_parameter_count(parameter_count)
      .Build();
#undef __
}

DirectHandle<Code> CreateOptimizedCodeForTest(
    Isolate* isolate, Handle<DeoptimizationData> deoptimization_data,
    DirectHandle<TrustedByteArray> source_position_table) {
#define __ assm.
  Assembler assm(isolate->allocator(), AssemblerOptions{});
  __ nop();
  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  return Factory::CodeBuilder(isolate, desc, CodeKind::TURBOFAN_JS)
      .set_is_turbofanned()
      .set_deoptimization_data(deoptimization_data)
      .set_source_position_table(handle(*source_position_table, isolate))
      .Build();
#undef __
}

}  // namespace

UNINITIALIZED_TEST(HeapDumpCodeBaselineSupportExactEdges) {
  {
    SnapshotGenerator retag_generator;
    constexpr Address kExistingAddress = 0x1000;
    constexpr Address kMissingAddress = 0x2000;
    Node* existing = retag_generator.AddNode(kExistingAddress,
                                             Node::Type::kObject, "", 16);
    const uint32_t original_id = existing->id_;
    const uint32_t original_size = existing->self_size_;
    SnapshotGenerator::RetagNodeStatus retag_status =
        retag_generator.RetagNodeIfExisting(
        kExistingAddress, "(bytecode offset table)", Node::Type::kCode, true,
        false);
    CHECK(retag_status.node_found);
    CHECK(retag_status.name_updated);
    CHECK(!retag_status.name_suppressed);
    CHECK(retag_status.type_updated);
    CHECK(retag_status.Applied());
    CHECK_EQ(original_id, existing->id_);
    CHECK_EQ(original_size, existing->self_size_);
    CHECK_EQ(0u, existing->childs_.size());
    CHECK(NodeMatchesTypeAndName(&retag_generator, existing, Node::Type::kCode,
                                 "(bytecode offset table)"));
    SnapshotGenerator::RetagNodeStatus missing_status =
        retag_generator.RetagNodeIfExisting(
        kMissingAddress, "(bytecode offset table)", Node::Type::kCode, true,
        false);
    CHECK(!missing_status.node_found);
    CHECK(!missing_status.Applied());

    Node* label_only =
        retag_generator.AddNode(0x2500, Node::Type::kObject, "", 8);
    SnapshotGenerator::RetagNodeStatus label_only_status =
        retag_generator.RetagNodeIfExisting(
            0x2500, "(interpreter data)", Node::Type::kCode, false, false);
    CHECK(label_only_status.node_found);
    CHECK(label_only_status.name_updated);
    CHECK(!label_only_status.name_suppressed);
    CHECK(!label_only_status.type_updated);
    CHECK(label_only_status.Applied());
    CHECK(NodeMatchesTypeAndName(&retag_generator, label_only,
                                 Node::Type::kObject, "(interpreter data)"));

    Node* meaningful = retag_generator.AddNode(0x3000, Node::Type::kObject,
                                               "meaningful", 8);
    SnapshotGenerator::RetagNodeStatus suppressed_status =
        retag_generator.RetagNodeIfExisting(
        0x3000, "(bytecode offset table)", Node::Type::kCode, true, true);
    CHECK(suppressed_status.node_found);
    CHECK(!suppressed_status.name_updated);
    CHECK(suppressed_status.name_suppressed);
    CHECK(suppressed_status.type_updated);
    CHECK(suppressed_status.Applied());
    CHECK(NodeMatchesTypeAndName(&retag_generator, meaningful,
                                 Node::Type::kCode, "meaningful"));
  }

  struct {
    Address code = 0;
    Address instruction_stream = 0;
    Address interpreter_data = 0;
    Address bytecode_offset_table = 0;
    InstanceType interpreter_data_type = LAST_TYPE;
  } state;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              globalThis.__heap_dump_code_baseline_support =
                  function HeapDumpCodeBaselineSupport(x) { return x + 17; };
              globalThis.__heap_dump_code_baseline_support(25);
            )");

            DirectHandle<JSFunction> function =
                GetGlobalDirectHandle<JSFunction>(
                    isolate, context, "__heap_dump_code_baseline_support");
            DirectHandle<SharedFunctionInfo> shared(function->shared(),
                                                    isolate);
            CHECK(shared->HasBytecodeArray());
            DirectHandle<BytecodeArray> bytecode_array(
                shared->GetBytecodeArray(isolate), isolate);
            DirectHandle<Code> interpreter_trampoline =
                CreateCodeForInterpreterDataTest(isolate);
            DirectHandle<InterpreterData> interpreter_data =
                isolate->factory()->NewInterpreterData(bytecode_array,
                                                       interpreter_trampoline);
            DirectHandle<TrustedByteArray> bytecode_offset_table =
                isolate->factory()->NewTrustedByteArray(4);
            Handle<TrustedObject> baseline_interpreter_data =
                handle(Cast<TrustedObject>(*interpreter_data), isolate);
            DirectHandle<Code> baseline_code = CreateBaselineCodeForTest(
                isolate, baseline_interpreter_data,
                bytecode_array->parameter_count(), bytecode_offset_table);

            CHECK_EQ(CODE_TYPE, baseline_code->map()->instance_type());
            CHECK_EQ(CodeKind::BASELINE, baseline_code->kind());
            CHECK(baseline_code->has_instruction_stream());
            CHECK_EQ(INTERPRETER_DATA_TYPE,
                     interpreter_data->map()->instance_type());
            CHECK_EQ(TRUSTED_BYTE_ARRAY_TYPE,
                     bytecode_offset_table->map()->instance_type());
            CHECK_EQ(*interpreter_data,
                     baseline_code->bytecode_or_interpreter_data());
            CHECK_EQ(*bytecode_offset_table,
                     baseline_code->bytecode_offset_table());

            state.code = baseline_code->address();
            state.instruction_stream =
                baseline_code->instruction_stream().address();
            state.interpreter_data =
                baseline_code->bytecode_or_interpreter_data().address();
            state.bytecode_offset_table =
                baseline_code->bytecode_offset_table().address();
            state.interpreter_data_type =
                baseline_code->bytecode_or_interpreter_data()
                    ->map()
                    ->instance_type();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(function);
            artifacts.keep_alive.push_back(shared);
            artifacts.keep_alive.push_back(bytecode_array);
            artifacts.keep_alive.push_back(interpreter_trampoline);
            artifacts.keep_alive.push_back(interpreter_data);
            artifacts.keep_alive.push_back(bytecode_offset_table);
            artifacts.keep_alive.push_back(baseline_code);
            return artifacts;
          });

  const Node* code = GetSnapshotNode(generator.get(), state.code);
  const Node* instruction_stream =
      GetSnapshotNode(generator.get(), state.instruction_stream);
  const Node* interpreter_data =
      GetSnapshotNode(generator.get(), state.interpreter_data);
  const Node* bytecode_offset_table =
      GetSnapshotNode(generator.get(), state.bytecode_offset_table);
  CHECK_NOT_NULL(code);
  CHECK_NOT_NULL(instruction_stream);
  CHECK_NOT_NULL(interpreter_data);
  CHECK_NOT_NULL(bytecode_offset_table);
  CHECK(NodeHasType(code, Node::Type::kCode));
  CHECK(NodeHasType(instruction_stream, Node::Type::kCode));
  CHECK(state.interpreter_data_type == BYTECODE_ARRAY_TYPE ||
        state.interpreter_data_type == INTERPRETER_DATA_TYPE);
  CHECK_EQ("(interpreter data)",
           NodeDisplayName(generator.get(), interpreter_data));
  // InterpreterData's default heap-entry category is already kCode; the
  // label-only RetagNodeIfExisting behavior is covered by the helper probe
  // above where update_type=false keeps an Object node as kObject.
  CHECK(NodeHasType(interpreter_data, Node::Type::kCode));
  CHECK(NodeHasType(bytecode_offset_table, Node::Type::kCode));
  CHECK_EQ("(bytecode offset table)",
           NodeDisplayName(generator.get(), bytecode_offset_table));

  // White-box coverage for V8HeapExplorer::ExtractCodeReferences, BASELINE:
  // E1: SetInternalReference("instruction_stream",
  //                          code->instruction_stream(), kInstructionStream).
  CHECK(HasExactNamedEdge(generator.get(), code, Edge::Type::kInternal,
                          "instruction_stream", instruction_stream));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), code,
                                   Edge::Type::kInternal,
                                   "instruction_stream", instruction_stream));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(code, Edge::Type::kInternal,
                                         instruction_stream));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kHidden,
                                         instruction_stream));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kWeak,
                                         instruction_stream));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kProperty,
                                         instruction_stream));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kShortcut,
                                         instruction_stream));

  // E2: SetInternalReference("interpreter_data",
  //                          code->bytecode_or_interpreter_data(),
  //                          kDeoptimizationDataOrInterpreterDataOffset).
  CHECK(HasExactNamedEdge(generator.get(), code, Edge::Type::kInternal,
                          "interpreter_data", interpreter_data));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), code,
                                   Edge::Type::kInternal, "interpreter_data",
                                   interpreter_data));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(code, Edge::Type::kInternal,
                                         interpreter_data));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kHidden,
                                         interpreter_data));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kWeak,
                                         interpreter_data));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kProperty,
                                         interpreter_data));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kShortcut,
                                         interpreter_data));

  // E3: SetInternalReference("bytecode_offset_table",
  //                          code->bytecode_offset_table(),
  //                          kPositionTableOffset).
  CHECK(HasExactNamedEdge(generator.get(), code, Edge::Type::kInternal,
                          "bytecode_offset_table", bytecode_offset_table));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), code,
                                   Edge::Type::kInternal,
                                   "bytecode_offset_table",
                                   bytecode_offset_table));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kHidden,
                                         bytecode_offset_table));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kWeak,
                                         bytecode_offset_table));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kProperty,
                                         bytecode_offset_table));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kShortcut,
                                         bytecode_offset_table));

  CHECK_EQ(0, CountExactNamedEdges(generator.get(), code,
                                   Edge::Type::kInternal,
                                   "bytecode_offset_table",
                                   interpreter_data));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), code,
                                   Edge::Type::kInternal, "interpreter_data",
                                   bytecode_offset_table));
}

UNINITIALIZED_TEST(HeapDumpCodeDeoptSupportExactEdges) {
  struct {
    Address code = 0;
    Address instruction_stream = 0;
    Address deoptimization_data = 0;
    Address frame_translation = 0;
    Address literal_array = 0;
    Address inlining_positions = 0;
    Address source_position_table = 0;
  } state;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);

            Handle<DeoptimizationData> deoptimization_data =
                DeoptimizationData::New(isolate, 0);
            DirectHandle<DeoptimizationFrameTranslation> frame_translation =
                isolate->factory()->NewDeoptimizationFrameTranslation(4);
            deoptimization_data->SetFrameTranslation(*frame_translation);
            DirectHandle<DeoptimizationLiteralArray> literal_array =
                isolate->factory()->NewDeoptimizationLiteralArray(1);
            deoptimization_data->SetLiteralArray(*literal_array);
            DirectHandle<TrustedPodArray<InliningPosition>>
                inlining_positions =
                    TrustedPodArray<InliningPosition>::New(isolate, 1);
            deoptimization_data->SetInliningPositions(*inlining_positions);
            DirectHandle<TrustedByteArray> source_position_table =
                isolate->factory()->NewTrustedByteArray(4);
            DirectHandle<Code> optimized_code = CreateOptimizedCodeForTest(
                isolate, deoptimization_data, source_position_table);

            CHECK_EQ(CODE_TYPE, optimized_code->map()->instance_type());
            CHECK_EQ(CodeKind::TURBOFAN_JS, optimized_code->kind());
            CHECK(optimized_code->has_instruction_stream());
            CHECK(optimized_code->uses_deoptimization_data());
            CHECK_EQ(PROTECTED_FIXED_ARRAY_TYPE,
                     deoptimization_data->map()->instance_type());
            CHECK_GT(deoptimization_data->length(),
                     DeoptimizationData::kFrameTranslationIndex);
            CHECK_EQ(TRUSTED_BYTE_ARRAY_TYPE,
                     frame_translation->map()->instance_type());
            CHECK_EQ(*frame_translation,
                     deoptimization_data->FrameTranslation());
            CHECK_GT(deoptimization_data->length(),
                     DeoptimizationData::kLiteralArrayIndex);
            CHECK_EQ(TRUSTED_WEAK_FIXED_ARRAY_TYPE,
                     literal_array->map()->instance_type());
            CHECK_EQ(*literal_array, deoptimization_data->LiteralArray());
            CHECK_GT(deoptimization_data->length(),
                     DeoptimizationData::kInliningPositionsIndex);
            CHECK_EQ(TRUSTED_BYTE_ARRAY_TYPE,
                     inlining_positions->map()->instance_type());
            CHECK_EQ(*inlining_positions,
                     deoptimization_data->InliningPositions());
            CHECK_EQ(TRUSTED_BYTE_ARRAY_TYPE,
                     source_position_table->map()->instance_type());
            CHECK_EQ(*deoptimization_data,
                     optimized_code->deoptimization_data());
            CHECK_EQ(*source_position_table,
                     optimized_code->source_position_table());

            state.code = optimized_code->address();
            state.instruction_stream =
                optimized_code->instruction_stream().address();
            state.deoptimization_data =
                optimized_code->deoptimization_data().address();
            state.frame_translation =
                deoptimization_data->FrameTranslation().address();
            state.literal_array = deoptimization_data->LiteralArray().address();
            state.inlining_positions =
                deoptimization_data->InliningPositions().address();
            state.source_position_table =
                optimized_code->source_position_table().address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(direct_handle(*deoptimization_data,
                                                         isolate));
            artifacts.keep_alive.push_back(frame_translation);
            artifacts.keep_alive.push_back(literal_array);
            artifacts.keep_alive.push_back(inlining_positions);
            artifacts.keep_alive.push_back(source_position_table);
            artifacts.keep_alive.push_back(optimized_code);
            return artifacts;
          });

  const Node* code = GetSnapshotNode(generator.get(), state.code);
  const Node* instruction_stream =
      GetSnapshotNode(generator.get(), state.instruction_stream);
  const Node* deoptimization_data =
      GetSnapshotNode(generator.get(), state.deoptimization_data);
  const Node* source_position_table =
      GetSnapshotNode(generator.get(), state.source_position_table);
  const Node* frame_translation =
      GetSnapshotNode(generator.get(), state.frame_translation);
  const Node* literal_array =
      GetSnapshotNode(generator.get(), state.literal_array);
  const Node* inlining_positions =
      GetSnapshotNode(generator.get(), state.inlining_positions);
  CHECK_NOT_NULL(code);
  CHECK_NOT_NULL(instruction_stream);
  CHECK_NOT_NULL(deoptimization_data);
  CHECK_NOT_NULL(frame_translation);
  CHECK_NOT_NULL(literal_array);
  CHECK_NOT_NULL(inlining_positions);
  CHECK_NOT_NULL(source_position_table);
  CHECK(NodeHasType(code, Node::Type::kCode));
  CHECK(NodeHasType(instruction_stream, Node::Type::kCode));
  CHECK(NodeHasType(deoptimization_data, Node::Type::kCode));
  CHECK_EQ("(code deopt data)",
           NodeDisplayName(generator.get(), deoptimization_data));
  CHECK(NodeHasType(source_position_table, Node::Type::kCode));
  CHECK_EQ("(source position table)",
           NodeDisplayName(generator.get(), source_position_table));
  CHECK(NodeHasType(frame_translation, Node::Type::kCode));
  CHECK_EQ("(code deopt data)",
           NodeDisplayName(generator.get(), frame_translation));
  CHECK(NodeHasType(literal_array, Node::Type::kCode));
  CHECK_EQ("(code deopt data)",
           NodeDisplayName(generator.get(), literal_array));
  CHECK(NodeHasType(inlining_positions, Node::Type::kCode));
  CHECK_EQ("(code deopt data)",
           NodeDisplayName(generator.get(), inlining_positions));

  // White-box coverage for V8HeapExplorer::ExtractCodeReferences, optimized:
  // E1: SetInternalReference("instruction_stream",
  //                          code->instruction_stream(), kInstructionStream).
  CHECK(HasExactNamedEdge(generator.get(), code, Edge::Type::kInternal,
                          "instruction_stream", instruction_stream));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), code,
                                   Edge::Type::kInternal,
                                   "instruction_stream", instruction_stream));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(code, Edge::Type::kInternal,
                                         instruction_stream));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kHidden,
                                         instruction_stream));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kWeak,
                                         instruction_stream));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kProperty,
                                         instruction_stream));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kShortcut,
                                         instruction_stream));

  // E2: SetInternalReference("deoptimization_data",
  //                          code->deoptimization_data(),
  //                          kDeoptimizationDataOrInterpreterDataOffset).
  CHECK(HasExactNamedEdge(generator.get(), code, Edge::Type::kInternal,
                          "deoptimization_data", deoptimization_data));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), code,
                                   Edge::Type::kInternal,
                                   "deoptimization_data",
                                   deoptimization_data));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(code, Edge::Type::kInternal,
                                         deoptimization_data));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kHidden,
                                         deoptimization_data));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kWeak,
                                         deoptimization_data));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kProperty,
                                         deoptimization_data));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kShortcut,
                                         deoptimization_data));

  // E3: SetInternalReference("source_position_table",
  //                          code->source_position_table(),
  //                          kPositionTableOffset).
  CHECK(HasExactNamedEdge(generator.get(), code, Edge::Type::kInternal,
                          "source_position_table", source_position_table));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), code,
                                   Edge::Type::kInternal,
                                   "source_position_table",
                                   source_position_table));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(code, Edge::Type::kInternal,
                                         source_position_table));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kHidden,
                                         source_position_table));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kWeak,
                                         source_position_table));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kProperty,
                                         source_position_table));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kShortcut,
                                         source_position_table));

  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kInternal,
                                         frame_translation));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kHidden,
                                         frame_translation));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kWeak,
                                         frame_translation));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kProperty,
                                         frame_translation));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kShortcut,
                                         frame_translation));

  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kInternal,
                                         literal_array));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kHidden,
                                         literal_array));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kWeak,
                                         literal_array));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kProperty,
                                         literal_array));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kShortcut,
                                         literal_array));

  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kInternal,
                                         inlining_positions));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kHidden,
                                         inlining_positions));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kWeak,
                                         inlining_positions));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kProperty,
                                         inlining_positions));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(code, Edge::Type::kShortcut,
                                         inlining_positions));

  CHECK_EQ(0, CountExactNamedEdges(generator.get(), code,
                                   Edge::Type::kInternal,
                                   "source_position_table",
                                   deoptimization_data));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), code,
                                   Edge::Type::kInternal,
                                   "deoptimization_data",
                                   source_position_table));
}

UNINITIALIZED_TEST(HeapDumpRegExpBoilerplateDescriptionTagsRegExpDataOnly) {
  Address boilerplate_address = 0;
  Address data_address = 0;
  Address source_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              const re = /heap-dump-regexp-boilerplate/g;
              re.exec('heap-dump-regexp-boilerplate');
              globalThis.__heap_dump_regexp_boilerplate_target = re;
            )");

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::RegExp> regexp =
                v8::Local<v8::RegExp>::Cast(
                    context->Global()
                        ->Get(context, NewApiString(
                                      v8_isolate,
                                      "__heap_dump_regexp_boilerplate_target"))
                        .ToLocalChecked());
            DirectHandle<JSRegExp> js_regexp =
                Cast<JSRegExp>(v8::Utils::OpenDirectHandle(*regexp));
            DirectHandle<String> source = direct_handle(js_regexp->source(), isolate);
            DirectHandle<RegExpData> regexp_data(js_regexp->data(isolate), isolate);
            DirectHandle<RegExpBoilerplateDescription> boilerplate =
                isolate->factory()->NewRegExpBoilerplateDescription(
                    regexp_data, source, Smi::FromInt(JSRegExp::kGlobal));

            is_expected_instance_type =
                REG_EXP_BOILERPLATE_DESCRIPTION_TYPE ==
                boilerplate->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(js_regexp);
            artifacts.keep_alive.push_back(regexp_data);
            artifacts.keep_alive.push_back(source);
            artifacts.keep_alive.push_back(boilerplate);
            InvokeAtomicMajorGC(isolate->heap());

            boilerplate_address = boilerplate->address();
            data_address = regexp_data->address();
            source_address = source->address();

            return artifacts;
          });

  CHECK(is_expected_instance_type);
  Node* boilerplate_node = GetSnapshotNode(generator.get(), boilerplate_address);
  Node* data_node = GetSnapshotNode(generator.get(), data_address);
  Node* source_node = GetSnapshotNode(generator.get(), source_address);
  CHECK_NOT_NULL(boilerplate_node);
  CHECK_NOT_NULL(data_node);
  CHECK_NOT_NULL(source_node);

  CHECK(NodeMatchesTypeAndName(generator.get(), boilerplate_node,
                               Node::Type::kCode,
                               "system / RegExpBoilerplateDescription"));
  CHECK(NodeHasType(data_node, Node::Type::kCode));

  // Object-specific named edges must not be emitted; tags are validated by node
  // existence/type plus body-descriptor hidden edges. The display name produced
  // by native TagObject is not part of this translate compatibility contract.
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), boilerplate_node,
                                   Edge::Type::kInternal, "data", data_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), boilerplate_node,
                                   Edge::Type::kInternal, "(RegExpData)",
                                   data_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), boilerplate_node,
                                   Edge::Type::kProperty, "data", data_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), boilerplate_node,
                                   Edge::Type::kWeak, "data", data_node));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(boilerplate_node, Edge::Type::kHidden,
                                         data_node));

  // Source field remains a regular body-descriptor hidden edge and is not retagged
  // as (RegExpData).
  CHECK_EQ(1, CountEdgesFromNodeToTarget(boilerplate_node, Edge::Type::kHidden,
                                         source_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), boilerplate_node,
                                   Edge::Type::kInternal, "source",
                                   source_node));
  CHECK(!NodeMatchesTypeAndName(generator.get(), source_node, Node::Type::kCode,
                               "(RegExpData)"));

  MarkCoveredInstanceType(REG_EXP_BOILERPLATE_DESCRIPTION_TYPE);
}

UNINITIALIZED_TEST(HeapDumpExtractReferencesBroadCoverage) {
  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            CreateBaselineObjects(context);
            CreateExtendedObjects(context);
            RunScript(context, R"(
              globalThis.__heap_dump_extract_ref = {};
              (function() {
                function f(a) { return a + 1; }
                function* g() { yield 1; }
                const weakKey = { key: 1 };
                const weakTarget = { target: 1 };
                globalThis.__heap_dump_extract_ref.bound =
                    f.bind({ x: 1 }, 3, { y: 2 });
                globalThis.__heap_dump_extract_ref.promise =
                    Promise.resolve({ done: false });
                globalThis.__heap_dump_extract_ref.generator = g();
                globalThis.__heap_dump_extract_ref.map =
                    new Map([[1, { deep: true }]]);
                globalThis.__heap_dump_extract_ref.set = new Set([1, 2, 3]);
                globalThis.__heap_dump_extract_ref.weakMap =
                    new WeakMap([[weakKey, { held: true }]]);
                globalThis.__heap_dump_extract_ref.weakSet =
                    new WeakSet([weakKey]);
                globalThis.__heap_dump_extract_ref.weakRef =
                    typeof WeakRef === 'function' ? new WeakRef(weakTarget) : null;
                globalThis.__heap_dump_extract_ref.arrayBuffer =
                    new ArrayBuffer(64);
                globalThis.__heap_dump_extract_ref.typed =
                    new Uint8Array(globalThis.__heap_dump_extract_ref.arrayBuffer);
                globalThis.__heap_dump_extract_ref.symbol = Symbol('extract-ref');
                globalThis.__heap_dump_extract_ref.regexp = /extract-ref/gi;
                globalThis.__heap_dump_extract_ref.object = {
                  get acc() { return 1; },
                  set acc(v) { this._v = v; },
                  nested: { ok: true }
                };
                globalThis.__heap_dump_extract_ref.array = [1, 2, 3, { x: 4 }];
                globalThis.__heap_dump_extract_ref.withClosure = (function() {
                  var ext = { p: 9 };
                  with (ext) {
                    return function() { return p; };
                  }
                })();
                globalThis.__heap_dump_extract_ref.evalClosure = (function() {
                  'use strict';
                  let q = 7;
                  return function() { return eval('q'); };
                })();
                globalThis.__heap_dump_extract_ref.disposable =
                    typeof DisposableStack === 'function'
                        ? new DisposableStack()
                        : null;

                if (typeof WebAssembly === 'object') {
                  const bytes = new Uint8Array([
                    0x00, 0x61, 0x73, 0x6d,
                    0x01, 0x00, 0x00, 0x00,
                    0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
                    0x03, 0x02, 0x01, 0x00,
                    0x07, 0x07, 0x01, 0x03, 0x6e, 0x6f, 0x70, 0x00, 0x00,
                    0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b
                  ]);
                  const module = new WebAssembly.Module(bytes);
                  const instance = new WebAssembly.Instance(module, {});
                  globalThis.__heap_dump_extract_ref.wasm = {
                    module,
                    instance,
                    memory: new WebAssembly.Memory({ initial: 1, maximum: 1 }),
                    table: new WebAssembly.Table({ initial: 1, element: 'anyfunc' })
                  };
                }
              })();
            )");

            DumpArtifacts artifacts;

            DirectHandle<JSObject> holder =
                isolate->factory()->NewJSObject(isolate->object_function());
            artifacts.keep_alive.push_back(holder);

            DirectHandle<InstructionStream> instruction_stream =
                CreateInstructionStreamForTest(isolate);
            artifacts.keep_alive.push_back(instruction_stream);

            Handle<Cell> cell = isolate->factory()->NewCell();
            cell->set_value(*holder);
            artifacts.keep_alive.push_back(direct_handle(*cell, isolate));

            DirectHandle<FeedbackCell> feedback_cell =
                isolate->factory()->NewManyClosuresCell();
            artifacts.keep_alive.push_back(feedback_cell);

            DirectHandle<String> prop_name =
                isolate->factory()->InternalizeUtf8String("extract_ref_prop");
            PropertyDetails details(PropertyKind::kData, NONE,
                                    PropertyCellType::kMutable);
            Handle<PropertyCell> property_cell =
                isolate->factory()->NewPropertyCell(prop_name, details, holder);
            artifacts.keep_alive.push_back(direct_handle(*property_cell, isolate));

            DirectHandle<AccessorInfo> accessor_info =
                isolate->factory()->NewAccessorInfo();
            accessor_info->set_name(*prop_name);
            accessor_info->set_data(*holder);
            artifacts.keep_alive.push_back(accessor_info);

            Handle<AccessorPair> accessor_pair =
                isolate->factory()->NewAccessorPair();
            accessor_pair->set_getter(*holder);
            accessor_pair->set_setter(*holder);
            artifacts.keep_alive.push_back(direct_handle(*accessor_pair, isolate));

            InvokeAtomicMajorGC(isolate->heap());
            return artifacts;
          });

  CHECK(HasString(generator.get(), "properties"));
  CHECK(HasString(generator.get(), "elements"));
  CHECK(HasString(generator.get(), "bindings"));
  CHECK(HasString(generator.get(), "bound_this"));
  CHECK(HasString(generator.get(), "bound_function"));
  CHECK(HasString(generator.get(), "feedback_cell"));
  CHECK(HasString(generator.get(), "shared"));
  CHECK(HasString(generator.get(), "context"));
  CHECK(HasString(generator.get(), "table"));
  CHECK(HasString(generator.get(), "reactions_or_result"));
  CHECK(HasString(generator.get(), "parameters_and_registers"));
  CHECK(HasString(generator.get(), "buffer"));
  CHECK(HasString(generator.get(), "scope_info"));
  CHECK(HasString(generator.get(), "previous"));
  CHECK(HasString(generator.get(), "global_object"));
  CHECK(HasString(generator.get(), "name_or_scope_info"));
  CHECK(HasString(generator.get(), "source"));
  CHECK(HasString(generator.get(), "line_ends"));
  CHECK_GE(
      CountPresentStrings(generator.get(), {"instruction_stream", "code"}), 1u);
  CHECK(HasString(generator.get(), "relocation_info"));
  CHECK(HasString(generator.get(), "value"));
  CHECK(HasString(generator.get(), "dependent_code"));
  CHECK(HasString(generator.get(), "constant_elements"));
  CHECK(HasString(generator.get(), "getter"));
  CHECK(HasString(generator.get(), "setter"));

#if V8_ENABLE_WEBASSEMBLY
  // Wasm object graphs vary by build/runtime feature set. Use a minimum-hit
  // threshold to keep this as a stable coverage test.
  CHECK_GE(CountPresentStrings(generator.get(),
                               {"module_object", "exports", "trusted_data",
                                "managed_native_module", "script"}),
           2u);
#endif
}

UNINITIALIZED_TEST(HeapDumpExtractReferencesSystemNodeCoverage) {
  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [](Isolate* isolate, v8::Local<v8::Context> context) {
            CreateBaselineObjects(context);
            CreateExtendedObjects(context);
            InvokeAtomicMajorGC(isolate->heap());
            return DumpArtifacts{};
          });

  CHECK(HasNode(generator.get(), Node::Type::kObjectShape, "system / EnumCache"));
  CHECK(
      HasNode(generator.get(), Node::Type::kObjectShape, "system / PrototypeInfo"));
  CHECK(HasNode(generator.get(), Node::Type::kObjectShape,
                "system / TransitionArray"));
  CHECK(HasNode(generator.get(), Node::Type::kCode, "system / ScopeInfo"));
  CHECK(HasNode(generator.get(), Node::Type::kCode, "system / BytecodeArray"));

  CHECK_GE(CountPresentStrings(generator.get(),
                               {"prototype_info", "transitions", "descriptors",
                                "prototype", "native_context"}),
           3u);
}

#else

UNINITIALIZED_TEST(HeapDumpExtractReferencesBroadCoverage) {}
UNINITIALIZED_TEST(HeapDumpExtractReferencesSystemNodeCoverage) {}
UNINITIALIZED_TEST(HeapDumpRegExpBoilerplateDescriptionTagsRegExpDataOnly) {}

#endif

}  // namespace heap
}  // namespace internal
}  // namespace v8
