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

// White-box cctest for OffheapTableResolver diagnostic paths.
//
// These tests build minimal synthetic rawheap binaries in-process and drive
// OffheapTableResolver directly without a live isolate.  They migrate the
// runtime-behaviour assertions from
// check_sandbox_offheap_placeholder_warning.py into the cctest acceptance gate.

#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-offheap-tables.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-offheap-tables.h"
#include "src/objects/code-inl.h"
#include "src/objects/js-function-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/literal-objects-inl.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/tests/cctest/heap-dump-test-utils.h"

namespace v8 {
namespace internal {
namespace heap {

#if defined(OH_ENABLE_HEAP_DUMP_TEST) && defined(OH_ENABLE_HEAP_TRANSLATE)

namespace {

using namespace heap_dump_test;

bool HasDiagnostic(const dfx::TranslateDiagnostics& diagnostics,
                   dfx::TranslateDiagnostics::Severity severity,
                   const char* code, const char* section) {
  for (const auto& entry : diagnostics.entries()) {
    if (entry.severity == severity && entry.code == code &&
        entry.section == section) {
      return true;
    }
  }
  return false;
}

// Only used inside V8_ENABLE_SANDBOX blocks where the message content matters.
#if defined(V8_ENABLE_SANDBOX)
bool HasDiagnosticMessageContaining(
    const dfx::TranslateDiagnostics& diagnostics,
    dfx::TranslateDiagnostics::Severity severity, const char* code,
    const char* section, const char* text) {
  for (const auto& entry : diagnostics.entries()) {
    if (entry.severity == severity && entry.code == code &&
        entry.section == section &&
        entry.message.find(text) != std::string::npos) {
      return true;
    }
  }
  return false;
}
#else

UNINITIALIZED_TEST(HeapDumpNoSandboxCodeWrapperCodeHiddenEdge) {
  i::Address wrapper_addr = 0;
  i::Address code_addr = 0;

  auto generator = DumpAndTranslateWithIsolateTeardown(
      [&](i::Isolate* isolate, v8::Local<v8::Context> context) {
        RunScript(context, R"(
          function HeapDumpNoSandboxCodeWrapperTarget(x) { return x + 2; }
          globalThis.__heap_dump_no_sandbox_code_wrapper_target =
              HeapDumpNoSandboxCodeWrapperTarget;
        )");

        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        v8::Local<v8::Function> fn = v8::Local<v8::Function>::Cast(
            context->Global()
                ->Get(context,
                      NewApiString(v8_isolate,
                                   "__heap_dump_no_sandbox_code_wrapper_target"))
                .ToLocalChecked());
        i::DirectHandle<i::JSFunction> js_function =
            i::Cast<i::JSFunction>(v8::Utils::OpenDirectHandle(*fn));
        i::DirectHandle<i::Code> code(js_function->code(isolate), isolate);
        i::DirectHandle<i::CodeWrapper> wrapper(code->wrapper(), isolate);

        CHECK_EQ(i::CODE_WRAPPER_TYPE, wrapper->map()->instance_type());
        wrapper_addr = wrapper->address();
        code_addr = wrapper->code(isolate).address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(wrapper);
        artifacts.keep_alive.push_back(code);
        artifacts.keep_alive.push_back(js_function);
        return artifacts;
      });

  const dfx::Node* wrapper_node = GetSnapshotNode(generator.get(), wrapper_addr);
  const dfx::Node* code_node = GetSnapshotNode(generator.get(), code_addr);
  CHECK_NOT_NULL(wrapper_node);
  CHECK_NOT_NULL(code_node);

  CHECK_LT(0, CountEdgesFromNodeToTarget(wrapper_node,
                                         dfx::Edge::Type::kHidden, code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), wrapper_node,
                                   dfx::Edge::Type::kInternal, "code",
                                   code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), wrapper_node,
                                   dfx::Edge::Type::kProperty, "code",
                                   code_node));
}

UNINITIALIZED_TEST(HeapDumpNoSandboxRegExpBoilerplateDataHiddenEdge) {
  i::Address boilerplate_addr = 0;
  i::Address data_addr = 0;
  i::Address source_addr = 0;

  auto generator = DumpAndTranslateWithIsolateTeardown(
      [&](i::Isolate* isolate, v8::Local<v8::Context> context) {
        RunScript(context, R"(
          const re = /heap-(dump)-no-sandbox-(boilerplate)/g;
          re.exec('heap-dump-no-sandbox-boilerplate');
          globalThis.__heap_dump_no_sandbox_boilerplate_target = re;
        )");

        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        v8::Local<v8::RegExp> regexp = v8::Local<v8::RegExp>::Cast(
            context->Global()
                ->Get(context,
                      NewApiString(v8_isolate,
                                   "__heap_dump_no_sandbox_boilerplate_target"))
                .ToLocalChecked());
        i::DirectHandle<i::JSRegExp> js_regexp =
            i::Cast<i::JSRegExp>(v8::Utils::OpenDirectHandle(*regexp));
        i::DirectHandle<i::RegExpData> regexp_data(
            js_regexp->data(isolate), isolate);
        i::DirectHandle<i::String> source(js_regexp->source(), isolate);
        i::DirectHandle<i::RegExpBoilerplateDescription> boilerplate =
            isolate->factory()->NewRegExpBoilerplateDescription(
                regexp_data, source, i::Smi::FromInt(i::JSRegExp::kGlobal));

        boilerplate_addr = boilerplate->address();
        data_addr = regexp_data->address();
        source_addr = source->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(js_regexp);
        artifacts.keep_alive.push_back(regexp_data);
        artifacts.keep_alive.push_back(source);
        artifacts.keep_alive.push_back(boilerplate);
        return artifacts;
      });

  const dfx::Node* boilerplate_node =
      GetSnapshotNode(generator.get(), boilerplate_addr);
  const dfx::Node* data_node = GetSnapshotNode(generator.get(), data_addr);
  const dfx::Node* source_node = GetSnapshotNode(generator.get(), source_addr);
  CHECK_NOT_NULL(boilerplate_node);
  CHECK_NOT_NULL(data_node);
  CHECK_NOT_NULL(source_node);

  CHECK(NodeHasType(data_node, dfx::Node::Type::kCode));
  CHECK_LT(0, CountEdgesFromNodeToTarget(boilerplate_node,
                                         dfx::Edge::Type::kHidden, data_node));
  CHECK_LT(0, CountEdgesFromNodeToTarget(boilerplate_node,
                                         dfx::Edge::Type::kHidden,
                                         source_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), boilerplate_node,
                                   dfx::Edge::Type::kInternal, "data",
                                   data_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), boilerplate_node,
                                   dfx::Edge::Type::kProperty, "data",
                                   data_node));
}

UNINITIALIZED_TEST(HeapDumpNoSandboxIrRegExpDataCodeHiddenEdge) {
  i::Address data_addr = 0;
  i::Address code_addr = 0;
  bool has_latin1_code = false;

  auto generator = DumpAndTranslateWithIsolateTeardown(
      [&](i::Isolate* isolate, v8::Local<v8::Context> context) {
        RunScript(context, R"(
          const re = /heap-(dump)-no-sandbox-(code)/g;
          re.exec('heap-dump-no-sandbox-code');
          globalThis.__heap_dump_no_sandbox_irregexp_target = re;
        )");

        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        v8::Local<v8::RegExp> regexp = v8::Local<v8::RegExp>::Cast(
            context->Global()
                ->Get(context,
                      NewApiString(v8_isolate,
                                   "__heap_dump_no_sandbox_irregexp_target"))
                .ToLocalChecked());
        i::DirectHandle<i::JSRegExp> js_regexp =
            i::Cast<i::JSRegExp>(v8::Utils::OpenDirectHandle(*regexp));
        i::Tagged<i::IrRegExpData> irregexp_data =
            i::TrustedCast<i::IrRegExpData>(js_regexp->data(isolate));

        CHECK_EQ(i::IR_REG_EXP_DATA_TYPE,
                 irregexp_data->map()->instance_type());
        has_latin1_code = irregexp_data->has_latin1_code();
        if (!has_latin1_code) {
          return DumpArtifacts{};
        }

        i::Tagged<i::Code> code = irregexp_data->latin1_code(isolate);
        data_addr = irregexp_data->address();
        code_addr = code->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(js_regexp);
        artifacts.keep_alive.push_back(
            i::direct_handle(i::Tagged<i::RegExpData>(irregexp_data),
                             isolate));
        artifacts.keep_alive.push_back(i::direct_handle(code, isolate));
        artifacts.keep_alive.push_back(
            i::direct_handle(irregexp_data->wrapper(), isolate));
        return artifacts;
      });

  CHECK(has_latin1_code);
  const dfx::Node* data_node = GetSnapshotNode(generator.get(), data_addr);
  const dfx::Node* code_node = GetSnapshotNode(generator.get(), code_addr);
  CHECK_NOT_NULL(data_node);
  CHECK_NOT_NULL(code_node);

  CHECK_LT(0, CountEdgesFromNodeToTarget(data_node, dfx::Edge::Type::kHidden,
                                         code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), data_node,
                                   dfx::Edge::Type::kInternal, "code",
                                   code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), data_node,
                                   dfx::Edge::Type::kProperty, "code",
                                   code_node));
}

#endif  // V8_ENABLE_SANDBOX

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

struct SyntheticSingleEntryOffheapTable {
  dfx::RawHeapHeader header{};
  std::vector<uint8_t> bytes;
};

struct SyntheticOffheapTable {
  dfx::RawHeapHeader header{};
  std::vector<uint8_t> bytes;
};

// Builds a minimal rawheap binary containing exactly one offheap table
// directory entry of the given numeric kind and an empty payload.
// Unsupported and unknown kinds trigger early return in ValidateAndStoreTable
// before any payload-range validation, so an empty payload is safe.
SyntheticSingleEntryOffheapTable BuildSingleEntryOffheapTable(uint32_t kind) {
  SyntheticSingleEntryOffheapTable result;
  // Offheap section starts immediately after the rawheap header.
  result.header.offheap_table_offset_ = sizeof(dfx::RawHeapHeader);
  // Section contains only the single directory entry; no payload bytes.
  result.header.offheap_table_size_ = sizeof(dfx::RawHeapOffheapTableHeader);
  result.header.offheap_table_count_ = 1;

  // offset_ is relative to the section start; points past the directory entry.
  // size_ and count_ are 0: the kind will be rejected before payload reads.
  dfx::RawHeapOffheapTableHeader table_header{
      kind, sizeof(dfx::RawHeapOffheapTableHeader), 0u, 0u};

  result.bytes.resize(sizeof(dfx::RawHeapHeader));
  WritePodAt(&result.bytes, 0, result.header);
  AppendPod(&result.bytes, table_header);
  return result;
}

dfx::OffheapTableResolver BuildResolver(
    const SyntheticSingleEntryOffheapTable& synth,
    dfx::TranslateDiagnostics* diagnostics) {
  dfx::BinaryReaderTest reader(const_cast<uint8_t*>(synth.bytes.data()),
                               static_cast<uint32_t>(synth.bytes.size()));
  return dfx::OffheapTableResolver(&reader, synth.header, diagnostics);
}

dfx::OffheapTableResolver BuildResolver(
    const SyntheticOffheapTable& synth,
    dfx::TranslateDiagnostics* diagnostics) {
  dfx::BinaryReaderTest reader(const_cast<uint8_t*>(synth.bytes.data()),
                               static_cast<uint32_t>(synth.bytes.size()));
  return dfx::OffheapTableResolver(&reader, synth.header, diagnostics);
}

[[maybe_unused]] SyntheticOffheapTable BuildOffheapTableFromProducer(
    dfx::BinaryWriterTest& writer, uint32_t table_count) {
  SyntheticOffheapTable result;
  result.header.offheap_table_offset_ = sizeof(dfx::RawHeapHeader);
  result.header.offheap_table_size_ = writer.Size();
  result.header.offheap_table_count_ = table_count;
  result.header.code_cage_base_ = 0x75000000u;

  result.bytes.resize(sizeof(dfx::RawHeapHeader));
  WritePodAt(&result.bytes, 0, result.header);
  const uint8_t* data = writer.Data();
  result.bytes.insert(result.bytes.end(), data, data + writer.Size());
  return result;
}

struct SyntheticTablePayload {
  uint32_t kind{0};
  std::vector<uint8_t> payload;
  uint32_t count{0};
};

SyntheticOffheapTable BuildOffheapTable(
    const std::vector<SyntheticTablePayload>& tables) {
  SyntheticOffheapTable result;
  result.header.offheap_table_offset_ = sizeof(dfx::RawHeapHeader);
  result.header.offheap_table_count_ = static_cast<uint32_t>(tables.size());

  const uint32_t directory_size =
      result.header.offheap_table_count_ *
      static_cast<uint32_t>(sizeof(dfx::RawHeapOffheapTableHeader));
  uint32_t payload_cursor = directory_size;

  result.bytes.resize(sizeof(dfx::RawHeapHeader));
  for (const auto& table : tables) {
    dfx::RawHeapOffheapTableHeader table_header{
        table.kind, payload_cursor, static_cast<uint32_t>(table.payload.size()),
        table.count};
    AppendPod(&result.bytes, table_header);
    payload_cursor += static_cast<uint32_t>(table.payload.size());
  }

  for (const auto& table : tables) {
    result.bytes.insert(result.bytes.end(), table.payload.begin(),
                        table.payload.end());
  }

  result.header.offheap_table_size_ = payload_cursor;
  WritePodAt(&result.bytes, 0, result.header);
  return result;
}

template <typename T>
std::vector<uint8_t> PodVector(const T& value) {
  std::vector<uint8_t> bytes;
  AppendPod(&bytes, value);
  return bytes;
}

#if defined(V8_ENABLE_SANDBOX)
// Helper: dump once, translate twice (graph pass + diagnostics pass), return
// both.  Two separate HeapTranslator runs on identical bytes avoid exposing
// diagnostics through SnapshotGenerator's public API surface.
template <typename PrepareFn>
std::pair<std::unique_ptr<SnapshotGenerator>, TranslateDiagnostics>
DumpTranslateCaptureDiagnostics(PrepareFn prepare) {
  auto bytes = DumpWithIsolateTeardown(std::move(prepare));
  auto generator = TranslateHeapBytes(bytes);
  TranslateDiagnostics diagnostics;
  TryTranslateHeapBytes(bytes, &diagnostics);
  return {std::move(generator), std::move(diagnostics)};
}
#endif  // V8_ENABLE_SANDBOX

}  // namespace

#if defined(V8_ENABLE_SANDBOX)
UNINITIALIZED_TEST(HeapDumpDumpsOffheapAndExternalCodeSpacePayloads) {
  static_assert(!std::is_same<dfx::RawHeapSandboxCodePointerEntry,
                              dfx::RawHeapSandboxTrustedPointerEntry>::value,
                "CodePointerTable must not reuse TrustedPointer schema");

  dfx::RawHeapSandboxTrustedPointerEntry trusted_entry{};
  trusted_entry.handle_ = 0x10000001u;
  trusted_entry.indirect_pointer_tag_ = dfx::kRawHeapUnknownIndirectPointerTag;
  trusted_entry.trusted_object_address_ = 0x71000000u;
  trusted_entry.state_ = dfx::kRawHeapSandboxTrustedPointerEntryActive;

  dfx::RawHeapSandboxJSDispatchEntry js_dispatch_entry{};
  js_dispatch_entry.handle_ = 0x20000002u;
  js_dispatch_entry.state_ = dfx::kRawHeapSandboxJSDispatchEntryActive;
  js_dispatch_entry.code_object_address_ = 0x72000000u;
  js_dispatch_entry.entrypoint_ = 0x73000000u;
  js_dispatch_entry.parameter_count_ = 3u;

  dfx::RawHeapSandboxCodePointerEntry code_pointer_entry{};
  code_pointer_entry.handle_ = 0x30000003u;
  code_pointer_entry.flags_ = 0u;
  code_pointer_entry.code_object_address_ = 0x74000000u;
  code_pointer_entry.entrypoint_ = 0u;

  SyntheticOffheapTable synth = BuildOffheapTable({
      {static_cast<uint32_t>(
           dfx::RawHeapOffheapTableKind::kSandboxTrustedPointerTable),
       PodVector(trusted_entry),
       1u},
      {static_cast<uint32_t>(
           dfx::RawHeapOffheapTableKind::kSandboxJSDispatchTable),
       PodVector(js_dispatch_entry),
       1u},
      {static_cast<uint32_t>(
           dfx::RawHeapOffheapTableKind::kSandboxCodePointerTable),
       PodVector(code_pointer_entry),
       1u},
  });

  dfx::TranslateDiagnostics diagnostics;
  auto resolver = BuildResolver(synth, &diagnostics);

  CHECK(resolver.IsValid());
  CHECK(!diagnostics.has_fatal());
  CHECK(resolver.HasTable(
      dfx::RawHeapOffheapTableKind::kSandboxTrustedPointerTable));
  CHECK(
      resolver.HasTable(dfx::RawHeapOffheapTableKind::kSandboxJSDispatchTable));
  CHECK(resolver.HasTable(
      dfx::RawHeapOffheapTableKind::kSandboxCodePointerTable));
  CHECK(!resolver.HasTable(
      dfx::RawHeapOffheapTableKind::kSandboxExternalPointerTable));
  CHECK(!resolver.HasTable(
      dfx::RawHeapOffheapTableKind::kSandboxCppHeapPointerTable));

  const dfx::RawHeapSandboxTrustedPointerEntry* trusted =
      resolver.FindTrustedPointer(trusted_entry.handle_,
                                  dfx::kRawHeapUnknownIndirectPointerTag);
  CHECK_NOT_NULL(trusted);
  CHECK_EQ(trusted_entry.trusted_object_address_,
           trusted->trusted_object_address_);

  const dfx::RawHeapSandboxJSDispatchEntry* js_dispatch =
      resolver.FindJSDispatchCode(js_dispatch_entry.handle_);
  CHECK_NOT_NULL(js_dispatch);
  CHECK_EQ(js_dispatch_entry.code_object_address_,
           js_dispatch->code_object_address_);
  CHECK_EQ(js_dispatch_entry.entrypoint_, js_dispatch->entrypoint_);

  const dfx::RawHeapSandboxCodePointerEntry* code_pointer =
      resolver.FindCodePointer(code_pointer_entry.handle_);
  CHECK_NOT_NULL(code_pointer);
  CHECK_EQ(code_pointer_entry.code_object_address_,
           code_pointer->code_object_address_);
  CHECK_EQ(0u, code_pointer->entrypoint_);

  // The live producer path may legitimately have zero active entries in a
  // small cctest isolate. Still exercise PreVisit()+Dump() so regressions in
  // table size/count accounting are caught outside the synthetic seed helper.
  USE(DumpWithIsolateTeardown(
      [](Isolate* isolate, v8::Local<v8::Context> context) {
        USE(context);
        dfx::BinaryWriterTest live_writer;
        dfx::OffheapTableDumper live_dumper(isolate->heap(), &live_writer);
        live_dumper.PreVisit();
        live_dumper.Dump();
        CHECK_EQ(live_dumper.OffheapTableDumpSize(), live_writer.Size());
        return DumpArtifacts{};
      }));

  // ExternalPointer / CppHeapPointer remain native-resource diagnostics in
  // this dump-first slice. Zero-count unsupported directory entries must be
  // accepted without becoming HeapObject edge producers.
  SyntheticOffheapTable unsupported_synth = BuildOffheapTable({
      {static_cast<uint32_t>(
           dfx::RawHeapOffheapTableKind::kSandboxExternalPointerTable),
       {},
       0u},
      {static_cast<uint32_t>(
           dfx::RawHeapOffheapTableKind::kSandboxCppHeapPointerTable),
       {},
       0u},
  });
  dfx::TranslateDiagnostics unsupported_diagnostics;
  auto unsupported_resolver =
      BuildResolver(unsupported_synth, &unsupported_diagnostics);
  CHECK(unsupported_resolver.IsValid());
  CHECK(HasDiagnostic(
      unsupported_diagnostics, dfx::TranslateDiagnostics::Severity::kWarning,
      "sandbox_offheap_table_decoder_unsupported", "offheap_table"));
}
#else
UNINITIALIZED_TEST(HeapDumpDumpsOffheapAndExternalCodeSpacePayloads) {
  SyntheticOffheapTable synth = BuildOffheapTable({
      {100u, {}, 0u},
      {101u, {}, 0u},
      {102u, {}, 0u},
      {103u, {}, 0u},
      {104u, {}, 0u},
  });
  synth.header.code_cage_base_ = 0x75000000u;
  WritePodAt(&synth.bytes, 0, synth.header);

  dfx::TranslateDiagnostics diagnostics;
  auto resolver = BuildResolver(synth, &diagnostics);

  CHECK(resolver.IsValid());
  CHECK(!diagnostics.has_fatal());
  CHECK(HasDiagnostic(diagnostics,
                      dfx::TranslateDiagnostics::Severity::kWarning,
                      "offheap_table_unknown_kind", "offheap_table"));
}
#endif  // V8_ENABLE_SANDBOX

// V8_ENABLE_SANDBOX: kSandboxExternalPointerTable (101) must reach
// ReportSandboxUnsupportedTableKind and emit
// sandbox_offheap_table_decoder_unsupported. IsValid() must remain true —
// graceful degradation, not a fatal error.
#if defined(V8_ENABLE_SANDBOX)
UNINITIALIZED_TEST(HeapDumpOffheapTableResolverSandboxPlaceholderWarning) {
  constexpr uint32_t kKind = static_cast<uint32_t>(
      dfx::RawHeapOffheapTableKind::kSandboxExternalPointerTable);
  auto synth = BuildSingleEntryOffheapTable(kKind);
  dfx::TranslateDiagnostics diagnostics;
  auto resolver = BuildResolver(synth, &diagnostics);

  // Placeholder kind must not invalidate the resolver.
  CHECK(resolver.IsValid());
  // Exactly the centralised warning code for placeholder sandbox kinds.
  CHECK(HasDiagnostic(
      diagnostics, dfx::TranslateDiagnostics::Severity::kWarning,
      "sandbox_offheap_table_decoder_unsupported", "offheap_table"));
  // Warning message must include the numeric kind value so it is actionable.
  CHECK(HasDiagnosticMessageContaining(
      diagnostics, dfx::TranslateDiagnostics::Severity::kWarning,
      "sandbox_offheap_table_decoder_unsupported", "offheap_table",
      std::to_string(kKind).c_str()));
  CHECK(!diagnostics.has_fatal());
}
#endif  // V8_ENABLE_SANDBOX

// No-sandbox: numeric value 101 (kSandboxExternalPointerTable in sandbox
// builds) is not recognised by IsSandboxUnsupportedTableKind (not compiled) and
// not in IsSupportedTableKind's switch, so it falls through to
// offheap_table_unknown_kind. Validates that the no-sandbox path also degrades
// gracefully for this value.
#if !defined(V8_ENABLE_SANDBOX)
UNINITIALIZED_TEST(HeapDumpOffheapTableResolverSandboxKindFallbackNoSandbox) {
  // Numeric value that corresponds to kSandboxExternalPointerTable under
  // V8_ENABLE_SANDBOX; no-sandbox builds treat it as a generic unknown kind.
  constexpr uint32_t kSandboxKindValue = 101u;
  auto synth = BuildSingleEntryOffheapTable(kSandboxKindValue);
  dfx::TranslateDiagnostics diagnostics;
  auto resolver = BuildResolver(synth, &diagnostics);

  CHECK(resolver.IsValid());
  CHECK(HasDiagnostic(diagnostics,
                      dfx::TranslateDiagnostics::Severity::kWarning,
                      "offheap_table_unknown_kind", "offheap_table"));
  CHECK(!diagnostics.has_fatal());
}
#endif  // !V8_ENABLE_SANDBOX

// Both builds: a completely unrecognised kind (200) must emit
// offheap_table_unknown_kind without crashing and leave IsValid() == true.
UNINITIALIZED_TEST(HeapDumpOffheapTableResolverUnknownKindWarning) {
  constexpr uint32_t kUnknownKind = 200u;
  auto synth = BuildSingleEntryOffheapTable(kUnknownKind);
  dfx::TranslateDiagnostics diagnostics;
  auto resolver = BuildResolver(synth, &diagnostics);

  CHECK(resolver.IsValid());
  CHECK(HasDiagnostic(diagnostics,
                      dfx::TranslateDiagnostics::Severity::kWarning,
                      "offheap_table_unknown_kind", "offheap_table"));
  CHECK(!diagnostics.has_fatal());
}

UNINITIALIZED_TEST(HeapDumpOffheapTableResolverJSDispatchTablePresent) {
  dfx::RawHeapJSDispatchEntry js_dispatch_entry{};
  js_dispatch_entry.handle_ = 0x20000002u;
  js_dispatch_entry.state_ = dfx::kRawHeapJSDispatchEntryActive;
  js_dispatch_entry.code_object_address_ = 0x72000000u;
  js_dispatch_entry.entrypoint_ = 0x73000000u;
  js_dispatch_entry.parameter_count_ = 3u;

  SyntheticOffheapTable synth = BuildOffheapTable({
      {static_cast<uint32_t>(dfx::RawHeapOffheapTableKind::kJSDispatchTable),
       PodVector(js_dispatch_entry),
       1u},
  });

  dfx::TranslateDiagnostics diagnostics;
  auto resolver = BuildResolver(synth, &diagnostics);

  CHECK(resolver.IsValid());
  CHECK(!diagnostics.has_fatal());
  CHECK(resolver.HasTable(dfx::RawHeapOffheapTableKind::kJSDispatchTable));

  const dfx::RawHeapJSDispatchEntry* js_dispatch =
      resolver.FindJSDispatchCode(js_dispatch_entry.handle_);
  CHECK_NOT_NULL(js_dispatch);
  CHECK_EQ(js_dispatch_entry.code_object_address_,
           js_dispatch->code_object_address_);
  CHECK_EQ(js_dispatch_entry.entrypoint_, js_dispatch->entrypoint_);
}

UNINITIALIZED_TEST(HeapDumpOffheapTableResolverJSDispatchTableMissingWarning) {
  SyntheticOffheapTable synth = BuildOffheapTable({});
  dfx::TranslateDiagnostics diagnostics;
  auto resolver = BuildResolver(synth, &diagnostics);

  CHECK(resolver.IsValid());
  CHECK_NULL(resolver.FindJSDispatchCode(0x20000002u));
  CHECK(HasDiagnostic(diagnostics,
                      dfx::TranslateDiagnostics::Severity::kWarning,
                      "js_dispatch_table_missing", "offheap_table"));
  CHECK(!diagnostics.has_fatal());
}

// Verifies that VisitJSDispatchTableEntry emits a named "code" internal edge
// from a JSFunction node to its Code object via the JSDispatch offheap table.
// This is a white-box translate-level test: it exercises the full
// dump-to-translate pipeline with a real JSDispatch table so that any
// regression in the dispatch-table edge logic is caught here.
UNINITIALIZED_TEST(HeapDumpOffheapTranslateJSDispatchCodeEdge) {
  i::Address fn_addr = 0;
  i::Address code_addr = 0;

  auto generator = DumpAndTranslateWithIsolateTeardown(
      [&](i::Isolate* isolate, v8::Local<v8::Context> context) {
        RunScript(context, R"(
          function HeapDumpOffheapDispatchCodeTarget() { return 42; }
          globalThis.__heap_dump_offheap_dispatch_code_target =
              HeapDumpOffheapDispatchCodeTarget;
        )");

        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        v8::Local<v8::Function> fn = v8::Local<v8::Function>::Cast(
            context->Global()
                ->Get(context,
                      NewApiString(v8_isolate,
                                   "__heap_dump_offheap_dispatch_code_target"))
                .ToLocalChecked());
        i::DirectHandle<i::JSFunction> js_function =
            i::Cast<i::JSFunction>(v8::Utils::OpenDirectHandle(*fn));
        i::DirectHandle<i::Code> code(js_function->code(isolate), isolate);
        fn_addr = js_function->address();
        code_addr = code->address();
        return DumpArtifacts{};
      });

  // The JSFunction node must have a named "code" internal edge added by
  // VisitJSDispatchTableEntry -> FindJSDispatchCode -> SetInternalReference.
  const dfx::Node* fn_node = GetSnapshotNode(generator.get(), fn_addr);
  const dfx::Node* code_node = GetSnapshotNode(generator.get(), code_addr);
  CHECK_NOT_NULL(fn_node);
  CHECK_NOT_NULL(code_node);
  CHECK(HasExactNamedEdge(generator.get(), fn_node, dfx::Edge::Type::kInternal,
                          "code", code_node));
}

#if defined(V8_ENABLE_SANDBOX)
// CodeWrapper::code is visited by the native generic IndexedReferencesExtractor
// through IterateCodePointer.  The offline translator should resolve the
// dumped CodePointer handle and emit the same hidden/indexed edge, not a
// fabricated named "code" edge.
UNINITIALIZED_TEST(HeapDumpOffheapTranslateCodeWrapperCodePointerHiddenEdge) {
  i::Address wrapper_addr = 0;
  i::Address code_addr = 0;
  bool is_expected_instance_type = false;

  auto generator = DumpAndTranslateWithIsolateTeardown(
      [&](i::Isolate* isolate, v8::Local<v8::Context> context) {
        RunScript(context, R"(
          function HeapDumpOffheapCodeWrapperTarget(x) { return x + 1; }
          globalThis.__heap_dump_offheap_code_wrapper_target =
              HeapDumpOffheapCodeWrapperTarget;
        )");

        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        v8::Local<v8::Function> fn = v8::Local<v8::Function>::Cast(
            context->Global()
                ->Get(context,
                      NewApiString(v8_isolate,
                                   "__heap_dump_offheap_code_wrapper_target"))
                .ToLocalChecked());
        i::DirectHandle<i::JSFunction> js_function =
            i::Cast<i::JSFunction>(v8::Utils::OpenDirectHandle(*fn));
        i::DirectHandle<i::Code> code(js_function->code(isolate), isolate);
        i::DirectHandle<i::CodeWrapper> wrapper(code->wrapper(), isolate);

        is_expected_instance_type =
            i::CODE_WRAPPER_TYPE == wrapper->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        wrapper_addr = wrapper->address();
        code_addr = wrapper->code(isolate).address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(wrapper);
        artifacts.keep_alive.push_back(code);
        artifacts.keep_alive.push_back(js_function);
        return artifacts;
      });

  const dfx::Node* wrapper_node = GetSnapshotNode(generator.get(), wrapper_addr);
  const dfx::Node* code_node = GetSnapshotNode(generator.get(), code_addr);
  // CODE_WRAPPER_TYPE must be stable; a mismatch here is a V8 regression.
  CHECK(is_expected_instance_type);
  // wrapper and code were kept alive by keep_alive; both must appear in the snapshot.
  CHECK_NOT_NULL(wrapper_node);
  CHECK_NOT_NULL(code_node);

  CHECK_LT(0, CountEdgesFromNodeToTarget(wrapper_node,
                                         dfx::Edge::Type::kHidden, code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), wrapper_node,
                                   dfx::Edge::Type::kInternal, "code",
                                   code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), wrapper_node,
                                   dfx::Edge::Type::kProperty, "code",
                                   code_node));
}

UNINITIALIZED_TEST(
    HeapDumpOffheapTranslateRegExpBoilerplateTrustedDataHiddenEdge) {
  i::Address boilerplate_addr = 0;
  i::Address data_addr = 0;
  i::Address source_addr = 0;
  bool is_expected_instance_type = false;

  auto generator = DumpAndTranslateWithIsolateTeardown(
      [&](i::Isolate* isolate, v8::Local<v8::Context> context) {
        RunScript(context, R"(
          const re = /heap-(dump)-offheap-(boilerplate)/g;
          re.exec('heap-dump-offheap-boilerplate');
          globalThis.__heap_dump_offheap_boilerplate_target = re;
        )");

        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        v8::Local<v8::RegExp> regexp = v8::Local<v8::RegExp>::Cast(
            context->Global()
                ->Get(context,
                      NewApiString(v8_isolate,
                                   "__heap_dump_offheap_boilerplate_target"))
                .ToLocalChecked());
        i::DirectHandle<i::JSRegExp> js_regexp =
            i::Cast<i::JSRegExp>(v8::Utils::OpenDirectHandle(*regexp));
        i::DirectHandle<i::RegExpData> regexp_data(
            js_regexp->data(isolate), isolate);
        i::DirectHandle<i::String> source(js_regexp->source(), isolate);
        i::DirectHandle<i::RegExpBoilerplateDescription> boilerplate =
            isolate->factory()->NewRegExpBoilerplateDescription(
                regexp_data, source, i::Smi::FromInt(i::JSRegExp::kGlobal));

        i::InstanceType data_type = regexp_data->map()->instance_type();
        is_expected_instance_type =
            data_type == i::REG_EXP_DATA_TYPE ||
            data_type == i::ATOM_REG_EXP_DATA_TYPE ||
            data_type == i::IR_REG_EXP_DATA_TYPE;
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        boilerplate_addr = boilerplate->address();
        data_addr = regexp_data->address();
        source_addr = source->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(js_regexp);
        artifacts.keep_alive.push_back(regexp_data);
        artifacts.keep_alive.push_back(source);
        artifacts.keep_alive.push_back(boilerplate);
        return artifacts;
      });

  const dfx::Node* boilerplate_node =
      GetSnapshotNode(generator.get(), boilerplate_addr);
  const dfx::Node* data_node = GetSnapshotNode(generator.get(), data_addr);
  const dfx::Node* source_node = GetSnapshotNode(generator.get(), source_addr);
  CHECK(is_expected_instance_type);
  CHECK_NOT_NULL(boilerplate_node);
  CHECK_NOT_NULL(data_node);
  CHECK_NOT_NULL(source_node);

  // RegExpBoilerplateDescription::data is a TrustedPointer slot in sandbox.
  // Native BodyDescriptor traversal emits a hidden/indexed edge to RegExpData;
  // offline translate must resolve the dumped TrustedPointerTable handle rather
  // than treating the handle/table index as an address or fabricating a named
  // "data" edge.
  CHECK(NodeHasType(data_node, dfx::Node::Type::kCode));
  CHECK_LT(0, CountEdgesFromNodeToTarget(boilerplate_node,
                                         dfx::Edge::Type::kHidden, data_node));
  CHECK_LT(0, CountEdgesFromNodeToTarget(boilerplate_node,
                                         dfx::Edge::Type::kHidden,
                                         source_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), boilerplate_node,
                                   dfx::Edge::Type::kInternal, "data",
                                   data_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), boilerplate_node,
                                   dfx::Edge::Type::kProperty, "data",
                                   data_node));
}

// IrRegExpData::{latin1_code,uc16_code} uses IterateCodePointer in the native
// body descriptor.  The offline translator should therefore reuse the generic
// CodePointer hidden/indexed edge path after object-table membership and
// CODE_TYPE validation.
UNINITIALIZED_TEST(HeapDumpOffheapTranslateIrRegExpDataCodePointerHiddenEdge) {
  i::Address data_addr = 0;
  i::Address code_addr = 0;
  bool is_expected_instance_type = false;
  bool has_latin1_code = false;

  auto generator = DumpAndTranslateWithIsolateTeardown(
      [&](i::Isolate* isolate, v8::Local<v8::Context> context) {
        RunScript(context, R"(
          const re = /heap-(dump)-offheap-(code)/g;
          re.exec('heap-dump-offheap-code');
          globalThis.__heap_dump_offheap_irregexp_target = re;
        )");

        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        v8::Local<v8::RegExp> regexp = v8::Local<v8::RegExp>::Cast(
            context->Global()
                ->Get(context,
                      NewApiString(v8_isolate,
                                   "__heap_dump_offheap_irregexp_target"))
                .ToLocalChecked());
        i::DirectHandle<i::JSRegExp> js_regexp =
            i::Cast<i::JSRegExp>(v8::Utils::OpenDirectHandle(*regexp));
        i::Tagged<i::RegExpData> regexp_data = js_regexp->data(isolate);
        i::Tagged<i::IrRegExpData> irregexp_data =
            i::TrustedCast<i::IrRegExpData>(regexp_data);

        is_expected_instance_type =
            i::IR_REG_EXP_DATA_TYPE == irregexp_data->map()->instance_type();
        has_latin1_code = irregexp_data->has_latin1_code();
        if (!is_expected_instance_type || !has_latin1_code) {
          return DumpArtifacts{};
        }

        i::Tagged<i::Code> code = irregexp_data->latin1_code(isolate);
        data_addr = irregexp_data->address();
        code_addr = code->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(js_regexp);
        artifacts.keep_alive.push_back(
            i::direct_handle(i::Tagged<i::RegExpData>(irregexp_data),
                             isolate));
        artifacts.keep_alive.push_back(i::direct_handle(code, isolate));
        artifacts.keep_alive.push_back(
            i::direct_handle(irregexp_data->wrapper(), isolate));
        return artifacts;
      });

  const dfx::Node* data_node = GetSnapshotNode(generator.get(), data_addr);
  const dfx::Node* code_node = GetSnapshotNode(generator.get(), code_addr);
  // This fixture must compile latin1_code; otherwise the hidden edge was not
  // actually exercised and the test should fail instead of soft-passing.
  CHECK(has_latin1_code);
  CHECK(is_expected_instance_type);
  // data and code were kept alive; both must appear in the snapshot.
  CHECK_NOT_NULL(data_node);
  CHECK_NOT_NULL(code_node);

  CHECK_LT(0, CountEdgesFromNodeToTarget(data_node, dfx::Edge::Type::kHidden,
                                         code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), data_node,
                                   dfx::Edge::Type::kInternal, "code",
                                   code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), data_node,
                                   dfx::Edge::Type::kProperty, "code",
                                   code_node));
}

// IrRegExpData::uc16_code shares the same CodePointer body-descriptor slot as
// latin1_code but is compiled only when the regexp is executed against a
// two-byte subject.  A subject string containing U+0100 (outside Latin-1 range)
// forces V8 to use a SeqTwoByteString, which triggers uc16 mode compilation.
// The offline translator must resolve the dumped CodePointerTable handle and
// emit a hidden/indexed edge, not a named "code" edge.
UNINITIALIZED_TEST(
    HeapDumpOffheapTranslateIrRegExpDataUc16CodePointerHiddenEdge) {
  i::Address data_addr = 0;
  i::Address code_addr = 0;
  bool is_expected_instance_type = false;
  bool has_uc16_code_val = false;

  auto generator = DumpAndTranslateWithIsolateTeardown(
      [&](i::Isolate* isolate, v8::Local<v8::Context> context) {
        // Capturing groups ensure this is compiled as IrRegExp (not atom).
        // The subject contains U+0100 so V8 allocates a SeqTwoByteString,
        // which forces compilation into IrRegExpData::uc16_code.
        RunScript(context, R"(
          const re = /heap-(dump)-offheap-(uc16)/;
          re.exec('heap-dump-offheap-uc16Ā');
          globalThis.__heap_dump_offheap_irregexp_uc16_target = re;
        )");

        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        v8::Local<v8::RegExp> regexp = v8::Local<v8::RegExp>::Cast(
            context->Global()
                ->Get(context,
                      NewApiString(v8_isolate,
                                   "__heap_dump_offheap_irregexp_uc16_target"))
                .ToLocalChecked());
        i::DirectHandle<i::JSRegExp> js_regexp =
            i::Cast<i::JSRegExp>(v8::Utils::OpenDirectHandle(*regexp));
        i::Tagged<i::RegExpData> regexp_data = js_regexp->data(isolate);
        i::Tagged<i::IrRegExpData> irregexp_data =
            i::TrustedCast<i::IrRegExpData>(regexp_data);

        is_expected_instance_type =
            i::IR_REG_EXP_DATA_TYPE == irregexp_data->map()->instance_type();
        has_uc16_code_val = irregexp_data->has_uc16_code();
        if (!is_expected_instance_type || !has_uc16_code_val) {
          return DumpArtifacts{};
        }

        i::Tagged<i::Code> code = irregexp_data->uc16_code(isolate);
        data_addr = irregexp_data->address();
        code_addr = code->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(js_regexp);
        artifacts.keep_alive.push_back(
            i::direct_handle(i::Tagged<i::RegExpData>(irregexp_data),
                             isolate));
        artifacts.keep_alive.push_back(i::direct_handle(code, isolate));
        artifacts.keep_alive.push_back(
            i::direct_handle(irregexp_data->wrapper(), isolate));
        return artifacts;
      });

  const dfx::Node* data_node = GetSnapshotNode(generator.get(), data_addr);
  const dfx::Node* code_node = GetSnapshotNode(generator.get(), code_addr);
  // This fixture must compile uc16_code; otherwise the hidden edge was not
  // actually exercised and the test should fail instead of soft-passing.
  CHECK(has_uc16_code_val);
  CHECK(is_expected_instance_type);
  // data and code were kept alive; both must appear in the snapshot.
  CHECK_NOT_NULL(data_node);
  CHECK_NOT_NULL(code_node);

  // The translator must resolve uc16_code CodePointer via the same generic
  // FindCodePointer path used for latin1_code; a hidden/indexed edge is
  // expected, not a fabricated named property edge.
  CHECK_LT(0, CountEdgesFromNodeToTarget(data_node, dfx::Edge::Type::kHidden,
                                         code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), data_node,
                                   dfx::Edge::Type::kInternal, "code",
                                   code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), data_node,
                                   dfx::Edge::Type::kProperty, "code",
                                   code_node));
}

// Focused white-box test for the GetNode gate in VisitJSDispatchTableEntry.
//
// Invariant under test:
//   JSFunction dispatch target → proved by FindObjectHead (rawheap membership)
//   CODE_TYPE confirmed → instance-type guard passes
//   Snapshot node exists → GetNode gate passes → real "code" edge emitted
//   No sandbox_*_node_missing warning → gate does not misfire on healthy data
//
// Uses DumpTranslateCaptureDiagnostics so both graph state and diagnostics are
// verified from the same dump bytes without exposing diagnostics through
// SnapshotGenerator's public API.
UNINITIALIZED_TEST(HeapDumpOffheapTranslateJSDispatchGetNodeGateDiagnostic) {
  i::Address fn_addr = 0;
  i::Address code_addr = 0;

  auto [generator, diagnostics] = DumpTranslateCaptureDiagnostics(
      [&](i::Isolate* isolate, v8::Local<v8::Context> context) {
        RunScript(context, R"(
          function HeapDumpGetNodeGateDispatch() { return 99; }
          globalThis.__heap_dump_get_node_gate_dispatch =
              HeapDumpGetNodeGateDispatch;
        )");
        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        v8::Local<v8::Function> fn = v8::Local<v8::Function>::Cast(
            context->Global()
                ->Get(context,
                      NewApiString(v8_isolate,
                                   "__heap_dump_get_node_gate_dispatch"))
                .ToLocalChecked());
        i::DirectHandle<i::JSFunction> js_function =
            i::Cast<i::JSFunction>(v8::Utils::OpenDirectHandle(*fn));
        i::DirectHandle<i::Code> code(js_function->code(isolate), isolate);
        fn_addr = js_function->address();
        code_addr = code->address();
        return DumpArtifacts{};
      });

  // Both JSFunction and its Code must appear as snapshot nodes.
  const dfx::Node* fn_node = GetSnapshotNode(generator.get(), fn_addr);
  const dfx::Node* code_node = GetSnapshotNode(generator.get(), code_addr);
  CHECK_NOT_NULL(fn_node);
  CHECK_NOT_NULL(code_node);

  // HeapObject target with existing snapshot node: GetNode gate passes →
  // the "code" internal edge must be emitted (not suppressed).
  CHECK(HasExactNamedEdge(generator.get(), fn_node, dfx::Edge::Type::kInternal,
                          "code", code_node));

  // The new GetNode gate must NOT emit a node-missing warning when the node
  // exists; a spurious warning here would indicate the gate is incorrectly
  // treating a healthy HeapObject target as a missing-node case.
  CHECK(!HasDiagnostic(diagnostics,
                       dfx::TranslateDiagnostics::Severity::kWarning,
                       "js_dispatch_code_node_missing", "object_body"));
  // CodePointer gate must also be silent for this fixture.
  CHECK(!HasDiagnostic(diagnostics,
                       dfx::TranslateDiagnostics::Severity::kWarning,
                       "sandbox_code_pointer_code_node_missing", "object_body"));
}

// Focused white-box test for the GetNode gate in ResolveHandle (CodePointer branch).
//
// Invariant under test:
//   IrRegExpData latin1_code target → proved by FindObjectHead (rawheap membership)
//   CODE_TYPE confirmed → instance-type guard passes
//   Snapshot node exists → GetNode gate passes → hidden edge emitted
//   No sandbox_code_pointer_code_node_missing warning → gate does not misfire
//
// Mirrors HeapDumpOffheapTranslateJSDispatchGetNodeGateDiagnostic for the
// CodePointer consumer path so both gates have explicit diagnostic coverage.
// The "node-missing" branch is architecturally unreachable in normal translation
// (PreVisit populates objects_head_ and the node map together before any edge
// pass runs), so this test proves absence of spurious misfires on healthy data.
UNINITIALIZED_TEST(HeapDumpOffheapTranslateCodePointerGetNodeGateDiagnostic) {
  i::Address data_addr = 0;
  i::Address code_addr = 0;
  bool has_latin1_code = false;

  auto [generator, diagnostics] = DumpTranslateCaptureDiagnostics(
      [&](i::Isolate* isolate, v8::Local<v8::Context> context) {
        // Capturing groups force IrRegExp compilation; latin1-only subject
        // ensures latin1_code is filled so the CodePointer slot is exercised.
        RunScript(context, R"(
          const re = /heap-(dump)-code-pointer-(gate)/g;
          re.exec('heap-dump-code-pointer-gate');
          globalThis.__heap_dump_code_pointer_gate_re = re;
        )");
        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        v8::Local<v8::RegExp> regexp = v8::Local<v8::RegExp>::Cast(
            context->Global()
                ->Get(context,
                      NewApiString(v8_isolate,
                                   "__heap_dump_code_pointer_gate_re"))
                .ToLocalChecked());
        i::DirectHandle<i::JSRegExp> js_regexp =
            i::Cast<i::JSRegExp>(v8::Utils::OpenDirectHandle(*regexp));
        i::Tagged<i::RegExpData> regexp_data = js_regexp->data(isolate);
        i::Tagged<i::IrRegExpData> irregexp_data =
            i::TrustedCast<i::IrRegExpData>(regexp_data);

        has_latin1_code = irregexp_data->has_latin1_code();
        if (!has_latin1_code) {
          return DumpArtifacts{};
        }

        i::Tagged<i::Code> code = irregexp_data->latin1_code(isolate);
        data_addr = irregexp_data->address();
        code_addr = code->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(js_regexp);
        artifacts.keep_alive.push_back(
            i::direct_handle(i::Tagged<i::RegExpData>(irregexp_data), isolate));
        artifacts.keep_alive.push_back(i::direct_handle(code, isolate));
        artifacts.keep_alive.push_back(
            i::direct_handle(irregexp_data->wrapper(), isolate));
        return artifacts;
      });

  // The fixture must compile latin1_code; a miss here means the CodePointer
  // slot was not populated and the gate was not exercised at all.
  CHECK(has_latin1_code);

  // Both IrRegExpData and its Code must appear as snapshot nodes.
  const dfx::Node* data_node = GetSnapshotNode(generator.get(), data_addr);
  const dfx::Node* code_node = GetSnapshotNode(generator.get(), code_addr);
  CHECK_NOT_NULL(data_node);
  CHECK_NOT_NULL(code_node);

  // HeapObject target with existing snapshot node: GetNode gate passes →
  // the hidden edge must be emitted (not suppressed).
  CHECK_LT(0, CountEdgesFromNodeToTarget(data_node, dfx::Edge::Type::kHidden,
                                         code_node));

  // The CodePointer GetNode gate must NOT emit a node-missing warning when the
  // node exists; a spurious warning here would indicate a gate misfire.
  CHECK(!HasDiagnostic(diagnostics,
                       dfx::TranslateDiagnostics::Severity::kWarning,
                       "sandbox_code_pointer_code_node_missing", "object_body"));
  // JSDispatch gate must also be silent for this CodePointer fixture.
  CHECK(!HasDiagnostic(diagnostics,
                       dfx::TranslateDiagnostics::Severity::kWarning,
                       "js_dispatch_code_node_missing", "object_body"));
}

#endif  // V8_ENABLE_SANDBOX

#endif  // OH_ENABLE_HEAP_DUMP_TEST && OH_ENABLE_HEAP_TRANSLATE

}  // namespace heap
}  // namespace internal
}  // namespace v8
