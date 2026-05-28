#include "arkweb/chromium_ext/v8/dfx/heap_dump/tests/cctest/heap-dump-test-utils.h"

#include "include/v8-primitive-object.h"
#include "include/v8-primitive.h"
#include "src/objects/api-callbacks-inl.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/bigint.h"
#include "src/objects/bytecode-array-inl.h"
#include "src/objects/contexts-inl.h"
#include "src/objects/descriptor-array-inl.h"
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
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/scope-info.h"
#include "src/objects/struct-inl.h"

namespace v8 {
namespace internal {
namespace heap {

#if defined(OH_ENABLE_HEAP_DUMP_TEST) && defined(OH_ENABLE_HEAP_DUMP) && \
    defined(OH_ENABLE_HEAP_TRANSLATE)

using namespace heap_dump_test;

static void CheckExactInternalMapEdgeNoWrongKind(SnapshotGenerator* generator,
                                                const Node* object,
                                                const Node* map) {
  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CHECK(HasExactNamedEdge(generator, object, Edge::Type::kInternal, "map",
                          map));
  CHECK_EQ(1, CountExactNamedEdges(generator, object, Edge::Type::kInternal,
                                   "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(object, Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kProperty, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kShortcut, map));
}

static int CountEphemeronAnnotationEdges(SnapshotGenerator* generator,
                                         const Node* source,
                                         const Node* target) {
  CHECK_NOT_NULL(source);
  CHECK_NOT_NULL(target);
  std::vector<std::string> strings = SnapshotStringsById(generator);
  int count = 0;
  for (const Edge* edge : source->childs_) {
    if (edge->type_ != static_cast<uint32_t>(Edge::Type::kInternal) ||
        edge->to_node_ != target) {
      continue;
    }
    const std::string& name = SnapshotStringAt(strings, edge->name_or_index_);
    if (name.find("part of key (") != std::string::npos &&
        name.find(" -> value (") != std::string::npos &&
        name.find(" pair in WeakMap (table @") != std::string::npos) {
      ++count;
    }
  }
  return count;
}

UNINITIALIZED_TEST(HeapDumpWeakArraysExactWeakAndClearedSlotEdges) {
  Address weak_fixed_array_address = 0;
  Address weak_fixed_first_target_address = 0;
  Address weak_fixed_second_target_address = 0;
  Address weak_fixed_strong_target_address = 0;
  Address weak_fixed_cleared_target_address = 0;
  Address weak_array_list_address = 0;
  Address weak_array_list_first_target_address = 0;
  Address weak_array_list_second_target_address = 0;
  Address weak_array_list_strong_target_address = 0;
  Address weak_array_list_cleared_target_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DumpArtifacts artifacts;
            Factory* factory = isolate->factory();

            DirectHandle<FixedArray> weak_fixed_first_target =
                factory->NewFixedArray(1, AllocationType::kOld);
            DirectHandle<FixedArray> weak_fixed_second_target =
                factory->NewFixedArray(1, AllocationType::kOld);
            DirectHandle<FixedArray> weak_fixed_strong_target =
                factory->NewFixedArray(1, AllocationType::kOld);
            DirectHandle<FixedArray> weak_fixed_cleared_target =
                factory->NewFixedArray(1, AllocationType::kOld);
            weak_fixed_first_target->set(0, Smi::FromInt(3101));
            weak_fixed_second_target->set(0, Smi::FromInt(3102));
            weak_fixed_strong_target->set(0, Smi::FromInt(3104));
            weak_fixed_cleared_target->set(0, Smi::FromInt(3103));

            DirectHandle<WeakFixedArray> weak_fixed_array =
                factory->NewWeakFixedArray(4, AllocationType::kOld);
            weak_fixed_array->set(0, MakeWeak(*weak_fixed_first_target));
            weak_fixed_array->set(1, MakeWeak(*weak_fixed_second_target));
            weak_fixed_array->set(2, MakeWeak(*weak_fixed_cleared_target));
            weak_fixed_array->set(2, kClearedWeakValue);
            weak_fixed_array->set(3, *weak_fixed_strong_target);

            DirectHandle<FixedArray> weak_array_list_first_target =
                factory->NewFixedArray(1, AllocationType::kOld);
            DirectHandle<FixedArray> weak_array_list_second_target =
                factory->NewFixedArray(1, AllocationType::kOld);
            DirectHandle<FixedArray> weak_array_list_strong_target =
                factory->NewFixedArray(1, AllocationType::kOld);
            DirectHandle<FixedArray> weak_array_list_cleared_target =
                factory->NewFixedArray(1, AllocationType::kOld);
            weak_array_list_first_target->set(0, Smi::FromInt(3201));
            weak_array_list_second_target->set(0, Smi::FromInt(3202));
            weak_array_list_strong_target->set(0, Smi::FromInt(3204));
            weak_array_list_cleared_target->set(0, Smi::FromInt(3203));

            DirectHandle<WeakArrayList> weak_array_list =
                factory->NewWeakArrayList(4, AllocationType::kOld);
            weak_array_list->set_length(4);
            weak_array_list->Set(0, MakeWeak(*weak_array_list_first_target));
            weak_array_list->Set(1, MakeWeak(*weak_array_list_second_target));
            weak_array_list->Set(2, MakeWeak(*weak_array_list_cleared_target));
            weak_array_list->Set(2, kClearedWeakValue);
            weak_array_list->Set(3, *weak_array_list_strong_target);

            artifacts.keep_alive.push_back(weak_fixed_array);
            artifacts.keep_alive.push_back(weak_fixed_first_target);
            artifacts.keep_alive.push_back(weak_fixed_second_target);
            artifacts.keep_alive.push_back(weak_fixed_strong_target);
            artifacts.keep_alive.push_back(weak_fixed_cleared_target);
            artifacts.keep_alive.push_back(weak_array_list);
            artifacts.keep_alive.push_back(weak_array_list_first_target);
            artifacts.keep_alive.push_back(weak_array_list_second_target);
            artifacts.keep_alive.push_back(weak_array_list_strong_target);
            artifacts.keep_alive.push_back(weak_array_list_cleared_target);

            InvokeAtomicMajorGC(isolate->heap());

            Tagged<HeapObject> heap_object;
            CHECK(weak_fixed_array->get(0).GetHeapObjectIfWeak(&heap_object));
            CHECK_EQ(heap_object, *weak_fixed_first_target);
            CHECK(weak_fixed_array->get(1).GetHeapObjectIfWeak(&heap_object));
            CHECK_EQ(heap_object, *weak_fixed_second_target);
            CHECK(weak_fixed_array->get(2).IsCleared());
            CHECK(weak_fixed_array->get(3).GetHeapObjectIfStrong(&heap_object));
            CHECK_EQ(heap_object, *weak_fixed_strong_target);
            CHECK_EQ(WEAK_FIXED_ARRAY_TYPE,
                     weak_fixed_array->map()->instance_type());

            CHECK(weak_array_list->Get(0).GetHeapObjectIfWeak(&heap_object));
            CHECK_EQ(heap_object, *weak_array_list_first_target);
            CHECK(weak_array_list->Get(1).GetHeapObjectIfWeak(&heap_object));
            CHECK_EQ(heap_object, *weak_array_list_second_target);
            CHECK(weak_array_list->Get(2).IsCleared());
            CHECK(
                weak_array_list->Get(3).GetHeapObjectIfStrong(&heap_object));
            CHECK_EQ(heap_object, *weak_array_list_strong_target);
            CHECK_EQ(WEAK_ARRAY_LIST_TYPE,
                     weak_array_list->map()->instance_type());

            weak_fixed_array_address = weak_fixed_array->address();
            weak_fixed_first_target_address =
                weak_fixed_first_target->address();
            weak_fixed_second_target_address =
                weak_fixed_second_target->address();
            weak_fixed_strong_target_address =
                weak_fixed_strong_target->address();
            weak_fixed_cleared_target_address =
                weak_fixed_cleared_target->address();
            weak_array_list_address = weak_array_list->address();
            weak_array_list_first_target_address =
                weak_array_list_first_target->address();
            weak_array_list_second_target_address =
                weak_array_list_second_target->address();
            weak_array_list_strong_target_address =
                weak_array_list_strong_target->address();
            weak_array_list_cleared_target_address =
                weak_array_list_cleared_target->address();

            return artifacts;
          });

  Node* weak_fixed_array =
      GetSnapshotNode(generator.get(), weak_fixed_array_address);
  Node* weak_fixed_first_target =
      GetSnapshotNode(generator.get(), weak_fixed_first_target_address);
  Node* weak_fixed_second_target =
      GetSnapshotNode(generator.get(), weak_fixed_second_target_address);
  Node* weak_fixed_strong_target =
      GetSnapshotNode(generator.get(), weak_fixed_strong_target_address);
  Node* weak_fixed_cleared_target =
      GetSnapshotNode(generator.get(), weak_fixed_cleared_target_address);
  Node* weak_array_list =
      GetSnapshotNode(generator.get(), weak_array_list_address);
  Node* weak_array_list_first_target =
      GetSnapshotNode(generator.get(), weak_array_list_first_target_address);
  Node* weak_array_list_second_target =
      GetSnapshotNode(generator.get(), weak_array_list_second_target_address);
  Node* weak_array_list_strong_target =
      GetSnapshotNode(generator.get(), weak_array_list_strong_target_address);
  Node* weak_array_list_cleared_target =
      GetSnapshotNode(generator.get(), weak_array_list_cleared_target_address);
  CHECK_NOT_NULL(weak_fixed_array);
  CHECK_NOT_NULL(weak_fixed_first_target);
  CHECK_NOT_NULL(weak_fixed_second_target);
  CHECK_NOT_NULL(weak_fixed_strong_target);
  CHECK_NOT_NULL(weak_fixed_cleared_target);
  CHECK_NOT_NULL(weak_array_list);
  CHECK_NOT_NULL(weak_array_list_first_target);
  CHECK_NOT_NULL(weak_array_list_second_target);
  CHECK_NOT_NULL(weak_array_list_strong_target);
  CHECK_NOT_NULL(weak_array_list_cleared_target);

  CheckExactWeakIndexedEdge(generator.get(), weak_fixed_array, 0,
                            weak_fixed_first_target);
  CheckExactWeakIndexedEdge(generator.get(), weak_fixed_array, 1,
                            weak_fixed_second_target);
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_fixed_array,
                                         Edge::Type::kShortcut,
                                         weak_fixed_first_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_fixed_array,
                                         Edge::Type::kShortcut,
                                         weak_fixed_second_target));
  CheckNoWeakOrInternalIndexedEdge(generator.get(), weak_fixed_array, 2);
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_fixed_array, Edge::Type::kWeak,
                                         weak_fixed_cleared_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_fixed_array, Edge::Type::kHidden,
                                         weak_fixed_cleared_target));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(weak_fixed_array, Edge::Type::kInternal,
                                      weak_fixed_cleared_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_fixed_array,
                                         Edge::Type::kProperty,
                                         weak_fixed_cleared_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_fixed_array,
                                         Edge::Type::kShortcut,
                                         weak_fixed_cleared_target));
  const std::string weak_fixed_strong_index = EdgeIndexName(3);
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), weak_fixed_array,
                                   Edge::Type::kInternal,
                                   weak_fixed_strong_index.c_str(),
                                   weak_fixed_strong_target));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(weak_fixed_array,
                                         Edge::Type::kInternal,
                                         weak_fixed_strong_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_fixed_array, Edge::Type::kWeak,
                                         weak_fixed_strong_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_fixed_array, Edge::Type::kHidden,
                                         weak_fixed_strong_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_fixed_array,
                                         Edge::Type::kProperty,
                                         weak_fixed_strong_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_fixed_array,
                                         Edge::Type::kShortcut,
                                         weak_fixed_strong_target));
  MarkCoveredInstanceType(WEAK_FIXED_ARRAY_TYPE);

  CheckExactWeakIndexedEdge(generator.get(), weak_array_list, 0,
                            weak_array_list_first_target);
  CheckExactWeakIndexedEdge(generator.get(), weak_array_list, 1,
                            weak_array_list_second_target);
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_array_list,
                                         Edge::Type::kShortcut,
                                         weak_array_list_first_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_array_list,
                                         Edge::Type::kShortcut,
                                         weak_array_list_second_target));
  CheckNoWeakOrInternalIndexedEdge(generator.get(), weak_array_list, 2);
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_array_list, Edge::Type::kWeak,
                                         weak_array_list_cleared_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_array_list, Edge::Type::kHidden,
                                         weak_array_list_cleared_target));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(weak_array_list, Edge::Type::kInternal,
                                      weak_array_list_cleared_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_array_list,
                                         Edge::Type::kProperty,
                                         weak_array_list_cleared_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_array_list,
                                         Edge::Type::kShortcut,
                                         weak_array_list_cleared_target));
  const std::string weak_array_list_strong_index = EdgeIndexName(3);
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), weak_array_list,
                                   Edge::Type::kInternal,
                                   weak_array_list_strong_index.c_str(),
                                   weak_array_list_strong_target));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(weak_array_list,
                                         Edge::Type::kInternal,
                                         weak_array_list_strong_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_array_list, Edge::Type::kWeak,
                                         weak_array_list_strong_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_array_list, Edge::Type::kHidden,
                                         weak_array_list_strong_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_array_list,
                                         Edge::Type::kProperty,
                                         weak_array_list_strong_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_array_list,
                                         Edge::Type::kShortcut,
                                         weak_array_list_strong_target));
  MarkCoveredInstanceType(WEAK_ARRAY_LIST_TYPE);
}

UNINITIALIZED_TEST(HeapDumpEphemeronHashTableExactWeakKeyValueEdges) {
  Address table_address = 0;
  Address key_address = 0;
  Address value_address = 0;
  int key_index = -1;
  int value_index = -1;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              globalThis.__heap_dump_weak_key = {};
              globalThis.__heap_dump_weak_value = {};
              globalThis.__heap_dump_weak_map = new WeakMap([
                [globalThis.__heap_dump_weak_key,
                 globalThis.__heap_dump_weak_value]
              ]);
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            auto get_global = [&](const char* name) {
              return context->Global()
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Object> key =
                v8::Local<v8::Object>::Cast(get_global("__heap_dump_weak_key"));
            v8::Local<v8::Object> value = v8::Local<v8::Object>::Cast(
                get_global("__heap_dump_weak_value"));
            v8::Local<v8::Object> weak_map_object = v8::Local<v8::Object>::Cast(
                get_global("__heap_dump_weak_map"));
            key_address = LocalObjectAddress(key);
            value_address = LocalObjectAddress(value);

            DirectHandle<JSWeakMap> weak_map =
                Cast<JSWeakMap>(v8::Utils::OpenDirectHandle(*weak_map_object));
            Tagged<EphemeronHashTable> table =
                Cast<EphemeronHashTable>(weak_map->table());
            CHECK_EQ(EPHEMERON_HASH_TABLE_TYPE,
                     table->map()->instance_type());
            table_address = table->address();
            for (InternalIndex entry : table->IterateEntries()) {
              const int candidate_key_index =
                  EphemeronHashTable::EntryToIndex(entry) +
                  EphemeronHashTable::kEntryKeyIndex;
              const int candidate_value_index =
                  EphemeronHashTable::EntryToValueIndex(entry);
              Tagged<Object> candidate_key = table->get(candidate_key_index);
              Tagged<Object> candidate_value =
                  table->get(candidate_value_index);
              if (IsHeapObject(candidate_key) &&
                  Cast<HeapObject>(candidate_key)->address() == key_address &&
                  IsHeapObject(candidate_value) &&
                  Cast<HeapObject>(candidate_value)->address() ==
                      value_address) {
                key_index = candidate_key_index;
                value_index = candidate_value_index;
                break;
              }
            }
            CHECK_GE(key_index, 0);
            CHECK_GE(value_index, 0);

            return DumpArtifacts{};
          });

  Node* table = GetSnapshotNode(generator.get(), table_address);
  Node* key = GetSnapshotNode(generator.get(), key_address);
  Node* value = GetSnapshotNode(generator.get(), value_address);
  CHECK_NOT_NULL(table);
  CHECK_NOT_NULL(key);
  CHECK_NOT_NULL(value);

  CheckExactWeakEdgeNoInternalHiddenPropertyOrShortcutDuplicate(
      generator.get(), table, EdgeIndexName(key_index).c_str(), key);
  CheckExactWeakEdgeNoInternalHiddenPropertyOrShortcutDuplicate(
      generator.get(), table, EdgeIndexName(value_index).c_str(), value, true);

  CHECK_EQ(1, CountEphemeronAnnotationEdges(generator.get(), key, value));
  CHECK_EQ(1, CountEphemeronAnnotationEdges(generator.get(), table, value));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(table, Edge::Type::kInternal, value));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(key, Edge::Type::kWeak, value));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(key, Edge::Type::kHidden, value));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(key, Edge::Type::kProperty, value));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(key, Edge::Type::kShortcut, value));
  MarkCoveredInstanceType(EPHEMERON_HASH_TABLE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpExtractEphemeronHashTableReferencesWhiteBoxEdges) {
  Address table_address = 0;
  Address key_address = 0;
  Address value_address = 0;
  int key_index = -1;
  int value_index = -1;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              globalThis.__heap_dump_ephemeron_whitebox_key = {};
              globalThis.__heap_dump_ephemeron_whitebox_value = {};
              globalThis.__heap_dump_ephemeron_whitebox_map = new WeakMap([
                [globalThis.__heap_dump_ephemeron_whitebox_key,
                 globalThis.__heap_dump_ephemeron_whitebox_value]
              ]);
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            auto get_global = [&](const char* name) {
              return context->Global()
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Object> key = v8::Local<v8::Object>::Cast(
                get_global("__heap_dump_ephemeron_whitebox_key"));
            v8::Local<v8::Object> value = v8::Local<v8::Object>::Cast(
                get_global("__heap_dump_ephemeron_whitebox_value"));
            v8::Local<v8::Object> weak_map_object = v8::Local<v8::Object>::Cast(
                get_global("__heap_dump_ephemeron_whitebox_map"));
            key_address = LocalObjectAddress(key);
            value_address = LocalObjectAddress(value);

            DirectHandle<JSWeakMap> weak_map =
                Cast<JSWeakMap>(v8::Utils::OpenDirectHandle(*weak_map_object));
            Tagged<EphemeronHashTable> table =
                Cast<EphemeronHashTable>(weak_map->table());
            CHECK_EQ(EPHEMERON_HASH_TABLE_TYPE,
                     table->map()->instance_type());
            table_address = table->address();
            for (InternalIndex entry : table->IterateEntries()) {
              const int candidate_key_index =
                  EphemeronHashTable::EntryToIndex(entry) +
                  EphemeronHashTable::kEntryKeyIndex;
              const int candidate_value_index =
                  EphemeronHashTable::EntryToValueIndex(entry);
              Tagged<Object> candidate_key = table->get(candidate_key_index);
              Tagged<Object> candidate_value =
                  table->get(candidate_value_index);
              if (IsHeapObject(candidate_key) &&
                  Cast<HeapObject>(candidate_key)->address() == key_address &&
                  IsHeapObject(candidate_value) &&
                  Cast<HeapObject>(candidate_value)->address() ==
                      value_address) {
                key_index = candidate_key_index;
                value_index = candidate_value_index;
                break;
              }
            }
            CHECK_GE(key_index, 0);
            CHECK_GE(value_index, 0);

            return DumpArtifacts{};
          });

  Node* table = GetSnapshotNode(generator.get(), table_address);
  Node* key = GetSnapshotNode(generator.get(), key_address);
  Node* value = GetSnapshotNode(generator.get(), value_address);
  CHECK_NOT_NULL(table);
  CHECK_NOT_NULL(key);
  CHECK_NOT_NULL(value);

  // Native ExtractEphemeronHashTableReferences emits weak slot edges from the
  // table to both key and value.
  CheckExactWeakEdgeNoInternalHiddenPropertyOrShortcutDuplicate(
      generator.get(), table, EdgeIndexName(key_index).c_str(), key);
  CheckExactWeakEdgeNoInternalHiddenPropertyOrShortcutDuplicate(
      generator.get(), table, EdgeIndexName(value_index).c_str(), value, true);

  // Native ephemeron annotations are internal edges: key -> value and
  // table -> value. They must not be downgraded to hidden/property/weak edges.
  CHECK_EQ(1, CountEphemeronAnnotationEdges(generator.get(), key, value));
  CHECK_EQ(1, CountEphemeronAnnotationEdges(generator.get(), table, value));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(key, Edge::Type::kInternal, value));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(table, Edge::Type::kInternal, value));

  CHECK_EQ(0, CountEdgesFromNodeToTarget(table, Edge::Type::kInternal, key));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(table, Edge::Type::kHidden, key));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(table, Edge::Type::kProperty, key));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(table, Edge::Type::kShortcut, key));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(table, Edge::Type::kHidden, value));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(table, Edge::Type::kProperty, value));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(table, Edge::Type::kShortcut, value));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(key, Edge::Type::kWeak, value));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(key, Edge::Type::kHidden, value));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(key, Edge::Type::kProperty, value));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(key, Edge::Type::kShortcut, value));
  MarkCoveredInstanceType(EPHEMERON_HASH_TABLE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSMapExactTableEdge) {
  Address map_address = 0;
  Address js_map_map_address = 0;
  Address table_address = 0;
  Address key_address = 0;
  Address value_address = 0;
  int key_index = -1;
  int value_index = -1;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const key = { marker: 'js-map-key' };
                const value = { marker: 'js-map-value' };
                const map = new Map([[key, value]]);
                if (map.size !== 1 || map.get(key) !== value) {
                  throw new Error('unexpected JSMap fixture');
                }
                globalThis.__heap_dump_js_map_exact = map;
                globalThis.__heap_dump_js_map_key = key;
                globalThis.__heap_dump_js_map_value = value;
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            RunScript(context, R"(
              if (globalThis.__heap_dump_js_map_exact.size !== 1 ||
                  globalThis.__heap_dump_js_map_exact.get(
                      globalThis.__heap_dump_js_map_key) !==
                      globalThis.__heap_dump_js_map_value) {
                throw new Error('unexpected JSMap fixture after GC');
              }
            )");

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            auto get_global = [&](const char* name) {
              return context->Global()
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Object> map_object = v8::Local<v8::Object>::Cast(
                get_global("__heap_dump_js_map_exact"));
            v8::Local<v8::Object> key = v8::Local<v8::Object>::Cast(
                get_global("__heap_dump_js_map_key"));
            v8::Local<v8::Object> value = v8::Local<v8::Object>::Cast(
                get_global("__heap_dump_js_map_value"));

            DirectHandle<JSMap> js_map =
                Cast<JSMap>(v8::Utils::OpenDirectHandle(*map_object));
            CHECK_EQ(JS_MAP_TYPE, js_map->map()->instance_type());
            js_map_map_address = js_map->map()->address();
            DirectHandle<JSObject> js_key =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(*key));
            DirectHandle<JSObject> js_value =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(*value));
            Tagged<OrderedHashMap> table =
                Cast<OrderedHashMap>(js_map->table());
            CHECK_EQ(ORDERED_HASH_MAP_TYPE, table->map()->instance_type());
            CHECK_EQ(1, table->NumberOfElements());
            InternalIndex entry = table->FindEntry(isolate, *js_key);
            CHECK(entry.is_found());
            CHECK_EQ(*js_value, table->ValueAt(entry));
            CHECK_EQ(*js_key, table->KeyAt(entry));
            for (int index = 0; index < table->length(); ++index) {
              Tagged<Object> candidate = table->get(index);
              if (candidate == *js_key) {
                key_index = index;
              } else if (candidate == *js_value) {
                value_index = index;
              }
            }
            CHECK_GE(key_index, 0);
            CHECK_GE(value_index, 0);
            CHECK_EQ(*js_key, table->get(key_index));
            CHECK_EQ(*js_value, table->get(value_index));

            map_address = js_map->address();
            table_address = table->address();
            key_address = js_key->address();
            value_address = js_value->address();

            return DumpArtifacts{};
          });

  Node* map = GetSnapshotNode(generator.get(), map_address);
  Node* js_map_map = GetSnapshotNode(generator.get(), js_map_map_address);
  Node* table = GetSnapshotNode(generator.get(), table_address);
  Node* key = GetSnapshotNode(generator.get(), key_address);
  Node* value = GetSnapshotNode(generator.get(), value_address);
  CHECK_NOT_NULL(map);
  CHECK_NOT_NULL(js_map_map);
  CHECK_NOT_NULL(table);
  CHECK_NOT_NULL(key);
  CHECK_NOT_NULL(value);
  CHECK(NodeHasType(map, Node::Type::kObject));
  CHECK(NodeHasType(table, Node::Type::kArray));
  CHECK(NodeHasType(key, Node::Type::kObject));
  CHECK(NodeHasType(value, Node::Type::kObject));
  CHECK_GE(key_index, 0);
  CHECK_GE(value_index, 0);

  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), map, "table", table);
  CHECK(NodeHasType(js_map_map, Node::Type::kObjectShape));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), map, js_map_map);
  MarkCoveredInstanceType(JS_MAP_TYPE);

  const std::string key_edge_name = EdgeIndexName(key_index);
  const std::string value_edge_name = EdgeIndexName(value_index);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), table, key_edge_name.c_str(), key);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), table, value_edge_name.c_str(), value);
  MarkCoveredInstanceType(ORDERED_HASH_MAP_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSSetExactTableEdge) {
  Address set_address = 0;
  Address table_address = 0;
  Address value_address = 0;
  int value_index = -1;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const value = { marker: 'js-set-value' };
                const set = new Set([value]);
                if (set.size !== 1 || !set.has(value)) {
                  throw new Error('unexpected JSSet fixture');
                }
                globalThis.__heap_dump_js_set_exact = set;
                globalThis.__heap_dump_js_set_value = value;
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            RunScript(context, R"(
              if (globalThis.__heap_dump_js_set_exact.size !== 1 ||
                  !globalThis.__heap_dump_js_set_exact.has(
                      globalThis.__heap_dump_js_set_value)) {
                throw new Error('unexpected JSSet fixture after GC');
              }
            )");

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            auto get_global = [&](const char* name) {
              return context->Global()
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Object> set_object = v8::Local<v8::Object>::Cast(
                get_global("__heap_dump_js_set_exact"));
            v8::Local<v8::Object> value = v8::Local<v8::Object>::Cast(
                get_global("__heap_dump_js_set_value"));

            DirectHandle<JSSet> js_set =
                Cast<JSSet>(v8::Utils::OpenDirectHandle(*set_object));
            CHECK_EQ(JS_SET_TYPE, js_set->map()->instance_type());
            DirectHandle<JSObject> js_value =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(*value));
            Tagged<OrderedHashSet> table =
                Cast<OrderedHashSet>(js_set->table());
            CHECK_EQ(ORDERED_HASH_SET_TYPE, table->map()->instance_type());
            CHECK_EQ(1, table->NumberOfElements());
            InternalIndex entry = table->FindEntry(isolate, *js_value);
            CHECK(entry.is_found());
            CHECK_EQ(*js_value, table->KeyAt(entry));
            for (int index = 0; index < table->length(); ++index) {
              if (table->get(index) == *js_value) {
                value_index = index;
                break;
              }
            }
            CHECK_GE(value_index, 0);
            CHECK_EQ(*js_value, table->get(value_index));

            set_address = js_set->address();
            table_address = table->address();
            value_address = js_value->address();

            return DumpArtifacts{};
          });

  Node* set = GetSnapshotNode(generator.get(), set_address);
  Node* table = GetSnapshotNode(generator.get(), table_address);
  Node* value = GetSnapshotNode(generator.get(), value_address);
  CHECK_NOT_NULL(set);
  CHECK_NOT_NULL(table);
  CHECK_NOT_NULL(value);
  CHECK(NodeHasType(set, Node::Type::kObject));
  CHECK(NodeHasType(table, Node::Type::kArray));
  CHECK(NodeHasType(value, Node::Type::kObject));
  CHECK_GE(value_index, 0);

  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), set, "table", table);
  MarkCoveredInstanceType(JS_SET_TYPE);

  const std::string value_edge_name = EdgeIndexName(value_index);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), table, value_edge_name.c_str(), value);
  MarkCoveredInstanceType(ORDERED_HASH_SET_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSWeakCollectionExactTableEdges) {
  Address weak_map_address = 0;
  Address weak_map_map_address = 0;
  Address weak_map_table_address = 0;
  Address weak_set_address = 0;
  Address weak_set_map_address = 0;
  Address weak_set_table_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              globalThis.__heap_dump_weak_key = {};
              globalThis.__heap_dump_weak_value = {};
              globalThis.__heap_dump_weak_map = new WeakMap([
                [globalThis.__heap_dump_weak_key,
                 globalThis.__heap_dump_weak_value]
              ]);
              globalThis.__heap_dump_weak_set =
                  new WeakSet([globalThis.__heap_dump_weak_key]);
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            auto get_global = [&](const char* name) {
              return context->Global()
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Object> weak_map_object = v8::Local<v8::Object>::Cast(
                get_global("__heap_dump_weak_map"));
            v8::Local<v8::Object> weak_set_object = v8::Local<v8::Object>::Cast(
                get_global("__heap_dump_weak_set"));

            weak_map_address = LocalObjectAddress(weak_map_object);
            weak_set_address = LocalObjectAddress(weak_set_object);

            DirectHandle<JSWeakMap> weak_map =
                Cast<JSWeakMap>(v8::Utils::OpenDirectHandle(*weak_map_object));
            CHECK_EQ(JS_WEAK_MAP_TYPE, weak_map->map()->instance_type());
            weak_map_map_address = weak_map->map()->address();
            Tagged<EphemeronHashTable> weak_map_table =
                Cast<EphemeronHashTable>(weak_map->table());
            weak_map_table_address = weak_map_table->address();

            DirectHandle<JSWeakSet> weak_set =
                Cast<JSWeakSet>(v8::Utils::OpenDirectHandle(*weak_set_object));
            CHECK_EQ(JS_WEAK_SET_TYPE, weak_set->map()->instance_type());
            weak_set_map_address = weak_set->map()->address();
            Tagged<EphemeronHashTable> weak_set_table =
                Cast<EphemeronHashTable>(weak_set->table());
            weak_set_table_address = weak_set_table->address();

            return DumpArtifacts{};
          });

  Node* weak_map = GetSnapshotNode(generator.get(), weak_map_address);
  Node* weak_map_map = GetSnapshotNode(generator.get(), weak_map_map_address);
  Node* weak_map_table =
      GetSnapshotNode(generator.get(), weak_map_table_address);
  Node* weak_set = GetSnapshotNode(generator.get(), weak_set_address);
  Node* weak_set_map = GetSnapshotNode(generator.get(), weak_set_map_address);
  Node* weak_set_table =
      GetSnapshotNode(generator.get(), weak_set_table_address);
  CHECK_NOT_NULL(weak_map);
  CHECK_NOT_NULL(weak_map_map);
  CHECK_NOT_NULL(weak_map_table);
  CHECK_NOT_NULL(weak_set);
  CHECK_NOT_NULL(weak_set_map);
  CHECK_NOT_NULL(weak_set_table);
  CHECK(NodeHasType(weak_map_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(weak_set_map, Node::Type::kObjectShape));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), weak_map, weak_map_map);
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), weak_set, weak_set_map);

  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), weak_map, "table", weak_map_table);
  MarkCoveredInstanceType(JS_WEAK_MAP_TYPE);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), weak_set, "table", weak_set_table);
  MarkCoveredInstanceType(JS_WEAK_SET_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSWeakRefExactWeakTargetEdge) {
  Address weak_ref_address = 0;
  Address target_address = 0;
  Address js_weak_ref_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              globalThis.__heap_dump_js_weak_ref_target = {};
              globalThis.__heap_dump_js_weak_ref =
                  new WeakRef(globalThis.__heap_dump_js_weak_ref_target);
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            auto get_global = [&](const char* name) {
              return context->Global()
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Object> weak_ref =
                v8::Local<v8::Object>::Cast(
                    get_global("__heap_dump_js_weak_ref"));
            v8::Local<v8::Object> target = v8::Local<v8::Object>::Cast(
                get_global("__heap_dump_js_weak_ref_target"));

            DirectHandle<JSWeakRef> js_weak_ref =
                Cast<JSWeakRef>(v8::Utils::OpenDirectHandle(*weak_ref));
            CHECK_EQ(JS_WEAK_REF_TYPE, js_weak_ref->map()->instance_type());
            js_weak_ref_map_address = js_weak_ref->map()->address();
            weak_ref_address = LocalObjectAddress(weak_ref);
            target_address = LocalObjectAddress(target);

            return DumpArtifacts{};
          });

  Node* weak_ref = GetSnapshotNode(generator.get(), weak_ref_address);
  Node* target = GetSnapshotNode(generator.get(), target_address);
  Node* js_weak_ref_map =
      GetSnapshotNode(generator.get(), js_weak_ref_map_address);
  CHECK_NOT_NULL(weak_ref);
  CHECK_NOT_NULL(target);
  CHECK_NOT_NULL(js_weak_ref_map);
  CHECK(NodeHasType(weak_ref, Node::Type::kObject));
  CHECK(NodeHasType(target, Node::Type::kObject));
  CHECK(NodeHasType(js_weak_ref_map, Node::Type::kObjectShape));

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), weak_ref, js_weak_ref_map);
  CheckExactWeakEdgeNoInternalHiddenPropertyOrShortcutDuplicate(
      generator.get(), weak_ref, "target", target);
  MarkCoveredInstanceType(JS_WEAK_REF_TYPE);
}

UNINITIALIZED_TEST(HeapDumpWeakCellExactWeakTargetAndTokenEdges) {
  Address registry_address = 0;
  Address registry_map_address = 0;
  Address weak_cell_address = 0;
  Address weak_cell_map_address = 0;
  Address target_address = 0;
  Address unregister_token_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              globalThis.__heap_dump_weak_cell_target = {};
              globalThis.__heap_dump_weak_cell_unregister_token = {};
              globalThis.__heap_dump_finalization_registry =
                  new FinalizationRegistry(() => {});
              globalThis.__heap_dump_finalization_registry.register(
                  globalThis.__heap_dump_weak_cell_target,
                  17,
                  globalThis.__heap_dump_weak_cell_unregister_token);
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            auto get_global = [&](const char* name) {
              return context->Global()
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Object> target = v8::Local<v8::Object>::Cast(
                get_global("__heap_dump_weak_cell_target"));
            v8::Local<v8::Object> unregister_token =
                v8::Local<v8::Object>::Cast(
                    get_global("__heap_dump_weak_cell_unregister_token"));
            v8::Local<v8::Object> registry = v8::Local<v8::Object>::Cast(
                get_global("__heap_dump_finalization_registry"));

            target_address = LocalObjectAddress(target);
            unregister_token_address = LocalObjectAddress(unregister_token);

            DirectHandle<JSFinalizationRegistry> finalization_registry =
                Cast<JSFinalizationRegistry>(
                    v8::Utils::OpenDirectHandle(*registry));
            Tagged<Map> registry_map = finalization_registry->map();
            CHECK_EQ(JS_FINALIZATION_REGISTRY_TYPE, registry_map->instance_type());
            registry_address = LocalObjectAddress(registry);
            registry_map_address = registry_map->address();
            CHECK(IsWeakCell(finalization_registry->active_cells()));
            Tagged<WeakCell> weak_cell_object =
                Cast<WeakCell>(finalization_registry->active_cells());
            Tagged<Map> weak_cell_map = weak_cell_object->map();
            CHECK_EQ(WEAK_CELL_TYPE, weak_cell_map->instance_type());
            weak_cell_address = weak_cell_object->address();
            weak_cell_map_address = weak_cell_map->address();

            return DumpArtifacts{};
          });

  Node* registry = GetSnapshotNode(generator.get(), registry_address);
  Node* registry_map = GetSnapshotNode(generator.get(), registry_map_address);
  Node* weak_cell = GetSnapshotNode(generator.get(), weak_cell_address);
  Node* weak_cell_map = GetSnapshotNode(generator.get(), weak_cell_map_address);
  Node* target = GetSnapshotNode(generator.get(), target_address);
  Node* unregister_token =
      GetSnapshotNode(generator.get(), unregister_token_address);
  CHECK_NOT_NULL(registry);
  CHECK_NOT_NULL(registry_map);
  CHECK_NOT_NULL(weak_cell);
  CHECK_NOT_NULL(weak_cell_map);
  CHECK_NOT_NULL(target);
  CHECK_NOT_NULL(unregister_token);
  CHECK(NodeHasType(registry, Node::Type::kObject));
  CHECK(NodeHasType(registry_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(target, Node::Type::kObject));
  CHECK(NodeHasType(weak_cell_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(unregister_token, Node::Type::kObject));

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), registry,
                                       registry_map);
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), weak_cell,
                                       weak_cell_map);

  CHECK_EQ(1, CountEdgesFromNodeToTarget(registry, Edge::Type::kHidden,
                                         weak_cell));
  CheckExactWeakEdgeNoInternalHiddenPropertyOrShortcutDuplicate(
      generator.get(), weak_cell, "target", target);
  CheckExactWeakEdgeNoInternalHiddenPropertyOrShortcutDuplicate(
      generator.get(), weak_cell, "unregister_token", unregister_token);
  MarkCoveredInstanceType(JS_FINALIZATION_REGISTRY_TYPE);
  MarkCoveredInstanceType(WEAK_CELL_TYPE);
}

UNINITIALIZED_TEST(HeapDumpExtractWeakCellReferencesWhiteBoxEdges) {
  Address weak_cell_address = 0;
  Address target_address = 0;
  Address unregister_token_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              globalThis.__heap_dump_weak_cell_whitebox_target = {};
              globalThis.__heap_dump_weak_cell_whitebox_unregister_token = {};
              globalThis.__heap_dump_weak_cell_whitebox_registry =
                  new FinalizationRegistry(() => {});
              globalThis.__heap_dump_weak_cell_whitebox_registry.register(
                  globalThis.__heap_dump_weak_cell_whitebox_target,
                  17,
                  globalThis.__heap_dump_weak_cell_whitebox_unregister_token);
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            auto get_global = [&](const char* name) {
              return context->Global()
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Object> target = v8::Local<v8::Object>::Cast(
                get_global("__heap_dump_weak_cell_whitebox_target"));
            v8::Local<v8::Object> unregister_token =
                v8::Local<v8::Object>::Cast(
                    get_global(
                        "__heap_dump_weak_cell_whitebox_unregister_token"));
            v8::Local<v8::Object> registry = v8::Local<v8::Object>::Cast(
                get_global("__heap_dump_weak_cell_whitebox_registry"));

            target_address = LocalObjectAddress(target);
            unregister_token_address = LocalObjectAddress(unregister_token);

            DirectHandle<JSFinalizationRegistry> finalization_registry =
                Cast<JSFinalizationRegistry>(
                    v8::Utils::OpenDirectHandle(*registry));
            CHECK(IsWeakCell(finalization_registry->active_cells()));
            Tagged<WeakCell> weak_cell =
                Cast<WeakCell>(finalization_registry->active_cells());
            CHECK_EQ(WEAK_CELL_TYPE, weak_cell->map()->instance_type());
            weak_cell_address = weak_cell->address();

            return DumpArtifacts{};
          });

  Node* weak_cell = GetSnapshotNode(generator.get(), weak_cell_address);
  Node* target = GetSnapshotNode(generator.get(), target_address);
  Node* unregister_token =
      GetSnapshotNode(generator.get(), unregister_token_address);
  CHECK_NOT_NULL(weak_cell);
  CHECK_NOT_NULL(target);
  CHECK_NOT_NULL(unregister_token);

  // Native ExtractWeakCellReferences emits exactly these object-specific weak
  // edges. Generic map/list bookkeeping is intentionally outside this slice.
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), weak_cell,
                                   Edge::Type::kWeak, "target", target));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), weak_cell,
                                   Edge::Type::kWeak, "unregister_token",
                                   unregister_token));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(weak_cell, Edge::Type::kWeak,
                                         target));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(weak_cell, Edge::Type::kWeak,
                                         unregister_token));

  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_cell, Edge::Type::kInternal,
                                         target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_cell, Edge::Type::kInternal,
                                         unregister_token));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_cell, Edge::Type::kHidden,
                                         target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_cell, Edge::Type::kHidden,
                                         unregister_token));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_cell, Edge::Type::kProperty,
                                         target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_cell, Edge::Type::kProperty,
                                         unregister_token));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_cell, Edge::Type::kShortcut,
                                         target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(weak_cell, Edge::Type::kShortcut,
                                         unregister_token));
  MarkCoveredInstanceType(WEAK_CELL_TYPE);
}

UNINITIALIZED_TEST(HeapDumpFinalizationRegistryNextDirtyExactWeakEdge) {
  Address source_registry_address = 0;
  Address target_registry_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              globalThis.__heap_dump_finalization_registry_source =
                  new FinalizationRegistry(() => {});
              globalThis.__heap_dump_finalization_registry_target =
                  new FinalizationRegistry(() => {});
            )");

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            auto get_global = [&](const char* name) {
              return context->Global()
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Object> source_registry = v8::Local<v8::Object>::Cast(
                get_global("__heap_dump_finalization_registry_source"));
            v8::Local<v8::Object> target_registry = v8::Local<v8::Object>::Cast(
                get_global("__heap_dump_finalization_registry_target"));

            DirectHandle<JSFinalizationRegistry> source =
                Cast<JSFinalizationRegistry>(
                    v8::Utils::OpenDirectHandle(*source_registry));
            DirectHandle<JSFinalizationRegistry> target =
                Cast<JSFinalizationRegistry>(
                    v8::Utils::OpenDirectHandle(*target_registry));
            CHECK_EQ(JS_FINALIZATION_REGISTRY_TYPE,
                     source->map()->instance_type());
            CHECK_EQ(JS_FINALIZATION_REGISTRY_TYPE,
                     target->map()->instance_type());
            CHECK(IsUndefined(source->next_dirty(), isolate));
            source->set_next_dirty(*target);
            CHECK_EQ(*target, source->next_dirty());

            source_registry_address = LocalObjectAddress(source_registry);
            target_registry_address = LocalObjectAddress(target_registry);
            return DumpArtifacts{};
          });

  Node* source_registry =
      GetSnapshotNode(generator.get(), source_registry_address);
  Node* target_registry =
      GetSnapshotNode(generator.get(), target_registry_address);
  CHECK_NOT_NULL(source_registry);
  CHECK_NOT_NULL(target_registry);
  CHECK(NodeHasType(source_registry, Node::Type::kObject));
  CHECK(NodeHasType(target_registry, Node::Type::kObject));

  // Native heap-snapshot-generator.cc's IsEssentialHiddenReference returns
  // false for JSFinalizationRegistry's kNextDirtyOffset, which prevents
  // hidden edge creation. The next_dirty field uses IterateCustomWeakPointer
  // in BodyDescriptor but stores a strong-tagged value (not Weak<T> type),
  // so native traversal does not create a weak edge either.
  // Result: native creates no edge for next_dirty, and translator should
  // match this behavior.
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source_registry, Edge::Type::kWeak,
                                         target_registry));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source_registry, Edge::Type::kHidden,
                                         target_registry));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source_registry, Edge::Type::kInternal,
                                         target_registry));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source_registry, Edge::Type::kProperty,
                                         target_registry));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source_registry, Edge::Type::kShortcut,
                                         target_registry));
  MarkCoveredInstanceType(JS_FINALIZATION_REGISTRY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpCustomWeakPointerFamiliesUseWeakEdges) {
  Address weak_ref_address = 0;
  Address weak_ref_target_address = 0;
  Address weak_cell_address = 0;
  Address weak_cell_target_address = 0;
  Address unregister_token_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              globalThis.__heap_dump_custom_weak_ref_target = {};
              globalThis.__heap_dump_custom_weak_ref =
                  new WeakRef(globalThis.__heap_dump_custom_weak_ref_target);
              globalThis.__heap_dump_custom_weak_cell_target = {};
              globalThis.__heap_dump_custom_weak_unregister_token = {};
              globalThis.__heap_dump_custom_weak_registry =
                  new FinalizationRegistry(() => {});
              globalThis.__heap_dump_custom_weak_registry.register(
                  globalThis.__heap_dump_custom_weak_cell_target,
                  23,
                  globalThis.__heap_dump_custom_weak_unregister_token);
            )");

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            auto get_global = [&](const char* name) {
              return context->Global()
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Object> weak_ref =
                v8::Local<v8::Object>::Cast(
                    get_global("__heap_dump_custom_weak_ref"));
            v8::Local<v8::Object> weak_ref_target =
                v8::Local<v8::Object>::Cast(
                    get_global("__heap_dump_custom_weak_ref_target"));
            v8::Local<v8::Object> weak_cell_target =
                v8::Local<v8::Object>::Cast(
                    get_global("__heap_dump_custom_weak_cell_target"));
            v8::Local<v8::Object> unregister_token =
                v8::Local<v8::Object>::Cast(
                    get_global("__heap_dump_custom_weak_unregister_token"));
            v8::Local<v8::Object> source_registry =
                v8::Local<v8::Object>::Cast(
                    get_global("__heap_dump_custom_weak_registry"));

            DirectHandle<JSWeakRef> js_weak_ref =
                Cast<JSWeakRef>(v8::Utils::OpenDirectHandle(*weak_ref));
            CHECK_EQ(JS_WEAK_REF_TYPE, js_weak_ref->map()->instance_type());
            DirectHandle<JSFinalizationRegistry> source =
                Cast<JSFinalizationRegistry>(
                    v8::Utils::OpenDirectHandle(*source_registry));
            CHECK(IsWeakCell(source->active_cells()));
            Tagged<WeakCell> weak_cell_object =
                Cast<WeakCell>(source->active_cells());
            CHECK_EQ(WEAK_CELL_TYPE,
                     weak_cell_object->map()->instance_type());
            weak_ref_address = LocalObjectAddress(weak_ref);
            weak_ref_target_address = LocalObjectAddress(weak_ref_target);
            weak_cell_address = weak_cell_object->address();
            weak_cell_target_address = LocalObjectAddress(weak_cell_target);
            unregister_token_address = LocalObjectAddress(unregister_token);
            return DumpArtifacts{};
          });

  Node* weak_ref = GetSnapshotNode(generator.get(), weak_ref_address);
  Node* weak_ref_target =
      GetSnapshotNode(generator.get(), weak_ref_target_address);
  Node* weak_cell = GetSnapshotNode(generator.get(), weak_cell_address);
  Node* weak_cell_target =
      GetSnapshotNode(generator.get(), weak_cell_target_address);
  Node* unregister_token =
      GetSnapshotNode(generator.get(), unregister_token_address);
  CHECK_NOT_NULL(weak_ref);
  CHECK_NOT_NULL(weak_ref_target);
  CHECK_NOT_NULL(weak_cell);
  CHECK_NOT_NULL(weak_cell_target);
  CHECK_NOT_NULL(unregister_token);

  CheckExactWeakEdgeNoInternalHiddenPropertyOrShortcutDuplicate(
      generator.get(), weak_ref, "target", weak_ref_target);
  CheckExactWeakEdgeNoInternalHiddenPropertyOrShortcutDuplicate(
      generator.get(), weak_cell, "target", weak_cell_target);
  CheckExactWeakEdgeNoInternalHiddenPropertyOrShortcutDuplicate(
      generator.get(), weak_cell, "unregister_token", unregister_token);
}

#else
UNINITIALIZED_TEST(HeapDumpWeakArraysExactWeakAndClearedSlotEdges) {}
UNINITIALIZED_TEST(HeapDumpEphemeronHashTableExactWeakKeyValueEdges) {}
UNINITIALIZED_TEST(HeapDumpExtractEphemeronHashTableReferencesWhiteBoxEdges) {}
UNINITIALIZED_TEST(HeapDumpJSMapExactTableEdge) {}
UNINITIALIZED_TEST(HeapDumpJSSetExactTableEdge) {}
UNINITIALIZED_TEST(HeapDumpJSWeakCollectionExactTableEdges) {}
UNINITIALIZED_TEST(HeapDumpJSWeakRefExactWeakTargetEdge) {}
UNINITIALIZED_TEST(HeapDumpWeakCellExactWeakTargetAndTokenEdges) {}
UNINITIALIZED_TEST(HeapDumpExtractWeakCellReferencesWhiteBoxEdges) {}
UNINITIALIZED_TEST(HeapDumpFinalizationRegistryNextDirtyExactWeakEdge) {}
UNINITIALIZED_TEST(HeapDumpCustomWeakPointerFamiliesUseWeakEdges) {}
#endif

}  // namespace heap
}  // namespace internal
}  // namespace v8
