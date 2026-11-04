#ifndef V8_TEST_CCTEST_HEAP_DUMP_HEAP_DUMP_TEST_UTILS_H_
#define V8_TEST_CCTEST_HEAP_DUMP_HEAP_DUMP_TEST_UTILS_H_

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "include/v8-function.h"
#include "include/v8-persistent-handle.h"
#include "include/v8-template.h"
#include "src/api/api-inl.h"
#include "src/codegen/assembler-inl.h"
#include "src/heap/combined-heap.h"
#include "src/heap/factory-inl.h"
#include "src/heap/factory.h"
#include "src/heap/heap-inl.h"
#include "src/init/v8.h"
#include "src/objects/code.h"
#include "src/objects/foreign.h"
#include "src/objects/instruction-stream.h"
#include "src/objects/map-inl.h"
#include "src/objects/objects-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

#if defined(OH_ENABLE_HEAP_DUMP_TEST) && defined(OH_ENABLE_HEAP_DUMP)

#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-format.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-heap.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-objects.h"
#if defined(OH_ENABLE_HEAP_TRANSLATE)
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/snapshot-generator.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/snapshot-serializer.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-heap.h"
#endif
#include "arkweb/chromium_ext/v8/dfx/heap_dump/ut/binary-reader-ut.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/ut/binary-writer-ut.h"

namespace v8 {
namespace internal {
namespace heap {
namespace heap_dump_test {

using dfx::BinaryReaderTest;
using dfx::BinaryWriterTest;
using dfx::HeapDumper;
using dfx::ObjectDumper;
using dfx::RawHeapHeader;

#if defined(OH_ENABLE_HEAP_TRANSLATE)
using dfx::Edge;
using dfx::HeapTranslator;
using dfx::Node;
using dfx::SnapshotGenerator;
using dfx::TranslateDiagnostics;
#endif

using VisitorIdCounts = std::unordered_map<VisitorId, size_t>;

struct DumpArtifacts {
  std::vector<DirectHandle<Object>> keep_alive;
  std::vector<v8::Global<v8::Value>> global_keep_alive;
};

struct RawObjectRecord {
  Address address;
  uint32_t dump_size;
  uint32_t payload_offset;
};

struct RawRootRecord {
  uint32_t type;
  Address address;
};

inline void RunScript(v8::Local<v8::Context> context, const char* source) {
  CHECK(!CompileRun(context, source).IsEmpty());
}

inline v8::Local<v8::String> NewApiString(v8::Isolate* isolate,
                                          const char* value) {
  return v8::String::NewFromUtf8(isolate, value).ToLocalChecked();
}

inline void SetGlobalValue(v8::Local<v8::Context> context, const char* key,
                           v8::Local<v8::Value> value) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  CHECK_NOT_NULL(isolate);
  CHECK(context->Global()
            ->Set(context, NewApiString(isolate, key), value)
            .FromJust());
}

inline void CreateBaselineObjects(v8::Local<v8::Context> context) {
  RunScript(context, R"(
    globalThis.__heap_dump_object = { answer: 42, nested: { ok: true } };
    globalThis.__heap_dump_array = [1, 2, 3, { marker: 'array' }];
    function HeapDumpNamedCtor() { this.value = 1; }
    globalThis.__heap_dump_named_ctor_instance = new HeapDumpNamedCtor();
    globalThis.__heap_dump_regexp = /heap-dump-framework/;
    globalThis.__heap_dump_bigint = 2026n;
    function HeapDumpTarget() { return 1; }
    globalThis.__heap_dump_bound = HeapDumpTarget.bind(null);
  )");
}

inline void CreateExtendedObjects(v8::Local<v8::Context> context) {
  RunScript(context, R"(
    globalThis.__heap_dump_map = new Map([[1, 'one']]);
    globalThis.__heap_dump_set = new Set([1, 2, 3]);
    globalThis.__heap_dump_array_buffer = new ArrayBuffer(32);
    globalThis.__heap_dump_typed_array = new Uint8Array([1, 2, 3, 4]);
  )");
}

inline DirectHandle<InstructionStream> CreateInstructionStreamForTest(
    Isolate* isolate) {
#define __ assm.
  Assembler assm(isolate->allocator(), AssemblerOptions{});
  __ nop();
  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  return direct_handle(
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING)
          .Build()
          ->instruction_stream(),
      isolate);
#undef __
}

inline DirectHandle<Code> CreateCodeForTest(Isolate* isolate) {
#define __ assm.
  Assembler assm(isolate->allocator(), AssemblerOptions{});
  __ nop();
  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  return Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#undef __
}

inline DirectHandle<String> CreateExternalStringForTest(
    Isolate* isolate, v8::Local<v8::Context> context) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  auto* resource = new StaticOneByteResource("heap-dump-external");
  v8::Local<v8::String> external_string =
      v8::String::NewExternalOneByte(v8_isolate, resource).ToLocalChecked();
  SetGlobalValue(context, "__heap_dump_external_string", external_string);
  return v8::Utils::OpenDirectHandle(*external_string);
}

inline DirectHandle<Foreign> CreateForeignForTest(Isolate* isolate) {
  return direct_handle(*isolate->factory()
                            ->NewForeign<i::kMicrotaskCallbackDataTag>(
                                0x135, AllocationType::kOld),
                       isolate);
}

inline std::vector<uint8_t> DumpHeapBytes(Isolate* isolate) {
  BinaryWriterTest writer;
  HeapDumper dumper(isolate->heap(), &writer);
  dumper.DumpHeap();
  return std::vector<uint8_t>(writer.Data(), writer.Data() + writer.Size());
}

inline VisitorIdCounts CollectVisitorIdCounts(Isolate* isolate) {
  VisitorIdCounts counts;
  CombinedHeapObjectIterator it(isolate->heap());
  for (Tagged<HeapObject> object = it.Next(); !object.is_null();
       object = it.Next()) {
    counts[object->map(isolate)->visitor_id()]++;
  }
  return counts;
}

template <typename PrepareFn>
std::vector<uint8_t> DumpWithIsolateTeardown(PrepareFn prepare) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* v8_isolate = v8::Isolate::New(create_params);
  Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);

  std::vector<uint8_t> bytes;
  v8_isolate->Enter();
  {
    v8::HandleScope scope(v8_isolate);
    LocalContext env(v8_isolate);
    DumpArtifacts artifacts = prepare(isolate, env.local());
    USE(artifacts);
    bytes = DumpHeapBytes(isolate);
  }
  v8_isolate->Exit();
  v8_isolate->Dispose();
  return bytes;
}

#if defined(OH_ENABLE_HEAP_TRANSLATE)
inline std::unique_ptr<SnapshotGenerator> TranslateHeapBytes(
    const std::vector<uint8_t>& bytes, TranslateDiagnostics* diagnostics,
    bool* ok) {
  BinaryReaderTest reader(const_cast<uint8_t*>(bytes.data()),
                          static_cast<uint32_t>(bytes.size()));
  auto generator = std::make_unique<SnapshotGenerator>();
  HeapTranslator translator(&reader, generator.get());
  bool translated = translator.TranslateHeap();
  if (diagnostics != nullptr) {
    *diagnostics = translator.diagnostics();
  }
  if (ok != nullptr) {
    *ok = translated;
  }
  return generator;
}

inline std::unique_ptr<SnapshotGenerator> TranslateHeapBytes(
    const std::vector<uint8_t>& bytes) {
  return TranslateHeapBytes(bytes, nullptr, nullptr);
}

inline bool TryTranslateHeapBytes(const std::vector<uint8_t>& bytes,
                                  TranslateDiagnostics* diagnostics) {
  bool ok = false;
  USE(TranslateHeapBytes(bytes, diagnostics, &ok));
  return ok;
}

template <typename PrepareFn>
std::unique_ptr<SnapshotGenerator> DumpAndTranslateWithIsolateTeardown(
    PrepareFn prepare) {
  return TranslateHeapBytes(DumpWithIsolateTeardown(std::move(prepare)));
}

inline bool HasString(SnapshotGenerator* generator, const char* value) {
  return generator->strings().find(value) != generator->strings().end();
}

inline int CountNodes(SnapshotGenerator* generator, Node::Type type,
                      const char* name) {
  auto it = generator->strings().find(name);
  if (it == generator->strings().end()) {
    return 0;
  }
  int count = 0;
  for (const Node* node : generator->nodes()) {
    if (node->type_ == static_cast<uint32_t>(type) &&
        node->name_ == it->second) {
      count++;
    }
  }
  return count;
}

inline bool HasNode(SnapshotGenerator* generator, Node::Type type,
                    const char* name) {
  return CountNodes(generator, type, name) > 0;
}

inline std::vector<std::string> SnapshotStringsById(
    SnapshotGenerator* generator) {
  std::vector<std::string> strings;
  strings.resize(generator->strings().size());
  for (const auto& entry : generator->strings()) {
    CHECK_LT(entry.second, strings.size());
    strings[entry.second] = entry.first;
  }
  return strings;
}

inline const std::string& SnapshotStringAt(
    const std::vector<std::string>& strings, uint32_t id) {
  CHECK_LT(id, strings.size());
  return strings[id];
}

inline std::string NodeDisplayName(SnapshotGenerator* generator,
                                   const Node* node) {
  return SnapshotStringAt(SnapshotStringsById(generator), node->name_);
}

inline bool NodeHasType(const Node* node, Node::Type type) {
  return node != nullptr && node->type_ == static_cast<uint32_t>(type);
}

inline bool NodeHasTypeAndName(SnapshotGenerator* generator, const Node* node,
                               Node::Type type, const char* name) {
  if (!NodeHasType(node, type)) {
    return false;
  }
  std::vector<std::string> strings = SnapshotStringsById(generator);
  return SnapshotStringAt(strings, node->name_) == name;
}

inline Node* GetSnapshotNode(SnapshotGenerator* generator, Address address) {
  return generator->GetNode(address);
}

inline bool NodeMatchesTypeAndName(SnapshotGenerator* generator,
                                   const Node* node, Node::Type type,
                                   const char* name) {
  return node != nullptr && NodeHasTypeAndName(generator, node, type, name);
}

inline DirectHandle<JSReceiver> LocalJSReceiver(v8::Local<v8::Value> value) {
  return Cast<JSReceiver>(
      v8::Utils::OpenDirectHandle(*v8::Local<v8::Object>::Cast(value)));
}

inline Address LocalObjectAddress(v8::Local<v8::Value> value) {
  DirectHandle<JSReceiver> object = LocalJSReceiver(value);
  return object->address();
}

inline InstanceType LocalObjectInstanceType(v8::Local<v8::Value> value) {
  return LocalJSReceiver(value)->map()->instance_type();
}

inline const char* HeapDumpInstanceTypeName(InstanceType type) {
  switch (type) {
#define HEAP_DUMP_INSTANCE_TYPE_NAME_CASE(TYPE) \
  case TYPE:                                    \
    return #TYPE;
    INSTANCE_TYPE_LIST(HEAP_DUMP_INSTANCE_TYPE_NAME_CASE)
#undef HEAP_DUMP_INSTANCE_TYPE_NAME_CASE
  }
  return "UNKNOWN_INSTANCE_TYPE";
}

inline bool HeapDumpIsInstanceTypeRangeAliasName(const char* name) {
  return std::strncmp(name, "FIRST_", 6) == 0 ||
         std::strncmp(name, "LAST_", 5) == 0;
}

inline void PrintHeapDumpCandidateInstanceType(InstanceType type,
                                               const char* name) {
  if (HeapDumpIsInstanceTypeRangeAliasName(name)) {
    return;
  }
  std::printf("HEAP_DUMP_CANDIDATE_INSTANCE_TYPE:%u:%s\n",
              static_cast<uint32_t>(type), name);
}

inline void PrintHeapDumpCandidateInstanceTypes() {
#define HEAP_DUMP_CANDIDATE_INSTANCE_TYPE(TYPE) \
  PrintHeapDumpCandidateInstanceType(TYPE, #TYPE);
  INSTANCE_TYPE_LIST(HEAP_DUMP_CANDIDATE_INSTANCE_TYPE)
#undef HEAP_DUMP_CANDIDATE_INSTANCE_TYPE
  std::fflush(stdout);
}

inline void MaybePrintHeapDumpCandidateInstanceTypesFromEnv() {
  static bool printed = false;
  if (printed) {
    return;
  }
  printed = true;
  const char* value = std::getenv("HEAP_DUMP_PRINT_CANDIDATE_INSTANCE_TYPES");
  if (value == nullptr || value[0] == '\0' || std::strcmp(value, "0") == 0) {
    return;
  }
  PrintHeapDumpCandidateInstanceTypes();
}

inline void MarkCoveredInstanceType(InstanceType type) {
  std::printf("HEAP_DUMP_COVERED_INSTANCE_TYPE:%u:%s\n",
              static_cast<uint32_t>(type), HeapDumpInstanceTypeName(type));
  std::fflush(stdout);
}

namespace {

[[maybe_unused]] const bool kHeapDumpCandidateInstanceTypesMaybePrinted =
    (MaybePrintHeapDumpCandidateInstanceTypesFromEnv(), true);

}  // namespace

template <typename TargetPredicate>
int CountEdgesByNameFromNode(SnapshotGenerator* generator, const Node* source,
                             Edge::Type type, const char* edge_name,
                             TargetPredicate target_predicate) {
  CHECK_NOT_NULL(source);
  std::vector<std::string> strings = SnapshotStringsById(generator);
  int count = 0;
  for (const Edge* edge : source->childs_) {
    if (edge->type_ != static_cast<uint32_t>(type)) {
      continue;
    }
    if (SnapshotStringAt(strings, edge->name_or_index_) != edge_name) {
      continue;
    }
    if (target_predicate(edge->to_node_)) {
      ++count;
    }
  }
  return count;
}

template <typename TargetPredicate>
bool HasEdgeByNameFromNode(SnapshotGenerator* generator, const Node* source,
                           Edge::Type type, const char* edge_name,
                           TargetPredicate target_predicate) {
  return CountEdgesByNameFromNode(generator, source, type, edge_name,
                                  target_predicate) > 0;
}

template <typename NodePredicate>
int CountNodesIf(SnapshotGenerator* generator, NodePredicate predicate) {
  int count = 0;
  for (const Node* node : generator->nodes()) {
    if (predicate(node)) {
      ++count;
    }
  }
  return count;
}

template <typename NodePredicate>
bool HasNodeIf(SnapshotGenerator* generator, NodePredicate predicate) {
  return CountNodesIf(generator, predicate) > 0;
}

template <typename SourcePredicate, typename TargetPredicate>
int CountEdgesIf(SnapshotGenerator* generator, Edge::Type type,
                 SourcePredicate source_predicate,
                 TargetPredicate target_predicate) {
  int count = 0;
  for (const Node* source : generator->nodes()) {
    if (!source_predicate(source)) {
      continue;
    }
    for (const Edge* edge : source->childs_) {
      if (edge->type_ == static_cast<uint32_t>(type) &&
          target_predicate(edge->to_node_)) {
        ++count;
      }
    }
  }
  return count;
}

template <typename SourcePredicate, typename TargetPredicate>
bool HasEdgeIf(SnapshotGenerator* generator, Edge::Type type,
               SourcePredicate source_predicate,
               TargetPredicate target_predicate) {
  return CountEdgesIf(generator, type, source_predicate, target_predicate) > 0;
}

template <typename SourcePredicate, typename TargetPredicate>
int CountEdgesByName(SnapshotGenerator* generator, Edge::Type type,
                     const char* edge_name, SourcePredicate source_predicate,
                     TargetPredicate target_predicate) {
  std::vector<std::string> strings = SnapshotStringsById(generator);
  int count = 0;
  for (const Node* source : generator->nodes()) {
    if (!source_predicate(source)) {
      continue;
    }
    for (const Edge* edge : source->childs_) {
      if (edge->type_ != static_cast<uint32_t>(type)) {
        continue;
      }
      if (SnapshotStringAt(strings, edge->name_or_index_) != edge_name) {
        continue;
      }
      if (target_predicate(edge->to_node_)) {
        ++count;
      }
    }
  }
  return count;
}

template <typename SourcePredicate, typename TargetPredicate>
bool HasEdgeByName(SnapshotGenerator* generator, Edge::Type type,
                   const char* edge_name, SourcePredicate source_predicate,
                   TargetPredicate target_predicate) {
  return CountEdgesByName(generator, type, edge_name, source_predicate,
                          target_predicate) > 0;
}

template <typename SourcePredicate, typename TargetPredicate>
int CountEdgesByIndex(SnapshotGenerator* generator, Edge::Type type,
                      uint32_t edge_index, SourcePredicate source_predicate,
                      TargetPredicate target_predicate) {
  int count = 0;
  for (const Node* source : generator->nodes()) {
    if (!source_predicate(source)) {
      continue;
    }
    for (const Edge* edge : source->childs_) {
      if (edge->type_ == static_cast<uint32_t>(type) &&
          edge->name_or_index_ == edge_index &&
          target_predicate(edge->to_node_)) {
        ++count;
      }
    }
  }
  return count;
}

template <typename SourcePredicate, typename TargetPredicate>
bool HasEdgeByIndex(SnapshotGenerator* generator, Edge::Type type,
                    uint32_t edge_index, SourcePredicate source_predicate,
                    TargetPredicate target_predicate) {
  return CountEdgesByIndex(generator, type, edge_index, source_predicate,
                           target_predicate) > 0;
}

inline int CountEdgesFromNode(const Node* source, Edge::Type type) {
  CHECK_NOT_NULL(source);
  int count = 0;
  for (const Edge* edge : source->childs_) {
    if (edge->type_ == static_cast<uint32_t>(type)) {
      ++count;
    }
  }
  return count;
}

template <typename = void>
inline int CountEdgesFromNodeToTarget(const Node* source, Edge::Type type,
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

inline std::string EdgeIndexName(int index) { return std::to_string(index); }

inline int CountIndexedEdges(const Node* source, Edge::Type type,
                             uint32_t edge_index) {
  CHECK_NOT_NULL(source);
  int count = 0;
  for (const Edge* edge : source->childs_) {
    if (edge->type_ == static_cast<uint32_t>(type) &&
        edge->name_or_index_ == edge_index) {
      ++count;
    }
  }
  return count;
}

inline int CountExactIndexedEdges(const Node* source, Edge::Type type,
                                  uint32_t edge_index, const Node* target) {
  CHECK_NOT_NULL(source);
  int count = 0;
  for (const Edge* edge : source->childs_) {
    if (edge->type_ == static_cast<uint32_t>(type) &&
        edge->name_or_index_ == edge_index && edge->to_node_ == target) {
      ++count;
    }
  }
  return count;
}

template <typename = void>
inline int CountExactNamedEdges(SnapshotGenerator* generator,
                                const Node* source, Edge::Type type,
                                const char* edge_name, const Node* target) {
  return CountEdgesByNameFromNode(
      generator, source, type, edge_name,
      [target](const Node* node) { return node == target; });
}

template <typename = void>
inline bool HasExactNamedEdge(SnapshotGenerator* generator, const Node* source,
                              Edge::Type type, const char* edge_name,
                              const Node* target) {
  return CountExactNamedEdges(generator, source, type, edge_name, target) > 0;
}

inline void CheckExactWeakIndexedEdge(SnapshotGenerator* generator,
                                      const Node* source, int index,
                                      const Node* target) {
  const std::string edge_name = EdgeIndexName(index);
  CHECK(HasExactNamedEdge(generator, source, Edge::Type::kWeak,
                          edge_name.c_str(), target));
  CHECK_EQ(1, CountExactNamedEdges(generator, source, Edge::Type::kWeak,
                                   edge_name.c_str(), target));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(source, Edge::Type::kWeak, target));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(source, Edge::Type::kProperty, target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kHidden, target));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(source, Edge::Type::kInternal, target));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(source, Edge::Type::kShortcut, target));
}

inline void CheckExactWeakEdgeNoInternalHiddenPropertyOrShortcutDuplicate(
    SnapshotGenerator* generator, const Node* source, const char* edge_name,
    const Node* target, bool allow_additional_internal_edges_to_target = false) {
  CHECK(HasExactNamedEdge(generator, source, Edge::Type::kWeak, edge_name,
                          target));
  CHECK_EQ(1, CountExactNamedEdges(generator, source, Edge::Type::kWeak,
                                   edge_name, target));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(source, Edge::Type::kWeak, target));
  if (allow_additional_internal_edges_to_target) {
    CHECK_GE(CountEdgesFromNodeToTarget(source, Edge::Type::kInternal, target),
             1);
  } else {
    CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kInternal,
                                           target));
  }
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kHidden, target));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(source, Edge::Type::kProperty, target));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(source, Edge::Type::kShortcut, target));
  CHECK_EQ(0, CountExactNamedEdges(generator, source, Edge::Type::kInternal,
                                   edge_name, target));
  CHECK_EQ(0, CountExactNamedEdges(generator, source, Edge::Type::kHidden,
                                   edge_name, target));
  CHECK_EQ(0, CountExactNamedEdges(generator, source, Edge::Type::kProperty,
                                   edge_name, target));
  CHECK_EQ(0, CountExactNamedEdges(generator, source, Edge::Type::kShortcut,
                                   edge_name, target));
}

inline void CheckNoWeakOrInternalIndexedEdge(SnapshotGenerator* generator,
                                             const Node* source, int index) {
  const std::string edge_name = EdgeIndexName(index);
  auto any = [](const Node* node) {
    USE(node);
    return true;
  };
  CHECK_EQ(0, CountEdgesByNameFromNode(generator, source, Edge::Type::kWeak,
                                       edge_name.c_str(), any));
  CHECK_EQ(0, CountEdgesByNameFromNode(generator, source, Edge::Type::kInternal,
                                       edge_name.c_str(), any));
}

inline void CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
    SnapshotGenerator* generator, const Node* source, const char* edge_name,
    const Node* target) {
  CHECK(HasExactNamedEdge(generator, source, Edge::Type::kProperty, edge_name,
                          target));
  CHECK_EQ(1, CountExactNamedEdges(generator, source, Edge::Type::kProperty,
                                   edge_name, target));
  CHECK_EQ(1,
           CountEdgesFromNodeToTarget(source, Edge::Type::kProperty, target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kWeak, target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kHidden, target));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(source, Edge::Type::kInternal, target));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(source, Edge::Type::kShortcut, target));
}

inline void CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
    SnapshotGenerator* generator, const Node* source, const char* edge_name,
    const Node* target, bool allow_additional_internal_edges_to_target = false) {
  CHECK(HasExactNamedEdge(generator, source, Edge::Type::kInternal, edge_name,
                          target));
  CHECK_EQ(1, CountExactNamedEdges(generator, source, Edge::Type::kInternal,
                                   edge_name, target));
  if (allow_additional_internal_edges_to_target) {
    CHECK_GE(CountEdgesFromNodeToTarget(source, Edge::Type::kInternal, target),
             1);
  } else {
    CHECK_EQ(1, CountEdgesFromNodeToTarget(source, Edge::Type::kInternal,
                                          target));
  }
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kProperty, target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kHidden, target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kWeak, target));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(source, Edge::Type::kShortcut, target));
}

inline void CheckInternalEdgeNoHiddenPropertyOrWeakDuplicate(
    SnapshotGenerator* generator, const Node* source, const Node* target) {
  CHECK_EQ(1, CountEdgesFromNodeToTarget(source, Edge::Type::kInternal, target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kProperty, target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kHidden, target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kWeak, target));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(source, Edge::Type::kShortcut, target));
}

inline void CheckSingleHiddenEdgeToTargetNoPropertyOrInternalDuplicate(
    const Node* source, const Node* target) {
  CHECK_EQ(1, CountEdgesFromNodeToTarget(source, Edge::Type::kHidden, target));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(source, Edge::Type::kProperty, target));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(source, Edge::Type::kInternal, target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kWeak, target));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(source, Edge::Type::kShortcut, target));
}

inline void CheckSinglePropertyOrHiddenEdgeToTargetNoInternalDuplicate(
    const Node* source, const Node* target) {
  int hidden_count =
      CountEdgesFromNodeToTarget(source, Edge::Type::kHidden, target);
  int property_count =
      CountEdgesFromNodeToTarget(source, Edge::Type::kProperty, target);
  int internal_count =
      CountEdgesFromNodeToTarget(source, Edge::Type::kInternal, target);
  CHECK_EQ(1, hidden_count + property_count + internal_count);
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kWeak, target));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(source, Edge::Type::kShortcut, target));
}
#endif

template <typename T>
inline T ReadPod(const std::vector<uint8_t>& bytes, uint32_t offset) {
  CHECK(static_cast<size_t>(offset) + sizeof(T) <= bytes.size());
  T value{};
  memcpy(&value, bytes.data() + offset, sizeof(T));
  return value;
}

inline RawHeapHeader ReadRawHeapHeader(const std::vector<uint8_t>& bytes) {
  return ReadPod<RawHeapHeader>(bytes, 0);
}

inline std::vector<RawRootRecord> CollectRawRoots(
    const std::vector<uint8_t>& bytes) {
  RawHeapHeader header = ReadRawHeapHeader(bytes);
  std::vector<RawRootRecord> records;
  uint32_t offset = header.root_table_offset_;
  uint32_t table_end = header.root_table_offset_ + header.root_table_size_;
  for (uint32_t i = 0; i < header.root_type_count_; ++i) {
    uint32_t type = ReadPod<uint32_t>(bytes, offset);
    offset += sizeof(uint32_t);
    uint32_t count = ReadPod<uint32_t>(bytes, offset);
    offset += sizeof(uint32_t);
    for (uint32_t j = 0; j < count; ++j) {
      Address address = ReadPod<Address>(bytes, offset);
      offset += sizeof(Address);
      records.push_back({type, address});
    }
  }
  CHECK_EQ(table_end, offset);
  return records;
}

inline std::vector<RawObjectRecord> CollectRawObjects(
    const std::vector<uint8_t>& bytes) {
  RawHeapHeader header = ReadRawHeapHeader(bytes);
  std::vector<RawObjectRecord> records;
  uint32_t offset = header.object_table_offset_;
  uint32_t table_end = header.object_table_offset_ + header.object_table_size_;
  for (uint32_t i = 0; i < header.object_count_; ++i) {
    Address address = ReadPod<Address>(bytes, offset);
    offset += sizeof(Address);
    uint32_t dump_size = ReadPod<uint32_t>(bytes, offset);
    offset += sizeof(uint32_t);
    records.push_back({address, dump_size, offset});
    offset += dump_size;
  }
  CHECK_EQ(table_end, offset);
  return records;
}

inline RawObjectRecord FindRawObject(const std::vector<uint8_t>& bytes,
                                     Address address) {
  for (const RawObjectRecord& record : CollectRawObjects(bytes)) {
    if (record.address == address) {
      return record;
    }
  }
  UNREACHABLE();
}

inline Tagged_t ReadRawObjectMapPointer(const std::vector<uint8_t>& bytes,
                                        const RawObjectRecord& record) {
  return ReadPod<Tagged_t>(bytes, record.payload_offset);
}

inline uint32_t ReadRawObjectRealSize(const std::vector<uint8_t>& bytes,
                                      const RawObjectRecord& record) {
  return ReadPod<uint32_t>(bytes, record.payload_offset + sizeof(Tagged_t));
}

struct ExpectedDumpObjectCounts {
  uint32_t dumped = 0;
  uint32_t filler = 0;
  uint32_t free_space = 0;
};

inline ExpectedDumpObjectCounts CountExpectedDumpObjectStats(Isolate* isolate) {
  ExpectedDumpObjectCounts counts;
  i::CombinedHeapObjectIterator iterator(isolate->heap());
  for (Tagged<HeapObject> obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    switch (obj->map()->instance_type()) {
      case InstanceType::FREE_SPACE_TYPE:
        ++counts.free_space;
        break;
      case InstanceType::FILLER_TYPE:
        ++counts.filler;
        break;
      default:
        ++counts.dumped;
        break;
    }
  }
  return counts;
}

inline uint32_t CountExpectedDumpedObjects(Isolate* isolate) {
  return CountExpectedDumpObjectStats(isolate).dumped;
}

inline std::string ReadTextFile(const std::string& path) {
  std::ifstream stream(path);
  CHECK(stream.is_open());
  return std::string((std::istreambuf_iterator<char>(stream)),
                     std::istreambuf_iterator<char>());
}

}  // namespace heap_dump_test
}  // namespace heap
}  // namespace internal
}  // namespace v8

#endif  // defined(OH_ENABLE_HEAP_DUMP_TEST) && defined(OH_ENABLE_HEAP_DUMP)

#endif  // V8_TEST_CCTEST_HEAP_DUMP_HEAP_DUMP_TEST_UTILS_H_
