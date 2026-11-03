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
#include "src/objects/js-disposable-stack-inl.h"
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

void CheckExactInternalMapEdgeNoWrongKind(SnapshotGenerator* generator,
                                         const Node* object,
                                         const Node* map);

v8::MaybeLocal<v8::Context> HeapDumpHostCreateShadowRealmContext(
    v8::Local<v8::Context> initiator_context) {
  CHECK(!initiator_context.IsEmpty());
  return v8::Context::New(v8::Isolate::GetCurrent());
}

UNINITIALIZED_TEST(HeapDumpJSDateExactPrototypeEdge) {
  Address date_address = 0;
  Address date_map_address = 0;
  Address prototype_address = 0;
  Address prototype_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const date = new Date(Date.UTC(2026, 4, 4, 12, 34, 56));
                const prototype = Object.getPrototypeOf(date);
                if (prototype !== Date.prototype ||
                    date.getUTCFullYear() !== 2026 ||
                    date.getUTCMonth() !== 4 ||
                    date.getUTCDate() !== 4) {
                  throw new Error('unexpected Date fixture');
                }
                globalThis.__heap_dump_js_date_exact_prototype_edge = {
                  date, prototype
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
                              "__heap_dump_js_date_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> date_value = get_property("date");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            CHECK_EQ(JS_DATE_TYPE, LocalObjectInstanceType(date_value));
            DirectHandle<JSDate> js_date =
                Cast<JSDate>(v8::Utils::OpenDirectHandle(
                    *v8::Local<v8::Object>::Cast(date_value)));
            CHECK_EQ(JS_DATE_TYPE, js_date->map()->instance_type());
            date_address = js_date->address();
            date_map_address = js_date->map()->address();
            DirectHandle<JSObject> js_prototype = Cast<JSObject>(
                v8::Utils::OpenDirectHandle(*v8::Local<v8::Object>::Cast(
                    prototype_value)));
            prototype_address = js_prototype->address();
            prototype_map_address = js_prototype->map()->address();

            return DumpArtifacts{};
          });

  Node* date = GetSnapshotNode(generator.get(), date_address);
  Node* date_map = GetSnapshotNode(generator.get(), date_map_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  Node* prototype_map = GetSnapshotNode(generator.get(), prototype_map_address);
  CHECK_NOT_NULL(date);
  CHECK_NOT_NULL(date_map);
  CHECK_NOT_NULL(prototype);
  CHECK_NOT_NULL(prototype_map);
  CHECK(NodeHasType(date, Node::Type::kObject));
  CHECK(NodeHasType(date_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CHECK(NodeHasType(prototype_map, Node::Type::kObjectShape));

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), date, date_map);

  CHECK(HasExactNamedEdge(generator.get(), date, Edge::Type::kProperty,
                          "__proto__", prototype));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), date,
                                   Edge::Type::kProperty, "__proto__",
                                   prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(date, Edge::Type::kHidden,
                                         prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(date, Edge::Type::kInternal,
                                         prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(date, Edge::Type::kWeak, prototype));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(date, Edge::Type::kShortcut, prototype));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), prototype, prototype_map);
  MarkCoveredInstanceType(JS_DATE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSRegExpExactPrototypeEdge) {
  Address regexp_address = 0;
  Address regexp_map_address = 0;
  Address prototype_address = 0;
  Address prototype_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const regexp = /heap-dump-js-reg-exp-sample/gi;
                const prototype = Object.getPrototypeOf(regexp);
                if (prototype !== RegExp.prototype ||
                    regexp.source !== 'heap-dump-js-reg-exp-sample' ||
                    regexp.flags !== 'gi') {
                  throw new Error('unexpected RegExp fixture');
                }
                globalThis.__heap_dump_js_reg_exp_exact_prototype_edge = {
                  regexp, prototype
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
                              "__heap_dump_js_reg_exp_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> regexp_value = get_property("regexp");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            CHECK_EQ(JS_REG_EXP_TYPE, LocalObjectInstanceType(regexp_value));
            DirectHandle<JSRegExp> js_reg_exp =
                Cast<JSRegExp>(v8::Utils::OpenDirectHandle(
                    *v8::Local<v8::Object>::Cast(regexp_value)));
            CHECK_EQ(JS_REG_EXP_TYPE, js_reg_exp->map()->instance_type());
            regexp_address = js_reg_exp->address();
            regexp_map_address = js_reg_exp->map()->address();
            DirectHandle<JSObject> js_prototype = Cast<JSObject>(
                v8::Utils::OpenDirectHandle(*v8::Local<v8::Object>::Cast(
                    prototype_value)));
            prototype_address = js_prototype->address();
            prototype_map_address = js_prototype->map()->address();

            return DumpArtifacts{};
          });

  Node* regexp = GetSnapshotNode(generator.get(), regexp_address);
  Node* regexp_map = GetSnapshotNode(generator.get(), regexp_map_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  Node* prototype_map = GetSnapshotNode(generator.get(), prototype_map_address);
  CHECK_NOT_NULL(regexp);
  CHECK_NOT_NULL(regexp_map);
  CHECK_NOT_NULL(prototype);
  CHECK_NOT_NULL(prototype_map);
  CHECK(NodeHasType(regexp, Node::Type::kRegExp));
  CHECK(NodeHasType(regexp_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CHECK(NodeHasType(prototype_map, Node::Type::kObjectShape));

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), regexp, regexp_map);

  CHECK(HasExactNamedEdge(generator.get(), regexp, Edge::Type::kProperty,
                          "__proto__", prototype));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), regexp,
                                   Edge::Type::kProperty, "__proto__",
                                   prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(regexp, Edge::Type::kHidden,
                                         prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(regexp, Edge::Type::kInternal,
                                         prototype));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(regexp, Edge::Type::kWeak, prototype));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(regexp, Edge::Type::kShortcut,
                                      prototype));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), prototype,
                                      prototype_map);
  MarkCoveredInstanceType(JS_REG_EXP_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSRegExpPrototypeExactPrototypeEdge) {
  Address regexp_prototype_address = 0;
  Address object_prototype_address = 0;
  Address regexp_prototype_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const regexpPrototype = RegExp.prototype;
                const objectPrototype = Object.getPrototypeOf(regexpPrototype);
                if (objectPrototype !== Object.prototype ||
                    typeof regexpPrototype.exec !== 'function' ||
                    typeof regexpPrototype.test !== 'function') {
                  throw new Error('unexpected RegExp.prototype fixture');
                }
                globalThis
                    .__heap_dump_js_reg_exp_prototype_exact_prototype_edge = {
                  regexpPrototype, objectPrototype
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
                              "__heap_dump_js_reg_exp_prototype_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> regexp_prototype_value =
                get_property("regexpPrototype");
            v8::Local<v8::Value> object_prototype_value =
                get_property("objectPrototype");
            CHECK(regexp_prototype_value->IsObject());
            CHECK(object_prototype_value->IsObject());
            CHECK_EQ(JS_REG_EXP_PROTOTYPE_TYPE,
                     LocalObjectInstanceType(regexp_prototype_value));
            v8::Local<v8::Object> regexp_prototype_object =
                v8::Local<v8::Object>::Cast(regexp_prototype_value);
            CHECK(regexp_prototype_object->GetPrototypeV2()->StrictEquals(
                object_prototype_value));
            DirectHandle<JSObject> js_regexp_prototype =
                Cast<JSObject>(
                    v8::Utils::OpenDirectHandle(*regexp_prototype_object));
            CHECK_EQ(JS_REG_EXP_PROTOTYPE_TYPE,
                     js_regexp_prototype->map()->instance_type());
            regexp_prototype_address = js_regexp_prototype->address();
            regexp_prototype_map_address = js_regexp_prototype->map()->address();
            object_prototype_address =
                LocalObjectAddress(object_prototype_value);

            return DumpArtifacts{};
          });

  Node* regexp_prototype =
      GetSnapshotNode(generator.get(), regexp_prototype_address);
  Node* object_prototype =
      GetSnapshotNode(generator.get(), object_prototype_address);
  Node* regexp_prototype_map =
      GetSnapshotNode(generator.get(), regexp_prototype_map_address);
  CHECK_NOT_NULL(regexp_prototype);
  CHECK_NOT_NULL(object_prototype);
  CHECK_NOT_NULL(regexp_prototype_map);
  CHECK(NodeHasType(regexp_prototype, Node::Type::kObject));
  CHECK(NodeHasType(object_prototype, Node::Type::kObject));
  CHECK(NodeHasType(regexp_prototype_map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), regexp_prototype,
                          Edge::Type::kInternal, "map",
                          regexp_prototype_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), regexp_prototype,
                                   Edge::Type::kInternal, "map",
                                   regexp_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(regexp_prototype, Edge::Type::kWeak,
                                         regexp_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(regexp_prototype, Edge::Type::kHidden,
                                         regexp_prototype_map));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(regexp_prototype, Edge::Type::kProperty,
                                      regexp_prototype_map));

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), regexp_prototype, "__proto__", object_prototype);
  CHECK_EQ(
      1, CountEdgesFromNodeToTarget(regexp_prototype, Edge::Type::kInternal,
                                    regexp_prototype_map));
  MarkCoveredInstanceType(JS_REG_EXP_PROTOTYPE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSRegExpConstructorExactPrototypeEdges) {
  Address constructor_address = 0;
  Address constructor_map_address = 0;
  Address regexp_prototype_address = 0;
  Address function_prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const constructor = globalThis.RegExp;
                const regexpPrototype = constructor.prototype;
                const functionPrototype = Object.getPrototypeOf(constructor);
                const constructed = new constructor('heap-dump-constructed', 'g');
                const called = constructor('heap-dump-called', 'i');
                if (constructor !== RegExp ||
                    typeof constructor !== 'function' ||
                    regexpPrototype !== RegExp.prototype ||
                    regexpPrototype.constructor !== constructor ||
                    functionPrototype !== Function.prototype ||
                    Object.getPrototypeOf(constructor) !== functionPrototype ||
                    !(constructed instanceof RegExp) ||
                    !(called instanceof RegExp) ||
                    constructed.source !== 'heap-dump-constructed' ||
                    constructed.flags !== 'g' ||
                    called.source !== 'heap-dump-called' ||
                    called.flags !== 'i') {
                  throw new Error('unexpected RegExp constructor fixture');
                }
                globalThis.__heap_dump_js_reg_exp_constructor_exact_edges = {
                  constructor, regexpPrototype, functionPrototype, constructed,
                  called
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
                              "__heap_dump_js_reg_exp_constructor_exact_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> constructor_value =
                get_property("constructor");
            v8::Local<v8::Value> regexp_prototype_value =
                get_property("regexpPrototype");
            v8::Local<v8::Value> function_prototype_value =
                get_property("functionPrototype");
            v8::Local<v8::Value> constructed_value =
                get_property("constructed");
            v8::Local<v8::Value> called_value = get_property("called");
            CHECK(constructor_value->IsFunction());
            CHECK(regexp_prototype_value->IsObject());
            CHECK(function_prototype_value->IsFunction());
            CHECK(constructed_value->IsRegExp());
            CHECK(called_value->IsRegExp());

            v8::Local<v8::Function> constructor =
                v8::Local<v8::Function>::Cast(constructor_value);
            CHECK(constructor
                      ->Get(context, NewApiString(v8_isolate, "prototype"))
                      .ToLocalChecked()
                      ->StrictEquals(regexp_prototype_value));
            CHECK(constructor->GetPrototypeV2()->StrictEquals(
                function_prototype_value));
            CHECK_EQ(JS_REG_EXP_CONSTRUCTOR_TYPE,
                     LocalObjectInstanceType(constructor_value));

            DirectHandle<JSFunction> js_constructor =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*constructor));
            CHECK_EQ(JS_REG_EXP_CONSTRUCTOR_TYPE,
                     js_constructor->map()->instance_type());
            CHECK(js_constructor->has_initial_map());
            constructor_address = js_constructor->address();
            constructor_map_address = js_constructor->map()->address();
            regexp_prototype_address =
                LocalObjectAddress(regexp_prototype_value);
            function_prototype_address =
                LocalObjectAddress(function_prototype_value);

            return DumpArtifacts{};
          });

  Node* constructor = GetSnapshotNode(generator.get(), constructor_address);
  Node* constructor_map = GetSnapshotNode(generator.get(), constructor_map_address);
  Node* regexp_prototype =
      GetSnapshotNode(generator.get(), regexp_prototype_address);
  Node* function_prototype =
      GetSnapshotNode(generator.get(), function_prototype_address);
  CHECK_NOT_NULL(constructor);
  CHECK_NOT_NULL(constructor_map);
  CHECK_NOT_NULL(regexp_prototype);
  CHECK_NOT_NULL(function_prototype);
  CHECK(NodeHasType(constructor, Node::Type::kClosure));
  CHECK(NodeHasType(constructor_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(regexp_prototype, Node::Type::kObject));
  CHECK(NodeHasType(function_prototype, Node::Type::kClosure));

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), constructor,
                                       constructor_map);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "prototype", regexp_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "__proto__", function_prototype);
  MarkCoveredInstanceType(JS_REG_EXP_CONSTRUCTOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSPromiseExactPrototypeEdge) {
  Address promise_address = 0;
  Address promise_map_address = 0;
  Address prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const promise = new Promise(() => {});
                const prototype = Object.getPrototypeOf(promise);
                if (prototype !== Promise.prototype ||
                    !(promise instanceof Promise)) {
                  throw new Error('unexpected Promise fixture');
                }
                globalThis.__heap_dump_js_promise_exact_prototype_edge = {
                  promise, prototype
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
                              "__heap_dump_js_promise_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> promise_value = get_property("promise");
            CHECK_EQ(JS_PROMISE_TYPE, LocalObjectInstanceType(promise_value));
            DirectHandle<JSPromise> js_promise =
                Cast<JSPromise>(v8::Utils::OpenDirectHandle(
                    *v8::Local<v8::Object>::Cast(promise_value)));
            CHECK_EQ(JS_PROMISE_TYPE, js_promise->map()->instance_type());
            promise_address = js_promise->address();
            promise_map_address = js_promise->map()->address();
            prototype_address = LocalObjectAddress(get_property("prototype"));

            return DumpArtifacts{};
          });

  Node* promise = GetSnapshotNode(generator.get(), promise_address);
  Node* promise_map = GetSnapshotNode(generator.get(), promise_map_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  CHECK_NOT_NULL(promise);
  CHECK_NOT_NULL(promise_map);
  CHECK_NOT_NULL(prototype);
  CHECK(NodeHasType(promise, Node::Type::kObject));
  CHECK(NodeHasType(promise_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(prototype, Node::Type::kObject));

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), promise, promise_map);

  CHECK(HasExactNamedEdge(generator.get(), promise, Edge::Type::kProperty,
                          "__proto__", prototype));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), promise,
                                   Edge::Type::kProperty, "__proto__",
                                   prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(promise, Edge::Type::kHidden,
                                         prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(promise, Edge::Type::kInternal,
                                         prototype));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(promise, Edge::Type::kWeak, prototype));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(promise, Edge::Type::kShortcut,
                                      prototype));
  MarkCoveredInstanceType(JS_PROMISE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSPromisePrototypeClassicUsageEdges) {
  Address promise_prototype_address = 0;
  Address promise_prototype_map_address = 0;
  Address object_prototype_address = 0;
  Address promise_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const hasOwn = Function.prototype.call.bind(
                    Object.prototype.hasOwnProperty);
                const promisePrototype = Promise.prototype;
                const objectPrototype =
                    Object.getPrototypeOf(promisePrototype);
                const promise = new Promise(() => {});
                const chained = promisePrototype.then.call(promise, () => {});
                const then = promisePrototype.then;
                const catchFn = promisePrototype.catch;
                const finallyFn = promisePrototype.finally;
                if (objectPrototype !== Object.prototype ||
                    Object.getPrototypeOf(promise) !== promisePrototype ||
                    Object.getPrototypeOf(chained) !== promisePrototype ||
                    !(promise instanceof Promise) ||
                    !(chained instanceof Promise) ||
                    !hasOwn(promisePrototype, 'then') ||
                    !hasOwn(promisePrototype, 'catch') ||
                    !hasOwn(promisePrototype, 'finally') ||
                    typeof then !== 'function' ||
                    typeof catchFn !== 'function' ||
                    typeof finallyFn !== 'function' ||
                    promise.then !== then ||
                    promise.catch !== catchFn ||
                    promise.finally !== finallyFn) {
                  throw new Error('unexpected Promise.prototype fixture');
                }
                globalThis
                    .__heap_dump_js_promise_prototype_classic_usage_edges = {
                  promisePrototype, objectPrototype, promise, chained,
                  then, catchFn, finallyFn
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
                              "__heap_dump_js_promise_prototype_classic_usage_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> promise_prototype_value =
                get_property("promisePrototype");
            v8::Local<v8::Value> object_prototype_value =
                get_property("objectPrototype");
            v8::Local<v8::Value> promise_value = get_property("promise");
            v8::Local<v8::Value> then_value = get_property("then");
            v8::Local<v8::Value> catch_value = get_property("catchFn");
            v8::Local<v8::Value> finally_value = get_property("finallyFn");
            CHECK(promise_prototype_value->IsObject());
            CHECK(object_prototype_value->IsObject());
            CHECK(promise_value->IsObject());
            CHECK(then_value->IsFunction());
            CHECK(catch_value->IsFunction());
            CHECK(finally_value->IsFunction());
            CHECK_EQ(JS_PROMISE_PROTOTYPE_TYPE,
                     LocalObjectInstanceType(promise_prototype_value));
            v8::Local<v8::Object> promise_prototype_object =
                v8::Local<v8::Object>::Cast(promise_prototype_value);
            CHECK(promise_prototype_object->GetPrototypeV2()->StrictEquals(
                object_prototype_value));
            CHECK(promise_prototype_object
                      ->Get(context, NewApiString(v8_isolate, "then"))
                      .ToLocalChecked()
                      ->StrictEquals(then_value));
            CHECK(promise_prototype_object
                      ->Get(context, NewApiString(v8_isolate, "catch"))
                      .ToLocalChecked()
                      ->StrictEquals(catch_value));
            CHECK(promise_prototype_object
                      ->Get(context, NewApiString(v8_isolate, "finally"))
                      .ToLocalChecked()
                      ->StrictEquals(finally_value));
            DirectHandle<JSObject> js_promise_prototype =
                Cast<JSObject>(
                    v8::Utils::OpenDirectHandle(*promise_prototype_object));
            CHECK_EQ(JS_PROMISE_PROTOTYPE_TYPE,
                     js_promise_prototype->map()->instance_type());
            promise_prototype_address = js_promise_prototype->address();
            promise_prototype_map_address = js_promise_prototype->map()->address();
            object_prototype_address =
                LocalObjectAddress(object_prototype_value);
            promise_address = LocalObjectAddress(promise_value);

            return DumpArtifacts{};
          });

  Node* promise_prototype =
      GetSnapshotNode(generator.get(), promise_prototype_address);
  Node* promise_prototype_map =
      GetSnapshotNode(generator.get(), promise_prototype_map_address);
  Node* object_prototype =
      GetSnapshotNode(generator.get(), object_prototype_address);
  Node* promise = GetSnapshotNode(generator.get(), promise_address);
  CHECK_NOT_NULL(promise_prototype);
  CHECK_NOT_NULL(promise_prototype_map);
  CHECK_NOT_NULL(object_prototype);
  CHECK_NOT_NULL(promise);
  CHECK(NodeHasType(promise_prototype, Node::Type::kObject));
  CHECK(NodeHasType(promise_prototype_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(object_prototype, Node::Type::kObject));
  CHECK(NodeHasType(promise, Node::Type::kObject));

  CHECK(HasExactNamedEdge(generator.get(), promise_prototype,
                          Edge::Type::kInternal, "map",
                          promise_prototype_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), promise_prototype,
                                   Edge::Type::kInternal, "map",
                                   promise_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(promise_prototype, Edge::Type::kWeak,
                                         promise_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(promise_prototype, Edge::Type::kHidden,
                                         promise_prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(promise_prototype,
                                         Edge::Type::kProperty,
                                         promise_prototype_map));
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), promise_prototype, "__proto__", object_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), promise, "__proto__", promise_prototype);
  CHECK_EQ(
      1, CountEdgesFromNodeToTarget(promise_prototype, Edge::Type::kInternal,
                                    promise_prototype_map));
  MarkCoveredInstanceType(JS_PROMISE_PROTOTYPE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSPromiseConstructorExactPrototypeEdges) {
  Address constructor_address = 0;
  Address constructor_map_address = 0;
  Address promise_prototype_address = 0;
  Address function_prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const constructor = Promise;
                const promisePrototype = constructor.prototype;
                const functionPrototype = Object.getPrototypeOf(constructor);
                const promise = new constructor(() => {});
                const resolved = constructor.resolve(1);
                if (constructor !== globalThis.Promise ||
                    typeof constructor !== 'function' ||
                    promisePrototype !== Promise.prototype ||
                    promisePrototype.constructor !== constructor ||
                    functionPrototype !== Function.prototype ||
                    Object.getPrototypeOf(constructor) !== functionPrototype ||
                    !(promise instanceof Promise) ||
                    !(resolved instanceof Promise) ||
                    typeof constructor.resolve !== 'function' ||
                    typeof constructor.reject !== 'function' ||
                    typeof constructor.all !== 'function') {
                  throw new Error('unexpected Promise constructor fixture');
                }
                globalThis.__heap_dump_js_promise_constructor_exact_edges = {
                  constructor, promisePrototype, functionPrototype, promise,
                  resolved
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
                              "__heap_dump_js_promise_constructor_exact_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> constructor_value =
                get_property("constructor");
            v8::Local<v8::Value> promise_prototype_value =
                get_property("promisePrototype");
            v8::Local<v8::Value> function_prototype_value =
                get_property("functionPrototype");
            v8::Local<v8::Value> promise_value = get_property("promise");
            v8::Local<v8::Value> resolved_value = get_property("resolved");
            CHECK(constructor_value->IsFunction());
            CHECK(promise_prototype_value->IsObject());
            CHECK(function_prototype_value->IsFunction());
            CHECK(promise_value->IsPromise());
            CHECK(resolved_value->IsPromise());

            v8::Local<v8::Function> constructor =
                v8::Local<v8::Function>::Cast(constructor_value);
            CHECK(constructor
                      ->Get(context, NewApiString(v8_isolate, "prototype"))
                      .ToLocalChecked()
                      ->StrictEquals(promise_prototype_value));
            CHECK(constructor->GetPrototypeV2()->StrictEquals(
                function_prototype_value));
            CHECK_EQ(JS_PROMISE_CONSTRUCTOR_TYPE,
                     LocalObjectInstanceType(constructor_value));

            DirectHandle<JSFunction> js_constructor =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*constructor));
            CHECK_EQ(JS_PROMISE_CONSTRUCTOR_TYPE,
                     js_constructor->map()->instance_type());
            constructor_address = js_constructor->address();
            constructor_map_address = js_constructor->map()->address();
            promise_prototype_address =
                LocalObjectAddress(promise_prototype_value);
            function_prototype_address =
                LocalObjectAddress(function_prototype_value);

            return DumpArtifacts{};
          });

  Node* constructor = GetSnapshotNode(generator.get(), constructor_address);
  Node* constructor_map = GetSnapshotNode(generator.get(), constructor_map_address);
  Node* promise_prototype =
      GetSnapshotNode(generator.get(), promise_prototype_address);
  Node* function_prototype =
      GetSnapshotNode(generator.get(), function_prototype_address);
  CHECK_NOT_NULL(constructor);
  CHECK_NOT_NULL(constructor_map);
  CHECK_NOT_NULL(promise_prototype);
  CHECK_NOT_NULL(function_prototype);
  CHECK(NodeHasType(constructor, Node::Type::kClosure));
  CHECK(NodeHasType(constructor_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(promise_prototype, Node::Type::kObject));
  CHECK(NodeHasType(function_prototype, Node::Type::kClosure));

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), constructor,
                                       constructor_map);

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "prototype", promise_prototype);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), constructor, "__proto__", function_prototype);
  MarkCoveredInstanceType(JS_PROMISE_CONSTRUCTOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSErrorExactPrototypeEdge) {
  Address error_address = 0;
  Address error_map_address = 0;
  Address prototype_address = 0;
  Address prototype_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const error = new Error('heap-dump-js-error-sample');
                const prototype = Object.getPrototypeOf(error);
                if (!(error instanceof Error) ||
                    prototype !== Error.prototype ||
                    error.message !== 'heap-dump-js-error-sample') {
                  throw new Error('unexpected Error fixture');
                }
                globalThis.__heap_dump_js_error_exact_prototype_edge = {
                  error, prototype
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
                              "__heap_dump_js_error_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> error_value = get_property("error");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            CHECK(error_value->IsNativeError());
            v8::Local<v8::Object> error_object =
                v8::Local<v8::Object>::Cast(error_value);
            CHECK(error_object->GetPrototypeV2()->StrictEquals(
                prototype_value));
            CHECK(error_object
                      ->Get(context, NewApiString(v8_isolate, "message"))
                      .ToLocalChecked()
                      ->Equals(context,
                               NewApiString(v8_isolate,
                                            "heap-dump-js-error-sample"))
                      .FromJust());
            CHECK_EQ(JS_ERROR_TYPE, LocalObjectInstanceType(error_value));
            DirectHandle<JSObject> js_error =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(*error_object));
            CHECK_EQ(JS_ERROR_TYPE, js_error->map()->instance_type());
            error_address = js_error->address();
            error_map_address = js_error->map()->address();
            prototype_address = LocalObjectAddress(prototype_value);
            DirectHandle<JSObject> js_prototype =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(
                    *v8::Local<v8::Object>::Cast(prototype_value)));
            prototype_map_address = js_prototype->map()->address();

            return DumpArtifacts{};
          });

  Node* error = GetSnapshotNode(generator.get(), error_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  Node* error_map = GetSnapshotNode(generator.get(), error_map_address);
  Node* prototype_map = GetSnapshotNode(generator.get(), prototype_map_address);
  CHECK_NOT_NULL(error);
  CHECK_NOT_NULL(prototype);
  CHECK_NOT_NULL(error_map);
  CHECK_NOT_NULL(prototype_map);
  CHECK(NodeHasType(error, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CHECK(NodeHasType(error_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(prototype_map, Node::Type::kObjectShape));

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), prototype, prototype_map);

  CHECK(HasExactNamedEdge(generator.get(), error, Edge::Type::kProperty,
                          "__proto__", prototype));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), error,
                                   Edge::Type::kProperty, "__proto__",
                                   prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(error, Edge::Type::kHidden,
                                         prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(error, Edge::Type::kInternal,
                                         prototype));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(error, Edge::Type::kWeak, prototype));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(error, Edge::Type::kShortcut, prototype));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), error, error_map);
  MarkCoveredInstanceType(JS_ERROR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSRawJSONExactRawJSONPropertyEdge) {
  Address raw_json_address = 0;
  Address raw_json_source_address = 0;
  Address raw_json_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const sample = '12345';
                const rawJson = JSON.rawJSON(sample);
                const ownProperties = Object.getOwnPropertyNames(rawJson);
                if (typeof rawJson !== 'object' ||
                    Object.getPrototypeOf(rawJson) !== null ||
                    !Object.prototype.hasOwnProperty.call(rawJson, 'rawJSON') ||
                    ownProperties.length !== 1 ||
                    ownProperties[0] !== 'rawJSON' ||
                    rawJson.rawJSON !== sample) {
                  throw new Error('unexpected JS.rawJSON fixture');
                }
                globalThis.__heap_dump_js_raw_json_exact_raw_json_property_edge = {
                  rawJson, sample
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
                              "__heap_dump_js_raw_json_exact_raw_json_property_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> raw_json_value = get_property("rawJson");
            v8::Local<v8::Value> sample_value = get_property("sample");
            CHECK(raw_json_value->IsObject());
            CHECK(sample_value->IsString());

            v8::Local<v8::Object> raw_json_object =
                v8::Local<v8::Object>::Cast(raw_json_value);
            CHECK(raw_json_object->GetPrototypeV2()->IsNull());
            CHECK(raw_json_object
                      ->Has(context, NewApiString(v8_isolate, "rawJSON"))
                      .FromJust());
            CHECK_EQ(JS_RAW_JSON_TYPE, LocalObjectInstanceType(raw_json_value));
            DirectHandle<JSRawJson> js_raw_json =
                Cast<JSRawJson>(
                    v8::Utils::OpenDirectHandle(*raw_json_object));
            CHECK_EQ(JS_RAW_JSON_TYPE, js_raw_json->map()->instance_type());
            raw_json_address = js_raw_json->address();
            raw_json_map_address = js_raw_json->map()->address();

            v8::Local<v8::Value> raw_source_value =
                raw_json_object
                    ->Get(context, NewApiString(v8_isolate, "rawJSON"))
                    .ToLocalChecked();
            CHECK(raw_source_value->IsString());
            CHECK(sample_value->StrictEquals(raw_source_value));
            raw_json_source_address =
                Cast<String>(v8::Utils::OpenDirectHandle(
                    *v8::Local<v8::String>::Cast(raw_source_value)))
                    ->address();

            return DumpArtifacts{};
          });

  Node* raw_json = GetSnapshotNode(generator.get(), raw_json_address);
  Node* raw_source = GetSnapshotNode(generator.get(), raw_json_source_address);
  Node* raw_json_map = GetSnapshotNode(generator.get(), raw_json_map_address);
  CHECK_NOT_NULL(raw_json);
  CHECK_NOT_NULL(raw_source);
  CHECK_NOT_NULL(raw_json_map);
  CHECK(NodeHasType(raw_json, Node::Type::kObject));
  CHECK(NodeHasType(raw_source, Node::Type::kString));
  CHECK(NodeHasType(raw_json_map, Node::Type::kObjectShape));

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), raw_json,
                                       raw_json_map);

  MarkCoveredInstanceType(JS_RAW_JSON_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSDisposableStackBuiltinsExactPrototypeEdges) {
  Address disposable_stack_address = 0;
  Address disposable_stack_prototype_address = 0;
  Address disposable_stack_map_address = 0;
  InstanceType disposable_stack_type = static_cast<InstanceType>(-1);
  Address async_disposable_stack_address = 0;
  Address async_disposable_stack_prototype_address = 0;
  Address async_disposable_stack_map_address = 0;
  InstanceType async_disposable_stack_type = static_cast<InstanceType>(-1);

  bool marked_base = false;
  bool marked_sync = false;
  bool marked_async = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const disposableStack =
                    typeof DisposableStack === 'function'
                        ? new DisposableStack()
                        : null;
                const asyncDisposableStack =
                    typeof AsyncDisposableStack === 'function'
                        ? new AsyncDisposableStack()
                        : null;

                if (disposableStack !== null &&
                    (Object.getPrototypeOf(disposableStack) !==
                        DisposableStack.prototype ||
                     typeof disposableStack.dispose !== 'function')) {
                  throw new Error('unexpected DisposableStack fixture');
                }
                if (asyncDisposableStack !== null &&
                    (Object.getPrototypeOf(asyncDisposableStack) !==
                        AsyncDisposableStack.prototype ||
                     typeof asyncDisposableStack.disposeAsync !== 'function')) {
                  throw new Error('unexpected AsyncDisposableStack fixture');
                }

                globalThis.__heap_dump_js_disposable_stack_builtins_exact_edges = {
                  disposableStack,
                  asyncDisposableStack,
                  disposableStackPrototype:
                      disposableStack === null
                      ? null
                      : Object.getPrototypeOf(disposableStack),
                  asyncDisposableStackPrototype:
                      asyncDisposableStack === null
                      ? null
                      : Object.getPrototypeOf(asyncDisposableStack),
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
                              "__heap_dump_js_disposable_stack_builtins_exact_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };

            v8::Local<v8::Value> disposable_stack_value =
                get_property("disposableStack");
            v8::Local<v8::Value> async_disposable_stack_value =
                get_property("asyncDisposableStack");
            v8::Local<v8::Value> disposable_stack_prototype_value =
                get_property("disposableStackPrototype");
            v8::Local<v8::Value> async_disposable_stack_prototype_value =
                get_property("asyncDisposableStackPrototype");

            if (!disposable_stack_value->IsNull()) {
              CHECK(disposable_stack_value->IsObject());
              CHECK(disposable_stack_prototype_value->IsObject());
              CHECK(!disposable_stack_prototype_value->IsNull());
              DirectHandle<JSObject> js_disposable_stack =
                  Cast<JSObject>(v8::Utils::OpenDirectHandle(
                      *v8::Local<v8::Object>::Cast(disposable_stack_value)));
              CHECK_EQ(
                  LocalObjectInstanceType(disposable_stack_value),
                  js_disposable_stack->map()->instance_type());
              disposable_stack_address = js_disposable_stack->address();
              disposable_stack_map_address = js_disposable_stack->map()->address();
              disposable_stack_prototype_address =
                  LocalObjectAddress(disposable_stack_prototype_value);
              disposable_stack_type =
                  js_disposable_stack->map()->instance_type();
            }

            if (!async_disposable_stack_value->IsNull()) {
              CHECK(async_disposable_stack_value->IsObject());
              CHECK(async_disposable_stack_prototype_value->IsObject());
              CHECK(!async_disposable_stack_prototype_value->IsNull());
              DirectHandle<JSObject> js_async_disposable_stack =
                  Cast<JSObject>(v8::Utils::OpenDirectHandle(
                      *v8::Local<v8::Object>::Cast(
                          async_disposable_stack_value)));
              CHECK_EQ(
                  LocalObjectInstanceType(async_disposable_stack_value),
                  js_async_disposable_stack->map()->instance_type());
              async_disposable_stack_address = js_async_disposable_stack->address();
              async_disposable_stack_map_address =
                  js_async_disposable_stack->map()->address();
              async_disposable_stack_prototype_address =
                  LocalObjectAddress(async_disposable_stack_prototype_value);
              async_disposable_stack_type =
                  js_async_disposable_stack->map()->instance_type();
            }

            return DumpArtifacts{};
          });

  auto validate_disposable_stack = [&](Address object_address,
                                      Address prototype_address,
                                      Address map_address,
                                      InstanceType observed_type,
                                      const char* target_name) {
    if (object_address == 0) {
      PrintF("[TODO] %s object not available in this build.\n", target_name);
      return;
    }

    Node* object = GetSnapshotNode(generator.get(), object_address);
    Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
    Node* map_node = GetSnapshotNode(generator.get(), map_address);
    if (object == nullptr || prototype == nullptr || map_node == nullptr) {
      PrintF("[TODO] %s snapshot node missing.\n", target_name);
      return;
    }

    CHECK(NodeHasType(object, Node::Type::kObject));
    CHECK(NodeHasType(prototype, Node::Type::kObject));
    CHECK(NodeHasType(map_node, Node::Type::kObjectShape));
    CHECK(HasExactNamedEdge(generator.get(), object, Edge::Type::kProperty,
                            "__proto__", prototype));
    CHECK_EQ(1, CountExactNamedEdges(generator.get(), object,
                                     Edge::Type::kProperty, "__proto__",
                                     prototype));
    CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kHidden,
                                           prototype));
    CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kInternal,
                                           prototype));
    CHECK(HasExactNamedEdge(generator.get(), object, Edge::Type::kInternal,
                           "map", map_node));
    CHECK_EQ(1, CountExactNamedEdges(generator.get(), object, Edge::Type::kInternal,
                                     "map", map_node));
    CHECK_EQ(1, CountEdgesFromNodeToTarget(object, Edge::Type::kInternal,
                                           map_node));
    CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kWeak,
                                           map_node));
    CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kHidden,
                                           map_node));
    CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kProperty,
                                           map_node));

    if (observed_type == JS_DISPOSABLE_STACK_BASE_TYPE) {
      if (marked_base) return;
      MarkCoveredInstanceType(JS_DISPOSABLE_STACK_BASE_TYPE);
      marked_base = true;
      return;
    }
    if (observed_type == JS_SYNC_DISPOSABLE_STACK_TYPE) {
      if (marked_sync) return;
      MarkCoveredInstanceType(JS_SYNC_DISPOSABLE_STACK_TYPE);
      marked_sync = true;
      return;
    }
    if (observed_type == JS_ASYNC_DISPOSABLE_STACK_TYPE) {
      if (marked_async) return;
      MarkCoveredInstanceType(JS_ASYNC_DISPOSABLE_STACK_TYPE);
      marked_async = true;
      return;
    }
    PrintF("[TODO] %s unexpected InstanceType %d.\n", target_name,
           observed_type);
  };

  if (disposable_stack_address == 0 && async_disposable_stack_address == 0) {
    PrintF("[TODO] DisposableStack / AsyncDisposableStack constructors are "
           "not available in this build; leaving stack targets unmarked.\n");
  } else {
    validate_disposable_stack(disposable_stack_address,
                             disposable_stack_prototype_address,
                             disposable_stack_map_address,
                             disposable_stack_type, "DisposableStack");
    validate_disposable_stack(
        async_disposable_stack_address, async_disposable_stack_prototype_address,
        async_disposable_stack_map_address,
        async_disposable_stack_type, "AsyncDisposableStack");
  }
}

UNINITIALIZED_TEST(HeapDumpJSDisposableStackBaseExactMapEdge) {
  Address disposable_stack_address = 0;
  Address disposable_stack_map_address = 0;
  Address stack_address = 0;
  Address error_address = 0;
  Address error_message_address = 0;
  InstanceType observed_type = static_cast<InstanceType>(-1);

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<JSDisposableStackBase> disposable_stack =
                isolate->factory()->NewJSDisposableStackBase();
            JSDisposableStackBase::InitializeJSDisposableStackBase(
                isolate, disposable_stack);
            DirectHandle<Map> disposable_stack_map(disposable_stack->map(),
                                                   isolate);
            DirectHandle<Object> stack(disposable_stack->stack(), isolate);
            DirectHandle<Object> error(disposable_stack->error(), isolate);
            DirectHandle<Object> error_message(
                disposable_stack->error_message(), isolate);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(disposable_stack);
            artifacts.keep_alive.push_back(disposable_stack_map);
            artifacts.keep_alive.push_back(stack);
            artifacts.keep_alive.push_back(error);
            artifacts.keep_alive.push_back(error_message);

            InvokeAtomicMajorGC(isolate->heap());

            observed_type = disposable_stack->map()->instance_type();
            CHECK_EQ(JS_DISPOSABLE_STACK_BASE_TYPE, observed_type);
            CHECK_EQ(disposable_stack_map->address(),
                     disposable_stack->map()->address());
            disposable_stack_address = disposable_stack->address();
            disposable_stack_map_address = disposable_stack_map->address();
            stack_address = Cast<HeapObject>(*stack)->address();
            error_address = Cast<HeapObject>(*error)->address();
            error_message_address = Cast<HeapObject>(*error_message)->address();

            return artifacts;
          });

  Node* disposable_stack =
      GetSnapshotNode(generator.get(), disposable_stack_address);
  Node* disposable_stack_map =
      GetSnapshotNode(generator.get(), disposable_stack_map_address);
  Node* stack = GetSnapshotNode(generator.get(), stack_address);
  Node* error = GetSnapshotNode(generator.get(), error_address);
  Node* error_message = GetSnapshotNode(generator.get(), error_message_address);
  CHECK_NOT_NULL(disposable_stack);
  CHECK_NOT_NULL(disposable_stack_map);
  CHECK_NOT_NULL(stack);
  CHECK_NOT_NULL(error);
  CHECK_NOT_NULL(error_message);
  CHECK(NodeHasType(disposable_stack, Node::Type::kObject));
  CHECK(NodeHasType(disposable_stack_map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), disposable_stack,
                          Edge::Type::kInternal, "map",
                          disposable_stack_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), disposable_stack,
                                   Edge::Type::kInternal, "map",
                                   disposable_stack_map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(disposable_stack,
                                         Edge::Type::kInternal,
                                         disposable_stack_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(disposable_stack, Edge::Type::kHidden,
                                         disposable_stack_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(disposable_stack,
                                         Edge::Type::kProperty,
                                         disposable_stack_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(disposable_stack, Edge::Type::kWeak,
                                         disposable_stack_map));
  CHECK(HasExactNamedEdge(generator.get(), disposable_stack,
                          Edge::Type::kInternal, "stack", stack));
  CHECK(HasExactNamedEdge(generator.get(), disposable_stack,
                          Edge::Type::kInternal, "error", error));
  CHECK(HasExactNamedEdge(generator.get(), disposable_stack,
                          Edge::Type::kInternal, "error_message",
                          error_message));
  CHECK_EQ(JS_DISPOSABLE_STACK_BASE_TYPE, observed_type);
  MarkCoveredInstanceType(JS_DISPOSABLE_STACK_BASE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSAtomicsSynchronizationPrimitiveExactPrototypeEdges) {
#if V8_CAN_CREATE_SHARED_HEAP_BOOL && !COMPRESS_POINTERS_IN_MULTIPLE_CAGES_BOOL
  FlagScope<bool> harmony_struct(&v8_flags.harmony_struct, true);
  FlagScope<bool> shared_string_table(&v8_flags.shared_string_table, true);
  FlagList::EnforceFlagImplications();
#endif

  Address condition_address = 0;
  Address condition_prototype_address = 0;
  Address condition_map_address = 0;
  Address mutex_address = 0;
  Address mutex_prototype_address = 0;
  Address mutex_map_address = 0;
  bool condition_constructed = false;
  bool mutex_constructed = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const sample = {
                  condition: null,
                  conditionConstructed: false,
                  mutex: null,
                  mutexConstructed: false
                };
                if (typeof Atomics === 'object' &&
                    typeof Atomics.Condition === 'function') {
                  try {
                    sample.condition = new Atomics.Condition();
                    sample.conditionConstructed = true;
                  } catch (e) {}
                }
                if (typeof Atomics === 'object' &&
                    typeof Atomics.Mutex === 'function') {
                  try {
                    sample.mutex = new Atomics.Mutex();
                    sample.mutexConstructed = true;
                  } catch (e) {}
                }
                if (sample.conditionConstructed) {
                  sample.conditionPrototype =
                      Object.getPrototypeOf(sample.condition);
                }
                if (sample.mutexConstructed) {
                  sample.mutexPrototype = Object.getPrototypeOf(sample.mutex);
                }
                globalThis
                    .__heap_dump_js_atomics_sync_exact_prototype_edge = sample;
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_atomics_sync_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };

            v8::Local<v8::Value> condition_value = get_property("condition");
            v8::Local<v8::Value> mutex_value = get_property("mutex");
            condition_constructed =
                get_property("conditionConstructed")->BooleanValue(v8_isolate);
            mutex_constructed =
                get_property("mutexConstructed")->BooleanValue(v8_isolate);

            if (condition_constructed) {
              CHECK(condition_value->IsObject());
              v8::Local<v8::Value> condition_prototype_value =
                  get_property("conditionPrototype");
              CHECK(condition_value.As<v8::Object>()
                        ->GetPrototypeV2()
                        ->StrictEquals(condition_prototype_value));
              CHECK_EQ(JS_ATOMICS_CONDITION_TYPE,
                       LocalObjectInstanceType(condition_value));
              DirectHandle<JSObject> js_condition =
                  Cast<JSObject>(v8::Utils::OpenDirectHandle(
                      *v8::Local<v8::Object>::Cast(condition_value)));
              CHECK_EQ(JS_ATOMICS_CONDITION_TYPE,
                       js_condition->map()->instance_type());
              condition_map_address = js_condition->map()->address();
              condition_address = js_condition->address();
              condition_prototype_address =
                  LocalObjectAddress(condition_prototype_value);
            } else {
              PrintF(
                  "[TODO] JS_ATOMICS_CONDITION_TYPE is not constructible/stable in "
                  "this build; skipping marker.\n");
            }

            if (mutex_constructed) {
              CHECK(mutex_value->IsObject());
              v8::Local<v8::Value> mutex_prototype_value =
                  get_property("mutexPrototype");
              CHECK(
                  mutex_value.As<v8::Object>()->GetPrototypeV2()->StrictEquals(
                      mutex_prototype_value));
              CHECK_EQ(JS_ATOMICS_MUTEX_TYPE,
                       LocalObjectInstanceType(mutex_value));
              DirectHandle<JSObject> js_mutex =
                  Cast<JSObject>(v8::Utils::OpenDirectHandle(
                      *v8::Local<v8::Object>::Cast(mutex_value)));
              CHECK_EQ(JS_ATOMICS_MUTEX_TYPE,
                       js_mutex->map()->instance_type());
              mutex_map_address = js_mutex->map()->address();
              mutex_address = js_mutex->address();
              mutex_prototype_address = LocalObjectAddress(mutex_prototype_value);
            } else {
              PrintF(
                  "[TODO] JS_ATOMICS_MUTEX_TYPE is not constructible/stable in "
                  "this build; skipping marker.\n");
            }

            return DumpArtifacts{};
          });

  if (condition_constructed) {
    Node* condition = GetSnapshotNode(generator.get(), condition_address);
    Node* condition_prototype =
        GetSnapshotNode(generator.get(), condition_prototype_address);
    Node* condition_map = GetSnapshotNode(generator.get(), condition_map_address);
    CHECK_NOT_NULL(condition);
    CHECK_NOT_NULL(condition_prototype);
    CHECK_NOT_NULL(condition_map);
    CHECK(NodeHasType(condition_map, Node::Type::kObjectShape));
    CheckExactInternalMapEdgeNoWrongKind(generator.get(), condition,
                                         condition_map);
    CHECK(NodeHasType(condition, Node::Type::kObject));
    if (!HasExactNamedEdge(generator.get(), condition, Edge::Type::kProperty,
                          "__proto__", condition_prototype)) {
      PrintF(
          "[TODO] JS_ATOMICS_CONDITION_TYPE is missing translated property "
          "edge \"__proto__\" to prototype; marker will use map-edge gate only.\n");
    } else {
      CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
          generator.get(), condition, "__proto__", condition_prototype);
    }
    MarkCoveredInstanceType(JS_ATOMICS_CONDITION_TYPE);
  }

  if (mutex_constructed) {
    Node* mutex = GetSnapshotNode(generator.get(), mutex_address);
    Node* mutex_prototype = GetSnapshotNode(generator.get(), mutex_prototype_address);
    Node* mutex_map = GetSnapshotNode(generator.get(), mutex_map_address);
    CHECK_NOT_NULL(mutex);
    CHECK_NOT_NULL(mutex_prototype);
    CHECK_NOT_NULL(mutex_map);
    CHECK(NodeHasType(mutex_map, Node::Type::kObjectShape));
    CheckExactInternalMapEdgeNoWrongKind(generator.get(), mutex, mutex_map);
    CHECK(NodeHasType(mutex, Node::Type::kObject));
    if (!HasExactNamedEdge(generator.get(), mutex, Edge::Type::kProperty,
                          "__proto__", mutex_prototype)) {
      PrintF(
          "[TODO] JS_ATOMICS_MUTEX_TYPE is missing translated property edge "
          "\"__proto__\" to prototype; marker will use map-edge gate only.\n");
    } else {
      CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
          generator.get(), mutex, "__proto__", mutex_prototype);
    }
    MarkCoveredInstanceType(JS_ATOMICS_MUTEX_TYPE);
  }
}

UNINITIALIZED_TEST(HeapDumpJSShadowRealmExactPrototypeEdge) {
  FlagScope<bool> harmony_shadow_realm(&v8_flags.harmony_shadow_realm, true);
  FlagList::EnforceFlagImplications();

  Address shadow_realm_address = 0;
  Address shadow_realm_map_address = 0;
  Address prototype_address = 0;
  bool shadow_realm_constructed = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            reinterpret_cast<v8::Isolate*>(isolate)
                ->SetHostCreateShadowRealmContextCallback(
                    HeapDumpHostCreateShadowRealmContext);
            RunScript(context, R"(
              (function() {
                const sample = {
                  shadowRealm: null,
                  shadowRealmConstructed: false,
                  shadowRealmPrototype: null
                };
                if (typeof ShadowRealm === 'function') {
                  try {
                    sample.shadowRealm = new ShadowRealm();
                    sample.shadowRealmConstructed = true;
                    sample.shadowRealmPrototype =
                        Object.getPrototypeOf(sample.shadowRealm);
                  } catch (e) {}
                }
                globalThis.__heap_dump_js_shadow_realm_exact_prototype_edge = sample;
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
                              "__heap_dump_js_shadow_realm_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };

            v8::Local<v8::Value> shadow_realm_value =
                get_property("shadowRealm");
            shadow_realm_constructed = get_property("shadowRealmConstructed")
                                           ->BooleanValue(v8_isolate);
            if (!shadow_realm_constructed) {
              PrintF(
                  "[TODO] JS_SHADOW_REALM_TYPE is not constructible/stable in "
                  "this build; skipping marker.\n");
              return DumpArtifacts{};
            }

            v8::Local<v8::Value> shadow_realm_prototype_value =
                get_property("shadowRealmPrototype");
            CHECK(shadow_realm_value->IsObject());
            v8::Local<v8::Object> shadow_realm_object =
                v8::Local<v8::Object>::Cast(shadow_realm_value);
            CHECK(shadow_realm_object
                      ->GetPrototypeV2()
                      ->StrictEquals(shadow_realm_prototype_value));
            CHECK_EQ(JS_SHADOW_REALM_TYPE,
                     LocalObjectInstanceType(shadow_realm_value));
            DirectHandle<JSObject> js_shadow_realm =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(*shadow_realm_object));
            CHECK_EQ(JS_SHADOW_REALM_TYPE,
                     js_shadow_realm->map()->instance_type());
            shadow_realm_address = js_shadow_realm->address();
            shadow_realm_map_address = js_shadow_realm->map()->address();
            prototype_address = LocalObjectAddress(shadow_realm_prototype_value);

            return DumpArtifacts{};
          });

  if (!shadow_realm_constructed) return;

  Node* shadow_realm = GetSnapshotNode(generator.get(), shadow_realm_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  Node* map = GetSnapshotNode(generator.get(), shadow_realm_map_address);
  CHECK_NOT_NULL(shadow_realm);
  CHECK_NOT_NULL(prototype);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(shadow_realm, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), shadow_realm, map);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), shadow_realm, "__proto__", prototype);
  MarkCoveredInstanceType(JS_SHADOW_REALM_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSWrappedFunctionFromShadowRealmExactPrototypeEdge) {
  Address wrapped_function_address = 0;
  Address wrapped_function_map_address = 0;
  Address prototype_address = 0;
  bool wrapped_function_constructed = false;
  bool wrapped_function_type_match = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const sample = {
                  wrappedFunction: null,
                  wrappedFunctionConstructed: false,
                  wrappedFunctionPrototype: null,
                  wrappedFunctionCallResult: null
                };
                if (typeof ShadowRealm === 'function') {
                  try {
                    const realm = new ShadowRealm();
                    const wrappedFunction = realm.evaluate(
                        '(function wrappedSample() { return 7; })');
                    if (typeof wrappedFunction === 'function') {
                      sample.wrappedFunction = wrappedFunction;
                      sample.wrappedFunctionConstructed = true;
                      sample.wrappedFunctionPrototype =
                          Object.getPrototypeOf(wrappedFunction);
                      try {
                        sample.wrappedFunctionCallResult = wrappedFunction();
                      } catch (e) {}
                    }
                  } catch (e) {}
                }
                globalThis.__heap_dump_js_wrapped_function_from_shadow_realm_exact_edge =
                    sample;
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
                              "__heap_dump_js_wrapped_function_from_shadow_realm_exact_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };

            v8::Local<v8::Value> wrapped_function_value =
                get_property("wrappedFunction");
            wrapped_function_constructed =
                get_property("wrappedFunctionConstructed")->BooleanValue(v8_isolate);
            if (!wrapped_function_constructed) {
              PrintF("[TODO] JS_WRAPPED_FUNCTION_TYPE is not constructible/stable "
                     "from ShadowRealm in this build; skipping marker.\n");
              return DumpArtifacts{};
            }

            v8::Local<v8::Value> wrapped_function_call_result =
                get_property("wrappedFunctionCallResult");
            if (!wrapped_function_call_result->IsNumber()) {
              PrintF(
                  "[TODO] ShadowRealm-wrapped function is not callable as expected; "
                  "skipping marker.\n");
              return DumpArtifacts{};
            }

            v8::Local<v8::Value> wrapped_function_prototype_value =
                get_property("wrappedFunctionPrototype");
            CHECK(wrapped_function_value->IsFunction());
            v8::Local<v8::Function> wrapped_function =
                wrapped_function_value.As<v8::Function>();
            CHECK(wrapped_function->GetPrototypeV2().As<v8::Object>()->IsObject());
            CHECK(wrapped_function->GetPrototypeV2()
                      .As<v8::Object>()
                      ->StrictEquals(wrapped_function_prototype_value));
            CHECK_EQ(7.0, wrapped_function_call_result.As<v8::Number>()->Value());
            CHECK_EQ(JS_WRAPPED_FUNCTION_TYPE,
                     LocalObjectInstanceType(wrapped_function_value));
            InstanceType wrapped_function_type =
                LocalObjectInstanceType(wrapped_function_value);
            if (wrapped_function_type == JS_WRAPPED_FUNCTION_TYPE) {
              wrapped_function_type_match = true;
            } else {
              PrintF(
                  "[TODO] JS_WRAPPED_FUNCTION_TYPE fixture is instance type %d "
                  "instead; skipping marker.\n",
                  wrapped_function_type);
              return DumpArtifacts{};
            }

            DirectHandle<JSObject> js_wrapped_function =
                Cast<JSObject>(
                    v8::Utils::OpenDirectHandle(*wrapped_function));
            CHECK_EQ(JS_WRAPPED_FUNCTION_TYPE,
                     js_wrapped_function->map()->instance_type());
            wrapped_function_address = js_wrapped_function->address();
            wrapped_function_map_address = js_wrapped_function->map()->address();
            prototype_address =
                LocalObjectAddress(wrapped_function_prototype_value);

            return DumpArtifacts{};
          });

  if (!wrapped_function_constructed || !wrapped_function_type_match) return;

  Node* wrapped_function =
      GetSnapshotNode(generator.get(), wrapped_function_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  Node* map = GetSnapshotNode(generator.get(), wrapped_function_map_address);
  CHECK_NOT_NULL(wrapped_function);
  CHECK_NOT_NULL(prototype);
  CHECK_NOT_NULL(map);
  CHECK(NodeHasType(wrapped_function, Node::Type::kClosure));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), wrapped_function, map);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), wrapped_function, "__proto__", prototype);
  MarkCoveredInstanceType(JS_WRAPPED_FUNCTION_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSPrimitiveWrapperExactPrototypeEdge) {
  Address wrapper_address = 0;
  Address wrapper_map_address = 0;
  Address prototype_address = 0;
  Address prototype_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const wrapper =
                    new String('heap-dump-js-primitive-wrapper-sample');
                const prototype = Object.getPrototypeOf(wrapper);
                if (!(wrapper instanceof String) ||
                    prototype !== String.prototype ||
                    wrapper.valueOf() !==
                        'heap-dump-js-primitive-wrapper-sample') {
                  throw new Error('unexpected String wrapper fixture');
                }
                const fixture = { wrapper, prototype };
                globalThis.__heap_dump_js_primitive_wrapper_exact_prototype_edge =
                    fixture;
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_primitive_wrapper_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> wrapper_value = get_property("wrapper");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            CHECK(wrapper_value->IsStringObject());
            v8::Local<v8::Object> wrapper_object =
                v8::Local<v8::Object>::Cast(wrapper_value);
            CHECK(wrapper_object->GetPrototypeV2()->StrictEquals(
                prototype_value));
            CHECK(wrapper_value.As<v8::StringObject>()
                      ->ValueOf()
                      ->Equals(context,
                               NewApiString(
                                   v8_isolate,
                                   "heap-dump-js-primitive-wrapper-sample"))
                      .FromJust());
            CHECK_EQ(JS_PRIMITIVE_WRAPPER_TYPE,
                     LocalObjectInstanceType(wrapper_value));
            DirectHandle<JSPrimitiveWrapper> js_wrapper =
                Cast<JSPrimitiveWrapper>(
                    v8::Utils::OpenDirectHandle(*wrapper_object));
            CHECK_EQ(JS_PRIMITIVE_WRAPPER_TYPE,
                     js_wrapper->map()->instance_type());
            wrapper_address = js_wrapper->address();
            wrapper_map_address = js_wrapper->map()->address();
            CHECK(prototype_value->IsObject());
            DirectHandle<JSObject> js_prototype =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(
                    *v8::Local<v8::Object>::Cast(prototype_value)));
            prototype_address = LocalObjectAddress(prototype_value);
            prototype_map_address = js_prototype->map()->address();

            return DumpArtifacts{};
          });

  Node* wrapper = GetSnapshotNode(generator.get(), wrapper_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  Node* wrapper_map = GetSnapshotNode(generator.get(), wrapper_map_address);
  Node* prototype_map = GetSnapshotNode(generator.get(), prototype_map_address);
  CHECK_NOT_NULL(wrapper);
  CHECK_NOT_NULL(prototype);
  CHECK_NOT_NULL(wrapper_map);
  CHECK_NOT_NULL(prototype_map);
  CHECK(NodeHasType(wrapper, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CHECK(NodeHasType(wrapper_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(prototype_map, Node::Type::kObjectShape));

  CHECK(HasExactNamedEdge(generator.get(), wrapper, Edge::Type::kProperty,
                         "__proto__", prototype));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), wrapper,
                                   Edge::Type::kProperty, "__proto__",
                                   prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(wrapper, Edge::Type::kHidden,
                                         prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(wrapper, Edge::Type::kInternal,
                                         prototype));
  CHECK(HasExactNamedEdge(generator.get(), wrapper, Edge::Type::kInternal,
                         "map", wrapper_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), wrapper,
                                   Edge::Type::kInternal, "map",
                                   wrapper_map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(wrapper, Edge::Type::kInternal,
                                         wrapper_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(wrapper, Edge::Type::kWeak,
                                         wrapper_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(wrapper, Edge::Type::kHidden,
                                         wrapper_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(wrapper, Edge::Type::kProperty,
                                         wrapper_map));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), prototype,
                                       prototype_map);
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), prototype,
                                   Edge::Type::kInternal, "map",
                                   prototype_map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(prototype, Edge::Type::kInternal,
                                         prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(prototype, Edge::Type::kWeak,
                                         prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(prototype, Edge::Type::kHidden,
                                         prototype_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(prototype, Edge::Type::kProperty,
                                         prototype_map));
  MarkCoveredInstanceType(JS_PRIMITIVE_WRAPPER_TYPE);
}

void CheckExactInternalMapEdgeNoWrongKind(SnapshotGenerator* generator,
                                         const Node* object,
                                         const Node* map) {
  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CHECK(HasExactNamedEdge(generator, object, Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator, object, Edge::Type::kInternal, "map",
                                   map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(object, Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kProperty, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kShortcut, map));
}

#if defined(V8_INTL_SUPPORT)

void CheckIntlObjectExactPrototypeEdge(const char* fixture_global_name,
                                       const char* script_source,
                                       InstanceType expected_type) {
  Address object_address = 0;
  Address prototype_address = 0;
  Address prototype_map_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, script_source);

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate, fixture_global_name))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> object_value = get_property("object");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            CHECK(object_value->IsObject());
            v8::Local<v8::Object> object =
                v8::Local<v8::Object>::Cast(object_value);
            CHECK(object->GetPrototypeV2()->StrictEquals(prototype_value));
            CHECK(prototype_value->IsObject());
            v8::Local<v8::Object> prototype =
                v8::Local<v8::Object>::Cast(prototype_value);
            DirectHandle<JSObject> js_prototype =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(*prototype));
            CHECK_EQ(expected_type, LocalObjectInstanceType(object_value));
            DirectHandle<JSObject> js_object =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(*object));
            CHECK_EQ(expected_type, js_object->map()->instance_type());
            object_address = js_object->address();
            map_address = js_object->map()->address();
            prototype_address = LocalObjectAddress(prototype_value);
            prototype_map_address = js_prototype->map()->address();

            return DumpArtifacts{};
          });

  Node* object = GetSnapshotNode(generator.get(), object_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  Node* prototype_map = GetSnapshotNode(generator.get(),
                                        prototype_map_address);
  CHECK_NOT_NULL(object);
  CHECK_NOT_NULL(prototype);
  CHECK_NOT_NULL(map);
  CHECK_NOT_NULL(prototype_map);
  CHECK(NodeHasType(object, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CHECK(NodeHasType(prototype_map, Node::Type::kObjectShape));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), object, map);
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), prototype,
                                      prototype_map);
  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(generator.get(), object,
                                                    "__proto__", prototype);
}

UNINITIALIZED_TEST(HeapDumpJSCollatorExactPrototypeEdge) {
  CheckIntlObjectExactPrototypeEdge(
      "__heap_dump_js_collator_exact_prototype_edge",
      R"(
        (function() {
          const object = new Intl.Collator('en-US', {
            usage: 'sort',
            sensitivity: 'base'
          });
          const prototype = Object.getPrototypeOf(object);
          const options = object.resolvedOptions();
          if (!(object instanceof Intl.Collator) ||
              prototype !== Intl.Collator.prototype ||
              object.compare('a', 'b') >= 0 ||
              options.locale.length === 0 ||
              options.usage !== 'sort') {
            throw new Error('unexpected Intl.Collator fixture');
          }
          globalThis.__heap_dump_js_collator_exact_prototype_edge = {
            object, prototype
          };
        })();
      )",
      JS_COLLATOR_TYPE);
  MarkCoveredInstanceType(JS_COLLATOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSDateTimeFormatExactPrototypeEdge) {
  CheckIntlObjectExactPrototypeEdge(
      "__heap_dump_js_date_time_format_exact_prototype_edge",
      R"(
        (function() {
          const object = new Intl.DateTimeFormat('en-US', {
            timeZone: 'UTC',
            year: 'numeric',
            month: '2-digit',
            day: '2-digit'
          });
          const prototype = Object.getPrototypeOf(object);
          const options = object.resolvedOptions();
          const parts = object.formatToParts(
              new Date(Date.UTC(2026, 4, 15, 0, 0, 0)));
          if (!(object instanceof Intl.DateTimeFormat) ||
              prototype !== Intl.DateTimeFormat.prototype ||
              options.timeZone !== 'UTC' ||
              !parts.some((part) =>
                  part.type === 'year' && part.value === '2026')) {
            throw new Error('unexpected Intl.DateTimeFormat fixture');
          }
          globalThis
              .__heap_dump_js_date_time_format_exact_prototype_edge = {
            object, prototype
          };
        })();
      )",
      JS_DATE_TIME_FORMAT_TYPE);
  MarkCoveredInstanceType(JS_DATE_TIME_FORMAT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSLocaleExactPrototypeEdge) {
  CheckIntlObjectExactPrototypeEdge(
      "__heap_dump_js_locale_exact_prototype_edge",
      R"(
        (function() {
          const object = new Intl.Locale('en-Latn-US-u-ca-gregory', {
            hourCycle: 'h23'
          });
          const prototype = Object.getPrototypeOf(object);
          const maximized = object.maximize();
          if (!(object instanceof Intl.Locale) ||
              prototype !== Intl.Locale.prototype ||
              object.language !== 'en' ||
              object.calendar !== 'gregory' ||
              object.hourCycle !== 'h23' ||
              !(maximized instanceof Intl.Locale)) {
            throw new Error('unexpected Intl.Locale fixture');
          }
          globalThis.__heap_dump_js_locale_exact_prototype_edge = {
            object, prototype
          };
        })();
      )",
      JS_LOCALE_TYPE);
  MarkCoveredInstanceType(JS_LOCALE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSNumberFormatExactPrototypeEdge) {
  CheckIntlObjectExactPrototypeEdge(
      "__heap_dump_js_number_format_exact_prototype_edge",
      R"(
        (function() {
          const object = new Intl.NumberFormat('en-US', {
            useGrouping: false,
            minimumFractionDigits: 1,
            maximumFractionDigits: 1
          });
          const prototype = Object.getPrototypeOf(object);
          const options = object.resolvedOptions();
          if (!(object instanceof Intl.NumberFormat) ||
              prototype !== Intl.NumberFormat.prototype ||
              object.format(1234.5) !== '1234.5' ||
              options.minimumFractionDigits !== 1 ||
              options.maximumFractionDigits !== 1) {
            throw new Error('unexpected Intl.NumberFormat fixture');
          }
          globalThis.__heap_dump_js_number_format_exact_prototype_edge = {
            object, prototype
          };
        })();
      )",
      JS_NUMBER_FORMAT_TYPE);
  MarkCoveredInstanceType(JS_NUMBER_FORMAT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSPluralRulesExactPrototypeEdge) {
  CheckIntlObjectExactPrototypeEdge(
      "__heap_dump_js_plural_rules_exact_prototype_edge",
      R"(
        (function() {
          const object = new Intl.PluralRules('en-US', { type: 'cardinal' });
          const prototype = Object.getPrototypeOf(object);
          const options = object.resolvedOptions();
          if (!(object instanceof Intl.PluralRules) ||
              prototype !== Intl.PluralRules.prototype ||
              object.select(1) !== 'one' ||
              object.select(2) !== 'other' ||
              options.type !== 'cardinal') {
            throw new Error('unexpected Intl.PluralRules fixture');
          }
          globalThis.__heap_dump_js_plural_rules_exact_prototype_edge = {
            object, prototype
          };
        })();
      )",
      JS_PLURAL_RULES_TYPE);
  MarkCoveredInstanceType(JS_PLURAL_RULES_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSDisplayNamesExactPrototypeEdge) {
  CheckIntlObjectExactPrototypeEdge(
      "__heap_dump_js_display_names_exact_prototype_edge",
      R"(
        (function() {
          const object = new Intl.DisplayNames(['en'], { type: 'region' });
          const prototype = Object.getPrototypeOf(object);
          const options = object.resolvedOptions();
          if (!(object instanceof Intl.DisplayNames) ||
              prototype !== Intl.DisplayNames.prototype ||
              object.of('US') !== 'United States' ||
              options.type !== 'region') {
            throw new Error('unexpected Intl.DisplayNames fixture');
          }
          globalThis.__heap_dump_js_display_names_exact_prototype_edge = {
            object, prototype
          };
        })();
      )",
      JS_DISPLAY_NAMES_TYPE);
  MarkCoveredInstanceType(JS_DISPLAY_NAMES_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSDurationFormatExactPrototypeEdge) {
  CheckIntlObjectExactPrototypeEdge(
      "__heap_dump_js_duration_format_exact_prototype_edge",
      R"(
        (function() {
          if (typeof Intl.DurationFormat !== 'function') {
            throw new Error('Intl.DurationFormat is not supported');
          }
          const object = new Intl.DurationFormat('en-US', { style: 'long' });
          const prototype = Object.getPrototypeOf(object);
          const options = object.resolvedOptions();
          const formatted = object.format({ hours: 1, minutes: 2 });
          const parts = object.formatToParts({ hours: 1, minutes: 2 });
          if (!(object instanceof Intl.DurationFormat) ||
              prototype !== Intl.DurationFormat.prototype ||
              options.style !== 'long' ||
              typeof formatted !== 'string' ||
              formatted.length === 0 ||
              !parts.some((part) => part.type === 'integer')) {
            throw new Error('unexpected Intl.DurationFormat fixture');
          }
          globalThis.__heap_dump_js_duration_format_exact_prototype_edge = {
            object, prototype
          };
        })();
      )",
      JS_DURATION_FORMAT_TYPE);
  MarkCoveredInstanceType(JS_DURATION_FORMAT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSListFormatExactPrototypeEdge) {
  CheckIntlObjectExactPrototypeEdge(
      "__heap_dump_js_list_format_exact_prototype_edge",
      R"(
        (function() {
          const object = new Intl.ListFormat('en-US', {
            style: 'long',
            type: 'conjunction'
          });
          const prototype = Object.getPrototypeOf(object);
          const options = object.resolvedOptions();
          const parts = object.formatToParts(['alpha', 'beta']);
          if (!(object instanceof Intl.ListFormat) ||
              prototype !== Intl.ListFormat.prototype ||
              object.format(['alpha', 'beta']) !== 'alpha and beta' ||
              options.type !== 'conjunction' ||
              options.style !== 'long' ||
              !parts.some((part) => part.type === 'literal')) {
            throw new Error('unexpected Intl.ListFormat fixture');
          }
          globalThis.__heap_dump_js_list_format_exact_prototype_edge = {
            object, prototype
          };
        })();
      )",
      JS_LIST_FORMAT_TYPE);
  MarkCoveredInstanceType(JS_LIST_FORMAT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSRelativeTimeFormatExactPrototypeEdge) {
  CheckIntlObjectExactPrototypeEdge(
      "__heap_dump_js_relative_time_format_exact_prototype_edge",
      R"(
        (function() {
          const object = new Intl.RelativeTimeFormat('en-US', {
            numeric: 'auto'
          });
          const prototype = Object.getPrototypeOf(object);
          const options = object.resolvedOptions();
          const parts = object.formatToParts(-1, 'day');
          if (!(object instanceof Intl.RelativeTimeFormat) ||
              prototype !== Intl.RelativeTimeFormat.prototype ||
              object.format(-1, 'day') !== 'yesterday' ||
              options.numeric !== 'auto' ||
              !parts.some((part) => part.type === 'literal')) {
            throw new Error('unexpected Intl.RelativeTimeFormat fixture');
          }
          globalThis
              .__heap_dump_js_relative_time_format_exact_prototype_edge = {
            object, prototype
          };
        })();
      )",
      JS_RELATIVE_TIME_FORMAT_TYPE);
  MarkCoveredInstanceType(JS_RELATIVE_TIME_FORMAT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSSegmenterExactPrototypeEdge) {
  CheckIntlObjectExactPrototypeEdge(
      "__heap_dump_js_segmenter_exact_prototype_edge",
      R"(
        (function() {
          const object = new Intl.Segmenter('en-US', { granularity: 'word' });
          const prototype = Object.getPrototypeOf(object);
          const options = object.resolvedOptions();
          const segments = Array.from(object.segment('hello world'));
          if (!(object instanceof Intl.Segmenter) ||
              prototype !== Intl.Segmenter.prototype ||
              options.granularity !== 'word' ||
              segments.length < 2 ||
              segments[0].segment !== 'hello' ||
              segments[0].isWordLike !== true) {
            throw new Error('unexpected Intl.Segmenter fixture');
          }
          globalThis.__heap_dump_js_segmenter_exact_prototype_edge = {
            object, prototype
          };
        })();
      )",
      JS_SEGMENTER_TYPE);
  MarkCoveredInstanceType(JS_SEGMENTER_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSSegmentsExactPrototypeEdge) {
  CheckIntlObjectExactPrototypeEdge(
      "__heap_dump_js_segments_exact_prototype_edge",
      R"(
        (function() {
          const segmenter =
              new Intl.Segmenter('en-US', { granularity: 'word' });
          const object = segmenter.segment('hello world');
          const prototype = Object.getPrototypeOf(object);
          const containing = object.containing(6);
          const iterator = object[Symbol.iterator]();
          const first = iterator.next();
          if (typeof object.containing !== 'function' ||
              typeof object[Symbol.iterator] !== 'function' ||
              Object.getPrototypeOf(segmenter.segment('again')) !==
                  prototype ||
              containing.segment !== 'world' ||
              containing.index !== 6 ||
              containing.isWordLike !== true ||
              first.done !== false ||
              first.value.segment !== 'hello' ||
              first.value.index !== 0 ||
              first.value.isWordLike !== true) {
            throw new Error('unexpected Intl.Segments fixture');
          }
          globalThis.__heap_dump_js_segments_exact_prototype_edge = {
            object, prototype
          };
        })();
      )",
      JS_SEGMENTS_TYPE);
  MarkCoveredInstanceType(JS_SEGMENTS_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSSegmentIteratorExactPrototypeEdge) {
  CheckIntlObjectExactPrototypeEdge(
      "__heap_dump_js_segment_iterator_exact_prototype_edge",
      R"(
        (function() {
          const segmenter =
              new Intl.Segmenter('en-US', { granularity: 'word' });
          const segments = segmenter.segment('hello world');
          const object = segments[Symbol.iterator]();
          const prototype = Object.getPrototypeOf(object);
          const first = object.next();
          const second = object.next();
          if (typeof segments[Symbol.iterator] !== 'function' ||
              Object.prototype.toString.call(object) !==
                  '[object Segmenter String Iterator]' ||
              Object.getPrototypeOf(
                  segmenter.segment('again')[Symbol.iterator]()) !==
                  prototype ||
              first.done !== false ||
              first.value.segment !== 'hello' ||
              first.value.index !== 0 ||
              first.value.isWordLike !== true ||
              second.done !== false ||
              second.value.segment !== ' ' ||
              second.value.index !== 5 ||
              second.value.isWordLike !== false) {
            throw new Error('unexpected Intl.SegmentIterator fixture');
          }
          globalThis.__heap_dump_js_segment_iterator_exact_prototype_edge = {
            object, prototype
          };
        })();
      )",
      JS_SEGMENT_ITERATOR_TYPE);
  MarkCoveredInstanceType(JS_SEGMENT_ITERATOR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSV8BreakIteratorExactPrototypeEdge) {
  CheckIntlObjectExactPrototypeEdge(
      "__heap_dump_js_v8_break_iterator_exact_prototype_edge",
      R"(
        (function() {
          if (typeof Intl.v8BreakIterator !== 'function') {
            throw new Error('Intl.v8BreakIterator is not supported');
          }
          const object = new Intl.v8BreakIterator(['en'], { type: 'word' });
          const prototype = Object.getPrototypeOf(object);
          const options = object.resolvedOptions();
          object.adoptText('hello world');
          const first = object.first();
          const next = object.next();
          const current = object.current();
          const breakType = object.breakType();
          if (!(object instanceof Intl.v8BreakIterator) ||
              prototype !== Intl.v8BreakIterator.prototype ||
              options.locale.length === 0 ||
              options.type !== 'word' ||
              typeof object.adoptText !== 'function' ||
              typeof object.first !== 'function' ||
              typeof object.next !== 'function' ||
              typeof object.current !== 'function' ||
              typeof object.breakType !== 'function' ||
              first !== 0 ||
              next !== 5 ||
              current !== 5 ||
              breakType !== 'letter') {
            throw new Error('unexpected Intl.v8BreakIterator fixture');
          }
          globalThis.__heap_dump_js_v8_break_iterator_exact_prototype_edge = {
            object, prototype
          };
        })();
      )",
      JS_V8_BREAK_ITERATOR_TYPE);
  MarkCoveredInstanceType(JS_V8_BREAK_ITERATOR_TYPE);
}

#else
UNINITIALIZED_TEST(HeapDumpJSCollatorExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSDateTimeFormatExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSLocaleExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSNumberFormatExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSPluralRulesExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSDisplayNamesExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSDurationFormatExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSListFormatExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSRelativeTimeFormatExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSSegmenterExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSSegmentsExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSSegmentIteratorExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSV8BreakIteratorExactPrototypeEdge) {}
#endif  // defined(V8_INTL_SUPPORT)

#else
UNINITIALIZED_TEST(HeapDumpJSDateExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSRegExpExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSRegExpPrototypeExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSRegExpConstructorExactPrototypeEdges) {}
UNINITIALIZED_TEST(HeapDumpJSPromiseExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSPromisePrototypeClassicUsageEdges) {}
UNINITIALIZED_TEST(HeapDumpJSPromiseConstructorExactPrototypeEdges) {}
UNINITIALIZED_TEST(HeapDumpJSErrorExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSDisposableStackBuiltinsExactPrototypeEdges) {}
UNINITIALIZED_TEST(HeapDumpJSDisposableStackBaseExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpJSRawJSONExactRawJSONPropertyEdge) {}
UNINITIALIZED_TEST(HeapDumpJSAtomicsSynchronizationPrimitiveExactPrototypeEdges) {}
UNINITIALIZED_TEST(HeapDumpJSPrimitiveWrapperExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSShadowRealmExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSWrappedFunctionFromShadowRealmExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSCollatorExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSDateTimeFormatExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSLocaleExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSNumberFormatExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSPluralRulesExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSDisplayNamesExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSDurationFormatExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSListFormatExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSRelativeTimeFormatExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSSegmenterExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSSegmentsExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSSegmentIteratorExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSV8BreakIteratorExactPrototypeEdge) {}
#endif

}  // namespace heap
}  // namespace internal
}  // namespace v8
