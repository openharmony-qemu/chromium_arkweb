#include "arkweb/chromium_ext/v8/dfx/heap_dump/tests/cctest/heap-dump-test-utils.h"
#include "test/common/flag-utils.h"

#include "include/v8-primitive-object.h"
#include "include/v8-primitive.h"
#include "src/objects/api-callbacks-inl.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/bigint.h"
#include "src/heap/factory.h"
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
#include "src/objects/string.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/scope-info.h"
#include "src/objects/struct-inl.h"
#include "src/objects/swiss-name-dictionary-inl.h"

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
  CHECK(HasExactNamedEdge(generator, object, Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator, object, Edge::Type::kInternal,
                                   "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(object, Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kProperty, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kShortcut, map));
}

bool HeapDumpAllowSpecialApiObjectAccess(
    v8::Local<v8::Context> accessing_context,
    v8::Local<v8::Object> accessed_object,
    v8::Local<v8::Value> data) {
  USE(accessing_context);
  USE(accessed_object);
  USE(data);
  return true;
}

void HeapDumpEmptyApiCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  USE(info);
}

UNINITIALIZED_TEST(HeapDumpJSObjectPrototypeExactEdge) {
  Address object_address = 0;
  Address object_map_address = 0;
  Address prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const proto = { proto_marker: 1 };
                const obj = Object.create(proto);
                obj.own_marker = { child: true };
                globalThis.__heap_dump_proto_edge = { obj, proto };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate, "__heap_dump_proto_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> object_value = get_property("obj");
            CHECK_EQ(JS_OBJECT_TYPE, LocalObjectInstanceType(object_value));
            auto object = Cast<JSObject>(
                v8::Utils::OpenDirectHandle(*object_value));
            CHECK_EQ(JS_OBJECT_TYPE, object->map()->instance_type());
            object_address = LocalObjectAddress(object_value);
            object_map_address = object->map()->address();
            prototype_address = LocalObjectAddress(get_property("proto"));

            return DumpArtifacts{};
          });

  Node* object = GetSnapshotNode(generator.get(), object_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  Node* object_map = GetSnapshotNode(generator.get(), object_map_address);
  CHECK_NOT_NULL(object);
  CHECK_NOT_NULL(prototype);
  CHECK_NOT_NULL(object_map);
  CHECK(NodeHasType(object, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CHECK(NodeHasType(object_map, Node::Type::kObjectShape));

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), object, object_map);

  CHECK(HasExactNamedEdge(generator.get(), object, Edge::Type::kProperty,
                          "__proto__", prototype));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), object,
                                   Edge::Type::kProperty, "__proto__",
                                   prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kHidden,
                                         prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kInternal,
                                         prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kWeak,
                                         prototype));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kShortcut,
                                         prototype));
  MarkCoveredInstanceType(JS_OBJECT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSObjectOwnDataPropertyEdges) {
  Address object_address = 0;
  Address first_address = 0;
  Address second_address = 0;
  std::string first_edge_name;
  std::string second_edge_name;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const first = { first_marker: 1 };
                const second = { second_marker: 2 };
                const obj = {};
                obj.heapDumpOwnFirst = first;
                obj.heapDumpOwnSecond = second;
                if (obj.heapDumpOwnFirst !== first ||
                    obj.heapDumpOwnSecond !== second) {
                  throw new Error('unexpected own property fixture');
                }
                globalThis.__heap_dump_own_data_property_edges = {
                  obj, first, second
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
                              "__heap_dump_own_data_property_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> object_value = get_property("obj");
            v8::Local<v8::Value> first_value = get_property("first");
            v8::Local<v8::Value> second_value = get_property("second");
            CHECK(object_value->IsObject());
            CHECK(first_value->IsObject());
            CHECK(second_value->IsObject());
            CHECK_EQ(JS_OBJECT_TYPE, LocalObjectInstanceType(object_value));
            object_address = LocalObjectAddress(object_value);
            first_address = LocalObjectAddress(first_value);
            second_address = LocalObjectAddress(second_value);

            DirectHandle<JSObject> js_object =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(
                    *v8::Local<v8::Object>::Cast(object_value)));
            CHECK(js_object->HasFastProperties());
            Tagged<DescriptorArray> descs =
                js_object->map()->instance_descriptors(isolate);
            DirectHandle<String> first_name =
                isolate->factory()->InternalizeUtf8String("heapDumpOwnFirst");
            DirectHandle<String> second_name =
                isolate->factory()->InternalizeUtf8String("heapDumpOwnSecond");
            for (InternalIndex i : js_object->map()->IterateOwnDescriptors()) {
              Tagged<Name> key = descs->GetKey(i);
              if (key == *first_name) {
                first_edge_name = "(property #" + std::to_string(i.as_int()) + ")";
              } else if (key == *second_name) {
                second_edge_name = "(property #" + std::to_string(i.as_int()) + ")";
              }
            }
            CHECK(!first_edge_name.empty());
            CHECK(!second_edge_name.empty());

            return DumpArtifacts{};
          });

  Node* object = GetSnapshotNode(generator.get(), object_address);
  Node* first = GetSnapshotNode(generator.get(), first_address);
  Node* second = GetSnapshotNode(generator.get(), second_address);
  CHECK_NOT_NULL(object);
  CHECK_NOT_NULL(first);
  CHECK_NOT_NULL(second);
  CHECK(NodeHasType(object, Node::Type::kObject));
  CHECK(NodeHasType(first, Node::Type::kObject));
  CHECK(NodeHasType(second, Node::Type::kObject));

  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), object, first_edge_name.c_str(), first);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), object, second_edge_name.c_str(), second);
  MarkCoveredInstanceType(JS_OBJECT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSObjectDescriptorDataPropertyEdge) {
  Address object_address = 0;
  Address target_address = 0;
  std::string edge_name;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<JSObject> object =
                isolate->factory()->NewJSObject(isolate->object_function());
            DirectHandle<JSObject> target =
                isolate->factory()->NewJSObject(isolate->object_function());
            DirectHandle<String> property_name =
                isolate->factory()->InternalizeUtf8String(
                    "heapDumpDescriptorDataTarget");
            DirectHandle<Map> map(object->map(), isolate);
            Descriptor descriptor =
                Descriptor::DataConstant(property_name, target, NONE);
            Map::EnsureDescriptorSlack(isolate, map, 1);
            map->AppendDescriptor(isolate, &descriptor);
            CHECK(object->HasFastProperties());

            InvokeAtomicMajorGC(isolate->heap());
            object_address = object->address();
            target_address = target->address();

            Tagged<DescriptorArray> descs =
                object->map()->instance_descriptors(isolate);
            bool found_property = false;
            for (InternalIndex i : object->map()->IterateOwnDescriptors()) {
              if (descs->GetKey(i) != *property_name) {
                continue;
              }
              PropertyDetails details = descs->GetDetails(i);
              CHECK_EQ(PropertyKind::kData, details.kind());
              CHECK_EQ(PropertyLocation::kDescriptor, details.location());
              Tagged<Object> value = descs->GetStrongValue(i);
              CHECK(IsHeapObject(value));
              CHECK_EQ(target_address, Cast<HeapObject>(value).address());
              edge_name = "(property #" + std::to_string(i.as_int()) + ")";
              found_property = true;
              break;
            }
            CHECK(found_property);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(object);
            artifacts.keep_alive.push_back(target);
            artifacts.keep_alive.push_back(property_name);
            return artifacts;
          });

  Node* object = GetSnapshotNode(generator.get(), object_address);
  Node* target = GetSnapshotNode(generator.get(), target_address);
  CHECK_NOT_NULL(object);
  CHECK_NOT_NULL(target);
  CHECK(NodeHasType(object, Node::Type::kObject));
  CHECK(NodeHasType(target, Node::Type::kObject));
  CHECK(!edge_name.empty());

  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), object, edge_name.c_str(), target);
  MarkCoveredInstanceType(JS_OBJECT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSObjectOwnAccessorPropertyEdges) {
  Address object_address = 0;
  Address accessor_pair_address = 0;
  Address getter_address = 0;
  Address setter_address = 0;
  std::string accessor_edge_name;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                function heapDumpOwnAccessorGetter() { return 17; }
                function heapDumpOwnAccessorSetter(value) {
                  globalThis.__heap_dump_own_accessor_set_value = value;
                }
                const obj = {};
                Object.defineProperty(obj, 'heapDumpOwnAccessor', {
                  configurable: true,
                  enumerable: true,
                  get: heapDumpOwnAccessorGetter,
                  set: heapDumpOwnAccessorSetter
                });
                if (obj.heapDumpOwnAccessor !== 17) {
                  throw new Error('unexpected own accessor fixture');
                }
                globalThis.__heap_dump_own_accessor_property_edges = {
                  obj,
                  getter: heapDumpOwnAccessorGetter,
                  setter: heapDumpOwnAccessorSetter
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
                              "__heap_dump_own_accessor_property_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };

            v8::Local<v8::Value> object_value = get_property("obj");
            v8::Local<v8::Value> getter_value = get_property("getter");
            v8::Local<v8::Value> setter_value = get_property("setter");
            CHECK(object_value->IsObject());
            CHECK(getter_value->IsFunction());
            CHECK(setter_value->IsFunction());
            CHECK_EQ(JS_OBJECT_TYPE, LocalObjectInstanceType(object_value));
            object_address = LocalObjectAddress(object_value);
            getter_address = LocalObjectAddress(getter_value);
            setter_address = LocalObjectAddress(setter_value);

            DirectHandle<JSObject> js_object =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(
                    *v8::Local<v8::Object>::Cast(object_value)));
            CHECK(js_object->HasFastProperties());
            Tagged<DescriptorArray> descs =
                js_object->map()->instance_descriptors(isolate);
            DirectHandle<String> accessor_name =
                isolate->factory()->InternalizeUtf8String(
                    "heapDumpOwnAccessor");
            bool found_accessor = false;
            for (InternalIndex i : js_object->map()->IterateOwnDescriptors()) {
              if (descs->GetKey(i) != *accessor_name) {
                continue;
              }
              PropertyDetails details = descs->GetDetails(i);
              CHECK_EQ(PropertyKind::kAccessor, details.kind());
              Tagged<Object> value = descs->GetStrongValue(i);
              CHECK(IsAccessorPair(value));
              Tagged<AccessorPair> accessors = Cast<AccessorPair>(value);
              CHECK_EQ(getter_address,
                       Cast<HeapObject>(accessors->getter())->address());
              CHECK_EQ(setter_address,
                       Cast<HeapObject>(accessors->setter())->address());
              accessor_pair_address = accessors.address();
              accessor_edge_name =
                  "(property #" + std::to_string(i.as_int()) + ")";
              found_accessor = true;
              break;
            }
            CHECK(found_accessor);

            return DumpArtifacts{};
          });

  Node* object = GetSnapshotNode(generator.get(), object_address);
  Node* accessor_pair = GetSnapshotNode(generator.get(), accessor_pair_address);
  Node* getter = GetSnapshotNode(generator.get(), getter_address);
  Node* setter = GetSnapshotNode(generator.get(), setter_address);
  CHECK_NOT_NULL(object);
  CHECK_NOT_NULL(accessor_pair);
  CHECK_NOT_NULL(getter);
  CHECK_NOT_NULL(setter);
  CHECK(NodeHasType(object, Node::Type::kObject));
  CHECK(NodeMatchesTypeAndName(generator.get(), accessor_pair,
                               Node::Type::kHidden, "system / AccessorPair"));
  CHECK(NodeHasType(getter, Node::Type::kClosure));
  CHECK(NodeHasType(setter, Node::Type::kClosure));
  CHECK(!accessor_edge_name.empty());

  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), object, accessor_edge_name.c_str(), accessor_pair);
  std::string getter_edge_name = "get " + accessor_edge_name;
  std::string setter_edge_name = "set " + accessor_edge_name;
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), object, getter_edge_name.c_str(), getter);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), object, setter_edge_name.c_str(), setter);
  MarkCoveredInstanceType(JS_OBJECT_TYPE);
  MarkCoveredInstanceType(ACCESSOR_PAIR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSObjectSwissDictionaryPropertyEdges) {
  if constexpr (!V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    PrintF("[TODO] SwissNameDictionary is disabled in this build.\n");
    return;
  } else {
    Address object_address = 0;
    Address target_address = 0;
    std::string edge_name;

    std::unique_ptr<SnapshotGenerator> generator =
        DumpAndTranslateWithIsolateTeardown(
            [&](Isolate* isolate, v8::Local<v8::Context> context) {
              RunScript(context, R"(
                (function() {
                  const target = { heap_dump_swiss_property_target: true };
                  const obj = {
                    heapDumpSwissPropertyTarget: target,
                    heapDumpSwissOther: { other: true }
                  };
                  if (obj.heapDumpSwissPropertyTarget !== target) {
                    throw new Error('unexpected Swiss property fixture');
                  }
                  globalThis.__heap_dump_swiss_property_edges = {
                    obj, target
                  };
                })();
              )");

              v8::Isolate* v8_isolate =
                  reinterpret_cast<v8::Isolate*>(isolate);
              v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                  context->Global()
                      ->Get(context,
                            NewApiString(
                                v8_isolate,
                                "__heap_dump_swiss_property_edges"))
                      .ToLocalChecked());
              auto get_property = [&](const char* name) {
                return fixture
                    ->Get(context, NewApiString(v8_isolate, name))
                    .ToLocalChecked();
              };
              v8::Local<v8::Value> object_value = get_property("obj");
              v8::Local<v8::Value> target_value = get_property("target");
              CHECK(object_value->IsObject());
              CHECK(target_value->IsObject());
              CHECK_EQ(JS_OBJECT_TYPE, LocalObjectInstanceType(object_value));
              Address current_target_address = LocalObjectAddress(target_value);

              DirectHandle<JSObject> js_object =
                  Cast<JSObject>(v8::Utils::OpenDirectHandle(
                      *v8::Local<v8::Object>::Cast(object_value)));
              JSObject::NormalizeProperties(isolate, js_object,
                                            KEEP_INOBJECT_PROPERTIES, 0,
                                            "heap dump Swiss property test");
              CHECK(js_object->map()->is_dictionary_map());

              DirectHandle<String> property_name =
                  isolate->factory()->InternalizeUtf8String(
                      "heapDumpSwissPropertyTarget");
              Tagged<SwissNameDictionary> dictionary =
                  js_object->property_dictionary_swiss();
              bool found_property = false;
              for (InternalIndex i : dictionary->IterateEntries()) {
                Tagged<Object> key = dictionary->KeyAt(i);
                if (key != *property_name) {
                  continue;
                }
                Tagged<Object> value = dictionary->ValueAt(i);
                CHECK(IsHeapObject(value));
                CHECK_EQ(current_target_address,
                         Cast<HeapObject>(value).address());
                CHECK_EQ(PropertyKind::kData, dictionary->DetailsAt(i).kind());
                edge_name =
                    "(property #" + std::to_string(i.as_int()) + ")";
                found_property = true;
                break;
              }
              CHECK(found_property);

              InvokeAtomicMajorGC(isolate->heap());
              object_address = js_object->address();
              target_address = LocalObjectAddress(target_value);
              return DumpArtifacts{};
            });

    Node* object = GetSnapshotNode(generator.get(), object_address);
    Node* target = GetSnapshotNode(generator.get(), target_address);
    CHECK_NOT_NULL(object);
    CHECK_NOT_NULL(target);
    CHECK(NodeHasType(object, Node::Type::kObject));
    CHECK(NodeHasType(target, Node::Type::kObject));
    CHECK(!edge_name.empty());

    CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
        generator.get(), object, edge_name.c_str(), target);
    MarkCoveredInstanceType(JS_OBJECT_TYPE);
    MarkCoveredInstanceType(SWISS_NAME_DICTIONARY_TYPE);
  }
}

UNINITIALIZED_TEST(HeapDumpSymbolDescriptionNameEdge) {
  Address symbol_address = 0;
  Address description_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const symbol = Symbol('heap-dump-symbol-description');
                if (symbol.description !== 'heap-dump-symbol-description') {
                  throw new Error('unexpected symbol fixture');
                }
                globalThis.__heap_dump_symbol_description_edge = { symbol };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_symbol_description_edge"))
                    .ToLocalChecked());
            v8::Local<v8::Value> symbol_value =
                fixture->Get(context, NewApiString(v8_isolate, "symbol"))
                    .ToLocalChecked();
            CHECK(symbol_value->IsSymbol());
            DirectHandle<Symbol> symbol =
                Cast<Symbol>(v8::Utils::OpenDirectHandle(*symbol_value));
            Tagged<PrimitiveHeapObject> description = symbol->description();
            CHECK(IsString(description));
            symbol_address = symbol->address();
            description_address = Cast<String>(description)->address();

            return DumpArtifacts{};
          });

  Node* symbol = GetSnapshotNode(generator.get(), symbol_address);
  Node* description = GetSnapshotNode(generator.get(), description_address);
  CHECK_NOT_NULL(symbol);
  CHECK_NOT_NULL(description);
  CHECK(NodeHasType(symbol, Node::Type::kSymbol));
  CHECK(NodeHasType(description, Node::Type::kString));

  CHECK(HasExactNamedEdge(generator.get(), symbol, Edge::Type::kInternal,
                          "name", description));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), symbol,
                                   Edge::Type::kInternal, "name",
                                   description));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(symbol, Edge::Type::kProperty,
                                         description));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(symbol, Edge::Type::kWeak, description));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(symbol, Edge::Type::kHidden,
                                         description));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(symbol, Edge::Type::kShortcut,
                                         description));
  MarkCoveredInstanceType(SYMBOL_TYPE);
}

UNINITIALIZED_TEST(HeapDumpPrivateNameSymbolExactNameEdge) {
  Address private_symbol_address = 0;
  Address private_name_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            DirectHandle<String> private_name =
                isolate->factory()->NewStringFromStaticChars(
                    "heap-dump-private-symbol");
            DirectHandle<Symbol> private_symbol =
                isolate->factory()->NewPrivateNameSymbol(private_name);

            CHECK(context
                      ->Global()
                      ->Set(context, NewApiString(v8_isolate, "__heap_dump_private_symbol"),
                            v8::Utils::ToLocal(private_symbol))
                      .FromMaybe(false));

            InvokeAtomicMajorGC(isolate->heap());

            private_symbol_address = private_symbol->address();
            private_name_address = private_name->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(private_symbol);
            artifacts.keep_alive.push_back(private_name);
            return artifacts;
          });

  Node* private_symbol = GetSnapshotNode(generator.get(), private_symbol_address);
  Node* private_name = GetSnapshotNode(generator.get(), private_name_address);
  CHECK_NOT_NULL(private_symbol);
  CHECK_NOT_NULL(private_name);
  CHECK(NodeMatchesTypeAndName(generator.get(), private_symbol,
                               Node::Type::kHidden, "private symbol"));
  CHECK(NodeHasType(private_name, Node::Type::kString));

  CHECK(HasExactNamedEdge(generator.get(), private_symbol,
                          Edge::Type::kInternal, "name", private_name));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), private_symbol,
                                   Edge::Type::kInternal, "name",
                                   private_name));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(private_symbol, Edge::Type::kInternal,
                                         private_name));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(private_symbol, Edge::Type::kHidden,
                                         private_name));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(private_symbol, Edge::Type::kWeak,
                                         private_name));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(private_symbol, Edge::Type::kProperty,
                                         private_name));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(private_symbol, Edge::Type::kShortcut,
                                         private_name));
  MarkCoveredInstanceType(SYMBOL_TYPE);
}

UNINITIALIZED_TEST(HeapDumpObjectPrototypeClassicUsageEdge) {
  Address object_prototype_address = 0;
  Address object_prototype_map_address = 0;
  Address ordinary_object_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const objectPrototype = Object.prototype;
                const ordinary = { marker: { nested: true } };
                const hasOwn = Function.prototype.call.bind(
                    objectPrototype.hasOwnProperty);
                if (Object.getPrototypeOf(objectPrototype) !== null ||
                    Object.getPrototypeOf(ordinary) !== objectPrototype ||
                    typeof objectPrototype.hasOwnProperty !== 'function' ||
                    !hasOwn(ordinary, 'marker')) {
                  throw new Error('unexpected Object.prototype fixture');
                }
                globalThis.__heap_dump_object_prototype_classic_usage_edge = {
                  objectPrototype, ordinary
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
                              "__heap_dump_object_prototype_classic_usage_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> object_prototype_value =
                get_property("objectPrototype");
            v8::Local<v8::Value> ordinary_object_value =
                get_property("ordinary");
            CHECK(object_prototype_value->IsObject());
            CHECK(ordinary_object_value->IsObject());
            CHECK_EQ(JS_OBJECT_PROTOTYPE_TYPE,
                     LocalObjectInstanceType(object_prototype_value));
            v8::Local<v8::Object> object_prototype_object =
                v8::Local<v8::Object>::Cast(object_prototype_value);
            v8::Local<v8::Object> ordinary_object =
                v8::Local<v8::Object>::Cast(ordinary_object_value);
            CHECK(ordinary_object->GetPrototypeV2()->StrictEquals(
                object_prototype_value));
            DirectHandle<JSObject> js_object_prototype =
                Cast<JSObject>(
                    v8::Utils::OpenDirectHandle(*object_prototype_object));
            CHECK_EQ(JS_OBJECT_PROTOTYPE_TYPE,
                     js_object_prototype->map()->instance_type());
            object_prototype_map_address = js_object_prototype->map()->address();
            object_prototype_address = js_object_prototype->address();
            ordinary_object_address = LocalObjectAddress(ordinary_object_value);

            return DumpArtifacts{};
          });

  Node* object_prototype =
      GetSnapshotNode(generator.get(), object_prototype_address);
  Node* object_prototype_map =
      GetSnapshotNode(generator.get(), object_prototype_map_address);
  Node* ordinary_object =
      GetSnapshotNode(generator.get(), ordinary_object_address);
  CHECK_NOT_NULL(object_prototype);
  CHECK_NOT_NULL(object_prototype_map);
  CHECK_NOT_NULL(ordinary_object);
  CHECK(NodeHasType(object_prototype, Node::Type::kObject));
  CHECK(NodeHasType(ordinary_object, Node::Type::kObject));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), object_prototype,
                                      object_prototype_map);

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), ordinary_object, "__proto__", object_prototype);
  MarkCoveredInstanceType(JS_OBJECT_PROTOTYPE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSProxyExactTargetAndHandlerEdges) {
  Address proxy_address = 0;
  Address proxy_map_address = 0;
  Address target_address = 0;
  Address handler_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const targetPrototype = { proto_marker: 303 };
                const target = Object.create(targetPrototype);
                const payload = { stable: true };
                target.payload = payload;
                const handler = {};
                const proxy = new Proxy(target, handler);
                if (Object.getPrototypeOf(proxy) !== targetPrototype ||
                    proxy.payload !== payload) {
                  throw new Error('unexpected Proxy fixture');
                }
                globalThis.__heap_dump_js_proxy_exact_edges = {
                  proxy, target, handler
                };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "__heap_dump_js_proxy_exact_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> proxy_value = get_property("proxy");
            v8::Local<v8::Value> target_value = get_property("target");
            v8::Local<v8::Value> handler_value = get_property("handler");
            CHECK(proxy_value->IsProxy());
            CHECK(target_value->IsObject());
            CHECK(handler_value->IsObject());
            CHECK_EQ(JS_PROXY_TYPE, LocalObjectInstanceType(proxy_value));
            DirectHandle<JSProxy> js_proxy =
                Cast<JSProxy>(v8::Utils::OpenDirectHandle(
                    *v8::Local<v8::Object>::Cast(proxy_value)));
            CHECK_EQ(JS_PROXY_TYPE, js_proxy->map()->instance_type());
            proxy_map_address = js_proxy->map()->address();
            target_address = LocalObjectAddress(target_value);
            handler_address = LocalObjectAddress(handler_value);
            CHECK(IsJSReceiver(js_proxy->target()));
            CHECK(IsJSReceiver(js_proxy->handler()));
            CHECK_EQ(target_address,
                     Cast<JSReceiver>(js_proxy->target())->address());
            CHECK_EQ(handler_address,
                     Cast<JSReceiver>(js_proxy->handler())->address());
            proxy_address = js_proxy->address();

            return DumpArtifacts{};
          });

  Node* proxy = GetSnapshotNode(generator.get(), proxy_address);
  Node* target = GetSnapshotNode(generator.get(), target_address);
  Node* handler = GetSnapshotNode(generator.get(), handler_address);
  Node* proxy_map = GetSnapshotNode(generator.get(), proxy_map_address);
  CHECK_NOT_NULL(proxy);
  CHECK_NOT_NULL(target);
  CHECK_NOT_NULL(handler);
  CHECK_NOT_NULL(proxy_map);
  CHECK(NodeHasType(target, Node::Type::kObject));
  CHECK(NodeHasType(handler, Node::Type::kObject));
  CHECK(NodeHasType(proxy_map, Node::Type::kObjectShape));

  CheckSingleHiddenEdgeToTargetNoPropertyOrInternalDuplicate(proxy, target);
  CheckSingleHiddenEdgeToTargetNoPropertyOrInternalDuplicate(proxy, handler);
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), proxy, proxy_map);
  MarkCoveredInstanceType(JS_PROXY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSApiObjectInternalFieldExactEdge) {
  Address api_object_address = 0;
  Address target_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::ObjectTemplate> object_template =
                v8::ObjectTemplate::New(v8_isolate);
            object_template->SetInternalFieldCount(1);

            v8::Local<v8::Object> api_object =
                object_template->NewInstance(context).ToLocalChecked();
            v8::Local<v8::Object> target = v8::Object::New(v8_isolate);
            CHECK_EQ(1, api_object->InternalFieldCount());
            api_object->SetInternalField(0, target);
            v8::Local<v8::Data> internal_field =
                api_object->GetInternalField(0);
            CHECK(internal_field->IsValue());
            CHECK(internal_field.As<v8::Value>()->StrictEquals(target));

            SetGlobalValue(context, "__heap_dump_api_internal_field_object",
                           api_object);
            SetGlobalValue(context, "__heap_dump_api_internal_field_target",
                           target);

            InvokeAtomicMajorGC(isolate->heap());

            api_object = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_api_internal_field_object"))
                    .ToLocalChecked());
            target = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_api_internal_field_target"))
                    .ToLocalChecked());
            internal_field = api_object->GetInternalField(0);
            CHECK(internal_field->IsValue());
            CHECK(internal_field.As<v8::Value>()->StrictEquals(target));
            CHECK_EQ(JS_API_OBJECT_TYPE, LocalObjectInstanceType(api_object));
            api_object_address = LocalObjectAddress(api_object);
            target_address = LocalObjectAddress(target);

            return DumpArtifacts{};
          });

  Node* api_object = GetSnapshotNode(generator.get(), api_object_address);
  Node* target = GetSnapshotNode(generator.get(), target_address);
  CHECK_NOT_NULL(api_object);
  CHECK_NOT_NULL(target);
  CHECK(NodeHasType(api_object, Node::Type::kObject));
  CHECK(NodeHasType(target, Node::Type::kObject));

  CHECK(HasExactNamedEdge(generator.get(), api_object, Edge::Type::kInternal,
                          "0", target));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), api_object,
                                   Edge::Type::kInternal, "0", target));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(api_object, Edge::Type::kInternal,
                                         target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(api_object, Edge::Type::kHidden,
                                         target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(api_object, Edge::Type::kWeak,
                                         target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(api_object, Edge::Type::kProperty,
                                         target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(api_object, Edge::Type::kShortcut,
                                         target));
  MarkCoveredInstanceType(JS_API_OBJECT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSLastDummyApiObjectExactMapEdge) {
  Address api_object_address = 0;
  Address api_object_map_address = 0;
  Address target_address = 0;
  FlagScope<bool> experimental_embedder_instance_types(
      &v8_flags.experimental_embedder_instance_types, true);

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::FunctionTemplate> function_template =
                v8::FunctionTemplate::New(
                    v8_isolate, HeapDumpEmptyApiCallback,
                    v8::Local<v8::Value>(),
                    v8::Local<v8::Signature>(), 0,
                    v8::ConstructorBehavior::kAllow,
                    v8::SideEffectType::kHasSideEffect, nullptr,
                    Internals::kLastEmbedderJSApiObjectType);
            function_template->InstanceTemplate()->SetInternalFieldCount(1);

            v8::Local<v8::Function> constructor =
                function_template->GetFunction(context).ToLocalChecked();
            v8::Local<v8::Object> api_object =
                constructor->NewInstance(context).ToLocalChecked();
            v8::Local<v8::Object> target = v8::Object::New(v8_isolate);
            CHECK_EQ(1, api_object->InternalFieldCount());
            api_object->SetInternalField(0, target);
            CHECK_EQ(JS_LAST_DUMMY_API_OBJECT_TYPE,
                     LocalObjectInstanceType(api_object));

            SetGlobalValue(context, "__heap_dump_last_dummy_api_object",
                           api_object);
            SetGlobalValue(context, "__heap_dump_last_dummy_api_target",
                           target);

            InvokeAtomicMajorGC(isolate->heap());

            api_object = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "__heap_dump_last_dummy_api_object"))
                    .ToLocalChecked());
            target = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "__heap_dump_last_dummy_api_target"))
                    .ToLocalChecked());
            v8::Local<v8::Data> internal_field =
                api_object->GetInternalField(0);
            CHECK(internal_field->IsValue());
            CHECK(internal_field.As<v8::Value>()->StrictEquals(target));
            DirectHandle<JSObject> js_object =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(*api_object));
            CHECK_EQ(JS_LAST_DUMMY_API_OBJECT_TYPE,
                     js_object->map()->instance_type());
            api_object_address = js_object->address();
            api_object_map_address = js_object->map()->address();
            target_address = LocalObjectAddress(target);

            return DumpArtifacts{};
          });

  Node* api_object = GetSnapshotNode(generator.get(), api_object_address);
  Node* api_object_map = GetSnapshotNode(generator.get(), api_object_map_address);
  Node* target = GetSnapshotNode(generator.get(), target_address);
  CHECK_NOT_NULL(api_object);
  CHECK_NOT_NULL(api_object_map);
  CHECK_NOT_NULL(target);
  CHECK(NodeHasType(api_object, Node::Type::kObject));
  CHECK(NodeHasType(api_object_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(target, Node::Type::kObject));

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), api_object,
                                       api_object_map);
  CHECK(HasExactNamedEdge(generator.get(), api_object, Edge::Type::kInternal,
                          "0", target));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), api_object,
                                   Edge::Type::kInternal, "0", target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(api_object, Edge::Type::kWeak,
                                         target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(api_object, Edge::Type::kHidden,
                                         target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(api_object, Edge::Type::kProperty,
                                         target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(api_object, Edge::Type::kShortcut,
                                         target));
  MarkCoveredInstanceType(JS_LAST_DUMMY_API_OBJECT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSSpecialApiObjectExactMapEdge) {
  Address api_object_address = 0;
  Address api_object_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::FunctionTemplate> function_template =
                v8::FunctionTemplate::New(v8_isolate);
            function_template->InstanceTemplate()->SetAccessCheckCallback(
                HeapDumpAllowSpecialApiObjectAccess);
            v8::Local<v8::Object> api_object =
                function_template->InstanceTemplate()->NewInstance(context)
                    .ToLocalChecked();

            CHECK(api_object->IsObject());
            CHECK_EQ(JS_SPECIAL_API_OBJECT_TYPE,
                     LocalObjectInstanceType(api_object));
            SetGlobalValue(context, "__heap_dump_special_api_object", api_object);

            InvokeAtomicMajorGC(isolate->heap());

            api_object = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "__heap_dump_special_api_object"))
                    .ToLocalChecked());
            DirectHandle<JSObject> js_object =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(*api_object));
            CHECK_EQ(JS_SPECIAL_API_OBJECT_TYPE, js_object->map()->instance_type());
            api_object_address = js_object->address();
            api_object_map_address = js_object->map()->address();

            return DumpArtifacts{};
          });

  Node* api_object = GetSnapshotNode(generator.get(), api_object_address);
  Node* api_object_map = GetSnapshotNode(generator.get(), api_object_map_address);
  CHECK_NOT_NULL(api_object);
  CHECK_NOT_NULL(api_object_map);
  CHECK(NodeHasType(api_object, Node::Type::kObject));
  CHECK(NodeHasType(api_object_map, Node::Type::kObjectShape));

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), api_object,
                                       api_object_map);
  MarkCoveredInstanceType(JS_SPECIAL_API_OBJECT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSExternalObjectExactMapEdge) {
  Address js_external_address = 0;
  Address js_external_map_address = 0;
  int payload = 123;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::External> api_external =
                v8::External::New(v8_isolate, &payload,
                                  v8::kExternalPointerTypeTagDefault);

            SetGlobalValue(context, "__heap_dump_external_object", api_external);

            InvokeAtomicMajorGC(isolate->heap());

            auto global_external = v8::Local<v8::External>::Cast(
                context->Global()
                    ->Get(context, NewApiString(v8_isolate, "__heap_dump_external_object"))
                    .ToLocalChecked());
            CHECK(global_external->IsExternal());
            CHECK_EQ(JS_EXTERNAL_OBJECT_TYPE,
                     LocalObjectInstanceType(global_external));
            DirectHandle<JSExternalObject> external =
                Cast<JSExternalObject>(
                    v8::Utils::OpenDirectHandle(*global_external));
            CHECK_EQ(JS_EXTERNAL_OBJECT_TYPE, external->map()->instance_type());

            js_external_address = external->address();
            js_external_map_address = external->map()->address();

            return DumpArtifacts{};
          });

  Node* js_external = GetSnapshotNode(generator.get(), js_external_address);
  Node* js_external_map = GetSnapshotNode(generator.get(), js_external_map_address);
  CHECK_NOT_NULL(js_external);
  CHECK_NOT_NULL(js_external_map);
  CHECK(NodeHasType(js_external, Node::Type::kObject));
  CHECK(NodeHasType(js_external_map, Node::Type::kObjectShape));

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), js_external,
                                       js_external_map);
  MarkCoveredInstanceType(JS_EXTERNAL_OBJECT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSObjectWithEmbedderSlotsExactInternalFieldEdges) {
  // Current product code does not map JS_OBJECT_WITH_EMBEDDER_SLOTS_TYPE to a
  // VisitorId. Constructing the exact map aborts in Factory::InitializeMap, so
  // this type must remain a no-marker blocker until product support returns.
  CHECK_NE(JS_OBJECT_WITH_EMBEDDER_SLOTS_TYPE, JS_API_OBJECT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpHeapNumberExactMapEdge) {
  Address heap_number_address = 0;
  Address heap_number_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<HeapNumber> heap_number =
                isolate->factory()->NewHeapNumber(7.25);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(heap_number);

            InvokeAtomicMajorGC(isolate->heap());

            CHECK_EQ(HEAP_NUMBER_TYPE, heap_number->map()->instance_type());
            heap_number_address = heap_number->address();
            Tagged<Map> heap_number_map = heap_number->map();
            heap_number_map_address = heap_number_map->address();

            return artifacts;
          });

  Node* heap_number = GetSnapshotNode(generator.get(), heap_number_address);
  Node* heap_number_map =
      GetSnapshotNode(generator.get(), heap_number_map_address);
  CHECK_NOT_NULL(heap_number);
  CHECK_NOT_NULL(heap_number_map);
  CHECK(NodeHasType(heap_number, Node::Type::kHeapNumber));
  CHECK(NodeHasType(heap_number_map, Node::Type::kObjectShape));

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), heap_number,
                                       heap_number_map);
  MarkCoveredInstanceType(HEAP_NUMBER_TYPE);
}

UNINITIALIZED_TEST(HeapDumpBigIntExactMapEdge) {
  Address bigint_address = 0;
  Address bigint_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              globalThis.__heap_dump_bigint_exact_map_edge = 20260403129n;
              if (globalThis.__heap_dump_bigint_exact_map_edge !==
                  20260403129n) {
                throw new Error('unexpected BigInt fixture');
              }
            )");

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Value> bigint_value =
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_bigint_exact_map_edge"))
                    .ToLocalChecked();
            CHECK(bigint_value->IsBigInt());
            CHECK(bigint_value->StrictEquals(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_bigint_exact_map_edge"))
                    .ToLocalChecked()));

            v8::Local<v8::BigInt> api_bigint =
                v8::Local<v8::BigInt>::Cast(bigint_value);
            bool lossless = false;
            CHECK_EQ(20260403129ULL, api_bigint->Uint64Value(&lossless));
            CHECK(lossless);
            CHECK_EQ(1, api_bigint->WordCount());
            int sign_bit = 0;
            int word_count = 1;
            uint64_t words[1] = {0};
            api_bigint->ToWordsArray(&sign_bit, &word_count, words);
            CHECK_EQ(0, sign_bit);
            CHECK_EQ(1, word_count);
            CHECK_EQ(20260403129ULL, words[0]);

            DirectHandle<BigInt> bigint =
                v8::Utils::OpenDirectHandle(*api_bigint);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(bigint);

            InvokeAtomicMajorGC(isolate->heap());

            CHECK_EQ(BIG_INT_BASE_TYPE, bigint->map()->instance_type());
            CHECK(!bigint->IsNegative());
            bool internal_lossless = false;
            CHECK_EQ(20260403129ULL, bigint->AsUint64(&internal_lossless));
            CHECK(internal_lossless);

            bigint_address = bigint->address();
            Tagged<Map> bigint_map = bigint->map();
            bigint_map_address = bigint_map->address();

            return artifacts;
          });

  Node* bigint = GetSnapshotNode(generator.get(), bigint_address);
  Node* bigint_map = GetSnapshotNode(generator.get(), bigint_map_address);
  CHECK_NOT_NULL(bigint);
  CHECK_NOT_NULL(bigint_map);
  CHECK(NodeHasTypeAndName(generator.get(), bigint, Node::Type::kBigInt,
                           "bigint"));
  CHECK(NodeHasType(bigint_map, Node::Type::kObjectShape));

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), bigint, bigint_map);
  MarkCoveredInstanceType(BIG_INT_BASE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpOddballExactRootValueEdges) {
  Address oddball_address = 0;
  Address oddball_map_address = 0;
  Address to_string_address = 0;
  Address type_of_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            SetGlobalValue(context, "__heap_dump_true_oddball",
                           v8::True(v8_isolate));
            v8::Local<v8::Value> api_true =
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "__heap_dump_true_oddball"))
                    .ToLocalChecked();
            CHECK(api_true->IsTrue());
            CHECK(api_true->StrictEquals(v8::True(v8_isolate)));

            DirectHandle<Oddball> oddball =
                Cast<Oddball>(v8::Utils::OpenDirectHandle(*api_true));
            CHECK_EQ(isolate->factory()->true_value()->address(),
                     oddball->address());

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(oddball);

            InvokeAtomicMajorGC(isolate->heap());

            CHECK_EQ(ODDBALL_TYPE, oddball->map()->instance_type());
            CHECK_EQ(Oddball::kTrue, oddball->kind());
            CHECK_EQ(1.0, oddball->to_number_raw());
            CHECK(IsString(oddball->to_string()));
            CHECK(IsString(oddball->type_of()));

            Tagged<Map> oddball_map = oddball->map();
            Tagged<String> to_string = oddball->to_string();
            Tagged<String> type_of = oddball->type_of();
            oddball_address = oddball->address();
            oddball_map_address = oddball_map->address();
            to_string_address = to_string->address();
            type_of_address = type_of->address();

            artifacts.keep_alive.push_back(DirectHandle<Map>(oddball_map,
                                                              isolate));
            artifacts.keep_alive.push_back(DirectHandle<String>(to_string,
                                                                 isolate));
            artifacts.keep_alive.push_back(DirectHandle<String>(type_of,
                                                                 isolate));

            return artifacts;
          });

  Node* oddball = GetSnapshotNode(generator.get(), oddball_address);
  Node* oddball_map = GetSnapshotNode(generator.get(), oddball_map_address);
  Node* to_string = GetSnapshotNode(generator.get(), to_string_address);
  Node* type_of = GetSnapshotNode(generator.get(), type_of_address);
  CHECK_NOT_NULL(oddball);
  CHECK_NOT_NULL(oddball_map);
  CHECK_NOT_NULL(to_string);
  CHECK_NOT_NULL(type_of);
  CHECK(NodeHasTypeAndName(generator.get(), oddball, Node::Type::kHidden,
                           "oddball"));
  CHECK(NodeHasType(oddball_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(to_string, Node::Type::kString));
  CHECK(NodeHasType(type_of, Node::Type::kString));

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), oddball, oddball_map);
  CHECK_EQ(1, CountExactIndexedEdges(oddball, Edge::Type::kHidden, 0,
                                     to_string));
  CHECK_EQ(1,
           CountExactIndexedEdges(oddball, Edge::Type::kHidden, 1, type_of));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(oddball, Edge::Type::kHidden,
                                         to_string));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(oddball, Edge::Type::kHidden,
                                         type_of));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(oddball, Edge::Type::kProperty,
                                         to_string));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(oddball, Edge::Type::kProperty,
                                         type_of));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(oddball, Edge::Type::kWeak,
                                         to_string));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(oddball, Edge::Type::kWeak, type_of));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(oddball, Edge::Type::kShortcut,
                                         to_string));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(oddball, Edge::Type::kShortcut, type_of));
  MarkCoveredInstanceType(ODDBALL_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSSharedArrayAndStructExactMapEdges) {
#if V8_CAN_CREATE_SHARED_HEAP_BOOL && !COMPRESS_POINTERS_IN_MULTIPLE_CAGES_BOOL
  FlagScope<bool> harmony_struct(&v8_flags.harmony_struct, true);
  FlagScope<bool> shared_string_table(&v8_flags.shared_string_table, true);
  FlagList::EnforceFlagImplications();

  Address shared_array_address = 0;
  Address shared_array_map_address = 0;
  Address shared_struct_address = 0;
  Address shared_struct_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const StructType = new SharedStructType(['field']);
                const sharedStruct = new StructType();
                const sharedArray = new SharedArray(2);
                Atomics.store(sharedArray, 0, sharedStruct);
                Atomics.store(sharedStruct, 'field', sharedArray);
                if (!SharedArray.isSharedArray(sharedArray) ||
                    !SharedStructType.isSharedStruct(sharedStruct) ||
                    Atomics.load(sharedArray, 0) !== sharedStruct ||
                    Atomics.load(sharedStruct, 'field') !== sharedArray) {
                  throw new Error('unexpected SharedArray/SharedStruct fixture');
                }
                globalThis.__heap_dump_js_shared_array_struct_exact_map_edges = {
                  sharedArray, sharedStruct
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
                              "__heap_dump_js_shared_array_struct_exact_map_edges"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };

            v8::Local<v8::Value> shared_array_value =
                get_property("sharedArray");
            v8::Local<v8::Value> shared_struct_value =
                get_property("sharedStruct");
            CHECK(shared_array_value->IsObject());
            CHECK(shared_struct_value->IsObject());
            CHECK_EQ(JS_SHARED_ARRAY_TYPE,
                     LocalObjectInstanceType(shared_array_value));
            CHECK_EQ(JS_SHARED_STRUCT_TYPE,
                     LocalObjectInstanceType(shared_struct_value));

            DirectHandle<JSObject> js_shared_array =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(
                    *v8::Local<v8::Object>::Cast(shared_array_value)));
            DirectHandle<JSObject> js_shared_struct =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(
                    *v8::Local<v8::Object>::Cast(shared_struct_value)));
            CHECK_EQ(JS_SHARED_ARRAY_TYPE,
                     js_shared_array->map()->instance_type());
            CHECK_EQ(JS_SHARED_STRUCT_TYPE,
                     js_shared_struct->map()->instance_type());

            shared_array_address = js_shared_array->address();
            shared_array_map_address = js_shared_array->map()->address();
            shared_struct_address = js_shared_struct->address();
            shared_struct_map_address = js_shared_struct->map()->address();

            return DumpArtifacts{};
          });

  Node* shared_array = GetSnapshotNode(generator.get(), shared_array_address);
  Node* shared_array_map =
      GetSnapshotNode(generator.get(), shared_array_map_address);
  Node* shared_struct = GetSnapshotNode(generator.get(), shared_struct_address);
  Node* shared_struct_map =
      GetSnapshotNode(generator.get(), shared_struct_map_address);
  CHECK_NOT_NULL(shared_array);
  CHECK_NOT_NULL(shared_array_map);
  CHECK_NOT_NULL(shared_struct);
  CHECK_NOT_NULL(shared_struct_map);
  CHECK(NodeHasType(shared_array, Node::Type::kObject));
  CHECK(NodeHasType(shared_struct, Node::Type::kObject));

  CHECK(HasExactNamedEdge(generator.get(), shared_array, Edge::Type::kInternal,
                          "map", shared_array_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), shared_array,
                                   Edge::Type::kInternal, "map",
                                   shared_array_map));
  CHECK(HasExactNamedEdge(generator.get(), shared_struct, Edge::Type::kInternal,
                          "map", shared_struct_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), shared_struct,
                                   Edge::Type::kInternal, "map",
                                   shared_struct_map));
  MarkCoveredInstanceType(JS_SHARED_ARRAY_TYPE);
  MarkCoveredInstanceType(JS_SHARED_STRUCT_TYPE);
#else
  PrintF(
      "[TODO] JS_SHARED_ARRAY_TYPE and JS_SHARED_STRUCT_TYPE require shared "
      "heap support without multiple pointer-compression cages in this build; "
      "skipping markers.\n");
#endif
}

UNINITIALIZED_TEST(HeapDumpSymbolExactNameEdge) {
  Address symbol_address = 0;
  Address description_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::String> description =
                NewApiString(v8_isolate, "heap-dump-symbol");
            v8::Local<v8::Symbol> api_symbol =
                v8::Symbol::New(v8_isolate, description);

            DirectHandle<Symbol> symbol =
                v8::Utils::OpenDirectHandle(*api_symbol);
            DirectHandle<String> description_string =
                v8::Utils::OpenDirectHandle(*description);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(symbol);
            artifacts.keep_alive.push_back(description_string);

            InvokeAtomicMajorGC(isolate->heap());

            CHECK(api_symbol->IsSymbol());
            CHECK(api_symbol->Description(v8_isolate)
                      ->StrictEquals(description));
            CHECK_EQ(SYMBOL_TYPE, symbol->map()->instance_type());
            CHECK(IsString(symbol->description()));
            CHECK_EQ(description_string->address(),
                     Cast<String>(symbol->description())->address());

            symbol_address = symbol->address();
            description_address =
                Cast<String>(symbol->description())->address();

            return artifacts;
          });

  Node* symbol = GetSnapshotNode(generator.get(), symbol_address);
  Node* description = GetSnapshotNode(generator.get(), description_address);
  CHECK_NOT_NULL(symbol);
  CHECK_NOT_NULL(description);
  CHECK(NodeMatchesTypeAndName(generator.get(), symbol, Node::Type::kSymbol,
                               "symbol"));
  CHECK(NodeHasType(description, Node::Type::kString));

  CHECK(HasExactNamedEdge(generator.get(), symbol, Edge::Type::kInternal,
                          "name", description));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), symbol,
                                   Edge::Type::kInternal, "name",
                                   description));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(symbol, Edge::Type::kInternal,
                                         description));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(symbol, Edge::Type::kHidden,
                                         description));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(symbol, Edge::Type::kProperty,
                                         description));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(symbol, Edge::Type::kWeak,
                                         description));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(symbol, Edge::Type::kShortcut,
                                         description));
  MarkCoveredInstanceType(SYMBOL_TYPE);
}

#else
UNINITIALIZED_TEST(HeapDumpJSObjectPrototypeExactEdge) {}
UNINITIALIZED_TEST(HeapDumpObjectPrototypeClassicUsageEdge) {}
UNINITIALIZED_TEST(HeapDumpJSProxyExactTargetAndHandlerEdges) {}
UNINITIALIZED_TEST(HeapDumpJSApiObjectInternalFieldExactEdge) {}
UNINITIALIZED_TEST(HeapDumpJSLastDummyApiObjectExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpJSExternalObjectExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpJSObjectWithEmbedderSlotsExactInternalFieldEdges) {}
UNINITIALIZED_TEST(HeapDumpHeapNumberExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpBigIntExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpOddballExactRootValueEdges) {}
UNINITIALIZED_TEST(HeapDumpJSSharedArrayAndStructExactMapEdges) {}
UNINITIALIZED_TEST(HeapDumpSymbolExactNameEdge) {}
#endif

}  // namespace heap
}  // namespace internal
}  // namespace v8
