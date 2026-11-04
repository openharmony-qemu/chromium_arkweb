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

UNINITIALIZED_TEST(HeapDumpJSArrayBufferExactPrototypeEdge) {
  Address array_buffer_address = 0;
  Address backing_store_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const arrayBuffer = new ArrayBuffer(16);
                if (arrayBuffer.byteLength !== 16 ||
                    !(arrayBuffer instanceof ArrayBuffer)) {
                  throw new Error('unexpected ArrayBuffer fixture');
                }
                globalThis.__heap_dump_js_array_buffer_exact_prototype_edge = {
                  arrayBuffer
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
                              "__heap_dump_js_array_buffer_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> array_buffer_value =
                get_property("arrayBuffer");
            CHECK_EQ(JS_ARRAY_BUFFER_TYPE,
                     LocalObjectInstanceType(array_buffer_value));
            v8::Local<v8::ArrayBuffer> array_buffer =
                v8::Local<v8::ArrayBuffer>::Cast(array_buffer_value);
            CHECK_EQ(16, static_cast<int>(array_buffer->ByteLength()));
            DirectHandle<JSArrayBuffer> js_array_buffer =
                v8::Utils::OpenDirectHandle(*array_buffer);
            CHECK_EQ(JS_ARRAY_BUFFER_TYPE,
                     js_array_buffer->map()->instance_type());
            map_address = js_array_buffer->map()->address();
            CHECK_EQ(16, static_cast<int>(js_array_buffer->byte_length()));
            void* backing_store = js_array_buffer->backing_store();
            CHECK_NOT_NULL(backing_store);
            array_buffer_address = js_array_buffer->address();
            backing_store_address =
                reinterpret_cast<Address>(backing_store);

            return DumpArtifacts{};
          });

  Node* array_buffer = GetSnapshotNode(generator.get(), array_buffer_address);
  Node* backing_store = GetSnapshotNode(generator.get(), backing_store_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(array_buffer);
  CHECK_NOT_NULL(backing_store);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(array_buffer, Node::Type::kObject));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CHECK(NodeHasTypeAndName(generator.get(), backing_store, Node::Type::kNative,
                           "system / JSArrayBufferData"));
  CHECK_EQ(16u, backing_store->self_size_);

  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), array_buffer, "backing_store", backing_store);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), array_buffer, "map", map);
  MarkCoveredInstanceType(JS_ARRAY_BUFFER_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSTypedArrayExactBufferEdge) {
  Address typed_array_address = 0;
  Address array_buffer_address = 0;
  Address typed_array_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const typedArray = new Uint8Array(16);
                typedArray[0] = 42;
                const arrayBuffer = typedArray.buffer;
                if (Object.getPrototypeOf(typedArray) !== Uint8Array.prototype ||
                    typedArray.length !== 16 ||
                    typedArray.byteLength !== 16 ||
                    typedArray.byteOffset !== 0 ||
                    typedArray[0] !== 42 ||
                    arrayBuffer.byteLength !== 16 ||
                    !(typedArray instanceof Uint8Array)) {
                  throw new Error('unexpected TypedArray fixture');
                }
                globalThis.__heap_dump_js_typed_array_exact_buffer_edge = {
                  typedArray, arrayBuffer
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
                              "__heap_dump_js_typed_array_exact_buffer_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> typed_array_value =
                get_property("typedArray");
            v8::Local<v8::Value> array_buffer_value =
                get_property("arrayBuffer");
            CHECK_EQ(JS_TYPED_ARRAY_TYPE,
                     LocalObjectInstanceType(typed_array_value));
            v8::Local<v8::TypedArray> typed_array =
                v8::Local<v8::TypedArray>::Cast(typed_array_value);
            CHECK_EQ(16, static_cast<int>(typed_array->Length()));
            CHECK_EQ(16, static_cast<int>(typed_array->ByteLength()));
            CHECK_EQ(0, static_cast<int>(typed_array->ByteOffset()));
            CHECK(typed_array->Buffer()
                      ->Equals(context, array_buffer_value)
                      .FromJust());
            DirectHandle<JSTypedArray> js_typed_array =
                v8::Utils::OpenDirectHandle(*typed_array);
            CHECK_EQ(JS_TYPED_ARRAY_TYPE,
                     js_typed_array->map()->instance_type());
            CHECK_EQ(16, static_cast<int>(js_typed_array->GetLength()));
            CHECK_EQ(16, static_cast<int>(js_typed_array->GetByteLength()));
            CHECK_EQ(0, static_cast<int>(js_typed_array->byte_offset()));
            DirectHandle<JSArrayBuffer> js_array_buffer =
                v8::Utils::OpenDirectHandle(*typed_array->Buffer());
            CHECK_EQ(JS_ARRAY_BUFFER_TYPE,
                     js_array_buffer->map()->instance_type());
            CHECK_EQ(16, static_cast<int>(js_array_buffer->byte_length()));
            CHECK_EQ(js_array_buffer->address(),
                     LocalObjectAddress(array_buffer_value));
            typed_array_map_address = js_typed_array->map()->address();
            typed_array_address = js_typed_array->address();
            array_buffer_address = js_array_buffer->address();

            return DumpArtifacts{};
          });

  Node* typed_array = GetSnapshotNode(generator.get(), typed_array_address);
  Node* array_buffer = GetSnapshotNode(generator.get(), array_buffer_address);
  Node* typed_array_map = GetSnapshotNode(generator.get(), typed_array_map_address);
  CHECK_NOT_NULL(typed_array);
  CHECK_NOT_NULL(array_buffer);
  CHECK_NOT_NULL(typed_array_map);
  CHECK(NodeHasType(typed_array, Node::Type::kObject));
  CHECK(NodeHasType(array_buffer, Node::Type::kObject));
  CHECK(NodeHasType(typed_array_map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), typed_array, Edge::Type::kInternal,
                          "buffer", array_buffer));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), typed_array,
                                   Edge::Type::kInternal, "buffer",
                                   array_buffer));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kInternal,
                                         array_buffer));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kHidden,
                                         array_buffer));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kProperty,
                                         array_buffer));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kWeak,
                                         array_buffer));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kShortcut,
                                         array_buffer));
  CHECK(HasExactNamedEdge(generator.get(), typed_array, Edge::Type::kInternal,
                          "map", typed_array_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), typed_array,
                                   Edge::Type::kInternal, "map",
                                   typed_array_map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kInternal,
                                         typed_array_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kWeak,
                                         typed_array_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kHidden,
                                         typed_array_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kProperty,
                                         typed_array_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kShortcut,
                                         typed_array_map));
  MarkCoveredInstanceType(JS_TYPED_ARRAY_TYPE);
  MarkCoveredInstanceType(JS_ARRAY_BUFFER_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSTypedArrayBufferBackingStoreEdgeParity) {
  Address typed_array_address = 0;
  Address array_buffer_address = 0;
  Address typed_array_map_address = 0;
  Address array_buffer_map_address = 0;
  Address backing_store_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const typedArray = new Uint8Array(32);
                typedArray[7] = 99;
                const arrayBuffer = typedArray.buffer;
                if (!(typedArray instanceof Uint8Array) ||
                    typedArray.byteOffset !== 0 ||
                    typedArray.byteLength !== 32 ||
                    typedArray.length !== 32 ||
                    typedArray[7] !== 99 ||
                    arrayBuffer.byteLength !== 32 ||
                    typedArray.buffer !== arrayBuffer) {
                  throw new Error(
                      'unexpected TypedArray backing-store fixture');
                }
                globalThis
                    .__heap_dump_js_typed_array_buffer_backing_store_edge = {
                  typedArray, arrayBuffer
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
                              "__heap_dump_js_typed_array_buffer_backing_store_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> typed_array_value =
                get_property("typedArray");
            v8::Local<v8::Value> array_buffer_value =
                get_property("arrayBuffer");
            CHECK_EQ(JS_TYPED_ARRAY_TYPE,
                     LocalObjectInstanceType(typed_array_value));
            CHECK_EQ(JS_ARRAY_BUFFER_TYPE,
                     LocalObjectInstanceType(array_buffer_value));
            v8::Local<v8::TypedArray> typed_array =
                v8::Local<v8::TypedArray>::Cast(typed_array_value);
            CHECK_EQ(32, static_cast<int>(typed_array->ByteLength()));
            CHECK(typed_array->Buffer()
                      ->Equals(context, array_buffer_value)
                      .FromJust());
            DirectHandle<JSTypedArray> js_typed_array =
                v8::Utils::OpenDirectHandle(*typed_array);
            CHECK_EQ(JS_TYPED_ARRAY_TYPE,
                     js_typed_array->map()->instance_type());
            typed_array_map_address = js_typed_array->map()->address();
            DirectHandle<JSArrayBuffer> js_array_buffer =
                v8::Utils::OpenDirectHandle(
                    *v8::Local<v8::ArrayBuffer>::Cast(array_buffer_value));
            CHECK_EQ(JS_ARRAY_BUFFER_TYPE,
                     js_array_buffer->map()->instance_type());
            array_buffer_map_address = js_array_buffer->map()->address();
            CHECK_EQ(32, static_cast<int>(js_array_buffer->byte_length()));
            void* backing_store = js_array_buffer->backing_store();
            CHECK_NOT_NULL(backing_store);

            typed_array_address = js_typed_array->address();
            array_buffer_address = js_array_buffer->address();
            backing_store_address = reinterpret_cast<Address>(backing_store);

            return DumpArtifacts{};
          });

  Node* typed_array = GetSnapshotNode(generator.get(), typed_array_address);
  Node* array_buffer = GetSnapshotNode(generator.get(), array_buffer_address);
  Node* backing_store = GetSnapshotNode(generator.get(), backing_store_address);
  Node* typed_array_map = GetSnapshotNode(generator.get(),
                                          typed_array_map_address);
  Node* array_buffer_map = GetSnapshotNode(generator.get(),
                                           array_buffer_map_address);
  CHECK_NOT_NULL(typed_array);
  CHECK_NOT_NULL(array_buffer);
  CHECK_NOT_NULL(backing_store);
  CHECK_NOT_NULL(typed_array_map);
  CHECK_NOT_NULL(array_buffer_map);
  CHECK(NodeHasType(typed_array, Node::Type::kObject));
  CHECK(NodeHasType(array_buffer, Node::Type::kObject));
  CHECK(NodeHasTypeAndName(generator.get(), backing_store, Node::Type::kNative,
                           "system / JSArrayBufferData"));
  CHECK(NodeHasType(typed_array_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(array_buffer_map, Node::Type::kObjectShape));
  CHECK_EQ(32u, backing_store->self_size_);

  CHECK(HasExactNamedEdge(generator.get(), typed_array, Edge::Type::kInternal,
                          "buffer", array_buffer));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), typed_array,
                                   Edge::Type::kInternal, "buffer",
                                   array_buffer));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kInternal,
                                         array_buffer));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kHidden,
                                         array_buffer));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kProperty,
                                         array_buffer));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kWeak,
                                         array_buffer));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kShortcut,
                                         array_buffer));
  CHECK(HasExactNamedEdge(generator.get(), typed_array, Edge::Type::kInternal,
                         "map", typed_array_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), typed_array,
                                   Edge::Type::kInternal, "map",
                                   typed_array_map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kInternal,
                                         typed_array_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kHidden,
                                         typed_array_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kProperty,
                                         typed_array_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kWeak,
                                         typed_array_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array, Edge::Type::kShortcut,
                                         typed_array_map));

  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), array_buffer, "backing_store", backing_store);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), array_buffer, "map", array_buffer_map);
  MarkCoveredInstanceType(JS_TYPED_ARRAY_TYPE);
  MarkCoveredInstanceType(JS_ARRAY_BUFFER_TYPE);
}

UNINITIALIZED_TEST(HeapDumpUint8TypedArrayConstructorExactPrototypeEdges) {
  Address constructor_address = 0;
  Address constructor_map_address = 0;
  Address typed_array_prototype_address = 0;
  Address function_prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const constructor = Uint8Array;
                Object.setPrototypeOf(constructor, Function.prototype);
                const typedArrayPrototype = constructor.prototype;
                const functionPrototype = Object.getPrototypeOf(constructor);
                const sample = new constructor(4);
                sample[0] = constructor.BYTES_PER_ELEMENT;
                if (constructor !== globalThis.Uint8Array ||
                    typeof constructor !== 'function' ||
                    constructor.BYTES_PER_ELEMENT !== 1 ||
                    typedArrayPrototype !== Uint8Array.prototype ||
                    typedArrayPrototype.constructor !== constructor ||
                    functionPrototype !== Function.prototype ||
                    Object.getPrototypeOf(constructor) !==
                        functionPrototype ||
                    sample.length !== 4 ||
                    sample.byteLength !== 4 ||
                    sample[0] !== 1 ||
                    Object.getPrototypeOf(sample) !==
                        typedArrayPrototype ||
                    !(sample instanceof constructor)) {
                  throw new Error(
                      'unexpected Uint8Array constructor fixture');
                }
                globalThis
                    .__heap_dump_uint8_typed_array_constructor_exact_edges = {
                  constructor, typedArrayPrototype, functionPrototype
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
                              "__heap_dump_uint8_typed_array_constructor_exact_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> constructor_value =
                get_property("constructor");
            v8::Local<v8::Value> typed_array_prototype_value =
                get_property("typedArrayPrototype");
            v8::Local<v8::Value> function_prototype_value =
                get_property("functionPrototype");
            CHECK(constructor_value->IsFunction());
            CHECK(typed_array_prototype_value->IsObject());
            CHECK(function_prototype_value->IsFunction());

            v8::Local<v8::Function> constructor =
                v8::Local<v8::Function>::Cast(constructor_value);
            CHECK(constructor
                      ->Get(context, NewApiString(v8_isolate, "prototype"))
                      .ToLocalChecked()
                      ->StrictEquals(typed_array_prototype_value));
            CHECK(constructor->GetPrototypeV2()->StrictEquals(
                function_prototype_value));
            v8::Local<v8::Value> bytes_per_element =
                constructor
                    ->Get(context,
                          NewApiString(v8_isolate, "BYTES_PER_ELEMENT"))
                    .ToLocalChecked();
            CHECK(bytes_per_element->IsUint32());
            CHECK_EQ(1, bytes_per_element->Uint32Value(context).FromJust());
            CHECK_EQ(UINT8_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     LocalObjectInstanceType(constructor_value));

            DirectHandle<JSFunction> js_constructor =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*constructor));
            CHECK_EQ(UINT8_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     js_constructor->map()->instance_type());
            constructor_address = js_constructor->address();
            constructor_map_address = js_constructor->map()->address();
            typed_array_prototype_address =
                LocalObjectAddress(typed_array_prototype_value);
            function_prototype_address =
                LocalObjectAddress(function_prototype_value);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(js_constructor);
            artifacts.keep_alive.push_back(
                LocalJSReceiver(typed_array_prototype_value));
            artifacts.keep_alive.push_back(
                LocalJSReceiver(function_prototype_value));
            return artifacts;
          });

  Node* constructor = GetSnapshotNode(generator.get(), constructor_address);
  Node* constructor_map =
      GetSnapshotNode(generator.get(), constructor_map_address);
  Node* typed_array_prototype =
      GetSnapshotNode(generator.get(), typed_array_prototype_address);
  Node* function_prototype =
      GetSnapshotNode(generator.get(), function_prototype_address);
  CHECK_NOT_NULL(constructor);
  CHECK_NOT_NULL(constructor_map);
  CHECK_NOT_NULL(typed_array_prototype);
  CHECK_NOT_NULL(function_prototype);
  CHECK(NodeHasType(constructor, Node::Type::kClosure));
  CHECK(NodeHasType(constructor_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(typed_array_prototype, Node::Type::kObject));
  CHECK(NodeHasType(function_prototype, Node::Type::kClosure));
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), constructor, "map", constructor_map);

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "prototype", typed_array_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "__proto__", function_prototype);
  MarkCoveredInstanceType(UINT8_TYPED_ARRAY_CONSTRUCTOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpInt8TypedArrayConstructorExactPrototypeEdges) {
  Address constructor_address = 0;
  Address constructor_map_address = 0;
  Address typed_array_prototype_address = 0;
  Address function_prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const constructor = Int8Array;
                Object.setPrototypeOf(constructor, Function.prototype);
                const typedArrayPrototype = constructor.prototype;
                const functionPrototype = Object.getPrototypeOf(constructor);
                const sample = new constructor(4);
                sample[0] = -constructor.BYTES_PER_ELEMENT;
                if (constructor !== globalThis.Int8Array ||
                    typeof constructor !== 'function' ||
                    constructor.BYTES_PER_ELEMENT !== 1 ||
                    typedArrayPrototype !== Int8Array.prototype ||
                    typedArrayPrototype.constructor !== constructor ||
                    functionPrototype !== Function.prototype ||
                    Object.getPrototypeOf(constructor) !==
                        functionPrototype ||
                    sample.length !== 4 ||
                    sample.byteLength !== 4 ||
                    sample[0] !== -1 ||
                    Object.getPrototypeOf(sample) !==
                        typedArrayPrototype ||
                    !(sample instanceof constructor)) {
                  throw new Error(
                      'unexpected Int8Array constructor fixture');
                }
                globalThis
                    .__heap_dump_int8_typed_array_constructor_exact_edges = {
                  constructor, typedArrayPrototype, functionPrototype
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
                              "__heap_dump_int8_typed_array_constructor_exact_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> constructor_value =
                get_property("constructor");
            v8::Local<v8::Value> typed_array_prototype_value =
                get_property("typedArrayPrototype");
            v8::Local<v8::Value> function_prototype_value =
                get_property("functionPrototype");
            CHECK(constructor_value->IsFunction());
            CHECK(typed_array_prototype_value->IsObject());
            CHECK(function_prototype_value->IsFunction());

            v8::Local<v8::Function> constructor =
                v8::Local<v8::Function>::Cast(constructor_value);
            CHECK(constructor
                      ->Get(context, NewApiString(v8_isolate, "prototype"))
                      .ToLocalChecked()
                      ->StrictEquals(typed_array_prototype_value));
            CHECK(constructor->GetPrototypeV2()->StrictEquals(
                function_prototype_value));
            v8::Local<v8::Value> bytes_per_element =
                constructor
                    ->Get(context,
                          NewApiString(v8_isolate, "BYTES_PER_ELEMENT"))
                    .ToLocalChecked();
            CHECK(bytes_per_element->IsUint32());
            CHECK_EQ(1, bytes_per_element->Uint32Value(context).FromJust());
            CHECK_EQ(INT8_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     LocalObjectInstanceType(constructor_value));

            DirectHandle<JSFunction> js_constructor =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*constructor));
            CHECK_EQ(INT8_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     js_constructor->map()->instance_type());
            constructor_address = js_constructor->address();
            constructor_map_address = js_constructor->map()->address();
            typed_array_prototype_address =
                LocalObjectAddress(typed_array_prototype_value);
            function_prototype_address =
                LocalObjectAddress(function_prototype_value);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(js_constructor);
            artifacts.keep_alive.push_back(
                LocalJSReceiver(typed_array_prototype_value));
            artifacts.keep_alive.push_back(
                LocalJSReceiver(function_prototype_value));
            return artifacts;
          });

  Node* constructor = GetSnapshotNode(generator.get(), constructor_address);
  Node* constructor_map =
      GetSnapshotNode(generator.get(), constructor_map_address);
  Node* typed_array_prototype =
      GetSnapshotNode(generator.get(), typed_array_prototype_address);
  Node* function_prototype =
      GetSnapshotNode(generator.get(), function_prototype_address);
  CHECK_NOT_NULL(constructor);
  CHECK_NOT_NULL(constructor_map);
  CHECK_NOT_NULL(typed_array_prototype);
  CHECK_NOT_NULL(function_prototype);
  CHECK(NodeHasType(constructor, Node::Type::kClosure));
  CHECK(NodeHasType(constructor_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(typed_array_prototype, Node::Type::kObject));
  CHECK(NodeHasType(function_prototype, Node::Type::kClosure));
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), constructor, "map", constructor_map);

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "prototype", typed_array_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "__proto__", function_prototype);
  MarkCoveredInstanceType(INT8_TYPED_ARRAY_CONSTRUCTOR_TYPE);
}

UNINITIALIZED_TEST(
    HeapDumpUint8ClampedTypedArrayConstructorExactPrototypeEdges) {
  Address constructor_address = 0;
  Address constructor_map_address = 0;
  Address typed_array_prototype_address = 0;
  Address function_prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const constructor = Uint8ClampedArray;
                Object.setPrototypeOf(constructor, Function.prototype);
                const typedArrayPrototype = constructor.prototype;
                const functionPrototype = Object.getPrototypeOf(constructor);
                const sample = new constructor(4);
                sample[0] = -constructor.BYTES_PER_ELEMENT;
                sample[1] = 255 + constructor.BYTES_PER_ELEMENT;
                if (constructor !== globalThis.Uint8ClampedArray ||
                    typeof constructor !== 'function' ||
                    constructor.BYTES_PER_ELEMENT !== 1 ||
                    typedArrayPrototype !== Uint8ClampedArray.prototype ||
                    typedArrayPrototype.constructor !== constructor ||
                    functionPrototype !== Function.prototype ||
                    Object.getPrototypeOf(constructor) !==
                        functionPrototype ||
                    sample.length !== 4 ||
                    sample.byteLength !== 4 ||
                    sample[0] !== 0 ||
                    sample[1] !== 255 ||
                    Object.getPrototypeOf(sample) !==
                        typedArrayPrototype ||
                    !(sample instanceof constructor)) {
                  throw new Error(
                      'unexpected Uint8ClampedArray constructor fixture');
                }
                globalThis
                    .__heap_dump_uint8_clamped_typed_array_constructor_exact_edges = {
                  constructor, typedArrayPrototype, functionPrototype
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
                              "__heap_dump_uint8_clamped_typed_array_constructor_exact_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> constructor_value =
                get_property("constructor");
            v8::Local<v8::Value> typed_array_prototype_value =
                get_property("typedArrayPrototype");
            v8::Local<v8::Value> function_prototype_value =
                get_property("functionPrototype");
            CHECK(constructor_value->IsFunction());
            CHECK(typed_array_prototype_value->IsObject());
            CHECK(function_prototype_value->IsFunction());

            v8::Local<v8::Function> constructor =
                v8::Local<v8::Function>::Cast(constructor_value);
            CHECK(constructor
                      ->Get(context, NewApiString(v8_isolate, "prototype"))
                      .ToLocalChecked()
                      ->StrictEquals(typed_array_prototype_value));
            CHECK(constructor->GetPrototypeV2()->StrictEquals(
                function_prototype_value));
            v8::Local<v8::Value> bytes_per_element =
                constructor
                    ->Get(context,
                          NewApiString(v8_isolate, "BYTES_PER_ELEMENT"))
                    .ToLocalChecked();
            CHECK(bytes_per_element->IsUint32());
            CHECK_EQ(1, bytes_per_element->Uint32Value(context).FromJust());
            CHECK_EQ(UINT8_CLAMPED_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     LocalObjectInstanceType(constructor_value));

            DirectHandle<JSFunction> js_constructor =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*constructor));
            CHECK_EQ(UINT8_CLAMPED_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     js_constructor->map()->instance_type());
            constructor_address = js_constructor->address();
            constructor_map_address = js_constructor->map()->address();
            typed_array_prototype_address =
                LocalObjectAddress(typed_array_prototype_value);
            function_prototype_address =
                LocalObjectAddress(function_prototype_value);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(js_constructor);
            artifacts.keep_alive.push_back(
                LocalJSReceiver(typed_array_prototype_value));
            artifacts.keep_alive.push_back(
                LocalJSReceiver(function_prototype_value));
            return artifacts;
          });

  Node* constructor = GetSnapshotNode(generator.get(), constructor_address);
  Node* constructor_map =
      GetSnapshotNode(generator.get(), constructor_map_address);
  Node* typed_array_prototype =
      GetSnapshotNode(generator.get(), typed_array_prototype_address);
  Node* function_prototype =
      GetSnapshotNode(generator.get(), function_prototype_address);
  CHECK_NOT_NULL(constructor);
  CHECK_NOT_NULL(constructor_map);
  CHECK_NOT_NULL(typed_array_prototype);
  CHECK_NOT_NULL(function_prototype);
  CHECK(NodeHasType(constructor, Node::Type::kClosure));
  CHECK(NodeHasType(constructor_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(typed_array_prototype, Node::Type::kObject));
  CHECK(NodeHasType(function_prototype, Node::Type::kClosure));
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), constructor, "map", constructor_map);

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "prototype", typed_array_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "__proto__", function_prototype);
  MarkCoveredInstanceType(UINT8_CLAMPED_TYPED_ARRAY_CONSTRUCTOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpUint16TypedArrayConstructorExactPrototypeEdges) {
  Address constructor_address = 0;
  Address constructor_map_address = 0;
  Address typed_array_prototype_address = 0;
  Address typed_array_prototype_map_address = 0;
  Address function_prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const constructor = Uint16Array;
                Object.setPrototypeOf(constructor, Function.prototype);
                const typedArrayPrototype = constructor.prototype;
                const functionPrototype = Object.getPrototypeOf(constructor);
                const sample = new constructor(4);
                sample[0] = constructor.BYTES_PER_ELEMENT;
                sample[1] = 65535;
                if (constructor !== globalThis.Uint16Array ||
                    typeof constructor !== 'function' ||
                    constructor.BYTES_PER_ELEMENT !== 2 ||
                    typedArrayPrototype !== Uint16Array.prototype ||
                    typedArrayPrototype.constructor !== constructor ||
                    functionPrototype !== Function.prototype ||
                    Object.getPrototypeOf(constructor) !==
                        functionPrototype ||
                    sample.length !== 4 ||
                    sample.byteLength !== 8 ||
                    sample[0] !== 2 ||
                    sample[1] !== 65535 ||
                    Object.getPrototypeOf(sample) !==
                        typedArrayPrototype ||
                    !(sample instanceof constructor)) {
                  throw new Error(
                      'unexpected Uint16Array constructor fixture');
                }
                globalThis
                    .__heap_dump_uint16_typed_array_constructor_exact_edges = {
                  constructor, typedArrayPrototype, functionPrototype
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
                              "__heap_dump_uint16_typed_array_constructor_exact_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> constructor_value =
                get_property("constructor");
            v8::Local<v8::Value> typed_array_prototype_value =
                get_property("typedArrayPrototype");
            v8::Local<v8::Value> function_prototype_value =
                get_property("functionPrototype");
            CHECK(constructor_value->IsFunction());
            CHECK(typed_array_prototype_value->IsObject());
            CHECK(function_prototype_value->IsFunction());

            v8::Local<v8::Function> constructor =
                v8::Local<v8::Function>::Cast(constructor_value);
            CHECK(constructor
                      ->Get(context, NewApiString(v8_isolate, "prototype"))
                      .ToLocalChecked()
                      ->StrictEquals(typed_array_prototype_value));
            CHECK(constructor->GetPrototypeV2()->StrictEquals(
                function_prototype_value));
            v8::Local<v8::Value> bytes_per_element =
                constructor
                    ->Get(context,
                          NewApiString(v8_isolate, "BYTES_PER_ELEMENT"))
                    .ToLocalChecked();
            CHECK(bytes_per_element->IsUint32());
            CHECK_EQ(2, bytes_per_element->Uint32Value(context).FromJust());
            CHECK_EQ(UINT16_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     LocalObjectInstanceType(constructor_value));

            DirectHandle<JSFunction> js_constructor =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*constructor));
            CHECK_EQ(UINT16_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     js_constructor->map()->instance_type());
            constructor_address = js_constructor->address();
            constructor_map_address = js_constructor->map()->address();
            typed_array_prototype_address =
                LocalObjectAddress(typed_array_prototype_value);
            typed_array_prototype_map_address =
                Cast<JSReceiver>(v8::Utils::OpenDirectHandle(
                                     *typed_array_prototype_value))
                    ->map()
                    ->address();
            function_prototype_address =
                LocalObjectAddress(function_prototype_value);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(js_constructor);
            artifacts.keep_alive.push_back(
                LocalJSReceiver(typed_array_prototype_value));
            artifacts.keep_alive.push_back(
                LocalJSReceiver(function_prototype_value));
            return artifacts;
          });

  Node* constructor = GetSnapshotNode(generator.get(), constructor_address);
  Node* constructor_map =
      GetSnapshotNode(generator.get(), constructor_map_address);
  Node* typed_array_prototype =
      GetSnapshotNode(generator.get(), typed_array_prototype_address);
  Node* typed_array_prototype_map =
      GetSnapshotNode(generator.get(), typed_array_prototype_map_address);
  Node* function_prototype =
      GetSnapshotNode(generator.get(), function_prototype_address);
  CHECK_NOT_NULL(constructor);
  CHECK_NOT_NULL(constructor_map);
  CHECK_NOT_NULL(typed_array_prototype);
  CHECK_NOT_NULL(typed_array_prototype_map);
  CHECK_NOT_NULL(function_prototype);
  CHECK(NodeHasType(constructor, Node::Type::kClosure));
  CHECK(NodeHasType(constructor_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(typed_array_prototype, Node::Type::kObject));
  CHECK(NodeHasType(typed_array_prototype_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(function_prototype, Node::Type::kClosure));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "prototype", typed_array_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "__proto__", function_prototype);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), constructor, "map", constructor_map);
  CHECK(HasExactNamedEdge(generator.get(), typed_array_prototype,
                          Edge::Type::kInternal, "map",
                          typed_array_prototype_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), typed_array_prototype,
                                   Edge::Type::kInternal, "map",
                                   typed_array_prototype_map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kInternal,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kWeak,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kHidden,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kProperty,
                                         typed_array_prototype_map));
  MarkCoveredInstanceType(UINT16_TYPED_ARRAY_CONSTRUCTOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpInt16TypedArrayConstructorExactPrototypeEdges) {
  Address constructor_address = 0;
  Address constructor_map_address = 0;
  Address typed_array_prototype_address = 0;
  Address typed_array_prototype_map_address = 0;
  Address function_prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const constructor = Int16Array;
                Object.setPrototypeOf(constructor, Function.prototype);
                const typedArrayPrototype = constructor.prototype;
                const functionPrototype = Object.getPrototypeOf(constructor);
                const sample = new constructor(4);
                sample[0] = -constructor.BYTES_PER_ELEMENT;
                sample[1] = 32767;
                sample[2] = 32768;
                if (constructor !== globalThis.Int16Array ||
                    typeof constructor !== 'function' ||
                    constructor.BYTES_PER_ELEMENT !== 2 ||
                    typedArrayPrototype !== Int16Array.prototype ||
                    typedArrayPrototype.constructor !== constructor ||
                    functionPrototype !== Function.prototype ||
                    Object.getPrototypeOf(constructor) !==
                        functionPrototype ||
                    sample.length !== 4 ||
                    sample.byteLength !== 8 ||
                    sample[0] !== -2 ||
                    sample[1] !== 32767 ||
                    sample[2] !== -32768 ||
                    Object.getPrototypeOf(sample) !==
                        typedArrayPrototype ||
                    !(sample instanceof constructor)) {
                  throw new Error(
                      'unexpected Int16Array constructor fixture');
                }
                globalThis
                    .__heap_dump_int16_typed_array_constructor_exact_edges = {
                  constructor, typedArrayPrototype, functionPrototype
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
                              "__heap_dump_int16_typed_array_constructor_exact_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> constructor_value =
                get_property("constructor");
            v8::Local<v8::Value> typed_array_prototype_value =
                get_property("typedArrayPrototype");
            v8::Local<v8::Value> function_prototype_value =
                get_property("functionPrototype");
            CHECK(constructor_value->IsFunction());
            CHECK(typed_array_prototype_value->IsObject());
            CHECK(function_prototype_value->IsFunction());

            v8::Local<v8::Function> constructor =
                v8::Local<v8::Function>::Cast(constructor_value);
            CHECK(constructor
                      ->Get(context, NewApiString(v8_isolate, "prototype"))
                      .ToLocalChecked()
                      ->StrictEquals(typed_array_prototype_value));
            CHECK(constructor->GetPrototypeV2()->StrictEquals(
                function_prototype_value));
            v8::Local<v8::Value> bytes_per_element =
                constructor
                    ->Get(context,
                          NewApiString(v8_isolate, "BYTES_PER_ELEMENT"))
                    .ToLocalChecked();
            CHECK(bytes_per_element->IsUint32());
            CHECK_EQ(2, bytes_per_element->Uint32Value(context).FromJust());
            CHECK_EQ(INT16_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     LocalObjectInstanceType(constructor_value));

            DirectHandle<JSFunction> js_constructor =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*constructor));
            CHECK_EQ(INT16_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     js_constructor->map()->instance_type());
            constructor_address = js_constructor->address();
            constructor_map_address = js_constructor->map()->address();
            typed_array_prototype_address =
                LocalObjectAddress(typed_array_prototype_value);
            typed_array_prototype_map_address =
                Cast<JSReceiver>(
                    v8::Utils::OpenDirectHandle(*typed_array_prototype_value))
                    ->map()
                    ->address();
            function_prototype_address =
                LocalObjectAddress(function_prototype_value);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(js_constructor);
            artifacts.keep_alive.push_back(
                LocalJSReceiver(typed_array_prototype_value));
            artifacts.keep_alive.push_back(
                LocalJSReceiver(function_prototype_value));
            return artifacts;
          });

  Node* constructor = GetSnapshotNode(generator.get(), constructor_address);
  Node* constructor_map =
      GetSnapshotNode(generator.get(), constructor_map_address);
  Node* typed_array_prototype =
      GetSnapshotNode(generator.get(), typed_array_prototype_address);
  Node* typed_array_prototype_map =
      GetSnapshotNode(generator.get(), typed_array_prototype_map_address);
  Node* function_prototype =
      GetSnapshotNode(generator.get(), function_prototype_address);
  CHECK_NOT_NULL(constructor);
  CHECK_NOT_NULL(constructor_map);
  CHECK_NOT_NULL(typed_array_prototype);
  CHECK_NOT_NULL(typed_array_prototype_map);
  CHECK_NOT_NULL(function_prototype);
  CHECK(NodeHasType(constructor, Node::Type::kClosure));
  CHECK(NodeHasType(constructor_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(typed_array_prototype, Node::Type::kObject));
  CHECK(NodeHasType(typed_array_prototype_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(function_prototype, Node::Type::kClosure));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "prototype", typed_array_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "__proto__", function_prototype);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), constructor, "map", constructor_map);
  CHECK(HasExactNamedEdge(generator.get(), typed_array_prototype,
                          Edge::Type::kInternal, "map",
                          typed_array_prototype_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), typed_array_prototype,
                                   Edge::Type::kInternal, "map",
                                   typed_array_prototype_map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kInternal,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kWeak,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kHidden,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kProperty,
                                         typed_array_prototype_map));
  MarkCoveredInstanceType(INT16_TYPED_ARRAY_CONSTRUCTOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpUint32TypedArrayConstructorExactPrototypeEdges) {
  Address constructor_address = 0;
  Address constructor_map_address = 0;
  Address typed_array_prototype_address = 0;
  Address typed_array_prototype_map_address = 0;
  Address function_prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const constructor = Uint32Array;
                Object.setPrototypeOf(constructor, Function.prototype);
                const typedArrayPrototype = constructor.prototype;
                const functionPrototype = Object.getPrototypeOf(constructor);
                const sample = new constructor(4);
                sample[0] = constructor.BYTES_PER_ELEMENT;
                sample[1] = 0xffffffff;
                sample[2] = 0x100000000;
                sample[3] = -1;
                if (constructor !== globalThis.Uint32Array ||
                    typeof constructor !== 'function' ||
                    constructor.BYTES_PER_ELEMENT !== 4 ||
                    typedArrayPrototype !== Uint32Array.prototype ||
                    typedArrayPrototype.constructor !== constructor ||
                    functionPrototype !== Function.prototype ||
                    Object.getPrototypeOf(constructor) !==
                        functionPrototype ||
                    sample.length !== 4 ||
                    sample.byteLength !== 16 ||
                    sample[0] !== 4 ||
                    sample[1] !== 0xffffffff ||
                    sample[2] !== 0 ||
                    sample[3] !== 0xffffffff ||
                    Object.getPrototypeOf(sample) !==
                        typedArrayPrototype ||
                    !(sample instanceof constructor)) {
                  throw new Error(
                      'unexpected Uint32Array constructor fixture');
                }
                globalThis
                    .__heap_dump_uint32_typed_array_constructor_exact_edges = {
                  constructor, typedArrayPrototype, functionPrototype
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
                              "__heap_dump_uint32_typed_array_constructor_exact_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> constructor_value =
                get_property("constructor");
            v8::Local<v8::Value> typed_array_prototype_value =
                get_property("typedArrayPrototype");
            v8::Local<v8::Value> function_prototype_value =
                get_property("functionPrototype");
            CHECK(constructor_value->IsFunction());
            CHECK(typed_array_prototype_value->IsObject());
            CHECK(function_prototype_value->IsFunction());

            v8::Local<v8::Function> constructor =
                v8::Local<v8::Function>::Cast(constructor_value);
            CHECK(constructor
                      ->Get(context, NewApiString(v8_isolate, "prototype"))
                      .ToLocalChecked()
                      ->StrictEquals(typed_array_prototype_value));
            CHECK(constructor->GetPrototypeV2()->StrictEquals(
                function_prototype_value));
            v8::Local<v8::Value> bytes_per_element =
                constructor
                    ->Get(context,
                          NewApiString(v8_isolate, "BYTES_PER_ELEMENT"))
                    .ToLocalChecked();
            CHECK(bytes_per_element->IsUint32());
            CHECK_EQ(4, bytes_per_element->Uint32Value(context).FromJust());
            CHECK_EQ(UINT32_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     LocalObjectInstanceType(constructor_value));

            DirectHandle<JSFunction> js_constructor =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*constructor));
            CHECK_EQ(UINT32_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     js_constructor->map()->instance_type());
            constructor_address = js_constructor->address();
            constructor_map_address = js_constructor->map()->address();
            typed_array_prototype_address =
                LocalObjectAddress(typed_array_prototype_value);
            typed_array_prototype_map_address =
                Cast<JSReceiver>(
                    v8::Utils::OpenDirectHandle(*typed_array_prototype_value))
                    ->map()
                    ->address();
            function_prototype_address =
                LocalObjectAddress(function_prototype_value);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(js_constructor);
            artifacts.keep_alive.push_back(
                LocalJSReceiver(typed_array_prototype_value));
            artifacts.keep_alive.push_back(
                LocalJSReceiver(function_prototype_value));
            return artifacts;
          });

  Node* constructor = GetSnapshotNode(generator.get(), constructor_address);
  Node* constructor_map =
      GetSnapshotNode(generator.get(), constructor_map_address);
  Node* typed_array_prototype =
      GetSnapshotNode(generator.get(), typed_array_prototype_address);
  Node* typed_array_prototype_map =
      GetSnapshotNode(generator.get(), typed_array_prototype_map_address);
  Node* function_prototype =
      GetSnapshotNode(generator.get(), function_prototype_address);
  CHECK_NOT_NULL(constructor);
  CHECK_NOT_NULL(constructor_map);
  CHECK_NOT_NULL(typed_array_prototype);
  CHECK_NOT_NULL(typed_array_prototype_map);
  CHECK_NOT_NULL(function_prototype);
  CHECK(NodeHasType(constructor, Node::Type::kClosure));
  CHECK(NodeHasType(constructor_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(typed_array_prototype, Node::Type::kObject));
  CHECK(NodeHasType(typed_array_prototype_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(function_prototype, Node::Type::kClosure));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "prototype", typed_array_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "__proto__", function_prototype);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), constructor, "map", constructor_map);
  CHECK(HasExactNamedEdge(generator.get(), typed_array_prototype,
                         Edge::Type::kInternal, "map",
                         typed_array_prototype_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), typed_array_prototype,
                                   Edge::Type::kInternal, "map",
                                   typed_array_prototype_map));
  CHECK_EQ(1,
           CountEdgesFromNodeToTarget(typed_array_prototype,
                                      Edge::Type::kInternal,
                                      typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kWeak,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kHidden,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kProperty,
                                         typed_array_prototype_map));
  MarkCoveredInstanceType(UINT32_TYPED_ARRAY_CONSTRUCTOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpInt32TypedArrayConstructorExactPrototypeEdges) {
  Address constructor_address = 0;
  Address constructor_map_address = 0;
  Address typed_array_prototype_address = 0;
  Address typed_array_prototype_map_address = 0;
  Address function_prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const constructor = Int32Array;
                Object.setPrototypeOf(constructor, Function.prototype);
                const typedArrayPrototype = constructor.prototype;
                const functionPrototype = Object.getPrototypeOf(constructor);
                const sample = new constructor(4);
                sample[0] = -constructor.BYTES_PER_ELEMENT;
                sample[1] = 2147483647;
                sample[2] = 2147483648;
                if (constructor !== globalThis.Int32Array ||
                    typeof constructor !== 'function' ||
                    constructor.BYTES_PER_ELEMENT !== 4 ||
                    typedArrayPrototype !== Int32Array.prototype ||
                    typedArrayPrototype.constructor !== constructor ||
                    functionPrototype !== Function.prototype ||
                    Object.getPrototypeOf(constructor) !==
                        functionPrototype ||
                    sample.length !== 4 ||
                    sample.byteLength !== 16 ||
                    sample[0] !== -4 ||
                    sample[1] !== 2147483647 ||
                    sample[2] !== -2147483648 ||
                    Object.getPrototypeOf(sample) !==
                        typedArrayPrototype ||
                    !(sample instanceof constructor)) {
                  throw new Error(
                      'unexpected Int32Array constructor fixture');
                }
                globalThis
                    .__heap_dump_int32_typed_array_constructor_exact_edges = {
                  constructor, typedArrayPrototype, functionPrototype
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
                              "__heap_dump_int32_typed_array_constructor_exact_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> constructor_value =
                get_property("constructor");
            v8::Local<v8::Value> typed_array_prototype_value =
                get_property("typedArrayPrototype");
            v8::Local<v8::Value> function_prototype_value =
                get_property("functionPrototype");
            CHECK(constructor_value->IsFunction());
            CHECK(typed_array_prototype_value->IsObject());
            CHECK(function_prototype_value->IsFunction());

            v8::Local<v8::Function> constructor =
                v8::Local<v8::Function>::Cast(constructor_value);
            CHECK(constructor
                      ->Get(context, NewApiString(v8_isolate, "prototype"))
                      .ToLocalChecked()
                      ->StrictEquals(typed_array_prototype_value));
            CHECK(constructor->GetPrototypeV2()->StrictEquals(
                function_prototype_value));
            v8::Local<v8::Value> bytes_per_element =
                constructor
                    ->Get(context,
                          NewApiString(v8_isolate, "BYTES_PER_ELEMENT"))
                    .ToLocalChecked();
            CHECK(bytes_per_element->IsUint32());
            CHECK_EQ(4, bytes_per_element->Uint32Value(context).FromJust());
            CHECK_EQ(INT32_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     LocalObjectInstanceType(constructor_value));

            DirectHandle<JSFunction> js_constructor =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*constructor));
            CHECK_EQ(INT32_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     js_constructor->map()->instance_type());
            constructor_address = js_constructor->address();
            constructor_map_address = js_constructor->map()->address();
            typed_array_prototype_address =
                LocalObjectAddress(typed_array_prototype_value);
            typed_array_prototype_map_address =
                Cast<JSReceiver>(
                    v8::Utils::OpenDirectHandle(*typed_array_prototype_value))
                    ->map()
                    ->address();
            function_prototype_address =
                LocalObjectAddress(function_prototype_value);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(js_constructor);
            artifacts.keep_alive.push_back(
                LocalJSReceiver(typed_array_prototype_value));
            artifacts.keep_alive.push_back(
                LocalJSReceiver(function_prototype_value));
            return artifacts;
          });

  Node* constructor = GetSnapshotNode(generator.get(), constructor_address);
  Node* constructor_map =
      GetSnapshotNode(generator.get(), constructor_map_address);
  Node* typed_array_prototype =
      GetSnapshotNode(generator.get(), typed_array_prototype_address);
  Node* typed_array_prototype_map =
      GetSnapshotNode(generator.get(), typed_array_prototype_map_address);
  Node* function_prototype =
      GetSnapshotNode(generator.get(), function_prototype_address);
  CHECK_NOT_NULL(constructor);
  CHECK_NOT_NULL(constructor_map);
  CHECK_NOT_NULL(typed_array_prototype);
  CHECK_NOT_NULL(typed_array_prototype_map);
  CHECK_NOT_NULL(function_prototype);
  CHECK(NodeHasType(constructor, Node::Type::kClosure));
  CHECK(NodeHasType(constructor_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(typed_array_prototype, Node::Type::kObject));
  CHECK(NodeHasType(typed_array_prototype_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(function_prototype, Node::Type::kClosure));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "prototype", typed_array_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "__proto__", function_prototype);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), constructor, "map", constructor_map);
  CHECK(HasExactNamedEdge(generator.get(), typed_array_prototype,
                         Edge::Type::kInternal, "map",
                         typed_array_prototype_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), typed_array_prototype,
                                   Edge::Type::kInternal, "map",
                                   typed_array_prototype_map));
  CHECK_EQ(1,
           CountEdgesFromNodeToTarget(typed_array_prototype,
                                      Edge::Type::kInternal,
                                      typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kWeak,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kHidden,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kProperty,
                                         typed_array_prototype_map));
  MarkCoveredInstanceType(INT32_TYPED_ARRAY_CONSTRUCTOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpFloat32TypedArrayConstructorExactPrototypeEdges) {
  Address constructor_address = 0;
  Address constructor_map_address = 0;
  Address typed_array_prototype_address = 0;
  Address typed_array_prototype_map_address = 0;
  Address function_prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const constructor = Float32Array;
                Object.setPrototypeOf(constructor, Function.prototype);
                const typedArrayPrototype = constructor.prototype;
                const functionPrototype = Object.getPrototypeOf(constructor);
                const sample = new constructor(4);
                sample[0] = constructor.BYTES_PER_ELEMENT;
                sample[1] = 1.5;
                if (constructor !== globalThis.Float32Array ||
                    typeof constructor !== 'function' ||
                    constructor.BYTES_PER_ELEMENT !== 4 ||
                    typedArrayPrototype !== Float32Array.prototype ||
                    typedArrayPrototype.constructor !== constructor ||
                    functionPrototype !== Function.prototype ||
                    Object.getPrototypeOf(constructor) !==
                        functionPrototype ||
                    sample.length !== 4 ||
                    sample.byteLength !== 16 ||
                    sample[0] !== 4 ||
                    sample[1] !== 1.5 ||
                    !Number.isFinite(sample[0]) ||
                    !Number.isFinite(sample[1]) ||
                    Object.getPrototypeOf(sample) !==
                        typedArrayPrototype ||
                    !(sample instanceof constructor)) {
                  throw new Error(
                      'unexpected Float32Array constructor fixture');
                }
                globalThis
                    .__heap_dump_float32_typed_array_constructor_exact_edges = {
                  constructor, typedArrayPrototype, functionPrototype
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
                              "__heap_dump_float32_typed_array_constructor_exact_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> constructor_value =
                get_property("constructor");
            v8::Local<v8::Value> typed_array_prototype_value =
                get_property("typedArrayPrototype");
            v8::Local<v8::Value> function_prototype_value =
                get_property("functionPrototype");
            CHECK(constructor_value->IsFunction());
            CHECK(typed_array_prototype_value->IsObject());
            CHECK(function_prototype_value->IsFunction());

            v8::Local<v8::Function> constructor =
                v8::Local<v8::Function>::Cast(constructor_value);
            CHECK(constructor
                      ->Get(context, NewApiString(v8_isolate, "prototype"))
                      .ToLocalChecked()
                      ->StrictEquals(typed_array_prototype_value));
            CHECK(constructor->GetPrototypeV2()->StrictEquals(
                function_prototype_value));
            v8::Local<v8::Value> bytes_per_element =
                constructor
                    ->Get(context,
                          NewApiString(v8_isolate, "BYTES_PER_ELEMENT"))
                    .ToLocalChecked();
            CHECK(bytes_per_element->IsUint32());
            CHECK_EQ(4, bytes_per_element->Uint32Value(context).FromJust());
            CHECK_EQ(FLOAT32_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     LocalObjectInstanceType(constructor_value));

            DirectHandle<JSFunction> js_constructor =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*constructor));
            CHECK_EQ(FLOAT32_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     js_constructor->map()->instance_type());
            constructor_address = js_constructor->address();
            constructor_map_address = js_constructor->map()->address();
            typed_array_prototype_address =
                LocalObjectAddress(typed_array_prototype_value);
            typed_array_prototype_map_address =
                Cast<JSReceiver>(v8::Utils::OpenDirectHandle(
                    *typed_array_prototype_value))
                    ->map()
                    ->address();
            function_prototype_address =
                LocalObjectAddress(function_prototype_value);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(js_constructor);
            artifacts.keep_alive.push_back(
                LocalJSReceiver(typed_array_prototype_value));
            artifacts.keep_alive.push_back(
                LocalJSReceiver(function_prototype_value));
            return artifacts;
          });

  Node* constructor = GetSnapshotNode(generator.get(), constructor_address);
  Node* constructor_map =
      GetSnapshotNode(generator.get(), constructor_map_address);
  Node* typed_array_prototype =
      GetSnapshotNode(generator.get(), typed_array_prototype_address);
  Node* typed_array_prototype_map =
      GetSnapshotNode(generator.get(), typed_array_prototype_map_address);
  Node* function_prototype =
      GetSnapshotNode(generator.get(), function_prototype_address);
  CHECK_NOT_NULL(constructor);
  CHECK_NOT_NULL(constructor_map);
  CHECK_NOT_NULL(typed_array_prototype);
  CHECK_NOT_NULL(typed_array_prototype_map);
  CHECK_NOT_NULL(function_prototype);
  CHECK(NodeHasType(constructor, Node::Type::kClosure));
  CHECK(NodeHasType(constructor_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(typed_array_prototype, Node::Type::kObject));
  CHECK(NodeHasType(typed_array_prototype_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(function_prototype, Node::Type::kClosure));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "prototype", typed_array_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "__proto__", function_prototype);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), constructor, "map", constructor_map);
  CHECK(HasExactNamedEdge(generator.get(), typed_array_prototype,
                         Edge::Type::kInternal, "map",
                         typed_array_prototype_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), typed_array_prototype,
                                   Edge::Type::kInternal, "map",
                                   typed_array_prototype_map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kInternal,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kWeak,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kHidden,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kProperty,
                                         typed_array_prototype_map));
  MarkCoveredInstanceType(FLOAT32_TYPED_ARRAY_CONSTRUCTOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpFloat64TypedArrayConstructorExactPrototypeEdges) {
  Address constructor_address = 0;
  Address constructor_map_address = 0;
  Address typed_array_prototype_address = 0;
  Address typed_array_prototype_map_address = 0;
  Address function_prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const constructor = Float64Array;
                Object.setPrototypeOf(constructor, Function.prototype);
                const typedArrayPrototype = constructor.prototype;
                const functionPrototype = Object.getPrototypeOf(constructor);
                const sample = new constructor(4);
                sample[0] = constructor.BYTES_PER_ELEMENT;
                sample[1] = 1.5;
                if (constructor !== globalThis.Float64Array ||
                    typeof constructor !== 'function' ||
                    constructor.BYTES_PER_ELEMENT !== 8 ||
                    typedArrayPrototype !== Float64Array.prototype ||
                    typedArrayPrototype.constructor !== constructor ||
                    functionPrototype !== Function.prototype ||
                    Object.getPrototypeOf(constructor) !==
                        functionPrototype ||
                    sample.length !== 4 ||
                    sample.byteLength !== 32 ||
                    sample[0] !== 8 ||
                    sample[1] !== 1.5 ||
                    !Number.isFinite(sample[0]) ||
                    !Number.isFinite(sample[1]) ||
                    Object.getPrototypeOf(sample) !==
                        typedArrayPrototype ||
                    !(sample instanceof constructor)) {
                  throw new Error(
                      'unexpected Float64Array constructor fixture');
                }
                globalThis
                    .__heap_dump_float64_typed_array_constructor_exact_edges = {
                  constructor, typedArrayPrototype, functionPrototype
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
                              "__heap_dump_float64_typed_array_constructor_exact_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> constructor_value =
                get_property("constructor");
            v8::Local<v8::Value> typed_array_prototype_value =
                get_property("typedArrayPrototype");
            v8::Local<v8::Value> function_prototype_value =
                get_property("functionPrototype");
            CHECK(constructor_value->IsFunction());
            CHECK(typed_array_prototype_value->IsObject());
            CHECK(function_prototype_value->IsFunction());

            v8::Local<v8::Function> constructor =
                v8::Local<v8::Function>::Cast(constructor_value);
            CHECK(constructor
                      ->Get(context, NewApiString(v8_isolate, "prototype"))
                      .ToLocalChecked()
                      ->StrictEquals(typed_array_prototype_value));
            CHECK(constructor->GetPrototypeV2()->StrictEquals(
                function_prototype_value));
            v8::Local<v8::Value> bytes_per_element =
                constructor
                    ->Get(context,
                          NewApiString(v8_isolate, "BYTES_PER_ELEMENT"))
                    .ToLocalChecked();
            CHECK(bytes_per_element->IsUint32());
            CHECK_EQ(8, bytes_per_element->Uint32Value(context).FromJust());
            CHECK_EQ(FLOAT64_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     LocalObjectInstanceType(constructor_value));

            DirectHandle<JSFunction> js_constructor =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*constructor));
            CHECK_EQ(FLOAT64_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     js_constructor->map()->instance_type());
            constructor_address = js_constructor->address();
            constructor_map_address = js_constructor->map()->address();
            typed_array_prototype_address =
                LocalObjectAddress(typed_array_prototype_value);
            typed_array_prototype_map_address =
                Cast<JSReceiver>(v8::Utils::OpenDirectHandle(
                    *typed_array_prototype_value))
                    ->map()
                    ->address();
            function_prototype_address =
                LocalObjectAddress(function_prototype_value);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(js_constructor);
            artifacts.keep_alive.push_back(
                LocalJSReceiver(typed_array_prototype_value));
            artifacts.keep_alive.push_back(
                LocalJSReceiver(function_prototype_value));
            return artifacts;
          });

  Node* constructor = GetSnapshotNode(generator.get(), constructor_address);
  Node* constructor_map =
      GetSnapshotNode(generator.get(), constructor_map_address);
  Node* typed_array_prototype =
      GetSnapshotNode(generator.get(), typed_array_prototype_address);
  Node* typed_array_prototype_map =
      GetSnapshotNode(generator.get(), typed_array_prototype_map_address);
  Node* function_prototype =
      GetSnapshotNode(generator.get(), function_prototype_address);
  CHECK_NOT_NULL(constructor);
  CHECK_NOT_NULL(constructor_map);
  CHECK_NOT_NULL(typed_array_prototype);
  CHECK_NOT_NULL(typed_array_prototype_map);
  CHECK_NOT_NULL(function_prototype);
  CHECK(NodeHasType(constructor, Node::Type::kClosure));
  CHECK(NodeHasType(constructor_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(typed_array_prototype, Node::Type::kObject));
  CHECK(NodeHasType(typed_array_prototype_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(function_prototype, Node::Type::kClosure));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "prototype", typed_array_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "__proto__", function_prototype);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), constructor, "map", constructor_map);
  CHECK(HasExactNamedEdge(generator.get(), typed_array_prototype,
                         Edge::Type::kInternal, "map",
                         typed_array_prototype_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), typed_array_prototype,
                                   Edge::Type::kInternal, "map",
                                   typed_array_prototype_map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kInternal,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kWeak,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kHidden,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kProperty,
                                         typed_array_prototype_map));
  MarkCoveredInstanceType(FLOAT64_TYPED_ARRAY_CONSTRUCTOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpFloat16TypedArrayConstructorExactPrototypeEdges) {
  FlagScope<bool> js_float16array(&v8_flags.js_float16array, true);

  Address constructor_address = 0;
  Address constructor_map_address = 0;
  Address typed_array_prototype_address = 0;
  Address typed_array_prototype_map_address = 0;
  Address function_prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const constructor = Float16Array;
                Object.setPrototypeOf(constructor, Function.prototype);
                const typedArrayPrototype = constructor.prototype;
                const functionPrototype = Object.getPrototypeOf(constructor);
                const sample = new constructor(4);
                sample[0] = constructor.BYTES_PER_ELEMENT;
                sample[1] = 1.5;
                if (constructor !== globalThis.Float16Array ||
                    typeof constructor !== 'function' ||
                    constructor.BYTES_PER_ELEMENT !== 2 ||
                    typedArrayPrototype !== Float16Array.prototype ||
                    typedArrayPrototype.constructor !== constructor ||
                    functionPrototype !== Function.prototype ||
                    Object.getPrototypeOf(constructor) !==
                        functionPrototype ||
                    sample.length !== 4 ||
                    sample.byteLength !== 8 ||
                    sample[0] !== 2 ||
                    sample[1] !== 1.5 ||
                    !Number.isFinite(sample[0]) ||
                    !Number.isFinite(sample[1]) ||
                    Object.getPrototypeOf(sample) !==
                        typedArrayPrototype ||
                    !(sample instanceof constructor)) {
                  throw new Error(
                      'unexpected Float16Array constructor fixture');
                }
                globalThis
                    .__heap_dump_float16_typed_array_constructor_exact_edges = {
                  constructor, typedArrayPrototype, functionPrototype
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
                              "__heap_dump_float16_typed_array_constructor_exact_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> constructor_value =
                get_property("constructor");
            v8::Local<v8::Value> typed_array_prototype_value =
                get_property("typedArrayPrototype");
            v8::Local<v8::Value> function_prototype_value =
                get_property("functionPrototype");
            CHECK(constructor_value->IsFunction());
            CHECK(typed_array_prototype_value->IsObject());
            CHECK(function_prototype_value->IsFunction());

            v8::Local<v8::Function> constructor =
                v8::Local<v8::Function>::Cast(constructor_value);
            CHECK(constructor
                      ->Get(context, NewApiString(v8_isolate, "prototype"))
                      .ToLocalChecked()
                      ->StrictEquals(typed_array_prototype_value));
            CHECK(constructor->GetPrototypeV2()->StrictEquals(
                function_prototype_value));
            v8::Local<v8::Value> bytes_per_element =
                constructor
                    ->Get(context,
                          NewApiString(v8_isolate, "BYTES_PER_ELEMENT"))
                    .ToLocalChecked();
            CHECK(bytes_per_element->IsUint32());
            CHECK_EQ(2, bytes_per_element->Uint32Value(context).FromJust());
            CHECK_EQ(FLOAT16_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     LocalObjectInstanceType(constructor_value));

            DirectHandle<JSFunction> js_constructor =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*constructor));
            CHECK_EQ(FLOAT16_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     js_constructor->map()->instance_type());
            constructor_address = js_constructor->address();
            constructor_map_address = js_constructor->map()->address();
            typed_array_prototype_address =
                LocalObjectAddress(typed_array_prototype_value);
            typed_array_prototype_map_address =
                Cast<JSReceiver>(
                    v8::Utils::OpenDirectHandle(*typed_array_prototype_value))
                    ->map()
                    ->address();
            function_prototype_address =
                LocalObjectAddress(function_prototype_value);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(js_constructor);
            artifacts.keep_alive.push_back(
                LocalJSReceiver(typed_array_prototype_value));
            artifacts.keep_alive.push_back(
                LocalJSReceiver(function_prototype_value));
            return artifacts;
          });

  Node* constructor = GetSnapshotNode(generator.get(), constructor_address);
  Node* constructor_map =
      GetSnapshotNode(generator.get(), constructor_map_address);
  Node* typed_array_prototype =
      GetSnapshotNode(generator.get(), typed_array_prototype_address);
  Node* typed_array_prototype_map =
      GetSnapshotNode(generator.get(), typed_array_prototype_map_address);
  Node* function_prototype =
      GetSnapshotNode(generator.get(), function_prototype_address);
  CHECK_NOT_NULL(constructor);
  CHECK_NOT_NULL(constructor_map);
  CHECK_NOT_NULL(typed_array_prototype);
  CHECK_NOT_NULL(typed_array_prototype_map);
  CHECK_NOT_NULL(function_prototype);
  CHECK(NodeHasType(constructor, Node::Type::kClosure));
  CHECK(NodeHasType(constructor_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(typed_array_prototype, Node::Type::kObject));
  CHECK(NodeHasType(typed_array_prototype_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(function_prototype, Node::Type::kClosure));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "prototype", typed_array_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "__proto__", function_prototype);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), constructor, "map", constructor_map);
  CHECK(HasExactNamedEdge(generator.get(), typed_array_prototype,
                         Edge::Type::kInternal, "map",
                         typed_array_prototype_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), typed_array_prototype,
                                   Edge::Type::kInternal, "map",
                                   typed_array_prototype_map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kInternal,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kWeak,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kHidden,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kProperty,
                                         typed_array_prototype_map));
  MarkCoveredInstanceType(FLOAT16_TYPED_ARRAY_CONSTRUCTOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpBigInt64TypedArrayConstructorExactPrototypeEdges) {
  Address constructor_address = 0;
  Address constructor_map_address = 0;
  Address typed_array_prototype_address = 0;
  Address typed_array_prototype_map_address = 0;
  Address function_prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const constructor = BigInt64Array;
                Object.setPrototypeOf(constructor, Function.prototype);
                const typedArrayPrototype = constructor.prototype;
                const functionPrototype = Object.getPrototypeOf(constructor);
                const sample = new constructor(4);
                sample[0] = BigInt(constructor.BYTES_PER_ELEMENT);
                sample[1] = -1n;
                sample[2] = 9223372036854775808n;
                sample[3] = -9223372036854775809n;
                if (constructor !== globalThis.BigInt64Array ||
                    typeof constructor !== 'function' ||
                    constructor.BYTES_PER_ELEMENT !== 8 ||
                    typedArrayPrototype !== BigInt64Array.prototype ||
                    typedArrayPrototype.constructor !== constructor ||
                    functionPrototype !== Function.prototype ||
                    Object.getPrototypeOf(constructor) !==
                        functionPrototype ||
                    sample.length !== 4 ||
                    sample.byteLength !== 32 ||
                    sample[0] !== 8n ||
                    sample[1] !== -1n ||
                    sample[2] !== -9223372036854775808n ||
                    sample[3] !== 9223372036854775807n ||
                    typeof sample[0] !== 'bigint' ||
                    Object.getPrototypeOf(sample) !==
                        typedArrayPrototype ||
                    !(sample instanceof constructor)) {
                  throw new Error(
                      'unexpected BigInt64Array constructor fixture');
                }
                globalThis
                    .__heap_dump_bigint64_typed_array_constructor_exact_edges = {
                  constructor, typedArrayPrototype, functionPrototype
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
                              "__heap_dump_bigint64_typed_array_constructor_exact_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> constructor_value =
                get_property("constructor");
            v8::Local<v8::Value> typed_array_prototype_value =
                get_property("typedArrayPrototype");
            v8::Local<v8::Value> function_prototype_value =
                get_property("functionPrototype");
            CHECK(constructor_value->IsFunction());
            CHECK(typed_array_prototype_value->IsObject());
            CHECK(function_prototype_value->IsFunction());

            v8::Local<v8::Function> constructor =
                v8::Local<v8::Function>::Cast(constructor_value);
            CHECK(constructor
                      ->Get(context, NewApiString(v8_isolate, "prototype"))
                      .ToLocalChecked()
                      ->StrictEquals(typed_array_prototype_value));
            CHECK(constructor->GetPrototypeV2()->StrictEquals(
                function_prototype_value));
            v8::Local<v8::Value> bytes_per_element =
                constructor
                    ->Get(context,
                          NewApiString(v8_isolate, "BYTES_PER_ELEMENT"))
                    .ToLocalChecked();
            CHECK(bytes_per_element->IsUint32());
            CHECK_EQ(8, bytes_per_element->Uint32Value(context).FromJust());
            CHECK_EQ(BIGINT64_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     LocalObjectInstanceType(constructor_value));

            DirectHandle<JSFunction> js_constructor =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*constructor));
            CHECK_EQ(BIGINT64_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     js_constructor->map()->instance_type());
            constructor_address = js_constructor->address();
            constructor_map_address = js_constructor->map()->address();
            typed_array_prototype_address =
                LocalObjectAddress(typed_array_prototype_value);
            typed_array_prototype_map_address =
                Cast<JSReceiver>(
                    v8::Utils::OpenDirectHandle(*typed_array_prototype_value))
                    ->map()
                    ->address();
            function_prototype_address =
                LocalObjectAddress(function_prototype_value);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(js_constructor);
            artifacts.keep_alive.push_back(
                LocalJSReceiver(typed_array_prototype_value));
            artifacts.keep_alive.push_back(
                LocalJSReceiver(function_prototype_value));
            return artifacts;
          });

  Node* constructor = GetSnapshotNode(generator.get(), constructor_address);
  Node* constructor_map =
      GetSnapshotNode(generator.get(), constructor_map_address);
  Node* typed_array_prototype =
      GetSnapshotNode(generator.get(), typed_array_prototype_address);
  Node* typed_array_prototype_map =
      GetSnapshotNode(generator.get(), typed_array_prototype_map_address);
  Node* function_prototype =
      GetSnapshotNode(generator.get(), function_prototype_address);
  CHECK_NOT_NULL(constructor);
  CHECK_NOT_NULL(constructor_map);
  CHECK_NOT_NULL(typed_array_prototype);
  CHECK_NOT_NULL(typed_array_prototype_map);
  CHECK_NOT_NULL(function_prototype);
  CHECK(NodeHasType(constructor, Node::Type::kClosure));
  CHECK(NodeHasType(constructor_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(typed_array_prototype, Node::Type::kObject));
  CHECK(NodeHasType(typed_array_prototype_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(function_prototype, Node::Type::kClosure));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "prototype", typed_array_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "__proto__", function_prototype);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), constructor, "map", constructor_map);
  CHECK(HasExactNamedEdge(generator.get(), typed_array_prototype,
                         Edge::Type::kInternal, "map",
                         typed_array_prototype_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), typed_array_prototype,
                                   Edge::Type::kInternal, "map",
                                   typed_array_prototype_map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kInternal,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kWeak,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kHidden,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kProperty,
                                         typed_array_prototype_map));
  MarkCoveredInstanceType(BIGINT64_TYPED_ARRAY_CONSTRUCTOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpBigUint64TypedArrayConstructorExactPrototypeEdges) {
  Address constructor_address = 0;
  Address constructor_map_address = 0;
  Address typed_array_prototype_address = 0;
  Address typed_array_prototype_map_address = 0;
  Address function_prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const constructor = BigUint64Array;
                Object.setPrototypeOf(constructor, Function.prototype);
                const typedArrayPrototype = constructor.prototype;
                const functionPrototype = Object.getPrototypeOf(constructor);
                const sample = new constructor(4);
                sample[0] = BigInt(constructor.BYTES_PER_ELEMENT);
                sample[1] = 18446744073709551615n;
                sample[2] = 18446744073709551616n;
                sample[3] = -1n;
                if (constructor !== globalThis.BigUint64Array ||
                    typeof constructor !== 'function' ||
                    constructor.BYTES_PER_ELEMENT !== 8 ||
                    typedArrayPrototype !== BigUint64Array.prototype ||
                    typedArrayPrototype.constructor !== constructor ||
                    functionPrototype !== Function.prototype ||
                    Object.getPrototypeOf(constructor) !==
                        functionPrototype ||
                    sample.length !== 4 ||
                    sample.byteLength !== 32 ||
                    sample[0] !== 8n ||
                    sample[1] !== 18446744073709551615n ||
                    sample[2] !== 0n ||
                    sample[3] !== 18446744073709551615n ||
                    typeof sample[0] !== 'bigint' ||
                    Object.getPrototypeOf(sample) !==
                        typedArrayPrototype ||
                    !(sample instanceof constructor)) {
                  throw new Error(
                      'unexpected BigUint64Array constructor fixture');
                }
                globalThis
                    .__heap_dump_biguint64_typed_array_constructor_exact_edges = {
                  constructor, typedArrayPrototype, functionPrototype
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
                              "__heap_dump_biguint64_typed_array_constructor_exact_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> constructor_value =
                get_property("constructor");
            v8::Local<v8::Value> typed_array_prototype_value =
                get_property("typedArrayPrototype");
            v8::Local<v8::Value> function_prototype_value =
                get_property("functionPrototype");
            CHECK(constructor_value->IsFunction());
            CHECK(typed_array_prototype_value->IsObject());
            CHECK(function_prototype_value->IsFunction());

            v8::Local<v8::Function> constructor =
                v8::Local<v8::Function>::Cast(constructor_value);
            CHECK(constructor
                      ->Get(context, NewApiString(v8_isolate, "prototype"))
                      .ToLocalChecked()
                      ->StrictEquals(typed_array_prototype_value));
            CHECK(constructor->GetPrototypeV2()->StrictEquals(
                function_prototype_value));
            v8::Local<v8::Value> bytes_per_element =
                constructor
                    ->Get(context,
                          NewApiString(v8_isolate, "BYTES_PER_ELEMENT"))
                    .ToLocalChecked();
            CHECK(bytes_per_element->IsUint32());
            CHECK_EQ(8, bytes_per_element->Uint32Value(context).FromJust());
            CHECK_EQ(BIGUINT64_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     LocalObjectInstanceType(constructor_value));

            DirectHandle<JSFunction> js_constructor =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*constructor));
            CHECK_EQ(BIGUINT64_TYPED_ARRAY_CONSTRUCTOR_TYPE,
                     js_constructor->map()->instance_type());
            constructor_address = js_constructor->address();
            constructor_map_address = js_constructor->map()->address();
            typed_array_prototype_address =
                LocalObjectAddress(typed_array_prototype_value);
            typed_array_prototype_map_address =
                Cast<JSReceiver>(
                    v8::Utils::OpenDirectHandle(*typed_array_prototype_value))
                    ->map()
                    ->address();
            function_prototype_address =
                LocalObjectAddress(function_prototype_value);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(js_constructor);
            artifacts.keep_alive.push_back(
                LocalJSReceiver(typed_array_prototype_value));
            artifacts.keep_alive.push_back(
                LocalJSReceiver(function_prototype_value));
            return artifacts;
          });

  Node* constructor = GetSnapshotNode(generator.get(), constructor_address);
  Node* constructor_map =
      GetSnapshotNode(generator.get(), constructor_map_address);
  Node* typed_array_prototype =
      GetSnapshotNode(generator.get(), typed_array_prototype_address);
  Node* typed_array_prototype_map =
      GetSnapshotNode(generator.get(), typed_array_prototype_map_address);
  Node* function_prototype =
      GetSnapshotNode(generator.get(), function_prototype_address);
  CHECK_NOT_NULL(constructor);
  CHECK_NOT_NULL(constructor_map);
  CHECK_NOT_NULL(typed_array_prototype);
  CHECK_NOT_NULL(typed_array_prototype_map);
  CHECK_NOT_NULL(function_prototype);
  CHECK(NodeHasType(constructor, Node::Type::kClosure));
  CHECK(NodeHasType(constructor_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(typed_array_prototype, Node::Type::kObject));
  CHECK(NodeHasType(typed_array_prototype_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(function_prototype, Node::Type::kClosure));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "prototype", typed_array_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "__proto__", function_prototype);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), constructor, "map", constructor_map);
  CHECK(HasExactNamedEdge(generator.get(), typed_array_prototype,
                         Edge::Type::kInternal, "map",
                         typed_array_prototype_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), typed_array_prototype,
                                   Edge::Type::kInternal, "map",
                                   typed_array_prototype_map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kInternal,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kWeak,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kHidden,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kProperty,
                                         typed_array_prototype_map));
  MarkCoveredInstanceType(BIGUINT64_TYPED_ARRAY_CONSTRUCTOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSTypedArrayPrototypeClassicUsageEdge) {
  Address typed_array_prototype_address = 0;
  Address typed_array_prototype_map_address = 0;
  Address parent_prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const hasOwn = Function.prototype.call.bind(
                    Object.prototype.hasOwnProperty);
                const typedArrayPrototype = Uint8Array.prototype;
                const parentPrototype =
                    Object.getPrototypeOf(typedArrayPrototype);
                const typedArray = new Uint8Array(4);
                typedArray[0] = 1;
                typedArray[1] = 2;
                typedArray[2] = 3;
                typedArray[3] = 4;
                const subarray =
                    typedArrayPrototype.subarray.call(typedArray, 1, 3);
                const slice =
                    typedArrayPrototype.slice.call(typedArray, 1, 3);
                const iterator =
                    typedArrayPrototype[Symbol.iterator].call(typedArray);
                const first = iterator.next();
                const second = iterator.next();
                if (Object.getPrototypeOf(Uint8Array.prototype) !==
                        parentPrototype ||
                    Object.getPrototypeOf(parentPrototype) !==
                        Object.prototype ||
                    Object.getPrototypeOf(typedArray) !==
                        Uint8Array.prototype ||
                    !hasOwn(typedArrayPrototype, 'BYTES_PER_ELEMENT') ||
                    typedArrayPrototype.BYTES_PER_ELEMENT !== 1 ||
                    typeof typedArrayPrototype.subarray !== 'function' ||
                    typeof typedArrayPrototype.slice !== 'function' ||
                    typeof typedArrayPrototype[Symbol.iterator] !==
                        'function' ||
                    subarray.length !== 2 ||
                    subarray[0] !== 2 ||
                    subarray[1] !== 3 ||
                    slice.length !== 2 ||
                    slice[0] !== 2 ||
                    slice[1] !== 3 ||
                    first.value !== 1 ||
                    first.done !== false ||
                    second.value !== 2 ||
                    second.done !== false) {
                  throw new Error(
                      'unexpected TypedArray.prototype fixture');
                }
                globalThis
                    .__heap_dump_js_typed_array_prototype_classic_usage_edge = {
                  typedArrayPrototype, parentPrototype, typedArray, subarray,
                  slice, iterator
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
                              "__heap_dump_js_typed_array_prototype_classic_usage_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> typed_array_prototype_value =
                get_property("typedArrayPrototype");
            v8::Local<v8::Value> parent_prototype_value =
                get_property("parentPrototype");
            CHECK(typed_array_prototype_value->IsObject());
            CHECK(parent_prototype_value->IsObject());
            CHECK_EQ(JS_TYPED_ARRAY_PROTOTYPE_TYPE,
                     LocalObjectInstanceType(typed_array_prototype_value));
            v8::Local<v8::Object> typed_array_prototype_object =
                v8::Local<v8::Object>::Cast(typed_array_prototype_value);
            CHECK(typed_array_prototype_object->GetPrototypeV2()->StrictEquals(
                parent_prototype_value));
            CHECK(typed_array_prototype_object
                      ->Get(context,
                            NewApiString(v8_isolate, "BYTES_PER_ELEMENT"))
                      .ToLocalChecked()
                      ->IsUint32());
            CHECK(typed_array_prototype_object
                      ->Get(context, NewApiString(v8_isolate, "subarray"))
                      .ToLocalChecked()
                      ->IsFunction());
            CHECK(typed_array_prototype_object
                      ->Get(context, NewApiString(v8_isolate, "slice"))
                      .ToLocalChecked()
                      ->IsFunction());
            CHECK(typed_array_prototype_object
                      ->Get(context, v8::Symbol::GetIterator(v8_isolate))
                      .ToLocalChecked()
                      ->IsFunction());
            DirectHandle<JSObject> js_typed_array_prototype =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(
                    *typed_array_prototype_object));
            CHECK_EQ(JS_TYPED_ARRAY_PROTOTYPE_TYPE,
                     js_typed_array_prototype->map()->instance_type());
            typed_array_prototype_map_address =
                js_typed_array_prototype->map()->address();
            typed_array_prototype_address =
                js_typed_array_prototype->address();
            parent_prototype_address =
                LocalObjectAddress(parent_prototype_value);

            return DumpArtifacts{};
          });

  Node* typed_array_prototype =
      GetSnapshotNode(generator.get(), typed_array_prototype_address);
  Node* typed_array_prototype_map =
      GetSnapshotNode(generator.get(), typed_array_prototype_map_address);
  Node* parent_prototype =
      GetSnapshotNode(generator.get(), parent_prototype_address);
  CHECK_NOT_NULL(typed_array_prototype);
  CHECK_NOT_NULL(typed_array_prototype_map);
  CHECK_NOT_NULL(parent_prototype);
  CHECK(NodeHasType(typed_array_prototype, Node::Type::kObject));
  CHECK(NodeHasType(typed_array_prototype_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(parent_prototype, Node::Type::kObject));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), typed_array_prototype, "__proto__", parent_prototype);
  CHECK(HasExactNamedEdge(generator.get(), typed_array_prototype,
                          Edge::Type::kInternal, "map",
                          typed_array_prototype_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), typed_array_prototype,
                                   Edge::Type::kInternal, "map",
                                   typed_array_prototype_map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kInternal,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype, Edge::Type::kWeak,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kHidden,
                                         typed_array_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(typed_array_prototype,
                                         Edge::Type::kProperty,
                                         typed_array_prototype_map));
  MarkCoveredInstanceType(JS_TYPED_ARRAY_PROTOTYPE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSDataViewExactBufferEdge) {
  Address data_view_address = 0;
  Address array_buffer_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const arrayBuffer = new ArrayBuffer(24);
                const dataView = new DataView(arrayBuffer, 4, 8);
                dataView.setUint8(0, 42);
                if (!(dataView instanceof DataView) ||
                    dataView.byteOffset !== 4 ||
                    dataView.byteLength !== 8 ||
                    dataView.buffer !== arrayBuffer ||
                    dataView.getUint8(0) !== 42 ||
                    arrayBuffer.byteLength !== 24) {
                  throw new Error('unexpected DataView fixture');
                }
                globalThis.__heap_dump_js_data_view_exact_buffer_edge = {
                  dataView, arrayBuffer
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
                              "__heap_dump_js_data_view_exact_buffer_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> data_view_value = get_property("dataView");
            v8::Local<v8::Value> array_buffer_value =
                get_property("arrayBuffer");
            CHECK(data_view_value->IsDataView());
            CHECK_EQ(JS_DATA_VIEW_TYPE,
                     LocalObjectInstanceType(data_view_value));
            v8::Local<v8::DataView> data_view =
                v8::Local<v8::DataView>::Cast(data_view_value);
            CHECK_EQ(4, static_cast<int>(data_view->ByteOffset()));
            CHECK_EQ(8, static_cast<int>(data_view->ByteLength()));
            CHECK(data_view->Buffer()
                      ->Equals(context, array_buffer_value)
                      .FromJust());
            DirectHandle<JSDataViewOrRabGsabDataView> js_data_view =
                v8::Utils::OpenDirectHandle(*data_view);
            CHECK_EQ(JS_DATA_VIEW_TYPE,
                     js_data_view->map()->instance_type());
            map_address = js_data_view->map()->address();
            CHECK_EQ(4, static_cast<int>(js_data_view->byte_offset()));
            CHECK_EQ(8, static_cast<int>(js_data_view->byte_length()));
            data_view_address = js_data_view->address();
            array_buffer_address = LocalObjectAddress(array_buffer_value);

            return DumpArtifacts{};
          });

  Node* data_view = GetSnapshotNode(generator.get(), data_view_address);
  Node* array_buffer = GetSnapshotNode(generator.get(), array_buffer_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(data_view);
  CHECK_NOT_NULL(array_buffer);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(data_view, Node::Type::kObject));
  CHECK(NodeHasType(array_buffer, Node::Type::kObject));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), data_view, "buffer", array_buffer);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(generator.get(),
                                                        data_view, "map", map);
  MarkCoveredInstanceType(JS_DATA_VIEW_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSRabGsabDataViewExactBufferEdge) {
  Address data_view_address = 0;
  Address shared_array_buffer_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                class RabGsabDataView extends DataView {}
                const sharedBuffer = new SharedArrayBuffer(24, { maxByteLength: 48 });
                const dataView = new RabGsabDataView(sharedBuffer, 4);
                dataView.setUint8(0, 42);
                if (dataView.constructor !== RabGsabDataView ||
                    !(dataView instanceof DataView) ||
                    dataView.byteOffset !== 4 ||
                    dataView.byteLength !== 20 ||
                    dataView.buffer !== sharedBuffer ||
                    dataView.getUint8(0) !== 42 ||
                    sharedBuffer.byteLength !== 24 ||
                    sharedBuffer.maxByteLength !== 48) {
                  throw new Error(
                      'unexpected RAB/GSAB DataView fixture');
                }
                globalThis
                    .__heap_dump_js_rab_gsab_data_view_exact_buffer_edge = {
                  dataView, sharedBuffer
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
                              "__heap_dump_js_rab_gsab_data_view_exact_buffer_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> data_view_value = get_property("dataView");
            v8::Local<v8::Value> shared_buffer_value =
                get_property("sharedBuffer");
            CHECK(data_view_value->IsDataView());
            CHECK(shared_buffer_value->IsSharedArrayBuffer());
            CHECK_EQ(JS_RAB_GSAB_DATA_VIEW_TYPE,
                     LocalObjectInstanceType(data_view_value));
            v8::Local<v8::DataView> data_view =
                v8::Local<v8::DataView>::Cast(data_view_value);
            CHECK_EQ(4, static_cast<int>(data_view->ByteOffset()));
            CHECK_EQ(20, static_cast<int>(data_view->ByteLength()));
            DirectHandle<JSDataViewOrRabGsabDataView> js_data_view =
                v8::Utils::OpenDirectHandle(*data_view);
            CHECK_EQ(JS_RAB_GSAB_DATA_VIEW_TYPE,
                     js_data_view->map()->instance_type());
            map_address = js_data_view->map()->address();
            CHECK_EQ(4, static_cast<int>(js_data_view->byte_offset()));
            data_view_address = js_data_view->address();
            shared_array_buffer_address = LocalObjectAddress(shared_buffer_value);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(js_data_view);
            artifacts.keep_alive.push_back(
                LocalJSReceiver(shared_buffer_value.As<v8::Object>()));
            return artifacts;
          });

  Node* data_view = GetSnapshotNode(generator.get(), data_view_address);
  Node* shared_array_buffer =
      GetSnapshotNode(generator.get(), shared_array_buffer_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK_NOT_NULL(data_view);
  CHECK_NOT_NULL(shared_array_buffer);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(data_view, Node::Type::kObject));
  CHECK(NodeHasType(shared_array_buffer, Node::Type::kObject));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(generator.get(),
                                                        data_view, "map", map);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), data_view, "buffer", shared_array_buffer);
  MarkCoveredInstanceType(JS_RAB_GSAB_DATA_VIEW_TYPE);
}

#else
UNINITIALIZED_TEST(HeapDumpJSArrayBufferExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSTypedArrayExactBufferEdge) {}
UNINITIALIZED_TEST(HeapDumpJSTypedArrayBufferBackingStoreEdgeParity) {}
UNINITIALIZED_TEST(HeapDumpUint8TypedArrayConstructorExactPrototypeEdges) {}
UNINITIALIZED_TEST(HeapDumpUint8ClampedTypedArrayConstructorExactPrototypeEdges) {}
UNINITIALIZED_TEST(HeapDumpInt8TypedArrayConstructorExactPrototypeEdges) {}
UNINITIALIZED_TEST(HeapDumpUint16TypedArrayConstructorExactPrototypeEdges) {}
UNINITIALIZED_TEST(HeapDumpInt16TypedArrayConstructorExactPrototypeEdges) {}
UNINITIALIZED_TEST(HeapDumpUint32TypedArrayConstructorExactPrototypeEdges) {}
UNINITIALIZED_TEST(HeapDumpInt32TypedArrayConstructorExactPrototypeEdges) {}
UNINITIALIZED_TEST(HeapDumpFloat32TypedArrayConstructorExactPrototypeEdges) {}
UNINITIALIZED_TEST(HeapDumpFloat64TypedArrayConstructorExactPrototypeEdges) {}
UNINITIALIZED_TEST(HeapDumpFloat16TypedArrayConstructorExactPrototypeEdges) {}
UNINITIALIZED_TEST(HeapDumpBigInt64TypedArrayConstructorExactPrototypeEdges) {}
UNINITIALIZED_TEST(HeapDumpBigUint64TypedArrayConstructorExactPrototypeEdges) {}
UNINITIALIZED_TEST(HeapDumpJSTypedArrayPrototypeClassicUsageEdge) {}
UNINITIALIZED_TEST(HeapDumpJSDataViewExactBufferEdge) {}
UNINITIALIZED_TEST(HeapDumpJSRabGsabDataViewExactBufferEdge) {}
#endif

}  // namespace heap
}  // namespace internal
}  // namespace v8
