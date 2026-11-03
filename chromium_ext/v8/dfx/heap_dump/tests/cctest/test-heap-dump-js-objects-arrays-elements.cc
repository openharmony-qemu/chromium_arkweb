#include "include/v8-primitive-object.h"
#include "include/v8-primitive.h"
#include "include/v8-profiler.h"
#include "src/objects/api-callbacks-inl.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/bigint.h"
#include "src/objects/bytecode-array-inl.h"
#include "src/objects/contexts-inl.h"
#include "src/objects/descriptor-array-inl.h"
#include "src/objects/feedback-vector-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
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
#include "src/objects/property-array-inl.h"
#include "src/objects/property.h"
#include "src/objects/scope-info.h"
#include "src/objects/script-inl.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/string.h"
#include "src/objects/struct-inl.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/tests/cctest/heap-dump-test-utils.h"
#include "test/common/flag-utils.h"

namespace v8 {
namespace internal {
namespace heap {

#if defined(OH_ENABLE_HEAP_DUMP_TEST) && defined(OH_ENABLE_HEAP_DUMP) && \
    defined(OH_ENABLE_HEAP_TRANSLATE)

using namespace heap_dump_test;

UNINITIALIZED_TEST(HeapDumpJSArrayExactElementsEdge) {
  Address array_address = 0;
  Address elements_address = 0;
  Address array_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const first = { marker: 'direct-elements-first' };
                const second = { marker: 'direct-elements-second' };
                const array = [first, second];
                if (array.length !== 2 || array[0] !== first ||
                    array[1] !== second) {
                  throw new Error('unexpected direct elements fixture');
                }
                globalThis.__heap_dump_js_array_exact_elements = array;
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "__heap_dump_js_array_exact_elements"))
                    .ToLocalChecked());
            DirectHandle<JSArray> js_array =
                Cast<JSArray>(v8::Utils::OpenDirectHandle(*array));
            CHECK_EQ(JS_ARRAY_TYPE, js_array->map()->instance_type());
            array_map_address = js_array->map()->address();
            CHECK(js_array->HasFastElements());
            CHECK(js_array->HasObjectElements());
            Tagged<FixedArrayBase> elements = js_array->elements();
            CHECK(IsFixedArray(elements));
            CHECK_EQ(FIXED_ARRAY_TYPE, elements->map()->instance_type());

            array_address = js_array->address();
            elements_address = elements->address();

            return DumpArtifacts{};
          });

  Node* array = GetSnapshotNode(generator.get(), array_address);
  Node* elements = GetSnapshotNode(generator.get(), elements_address);
  Node* array_map = GetSnapshotNode(generator.get(), array_map_address);
  CHECK_NOT_NULL(array);
  CHECK_NOT_NULL(elements);
  CHECK_NOT_NULL(array_map);
  CHECK(NodeHasType(array, Node::Type::kObject));
  CHECK(NodeHasType(elements, Node::Type::kArray));
  CHECK(NodeHasType(array_map, Node::Type::kObjectShape));

  CHECK_EQ(1,
           CountExactNamedEdges(generator.get(), array, Edge::Type::kInternal,
                                "elements", elements));
  CHECK_EQ(1,
           CountEdgesFromNodeToTarget(array, Edge::Type::kInternal, elements));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kWeak, elements));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kHidden, elements));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(array, Edge::Type::kProperty, elements));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(array, Edge::Type::kShortcut, elements));
  CHECK(HasExactNamedEdge(generator.get(), array, Edge::Type::kInternal, "map",
                          array_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), array,
                                   Edge::Type::kInternal, "map", array_map));
  CHECK_EQ(1,
           CountEdgesFromNodeToTarget(array, Edge::Type::kInternal, array_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kWeak, array_map));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(array, Edge::Type::kHidden, array_map));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(array, Edge::Type::kProperty, array_map));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(array, Edge::Type::kShortcut, array_map));

  MarkCoveredInstanceType(JS_ARRAY_TYPE);
  MarkCoveredInstanceType(FIXED_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSObjectExactPropertiesEdge) {
  Address object_address = 0;
  Address properties_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        RunScript(context, R"(
              (function() {
                const object = {};
                for (let i = 0; i < 8; ++i) {
                  object["heapDumpFiller" + i] = i;
                }
                object.heapDumpExactPropertiesTarget = { marker: 1 };
                globalThis.__heap_dump_js_object_exact_properties = object;
              })();
            )");

        InvokeAtomicMajorGC(isolate->heap());

        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(
            context->Global()
                ->Get(context,
                      NewApiString(v8_isolate,
                                   "__heap_dump_js_object_exact_properties"))
                .ToLocalChecked());
        DirectHandle<JSObject> js_object =
            Cast<JSObject>(v8::Utils::OpenDirectHandle(*object));
        CHECK_EQ(JS_OBJECT_TYPE, js_object->map()->instance_type());
        CHECK(js_object->HasFastProperties());
        Tagged<PropertyArray> properties = js_object->property_array();
        CHECK(IsPropertyArray(properties));
        CHECK_EQ(PROPERTY_ARRAY_TYPE, properties->map()->instance_type());

        object_address = js_object->address();
        properties_address = properties->address();

        return DumpArtifacts{};
      });

  Node* object = GetSnapshotNode(generator.get(), object_address);
  Node* properties = GetSnapshotNode(generator.get(), properties_address);
  CHECK_NOT_NULL(object);
  CHECK_NOT_NULL(properties);
  CHECK(NodeHasType(object, Node::Type::kObject));

  // Native emits the common JSObject-tail direct internal edge to the
  // properties backing store. Named data-property and generic PropertyArray
  // indexed edges are separate relationships and cannot substitute for E14.
  CHECK_EQ(1,
           CountExactNamedEdges(generator.get(), object, Edge::Type::kInternal,
                                "properties", properties));
  CHECK_EQ(
      1, CountEdgesFromNodeToTarget(object, Edge::Type::kInternal, properties));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(object, Edge::Type::kWeak, properties));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(object, Edge::Type::kHidden, properties));
  CHECK_EQ(
      0, CountEdgesFromNodeToTarget(object, Edge::Type::kProperty, properties));
  CHECK_EQ(
      0, CountEdgesFromNodeToTarget(object, Edge::Type::kShortcut, properties));

  MarkCoveredInstanceType(JS_OBJECT_TYPE);
  MarkCoveredInstanceType(PROPERTY_ARRAY_TYPE);
}
namespace {

void CheckExactInternalMapEdgeNoWrongKind(SnapshotGenerator* generator,
                                          const Node* source, const Node* map) {
  CHECK_NOT_NULL(source);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CHECK(
      HasExactNamedEdge(generator, source, Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator, source, Edge::Type::kInternal,
                                   "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(source, Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kProperty, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kShortcut, map));
}

struct NativeElementEdgeExpectation {
  int first_index_count = 0;
  int second_index_count = 0;
  int missing_index_count = 0;
  int element_edge_count = 0;
};

int CountNativeIndexedEdges(v8::Local<v8::Context> context,
                            const v8::HeapGraphNode* source,
                            v8::HeapGraphEdge::Type type, uint32_t edge_index) {
  CHECK_NOT_NULL(source);
  int count = 0;
  for (int i = 0; i < source->GetChildrenCount(); ++i) {
    const v8::HeapGraphEdge* edge = source->GetChild(i);
    if (edge->GetType() != type) {
      continue;
    }
    uint32_t name_index = 0;
    if (!edge->GetName()->Uint32Value(context).To(&name_index)) {
      continue;
    }
    if (name_index == edge_index) {
      ++count;
    }
  }
  return count;
}

int CountNativeExactIndexedEdges(v8::Local<v8::Context> context,
                                 const v8::HeapGraphNode* source,
                                 v8::HeapGraphEdge::Type type,
                                 uint32_t edge_index,
                                 const v8::HeapGraphNode* target) {
  CHECK_NOT_NULL(source);
  CHECK_NOT_NULL(target);
  int count = 0;
  for (int i = 0; i < source->GetChildrenCount(); ++i) {
    const v8::HeapGraphEdge* edge = source->GetChild(i);
    if (edge->GetType() != type || edge->GetToNode() != target) {
      continue;
    }
    uint32_t name_index = 0;
    if (!edge->GetName()->Uint32Value(context).To(&name_index)) {
      continue;
    }
    if (name_index == edge_index) {
      ++count;
    }
  }
  return count;
}

int CountNativeEdgesFromNode(const v8::HeapGraphNode* source,
                             v8::HeapGraphEdge::Type type) {
  CHECK_NOT_NULL(source);
  int count = 0;
  for (int i = 0; i < source->GetChildrenCount(); ++i) {
    if (source->GetChild(i)->GetType() == type) {
      ++count;
    }
  }
  return count;
}

NativeElementEdgeExpectation GetNativeElementEdgeExpectation(
    v8::Isolate* isolate, v8::Local<v8::Context> context,
    v8::Local<v8::Value> array_value, v8::Local<v8::Value> first_value,
    uint32_t first_index, v8::Local<v8::Value> second_value,
    uint32_t second_index, uint32_t missing_index) {
  v8::HeapProfiler* heap_profiler = isolate->GetHeapProfiler();
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  v8::SnapshotObjectId array_id = heap_profiler->GetObjectId(array_value);
  v8::SnapshotObjectId first_id = heap_profiler->GetObjectId(first_value);
  v8::SnapshotObjectId second_id = heap_profiler->GetObjectId(second_value);
  CHECK_NE(v8::HeapProfiler::kUnknownObjectId, array_id);
  CHECK_NE(v8::HeapProfiler::kUnknownObjectId, first_id);
  CHECK_NE(v8::HeapProfiler::kUnknownObjectId, second_id);

  const v8::HeapGraphNode* array_node = snapshot->GetNodeById(array_id);
  const v8::HeapGraphNode* first_node = snapshot->GetNodeById(first_id);
  const v8::HeapGraphNode* second_node = snapshot->GetNodeById(second_id);
  CHECK_NOT_NULL(array_node);
  CHECK_NOT_NULL(first_node);
  CHECK_NOT_NULL(second_node);

  NativeElementEdgeExpectation expectation;
  expectation.first_index_count = CountNativeExactIndexedEdges(
      context, array_node, v8::HeapGraphEdge::kElement, first_index,
      first_node);
  expectation.second_index_count = CountNativeExactIndexedEdges(
      context, array_node, v8::HeapGraphEdge::kElement, second_index,
      second_node);
  expectation.missing_index_count = CountNativeIndexedEdges(
      context, array_node, v8::HeapGraphEdge::kElement, missing_index);
  expectation.element_edge_count =
      CountNativeEdgesFromNode(array_node, v8::HeapGraphEdge::kElement);
  CHECK_EQ(1, expectation.first_index_count);
  CHECK_EQ(1, expectation.second_index_count);
  CHECK_EQ(0, expectation.missing_index_count);
  CHECK_EQ(2, expectation.element_edge_count);
  // HeapSnapshot::Delete requires non-const pointer per V8 API design
  const_cast<v8::HeapSnapshot*>(snapshot)->Delete();
  return expectation;
}

void CheckTranslatedElementEdgesMatchNative(
    SnapshotGenerator* generator, const NativeElementEdgeExpectation& native,
    Address array_address, Address first_target_address, uint32_t first_index,
    Address second_target_address, uint32_t second_index,
    uint32_t missing_index) {
  Node* array = GetSnapshotNode(generator, array_address);
  Node* first_target = GetSnapshotNode(generator, first_target_address);
  Node* second_target = GetSnapshotNode(generator, second_target_address);
  CHECK_NOT_NULL(array);
  CHECK_NOT_NULL(first_target);
  CHECK_NOT_NULL(second_target);

  CHECK_EQ(native.first_index_count,
           CountExactIndexedEdges(array, Edge::Type::kElement, first_index,
                                  first_target));
  CHECK_EQ(native.second_index_count,
           CountExactIndexedEdges(array, Edge::Type::kElement, second_index,
                                  second_target));
  CHECK_EQ(native.missing_index_count,
           CountIndexedEdges(array, Edge::Type::kElement, missing_index));
  CHECK_EQ(native.element_edge_count,
           CountEdgesFromNode(array, Edge::Type::kElement));
  CHECK_EQ(
      0, CountEdgesFromNodeToTarget(array, Edge::Type::kHidden, first_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kInternal,
                                         first_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kProperty,
                                         first_target));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(array, Edge::Type::kWeak, first_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kShortcut,
                                         first_target));
}

}  // namespace

UNINITIALIZED_TEST(HeapDumpFixedArrayBackingStoreExactElementEdges) {
  Address elements_address = 0;
  Address first_target_address = 0;
  Address second_target_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(isolate);
        RunScript(context, R"(
              globalThis.__heap_dump_fixed_array_targets = [{}, {}];
            )");

        InvokeAtomicMajorGC(isolate->heap());

        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(
            context->Global()
                ->Get(context, NewApiString(v8_isolate,
                                            "__heap_dump_fixed_array_targets"))
                .ToLocalChecked());
        DirectHandle<JSArray> js_array =
            Cast<JSArray>(v8::Utils::OpenDirectHandle(*array));
        Tagged<FixedArrayBase> elements = js_array->elements();
        CHECK(IsFixedArray(elements));
        CHECK_EQ(FIXED_ARRAY_TYPE, elements->map()->instance_type());
        elements_address = elements->address();
        first_target_address =
            LocalObjectAddress(array->Get(context, 0).ToLocalChecked());
        second_target_address =
            LocalObjectAddress(array->Get(context, 1).ToLocalChecked());

        return DumpArtifacts{};
      });

  Node* elements = GetSnapshotNode(generator.get(), elements_address);
  Node* first_target = GetSnapshotNode(generator.get(), first_target_address);
  Node* second_target = GetSnapshotNode(generator.get(), second_target_address);
  CHECK_NOT_NULL(elements);
  CHECK_NOT_NULL(first_target);
  CHECK_NOT_NULL(second_target);

  CHECK(HasExactNamedEdge(generator.get(), elements, Edge::Type::kInternal, "0",
                          first_target));
  CHECK(HasExactNamedEdge(generator.get(), elements, Edge::Type::kInternal, "1",
                          second_target));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), elements,
                                   Edge::Type::kInternal, "0", first_target));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), elements,
                                   Edge::Type::kInternal, "1", second_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(elements, Edge::Type::kHidden,
                                         first_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(elements, Edge::Type::kHidden,
                                         second_target));
  // Both element targets are plain JS objects, not native/closure/array nodes.
  CHECK(NodeHasType(first_target, Node::Type::kObject));
  CHECK(NodeHasType(second_target, Node::Type::kObject));
  // No fabricated weak/property/shortcut edge to FixedArray element targets.
  CHECK_EQ(
      0, CountEdgesFromNodeToTarget(elements, Edge::Type::kWeak, first_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(elements, Edge::Type::kWeak,
                                         second_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(elements, Edge::Type::kProperty,
                                         first_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(elements, Edge::Type::kProperty,
                                         second_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(elements, Edge::Type::kShortcut,
                                         first_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(elements, Edge::Type::kShortcut,
                                         second_target));
  MarkCoveredInstanceType(FIXED_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpFixedArrayMixedSlotInternalEdges) {
  Address elements_address = 0;
  Address first_target_address = 0;
  Address second_target_address = 0;
  Address hole_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DumpArtifacts artifacts;
            DirectHandle<JSObject> first_target =
                isolate->factory()->NewJSObject(isolate->object_function());
            DirectHandle<JSObject> second_target =
                isolate->factory()->NewJSObject(isolate->object_function());
            DirectHandle<FixedArray> elements =
                isolate->factory()->NewFixedArray(4, AllocationType::kOld);
            ReadOnlyRoots roots(isolate);
            elements->set(0, *first_target);
            elements->set(1, Smi::FromInt(42));
            elements->set(2, roots.the_hole_value());
            elements->set(3, *second_target);

            elements_address = elements->address();
            first_target_address = first_target->address();
            second_target_address = second_target->address();
            hole_address = roots.the_hole_value()->address();

            artifacts.keep_alive.push_back(first_target);
            artifacts.keep_alive.push_back(second_target);
            artifacts.keep_alive.push_back(elements);
            return artifacts;
          });

  Node* elements = GetSnapshotNode(generator.get(), elements_address);
  Node* first_target = GetSnapshotNode(generator.get(), first_target_address);
  Node* second_target = GetSnapshotNode(generator.get(), second_target_address);
  CHECK_NOT_NULL(elements);
  CHECK_NOT_NULL(first_target);
  CHECK_NOT_NULL(second_target);

  CHECK(HasExactNamedEdge(generator.get(), elements, Edge::Type::kInternal, "0",
                          first_target));
  CHECK(HasExactNamedEdge(generator.get(), elements, Edge::Type::kInternal, "3",
                          second_target));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), elements,
                                   Edge::Type::kInternal, "0", first_target));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), elements,
                                   Edge::Type::kInternal, "3", second_target));
  CHECK_EQ(0, CountEdgesByNameFromNode(
                  generator.get(), elements, Edge::Type::kInternal, "1",
                  [](const Node*) { return true; }));

  Node* hole = GetSnapshotNode(generator.get(), hole_address);
  if (hole != nullptr) {
    CHECK_EQ(1, CountExactNamedEdges(generator.get(), elements,
                                     Edge::Type::kInternal, "2", hole));
    CHECK_EQ(
        0, CountEdgesFromNodeToTarget(elements, Edge::Type::kHidden, hole));
  }
  CHECK_EQ(0, CountEdgesFromNodeToTarget(elements, Edge::Type::kHidden,
                                         first_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(elements, Edge::Type::kHidden,
                                         second_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(elements, Edge::Type::kWeak,
                                         first_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(elements, Edge::Type::kWeak,
                                         second_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(elements, Edge::Type::kProperty,
                                         first_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(elements, Edge::Type::kProperty,
                                         second_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(elements, Edge::Type::kShortcut,
                                         first_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(elements, Edge::Type::kShortcut,
                                         second_target));
  MarkCoveredInstanceType(FIXED_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpExtractFixedArrayReferencesWhiteBoxEdges) {
  Address array_address = 0;
  Address object_target_address = 0;
  Address hole_target_address = 0;
  Address string_target_address = 0;
  Address nested_array_target_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DumpArtifacts artifacts;
            DirectHandle<JSObject> object_target =
                isolate->factory()->NewJSObject(isolate->object_function());
            DirectHandle<String> string_target =
                isolate->factory()->InternalizeUtf8String(
                    "heap_dump_fixed_array_whitebox_string");
            DirectHandle<FixedArray> nested_array_target =
                isolate->factory()->NewFixedArray(1, AllocationType::kOld);
            nested_array_target->set(0, Smi::FromInt(7));

            DirectHandle<FixedArray> array =
                isolate->factory()->NewFixedArray(5, AllocationType::kOld);
            ReadOnlyRoots roots(isolate);
            array->set(0, *object_target);
            array->set(1, Smi::FromInt(42));
            array->set(2, roots.the_hole_value());
            array->set(3, *string_target);
            array->set(4, *nested_array_target);

            array_address = array->address();
            object_target_address = object_target->address();
            hole_target_address = roots.the_hole_value()->address();
            string_target_address = string_target->address();
            nested_array_target_address = nested_array_target->address();

            artifacts.keep_alive.push_back(object_target);
            artifacts.keep_alive.push_back(string_target);
            artifacts.keep_alive.push_back(nested_array_target);
            artifacts.keep_alive.push_back(array);
            return artifacts;
          });

  Node* array = GetSnapshotNode(generator.get(), array_address);
  Node* object_target = GetSnapshotNode(generator.get(), object_target_address);
  Node* hole_target = GetSnapshotNode(generator.get(), hole_target_address);
  Node* string_target = GetSnapshotNode(generator.get(), string_target_address);
  Node* nested_array_target =
      GetSnapshotNode(generator.get(), nested_array_target_address);
  CHECK_NOT_NULL(array);
  CHECK_NOT_NULL(object_target);
  CHECK_NOT_NULL(hole_target);
  CHECK_NOT_NULL(string_target);
  CHECK_NOT_NULL(nested_array_target);

  CHECK_EQ(1, CountExactNamedEdges(generator.get(), array,
                                   Edge::Type::kInternal, "0", object_target));
  CHECK_EQ(0, CountEdgesByNameFromNode(
                  generator.get(), array, Edge::Type::kInternal, "1",
                  [](const Node*) { return true; }));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), array,
                                   Edge::Type::kInternal, "2", hole_target));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), array,
                                   Edge::Type::kInternal, "3", string_target));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), array,
                                   Edge::Type::kInternal, "4",
                                   nested_array_target));

  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kHidden,
                                         object_target));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(array, Edge::Type::kHidden, hole_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kHidden,
                                         string_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kHidden,
                                         nested_array_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kWeak,
                                         object_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kWeak,
                                         string_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kWeak,
                                         nested_array_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kProperty,
                                         object_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kProperty,
                                         string_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kProperty,
                                         nested_array_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kShortcut,
                                         object_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kShortcut,
                                         string_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kShortcut,
                                         nested_array_target));
  MarkCoveredInstanceType(FIXED_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpFixedDoubleArrayExactElementEdges) {
  Address elements_address = 0;
  Address elements_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              const doubleArray = [1.5, -2.25];
              if (doubleArray.length !== 2 ||
                  doubleArray[0] !== 1.5 ||
                  doubleArray[1] !== -2.25) {
                throw new Error('unexpected double array fixture');
              }
              globalThis.__heap_dump_fixed_double_array_exact = doubleArray;
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "__heap_dump_fixed_double_array_exact"))
                    .ToLocalChecked());
            DirectHandle<JSArray> js_array =
                Cast<JSArray>(v8::Utils::OpenDirectHandle(*array));
            CHECK(js_array->HasFastElements());
            CHECK(js_array->HasDoubleElements());
            Tagged<FixedArrayBase> elements = js_array->elements();
            CHECK(IsFixedDoubleArray(elements));
            CHECK_EQ(FIXED_DOUBLE_ARRAY_TYPE, elements->map()->instance_type());
            elements_address = elements->address();
            elements_map_address = elements->map()->address();

            return DumpArtifacts{};
          });

  Node* elements = GetSnapshotNode(generator.get(), elements_address);
  Node* elements_map = GetSnapshotNode(generator.get(), elements_map_address);
  CHECK_NOT_NULL(elements);
  CHECK_NOT_NULL(elements_map);
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), elements, elements_map);
  CHECK_GT(CountEdgesFromNode(elements, Edge::Type::kInternal) +
               CountEdgesFromNode(elements, Edge::Type::kElement) +
               CountEdgesFromNode(elements, Edge::Type::kProperty),
           0);
  CHECK_EQ(0, CountEdgesFromNode(elements, Edge::Type::kWeak));
  CHECK_EQ(0, CountEdgesFromNode(elements, Edge::Type::kHidden));

  MarkCoveredInstanceType(FIXED_DOUBLE_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpSloppyArgumentsElementsExactInternalEdges) {
  Address elements_address = 0;
  Address elements_map_address = 0;
  Address context_address = 0;
  Address arguments_store_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        RunScript(context, R"(
              (function() {
                function HeapDumpSloppyArgumentsFixture(first, second, third) {
                  return arguments;
                }
                const first = { marker: 'arguments-first' };
                const second = { marker: 'arguments-second' };
                const third = { marker: 'arguments-third' };
                const argumentsObject =
                    HeapDumpSloppyArgumentsFixture(first, second, third);
                if (argumentsObject.length !== 3 ||
                    argumentsObject[0] !== first ||
                    argumentsObject[1] !== second ||
                    argumentsObject[2] !== third) {
                  throw new Error('unexpected sloppy arguments fixture');
                }
                globalThis.__heap_dump_sloppy_arguments_elements = {
                  argumentsObject
                };
              })();
            )");

        InvokeAtomicMajorGC(isolate->heap());

        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
            context->Global()
                ->Get(context,
                      NewApiString(v8_isolate,
                                   "__heap_dump_sloppy_arguments_elements"))
                .ToLocalChecked());
        v8::Local<v8::Value> arguments_value =
            fixture->Get(context, NewApiString(v8_isolate, "argumentsObject"))
                .ToLocalChecked();
        CHECK(arguments_value->IsArgumentsObject());
        v8::Local<v8::Object> arguments_object =
            v8::Local<v8::Object>::Cast(arguments_value);
        DirectHandle<JSArgumentsObject> js_arguments = Cast<JSArgumentsObject>(
            v8::Utils::OpenDirectHandle(*arguments_object));
        Tagged<FixedArrayBase> elements = js_arguments->elements();
        CHECK_EQ(SLOPPY_ARGUMENTS_ELEMENTS_TYPE,
                 elements->map()->instance_type());
        Tagged<SloppyArgumentsElements> sloppy_elements =
            Cast<SloppyArgumentsElements>(elements);
        elements_address = sloppy_elements->address();
        elements_map_address = sloppy_elements->map()->address();
        context_address = sloppy_elements->context()->address();
        arguments_store_address =
            Cast<HeapObject>(sloppy_elements->arguments())->address();

        return DumpArtifacts{};
      });

  Node* elements = GetSnapshotNode(generator.get(), elements_address);
  Node* context = GetSnapshotNode(generator.get(), context_address);
  Node* arguments_store =
      GetSnapshotNode(generator.get(), arguments_store_address);
  Node* elements_map = GetSnapshotNode(generator.get(), elements_map_address);
  CHECK_NOT_NULL(elements);
  CHECK_NOT_NULL(context);
  CHECK_NOT_NULL(arguments_store);
  CHECK_NOT_NULL(elements_map);
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), elements, elements_map);

  CHECK(NodeHasType(context, Node::Type::kObject));
  CHECK_GT(CountEdgesFromNode(elements, Edge::Type::kInternal) +
               CountEdgesFromNode(elements, Edge::Type::kElement) +
               CountEdgesFromNode(elements, Edge::Type::kProperty),
           0);
  CHECK_EQ(0, CountEdgesFromNode(elements, Edge::Type::kWeak));
  MarkCoveredInstanceType(SLOPPY_ARGUMENTS_ELEMENTS_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSArrayFastObjectElementsExactElementEdges) {
  Address array_address = 0;
  Address first_target_address = 0;
  Address fourth_target_address = 0;
  Address hole_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        RunScript(context, R"(
              (function() {
                const first = { marker: 'first' };
                const fourth = { marker: 'fourth' };
                const array = [];
                array[0] = first;
                array[1] = 42;
                array[3] = fourth;
                if (array.length !== 4 || array[1] !== 42 || 2 in array) {
                  throw new Error('unexpected sparse array fixture');
                }
                globalThis.__heap_dump_js_array_fast_object_elements = {
                  array, first, fourth
                };
              })();
            )");

        InvokeAtomicMajorGC(isolate->heap());

        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
            context->Global()
                ->Get(context,
                      NewApiString(v8_isolate,
                                   "__heap_dump_js_array_fast_object_elements"))
                .ToLocalChecked());
        auto get_property = [&](const char* name) {
          return fixture->Get(context, NewApiString(v8_isolate, name))
              .ToLocalChecked();
        };
        v8::Local<v8::Array> array =
            v8::Local<v8::Array>::Cast(get_property("array"));
        CHECK_EQ(4u, array->Length());

        v8::Local<v8::Value> first_element =
            array->Get(context, 0).ToLocalChecked();
        v8::Local<v8::Value> smi_element =
            array->Get(context, 1).ToLocalChecked();
        v8::Local<v8::Value> fourth_element =
            array->Get(context, 3).ToLocalChecked();
        CHECK(first_element->StrictEquals(get_property("first")));
        CHECK(smi_element->IsInt32());
        CHECK_EQ(42, smi_element->Int32Value(context).FromJust());
        CHECK(!array->Has(context, 2).FromJust());
        CHECK(fourth_element->StrictEquals(get_property("fourth")));

        DirectHandle<JSArray> js_array =
            Cast<JSArray>(v8::Utils::OpenDirectHandle(*array));
        CHECK_EQ(JS_ARRAY_TYPE, js_array->map()->instance_type());
        CHECK(js_array->HasFastElements());
        CHECK(js_array->HasObjectElements());
        CHECK(IsFixedArray(js_array->elements()));

        array_address = js_array->address();
        first_target_address = LocalObjectAddress(first_element);
        fourth_target_address = LocalObjectAddress(fourth_element);
        hole_address = ReadOnlyRoots(isolate).the_hole_value()->address();

        return DumpArtifacts{};
      });

  Node* array = GetSnapshotNode(generator.get(), array_address);
  Node* first_target = GetSnapshotNode(generator.get(), first_target_address);
  Node* fourth_target = GetSnapshotNode(generator.get(), fourth_target_address);
  CHECK_NOT_NULL(array);
  CHECK_NOT_NULL(first_target);
  CHECK_NOT_NULL(fourth_target);
  CHECK(NodeHasType(array, Node::Type::kObject));
  CHECK(NodeHasType(first_target, Node::Type::kObject));
  CHECK(NodeHasType(fourth_target, Node::Type::kObject));

  CHECK_EQ(
      1, CountExactIndexedEdges(array, Edge::Type::kElement, 0, first_target));
  CHECK_EQ(0, CountIndexedEdges(array, Edge::Type::kElement, 1));
  CHECK_EQ(0, CountIndexedEdges(array, Edge::Type::kElement, 2));
  CHECK_EQ(
      1, CountExactIndexedEdges(array, Edge::Type::kElement, 3, fourth_target));
  CHECK_EQ(2, CountEdgesFromNode(array, Edge::Type::kElement));
  CHECK_EQ(
      1, CountEdgesFromNodeToTarget(array, Edge::Type::kElement, first_target));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(array, Edge::Type::kElement,
                                         fourth_target));
  CHECK_EQ(
      0, CountEdgesFromNodeToTarget(array, Edge::Type::kHidden, first_target));
  CHECK_EQ(
      0, CountEdgesFromNodeToTarget(array, Edge::Type::kHidden, fourth_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kInternal,
                                         first_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kInternal,
                                         fourth_target));
  Node* hole = GetSnapshotNode(generator.get(), hole_address);
  if (hole != nullptr) {
    CHECK_EQ(0, CountExactIndexedEdges(array, Edge::Type::kElement, 2, hole));
    CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kElement, hole));
  }
  MarkCoveredInstanceType(JS_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSArrayFastObjectElementsUsesArrayLength) {
  Address array_address = 0;
  Address first_target_address = 0;
  Address hidden_target_address = 0;
  Address elements_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const first = { marker: 'array-length-first' };
                const hidden = { marker: 'array-length-hidden' };
                const array = [first];
                if (array.length !== 1 || array[0] !== first) {
                  throw new Error('unexpected array length fixture');
                }
                globalThis.__heap_dump_js_array_length_elements = {
                  array,
                  first,
                  hidden
                };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "__heap_dump_js_array_length_elements"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Array> array =
                v8::Local<v8::Array>::Cast(get_property("array"));
            v8::Local<v8::Value> first = get_property("first");
            v8::Local<v8::Value> hidden = get_property("hidden");

            DirectHandle<JSArray> js_array =
                Cast<JSArray>(v8::Utils::OpenDirectHandle(*array));
            CHECK(js_array->HasFastElements());
            CHECK(js_array->HasObjectElements());
            CHECK_EQ(Smi::FromInt(1), js_array->length());

            DirectHandle<FixedArray> elements =
                isolate->factory()->NewFixedArray(4);
            ReadOnlyRoots roots(isolate);
            elements->set(0, *v8::Utils::OpenDirectHandle(*first));
            elements->set(1, roots.the_hole_value());
            elements->set(2, roots.the_hole_value());
            elements->set(3, *v8::Utils::OpenDirectHandle(*hidden));
            js_array->set_elements(*elements);
            CHECK_EQ(Smi::FromInt(1), js_array->length());
            CHECK_EQ(4, Cast<FixedArray>(js_array->elements())->length());

            array_address = js_array->address();
            first_target_address = LocalObjectAddress(first);
            hidden_target_address = LocalObjectAddress(hidden);
            elements_address = elements->address();

            return DumpArtifacts{};
          });

  Node* array = GetSnapshotNode(generator.get(), array_address);
  Node* first_target = GetSnapshotNode(generator.get(), first_target_address);
  Node* hidden_target = GetSnapshotNode(generator.get(), hidden_target_address);
  Node* elements = GetSnapshotNode(generator.get(), elements_address);
  CHECK_NOT_NULL(array);
  CHECK_NOT_NULL(first_target);
  CHECK_NOT_NULL(hidden_target);
  CHECK_NOT_NULL(elements);

  CHECK_EQ(
      1, CountExactIndexedEdges(array, Edge::Type::kElement, 0, first_target));
  CHECK_EQ(0, CountIndexedEdges(array, Edge::Type::kElement, 3));
  CHECK_EQ(1, CountEdgesFromNode(array, Edge::Type::kElement));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kElement,
                                         hidden_target));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), elements,
                                   Edge::Type::kInternal, "3", hidden_target));

  MarkCoveredInstanceType(JS_ARRAY_TYPE);
  MarkCoveredInstanceType(FIXED_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSArrayDictionaryElementsExactElementEdges) {
  Address array_address = 0;
  Address first_target_address = 0;
  Address sparse_target_address = 0;
  constexpr uint32_t kSparseIndex = 100000;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        RunScript(context, R"(
              (function() {
                const first = { marker: 'dictionary-first' };
                const sparse = { marker: 'dictionary-sparse' };
                const array = [];
                array[0] = first;
                array[100000] = sparse;
                if (array.length !== 100001 || array[0] !== first ||
                    array[100000] !== sparse || 50000 in array) {
                  throw new Error('unexpected dictionary array fixture');
                }
                globalThis.__heap_dump_js_array_dictionary_elements = {
                  array, first, sparse
                };
              })();
            )");

        InvokeAtomicMajorGC(isolate->heap());

        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
            context->Global()
                ->Get(context,
                      NewApiString(v8_isolate,
                                   "__heap_dump_js_array_dictionary_elements"))
                .ToLocalChecked());
        auto get_property = [&](const char* name) {
          return fixture->Get(context, NewApiString(v8_isolate, name))
              .ToLocalChecked();
        };
        v8::Local<v8::Array> array =
            v8::Local<v8::Array>::Cast(get_property("array"));
        CHECK_EQ(kSparseIndex + 1, array->Length());
        CHECK(array->Has(context, kSparseIndex).FromJust());
        CHECK(!array->Has(context, 50000).FromJust());

        DirectHandle<JSArray> js_array =
            Cast<JSArray>(v8::Utils::OpenDirectHandle(*array));
        CHECK_EQ(JS_ARRAY_TYPE, js_array->map()->instance_type());
        CHECK(js_array->HasDictionaryElements());
        CHECK(IsNumberDictionary(js_array->elements()));

        array_address = js_array->address();
        first_target_address = LocalObjectAddress(get_property("first"));
        sparse_target_address = LocalObjectAddress(get_property("sparse"));

        return DumpArtifacts{};
      });

  Node* array = GetSnapshotNode(generator.get(), array_address);
  Node* first_target = GetSnapshotNode(generator.get(), first_target_address);
  Node* sparse_target = GetSnapshotNode(generator.get(), sparse_target_address);
  CHECK_NOT_NULL(array);
  CHECK_NOT_NULL(first_target);
  CHECK_NOT_NULL(sparse_target);

  CHECK_EQ(
      1, CountExactIndexedEdges(array, Edge::Type::kElement, 0, first_target));
  CHECK_EQ(1, CountExactIndexedEdges(array, Edge::Type::kElement, kSparseIndex,
                                     sparse_target));
  CHECK_EQ(0, CountIndexedEdges(array, Edge::Type::kElement, 50000));
  CHECK_EQ(2, CountEdgesFromNode(array, Edge::Type::kElement));
  CHECK_EQ(
      1, CountEdgesFromNodeToTarget(array, Edge::Type::kElement, first_target));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(array, Edge::Type::kElement,
                                         sparse_target));
  CHECK_EQ(
      0, CountEdgesFromNodeToTarget(array, Edge::Type::kHidden, sparse_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array, Edge::Type::kInternal,
                                         sparse_target));
  MarkCoveredInstanceType(JS_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpExtractElementReferencesEdgesMatchNativeSnapshot) {
  Address fast_array_address = 0;
  Address fast_first_address = 0;
  Address fast_second_address = 0;
  Address dictionary_array_address = 0;
  Address dictionary_first_address = 0;
  Address dictionary_second_address = 0;
  constexpr uint32_t kFastFirstIndex = 0;
  constexpr uint32_t kFastSecondIndex = 3;
  constexpr uint32_t kFastMissingIndex = 2;
  constexpr uint32_t kDictionaryFirstIndex = 0;
  constexpr uint32_t kDictionarySecondIndex = 100000;
  constexpr uint32_t kDictionaryMissingIndex = 50000;
  NativeElementEdgeExpectation fast_native;
  NativeElementEdgeExpectation dictionary_native;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        RunScript(context, R"(
              (function() {
                const fastFirst = { marker: 'native-fast-first' };
                const fastSecond = { marker: 'native-fast-second' };
                const fastArray = [];
                fastArray[0] = fastFirst;
                fastArray[1] = 7;
                fastArray[3] = fastSecond;

                const dictionaryFirst = {
                  marker: 'native-dictionary-first'
                };
                const dictionarySecond = {
                  marker: 'native-dictionary-second'
                };
                const dictionaryArray = [];
                dictionaryArray[0] = dictionaryFirst;
                dictionaryArray[100000] = dictionarySecond;

                if (2 in fastArray || 50000 in dictionaryArray) {
                  throw new Error('unexpected element fixture');
                }
                globalThis.__heap_dump_extract_elements_native_align = {
                  fastArray,
                  fastFirst,
                  fastSecond,
                  dictionaryArray,
                  dictionaryFirst,
                  dictionarySecond
                };
              })();
            )");

        InvokeAtomicMajorGC(isolate->heap());

        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
            context->Global()
                ->Get(context,
                      NewApiString(v8_isolate,
                                   "__heap_dump_extract_elements_native_align"))
                .ToLocalChecked());
        auto get_property = [&](const char* name) {
          return fixture->Get(context, NewApiString(v8_isolate, name))
              .ToLocalChecked();
        };

        v8::Local<v8::Value> fast_array_value = get_property("fastArray");
        v8::Local<v8::Value> fast_first = get_property("fastFirst");
        v8::Local<v8::Value> fast_second = get_property("fastSecond");
        v8::Local<v8::Value> dictionary_array_value =
            get_property("dictionaryArray");
        v8::Local<v8::Value> dictionary_first = get_property("dictionaryFirst");
        v8::Local<v8::Value> dictionary_second =
            get_property("dictionarySecond");

        fast_native = GetNativeElementEdgeExpectation(
            v8_isolate, context, fast_array_value, fast_first, kFastFirstIndex,
            fast_second, kFastSecondIndex, kFastMissingIndex);
        dictionary_native = GetNativeElementEdgeExpectation(
            v8_isolate, context, dictionary_array_value, dictionary_first,
            kDictionaryFirstIndex, dictionary_second, kDictionarySecondIndex,
            kDictionaryMissingIndex);

        DirectHandle<JSArray> fast_array =
            Cast<JSArray>(v8::Utils::OpenDirectHandle(
                *v8::Local<v8::Array>::Cast(fast_array_value)));
        DirectHandle<JSArray> dictionary_array =
            Cast<JSArray>(v8::Utils::OpenDirectHandle(
                *v8::Local<v8::Array>::Cast(dictionary_array_value)));
        CHECK(fast_array->HasObjectElements());
        CHECK(dictionary_array->HasDictionaryElements());

        fast_array_address = fast_array->address();
        fast_first_address = LocalObjectAddress(fast_first);
        fast_second_address = LocalObjectAddress(fast_second);
        dictionary_array_address = dictionary_array->address();
        dictionary_first_address = LocalObjectAddress(dictionary_first);
        dictionary_second_address = LocalObjectAddress(dictionary_second);

        return DumpArtifacts{};
      });

  CheckTranslatedElementEdgesMatchNative(generator.get(), fast_native,
                                         fast_array_address, fast_first_address,
                                         kFastFirstIndex, fast_second_address,
                                         kFastSecondIndex, kFastMissingIndex);
  CheckTranslatedElementEdgesMatchNative(
      generator.get(), dictionary_native, dictionary_array_address,
      dictionary_first_address, kDictionaryFirstIndex,
      dictionary_second_address, kDictionarySecondIndex,
      kDictionaryMissingIndex);

  MarkCoveredInstanceType(JS_ARRAY_TYPE);
}

#else
UNINITIALIZED_TEST(HeapDumpJSArrayExactElementsEdge) {}
UNINITIALIZED_TEST(HeapDumpJSObjectExactPropertiesEdge) {}
UNINITIALIZED_TEST(HeapDumpFixedArrayBackingStoreExactElementEdges) {}
UNINITIALIZED_TEST(HeapDumpFixedArrayMixedSlotInternalEdges) {}
UNINITIALIZED_TEST(HeapDumpExtractFixedArrayReferencesWhiteBoxEdges) {}
UNINITIALIZED_TEST(HeapDumpFixedDoubleArrayExactElementEdges) {}
UNINITIALIZED_TEST(HeapDumpSloppyArgumentsElementsExactInternalEdges) {}
UNINITIALIZED_TEST(HeapDumpJSArrayFastObjectElementsExactElementEdges) {}
UNINITIALIZED_TEST(HeapDumpJSArrayFastObjectElementsUsesArrayLength) {}
UNINITIALIZED_TEST(HeapDumpJSArrayDictionaryElementsExactElementEdges) {}
UNINITIALIZED_TEST(HeapDumpExtractElementReferencesEdgesMatchNativeSnapshot) {}
#endif

}  // namespace heap
}  // namespace internal
}  // namespace v8
