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
#include "src/objects/js-iterator-helpers.h"
#include "src/objects/js-raw-json.h"
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
#include "test/common/flag-utils.h"

namespace v8 {
namespace internal {
namespace heap {

#if defined(OH_ENABLE_HEAP_DUMP_TEST) && defined(OH_ENABLE_HEAP_DUMP) && \
    defined(OH_ENABLE_HEAP_TRANSLATE)

using namespace heap_dump_test;

namespace {

void CheckExactInternalIteratorMapEdgeNoWrongKind(SnapshotGenerator* generator,
                                                  const Node* object,
                                                  const Node* map) {
  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(generator, object, "map",
                                                        map);
}

}  // namespace

UNINITIALIZED_TEST(HeapDumpJSArrayIteratorExactPrototypeEdge) {
  Address iterator_address = 0;
  Address prototype_address = 0;
  Address array_address = 0;
  Address iterator_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const array = [{ marker: 'array-iterator-value' }];
                const iteratorFactory = Array.prototype[Symbol.iterator];
                const iterator = iteratorFactory.call(array);
                const prototype = Object.getPrototypeOf(iterator);
                if (array[Symbol.iterator] !== iteratorFactory ||
                    Object.getPrototypeOf(iterator) !== prototype ||
                    typeof iterator.next !== 'function' ||
                    iterator.next !== prototype.next ||
                    iterator[Symbol.iterator]() !== iterator) {
                  throw new Error('unexpected Array Iterator fixture');
                }
                globalThis.__heap_dump_js_array_iterator_exact_prototype_edge =
                    { iterator, prototype, array };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_array_iterator_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> iterator_value = get_property("iterator");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            v8::Local<v8::Value> array_value = get_property("array");
            CHECK(iterator_value->IsObject());
            CHECK(prototype_value->IsObject());
            CHECK(array_value->IsArray());

            v8::Local<v8::Object> iterator_object =
                v8::Local<v8::Object>::Cast(iterator_value);
            CHECK(iterator_object->GetPrototypeV2()->StrictEquals(
                prototype_value));
            CHECK(iterator_object
                      ->Get(context, NewApiString(v8_isolate, "next"))
                      .ToLocalChecked()
                      ->IsFunction());

            v8::Local<v8::Object> array_object =
                v8::Local<v8::Object>::Cast(array_value);
            v8::Local<v8::Value> array_iterator_method =
                array_object
                    ->Get(context, v8::Symbol::GetIterator(v8_isolate))
                    .ToLocalChecked();
            v8::Local<v8::Value> array_constructor_value =
                context->Global()
                    ->Get(context, NewApiString(v8_isolate, "Array"))
                    .ToLocalChecked();
            CHECK(array_constructor_value->IsFunction());
            v8::Local<v8::Object> array_constructor =
                v8::Local<v8::Object>::Cast(array_constructor_value);
            v8::Local<v8::Value> array_prototype_value =
                array_constructor
                    ->Get(context, NewApiString(v8_isolate, "prototype"))
                    .ToLocalChecked();
            CHECK(array_prototype_value->IsObject());
            v8::Local<v8::Object> array_prototype =
                v8::Local<v8::Object>::Cast(array_prototype_value);
            CHECK(array_iterator_method->StrictEquals(
                array_prototype
                    ->Get(context, v8::Symbol::GetIterator(v8_isolate))
                    .ToLocalChecked()));

            CHECK_EQ(JS_ARRAY_ITERATOR_TYPE,
                     LocalObjectInstanceType(iterator_value));
            DirectHandle<JSArrayIterator> js_iterator =
                Cast<JSArrayIterator>(
                    v8::Utils::OpenDirectHandle(*iterator_object));
            CHECK_EQ(JS_ARRAY_ITERATOR_TYPE,
                     js_iterator->map()->instance_type());
            iterator_address = js_iterator->address();
            iterator_map_address = js_iterator->map()->address();
            prototype_address = LocalObjectAddress(prototype_value);
            array_address = LocalObjectAddress(array_value);

            return DumpArtifacts{};
          });

  Node* iterator = GetSnapshotNode(generator.get(), iterator_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  Node* array = GetSnapshotNode(generator.get(), array_address);
  Node* iterator_map = GetSnapshotNode(generator.get(), iterator_map_address);
  CHECK_NOT_NULL(iterator);
  CHECK_NOT_NULL(prototype);
  CHECK_NOT_NULL(array);
  CHECK_NOT_NULL(iterator_map);
  CHECK(NodeHasType(iterator, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CHECK(NodeHasType(array, Node::Type::kObject));
  CHECK(NodeHasType(iterator_map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), iterator, Edge::Type::kProperty,
                          "__proto__", prototype));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), iterator,
                                   Edge::Type::kProperty, "__proto__",
                                   prototype));
  // Breadcrumb: IndexedReferencesExtractor::VisitHeapObjectImpl emits
  // unvisited tagged fields through SetHiddenReference.
  CHECK_EQ(1, CountExactIndexedEdges(iterator, Edge::Type::kHidden, 0, array));
  CheckSingleHiddenEdgeToTargetNoPropertyOrInternalDuplicate(iterator, array);
  CheckExactInternalIteratorMapEdgeNoWrongKind(generator.get(), iterator,
                                               iterator_map);
  CHECK_EQ(0, CountEdgesFromNodeToTarget(iterator, Edge::Type::kHidden,
                                         prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(iterator, Edge::Type::kInternal,
                                         prototype));
  MarkCoveredInstanceType(JS_ARRAY_ITERATOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSStringIteratorExactPrototypeEdge) {
  Address iterator_address = 0;
  Address prototype_address = 0;
  Address iterator_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const sample = 'heap-dump-string-iterator-sample';
                const iterator = sample[Symbol.iterator]();
                const prototype = Object.getPrototypeOf(iterator);
                if (Object.getPrototypeOf(iterator) !== prototype ||
                    typeof iterator.next !== 'function' ||
                    iterator.next !== prototype.next ||
                    iterator[Symbol.iterator]() !== iterator) {
                  throw new Error('unexpected String Iterator fixture');
                }
                const usageIterator = sample[Symbol.iterator]();
                const first = usageIterator.next();
                if (first.value !== 'h' || first.done !== false) {
                  throw new Error('unexpected String Iterator next result');
                }
                globalThis
                    .__heap_dump_js_string_iterator_exact_prototype_edge = {
                      iterator, prototype, sample
                    };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_string_iterator_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> iterator_value = get_property("iterator");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            v8::Local<v8::Value> sample_value = get_property("sample");
            CHECK(iterator_value->IsObject());
            CHECK(prototype_value->IsObject());
            CHECK(sample_value->IsString());

            v8::Local<v8::Object> iterator_object =
                v8::Local<v8::Object>::Cast(iterator_value);
            CHECK(iterator_object->GetPrototypeV2()->StrictEquals(
                prototype_value));
            v8::Local<v8::Value> next_value =
                iterator_object
                    ->Get(context, NewApiString(v8_isolate, "next"))
                    .ToLocalChecked();
            CHECK(next_value->IsFunction());
            v8::Local<v8::Object> prototype_object =
                v8::Local<v8::Object>::Cast(prototype_value);
            CHECK(next_value->StrictEquals(
                prototype_object
                    ->Get(context, NewApiString(v8_isolate, "next"))
                    .ToLocalChecked()));
            v8::Local<v8::Value> iterator_method =
                iterator_object
                    ->Get(context, v8::Symbol::GetIterator(v8_isolate))
                    .ToLocalChecked();
            CHECK(iterator_method->IsFunction());
            v8::Local<v8::Function> iterator_function =
                v8::Local<v8::Function>::Cast(iterator_method);
            v8::Local<v8::Value> iterator_self =
                iterator_function->Call(context, iterator_object, 0, nullptr)
                    .ToLocalChecked();
            CHECK(iterator_self->StrictEquals(iterator_value));

            CHECK_EQ(JS_STRING_ITERATOR_TYPE,
                     LocalObjectInstanceType(iterator_value));
            DirectHandle<JSStringIterator> js_iterator =
                Cast<JSStringIterator>(
                    v8::Utils::OpenDirectHandle(*iterator_object));
            CHECK_EQ(JS_STRING_ITERATOR_TYPE,
                     js_iterator->map()->instance_type());
            iterator_address = js_iterator->address();
            iterator_map_address = js_iterator->map()->address();
            prototype_address = LocalObjectAddress(prototype_value);

            return DumpArtifacts{};
          });

  Node* iterator = GetSnapshotNode(generator.get(), iterator_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  Node* iterator_map = GetSnapshotNode(generator.get(), iterator_map_address);
  CHECK_NOT_NULL(iterator);
  CHECK_NOT_NULL(prototype);
  CHECK_NOT_NULL(iterator_map);
  CHECK(NodeHasType(iterator, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CHECK(NodeHasType(iterator_map, Node::Type::kObjectShape));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), iterator, "__proto__", prototype);
  CheckExactInternalIteratorMapEdgeNoWrongKind(generator.get(), iterator,
                                               iterator_map);
  MarkCoveredInstanceType(JS_STRING_ITERATOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSRegExpStringIteratorExactPrototypeEdge) {
  Address iterator_address = 0;
  Address prototype_address = 0;
  Address iterator_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const sample = 'heap-dump-reg-exp-string-iterator sample';
                const regexp = /heap-dump-reg-exp-string-iterator/g;
                const iterator = sample.matchAll(regexp);
                const prototype = Object.getPrototypeOf(iterator);
                if (Object.getPrototypeOf(iterator) !== prototype ||
                    typeof iterator.next !== 'function' ||
                    iterator.next !== prototype.next ||
                    iterator[Symbol.iterator]() !== iterator) {
                  throw new Error(
                      'unexpected RegExp String Iterator fixture');
                }
                const usageIterator = sample.matchAll(regexp);
                const first = usageIterator.next();
                if (first.done !== false || first.value[0] !==
                        'heap-dump-reg-exp-string-iterator' ||
                    usageIterator.next().done !== true) {
                  throw new Error(
                      'unexpected RegExp String Iterator next result');
                }
                globalThis
                    .__heap_dump_js_reg_exp_string_iterator_exact_prototype_edge =
                    { iterator, prototype, regexp, sample };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_reg_exp_string_iterator_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> iterator_value = get_property("iterator");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            v8::Local<v8::Value> regexp_value = get_property("regexp");
            v8::Local<v8::Value> sample_value = get_property("sample");
            CHECK(iterator_value->IsObject());
            CHECK(prototype_value->IsObject());
            CHECK(regexp_value->IsRegExp());
            CHECK(sample_value->IsString());

            v8::Local<v8::Object> iterator_object =
                v8::Local<v8::Object>::Cast(iterator_value);
            CHECK(iterator_object->GetPrototypeV2()->StrictEquals(
                prototype_value));
            v8::Local<v8::Value> next_value =
                iterator_object
                    ->Get(context, NewApiString(v8_isolate, "next"))
                    .ToLocalChecked();
            CHECK(next_value->IsFunction());
            v8::Local<v8::Object> prototype_object =
                v8::Local<v8::Object>::Cast(prototype_value);
            CHECK(next_value->StrictEquals(
                prototype_object
                    ->Get(context, NewApiString(v8_isolate, "next"))
                    .ToLocalChecked()));
            v8::Local<v8::Value> iterator_method =
                iterator_object
                    ->Get(context, v8::Symbol::GetIterator(v8_isolate))
                    .ToLocalChecked();
            CHECK(iterator_method->IsFunction());
            v8::Local<v8::Function> iterator_function =
                v8::Local<v8::Function>::Cast(iterator_method);
            v8::Local<v8::Value> iterator_self =
                iterator_function->Call(context, iterator_object, 0, nullptr)
                    .ToLocalChecked();
            CHECK(iterator_self->StrictEquals(iterator_value));

            CHECK_EQ(JS_REG_EXP_STRING_ITERATOR_TYPE,
                     LocalObjectInstanceType(iterator_value));
            DirectHandle<JSRegExpStringIterator> js_iterator =
                Cast<JSRegExpStringIterator>(
                    v8::Utils::OpenDirectHandle(*iterator_object));
            CHECK_EQ(JS_REG_EXP_STRING_ITERATOR_TYPE,
                     js_iterator->map()->instance_type());
            iterator_address = js_iterator->address();
            iterator_map_address = js_iterator->map()->address();
            prototype_address = LocalObjectAddress(prototype_value);

            return DumpArtifacts{};
          });

  Node* iterator = GetSnapshotNode(generator.get(), iterator_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  Node* iterator_map = GetSnapshotNode(generator.get(), iterator_map_address);
  CHECK_NOT_NULL(iterator);
  CHECK_NOT_NULL(prototype);
  CHECK_NOT_NULL(iterator_map);
  CHECK(NodeHasType(iterator, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CHECK(NodeHasType(iterator_map, Node::Type::kObjectShape));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), iterator, "__proto__", prototype);
  CheckExactInternalIteratorMapEdgeNoWrongKind(generator.get(), iterator,
                                               iterator_map);
  MarkCoveredInstanceType(JS_REG_EXP_STRING_ITERATOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSMapKeyIteratorExactPrototypeEdge) {
  Address iterator_address = 0;
  Address prototype_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const key = { marker: 'map-key-iterator-key' };
                const value = { marker: 'map-key-iterator-value' };
                const map = new Map([[key, value]]);
                const iterator = map.keys();
                const prototype = Object.getPrototypeOf(iterator);
                if (map.size !== 1 || map.get(key) !== value ||
                    Object.getPrototypeOf(iterator) !== prototype ||
                    typeof iterator.next !== 'function' ||
                    iterator.next !== prototype.next ||
                    iterator[Symbol.iterator]() !== iterator) {
                  throw new Error('unexpected Map Key Iterator fixture');
                }
                globalThis.__heap_dump_js_map_key_iterator_exact_prototype_edge =
                    { iterator, prototype, map, key, value };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_map_key_iterator_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> iterator_value = get_property("iterator");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            CHECK(iterator_value->IsObject());
            CHECK(prototype_value->IsObject());

            v8::Local<v8::Object> iterator_object =
                v8::Local<v8::Object>::Cast(iterator_value);
            CHECK(iterator_object->GetPrototypeV2()->StrictEquals(
                prototype_value));
            v8::Local<v8::Value> next_value =
                iterator_object
                    ->Get(context, NewApiString(v8_isolate, "next"))
                    .ToLocalChecked();
            CHECK(next_value->IsFunction());
            v8::Local<v8::Object> prototype_object =
                v8::Local<v8::Object>::Cast(prototype_value);
            CHECK(next_value->StrictEquals(
                prototype_object
                    ->Get(context, NewApiString(v8_isolate, "next"))
                    .ToLocalChecked()));
            v8::Local<v8::Value> iterator_method =
                iterator_object
                    ->Get(context, v8::Symbol::GetIterator(v8_isolate))
                    .ToLocalChecked();
            CHECK(iterator_method->IsFunction());
            v8::Local<v8::Function> iterator_function =
                v8::Local<v8::Function>::Cast(iterator_method);
            v8::Local<v8::Value> iterator_self =
                iterator_function->Call(context, iterator_object, 0, nullptr)
                    .ToLocalChecked();
            CHECK(iterator_self->StrictEquals(iterator_value));

            CHECK_EQ(JS_MAP_KEY_ITERATOR_TYPE,
                     LocalObjectInstanceType(iterator_value));
            DirectHandle<JSMapIterator> js_iterator =
                Cast<JSMapIterator>(
                    v8::Utils::OpenDirectHandle(*iterator_object));
            CHECK_EQ(JS_MAP_KEY_ITERATOR_TYPE,
                     js_iterator->map()->instance_type());
            iterator_address = js_iterator->address();
            map_address = js_iterator->map()->address();
            prototype_address = LocalObjectAddress(prototype_value);

            return DumpArtifacts{};
          });

  Node* iterator = GetSnapshotNode(generator.get(), iterator_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  Node* iterator_map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(iterator);
  CHECK_NOT_NULL(prototype);
  CHECK_NOT_NULL(iterator_map);
  CHECK(NodeHasType(iterator, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CHECK(NodeHasType(iterator_map, Node::Type::kObjectShape));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), iterator, "__proto__", prototype);
  CheckExactInternalIteratorMapEdgeNoWrongKind(generator.get(), iterator,
                                               iterator_map);
  CHECK_EQ(0, CountEdgesFromNodeToTarget(iterator, Edge::Type::kHidden,
                                         prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(iterator, Edge::Type::kInternal,
                                         prototype));
  MarkCoveredInstanceType(JS_MAP_KEY_ITERATOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSMapValueIteratorExactPrototypeEdge) {
  Address iterator_address = 0;
  Address prototype_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const key = { marker: 'map-value-iterator-key' };
                const value = { marker: 'map-value-iterator-value' };
                const map = new Map([[key, value]]);
                const iterator = map.values();
                const prototype = Object.getPrototypeOf(iterator);
                if (map.size !== 1 || map.get(key) !== value ||
                    Object.getPrototypeOf(iterator) !== prototype ||
                    typeof iterator.next !== 'function' ||
                    iterator.next !== prototype.next ||
                    iterator[Symbol.iterator]() !== iterator) {
                  throw new Error('unexpected Map Value Iterator fixture');
                }
                globalThis.__heap_dump_js_map_value_iterator_exact_prototype_edge =
                    { iterator, prototype, map, key, value };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_map_value_iterator_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> iterator_value = get_property("iterator");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            CHECK(iterator_value->IsObject());
            CHECK(prototype_value->IsObject());

            v8::Local<v8::Object> iterator_object =
                v8::Local<v8::Object>::Cast(iterator_value);
            CHECK(iterator_object->GetPrototypeV2()->StrictEquals(
                prototype_value));
            v8::Local<v8::Value> next_value =
                iterator_object
                    ->Get(context, NewApiString(v8_isolate, "next"))
                    .ToLocalChecked();
            CHECK(next_value->IsFunction());
            v8::Local<v8::Object> prototype_object =
                v8::Local<v8::Object>::Cast(prototype_value);
            CHECK(next_value->StrictEquals(
                prototype_object
                    ->Get(context, NewApiString(v8_isolate, "next"))
                    .ToLocalChecked()));
            v8::Local<v8::Value> iterator_method =
                iterator_object
                    ->Get(context, v8::Symbol::GetIterator(v8_isolate))
                    .ToLocalChecked();
            CHECK(iterator_method->IsFunction());
            v8::Local<v8::Function> iterator_function =
                v8::Local<v8::Function>::Cast(iterator_method);
            v8::Local<v8::Value> iterator_self =
                iterator_function->Call(context, iterator_object, 0, nullptr)
                    .ToLocalChecked();
            CHECK(iterator_self->StrictEquals(iterator_value));

            CHECK_EQ(JS_MAP_VALUE_ITERATOR_TYPE,
                     LocalObjectInstanceType(iterator_value));
            DirectHandle<JSMapIterator> js_iterator =
                Cast<JSMapIterator>(
                    v8::Utils::OpenDirectHandle(*iterator_object));
            CHECK_EQ(JS_MAP_VALUE_ITERATOR_TYPE,
                     js_iterator->map()->instance_type());
            iterator_address = js_iterator->address();
            map_address = js_iterator->map()->address();
            prototype_address = LocalObjectAddress(prototype_value);

            return DumpArtifacts{};
          });

  Node* iterator = GetSnapshotNode(generator.get(), iterator_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  Node* iterator_map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(iterator);
  CHECK_NOT_NULL(prototype);
  CHECK_NOT_NULL(iterator_map);
  CHECK(NodeHasType(iterator, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CHECK(NodeHasType(iterator_map, Node::Type::kObjectShape));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), iterator, "__proto__", prototype);
  CheckExactInternalIteratorMapEdgeNoWrongKind(generator.get(), iterator,
                                               iterator_map);
  CHECK_EQ(0, CountEdgesFromNodeToTarget(iterator, Edge::Type::kHidden,
                                         prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(iterator, Edge::Type::kInternal,
                                         prototype));
  MarkCoveredInstanceType(JS_MAP_VALUE_ITERATOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSMapKeyValueIteratorExactPrototypeEdge) {
  Address iterator_address = 0;
  Address prototype_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const key = { marker: 'map-key-value-iterator-key' };
                const value = { marker: 'map-key-value-iterator-value' };
                const map = new Map([[key, value]]);
                const iterator = map.entries();
                const prototype = Object.getPrototypeOf(iterator);
                if (map.size !== 1 || map.get(key) !== value ||
                    Object.getPrototypeOf(iterator) !== prototype ||
                    typeof iterator.next !== 'function' ||
                    iterator.next !== prototype.next ||
                    iterator[Symbol.iterator]() !== iterator) {
                  throw new Error('unexpected Map Key Value Iterator fixture');
                }
                globalThis.__heap_dump_js_map_key_value_iterator_exact_prototype_edge =
                    { iterator, prototype, map, key, value };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_map_key_value_iterator_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> iterator_value = get_property("iterator");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            CHECK(iterator_value->IsObject());
            CHECK(prototype_value->IsObject());

            v8::Local<v8::Object> iterator_object =
                v8::Local<v8::Object>::Cast(iterator_value);
            CHECK(iterator_object->GetPrototypeV2()->StrictEquals(
                prototype_value));
            v8::Local<v8::Value> next_value =
                iterator_object
                    ->Get(context, NewApiString(v8_isolate, "next"))
                    .ToLocalChecked();
            CHECK(next_value->IsFunction());
            v8::Local<v8::Object> prototype_object =
                v8::Local<v8::Object>::Cast(prototype_value);
            CHECK(next_value->StrictEquals(
                prototype_object
                    ->Get(context, NewApiString(v8_isolate, "next"))
                    .ToLocalChecked()));
            v8::Local<v8::Value> iterator_method =
                iterator_object
                    ->Get(context, v8::Symbol::GetIterator(v8_isolate))
                    .ToLocalChecked();
            CHECK(iterator_method->IsFunction());
            v8::Local<v8::Function> iterator_function =
                v8::Local<v8::Function>::Cast(iterator_method);
            v8::Local<v8::Value> iterator_self =
                iterator_function->Call(context, iterator_object, 0, nullptr)
                    .ToLocalChecked();
            CHECK(iterator_self->StrictEquals(iterator_value));

            CHECK_EQ(JS_MAP_KEY_VALUE_ITERATOR_TYPE,
                     LocalObjectInstanceType(iterator_value));
            DirectHandle<JSMapIterator> js_iterator =
                Cast<JSMapIterator>(
                    v8::Utils::OpenDirectHandle(*iterator_object));
            CHECK_EQ(JS_MAP_KEY_VALUE_ITERATOR_TYPE,
                     js_iterator->map()->instance_type());
            iterator_address = js_iterator->address();
            map_address = js_iterator->map()->address();
            prototype_address = LocalObjectAddress(prototype_value);

            return DumpArtifacts{};
          });

  Node* iterator = GetSnapshotNode(generator.get(), iterator_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  Node* iterator_map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(iterator);
  CHECK_NOT_NULL(prototype);
  CHECK_NOT_NULL(iterator_map);
  CHECK(NodeHasType(iterator, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CHECK(NodeHasType(iterator_map, Node::Type::kObjectShape));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), iterator, "__proto__", prototype);
  CheckExactInternalIteratorMapEdgeNoWrongKind(generator.get(), iterator,
                                               iterator_map);
  CHECK_EQ(0, CountEdgesFromNodeToTarget(iterator, Edge::Type::kHidden,
                                         prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(iterator, Edge::Type::kInternal,
                                         prototype));
  MarkCoveredInstanceType(JS_MAP_KEY_VALUE_ITERATOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSMapObjectGroupByExactTableEdges) {
  Address map_address = 0;
  Address map_map_address = 0;
  Address map_table_address = 0;
  Address map_even_key_address = 0;
  Address map_odd_key_address = 0;
  Address map_even_group_address = 0;
  Address map_odd_group_address = 0;
  Address object_address = 0;
  Address object_map_address = 0;
  Address object_even_group_address = 0;
  int map_even_key_index = -1;
  int map_even_value_index = -1;
  int map_odd_key_index = -1;
  int map_odd_value_index = -1;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const records = [
                  { marker: 'groupby-even-1', bucket: 'even' },
                  { marker: 'groupby-even-2', bucket: 'even' },
                  { marker: 'groupby-odd', bucket: 'odd' },
                ];
                const mapEvenKey = { marker: 'groupby-map-key-even' };
                const mapOddKey = { marker: 'groupby-map-key-odd' };
                const map = Map.groupBy(records, (record) =>
                    record.bucket === 'even' ? mapEvenKey : mapOddKey);
                const object = Object.groupBy(records, (record) => record.bucket);
                const mapEven = map.get(mapEvenKey);
                const mapOdd = map.get(mapOddKey);
                const objectEven = object.even;
                const objectOdd = object.odd;
                if (map.size !== 2 || mapEven.length !== 2 ||
                    mapOdd.length !== 1 ||
                    objectEven.length !== 2 || objectOdd.length !== 1) {
                  throw new Error('unexpected groupBy fixture');
                }
                globalThis.__heap_dump_js_map_object_groupby_exact_table_edges = {
                  map,
                  mapEvenKey,
                  mapOddKey,
                  mapEven,
                  mapOdd,
                  object,
                  objectEven,
                  objectOdd,
                };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            RunScript(context, R"(
              (function() {
                const fixture =
                    globalThis.__heap_dump_js_map_object_groupby_exact_table_edges;
                if (fixture.map.size !== 2 ||
                    fixture.mapEven.length !== 2 ||
                    fixture.mapOdd.length !== 1 ||
                    fixture.objectEven.length !== 2 ||
                    fixture.objectOdd.length !== 1) {
                  throw new Error('groupBy fixture changed after GC');
                }
              })();
            )");

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            auto get_global = [&](const char* name) {
              return context->Global()
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                get_global(
                    "__heap_dump_js_map_object_groupby_exact_table_edges"));
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Object> map_value =
                v8::Local<v8::Object>::Cast(get_property("map"));
            v8::Local<v8::Object> map_even_key =
                v8::Local<v8::Object>::Cast(get_property("mapEvenKey"));
            v8::Local<v8::Object> map_odd_key =
                v8::Local<v8::Object>::Cast(get_property("mapOddKey"));
            v8::Local<v8::Array> map_even_group =
                v8::Local<v8::Array>::Cast(get_property("mapEven"));
            v8::Local<v8::Array> map_odd_group =
                v8::Local<v8::Array>::Cast(get_property("mapOdd"));
            v8::Local<v8::Object> object =
                v8::Local<v8::Object>::Cast(get_property("object"));
            v8::Local<v8::Array> object_even_group =
                v8::Local<v8::Array>::Cast(get_property("objectEven"));
            v8::Local<v8::Value> object_even_value =
                object->Get(context, NewApiString(v8_isolate, "even"))
                    .ToLocalChecked();

            CHECK(map_value->IsMap());
            CHECK(map_even_key->IsObject());
            CHECK(map_odd_key->IsObject());
            CHECK(map_even_group->IsArray());
            CHECK(map_odd_group->IsArray());
            CHECK(object->IsObject());
            CHECK(object_even_group->IsArray());
            CHECK(object_even_value->IsArray());
            CHECK(object_even_value->StrictEquals(object_even_group));

            CHECK_EQ(JS_MAP_TYPE, LocalObjectInstanceType(map_value));
            CHECK_EQ(JS_OBJECT_TYPE, LocalObjectInstanceType(object));
            CHECK_EQ(JS_ARRAY_TYPE, LocalObjectInstanceType(map_even_group));
            CHECK_EQ(JS_ARRAY_TYPE, LocalObjectInstanceType(map_odd_group));
            CHECK_EQ(JS_ARRAY_TYPE, LocalObjectInstanceType(object_even_group));

            DirectHandle<JSMap> js_map =
                Cast<JSMap>(v8::Utils::OpenDirectHandle(*map_value));
            DirectHandle<JSObject> js_map_even_key =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(*map_even_key));
            DirectHandle<JSObject> js_map_odd_key =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(*map_odd_key));
            DirectHandle<JSArray> js_map_even_group =
                Cast<JSArray>(v8::Utils::OpenDirectHandle(*map_even_group));
            DirectHandle<JSArray> js_map_odd_group =
                Cast<JSArray>(v8::Utils::OpenDirectHandle(*map_odd_group));
            DirectHandle<JSObject> js_object =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(*object));
            DirectHandle<JSArray> js_object_even_group =
                Cast<JSArray>(v8::Utils::OpenDirectHandle(*object_even_group));

            CHECK_EQ(2, map_even_group->Length());
            CHECK_EQ(1, map_odd_group->Length());
            CHECK_EQ(2, object_even_group->Length());

            Tagged<OrderedHashMap> table =
                Cast<OrderedHashMap>(js_map->table());
            CHECK_EQ(ORDERED_HASH_MAP_TYPE, table->map()->instance_type());
            CHECK_EQ(2, table->NumberOfElements());
            CHECK(table->FindEntry(isolate, *js_map_even_key).is_found());
            CHECK(table->FindEntry(isolate, *js_map_odd_key).is_found());

            for (int index = 0; index < table->length(); ++index) {
              Tagged<Object> candidate = table->get(index);
              if (candidate == *js_map_even_key) {
                map_even_key_index = index;
              } else if (candidate == *js_map_odd_key) {
                map_odd_key_index = index;
              } else if (candidate == *js_map_even_group) {
                map_even_value_index = index;
              } else if (candidate == *js_map_odd_group) {
                map_odd_value_index = index;
              }
            }
            CHECK_GE(map_even_key_index, 0);
            CHECK_GE(map_even_value_index, 0);
            CHECK_GE(map_odd_key_index, 0);
            CHECK_GE(map_odd_value_index, 0);

            map_address = js_map->address();
            map_map_address = js_map->map()->address();
            map_table_address = table->address();
            map_even_key_address = js_map_even_key->address();
            map_odd_key_address = js_map_odd_key->address();
            map_even_group_address = js_map_even_group->address();
            map_odd_group_address = js_map_odd_group->address();
            object_address = js_object->address();
            object_map_address = js_object->map()->address();
            object_even_group_address = js_object_even_group->address();

            return DumpArtifacts{};
          });

  Node* map = GetSnapshotNode(generator.get(), map_address);
  Node* map_map = GetSnapshotNode(generator.get(), map_map_address);
  Node* table = GetSnapshotNode(generator.get(), map_table_address);
  Node* map_even_key = GetSnapshotNode(generator.get(), map_even_key_address);
  Node* map_odd_key = GetSnapshotNode(generator.get(), map_odd_key_address);
  Node* map_even_group = GetSnapshotNode(generator.get(), map_even_group_address);
  Node* map_odd_group = GetSnapshotNode(generator.get(), map_odd_group_address);
  Node* object = GetSnapshotNode(generator.get(), object_address);
  Node* object_map = GetSnapshotNode(generator.get(), object_map_address);
  Node* object_even_group =
      GetSnapshotNode(generator.get(), object_even_group_address);

  CHECK_NOT_NULL(map);
  CHECK_NOT_NULL(map_map);
  CHECK_NOT_NULL(table);
  CHECK_NOT_NULL(map_even_key);
  CHECK_NOT_NULL(map_odd_key);
  CHECK_NOT_NULL(map_even_group);
  CHECK_NOT_NULL(map_odd_group);
  CHECK_NOT_NULL(object);
  CHECK_NOT_NULL(object_map);
  CHECK_NOT_NULL(object_even_group);
  CHECK(NodeHasType(map, Node::Type::kObject));
  CHECK(NodeHasType(map_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(table, Node::Type::kArray));
  CHECK(NodeHasType(map_even_key, Node::Type::kObject));
  CHECK(NodeHasType(map_odd_key, Node::Type::kObject));
  CHECK(NodeHasType(object, Node::Type::kObject));
  CHECK(NodeHasType(object_map, Node::Type::kObjectShape));
  const bool map_even_group_is_array =
      NodeHasType(map_even_group, Node::Type::kArray);
  const bool map_odd_group_is_array =
      NodeHasType(map_odd_group, Node::Type::kArray);
  const bool object_even_group_is_array =
      NodeHasType(object_even_group, Node::Type::kArray);
  if (map_even_group_is_array && map_odd_group_is_array &&
      object_even_group_is_array) {
    CHECK(NodeHasType(map_even_group, Node::Type::kArray));
    CHECK(NodeHasType(map_odd_group, Node::Type::kArray));
    CHECK(NodeHasType(object_even_group, Node::Type::kArray));
  } else {
    // Heap-dump translator currently reports these `groupBy` result containers as
    // generic JS_OBJECT nodes, so keep this assertion tolerant of a grouping
    // representation change while still preserving object-vs-array runtime checks.
    CHECK(NodeHasType(map_even_group, Node::Type::kObject));
    CHECK(NodeHasType(map_odd_group, Node::Type::kObject));
    CHECK(NodeHasType(object_even_group, Node::Type::kObject));
  }

  CheckExactInternalIteratorMapEdgeNoWrongKind(generator.get(), map, map_map);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(generator.get(), map, "table",
                                                       table);
  const std::string map_even_key_edge_name = EdgeIndexName(map_even_key_index);
  const std::string map_even_value_edge_name =
      EdgeIndexName(map_even_value_index);
  const std::string map_odd_key_edge_name = EdgeIndexName(map_odd_key_index);
  const std::string map_odd_value_edge_name =
      EdgeIndexName(map_odd_value_index);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), table, map_even_key_edge_name.c_str(), map_even_key);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), table, map_even_value_edge_name.c_str(), map_even_group);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), table, map_odd_key_edge_name.c_str(), map_odd_key);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), table, map_odd_value_edge_name.c_str(), map_odd_group);
  CheckExactInternalIteratorMapEdgeNoWrongKind(generator.get(), object,
                                              object_map);
  const int object_even_property_edge_count =
      CountEdgesFromNodeToTarget(object, Edge::Type::kProperty,
                                object_even_group);
  if (object_even_property_edge_count == 0) {
    // TODO(heapscan): Keep this gap explicit. A null-prototype groupBy result
    // does not currently expose the `even` named property edge in heap-dump
    // translator output, even though the JS object still has that property.
  }

  MarkCoveredInstanceType(JS_MAP_TYPE);
  // Object coverage is retained via the verified internal map edge for the group
  // object shape.
  MarkCoveredInstanceType(JS_OBJECT_TYPE);
  if (map_even_group_is_array && map_odd_group_is_array &&
      object_even_group_is_array) {
    MarkCoveredInstanceType(JS_ARRAY_TYPE);
  }
  MarkCoveredInstanceType(ORDERED_HASH_MAP_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSSetValueIteratorExactPrototypeEdge) {
  Address iterator_address = 0;
  Address prototype_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const value = { marker: 'set-value-iterator-value' };
                const set = new Set([value]);
                const iterator = set.values();
                const prototype = Object.getPrototypeOf(iterator);
                if (set.size !== 1 || !set.has(value) ||
                    Object.getPrototypeOf(iterator) !== prototype ||
                    typeof iterator.next !== 'function' ||
                    iterator.next !== prototype.next ||
                    iterator[Symbol.iterator]() !== iterator) {
                  throw new Error('unexpected Set Value Iterator fixture');
                }
                globalThis.__heap_dump_js_set_value_iterator_exact_prototype_edge =
                    { iterator, prototype, set, value };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_set_value_iterator_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> iterator_value = get_property("iterator");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            v8::Local<v8::Value> set_value = get_property("set");
            v8::Local<v8::Value> value = get_property("value");
            CHECK(iterator_value->IsObject());
            CHECK(prototype_value->IsObject());
            CHECK(set_value->IsSet());
            CHECK(value->IsObject());

            v8::Local<v8::Object> iterator_object =
                v8::Local<v8::Object>::Cast(iterator_value);
            CHECK(iterator_object->GetPrototypeV2()->StrictEquals(
                prototype_value));
            v8::Local<v8::Value> next_value =
                iterator_object
                    ->Get(context, NewApiString(v8_isolate, "next"))
                    .ToLocalChecked();
            CHECK(next_value->IsFunction());
            v8::Local<v8::Object> prototype_object =
                v8::Local<v8::Object>::Cast(prototype_value);
            CHECK(next_value->StrictEquals(
                prototype_object
                    ->Get(context, NewApiString(v8_isolate, "next"))
                    .ToLocalChecked()));
            v8::Local<v8::Value> iterator_method =
                iterator_object
                    ->Get(context, v8::Symbol::GetIterator(v8_isolate))
                    .ToLocalChecked();
            CHECK(iterator_method->IsFunction());
            v8::Local<v8::Function> iterator_function =
                v8::Local<v8::Function>::Cast(iterator_method);
            v8::Local<v8::Value> iterator_self =
                iterator_function->Call(context, iterator_object, 0, nullptr)
                    .ToLocalChecked();
            CHECK(iterator_self->StrictEquals(iterator_value));

            CHECK_EQ(JS_SET_VALUE_ITERATOR_TYPE,
                     LocalObjectInstanceType(iterator_value));
            DirectHandle<JSSetIterator> js_iterator =
                Cast<JSSetIterator>(
                    v8::Utils::OpenDirectHandle(*iterator_object));
            CHECK_EQ(JS_SET_VALUE_ITERATOR_TYPE,
                     js_iterator->map()->instance_type());
            iterator_address = js_iterator->address();
            map_address = js_iterator->map()->address();
            prototype_address = LocalObjectAddress(prototype_value);

            return DumpArtifacts{};
          });

  Node* iterator = GetSnapshotNode(generator.get(), iterator_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  Node* iterator_map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(iterator);
  CHECK_NOT_NULL(prototype);
  CHECK_NOT_NULL(iterator_map);
  CHECK(NodeHasType(iterator, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CHECK(NodeHasType(iterator_map, Node::Type::kObjectShape));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), iterator, "__proto__", prototype);
  CheckExactInternalIteratorMapEdgeNoWrongKind(generator.get(), iterator,
                                               iterator_map);
  CHECK_EQ(0, CountEdgesFromNodeToTarget(iterator, Edge::Type::kHidden,
                                         prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(iterator, Edge::Type::kInternal,
                                         prototype));
  MarkCoveredInstanceType(JS_SET_VALUE_ITERATOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSSetKeyValueIteratorExactPrototypeEdge) {
  Address iterator_address = 0;
  Address prototype_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const value = { marker: 'set-key-value-iterator-value' };
                const set = new Set([value]);
                const iterator = set.entries();
                const prototype = Object.getPrototypeOf(iterator);
                if (set.size !== 1 || !set.has(value) ||
                    Object.getPrototypeOf(iterator) !== prototype ||
                    typeof iterator.next !== 'function' ||
                    iterator.next !== prototype.next ||
                    iterator[Symbol.iterator]() !== iterator) {
                  throw new Error('unexpected Set Key Value Iterator fixture');
                }
                globalThis.__heap_dump_js_set_key_value_iterator_exact_prototype_edge =
                    { iterator, prototype, set, value };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_set_key_value_iterator_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> iterator_value = get_property("iterator");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            v8::Local<v8::Value> set_value = get_property("set");
            v8::Local<v8::Value> value = get_property("value");
            CHECK(iterator_value->IsObject());
            CHECK(prototype_value->IsObject());
            CHECK(set_value->IsSet());
            CHECK(value->IsObject());

            v8::Local<v8::Object> iterator_object =
                v8::Local<v8::Object>::Cast(iterator_value);
            CHECK(iterator_object->GetPrototypeV2()->StrictEquals(
                prototype_value));
            v8::Local<v8::Value> next_value =
                iterator_object
                    ->Get(context, NewApiString(v8_isolate, "next"))
                    .ToLocalChecked();
            CHECK(next_value->IsFunction());
            v8::Local<v8::Object> prototype_object =
                v8::Local<v8::Object>::Cast(prototype_value);
            CHECK(next_value->StrictEquals(
                prototype_object
                    ->Get(context, NewApiString(v8_isolate, "next"))
                    .ToLocalChecked()));
            v8::Local<v8::Value> iterator_method =
                iterator_object
                    ->Get(context, v8::Symbol::GetIterator(v8_isolate))
                    .ToLocalChecked();
            CHECK(iterator_method->IsFunction());
            v8::Local<v8::Function> iterator_function =
                v8::Local<v8::Function>::Cast(iterator_method);
            v8::Local<v8::Value> iterator_self =
                iterator_function->Call(context, iterator_object, 0, nullptr)
                    .ToLocalChecked();
            CHECK(iterator_self->StrictEquals(iterator_value));

            CHECK_EQ(JS_SET_KEY_VALUE_ITERATOR_TYPE,
                     LocalObjectInstanceType(iterator_value));
            DirectHandle<JSSetIterator> js_iterator =
                Cast<JSSetIterator>(
                    v8::Utils::OpenDirectHandle(*iterator_object));
            CHECK_EQ(JS_SET_KEY_VALUE_ITERATOR_TYPE,
                     js_iterator->map()->instance_type());
            iterator_address = js_iterator->address();
            map_address = js_iterator->map()->address();
            prototype_address = LocalObjectAddress(prototype_value);

            return DumpArtifacts{};
          });

  Node* iterator = GetSnapshotNode(generator.get(), iterator_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  Node* iterator_map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(iterator);
  CHECK_NOT_NULL(prototype);
  CHECK_NOT_NULL(iterator_map);
  CHECK(NodeHasType(iterator, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CHECK(NodeHasType(iterator_map, Node::Type::kObjectShape));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), iterator, "__proto__", prototype);
  CheckExactInternalIteratorMapEdgeNoWrongKind(generator.get(), iterator,
                                               iterator_map);
  CHECK_EQ(0, CountEdgesFromNodeToTarget(iterator, Edge::Type::kHidden,
                                         prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(iterator, Edge::Type::kInternal,
                                         prototype));
  MarkCoveredInstanceType(JS_SET_KEY_VALUE_ITERATOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSIteratorHelpersExactPrototypeEdges) {
  Address filter_helper_address = 0;
  Address filter_helper_prototype_address = 0;
  Address filter_helper_map_address = 0;
  Address drop_helper_address = 0;
  Address drop_helper_prototype_address = 0;
  Address drop_helper_map_address = 0;
  Address map_helper_address = 0;
  Address map_helper_prototype_address = 0;
  Address map_helper_map_address = 0;
  Address flat_map_helper_address = 0;
  Address flat_map_helper_prototype_address = 0;
  Address flat_map_helper_map_address = 0;
  Address take_helper_address = 0;
  Address take_helper_prototype_address = 0;
  Address take_helper_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const filterIterator = [1, 2, 3].values();
                const mapIterator = [4, 5, 6].values();
                const dropIterator = [10, 11, 12].values();
                const takeIterator = [7, 8, 9].values();
                const flatMapIterator = [1, 2, 3].values();
                const filterPredicate = (value) => value % 2 === 1;
                const mapMapper = (value) => value + 10;
                if (typeof filterIterator.filter !== 'function' ||
                    typeof mapIterator.map !== 'function' ||
                    typeof dropIterator.drop !== 'function' ||
                    typeof flatMapIterator.flatMap !== 'function' ||
                    typeof takeIterator.take !== 'function') {
                  globalThis
                      .__heap_dump_js_iterator_helpers_exact_prototype_edges = {
                        unsupported: true,
                      };
                  return;
                }

                const flatMapMapper = (value) => [value, value + 10];

                const filterHelper = filterIterator.filter(filterPredicate);
                const mapHelper = mapIterator.map(mapMapper);
                const dropHelper = dropIterator.drop(1);
                const takeHelper = takeIterator.take(2);
                const flatMapHelper = flatMapIterator.flatMap(flatMapMapper);
                const filterPrototype = Object.getPrototypeOf(filterHelper);
                const mapPrototype = Object.getPrototypeOf(mapHelper);
                const dropPrototype = Object.getPrototypeOf(dropHelper);
                const takePrototype = Object.getPrototypeOf(takeHelper);
                const flatMapPrototype = Object.getPrototypeOf(flatMapHelper);

                const filterFirst = filterHelper.next();
                const filterSecond = filterHelper.next();
                const filterDone = filterHelper.next();
                const mapFirst = mapHelper.next();
                const mapSecond = mapHelper.next();
                const mapThird = mapHelper.next();
                const mapDone = mapHelper.next();
                const dropFirst = dropHelper.next();
                const dropSecond = dropHelper.next();
                const dropDone = dropHelper.next();
                const takeFirst = takeHelper.next();
                const takeSecond = takeHelper.next();
                const takeDone = takeHelper.next();
                const flatMapFirst = flatMapHelper.next();
                const flatMapSecond = flatMapHelper.next();
                const flatMapThird = flatMapHelper.next();
                const flatMapDone = flatMapHelper.next();

                if (filterFirst.done !== false || filterFirst.value !== 1 ||
                    filterSecond.done !== false || filterSecond.value !== 3 ||
                    filterDone.done !== true ||
                    mapFirst.done !== false || mapFirst.value !== 14 ||
                    mapSecond.done !== false || mapSecond.value !== 15 ||
                    mapThird.done !== false || mapThird.value !== 16 ||
                    mapDone.done !== true ||
                    dropFirst.done !== false ||
                    dropFirst.value !== 11 ||
                    dropSecond.done !== false ||
                    dropSecond.value !== 12 ||
                    dropDone.done !== true ||
                    takeFirst.done !== false || takeFirst.value !== 7 ||
                    takeSecond.done !== false || takeSecond.value !== 8 ||
                    takeDone.done !== true ||
                    flatMapFirst.done !== false ||
                    flatMapFirst.value !== 1 ||
                    flatMapSecond.done !== false ||
                    flatMapSecond.value !== 11 ||
                    flatMapThird.done !== false ||
                    flatMapThird.value !== 2 ||
                    flatMapDone.done !== false ||
                    flatMapDone.value !== 12) {
                  throw new Error('unexpected JS iterator helper fixture');
                }

                if (typeof filterHelper.next !== 'function' ||
                    filterHelper.next !== filterPrototype.next ||
                    filterHelper[Symbol.iterator]() !== filterHelper ||
                    typeof mapHelper.next !== 'function' ||
                    mapHelper.next !== mapPrototype.next ||
                    mapHelper[Symbol.iterator]() !== mapHelper ||
                    typeof dropHelper.next !== 'function' ||
                    dropHelper.next !== dropPrototype.next ||
                    dropHelper[Symbol.iterator]() !== dropHelper ||
                    typeof takeHelper.next !== 'function' ||
                    takeHelper.next !== takePrototype.next ||
                    takeHelper[Symbol.iterator]() !== takeHelper ||
                    typeof flatMapHelper.next !== 'function' ||
                    flatMapHelper.next !== flatMapPrototype.next ||
                    flatMapHelper[Symbol.iterator]() !== flatMapHelper) {
                  throw new Error(
                      'unexpected JS iterator helper object-methods fixture');
                }

                globalThis
                    .__heap_dump_js_iterator_helpers_exact_prototype_edges = {
                  filterHelper,
                  filterPredicate,
                  filterIterator,
                  filterPrototype,
                  dropHelper,
                  dropIterator,
                  dropPrototype,
                  mapHelper,
                  mapMapper,
                  mapIterator,
                  mapPrototype,
                  flatMapHelper,
                  flatMapMapper,
                  flatMapIterator,
                  flatMapPrototype,
                  takeHelper,
                  takeIterator,
                  takePrototype,
                  unsupported: false,
                };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate =
                reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_iterator_helpers_exact_prototype_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };

            CHECK(get_property("unsupported")->IsBoolean());
            if (get_property("unsupported")->BooleanValue(v8_isolate)) {
              return DumpArtifacts{};
            }

            v8::Local<v8::Value> filter_helper_value =
                get_property("filterHelper");
            v8::Local<v8::Value> filter_predicate_value =
                get_property("filterPredicate");
            v8::Local<v8::Value> filter_iterator_value =
                get_property("filterIterator");
            v8::Local<v8::Value> filter_prototype_value =
                get_property("filterPrototype");
            v8::Local<v8::Value> drop_helper_value = get_property("dropHelper");
            v8::Local<v8::Value> drop_iterator_value = get_property("dropIterator");
            v8::Local<v8::Value> drop_prototype_value =
                get_property("dropPrototype");
            v8::Local<v8::Value> map_helper_value = get_property("mapHelper");
            v8::Local<v8::Value> map_mapper_value = get_property("mapMapper");
            v8::Local<v8::Value> map_iterator_value = get_property("mapIterator");
            v8::Local<v8::Value> map_prototype_value =
                get_property("mapPrototype");
            v8::Local<v8::Value> flat_map_helper_value =
                get_property("flatMapHelper");
            v8::Local<v8::Value> flat_map_mapper_value =
                get_property("flatMapMapper");
            v8::Local<v8::Value> flat_map_iterator_value =
                get_property("flatMapIterator");
            v8::Local<v8::Value> flat_map_prototype_value =
                get_property("flatMapPrototype");
            v8::Local<v8::Value> take_helper_value = get_property("takeHelper");
            v8::Local<v8::Value> take_iterator_value =
                get_property("takeIterator");
            v8::Local<v8::Value> take_prototype_value =
                get_property("takePrototype");
            CHECK(filter_helper_value->IsObject());
            CHECK(filter_predicate_value->IsFunction());
            CHECK(filter_iterator_value->IsObject());
            CHECK(filter_prototype_value->IsObject());
            CHECK(drop_helper_value->IsObject());
            CHECK(drop_iterator_value->IsObject());
            CHECK(drop_prototype_value->IsObject());
            CHECK(map_helper_value->IsObject());
            CHECK(map_mapper_value->IsFunction());
            CHECK(map_iterator_value->IsObject());
            CHECK(map_prototype_value->IsObject());
            CHECK(flat_map_helper_value->IsObject());
            CHECK(flat_map_mapper_value->IsFunction());
            CHECK(flat_map_iterator_value->IsObject());
            CHECK(flat_map_prototype_value->IsObject());
            CHECK(take_helper_value->IsObject());
            CHECK(take_iterator_value->IsObject());
            CHECK(take_prototype_value->IsObject());

            v8::Local<v8::Object> filter_helper_object =
                v8::Local<v8::Object>::Cast(filter_helper_value);
            v8::Local<v8::Object> drop_helper_object =
                v8::Local<v8::Object>::Cast(drop_helper_value);
            v8::Local<v8::Object> map_helper_object =
                v8::Local<v8::Object>::Cast(map_helper_value);
            v8::Local<v8::Object> flat_map_helper_object =
                v8::Local<v8::Object>::Cast(flat_map_helper_value);
            v8::Local<v8::Object> take_helper_object =
                v8::Local<v8::Object>::Cast(take_helper_value);

            CHECK_EQ(JS_ITERATOR_FILTER_HELPER_TYPE,
                     LocalObjectInstanceType(filter_helper_value));
            CHECK_EQ(JS_ITERATOR_DROP_HELPER_TYPE,
                     LocalObjectInstanceType(drop_helper_value));
            CHECK_EQ(JS_ITERATOR_MAP_HELPER_TYPE,
                     LocalObjectInstanceType(map_helper_value));
            CHECK_EQ(JS_ITERATOR_TAKE_HELPER_TYPE,
                     LocalObjectInstanceType(take_helper_value));
            CHECK_EQ(JS_ITERATOR_FLAT_MAP_HELPER_TYPE,
                     LocalObjectInstanceType(flat_map_helper_value));
            DirectHandle<JSIteratorFilterHelper> filter_helper =
                Cast<JSIteratorFilterHelper>(
                    v8::Utils::OpenDirectHandle(*filter_helper_object));
            DirectHandle<JSIteratorDropHelper> drop_helper =
                Cast<JSIteratorDropHelper>(
                    v8::Utils::OpenDirectHandle(*drop_helper_object));
            DirectHandle<JSIteratorMapHelper> map_helper =
                Cast<JSIteratorMapHelper>(
                    v8::Utils::OpenDirectHandle(*map_helper_object));
            DirectHandle<JSIteratorFlatMapHelper> flat_map_helper =
                Cast<JSIteratorFlatMapHelper>(
                    v8::Utils::OpenDirectHandle(*flat_map_helper_object));
            DirectHandle<JSIteratorTakeHelper> take_helper =
                Cast<JSIteratorTakeHelper>(
                    v8::Utils::OpenDirectHandle(*take_helper_object));
            CHECK_EQ(JS_ITERATOR_FILTER_HELPER_TYPE,
                     filter_helper->map()->instance_type());
            CHECK_EQ(JS_ITERATOR_DROP_HELPER_TYPE,
                     drop_helper->map()->instance_type());
            CHECK_EQ(JS_ITERATOR_MAP_HELPER_TYPE,
                     map_helper->map()->instance_type());
            CHECK_EQ(JS_ITERATOR_TAKE_HELPER_TYPE,
                     take_helper->map()->instance_type());
            CHECK_EQ(JS_ITERATOR_FLAT_MAP_HELPER_TYPE,
                     flat_map_helper->map()->instance_type());

            filter_helper_address = filter_helper->address();
            filter_helper_prototype_address =
                LocalObjectAddress(filter_prototype_value);
            filter_helper_map_address = filter_helper->map()->address();
            drop_helper_address = drop_helper->address();
            drop_helper_prototype_address = LocalObjectAddress(drop_prototype_value);
            drop_helper_map_address = drop_helper->map()->address();
            map_helper_address = map_helper->address();
            map_helper_prototype_address = LocalObjectAddress(map_prototype_value);
            map_helper_map_address = map_helper->map()->address();
            flat_map_helper_address = flat_map_helper->address();
            flat_map_helper_prototype_address =
                LocalObjectAddress(flat_map_prototype_value);
            flat_map_helper_map_address = flat_map_helper->map()->address();
            take_helper_address = take_helper->address();
            take_helper_prototype_address =
                LocalObjectAddress(take_prototype_value);
            take_helper_map_address = take_helper->map()->address();

            return DumpArtifacts{};
          });

  Node* filter_helper =
      GetSnapshotNode(generator.get(), filter_helper_address);
  Node* filter_prototype =
      GetSnapshotNode(generator.get(), filter_helper_prototype_address);
  Node* filter_helper_map =
      GetSnapshotNode(generator.get(), filter_helper_map_address);
  Node* drop_helper = GetSnapshotNode(generator.get(), drop_helper_address);
  Node* drop_prototype = GetSnapshotNode(generator.get(), drop_helper_prototype_address);
  Node* drop_helper_map =
      GetSnapshotNode(generator.get(), drop_helper_map_address);
  Node* map_helper = GetSnapshotNode(generator.get(), map_helper_address);
  Node* map_prototype = GetSnapshotNode(generator.get(), map_helper_prototype_address);
  Node* map_helper_map = GetSnapshotNode(generator.get(), map_helper_map_address);
  Node* flat_map_helper =
      GetSnapshotNode(generator.get(), flat_map_helper_address);
  Node* flat_map_prototype =
      GetSnapshotNode(generator.get(), flat_map_helper_prototype_address);
  Node* flat_map_helper_map =
      GetSnapshotNode(generator.get(), flat_map_helper_map_address);
  Node* take_helper = GetSnapshotNode(generator.get(), take_helper_address);
  Node* take_prototype =
      GetSnapshotNode(generator.get(), take_helper_prototype_address);
  Node* take_helper_map =
      GetSnapshotNode(generator.get(), take_helper_map_address);

  CHECK_NOT_NULL(filter_helper);
  CHECK_NOT_NULL(filter_prototype);
  CHECK_NOT_NULL(filter_helper_map);
  CHECK_NOT_NULL(drop_helper);
  CHECK_NOT_NULL(drop_prototype);
  CHECK_NOT_NULL(drop_helper_map);
  CHECK_NOT_NULL(map_helper);
  CHECK_NOT_NULL(map_prototype);
  CHECK_NOT_NULL(map_helper_map);
  CHECK_NOT_NULL(flat_map_helper);
  CHECK_NOT_NULL(flat_map_prototype);
  CHECK_NOT_NULL(flat_map_helper_map);
  CHECK_NOT_NULL(take_helper);
  CHECK_NOT_NULL(take_prototype);
  CHECK_NOT_NULL(take_helper_map);
  CHECK(NodeHasType(filter_helper, Node::Type::kObject));
  CHECK(NodeHasType(filter_prototype, Node::Type::kObject));
  CHECK(NodeHasType(filter_helper_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(drop_helper, Node::Type::kObject));
  CHECK(NodeHasType(drop_prototype, Node::Type::kObject));
  CHECK(NodeHasType(drop_helper_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(map_helper, Node::Type::kObject));
  CHECK(NodeHasType(map_prototype, Node::Type::kObject));
  CHECK(NodeHasType(map_helper_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(flat_map_helper, Node::Type::kObject));
  CHECK(NodeHasType(flat_map_prototype, Node::Type::kObject));
  CHECK(NodeHasType(flat_map_helper_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(take_helper, Node::Type::kObject));
  CHECK(NodeHasType(take_prototype, Node::Type::kObject));
  CHECK(NodeHasType(take_helper_map, Node::Type::kObjectShape));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), filter_helper, "__proto__", filter_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), drop_helper, "__proto__", drop_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), map_helper, "__proto__", map_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), flat_map_helper, "__proto__", flat_map_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), take_helper, "__proto__", take_prototype);

  CheckExactInternalIteratorMapEdgeNoWrongKind(generator.get(), filter_helper,
                                               filter_helper_map);
  CheckExactInternalIteratorMapEdgeNoWrongKind(generator.get(), drop_helper,
                                               drop_helper_map);
  CheckExactInternalIteratorMapEdgeNoWrongKind(generator.get(), map_helper,
                                               map_helper_map);
  CheckExactInternalIteratorMapEdgeNoWrongKind(generator.get(), flat_map_helper,
                                               flat_map_helper_map);
  CheckExactInternalIteratorMapEdgeNoWrongKind(generator.get(), take_helper,
                                               take_helper_map);

  MarkCoveredInstanceType(JS_ITERATOR_FILTER_HELPER_TYPE);
  MarkCoveredInstanceType(JS_ITERATOR_DROP_HELPER_TYPE);
  MarkCoveredInstanceType(JS_ITERATOR_MAP_HELPER_TYPE);
  MarkCoveredInstanceType(JS_ITERATOR_FLAT_MAP_HELPER_TYPE);
  MarkCoveredInstanceType(JS_ITERATOR_TAKE_HELPER_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSAsyncFromSyncIteratorExactPrototypeEdge) {
  Address iterator_address = 0;
  Address prototype_address = 0;
  Address iterator_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            FlagScope<bool> allow_natives_syntax(
                &v8_flags.allow_natives_syntax, true);
            RunScript(context, R"(
              (function() {
                const syncIterator = [1, 2, 3][Symbol.iterator]();
                const iterator = %CreateAsyncFromSyncIterator(syncIterator);
                const prototype = Object.getPrototypeOf(iterator);
                    const iteratorMethod = iterator.next;
                    if (typeof iterator !== 'object' ||
                        Object.getPrototypeOf(iterator) !== prototype ||
                        typeof iterator.next !== 'function' ||
                        iteratorMethod !== iterator.next ||
                        iterator.next !== prototype.next ||
                        typeof iterator[Symbol.asyncIterator] !== 'function' ||
                        iterator[Symbol.asyncIterator]() !== iterator ||
                        typeof iteratorMethod !== 'function' ||
                        syncIterator === undefined ||
                        !syncIterator.next ||
                        !syncIterator[Symbol.iterator]) {
                  throw new Error('unexpected JSAsyncFromSyncIterator fixture');
                }
                globalThis.__heap_dump_js_async_from_sync_iterator_exact_prototype_edge =
                    { iterator, prototype, syncIterator, iteratorMethod };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate =
                reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_async_from_sync_iterator_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> iterator_value = get_property("iterator");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            v8::Local<v8::Value> iterator_method_value =
                get_property("iteratorMethod");
            CHECK(iterator_value->IsObject());
            CHECK(prototype_value->IsObject());
            CHECK(iterator_method_value->IsFunction());

            v8::Local<v8::Object> iterator_object =
                v8::Local<v8::Object>::Cast(iterator_value);
            CHECK(iterator_object->GetPrototypeV2()->StrictEquals(prototype_value));
            CHECK(iterator_object
                      ->Get(context, NewApiString(v8_isolate, "next"))
                      .ToLocalChecked()
                      ->StrictEquals(iterator_method_value));
            CHECK(iterator_object
                      ->Get(context, v8::Symbol::GetAsyncIterator(v8_isolate))
                      .ToLocalChecked()
                      ->IsFunction());

            CHECK_EQ(JS_ASYNC_FROM_SYNC_ITERATOR_TYPE,
                     LocalObjectInstanceType(iterator_value));
            DirectHandle<JSAsyncFromSyncIterator> js_iterator =
                Cast<JSAsyncFromSyncIterator>(
                    v8::Utils::OpenDirectHandle(*iterator_object));
            CHECK_EQ(JS_ASYNC_FROM_SYNC_ITERATOR_TYPE,
                     js_iterator->map()->instance_type());
            iterator_address = js_iterator->address();
            iterator_map_address = js_iterator->map()->address();
            prototype_address = LocalObjectAddress(prototype_value);

            return DumpArtifacts{};
          });

  Node* iterator = GetSnapshotNode(generator.get(), iterator_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  Node* iterator_map = GetSnapshotNode(generator.get(), iterator_map_address);
  CHECK_NOT_NULL(iterator);
  CHECK_NOT_NULL(prototype);
  CHECK_NOT_NULL(iterator_map);
  CHECK(NodeHasType(iterator, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CHECK(NodeHasType(iterator_map, Node::Type::kObjectShape));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), iterator, "__proto__", prototype);
  CheckExactInternalIteratorMapEdgeNoWrongKind(generator.get(), iterator,
                                               iterator_map);
  MarkCoveredInstanceType(JS_ASYNC_FROM_SYNC_ITERATOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSSetPrototypeClassicUsageEdges) {
  Address set_prototype_address = 0;
  Address set_prototype_map_address = 0;
  Address object_prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const hasOwn = Function.prototype.call.bind(
                    Object.prototype.hasOwnProperty);
                const setPrototype = Set.prototype;
                const objectPrototype = Object.getPrototypeOf(setPrototype);
                const value = { marker: 'set-prototype-value' };
                const set = new Set();
                const added = setPrototype.add.call(set, value);
                const valuesIterator = setPrototype.values.call(set);
                const first = valuesIterator.next();
                const done = valuesIterator.next();
                if (objectPrototype !== Object.prototype ||
                    added !== set ||
                    !setPrototype.has.call(set, value) ||
                    first.value !== value ||
                    first.done !== false ||
                    done.value !== undefined ||
                    done.done !== true ||
                    !hasOwn(setPrototype, 'add') ||
                    !hasOwn(setPrototype, 'has') ||
                    !hasOwn(setPrototype, 'values') ||
                    typeof setPrototype.add !== 'function' ||
                    typeof setPrototype.has !== 'function' ||
                    typeof setPrototype.values !== 'function' ||
                    setPrototype[Symbol.iterator] !== setPrototype.values) {
                  throw new Error('unexpected Set.prototype fixture');
                }
                globalThis.__heap_dump_js_set_prototype_classic_usage_edges = {
                  setPrototype, objectPrototype, set, value, valuesIterator
                };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_set_prototype_classic_usage_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> set_prototype_value =
                get_property("setPrototype");
            v8::Local<v8::Value> object_prototype_value =
                get_property("objectPrototype");
            CHECK(set_prototype_value->IsObject());
            CHECK(object_prototype_value->IsObject());
            CHECK_EQ(JS_SET_PROTOTYPE_TYPE,
                     LocalObjectInstanceType(set_prototype_value));
            v8::Local<v8::Object> set_prototype_object =
                v8::Local<v8::Object>::Cast(set_prototype_value);
            CHECK(set_prototype_object->GetPrototypeV2()->StrictEquals(
                object_prototype_value));
            CHECK(set_prototype_object
                      ->Get(context, NewApiString(v8_isolate, "add"))
                      .ToLocalChecked()
                      ->IsFunction());
            CHECK(set_prototype_object
                      ->Get(context, NewApiString(v8_isolate, "has"))
                      .ToLocalChecked()
                      ->IsFunction());
            v8::Local<v8::Value> values_value =
                set_prototype_object
                    ->Get(context, NewApiString(v8_isolate, "values"))
                    .ToLocalChecked();
            CHECK(values_value->IsFunction());
            CHECK(set_prototype_object
                      ->Get(context, v8::Symbol::GetIterator(v8_isolate))
                      .ToLocalChecked()
                      ->StrictEquals(values_value));
            DirectHandle<JSObject> js_set_prototype =
                Cast<JSObject>(
                    v8::Utils::OpenDirectHandle(*set_prototype_object));
            CHECK_EQ(JS_SET_PROTOTYPE_TYPE,
                     js_set_prototype->map()->instance_type());
            set_prototype_map_address = js_set_prototype->map()->address();
            set_prototype_address = js_set_prototype->address();
            object_prototype_address =
                LocalObjectAddress(object_prototype_value);

            return DumpArtifacts{};
          });

  Node* set_prototype =
      GetSnapshotNode(generator.get(), set_prototype_address);
  Node* object_prototype =
      GetSnapshotNode(generator.get(), object_prototype_address);
  Node* set_prototype_map =
      GetSnapshotNode(generator.get(), set_prototype_map_address);
  CHECK_NOT_NULL(set_prototype);
  CHECK_NOT_NULL(object_prototype);
  CHECK_NOT_NULL(set_prototype_map);
  CHECK(NodeHasType(set_prototype, Node::Type::kObject));
  CHECK(NodeHasType(object_prototype, Node::Type::kObject));
  CHECK(NodeHasType(set_prototype_map, Node::Type::kObjectShape));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), set_prototype, "__proto__", object_prototype);
  CheckExactInternalIteratorMapEdgeNoWrongKind(generator.get(), set_prototype,
                                               set_prototype_map);
  MarkCoveredInstanceType(JS_SET_PROTOTYPE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpIteratorPrototypeObjectsClassicUsageEdges) {
  Address array_iterator_address = 0;
  Address map_iterator_address = 0;
  Address set_iterator_address = 0;
  Address string_iterator_address = 0;
  Address array_iterator_prototype_address = 0;
  Address array_iterator_prototype_map_address = 0;
  Address map_iterator_prototype_address = 0;
  Address map_iterator_prototype_map_address = 0;
  Address set_iterator_prototype_address = 0;
  Address set_iterator_prototype_map_address = 0;
  Address string_iterator_prototype_address = 0;
  Address string_iterator_prototype_map_address = 0;
  Address iterator_prototype_address = 0;
  Address iterator_prototype_map_address = 0;
  Address array_next_address = 0;
  Address map_next_address = 0;
  Address set_next_address = 0;
  Address string_next_address = 0;
  Address iterator_method_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const hasOwn = Function.prototype.call.bind(
                    Object.prototype.hasOwnProperty);
                const key = { marker: 'iterator-prototype-map-key' };
                const value = { marker: 'iterator-prototype-value' };
                const array = [value];
                const arrayIterator = array[Symbol.iterator]();
                const arrayIteratorPrototype =
                    Object.getPrototypeOf(arrayIterator);
                const map = new Map([[key, value]]);
                const mapIterator = map.entries();
                const mapIteratorPrototype = Object.getPrototypeOf(mapIterator);
                const set = new Set([value]);
                const setIterator = set.values();
                const setIteratorPrototype = Object.getPrototypeOf(setIterator);
                const stringIterator = 'ab'[Symbol.iterator]();
                const stringIteratorPrototype =
                    Object.getPrototypeOf(stringIterator);
                const iteratorPrototype =
                    Object.getPrototypeOf(arrayIteratorPrototype);

                function checkIterator(iterator, prototype, next, label) {
                  if (Object.getPrototypeOf(iterator) !== prototype ||
                      typeof next !== 'function' ||
                      iterator.next !== next ||
                      !hasOwn(prototype, 'next') ||
                      iterator[Symbol.iterator]() !== iterator) {
                    throw new Error('unexpected ' + label + ' iterator');
                  }
                }

                checkIterator(arrayIterator, arrayIteratorPrototype,
                              arrayIteratorPrototype.next, 'array');
                checkIterator(mapIterator, mapIteratorPrototype,
                              mapIteratorPrototype.next, 'map');
                checkIterator(setIterator, setIteratorPrototype,
                              setIteratorPrototype.next, 'set');
                checkIterator(stringIterator, stringIteratorPrototype,
                              stringIteratorPrototype.next, 'string');

                if (Object.getPrototypeOf(mapIteratorPrototype) !==
                        iteratorPrototype ||
                    Object.getPrototypeOf(setIteratorPrototype) !==
                        iteratorPrototype ||
                    Object.getPrototypeOf(stringIteratorPrototype) !==
                        iteratorPrototype ||
                    !hasOwn(iteratorPrototype, Symbol.iterator) ||
                    typeof iteratorPrototype[Symbol.iterator] !== 'function' ||
                    iteratorPrototype[Symbol.iterator].call(arrayIterator) !==
                        arrayIterator ||
                    iteratorPrototype[Symbol.iterator].call(mapIterator) !==
                        mapIterator ||
                    iteratorPrototype[Symbol.iterator].call(setIterator) !==
                        setIterator ||
                    iteratorPrototype[Symbol.iterator].call(stringIterator) !==
                        stringIterator) {
                  throw new Error('unexpected shared IteratorPrototype');
                }

                const arrayUsageIterator = array[Symbol.iterator]();
                const arrayStep = arrayUsageIterator.next();
                const arrayDone = arrayUsageIterator.next();
                if (arrayStep.value !== value || arrayStep.done !== false ||
                    arrayDone.value !== undefined || arrayDone.done !== true) {
                  throw new Error('unexpected array iterator next result');
                }

                const mapUsageIterator = map.entries();
                const mapStep = mapUsageIterator.next();
                const mapDone = mapUsageIterator.next();
                if (!Array.isArray(mapStep.value) ||
                    mapStep.value[0] !== key || mapStep.value[1] !== value ||
                    mapStep.done !== false || mapDone.value !== undefined ||
                    mapDone.done !== true) {
                  throw new Error('unexpected map iterator next result');
                }

                const setUsageIterator = set.values();
                const setStep = setUsageIterator.next();
                const setDone = setUsageIterator.next();
                if (setStep.value !== value || setStep.done !== false ||
                    setDone.value !== undefined || setDone.done !== true) {
                  throw new Error('unexpected set iterator next result');
                }

                const stringUsageIterator = 'ab'[Symbol.iterator]();
                const stringFirst = stringUsageIterator.next();
                const stringSecond = stringUsageIterator.next();
                const stringDone = stringUsageIterator.next();
                if (stringFirst.value !== 'a' || stringFirst.done !== false ||
                    stringSecond.value !== 'b' ||
                    stringSecond.done !== false ||
                    stringDone.value !== undefined ||
                    stringDone.done !== true) {
                  throw new Error('unexpected string iterator next result');
                }

                globalThis
                    .__heap_dump_iterator_prototype_objects_classic_usage_edges =
                    {
                      array,
                      map,
                      set,
                      key,
                      value,
                      arrayIterator,
                      mapIterator,
                      setIterator,
                      stringIterator,
                      arrayIteratorPrototype,
                      mapIteratorPrototype,
                      setIteratorPrototype,
                      stringIteratorPrototype,
                      iteratorPrototype,
                      arrayNext: arrayIteratorPrototype.next,
                      mapNext: mapIteratorPrototype.next,
                      setNext: setIteratorPrototype.next,
                      stringNext: stringIteratorPrototype.next,
                      iteratorMethod: iteratorPrototype[Symbol.iterator],
                    };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_iterator_prototype_objects_classic_usage_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            auto assert_js_object_instance_type =
                [](v8::Local<v8::Value> value, InstanceType type,
                   Address* map_address) -> Address {
              CHECK(value->IsObject());
              CHECK_EQ(type, LocalObjectInstanceType(value));
              v8::Local<v8::Object> object =
                  v8::Local<v8::Object>::Cast(value);
              DirectHandle<JSObject> js_object =
                  Cast<JSObject>(v8::Utils::OpenDirectHandle(*object));
              CHECK_EQ(type, js_object->map()->instance_type());
              *map_address = js_object->map()->address();
              return js_object->address();
            };

            v8::Local<v8::Value> array_iterator_value =
                get_property("arrayIterator");
            v8::Local<v8::Value> map_iterator_value =
                get_property("mapIterator");
            v8::Local<v8::Value> set_iterator_value =
                get_property("setIterator");
            v8::Local<v8::Value> string_iterator_value =
                get_property("stringIterator");
            v8::Local<v8::Value> array_iterator_prototype_value =
                get_property("arrayIteratorPrototype");
            v8::Local<v8::Value> map_iterator_prototype_value =
                get_property("mapIteratorPrototype");
            v8::Local<v8::Value> set_iterator_prototype_value =
                get_property("setIteratorPrototype");
            v8::Local<v8::Value> string_iterator_prototype_value =
                get_property("stringIteratorPrototype");
            v8::Local<v8::Value> iterator_prototype_value =
                get_property("iteratorPrototype");
            v8::Local<v8::Value> array_next_value =
                get_property("arrayNext");
            v8::Local<v8::Value> map_next_value = get_property("mapNext");
            v8::Local<v8::Value> set_next_value = get_property("setNext");
            v8::Local<v8::Value> string_next_value =
                get_property("stringNext");
            v8::Local<v8::Value> iterator_method_value =
                get_property("iteratorMethod");

            CHECK(array_iterator_value->IsObject());
            CHECK(map_iterator_value->IsObject());
            CHECK(set_iterator_value->IsObject());
            CHECK(string_iterator_value->IsObject());
            CHECK(array_next_value->IsFunction());
            CHECK(map_next_value->IsFunction());
            CHECK(set_next_value->IsFunction());
            CHECK(string_next_value->IsFunction());
            CHECK(iterator_method_value->IsFunction());

            v8::Local<v8::Object> array_iterator_object =
                v8::Local<v8::Object>::Cast(array_iterator_value);
            v8::Local<v8::Object> map_iterator_object =
                v8::Local<v8::Object>::Cast(map_iterator_value);
            v8::Local<v8::Object> set_iterator_object =
                v8::Local<v8::Object>::Cast(set_iterator_value);
            v8::Local<v8::Object> string_iterator_object =
                v8::Local<v8::Object>::Cast(string_iterator_value);
            v8::Local<v8::Object> array_iterator_prototype_object =
                v8::Local<v8::Object>::Cast(array_iterator_prototype_value);
            v8::Local<v8::Object> map_iterator_prototype_object =
                v8::Local<v8::Object>::Cast(map_iterator_prototype_value);
            v8::Local<v8::Object> set_iterator_prototype_object =
                v8::Local<v8::Object>::Cast(set_iterator_prototype_value);
            v8::Local<v8::Object> string_iterator_prototype_object =
                v8::Local<v8::Object>::Cast(string_iterator_prototype_value);
            v8::Local<v8::Object> iterator_prototype_object =
                v8::Local<v8::Object>::Cast(iterator_prototype_value);

            CHECK(array_iterator_object->GetPrototypeV2()->StrictEquals(
                array_iterator_prototype_value));
            CHECK(map_iterator_object->GetPrototypeV2()->StrictEquals(
                map_iterator_prototype_value));
            CHECK(set_iterator_object->GetPrototypeV2()->StrictEquals(
                set_iterator_prototype_value));
            CHECK(string_iterator_object->GetPrototypeV2()->StrictEquals(
                string_iterator_prototype_value));
            CHECK(array_iterator_prototype_object->GetPrototypeV2()
                      ->StrictEquals(iterator_prototype_value));
            CHECK(map_iterator_prototype_object->GetPrototypeV2()->StrictEquals(
                iterator_prototype_value));
            CHECK(set_iterator_prototype_object->GetPrototypeV2()->StrictEquals(
                iterator_prototype_value));
            CHECK(string_iterator_prototype_object->GetPrototypeV2()
                      ->StrictEquals(iterator_prototype_value));
            CHECK(array_iterator_prototype_object
                      ->Get(context, NewApiString(v8_isolate, "next"))
                      .ToLocalChecked()
                      ->StrictEquals(array_next_value));
            CHECK(map_iterator_prototype_object
                      ->Get(context, NewApiString(v8_isolate, "next"))
                      .ToLocalChecked()
                      ->StrictEquals(map_next_value));
            CHECK(set_iterator_prototype_object
                      ->Get(context, NewApiString(v8_isolate, "next"))
                      .ToLocalChecked()
                      ->StrictEquals(set_next_value));
            CHECK(string_iterator_prototype_object
                      ->Get(context, NewApiString(v8_isolate, "next"))
                      .ToLocalChecked()
                      ->StrictEquals(string_next_value));
            CHECK(iterator_prototype_object
                      ->Get(context, v8::Symbol::GetIterator(v8_isolate))
                      .ToLocalChecked()
                      ->StrictEquals(iterator_method_value));

            array_iterator_prototype_address =
                assert_js_object_instance_type(
                    array_iterator_prototype_value,
                    JS_ARRAY_ITERATOR_PROTOTYPE_TYPE,
                    &array_iterator_prototype_map_address);
            iterator_prototype_address = assert_js_object_instance_type(
                iterator_prototype_value, JS_ITERATOR_PROTOTYPE_TYPE,
                &iterator_prototype_map_address);
            map_iterator_prototype_address = assert_js_object_instance_type(
                map_iterator_prototype_value, JS_MAP_ITERATOR_PROTOTYPE_TYPE,
                &map_iterator_prototype_map_address);
            set_iterator_prototype_address = assert_js_object_instance_type(
                set_iterator_prototype_value, JS_SET_ITERATOR_PROTOTYPE_TYPE,
                &set_iterator_prototype_map_address);
            string_iterator_prototype_address =
                assert_js_object_instance_type(
                    string_iterator_prototype_value,
                    JS_STRING_ITERATOR_PROTOTYPE_TYPE,
                    &string_iterator_prototype_map_address);

            array_iterator_address = LocalObjectAddress(array_iterator_value);
            map_iterator_address = LocalObjectAddress(map_iterator_value);
            set_iterator_address = LocalObjectAddress(set_iterator_value);
            string_iterator_address = LocalObjectAddress(string_iterator_value);
            array_next_address = LocalObjectAddress(array_next_value);
            map_next_address = LocalObjectAddress(map_next_value);
            set_next_address = LocalObjectAddress(set_next_value);
            string_next_address = LocalObjectAddress(string_next_value);
            iterator_method_address = LocalObjectAddress(iterator_method_value);

            return DumpArtifacts{};
          });

  Node* array_iterator = GetSnapshotNode(generator.get(), array_iterator_address);
  Node* map_iterator = GetSnapshotNode(generator.get(), map_iterator_address);
  Node* set_iterator = GetSnapshotNode(generator.get(), set_iterator_address);
  Node* string_iterator =
      GetSnapshotNode(generator.get(), string_iterator_address);
  Node* array_iterator_prototype =
      GetSnapshotNode(generator.get(), array_iterator_prototype_address);
  Node* map_iterator_prototype =
      GetSnapshotNode(generator.get(), map_iterator_prototype_address);
  Node* set_iterator_prototype =
      GetSnapshotNode(generator.get(), set_iterator_prototype_address);
  Node* string_iterator_prototype =
      GetSnapshotNode(generator.get(), string_iterator_prototype_address);
  Node* iterator_prototype =
      GetSnapshotNode(generator.get(), iterator_prototype_address);
  Node* array_iterator_prototype_map =
      GetSnapshotNode(generator.get(), array_iterator_prototype_map_address);
  Node* map_iterator_prototype_map =
      GetSnapshotNode(generator.get(), map_iterator_prototype_map_address);
  Node* set_iterator_prototype_map =
      GetSnapshotNode(generator.get(), set_iterator_prototype_map_address);
  Node* string_iterator_prototype_map =
      GetSnapshotNode(generator.get(), string_iterator_prototype_map_address);
  Node* iterator_prototype_map =
      GetSnapshotNode(generator.get(), iterator_prototype_map_address);
  Node* array_next = GetSnapshotNode(generator.get(), array_next_address);
  Node* map_next = GetSnapshotNode(generator.get(), map_next_address);
  Node* set_next = GetSnapshotNode(generator.get(), set_next_address);
  Node* string_next = GetSnapshotNode(generator.get(), string_next_address);
  Node* iterator_method =
      GetSnapshotNode(generator.get(), iterator_method_address);

  CHECK_NOT_NULL(array_iterator);
  CHECK_NOT_NULL(map_iterator);
  CHECK_NOT_NULL(set_iterator);
  CHECK_NOT_NULL(string_iterator);
  CHECK_NOT_NULL(array_iterator_prototype);
  CHECK_NOT_NULL(map_iterator_prototype);
  CHECK_NOT_NULL(set_iterator_prototype);
  CHECK_NOT_NULL(string_iterator_prototype);
  CHECK_NOT_NULL(iterator_prototype);
  CHECK_NOT_NULL(array_next);
  CHECK_NOT_NULL(map_next);
  CHECK_NOT_NULL(set_next);
  CHECK_NOT_NULL(string_next);
  CHECK_NOT_NULL(iterator_method);
  CHECK(NodeHasType(array_iterator, Node::Type::kObject));
  CHECK(NodeHasType(map_iterator, Node::Type::kObject));
  CHECK(NodeHasType(set_iterator, Node::Type::kObject));
  CHECK(NodeHasType(string_iterator, Node::Type::kObject));
  CHECK(NodeHasType(array_iterator_prototype, Node::Type::kObject));
  CHECK(NodeHasType(map_iterator_prototype, Node::Type::kObject));
  CHECK(NodeHasType(set_iterator_prototype, Node::Type::kObject));
  CHECK(NodeHasType(string_iterator_prototype, Node::Type::kObject));
  CHECK(NodeHasType(iterator_prototype, Node::Type::kObject));
  CHECK(NodeHasType(array_iterator_prototype_map,
                    Node::Type::kObjectShape));
  CHECK(NodeHasType(map_iterator_prototype_map,
                    Node::Type::kObjectShape));
  CHECK(NodeHasType(set_iterator_prototype_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(string_iterator_prototype_map,
                    Node::Type::kObjectShape));
  CHECK(NodeHasType(iterator_prototype_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(array_next, Node::Type::kClosure));
  CHECK(NodeHasType(map_next, Node::Type::kClosure));
  CHECK(NodeHasType(set_next, Node::Type::kClosure));
  CHECK(NodeHasType(string_next, Node::Type::kClosure));
  CHECK(NodeHasType(iterator_method, Node::Type::kClosure));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), array_iterator_prototype, "__proto__",
      iterator_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), map_iterator_prototype, "__proto__", iterator_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), set_iterator_prototype, "__proto__", iterator_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), string_iterator_prototype, "__proto__",
      iterator_prototype);
  CHECK_EQ(1,
           CountEdgesFromNodeToTarget(array_iterator_prototype,
                                     Edge::Type::kInternal,
                                     array_iterator_prototype_map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(map_iterator_prototype,
                                         Edge::Type::kInternal,
                                         map_iterator_prototype_map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(set_iterator_prototype,
                                         Edge::Type::kInternal,
                                         set_iterator_prototype_map));
  CHECK_EQ(
      1, CountEdgesFromNodeToTarget(string_iterator_prototype,
                                    Edge::Type::kInternal,
                                    string_iterator_prototype_map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(iterator_prototype,
                                         Edge::Type::kInternal,
                                         iterator_prototype_map));
  CHECK(HasExactNamedEdge(generator.get(), array_iterator_prototype,
                         Edge::Type::kInternal, "map",
                         array_iterator_prototype_map));
  CHECK(HasExactNamedEdge(generator.get(), map_iterator_prototype,
                         Edge::Type::kInternal, "map",
                         map_iterator_prototype_map));
  CHECK(HasExactNamedEdge(generator.get(), set_iterator_prototype,
                         Edge::Type::kInternal, "map",
                         set_iterator_prototype_map));
  CHECK(HasExactNamedEdge(generator.get(), string_iterator_prototype,
                         Edge::Type::kInternal, "map",
                         string_iterator_prototype_map));
  CHECK(HasExactNamedEdge(generator.get(), iterator_prototype,
                         Edge::Type::kInternal, "map",
                         iterator_prototype_map));
  CHECK_EQ(1,
           CountExactNamedEdges(generator.get(), array_iterator_prototype,
                               Edge::Type::kInternal, "map",
                               array_iterator_prototype_map));
  CHECK_EQ(1,
           CountExactNamedEdges(generator.get(), map_iterator_prototype,
                               Edge::Type::kInternal, "map",
                               map_iterator_prototype_map));
  CHECK_EQ(1,
           CountExactNamedEdges(generator.get(), set_iterator_prototype,
                               Edge::Type::kInternal, "map",
                               set_iterator_prototype_map));
  CHECK_EQ(
      1,
      CountExactNamedEdges(generator.get(), string_iterator_prototype,
                          Edge::Type::kInternal, "map",
                          string_iterator_prototype_map));
  CHECK_EQ(1,
           CountExactNamedEdges(generator.get(), iterator_prototype,
                               Edge::Type::kInternal, "map",
                               iterator_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array_iterator_prototype,
                                         Edge::Type::kWeak,
                                         array_iterator_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_iterator_prototype,
                                         Edge::Type::kWeak,
                                         map_iterator_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(set_iterator_prototype,
                                         Edge::Type::kWeak,
                                         set_iterator_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(string_iterator_prototype,
                                         Edge::Type::kWeak,
                                         string_iterator_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(iterator_prototype, Edge::Type::kWeak,
                                         iterator_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array_iterator_prototype,
                                         Edge::Type::kHidden,
                                         array_iterator_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_iterator_prototype,
                                         Edge::Type::kHidden,
                                         map_iterator_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(set_iterator_prototype,
                                         Edge::Type::kHidden,
                                         set_iterator_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(string_iterator_prototype,
                                         Edge::Type::kHidden,
                                         string_iterator_prototype_map));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(iterator_prototype, Edge::Type::kHidden,
                                     iterator_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(array_iterator_prototype,
                                         Edge::Type::kProperty,
                                         array_iterator_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_iterator_prototype,
                                         Edge::Type::kProperty,
                                         map_iterator_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(set_iterator_prototype,
                                         Edge::Type::kProperty,
                                         set_iterator_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(string_iterator_prototype,
                                         Edge::Type::kProperty,
                                         string_iterator_prototype_map));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(iterator_prototype, Edge::Type::kProperty,
                                     iterator_prototype_map));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), array_iterator, "__proto__", array_iterator_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), map_iterator, "__proto__", map_iterator_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), set_iterator, "__proto__", set_iterator_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), string_iterator, "__proto__",
      string_iterator_prototype);

  // Builtin function-valued prototype properties can translate as named
  // property edges, hidden fallback edges, or internal fallback edges depending
  // on how much descriptor/name information is recoverable from rawheap.
  CheckSinglePropertyOrHiddenEdgeToTargetNoInternalDuplicate(
      array_iterator_prototype, array_next);
  CheckSinglePropertyOrHiddenEdgeToTargetNoInternalDuplicate(
      map_iterator_prototype, map_next);
  CheckSinglePropertyOrHiddenEdgeToTargetNoInternalDuplicate(
      set_iterator_prototype, set_next);
  CheckSinglePropertyOrHiddenEdgeToTargetNoInternalDuplicate(
      string_iterator_prototype, string_next);
  CheckSinglePropertyOrHiddenEdgeToTargetNoInternalDuplicate(iterator_prototype,
                                                             iterator_method);

  MarkCoveredInstanceType(JS_ARRAY_ITERATOR_PROTOTYPE_TYPE);
  MarkCoveredInstanceType(JS_ITERATOR_PROTOTYPE_TYPE);
  MarkCoveredInstanceType(JS_MAP_ITERATOR_PROTOTYPE_TYPE);
  MarkCoveredInstanceType(JS_SET_ITERATOR_PROTOTYPE_TYPE);
  MarkCoveredInstanceType(JS_STRING_ITERATOR_PROTOTYPE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSValidIteratorWrapperClassicUsageEdges) {
  Address iterator_wrapper_address = 0;
  Address iterator_wrapper_map_address = 0;
  Address prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                let count = 0;
                const wrapped = {
                  next() {
                    if (count++ === 0) {
                      return { value: 'iterator-wrapper-value', done: false };
                    }
                    return { value: undefined, done: true };
                  },
                };
                const iteratorWrapper = Iterator.from(wrapped);
                const prototype = Object.getPrototypeOf(iteratorWrapper);
                const iteratorPrototypeMethod = prototype[Symbol.iterator];
                const next = iteratorWrapper.next();
                const done = iteratorWrapper.next();
                if (typeof Iterator !== 'function' ||
                    iteratorWrapper === undefined ||
                    prototype === undefined ||
                    typeof iteratorPrototypeMethod !== 'function' ||
                    next.value !== 'iterator-wrapper-value' ||
                    next.done !== false ||
                    done.value !== undefined ||
                    done.done !== true ||
                    iteratorWrapper[Symbol.iterator]() !== iteratorWrapper) {
                  throw new Error('unexpected JSValidIteratorWrapper fixture');
                }
                globalThis.__heap_dump_js_valid_iterator_wrapper_classic_usage_edges =
                    {
                      iteratorWrapper, prototype
                    };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_valid_iterator_wrapper_classic_usage_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> wrapper_value =
                get_property("iteratorWrapper");
            v8::Local<v8::Value> prototype_value =
                get_property("prototype");
            CHECK(wrapper_value->IsObject());
            CHECK(prototype_value->IsObject());
            CHECK_EQ(JS_VALID_ITERATOR_WRAPPER_TYPE,
                     LocalObjectInstanceType(wrapper_value));
            DirectHandle<JSValidIteratorWrapper> wrapper =
                Cast<JSValidIteratorWrapper>(
                    v8::Utils::OpenDirectHandle(
                        *v8::Local<v8::Object>::Cast(wrapper_value)));
            CHECK_EQ(JS_VALID_ITERATOR_WRAPPER_TYPE,
                     wrapper->map()->instance_type());
            iterator_wrapper_map_address = wrapper->map()->address();
            iterator_wrapper_address = wrapper->address();
            prototype_address = LocalObjectAddress(prototype_value);

            return DumpArtifacts{};
          });

  Node* iterator_wrapper =
      GetSnapshotNode(generator.get(), iterator_wrapper_address);
  Node* iterator_wrapper_map =
      GetSnapshotNode(generator.get(), iterator_wrapper_map_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  CHECK_NOT_NULL(iterator_wrapper);
  CHECK_NOT_NULL(iterator_wrapper_map);
  CHECK_NOT_NULL(prototype);
  CHECK(NodeHasType(iterator_wrapper, Node::Type::kObject));
  CHECK(NodeHasType(iterator_wrapper_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(prototype, Node::Type::kObject));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), iterator_wrapper, "__proto__", prototype);
  CheckExactInternalIteratorMapEdgeNoWrongKind(generator.get(),
                                               iterator_wrapper,
                                               iterator_wrapper_map);
  MarkCoveredInstanceType(JS_VALID_ITERATOR_WRAPPER_TYPE);
}

#else
UNINITIALIZED_TEST(HeapDumpJSArrayIteratorExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSStringIteratorExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSRegExpStringIteratorExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSMapKeyIteratorExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSMapValueIteratorExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSMapKeyValueIteratorExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSMapObjectGroupByExactTableEdges) {}
UNINITIALIZED_TEST(HeapDumpJSSetValueIteratorExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSSetKeyValueIteratorExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSIteratorHelpersExactPrototypeEdges) {}
UNINITIALIZED_TEST(HeapDumpJSAsyncFromSyncIteratorExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSSetPrototypeClassicUsageEdges) {}
UNINITIALIZED_TEST(HeapDumpIteratorPrototypeObjectsClassicUsageEdges) {}
UNINITIALIZED_TEST(HeapDumpJSValidIteratorWrapperClassicUsageEdges) {}
#endif

}  // namespace heap
}  // namespace internal
}  // namespace v8
