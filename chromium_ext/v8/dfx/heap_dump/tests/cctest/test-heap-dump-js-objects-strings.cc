#include "arkweb/chromium_ext/v8/dfx/heap_dump/tests/cctest/heap-dump-test-utils.h"
#include "test/common/flag-utils.h"

#include "include/v8-primitive-object.h"
#include "include/v8-primitive.h"
#include "src/objects/api-callbacks-inl.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/bigint.h"
#include "src/objects/bytecode-array-inl.h"
#include "src/objects/contexts-inl.h"
#include "src/objects/descriptor-array-inl.h"
#include "src/flags/flags.h"
#include "src/objects/feedback-vector-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/js-function-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/js-promise-inl.h"
#include "src/objects/js-proxy-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/js-regexp-string-iterator-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/name-inl.h"
#include "src/objects/oddball-inl.h"
#include "src/objects/property.h"
#include "src/objects/property-array-inl.h"
#include "src/objects/script-inl.h"
#include "src/objects/string.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/scope-info.h"
#include "src/objects/struct-inl.h"

namespace v8 {
namespace internal {
namespace heap {

#if defined(OH_ENABLE_HEAP_DUMP_TEST) && defined(OH_ENABLE_HEAP_DUMP) && \
    defined(OH_ENABLE_HEAP_TRANSLATE)

using namespace heap_dump_test;

class ExternalOneByteResource : public v8::String::ExternalOneByteStringResource {
 public:
  ExternalOneByteResource(const char* data, size_t length)
      : data_(data), length_(length) {}

  const char* data() const override { return data_; }

  size_t length() const override { return length_; }

 private:
  const char* data_;
  size_t length_;
};

class UncachedExternalOneByteResource
    : public v8::String::ExternalOneByteStringResource {
 public:
  UncachedExternalOneByteResource(const char* data, size_t length)
      : data_(data), length_(length) {}

  const char* data() const override { return data_; }

  size_t length() const override { return length_; }

  bool IsCacheable() const override { return false; }

 private:
  const char* data_;
  size_t length_;
};

class ExternalTwoByteResource : public v8::String::ExternalStringResource {
 public:
  ExternalTwoByteResource(const uint16_t* data, size_t length)
      : data_(data), length_(length) {}

  const uint16_t* data() const override { return data_; }

  size_t length() const override { return length_; }

 private:
  const uint16_t* data_;
  size_t length_;
};

class UncachedExternalTwoByteResource : public v8::String::ExternalStringResource {
 public:
  UncachedExternalTwoByteResource(const uint16_t* data, size_t length)
      : data_(data), length_(length) {}

  const uint16_t* data() const override { return data_; }

  size_t length() const override { return length_; }

  bool IsCacheable() const override { return false; }

 private:
  const uint16_t* data_;
  size_t length_;
};

constexpr char kExternalOneByteValue[] = "heap-dump-external-one-byte";
constexpr size_t kExternalOneByteValueLength =
    sizeof(kExternalOneByteValue) / sizeof(kExternalOneByteValue[0]) - 1;
constexpr uint16_t kExternalTwoByteValue[] = {
    'h', 'e', 'a', 'p', '-', 'd', 'u', 'm', 'p', '-', 'e', 'x',
    't', 'e', 'r', 'n', 'a', 'l', '-', 't', 'w', 'o', '-',
    'b', 'y', 't', 'e', 0};
constexpr size_t kExternalTwoByteValueLength =
    sizeof(kExternalTwoByteValue) / sizeof(kExternalTwoByteValue[0]) - 1;

void InvokeSharedStringTransitionGC(Heap* heap) {
  FlagScope<bool> transition_strings_during_gc_with_stack(
      &v8_flags.transition_strings_during_gc_with_stack, true);
  heap::CollectSharedGarbage(heap);
}

void CheckExactInternalStringEdgeNoWrongKind(SnapshotGenerator* generator,
                                             const Node* source,
                                             const char* edge_name,
                                             const Node* target) {
  // White-box check for ExtractStringReferences' native SetInternalReference
  // shape: one named kInternal edge and no duplicate edge of another kind.
  CHECK(HasExactNamedEdge(generator, source, Edge::Type::kInternal, edge_name,
                          target));
  CHECK_EQ(1, CountExactNamedEdges(generator, source, Edge::Type::kInternal,
                                   edge_name, target));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(source, Edge::Type::kInternal,
                                         target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kWeak, target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kHidden, target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kProperty,
                                         target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kShortcut,
                                         target));
}

UNINITIALIZED_TEST(HeapDumpSeqOneByteStringExactMapEdge) {
  Address seq_one_byte_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DumpArtifacts artifacts;
            DirectHandle<String> one_byte_seq =
                isolate->factory()->NewStringFromAsciiChecked(
                    "heap-dump-seq-one-byte-type");
            CHECK_EQ(SEQ_ONE_BYTE_STRING_TYPE,
                     one_byte_seq->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            artifacts.keep_alive.push_back(one_byte_seq);
            seq_one_byte_address = one_byte_seq->address();
            map_address = one_byte_seq->map()->address();
            return artifacts;
          });

  Node* one_byte_seq = GetSnapshotNode(generator.get(), seq_one_byte_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(one_byte_seq);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(one_byte_seq, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), one_byte_seq, Edge::Type::kInternal,
                          "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), one_byte_seq,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(one_byte_seq, Edge::Type::kInternal,
                                         map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(one_byte_seq, Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(one_byte_seq, Edge::Type::kHidden,
                                         map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(one_byte_seq, Edge::Type::kProperty,
                                         map));
  MarkCoveredInstanceType(SEQ_ONE_BYTE_STRING_TYPE);
}

UNINITIALIZED_TEST(HeapDumpSeqTwoByteStringExactMapEdge) {
  Address seq_two_byte_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            const uint16_t kSeqTwoBytePayload[] = {0x4f60, 0x597d};
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::String> value = v8::String::NewFromTwoByte(
                                                v8_isolate, kSeqTwoBytePayload,
                                                v8::NewStringType::kNormal, 2)
                                                .ToLocalChecked();
            DirectHandle<String> two_byte_seq = v8::Utils::OpenDirectHandle(*value);
            DumpArtifacts artifacts;
            InvokeAtomicMajorGC(isolate->heap());

            CHECK_EQ(SEQ_TWO_BYTE_STRING_TYPE,
                     two_byte_seq->map()->instance_type());
            artifacts.keep_alive.push_back(two_byte_seq);

            seq_two_byte_address = two_byte_seq->address();
            map_address = two_byte_seq->map()->address();
            return artifacts;
          });

  Node* two_byte_seq = GetSnapshotNode(generator.get(), seq_two_byte_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(two_byte_seq);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(two_byte_seq, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), two_byte_seq, Edge::Type::kInternal,
                          "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), two_byte_seq,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(two_byte_seq, Edge::Type::kInternal,
                                         map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(two_byte_seq, Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(two_byte_seq, Edge::Type::kHidden,
                                         map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(two_byte_seq, Edge::Type::kProperty,
                                         map));
  MarkCoveredInstanceType(SEQ_TWO_BYTE_STRING_TYPE);
}

UNINITIALIZED_TEST(HeapDumpSharedSeqTwoByteStringExactMapEdge) {
  Address shared_seq_two_byte_address = 0;
  Address map_address = 0;

  v8_flags.shared_string_table = true;
  v8_flags.shared_strings = true;
  FlagList::EnforceFlagImplications();

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            const uint16_t kPayload[] = {0x4f60, 0x597d};
            v8::Local<v8::String> value = v8::String::NewFromTwoByte(
                                                v8_isolate, kPayload,
                                                v8::NewStringType::kNormal, 2)
                                                .ToLocalChecked();
            DirectHandle<String> two_byte_seq = v8::Utils::OpenDirectHandle(*value);
            DirectHandle<String> shared_seq = String::Share(isolate, two_byte_seq);

            InvokeAtomicMajorGC(isolate->heap());

            CHECK(shared_seq->IsShared());
            CHECK(shared_seq->IsTwoByteRepresentation());
            CHECK_EQ(SHARED_SEQ_TWO_BYTE_STRING_TYPE,
                     shared_seq->map()->instance_type());

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(shared_seq);
            shared_seq_two_byte_address = shared_seq->address();
            map_address = shared_seq->map()->address();
            return artifacts;
          });

  Node* shared_seq_two_byte = GetSnapshotNode(generator.get(),
                                              shared_seq_two_byte_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(shared_seq_two_byte);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(shared_seq_two_byte, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), shared_seq_two_byte,
                          Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), shared_seq_two_byte,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(shared_seq_two_byte,
                                         Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(shared_seq_two_byte,
                                         Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(shared_seq_two_byte,
                                         Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(shared_seq_two_byte,
                                         Edge::Type::kProperty, map));
  MarkCoveredInstanceType(SHARED_SEQ_TWO_BYTE_STRING_TYPE);
}

UNINITIALIZED_TEST(HeapDumpSharedSeqOneByteStringExactMapEdge) {
  Address shared_seq_one_byte_address = 0;
  Address map_address = 0;

  v8_flags.shared_string_table = true;
  v8_flags.shared_strings = true;
  FlagList::EnforceFlagImplications();

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DumpArtifacts artifacts;
            DirectHandle<String> one_byte_seq =
                isolate->factory()->NewStringFromAsciiChecked(
                    "heap-dump-shared-seq-one-byte-type");
            DirectHandle<String> shared_seq = String::Share(isolate, one_byte_seq);

            InvokeAtomicMajorGC(isolate->heap());

            CHECK(shared_seq->IsShared());
            CHECK(shared_seq->IsOneByteRepresentation());
            CHECK_EQ(SHARED_SEQ_ONE_BYTE_STRING_TYPE,
                     shared_seq->map()->instance_type());

            artifacts.keep_alive.push_back(shared_seq);
            shared_seq_one_byte_address = shared_seq->address();
            map_address = shared_seq->map()->address();
            return artifacts;
          });

  Node* shared_seq_one_byte = GetSnapshotNode(generator.get(),
                                              shared_seq_one_byte_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(shared_seq_one_byte);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(shared_seq_one_byte, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), shared_seq_one_byte,
                          Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), shared_seq_one_byte,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(shared_seq_one_byte,
                                         Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(shared_seq_one_byte,
                                         Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(shared_seq_one_byte,
                                         Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(shared_seq_one_byte,
                                         Edge::Type::kProperty, map));
  MarkCoveredInstanceType(SHARED_SEQ_ONE_BYTE_STRING_TYPE);
}

UNINITIALIZED_TEST(HeapDumpSharedExternalOneByteStringExactMapEdge) {
  Address shared_external_one_byte_address = 0;
  Address map_address = 0;

  v8_flags.shared_string_table = true;
  v8_flags.shared_strings = true;
  FlagList::EnforceFlagImplications();

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<String> one_byte_seq =
                isolate->factory()->NewStringFromAsciiChecked(
                    "heap-dump-shared-external-one-byte-type");
            DirectHandle<String> shared_external_one_byte =
                String::Share(isolate, one_byte_seq);
            CHECK(shared_external_one_byte->IsShared());
            CHECK(shared_external_one_byte->IsOneByteRepresentation());
            CHECK(shared_external_one_byte->MakeExternal(
                isolate, new ExternalOneByteResource(kExternalOneByteValue,
                                                    kExternalOneByteValueLength)));

            InvokeSharedStringTransitionGC(isolate->heap());

            CHECK(IsExternalString(*shared_external_one_byte));
            CHECK_EQ(SHARED_EXTERNAL_ONE_BYTE_STRING_TYPE,
                     shared_external_one_byte->map()->instance_type());

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(shared_external_one_byte);
            shared_external_one_byte_address =
                shared_external_one_byte->address();
            map_address = shared_external_one_byte->map()->address();
            return artifacts;
          });

  Node* shared_external_one_byte =
      GetSnapshotNode(generator.get(), shared_external_one_byte_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(shared_external_one_byte);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(shared_external_one_byte, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), shared_external_one_byte,
                          Edge::Type::kInternal, "map", map));
  CHECK_EQ(
      1, CountExactNamedEdges(generator.get(), shared_external_one_byte,
                              Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(shared_external_one_byte,
                                         Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(shared_external_one_byte,
                                         Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(shared_external_one_byte,
                                         Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(shared_external_one_byte,
                                         Edge::Type::kProperty, map));
  MarkCoveredInstanceType(SHARED_EXTERNAL_ONE_BYTE_STRING_TYPE);
}

UNINITIALIZED_TEST(HeapDumpSharedExternalTwoByteStringExactMapEdge) {
  Address shared_external_two_byte_address = 0;
  Address map_address = 0;

  v8_flags.shared_string_table = true;
  v8_flags.shared_strings = true;
  FlagList::EnforceFlagImplications();

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            const uint16_t kPayload[] = {0x4F60, 0x597D, 0x4E16, 0x754C,
                                        0x4EBA, 0x4E16, 0x7ECF, 0x5386,
                                        0x65F6, 0x95F4, 0x6D4B, 0x8BD5};
            const int kPayloadLength = sizeof(kPayload) / sizeof(kPayload[0]);
            DirectHandle<String> two_byte_seq =
                v8::Utils::OpenDirectHandle(*v8::String::NewFromTwoByte(
                    v8_isolate, kPayload, v8::NewStringType::kNormal,
                    kPayloadLength)
                                                .ToLocalChecked());
            DirectHandle<String> shared_external_two_byte =
                String::Share(isolate, two_byte_seq);
            CHECK(shared_external_two_byte->IsShared());
            CHECK(shared_external_two_byte->IsTwoByteRepresentation());
            CHECK(shared_external_two_byte->MakeExternal(
                isolate, new ExternalTwoByteResource(kPayload, kPayloadLength)));

            InvokeSharedStringTransitionGC(isolate->heap());

            CHECK(IsExternalString(*shared_external_two_byte));
            CHECK_EQ(SHARED_EXTERNAL_TWO_BYTE_STRING_TYPE,
                     shared_external_two_byte->map()->instance_type());

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(shared_external_two_byte);
            shared_external_two_byte_address =
                shared_external_two_byte->address();
            map_address = shared_external_two_byte->map()->address();
            return artifacts;
          });

  Node* shared_external_two_byte =
      GetSnapshotNode(generator.get(), shared_external_two_byte_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(shared_external_two_byte);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(shared_external_two_byte, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), shared_external_two_byte,
                          Edge::Type::kInternal, "map", map));
  CHECK_EQ(
      1, CountExactNamedEdges(generator.get(), shared_external_two_byte,
                              Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(shared_external_two_byte,
                                         Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(shared_external_two_byte,
                                         Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(shared_external_two_byte,
                                         Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(shared_external_two_byte,
                                         Edge::Type::kProperty, map));
  MarkCoveredInstanceType(SHARED_EXTERNAL_TWO_BYTE_STRING_TYPE);
}

UNINITIALIZED_TEST(HeapDumpSharedUncachedExternalOneByteStringExactMapEdge) {
#if V8_ENABLE_SANDBOX
  // Sandbox builds have no in-place size window that can transition this
  // shared string to the uncached map. Assert the real fallback map and edge
  // shape instead of faking exact uncached coverage.
  constexpr InstanceType kExpectedType = SHARED_EXTERNAL_ONE_BYTE_STRING_TYPE;
#else
  constexpr InstanceType kExpectedType =
      SHARED_UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE;
#endif
  Address shared_uncached_external_one_byte_address = 0;
  Address map_address = 0;

  v8_flags.shared_string_table = true;
  v8_flags.shared_strings = true;
  FlagList::EnforceFlagImplications();

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            constexpr char kPayload[] = "abcdefghijkl";
            constexpr size_t kPayloadLength =
                sizeof(kPayload) / sizeof(kPayload[0]) - 1;
            DirectHandle<String> one_byte_seq =
                isolate->factory()->NewStringFromAsciiChecked(kPayload);
            DirectHandle<String> shared_uncached_external_one_byte =
                String::Share(isolate, one_byte_seq);
            CHECK(shared_uncached_external_one_byte->IsShared());
            CHECK(shared_uncached_external_one_byte->IsOneByteRepresentation());
            CHECK(shared_uncached_external_one_byte->MakeExternal(
                isolate,
                new UncachedExternalOneByteResource(kPayload, kPayloadLength)));

            InvokeSharedStringTransitionGC(isolate->heap());

            CHECK(IsExternalString(*shared_uncached_external_one_byte));
            CHECK_EQ(kExpectedType,
                     shared_uncached_external_one_byte->map()->instance_type());
#if V8_ENABLE_SANDBOX
            CHECK(!Cast<ExternalString>(shared_uncached_external_one_byte)
                       ->is_uncached());
#else
            CHECK(Cast<ExternalString>(shared_uncached_external_one_byte)
                      ->is_uncached());
#endif

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(shared_uncached_external_one_byte);
            shared_uncached_external_one_byte_address =
                shared_uncached_external_one_byte->address();
            map_address = shared_uncached_external_one_byte->map()->address();
            return artifacts;
          });

  Node* shared_uncached_external_one_byte =
      GetSnapshotNode(generator.get(),
                      shared_uncached_external_one_byte_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(shared_uncached_external_one_byte);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(shared_uncached_external_one_byte, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CheckExactInternalStringEdgeNoWrongKind(
      generator.get(), shared_uncached_external_one_byte, "map", map);
  MarkCoveredInstanceType(kExpectedType);
}

UNINITIALIZED_TEST(HeapDumpSharedUncachedExternalTwoByteStringExactMapEdge) {
#if V8_ENABLE_SANDBOX
  // Sandbox builds have no in-place size window that can transition this
  // shared string to the uncached map. Assert the real fallback map and edge
  // shape instead of faking exact uncached coverage.
  constexpr InstanceType kExpectedType = SHARED_EXTERNAL_TWO_BYTE_STRING_TYPE;
#else
  constexpr InstanceType kExpectedType =
      SHARED_UNCACHED_EXTERNAL_TWO_BYTE_STRING_TYPE;
#endif
  Address shared_uncached_external_two_byte_address = 0;
  Address map_address = 0;

  v8_flags.shared_string_table = true;
  v8_flags.shared_strings = true;
  FlagList::EnforceFlagImplications();

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            const uint16_t kPayload[] = {0x4F60, 0x597D, 0x4E16};
            const int kPayloadLength = sizeof(kPayload) / sizeof(kPayload[0]);
            DirectHandle<String> two_byte_seq =
                v8::Utils::OpenDirectHandle(*v8::String::NewFromTwoByte(
                    v8_isolate, kPayload, v8::NewStringType::kNormal,
                    kPayloadLength)
                                                .ToLocalChecked());
            DirectHandle<String> shared_uncached_external_two_byte =
                String::Share(isolate, two_byte_seq);
            CHECK(shared_uncached_external_two_byte->IsShared());
            CHECK(shared_uncached_external_two_byte->IsTwoByteRepresentation());
            CHECK(shared_uncached_external_two_byte->MakeExternal(
                isolate,
                new UncachedExternalTwoByteResource(kPayload, kPayloadLength)));

            InvokeSharedStringTransitionGC(isolate->heap());

            CHECK(IsExternalString(*shared_uncached_external_two_byte));
            CHECK_EQ(kExpectedType,
                     shared_uncached_external_two_byte->map()->instance_type());
#if V8_ENABLE_SANDBOX
            CHECK(!Cast<ExternalString>(shared_uncached_external_two_byte)
                       ->is_uncached());
#else
            CHECK(Cast<ExternalString>(shared_uncached_external_two_byte)
                      ->is_uncached());
#endif

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(shared_uncached_external_two_byte);
            shared_uncached_external_two_byte_address =
                shared_uncached_external_two_byte->address();
            map_address = shared_uncached_external_two_byte->map()->address();
            return artifacts;
          });

  Node* shared_uncached_external_two_byte =
      GetSnapshotNode(generator.get(),
                      shared_uncached_external_two_byte_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(shared_uncached_external_two_byte);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(shared_uncached_external_two_byte, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CheckExactInternalStringEdgeNoWrongKind(
      generator.get(), shared_uncached_external_two_byte, "map", map);
  MarkCoveredInstanceType(kExpectedType);
}

UNINITIALIZED_TEST(HeapDumpConsOneByteStringExactFirstSecondEdges) {
  Address cons_string_address = 0;
  Address first_part_address = 0;
  Address second_part_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DumpArtifacts artifacts;
            DirectHandle<String> first =
                isolate->factory()->NewStringFromAsciiChecked(
                    "heap-dump-cons-one-byte-left");
            DirectHandle<String> second =
                isolate->factory()->NewStringFromAsciiChecked(
                    "heap-dump-cons-one-byte-right");
            DirectHandle<String> cons =
                isolate->factory()->NewConsString(first, second).ToHandleChecked();
            CHECK_EQ(CONS_ONE_BYTE_STRING_TYPE, cons->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            artifacts.keep_alive.push_back(first);
            artifacts.keep_alive.push_back(second);
            artifacts.keep_alive.push_back(cons);
            cons_string_address = cons->address();
            first_part_address = first->address();
            second_part_address = second->address();
            map_address = cons->map()->address();
            return artifacts;
          });

  Node* cons_string = GetSnapshotNode(generator.get(), cons_string_address);
  Node* first_part = GetSnapshotNode(generator.get(), first_part_address);
  Node* second_part = GetSnapshotNode(generator.get(), second_part_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(cons_string);
  CHECK_NOT_NULL(first_part);
  CHECK_NOT_NULL(second_part);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), cons_string, Edge::Type::kInternal,
                          "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), cons_string,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(cons_string, Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(cons_string, Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(cons_string, Edge::Type::kProperty,
                                         map));
  CheckExactInternalStringEdgeNoWrongKind(generator.get(), cons_string, "first",
                                          first_part);
  CheckExactInternalStringEdgeNoWrongKind(generator.get(), cons_string,
                                          "second", second_part);
  MarkCoveredInstanceType(CONS_ONE_BYTE_STRING_TYPE);
}

UNINITIALIZED_TEST(HeapDumpConsTwoByteStringExactFirstSecondEdges) {
  Address cons_string_address = 0;
  Address first_part_address = 0;
  Address second_part_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            const uint16_t kConsTwoByteLeft[] = {
                0x4F60, 0x597D, 0x4E16, 0x754C, 0x4EBA, 0x4E16, 0x7ECF, 0x5386,
                0x65F6, 0x95F4, 0x6D4B, 0x8BD5, 0x65B0, 0x65F6, 0x95F4,
                0x7ECF, 0x53D8, 0x5316};
            const uint16_t kConsTwoByteRight[] = {
                0x6D4B, 0x8BD5, 0x4ECA, 0x65F6, 0x95F4, 0x6D4B, 0x8BD5, 0x4E86,
                0x65F6, 0x95F4, 0x6D4B, 0x8BD5, 0x6587, 0x4EF6, 0x5927,
                0x65F6, 0x7ECF, 0x6781};
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::String> first_value =
                v8::String::NewFromTwoByte(v8_isolate, kConsTwoByteLeft,
                                           v8::NewStringType::kNormal, 18)
                    .ToLocalChecked();
            DirectHandle<String> first = v8::Utils::OpenDirectHandle(*first_value);
            v8::Local<v8::String> second_value =
                v8::String::NewFromTwoByte(v8_isolate, kConsTwoByteRight,
                                           v8::NewStringType::kNormal, 18)
                    .ToLocalChecked();
            DirectHandle<String> second =
                v8::Utils::OpenDirectHandle(*second_value);
            DirectHandle<String> cons =
                isolate->factory()->NewConsString(first, second).ToHandleChecked();
            CHECK_EQ(CONS_TWO_BYTE_STRING_TYPE, cons->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(first);
            artifacts.keep_alive.push_back(second);
            artifacts.keep_alive.push_back(cons);
            cons_string_address = cons->address();
            first_part_address = first->address();
            second_part_address = second->address();
            map_address = cons->map()->address();
            return artifacts;
          });

  Node* cons_string = GetSnapshotNode(generator.get(), cons_string_address);
  Node* first_part = GetSnapshotNode(generator.get(), first_part_address);
  Node* second_part = GetSnapshotNode(generator.get(), second_part_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(cons_string);
  CHECK_NOT_NULL(first_part);
  CHECK_NOT_NULL(second_part);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), cons_string, Edge::Type::kInternal,
                          "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), cons_string,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(cons_string, Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(cons_string, Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(cons_string, Edge::Type::kProperty,
                                         map));
  CheckExactInternalStringEdgeNoWrongKind(generator.get(), cons_string, "first",
                                          first_part);
  CheckExactInternalStringEdgeNoWrongKind(generator.get(), cons_string,
                                          "second", second_part);
  MarkCoveredInstanceType(CONS_TWO_BYTE_STRING_TYPE);
}

UNINITIALIZED_TEST(HeapDumpSlicedOneByteStringExactParentEdge) {
  Address sliced_string_address = 0;
  Address parent_string_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DumpArtifacts artifacts;
            DirectHandle<String> parent =
                isolate->factory()->NewStringFromAsciiChecked(
                    "heap-dump-sliced-one-byte-parent");
            DirectHandle<String> sliced =
                isolate->factory()
                    ->NewSubString(parent, 1, parent->length())
                    ;
            CHECK_EQ(SLICED_ONE_BYTE_STRING_TYPE, sliced->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            artifacts.keep_alive.push_back(parent);
            artifacts.keep_alive.push_back(sliced);
            sliced_string_address = sliced->address();
            parent_string_address = parent->address();
            map_address = sliced->map()->address();
            return artifacts;
          });

  Node* sliced_string = GetSnapshotNode(generator.get(), sliced_string_address);
  Node* parent_string = GetSnapshotNode(generator.get(), parent_string_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(sliced_string);
  CHECK_NOT_NULL(parent_string);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), sliced_string, Edge::Type::kInternal,
                          "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), sliced_string,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(sliced_string, Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(sliced_string, Edge::Type::kHidden,
                                         map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(sliced_string, Edge::Type::kProperty,
                                         map));
  CheckExactInternalStringEdgeNoWrongKind(generator.get(), sliced_string,
                                          "parent", parent_string);
  MarkCoveredInstanceType(SLICED_ONE_BYTE_STRING_TYPE);
}

UNINITIALIZED_TEST(HeapDumpSlicedTwoByteStringExactParentEdge) {
  Address sliced_string_address = 0;
  Address parent_string_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            const uint16_t kParentPayload[] = {
                0x4F60, 0x597D, 0x4E16, 0x754C, 0x4EBA, 0x4E16, 0x7ECF, 0x5386,
                0x65F6, 0x95F4, 0x6D4B, 0x8BD5, 0x4ECA, 0x65F6, 0x95F4,
                0x6D4B, 0x8BD5, 0x4E16, 0x754C, 0x4F60, 0x597D, 0x4EBA,
                0x4E16, 0x7ECF, 0x5386, 0x65F6, 0x95F4, 0x6D4B, 0x8BD5,
                0x4ECA, 0x65F6, 0x95F4, 0x6D4B, 0x8BD5, 0x4E16, 0x754C};
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::String> parent_value =
                v8::String::NewFromTwoByte(v8_isolate, kParentPayload,
                                           v8::NewStringType::kNormal, 35)
                    .ToLocalChecked();
            DirectHandle<String> parent = v8::Utils::OpenDirectHandle(*parent_value);
            DirectHandle<String> sliced =
                isolate->factory()
                    ->NewSubString(parent, 1, parent->length())
                    ;
            CHECK_EQ(SLICED_TWO_BYTE_STRING_TYPE, sliced->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(parent);
            artifacts.keep_alive.push_back(sliced);
            sliced_string_address = sliced->address();
            parent_string_address = parent->address();
            map_address = sliced->map()->address();
            return artifacts;
          });

  Node* sliced_string = GetSnapshotNode(generator.get(), sliced_string_address);
  Node* parent_string = GetSnapshotNode(generator.get(), parent_string_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(sliced_string);
  CHECK_NOT_NULL(parent_string);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), sliced_string, Edge::Type::kInternal,
                          "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), sliced_string,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(sliced_string, Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(sliced_string, Edge::Type::kHidden,
                                         map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(sliced_string, Edge::Type::kProperty,
                                         map));
  CheckExactInternalStringEdgeNoWrongKind(generator.get(), sliced_string,
                                          "parent", parent_string);
  MarkCoveredInstanceType(SLICED_TWO_BYTE_STRING_TYPE);
}

UNINITIALIZED_TEST(HeapDumpThinOneByteStringActualEdge) {
  Address thin_string_address = 0;
  Address actual_string_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            FlagScope<bool> allow_natives_syntax(
                &v8_flags.allow_natives_syntax, true);
            DumpArtifacts artifacts;
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            RunScript(context, R"(
              globalThis.__heap_dump_thin_string =
                  %ConstructThinString('heap-dump-thin-one-byte-surface');
            )");
            DirectHandle<String> duplicate = v8::Utils::OpenDirectHandle(
                *v8::Local<v8::String>::Cast(
                    context->Global()
                        ->Get(context, NewApiString(v8_isolate,
                                                    "__heap_dump_thin_string"))
                        .ToLocalChecked()));
            CHECK(IsThinString(*duplicate));
            CHECK_EQ(THIN_ONE_BYTE_STRING_TYPE,
                     duplicate->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            CHECK(IsThinString(*duplicate));
            CHECK(IsString(*Cast<HeapObject>(Cast<ThinString>(*duplicate)->actual())));
            artifacts.keep_alive.push_back(duplicate);

            thin_string_address = duplicate->address();
            actual_string_address = Cast<ThinString>(*duplicate)->actual()->address();
            map_address = duplicate->map()->address();
            return artifacts;
          });

  Node* thin_string = GetSnapshotNode(generator.get(), thin_string_address);
  Node* actual_string = GetSnapshotNode(generator.get(), actual_string_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(thin_string);
  CHECK_NOT_NULL(actual_string);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(thin_string, Node::Type::kString));
  CHECK(NodeHasType(actual_string, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), thin_string, Edge::Type::kInternal,
                          "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), thin_string,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(thin_string, Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(thin_string, Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(thin_string, Edge::Type::kProperty, map));
  CheckExactInternalStringEdgeNoWrongKind(generator.get(), thin_string,
                                          "actual", actual_string);
  MarkCoveredInstanceType(THIN_ONE_BYTE_STRING_TYPE);
}

UNINITIALIZED_TEST(HeapDumpThinTwoByteStringActualEdge) {
  Address thin_string_address = 0;
  Address actual_string_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            FlagScope<bool> allow_natives_syntax(
                &v8_flags.allow_natives_syntax, true);
            DumpArtifacts artifacts;
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            RunScript(context, R"(
              globalThis.__heap_dump_thin_two_byte_string =
                  %ConstructThinString(
                      '\u4F60\u597D\u4E16\u754C\u6D4B\u8BD5'
                      + '\u7CFB\u7EDF\u7ED3\u6784\u6587\u672C\u5B57'
                      + '\u7B26\u6D4B\u8BD5\u5B57\u7B26\u7D22'
                      + '\u5F15\u5B57\u7B26\u62E5\u6709\u6548\u957F'
                      + '\u5EA6\u7684\u5B57\u7B26\u5E8F\u5217');
            )");
            DirectHandle<String> duplicate = v8::Utils::OpenDirectHandle(
                *v8::Local<v8::String>::Cast(
                    context->Global()
                        ->Get(context, NewApiString(v8_isolate,
                                                    "__heap_dump_thin_two_byte_string"))
                        .ToLocalChecked()));
            CHECK(IsThinString(*duplicate));
            CHECK_EQ(THIN_TWO_BYTE_STRING_TYPE,
                     duplicate->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            CHECK(IsThinString(*duplicate));
            CHECK(IsString(*Cast<HeapObject>(Cast<ThinString>(*duplicate)->actual())));
            artifacts.keep_alive.push_back(duplicate);

            thin_string_address = duplicate->address();
            actual_string_address = Cast<ThinString>(*duplicate)->actual()->address();
            map_address = duplicate->map()->address();
            return artifacts;
          });

  Node* thin_string = GetSnapshotNode(generator.get(), thin_string_address);
  Node* actual_string = GetSnapshotNode(generator.get(), actual_string_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(thin_string);
  CHECK_NOT_NULL(actual_string);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(thin_string, Node::Type::kString));
  CHECK(NodeHasType(actual_string, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), thin_string, Edge::Type::kInternal,
                          "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), thin_string,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(thin_string, Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(thin_string, Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(thin_string, Edge::Type::kProperty,
                                         map));
  CheckExactInternalStringEdgeNoWrongKind(generator.get(), thin_string,
                                          "actual", actual_string);
  MarkCoveredInstanceType(THIN_TWO_BYTE_STRING_TYPE);
}

UNINITIALIZED_TEST(HeapDumpExternalOneByteStringExactMapEdge) {
  Address external_string_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<String> external_string =
                isolate->factory()
                    ->NewExternalStringFromOneByte(
                        new ExternalOneByteResource(kExternalOneByteValue,
                                                   kExternalOneByteValueLength))
                    .ToHandleChecked();
            CHECK(IsExternalString(*external_string));
            CHECK_EQ(EXTERNAL_ONE_BYTE_STRING_TYPE,
                     external_string->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(external_string);
            external_string_address = external_string->address();
            map_address = external_string->map()->address();
            return artifacts;
          });

  Node* external_string = GetSnapshotNode(generator.get(), external_string_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(external_string);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(external_string, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), external_string, Edge::Type::kInternal,
                          "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), external_string,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(external_string,
                                         Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(external_string,
                                         Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(external_string, Edge::Type::kWeak,
                                         map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(external_string, Edge::Type::kProperty,
                                         map));
  MarkCoveredInstanceType(EXTERNAL_ONE_BYTE_STRING_TYPE);
}

UNINITIALIZED_TEST(HeapDumpExternalTwoByteStringExactMapEdge) {
  Address external_string_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<String> external_string =
                isolate->factory()
                    ->NewExternalStringFromTwoByte(
                        new ExternalTwoByteResource(kExternalTwoByteValue,
                                                   kExternalTwoByteValueLength))
                    .ToHandleChecked();
            CHECK(IsExternalString(*external_string));
            CHECK_EQ(EXTERNAL_TWO_BYTE_STRING_TYPE,
                     external_string->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(external_string);
            external_string_address = external_string->address();
            map_address = external_string->map()->address();
            return artifacts;
          });

  Node* external_string = GetSnapshotNode(generator.get(), external_string_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(external_string);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(external_string, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), external_string, Edge::Type::kInternal,
                          "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), external_string,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(external_string,
                                         Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(external_string,
                                         Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(external_string, Edge::Type::kWeak,
                                         map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(external_string, Edge::Type::kProperty,
                                         map));
  MarkCoveredInstanceType(EXTERNAL_TWO_BYTE_STRING_TYPE);
}

UNINITIALIZED_TEST(HeapDumpUncachedExternalOneByteStringExactMapEdge) {
  Address external_string_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<String> external_string =
                isolate->factory()
                    ->NewExternalStringFromOneByte(new UncachedExternalOneByteResource(
                        kExternalOneByteValue,
                        kExternalOneByteValueLength))
                    .ToHandleChecked();
            CHECK(IsExternalString(*external_string));
            CHECK_EQ(UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE,
                     external_string->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(external_string);
            external_string_address = external_string->address();
            map_address = external_string->map()->address();
            return artifacts;
          });

  Node* external_string = GetSnapshotNode(generator.get(), external_string_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(external_string);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(external_string, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), external_string, Edge::Type::kInternal,
                          "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), external_string,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(external_string,
                                         Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(external_string,
                                         Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(external_string, Edge::Type::kWeak,
                                         map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(external_string, Edge::Type::kProperty,
                                         map));
  MarkCoveredInstanceType(UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE);
}

UNINITIALIZED_TEST(HeapDumpUncachedExternalTwoByteStringExactMapEdge) {
  Address external_string_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<String> external_string =
                isolate->factory()
                    ->NewExternalStringFromTwoByte(new UncachedExternalTwoByteResource(
                        kExternalTwoByteValue,
                        kExternalTwoByteValueLength))
                    .ToHandleChecked();
            CHECK(IsExternalString(*external_string));
            CHECK_EQ(UNCACHED_EXTERNAL_TWO_BYTE_STRING_TYPE,
                     external_string->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(external_string);
            external_string_address = external_string->address();
            map_address = external_string->map()->address();
            return artifacts;
          });

  Node* external_string = GetSnapshotNode(generator.get(), external_string_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(external_string);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(external_string, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), external_string, Edge::Type::kInternal,
                          "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), external_string,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(external_string,
                                         Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(external_string,
                                         Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(external_string, Edge::Type::kWeak,
                                         map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(external_string, Edge::Type::kProperty,
                                         map));
  MarkCoveredInstanceType(UNCACHED_EXTERNAL_TWO_BYTE_STRING_TYPE);
}

UNINITIALIZED_TEST(HeapDumpExternalInternalizedOneByteStringExactMapEdge) {
  Address internalized_string_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<String> external_string =
                isolate->factory()
                    ->NewExternalStringFromOneByte(
                        new ExternalOneByteResource(kExternalOneByteValue,
                                                   kExternalOneByteValueLength))
                    .ToHandleChecked();
            CHECK(IsExternalString(*external_string));
            CHECK(!Cast<ExternalString>(external_string)->is_uncached());
            DirectHandle<String> internalized_one_byte =
                isolate->factory()->InternalizeString(external_string);
            CHECK(IsInternalizedString(*internalized_one_byte));
            CHECK_EQ(EXTERNAL_INTERNALIZED_ONE_BYTE_STRING_TYPE,
                     internalized_one_byte->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(internalized_one_byte);
            internalized_string_address = internalized_one_byte->address();
            map_address = internalized_one_byte->map()->address();
            return artifacts;
          });

  Node* internalized_string = GetSnapshotNode(generator.get(),
                                             internalized_string_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(internalized_string);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(internalized_string, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), internalized_string,
                          Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), internalized_string,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(internalized_string,
                                         Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(internalized_string,
                                         Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(internalized_string, Edge::Type::kWeak,
                                         map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(internalized_string,
                                         Edge::Type::kProperty, map));
  MarkCoveredInstanceType(EXTERNAL_INTERNALIZED_ONE_BYTE_STRING_TYPE);
}

UNINITIALIZED_TEST(HeapDumpExternalInternalizedTwoByteStringExactMapEdge) {
  Address internalized_string_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<String> external_string =
                isolate->factory()
                    ->NewExternalStringFromTwoByte(
                        new ExternalTwoByteResource(kExternalTwoByteValue,
                                                   kExternalTwoByteValueLength))
                    .ToHandleChecked();
            CHECK(IsExternalString(*external_string));
            CHECK(!Cast<ExternalString>(external_string)->is_uncached());
            DirectHandle<String> internalized_two_byte =
                isolate->factory()->InternalizeString(external_string);
            CHECK(IsInternalizedString(*internalized_two_byte));
            CHECK_EQ(EXTERNAL_INTERNALIZED_TWO_BYTE_STRING_TYPE,
                     internalized_two_byte->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(internalized_two_byte);
            internalized_string_address = internalized_two_byte->address();
            map_address = internalized_two_byte->map()->address();
            return artifacts;
          });

  Node* internalized_string = GetSnapshotNode(generator.get(),
                                             internalized_string_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(internalized_string);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(internalized_string, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), internalized_string,
                          Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), internalized_string,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(internalized_string,
                                         Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(internalized_string,
                                         Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(internalized_string, Edge::Type::kWeak,
                                         map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(internalized_string,
                                         Edge::Type::kProperty, map));
  MarkCoveredInstanceType(EXTERNAL_INTERNALIZED_TWO_BYTE_STRING_TYPE);
}

UNINITIALIZED_TEST(HeapDumpUncachedExternalInternalizedOneByteStringExactMapEdge) {
#if V8_ENABLE_SANDBOX
  // Sandbox builds have no in-place size window that can transition this
  // internalized string to the uncached map. Assert the real fallback map and
  // edge shape instead of faking exact uncached coverage.
  constexpr InstanceType kExpectedType =
      EXTERNAL_INTERNALIZED_ONE_BYTE_STRING_TYPE;
#else
  constexpr InstanceType kExpectedType =
      UNCACHED_EXTERNAL_INTERNALIZED_ONE_BYTE_STRING_TYPE;
#endif
  Address internalized_string_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            constexpr char kPayload[] = "abcdefghijkl";
            constexpr size_t kPayloadLength =
                sizeof(kPayload) / sizeof(kPayload[0]) - 1;
            DirectHandle<String> one_byte_string =
                isolate->factory()->NewStringFromAsciiChecked(kPayload);
            DirectHandle<String> internalized_one_byte =
                isolate->factory()->InternalizeString(one_byte_string);
            CHECK(IsInternalizedString(*internalized_one_byte));
            CHECK(internalized_one_byte->MakeExternal(
                isolate,
                new UncachedExternalOneByteResource(kPayload, kPayloadLength)));
            CHECK(IsExternalString(*internalized_one_byte));
#if V8_ENABLE_SANDBOX
            CHECK(!Cast<ExternalString>(internalized_one_byte)->is_uncached());
#else
            CHECK(Cast<ExternalString>(internalized_one_byte)->is_uncached());
#endif
            CHECK_EQ(kExpectedType,
                     internalized_one_byte->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(internalized_one_byte);
            internalized_string_address = internalized_one_byte->address();
            map_address = internalized_one_byte->map()->address();
            return artifacts;
          });

  Node* internalized_string = GetSnapshotNode(generator.get(),
                                             internalized_string_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(internalized_string);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(internalized_string, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CheckExactInternalStringEdgeNoWrongKind(generator.get(), internalized_string,
                                          "map", map);
  MarkCoveredInstanceType(kExpectedType);
}

UNINITIALIZED_TEST(HeapDumpUncachedExternalInternalizedTwoByteStringExactMapEdge) {
#if V8_ENABLE_SANDBOX
  // Sandbox builds have no in-place size window that can transition this
  // internalized string to the uncached map. Assert the real fallback map and
  // edge shape instead of faking exact uncached coverage.
  constexpr InstanceType kExpectedType =
      EXTERNAL_INTERNALIZED_TWO_BYTE_STRING_TYPE;
#else
  constexpr InstanceType kExpectedType =
      UNCACHED_EXTERNAL_INTERNALIZED_TWO_BYTE_STRING_TYPE;
#endif
  Address internalized_string_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            const uint16_t kPayload[] = {0x4F60, 0x597D, 0x4E16};
            const int kPayloadLength = sizeof(kPayload) / sizeof(kPayload[0]);
            DirectHandle<String> two_byte_string =
                v8::Utils::OpenDirectHandle(*v8::String::NewFromTwoByte(
                    v8_isolate, kPayload, v8::NewStringType::kNormal,
                    kPayloadLength)
                                                .ToLocalChecked());
            DirectHandle<String> internalized_two_byte =
                isolate->factory()->InternalizeString(two_byte_string);
            CHECK(IsInternalizedString(*internalized_two_byte));
            CHECK(internalized_two_byte->MakeExternal(
                isolate,
                new UncachedExternalTwoByteResource(kPayload, kPayloadLength)));
            CHECK(IsExternalString(*internalized_two_byte));
#if V8_ENABLE_SANDBOX
            CHECK(!Cast<ExternalString>(internalized_two_byte)->is_uncached());
#else
            CHECK(Cast<ExternalString>(internalized_two_byte)->is_uncached());
#endif
            CHECK_EQ(kExpectedType,
                     internalized_two_byte->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(internalized_two_byte);
            internalized_string_address = internalized_two_byte->address();
            map_address = internalized_two_byte->map()->address();
            return artifacts;
          });

  Node* internalized_string = GetSnapshotNode(generator.get(),
                                             internalized_string_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(internalized_string);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(internalized_string, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CheckExactInternalStringEdgeNoWrongKind(generator.get(), internalized_string,
                                          "map", map);
  MarkCoveredInstanceType(kExpectedType);
}

UNINITIALIZED_TEST(HeapDumpInternalizedOneByteStringExactMapEdge) {
  Address internalized_one_byte_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<String> one_byte =
                isolate->factory()->NewStringFromAsciiChecked(
                    "heap-dump-internalized-one-byte");
            DirectHandle<String> internalized_one_byte =
                isolate->factory()->InternalizeString(one_byte);
            CHECK(IsInternalizedString(*internalized_one_byte));
            CHECK_EQ(INTERNALIZED_ONE_BYTE_STRING_TYPE,
                     internalized_one_byte->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(internalized_one_byte);
            internalized_one_byte_address = internalized_one_byte->address();
            map_address = internalized_one_byte->map()->address();
            return artifacts;
          });

  Node* internalized_one_byte = GetSnapshotNode(
      generator.get(), internalized_one_byte_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(internalized_one_byte);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(internalized_one_byte, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), internalized_one_byte,
                          Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), internalized_one_byte,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(internalized_one_byte,
                                         Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(internalized_one_byte,
                                         Edge::Type::kProperty, map));
  MarkCoveredInstanceType(INTERNALIZED_ONE_BYTE_STRING_TYPE);
}

UNINITIALIZED_TEST(HeapDumpInternalizedTwoByteStringExactMapEdge) {
  Address internalized_two_byte_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            const uint16_t kPayload[] = {
                0x4F60, 0x597D, 0x4E16, 0x754C, 0x6D4B, 0x8BD5};
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::String> two_byte_value =
                v8::String::NewFromTwoByte(v8_isolate, kPayload,
                                           v8::NewStringType::kNormal, 6)
                    .ToLocalChecked();
            DirectHandle<String> two_byte =
                v8::Utils::OpenDirectHandle(*two_byte_value);
            DirectHandle<String> internalized_two_byte =
                isolate->factory()->InternalizeString(two_byte);
            CHECK(IsInternalizedString(*internalized_two_byte));
            CHECK_EQ(INTERNALIZED_TWO_BYTE_STRING_TYPE,
                     internalized_two_byte->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(internalized_two_byte);
            internalized_two_byte_address = internalized_two_byte->address();
            map_address = internalized_two_byte->map()->address();
            return artifacts;
          });

  Node* internalized_two_byte = GetSnapshotNode(
      generator.get(), internalized_two_byte_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(internalized_two_byte);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(internalized_two_byte, Node::Type::kString));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), internalized_two_byte,
                          Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), internalized_two_byte,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(internalized_two_byte,
                                         Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(internalized_two_byte,
                                         Edge::Type::kProperty, map));
  MarkCoveredInstanceType(INTERNALIZED_TWO_BYTE_STRING_TYPE);
}

#else
UNINITIALIZED_TEST(HeapDumpSeqOneByteStringExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpSeqTwoByteStringExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpSharedSeqTwoByteStringExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpSharedSeqOneByteStringExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpSharedExternalOneByteStringExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpSharedExternalTwoByteStringExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpSharedUncachedExternalOneByteStringExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpSharedUncachedExternalTwoByteStringExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpConsOneByteStringExactFirstSecondEdges) {}
UNINITIALIZED_TEST(HeapDumpConsTwoByteStringExactFirstSecondEdges) {}
UNINITIALIZED_TEST(HeapDumpSlicedOneByteStringExactParentEdge) {}
UNINITIALIZED_TEST(HeapDumpSlicedTwoByteStringExactParentEdge) {}
UNINITIALIZED_TEST(HeapDumpThinOneByteStringActualEdge) {}
UNINITIALIZED_TEST(HeapDumpThinTwoByteStringActualEdge) {}
UNINITIALIZED_TEST(HeapDumpExternalOneByteStringExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpExternalTwoByteStringExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpUncachedExternalOneByteStringExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpUncachedExternalTwoByteStringExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpExternalInternalizedOneByteStringExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpExternalInternalizedTwoByteStringExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpUncachedExternalInternalizedOneByteStringExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpUncachedExternalInternalizedTwoByteStringExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpInternalizedOneByteStringExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpInternalizedTwoByteStringExactMapEdge) {}
#endif

}  // namespace heap
}  // namespace internal
}  // namespace v8
