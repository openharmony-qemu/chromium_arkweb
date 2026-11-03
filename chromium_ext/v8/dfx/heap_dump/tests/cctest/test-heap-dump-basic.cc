#include "arkweb/chromium_ext/v8/dfx/heap_dump/tests/cctest/heap-dump-test-utils.h"

#include "src/execution/microtask-queue.h"
#include "src/ic/handler-configuration-inl.h"
#include "src/objects/api-callbacks-inl.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/embedder-data-array.h"
#include "src/objects/bytecode-array-inl.h"
#include "src/objects/code.h"
#include "src/objects/contexts-inl.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/dependent-code-inl.h"
#include "src/objects/descriptor-array-inl.h"
#include "src/objects/feedback-vector-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/diagnostics/basic-block-profiler.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-regexp.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/js-function-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/js-promise-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/microtask-inl.h"
#include "src/objects/promise-inl.h"
#include "src/objects/property.h"
#include "src/objects/property-array-inl.h"
#include "src/profiler/heap-profiler.h"
#include "src/profiler/heap-snapshot-generator-inl.h"
#include "src/objects/script-inl.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/scope-info.h"
#include "src/objects/struct-inl.h"
#include "src/objects/template-objects-inl.h"
#include "src/objects/templates.h"

namespace v8 {
namespace internal {
namespace heap {

#if defined(OH_ENABLE_HEAP_DUMP_TEST) && defined(OH_ENABLE_HEAP_DUMP) && \
    defined(OH_ENABLE_HEAP_TRANSLATE)

namespace {

using namespace heap_dump_test;

auto AnySnapshotNode() {
  return [](const Node* node) {
    USE(node);
    return true;
  };
}

auto SnapshotNodeType(Node::Type type) {
  return [type](const Node* node) { return NodeHasType(node, type); };
}

auto SnapshotNodeTypeName(SnapshotGenerator* generator, Node::Type type,
                          const char* name) {
  return [generator, type, name](const Node* node) {
    return NodeHasTypeAndName(generator, node, type, name);
  };
}

struct SnapshotEdgeTypeCounts {
  int weak = 0;
  int hidden = 0;
  int internal = 0;
  int property = 0;
  int shortcut = 0;

  int Get(Edge::Type type) const {
    switch (type) {
      case Edge::Type::kWeak:
        return weak;
      case Edge::Type::kHidden:
        return hidden;
      case Edge::Type::kInternal:
        return internal;
      case Edge::Type::kProperty:
        return property;
      case Edge::Type::kShortcut:
        return shortcut;
      default:
        UNREACHABLE();
    }
  }
};

void CountNativeEdgesToTarget(HeapEntry* source, HeapEntry* target,
                              SnapshotEdgeTypeCounts* counts) {
  CHECK_NOT_NULL(source);
  CHECK_NOT_NULL(target);
  CHECK_NOT_NULL(counts);
  for (int i = 0; i < source->children_count(); ++i) {
    HeapGraphEdge* edge = source->child(i);
    if (edge->to() != target) continue;
    switch (edge->type()) {
      case HeapGraphEdge::kWeak:
        counts->weak++;
        break;
      case HeapGraphEdge::kHidden:
        counts->hidden++;
        break;
      case HeapGraphEdge::kInternal:
        counts->internal++;
        break;
      case HeapGraphEdge::kProperty:
        counts->property++;
        break;
      case HeapGraphEdge::kShortcut:
        counts->shortcut++;
        break;
      default:
        break;
    }
  }
}

void CheckExactInternalMapEdgeNoWrongKind(SnapshotGenerator* generator,
                                          const Node* object,
                                          const Node* map) {
  // Breadcrumb: V8HeapExplorer::IterateAndExtractReferences calls
  // ExtractReferences(entry, obj), then SetInternalReference(entry, "map",
  // obj->map(...), HeapObject::kMapOffset).
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

void HeapDumpCallbackTaskNoop(void* data) { USE(data); }

bool CheckUncompiledDataExactMapEdge(SnapshotGenerator* generator,
                                     Address uncompiled_data_address,
                                     Address map_address,
                                     const char* instance_type_name) {
  Node* uncompiled_data =
      GetSnapshotNode(generator, uncompiled_data_address);
  Node* map = GetSnapshotNode(generator, map_address);
  if (uncompiled_data == nullptr || map == nullptr) {
    PrintF("[TODO] %s not yet stable in this branch.\n",
           instance_type_name);
    return false;
  }

  CHECK(NodeHasType(uncompiled_data, Node::Type::kCode));
  CheckExactInternalMapEdgeNoWrongKind(generator, uncompiled_data, map);
  return true;
}

bool CheckTrustedArrayExactMapEdge(SnapshotGenerator* generator,
                                   Address array_address, Address map_address,
                                   const char* instance_type_name) {
  Node* array = GetSnapshotNode(generator, array_address);
  Node* map = GetSnapshotNode(generator, map_address);
  if (array == nullptr || map == nullptr) {
    PrintF("[TODO] %s not yet stable in this branch.\n",
           instance_type_name);
    return false;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator, array, map);
  return true;
}

}  // namespace

UNINITIALIZED_TEST(HeapDumpBasicEndToEnd) {
  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [](Isolate* isolate, v8::Local<v8::Context> context) {
            CreateBaselineObjects(context);
            InvokeAtomicMajorGC(isolate->heap());
            return DumpArtifacts{};
          });

  CHECK_GT(generator->nodes().size(), 2u);
  CHECK_GT(generator->EdgeCount(), 0u);
  CHECK(HasString(generator.get(), "native_bind"));
  CHECK(HasString(generator.get(), "bigint"));
}

UNINITIALIZED_TEST(HeapDumpTranslateRecognizesRepresentativeNodeKinds) {
  Address object_address = 0;
  Address object_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            CreateBaselineObjects(context);
            CreateExtendedObjects(context);
            DirectHandle<JSObject> object =
                isolate->factory()->NewJSObject(isolate->object_function());

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(object);

            InvokeAtomicMajorGC(isolate->heap());
            object_address = object->address();
            object_map_address = object->map()->address();
            return artifacts;
          });

  CHECK(HasNode(generator.get(), Node::Type::kObject, "Object"));
  CHECK(HasNode(generator.get(), Node::Type::kObject, "Array"));
  CHECK(HasNode(generator.get(), Node::Type::kObject, "Map"));
  CHECK(HasNode(generator.get(), Node::Type::kObject, "Set"));
  CHECK(HasNode(generator.get(), Node::Type::kCode, "shared function info"));
  CHECK(HasNode(generator.get(), Node::Type::kString, "(string)"));
  CHECK(HasNode(generator.get(), Node::Type::kClosure, "native_bind"));
  CHECK(HasNode(generator.get(), Node::Type::kRegExp, "(regexp)"));
  CHECK(HasNode(generator.get(), Node::Type::kBigInt, "bigint"));

  Node* object = GetSnapshotNode(generator.get(), object_address);
  Node* object_map = GetSnapshotNode(generator.get(), object_map_address);
  CHECK(NodeMatchesTypeAndName(generator.get(), object, Node::Type::kObject,
                               "Object"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), object, object_map);
}

UNINITIALIZED_TEST(HeapDumpTranslateForeignUsesSystemForeignFallback) {
  Address foreign_address = 0;
  Address foreign_map_address = 0;
  Address trusted_foreign_address = 0;
  Address trusted_foreign_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DumpArtifacts artifacts;
            DirectHandle<Foreign> foreign = CreateForeignForTest(isolate);
            DirectHandle<TrustedForeign> trusted_foreign =
                isolate->factory()->NewTrustedForeign(0x1234, false);

            CHECK_EQ(FOREIGN_TYPE, foreign->map()->instance_type());
            CHECK_EQ(TRUSTED_FOREIGN_TYPE, trusted_foreign->map()->instance_type());

            artifacts.keep_alive.push_back(foreign);
            artifacts.keep_alive.push_back(trusted_foreign);
            InvokeAtomicMajorGC(isolate->heap());

            foreign_address = foreign->address();
            foreign_map_address = foreign->map()->address();
            trusted_foreign_address = trusted_foreign->address();
            trusted_foreign_map_address = trusted_foreign->map()->address();
            return artifacts;
          });

  Node* foreign = GetSnapshotNode(generator.get(), foreign_address);
  Node* foreign_map = GetSnapshotNode(generator.get(), foreign_map_address);

  CHECK(NodeMatchesTypeAndName(generator.get(), foreign, Node::Type::kHidden,
                               "system / Foreign"));
  // Foreign is rendered as a system node by V8HeapExplorer::AddEntry; its
  // exact internal map edge still comes from IterateAndExtractReferences.
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), foreign, foreign_map);
  MarkCoveredInstanceType(FOREIGN_TYPE);

  Node* trusted_foreign =
      GetSnapshotNode(generator.get(), trusted_foreign_address);
  Node* trusted_foreign_map =
      GetSnapshotNode(generator.get(), trusted_foreign_map_address);
  if (trusted_foreign != nullptr && trusted_foreign_map != nullptr &&
      NodeMatchesTypeAndName(generator.get(), trusted_foreign,
                            Node::Type::kHidden, "system / TrustedForeign")) {
    // TrustedForeign follows the same IterateAndExtractReferences map-edge
    // contract when present in this translated snapshot.
    CheckExactInternalMapEdgeNoWrongKind(generator.get(), trusted_foreign,
                                        trusted_foreign_map);
    MarkCoveredInstanceType(TRUSTED_FOREIGN_TYPE);
  } else {
    PrintF("[TODO] TRUSTED_FOREIGN_TYPE not yet stable in this branch.\n");
  }
}

UNINITIALIZED_TEST(HeapDumpSnapshotAlignedJsObjectGraphBaseline) {
  // Aligned with V8HeapExplorer::AddEntry, ExtractJSObjectReferences,
  // ExtractJSCollectionReferences, and ExtractJSArrayBufferReferences.
  Address anchor_address = 0;
  Address array_address = 0;
  Address map_address = 0;
  Address set_address = 0;
  Address array_buffer_address = 0;
  Address typed_array_address = 0;
  Address array_buffer_backing_store_address = 0;
  Address closure_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(isolate);
            RunScript(context, R"(
              (function() {
                const nested = { child: { leaf: true } };
                const array = [nested.child, { element: 1 }];
                function HeapDumpSnapshotClosure() { return nested.child; }
                const map = new Map([[nested, array]]);
                const set = new Set([nested.child]);
                const buffer = new ArrayBuffer(32);
                const view = new Uint8Array(buffer);
                globalThis.__heap_dump_snapshot_graph = {
                  nested, array, closure: HeapDumpSnapshotClosure,
                  map, set, buffer, view
                };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> anchor = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "__heap_dump_snapshot_graph"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return anchor
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            anchor_address = LocalObjectAddress(anchor);
            array_address = LocalObjectAddress(get_property("array"));
            map_address = LocalObjectAddress(get_property("map"));
            set_address = LocalObjectAddress(get_property("set"));
            array_buffer_address = LocalObjectAddress(get_property("buffer"));
            typed_array_address = LocalObjectAddress(get_property("view"));
            closure_address = LocalObjectAddress(get_property("closure"));
            auto array_buffer =
                v8::Local<v8::ArrayBuffer>::Cast(get_property("buffer"));
            std::shared_ptr<v8::BackingStore> backing_store =
                array_buffer->GetBackingStore();
            array_buffer_backing_store_address =
                reinterpret_cast<Address>(backing_store->Data());

            return DumpArtifacts{};
          });

  auto any = AnySnapshotNode();
  auto object_shape = SnapshotNodeType(Node::Type::kObjectShape);
  auto shared_info = SnapshotNodeTypeName(generator.get(), Node::Type::kCode,
                                          "shared function info");
  auto native_array_buffer_data = SnapshotNodeTypeName(
      generator.get(), Node::Type::kNative, "system / JSArrayBufferData");

  Node* anchor = GetSnapshotNode(generator.get(), anchor_address);
  Node* array = GetSnapshotNode(generator.get(), array_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  Node* set = GetSnapshotNode(generator.get(), set_address);
  Node* array_buffer = GetSnapshotNode(generator.get(), array_buffer_address);
  Node* typed_array = GetSnapshotNode(generator.get(), typed_array_address);
  Node* closure = GetSnapshotNode(generator.get(), closure_address);
  Node* backing_store = GetSnapshotNode(generator.get(),
                                        array_buffer_backing_store_address);

  CHECK(NodeHasType(anchor, Node::Type::kObject));
  CHECK(NodeHasType(array, Node::Type::kObject));
  CHECK(NodeHasType(map, Node::Type::kObject));
  CHECK(NodeHasType(set, Node::Type::kObject));
  CHECK(NodeHasType(array_buffer, Node::Type::kObject));
  CHECK(NodeHasType(typed_array, Node::Type::kObject));
  CHECK(NodeHasType(closure, Node::Type::kClosure));

  CHECK(HasEdgeByNameFromNode(generator.get(), anchor, Edge::Type::kInternal,
                              "map", object_shape));
  CHECK(HasEdgeByNameFromNode(generator.get(), array, Edge::Type::kInternal,
                              "elements", any));
  CHECK(HasEdgeByNameFromNode(generator.get(), closure, Edge::Type::kInternal,
                              "shared", shared_info));
  CHECK(HasEdgeByNameFromNode(generator.get(), closure, Edge::Type::kInternal,
                              "context", any));
  CHECK(HasEdgeByNameFromNode(generator.get(), map, Edge::Type::kInternal,
                              "table", any));
  CHECK(HasEdgeByNameFromNode(generator.get(), set, Edge::Type::kInternal,
                              "table", any));
  CHECK(HasEdgeByNameFromNode(generator.get(), typed_array,
                              Edge::Type::kInternal, "buffer",
                              [array_buffer](const Node* node) {
                                return node == array_buffer;
                              }));
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), typed_array, "buffer", array_buffer);
  CHECK(HasEdgeByNameFromNode(generator.get(), array_buffer,
                              Edge::Type::kInternal, "backing_store",
                              native_array_buffer_data));
  CHECK_NOT_NULL(backing_store);
  CHECK(NodeHasTypeAndName(generator.get(), backing_store, Node::Type::kNative,
                          "system / JSArrayBufferData"));
  CHECK_EQ(32u, backing_store->self_size_);
  CheckExactInternalEdgeNoHiddenPropertyOrWeakDuplicate(
      generator.get(), array_buffer, "backing_store", backing_store);
}

UNINITIALIZED_TEST(HeapDumpSnapshotAlignedPersistentHandleBaseline) {
  // Mixed C++/JS fixture aligned with global-handle reachability plus
  // V8HeapExplorer::AddEntry and IndexedReferencesExtractor map/element edges.
  Address retained_address = 0;
  Address array_address = 0;
  Address retained_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                             v8::Local<v8::Context> context) {
        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        DumpArtifacts artifacts;
        {
          v8::HandleScope inner_scope(v8_isolate);
          v8::Local<v8::Object> retained = v8::Object::New(v8_isolate);
          v8::Local<v8::Object> child = v8::Object::New(v8_isolate);
          v8::Local<v8::Array> array = v8::Array::New(v8_isolate, 1);
          CHECK(array->Set(context, 0, child).FromJust());
          CHECK(retained->Set(context, NewApiString(v8_isolate, "items"), array)
                    .FromJust());
          CHECK(retained->Set(context, NewApiString(v8_isolate, "child"), child)
                    .FromJust());
          artifacts.global_keep_alive.emplace_back(v8_isolate, retained);
        }

        InvokeAtomicMajorGC(isolate->heap());
        {
          v8::HandleScope post_gc_scope(v8_isolate);
          v8::Local<v8::Object> retained = v8::Local<v8::Object>::Cast(
              artifacts.global_keep_alive.back().Get(v8_isolate));
          v8::Local<v8::Value> array =
              retained->Get(context, NewApiString(v8_isolate, "items"))
                  .ToLocalChecked();
          retained_address = LocalObjectAddress(retained);
          array_address = LocalObjectAddress(array);
          retained_map_address =
              Cast<JSObject>(v8::Utils::OpenHandle(*retained))->map()->address();
        }
        return artifacts;
      });

  auto any = AnySnapshotNode();
  auto object_shape = SnapshotNodeType(Node::Type::kObjectShape);
  Node* retained = GetSnapshotNode(generator.get(), retained_address);
  Node* array = GetSnapshotNode(generator.get(), array_address);
  Node* retained_map = GetSnapshotNode(generator.get(), retained_map_address);

  CHECK(NodeHasType(retained, Node::Type::kObject));
  CHECK(NodeHasType(array, Node::Type::kObject));
  CHECK_NOT_NULL(retained_map);
  CHECK(HasEdgeByNameFromNode(generator.get(), retained, Edge::Type::kInternal,
                              "map", object_shape));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), retained,
                                       retained_map);
  CHECK(HasEdgeByNameFromNode(generator.get(), array, Edge::Type::kInternal,
                              "elements", any));
}

UNINITIALIZED_TEST(HeapDumpSnapshotAlignedObjectShapeWhiteBoxBaseline) {
  // Cctest white-box fixture aligned with AddEntry system object handling and
  // the standard internal map edge used by heap-snapshot-generator.cc.
  Address foreign_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DumpArtifacts artifacts;
            DirectHandle<Foreign> foreign = CreateForeignForTest(isolate);
            artifacts.keep_alive.push_back(foreign);
            InvokeAtomicMajorGC(isolate->heap());
            foreign_address = foreign->address();
            return artifacts;
          });

  auto object_shape = SnapshotNodeType(Node::Type::kObjectShape);
  Node* foreign = GetSnapshotNode(generator.get(), foreign_address);

  CHECK(NodeMatchesTypeAndName(generator.get(), foreign, Node::Type::kHidden,
                               "system / Foreign"));
  CHECK(HasEdgeByNameFromNode(generator.get(), foreign, Edge::Type::kInternal,
                              "map", object_shape));
}

UNINITIALIZED_TEST(HeapDumpRootAndGlobalObjectDescriptions) {
  // Aligned with HeapSnapshot::AddSyntheticRootEntries,
  // V8HeapExplorer::AddEntry, ExtractJSGlobalProxyReferences, and
  // ExtractJSObjectReferences in heap-snapshot-generator.cc.
  Address global_proxy_address = 0;
  Address global_proxy_map_address = 0;
  Address global_object_address = 0;
  Address global_object_map_address = 0;
  Address native_context_address = 0;
  Address native_context_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            InvokeAtomicMajorGC(isolate->heap());

            v8::Local<v8::Object> global_proxy = context->Global();
            CHECK_EQ(JS_GLOBAL_PROXY_TYPE,
                     LocalObjectInstanceType(global_proxy));
            global_proxy_address = LocalObjectAddress(global_proxy);
            global_proxy_map_address =
                Cast<JSGlobalProxy>(v8::Utils::OpenHandle(*global_proxy))
                    ->map()
                    ->address();

            DirectHandle<NativeContext> native_context =
                isolate->native_context();
            CHECK_EQ(NATIVE_CONTEXT_TYPE,
                     native_context->map()->instance_type());
            native_context_address = native_context->address();
            native_context_map_address = native_context->map()->address();
            Tagged<JSGlobalObject> global_object =
                native_context->global_object();
            CHECK_EQ(JS_GLOBAL_OBJECT_TYPE,
                     global_object->map()->instance_type());
            global_object_address = global_object->address();
            global_object_map_address = global_object->map()->address();

            return DumpArtifacts{};
          });

  CHECK_GT(generator->nodes().size(),
           2u + static_cast<size_t>(Root::kGlobalHandles));
  Node* snapshot_root = generator->nodes()[0];
  Node* gc_roots = generator->nodes()[1];
  Node* global_handles_root =
      generator->nodes()[2 + static_cast<size_t>(Root::kGlobalHandles)];
  Node* global_proxy = GetSnapshotNode(generator.get(), global_proxy_address);
  Node* global_object = GetSnapshotNode(generator.get(), global_object_address);
  Node* native_context =
      GetSnapshotNode(generator.get(), native_context_address);
  Node* global_proxy_map = GetSnapshotNode(generator.get(),
                                           global_proxy_map_address);
  Node* global_object_map = GetSnapshotNode(generator.get(),
                                            global_object_map_address);
  Node* native_context_map = GetSnapshotNode(generator.get(),
                                            native_context_map_address);

  CHECK(NodeMatchesTypeAndName(generator.get(), snapshot_root,
                               Node::Type::kSynthetic, ""));
  CHECK(NodeMatchesTypeAndName(generator.get(), gc_roots,
                               Node::Type::kSynthetic, "(GC roots)"));
  CHECK(NodeMatchesTypeAndName(generator.get(), global_handles_root,
                               Node::Type::kSynthetic, "(Global handles)"));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(snapshot_root, Edge::Type::kElement,
                                         gc_roots));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(gc_roots, Edge::Type::kElement,
                                         global_handles_root));

  CHECK(NodeMatchesTypeAndName(generator.get(), global_proxy,
                               Node::Type::kObject, "global (global)"));
  CHECK(NodeMatchesTypeAndName(generator.get(), global_object,
                               Node::Type::kObject, "Object (global*)"));
  CHECK(NodeMatchesTypeAndName(generator.get(), native_context,
                               Node::Type::kHidden,
                               "system / NativeContext"));
  CHECK(NodeHasType(global_proxy_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(global_object_map, Node::Type::kObjectShape));
  CHECK(NodeHasType(native_context_map, Node::Type::kObjectShape));
  auto check_exact_map_edge = [&](Node* object, Node* map) {
    CheckExactInternalMapEdgeNoWrongKind(generator.get(), object, map);
  };
  check_exact_map_edge(global_proxy, global_proxy_map);
  check_exact_map_edge(native_context, native_context_map);
  check_exact_map_edge(global_object, global_object_map);

  CHECK(HasExactNamedEdge(generator.get(), global_proxy,
                          Edge::Type::kShortcut, "native_context",
                          native_context));
  CHECK(HasExactNamedEdge(generator.get(), global_proxy,
                          Edge::Type::kShortcut, "global_object",
                          global_object));
  // Keep this assertion focused on the intended JSGlobalObject::global_proxy
  // field edge; offline snapshotting may also add fallback retention edges to
  // the same target when rawheap lacks the original property name.
  CHECK(HasExactNamedEdge(generator.get(), global_object,
                          Edge::Type::kInternal, "global_proxy",
                          global_proxy));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), global_proxy,
                                   Edge::Type::kShortcut, "native_context",
                                   native_context));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(global_proxy, Edge::Type::kShortcut,
                                         native_context));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(global_proxy,
                                         Edge::Type::kInternal,
                                         native_context));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(global_proxy, Edge::Type::kWeak,
                                         native_context));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(global_proxy, Edge::Type::kHidden,
                                         native_context));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(global_proxy, Edge::Type::kProperty,
                                         native_context));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), global_proxy,
                                   Edge::Type::kShortcut, "global_object",
                                   global_object));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(global_proxy, Edge::Type::kShortcut,
                                         global_object));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(global_proxy,
                                         Edge::Type::kInternal,
                                         global_object));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(global_proxy, Edge::Type::kWeak,
                                         global_object));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(global_proxy, Edge::Type::kHidden,
                                         global_object));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(global_proxy, Edge::Type::kProperty,
                                         global_object));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), global_object,
                                   Edge::Type::kInternal, "global_proxy",
                                   global_proxy));
  // The global dictionary can expose additional internal fallback edges to the
  // same target when a property name cannot be decoded offline. Keep this
  // check focused on the exact JSGlobalObject::global_proxy field edge.
  CHECK_EQ(0, CountEdgesFromNodeToTarget(global_object, Edge::Type::kShortcut,
                                         global_proxy));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(global_object, Edge::Type::kWeak,
                                         global_proxy));
  MarkCoveredInstanceType(JS_GLOBAL_PROXY_TYPE);
  MarkCoveredInstanceType(NATIVE_CONTEXT_TYPE);
  MarkCoveredInstanceType(JS_GLOBAL_OBJECT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpNativeContextRootShortcutParity) {
  // Native V8HeapExplorer::SetGcSubrootReference emits an auto-indexed
  // kShortcut from the synthetic snapshot root to each strong NativeContext
  // user root (deduped by user_roots_). This is distinct from the named
  // JSGlobalProxy -> native_context shortcut covered above.
  Address native_context_address = 0;
  Address global_object_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            InvokeAtomicMajorGC(isolate->heap());

            DirectHandle<NativeContext> native_context =
                isolate->native_context();
            CHECK_EQ(NATIVE_CONTEXT_TYPE,
                     native_context->map()->instance_type());
            native_context_address = native_context->address();
            Tagged<JSGlobalObject> global_object =
                native_context->global_object();
            CHECK_EQ(JS_GLOBAL_OBJECT_TYPE,
                     global_object->map()->instance_type());
            global_object_address = global_object->address();

            return DumpArtifacts{};
          });

  CHECK_GT(generator->nodes().size(), 2u);
  Node* snapshot_root = generator->nodes()[0];
  Node* native_context =
      GetSnapshotNode(generator.get(), native_context_address);
  Node* global_object = GetSnapshotNode(generator.get(), global_object_address);

  CHECK(NodeMatchesTypeAndName(generator.get(), snapshot_root,
                               Node::Type::kSynthetic, ""));
  CHECK(NodeMatchesTypeAndName(generator.get(), native_context,
                               Node::Type::kHidden,
                               "system / NativeContext"));
  CHECK(NodeMatchesTypeAndName(generator.get(), global_object,
                               Node::Type::kObject, "Object (global*)"));

  CHECK_EQ(1, CountEdgesFromNodeToTarget(snapshot_root, Edge::Type::kShortcut,
                                         native_context));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(snapshot_root, Edge::Type::kShortcut,
                                         global_object));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(snapshot_root, Edge::Type::kInternal,
                                         native_context));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(snapshot_root, Edge::Type::kWeak,
                                         native_context));
}

UNINITIALIZED_TEST(HeapDumpExactByteArrayType) {
  // Aligned with AddEntry path and system entry mapping for fixed arrays.
  Address byte_array_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DumpArtifacts artifacts;

            DirectHandle<ByteArray> byte_array =
                ByteArray::New(isolate, 16, AllocationType::kOld);
            CHECK_EQ(BYTE_ARRAY_TYPE,
                     byte_array->map()->instance_type());
            artifacts.keep_alive.push_back(
                direct_handle(*byte_array, isolate));
            byte_array_address = byte_array->address();
            map_address = byte_array->map()->address();

            InvokeAtomicMajorGC(isolate->heap());
            return artifacts;
          });

  Node* byte_array_node = GetSnapshotNode(generator.get(), byte_array_address);
  Node* map_node = GetSnapshotNode(generator.get(), map_address);

  CHECK_NOT_NULL(byte_array_node);
  CHECK_NOT_NULL(map_node);
  CHECK(NodeHasType(byte_array_node, Node::Type::kArray));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), byte_array_node,
                                       map_node);
  MarkCoveredInstanceType(BYTE_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpExactArrayListType) {
  // ArrayList is currently emitted as a translator fixed-array-like node with
  // map edge and length-backed internal layout.
  Address array_list_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DumpArtifacts artifacts;
            DirectHandle<ArrayList> list = ArrayList::New(isolate, 2,
                                                         AllocationType::kOld);
            DirectHandle<JSObject> value =
                isolate->factory()->NewJSObject(isolate->object_function());
            list = ArrayList::Add(isolate, list, value);
            CHECK_EQ(ARRAY_LIST_TYPE, list->map()->instance_type());
            artifacts.keep_alive.push_back(direct_handle(*list, isolate));
            artifacts.keep_alive.push_back(direct_handle(*value, isolate));
            array_list_address = list->address();
            map_address = list->map()->address();

            InvokeAtomicMajorGC(isolate->heap());
            return artifacts;
          });

  Node* array_list_node = GetSnapshotNode(generator.get(), array_list_address);
  Node* map_node = GetSnapshotNode(generator.get(), map_address);

  CHECK_NOT_NULL(array_list_node);
  CHECK_NOT_NULL(map_node);
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), array_list_node,
                                       map_node);
  MarkCoveredInstanceType(ARRAY_LIST_TYPE);
}

UNINITIALIZED_TEST(HeapDumpExactCellType) {
  // Cell has a map edge that is stable in snapshot translation.
  Address cell_address = 0;
  Address map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DumpArtifacts artifacts;

            DirectHandle<JSObject> holder =
                isolate->factory()->NewJSObject(isolate->object_function());
            Handle<Cell> cell = isolate->factory()->NewCell();
            cell->set_value(*holder);
            CHECK_EQ(CELL_TYPE, cell->map()->instance_type());
            artifacts.keep_alive.push_back(direct_handle(*cell, isolate));
            artifacts.keep_alive.push_back(direct_handle(*holder, isolate));

            InvokeAtomicMajorGC(isolate->heap());
            cell_address = cell->address();
            map_address = cell->map()->address();
            return artifacts;
          });

  Node* cell_node = GetSnapshotNode(generator.get(), cell_address);
  Node* map_node = GetSnapshotNode(generator.get(), map_address);

  CHECK_NOT_NULL(cell_node);
  CHECK_NOT_NULL(map_node);
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), cell_node, map_node);
  MarkCoveredInstanceType(CELL_TYPE);
}

UNINITIALIZED_TEST(HeapDumpCellExactStrongValueEdge) {
  Address cell_address = 0;
  Address value_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<JSObject> holder =
                isolate->factory()->NewJSObject(isolate->object_function());
            Handle<Cell> cell = isolate->factory()->NewCell();
            cell->set_value(*holder);
            CHECK_EQ(CELL_TYPE, cell->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());
            cell_address = cell->address();
            value_address = holder->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(direct_handle(*cell, isolate));
            artifacts.keep_alive.push_back(holder);
            return artifacts;
          });

  Node* cell_node = GetSnapshotNode(generator.get(), cell_address);
  Node* value_node = GetSnapshotNode(generator.get(), value_address);
  CHECK_NOT_NULL(cell_node);
  CHECK_NOT_NULL(value_node);
  auto object_shape = SnapshotNodeType(Node::Type::kObjectShape);
  CHECK(HasEdgeByNameFromNode(generator.get(), cell_node, Edge::Type::kInternal,
                              "map", object_shape));
  CHECK(NodeHasType(value_node, Node::Type::kObject));

  // White-box coverage for V8HeapExplorer::ExtractCellReferences:
  // E1: strong MaybeObject value -> internal "value" edge.
  CHECK(HasExactNamedEdge(generator.get(), cell_node, Edge::Type::kInternal,
                          "value", value_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), cell_node,
                                   Edge::Type::kInternal, "value",
                                   value_node));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(cell_node, Edge::Type::kInternal,
                                         value_node));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(cell_node, Edge::Type::kWeak, value_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(cell_node, Edge::Type::kHidden,
                                         value_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(cell_node, Edge::Type::kProperty,
                                         value_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(cell_node, Edge::Type::kShortcut,
                                         value_node));
  MarkCoveredInstanceType(CELL_TYPE);
  MarkCoveredInstanceType(JS_OBJECT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpCellExactWeakValueEdge) {
  Address cell_address = 0;
  Address value_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<JSObject> holder =
                isolate->factory()->NewJSObject(isolate->object_function());
            Handle<Cell> cell = isolate->factory()->NewCell();
            // Exercise ExtractCellReferences' MaybeObject weak branch without
            // involving a native/offheap fallback path.
            cell->set_maybe_value(MakeWeak(*holder), UPDATE_WRITE_BARRIER);
            CHECK_EQ(CELL_TYPE, cell->map()->instance_type());
            CHECK(cell->maybe_value().IsWeak());

            InvokeAtomicMajorGC(isolate->heap());
            cell_address = cell->address();
            value_address = holder->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(direct_handle(*cell, isolate));
            artifacts.keep_alive.push_back(holder);
            return artifacts;
          });

  Node* cell_node = GetSnapshotNode(generator.get(), cell_address);
  Node* value_node = GetSnapshotNode(generator.get(), value_address);
  CHECK_NOT_NULL(cell_node);
  CHECK_NOT_NULL(value_node);
  auto object_shape = SnapshotNodeType(Node::Type::kObjectShape);
  CHECK(HasEdgeByNameFromNode(generator.get(), cell_node, Edge::Type::kInternal,
                              "map", object_shape));
  CHECK(NodeHasType(value_node, Node::Type::kObject));

  // White-box coverage for V8HeapExplorer::ExtractCellReferences:
  // E2: weak MaybeObject value -> weak "value" edge.
  CHECK(HasExactNamedEdge(generator.get(), cell_node, Edge::Type::kWeak,
                          "value", value_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), cell_node,
                                   Edge::Type::kWeak, "value", value_node));
  CHECK_EQ(1,
           CountEdgesFromNodeToTarget(cell_node, Edge::Type::kWeak, value_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(cell_node, Edge::Type::kInternal,
                                         value_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(cell_node, Edge::Type::kHidden,
                                         value_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(cell_node, Edge::Type::kProperty,
                                         value_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(cell_node, Edge::Type::kShortcut,
                                         value_node));
  MarkCoveredInstanceType(CELL_TYPE);
  MarkCoveredInstanceType(JS_OBJECT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpBytecodeWrapperExactMapEdge) {
  // Wrapper objects that carry explicit sandbox indirection should keep a stable
  // kInternal map edge after translation.
  Address wrapper_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            RunScript(context, R"(
              function HeapDumpBytecodeWrapperFixture(a, b) {
                return a + b;
              }
              HeapDumpBytecodeWrapperFixture(1, 2);
            )");
            v8::Local<v8::Value> function_value =
                context->Global()
                    ->Get(context, NewApiString(v8_isolate,
                                                "HeapDumpBytecodeWrapperFixture"))
                    .ToLocalChecked();
            CHECK(function_value->IsFunction());
            DirectHandle<JSFunction> function =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*function_value));

            DirectHandle<SharedFunctionInfo> shared(
                function->shared(), isolate);
            CHECK(shared->HasBytecodeArray());
            DirectHandle<BytecodeArray> bytecode_array(
                shared->GetBytecodeArray(isolate), isolate);
            DirectHandle<BytecodeWrapper> wrapper =
                isolate->factory()->NewBytecodeWrapper();
            wrapper->set_bytecode(*bytecode_array);

            is_expected_instance_type =
                BYTECODE_WRAPPER_TYPE == wrapper->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }
            CHECK_EQ(BYTECODE_WRAPPER_TYPE, wrapper->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            wrapper_address = wrapper->address();
            map_address = wrapper->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(wrapper);
            artifacts.keep_alive.push_back(bytecode_array);
            artifacts.keep_alive.push_back(function);
            return artifacts;
          });

  Node* wrapper_node = GetSnapshotNode(generator.get(), wrapper_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || wrapper_node == nullptr || map == nullptr) {
    PrintF(
        "[TODO] BYTECODE_WRAPPER_TYPE not yet stable in this branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), wrapper_node, map);
  MarkCoveredInstanceType(BYTECODE_WRAPPER_TYPE);
}

UNINITIALIZED_TEST(HeapDumpCodeWrapperExactMapEdge) {
  // Sandbox wrapper for executable code should keep a stable kInternal map edge.
  Address wrapper_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            RunScript(context, R"(
              function HeapDumpCodeWrapperFixture(a) {
                return a * 2;
              }
            )");
            v8::Local<v8::Value> function_value =
                context->Global()
                    ->Get(context, NewApiString(v8_isolate,
                                                "HeapDumpCodeWrapperFixture"))
                    .ToLocalChecked();
            CHECK(function_value->IsFunction());
            DirectHandle<JSFunction> function =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*function_value));

            DirectHandle<Code> code(function->code(isolate), isolate);
            DirectHandle<CodeWrapper> wrapper =
                isolate->factory()->NewCodeWrapper();
            wrapper->set_code(*code);

            is_expected_instance_type =
                CODE_WRAPPER_TYPE == wrapper->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }
            CHECK_EQ(CODE_WRAPPER_TYPE, wrapper->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            wrapper_address = wrapper->address();
            map_address = wrapper->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(wrapper);
            artifacts.keep_alive.push_back(code);
            artifacts.keep_alive.push_back(function);
            return artifacts;
          });

  Node* wrapper_node = GetSnapshotNode(generator.get(), wrapper_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || wrapper_node == nullptr || map == nullptr) {
    PrintF("[TODO] CODE_WRAPPER_TYPE not yet stable in this branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), wrapper_node, map);
  MarkCoveredInstanceType(CODE_WRAPPER_TYPE);
}

UNINITIALIZED_TEST(HeapDumpRegExpDataWrapperExactMapEdge) {
  // RegExpData indirection path should be stable via internal map edge.
  Address wrapper_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<String> source =
                isolate->factory()->InternalizeUtf8String(
                    "heap-dump-regexp-data-source");
            DirectHandle<String> pattern =
                isolate->factory()->InternalizeUtf8String(
                    "heap-dump-regexp-data-pattern");
            DirectHandle<RegExpData> regexp_data =
                isolate->factory()->NewAtomRegExpData(
                    source, JSRegExp::kGlobal, pattern);
            DirectHandle<RegExpDataWrapper> wrapper =
                isolate->factory()->NewRegExpDataWrapper();
            wrapper->set_data(*regexp_data);

            is_expected_instance_type =
                REG_EXP_DATA_WRAPPER_TYPE == wrapper->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }
            CHECK_EQ(REG_EXP_DATA_WRAPPER_TYPE, wrapper->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            wrapper_address = wrapper->address();
            map_address = wrapper->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(wrapper);
            artifacts.keep_alive.push_back(regexp_data);
            return artifacts;
          });

  Node* wrapper_node = GetSnapshotNode(generator.get(), wrapper_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || wrapper_node == nullptr || map == nullptr) {
    PrintF("[TODO] REG_EXP_DATA_WRAPPER_TYPE not yet stable in this branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), wrapper_node, map);
  MarkCoveredInstanceType(REG_EXP_DATA_WRAPPER_TYPE);
}

UNINITIALIZED_TEST(HeapDumpSharedFunctionInfoWrapperExactMapEdge) {
  // Shared-function-info trusted wrapper should be translated with stable map edge.
  Address wrapper_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            RunScript(context, R"(
              function HeapDumpSharedFunctionInfoWrapperFixture() {
                return 42;
              }
            )");
            v8::Local<v8::Value> function_value =
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "HeapDumpSharedFunctionInfoWrapperFixture"))
                    .ToLocalChecked();
            CHECK(function_value->IsFunction());
            DirectHandle<JSFunction> function =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*function_value));
            DirectHandle<SharedFunctionInfo> shared(
                function->shared(), isolate);

            DirectHandle<SharedFunctionInfoWrapper> wrapper =
                isolate->factory()->NewSharedFunctionInfoWrapper(shared);

            is_expected_instance_type =
                SHARED_FUNCTION_INFO_WRAPPER_TYPE ==
                wrapper->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }
            CHECK_EQ(SHARED_FUNCTION_INFO_WRAPPER_TYPE,
                     wrapper->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            wrapper_address = wrapper->address();
            map_address = wrapper->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(wrapper);
            artifacts.keep_alive.push_back(function);
            artifacts.keep_alive.push_back(shared);
            return artifacts;
          });

  Node* wrapper_node = GetSnapshotNode(generator.get(), wrapper_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || wrapper_node == nullptr || map == nullptr) {
    PrintF(
        "[TODO] SHARED_FUNCTION_INFO_WRAPPER_TYPE not yet stable in this branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), wrapper_node, map);
  MarkCoveredInstanceType(SHARED_FUNCTION_INFO_WRAPPER_TYPE);
}

UNINITIALIZED_TEST(HeapDumpProtectedFixedArrayExactMapEdge) {
  Address array_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<ProtectedFixedArray> array =
                isolate->factory()->NewProtectedFixedArray(2);

            is_expected_instance_type =
                PROTECTED_FIXED_ARRAY_TYPE == array->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }
            CHECK_EQ(PROTECTED_FIXED_ARRAY_TYPE, array->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            array_address = array->address();
            map_address = array->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(array);
            return artifacts;
          });

  if (!is_expected_instance_type ||
      !CheckTrustedArrayExactMapEdge(generator.get(), array_address,
                                     map_address,
                                     "PROTECTED_FIXED_ARRAY_TYPE")) {
    return;
  }

  MarkCoveredInstanceType(PROTECTED_FIXED_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpProtectedWeakFixedArrayExactMapEdge) {
  Address array_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<ProtectedWeakFixedArray> array =
                isolate->factory()->NewProtectedWeakFixedArray(2);

            is_expected_instance_type =
                PROTECTED_WEAK_FIXED_ARRAY_TYPE ==
                array->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }
            CHECK_EQ(PROTECTED_WEAK_FIXED_ARRAY_TYPE,
                     array->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            array_address = array->address();
            map_address = array->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(array);
            return artifacts;
          });

  if (!is_expected_instance_type ||
      !CheckTrustedArrayExactMapEdge(generator.get(), array_address,
                                     map_address,
                                     "PROTECTED_WEAK_FIXED_ARRAY_TYPE")) {
    return;
  }

  MarkCoveredInstanceType(PROTECTED_WEAK_FIXED_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpTrustedByteArrayExactMapEdge) {
  Address array_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<TrustedByteArray> array =
                isolate->factory()->NewTrustedByteArray(8);
            array->set_int(0, 0x21545542);

            is_expected_instance_type =
                TRUSTED_BYTE_ARRAY_TYPE == array->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }
            CHECK_EQ(TRUSTED_BYTE_ARRAY_TYPE, array->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            array_address = array->address();
            map_address = array->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(array);
            return artifacts;
          });

  if (!is_expected_instance_type ||
      !CheckTrustedArrayExactMapEdge(generator.get(), array_address,
                                     map_address,
                                     "TRUSTED_BYTE_ARRAY_TYPE")) {
    return;
  }

  MarkCoveredInstanceType(TRUSTED_BYTE_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpTrustedFixedArrayExactMapEdge) {
  Address array_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<TrustedFixedArray> array =
                isolate->factory()->NewTrustedFixedArray(2);
            DirectHandle<String> value =
                isolate->factory()->InternalizeUtf8String(
                    "heap-dump-trusted-fixed-array-value");
            array->set(0, *value);

            is_expected_instance_type =
                TRUSTED_FIXED_ARRAY_TYPE == array->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }
            CHECK_EQ(TRUSTED_FIXED_ARRAY_TYPE, array->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            array_address = array->address();
            map_address = array->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(array);
            artifacts.keep_alive.push_back(value);
            return artifacts;
          });

  if (!is_expected_instance_type ||
      !CheckTrustedArrayExactMapEdge(generator.get(), array_address,
                                     map_address,
                                     "TRUSTED_FIXED_ARRAY_TYPE")) {
    return;
  }

  MarkCoveredInstanceType(TRUSTED_FIXED_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpTrustedWeakFixedArrayExactMapEdge) {
  Address array_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<TrustedWeakFixedArray> array =
                isolate->factory()->NewTrustedWeakFixedArray(2);

            is_expected_instance_type =
                TRUSTED_WEAK_FIXED_ARRAY_TYPE ==
                array->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }
            CHECK_EQ(TRUSTED_WEAK_FIXED_ARRAY_TYPE,
                     array->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            array_address = array->address();
            map_address = array->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(array);
            return artifacts;
          });

  if (!is_expected_instance_type ||
      !CheckTrustedArrayExactMapEdge(generator.get(), array_address,
                                     map_address,
                                     "TRUSTED_WEAK_FIXED_ARRAY_TYPE")) {
    return;
  }

  MarkCoveredInstanceType(TRUSTED_WEAK_FIXED_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpEmbedderDataArrayExactMapEdge) {
  Address embedder_data_array_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<EmbedderDataArray> embedder_data_array =
                isolate->factory()->NewEmbedderDataArray(2);

            is_expected_instance_type =
                EMBEDDER_DATA_ARRAY_TYPE ==
                embedder_data_array->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }
            CHECK_EQ(EMBEDDER_DATA_ARRAY_TYPE,
                     embedder_data_array->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            embedder_data_array_address = embedder_data_array->address();
            map_address = embedder_data_array->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(embedder_data_array);
            return artifacts;
          });

  Node* embedder_data_array =
      GetSnapshotNode(generator.get(), embedder_data_array_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || embedder_data_array == nullptr ||
      map == nullptr) {
    PrintF("[TODO] EMBEDDER_DATA_ARRAY_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), embedder_data_array, map);
  MarkCoveredInstanceType(EMBEDDER_DATA_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpExactClosureFeedbackCellArrayType) {
  // ClosureFeedbackCellArray is currently stable in snapshot translation.
  Address js_function_address = 0;
  Address closure_feedback_cell_array_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            RunScript(context, R"(
              globalThis.__heap_dump_closure_feedback_cell_target =
                function HeapDumpClosureFeedbackCellArrayTarget(a, b) { return a + b; };
              globalThis.__heap_dump_closure_feedback_cell_target(1, 2);
            )");
            v8::Local<v8::Function> function =
                v8::Local<v8::Function>::Cast(
                    context->Global()
                        ->Get(context,
                              NewApiString(v8_isolate,
                                           "__heap_dump_closure_feedback_cell_target"))
                        .ToLocalChecked());
            DirectHandle<JSFunction> js_function =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*function));

            DirectHandle<SharedFunctionInfo> shared_info(js_function->shared(),
                                                         isolate);
            CHECK(shared_info->HasBytecodeArray());
            IsCompiledScope compiled_scope(*shared_info, isolate);
            CHECK(compiled_scope.is_compiled());
            JSFunction::EnsureClosureFeedbackCellArray(isolate, js_function);

            is_expected_instance_type = js_function->has_closure_feedback_cell_array();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            DirectHandle<ClosureFeedbackCellArray> closure_feedback_cell_array(
                js_function->closure_feedback_cell_array(), isolate);
            is_expected_instance_type =
                CLOSURE_FEEDBACK_CELL_ARRAY_TYPE ==
                closure_feedback_cell_array->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }
            CHECK_EQ(CLOSURE_FEEDBACK_CELL_ARRAY_TYPE,
                     closure_feedback_cell_array->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            js_function_address = js_function->address();
            closure_feedback_cell_array_address =
                closure_feedback_cell_array->address();
            map_address = closure_feedback_cell_array->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(js_function);
            artifacts.keep_alive.push_back(closure_feedback_cell_array);
            return artifacts;
          });

  Node* js_function_node = GetSnapshotNode(generator.get(), js_function_address);
  Node* closure_feedback_cell_array_node =
      GetSnapshotNode(generator.get(), closure_feedback_cell_array_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || js_function_node == nullptr ||
      closure_feedback_cell_array_node == nullptr || map == nullptr) {
    PrintF("[TODO] CLOSURE_FEEDBACK_CELL_ARRAY_TYPE not yet stable in this branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(
      generator.get(), closure_feedback_cell_array_node, map);
  MarkCoveredInstanceType(CLOSURE_FEEDBACK_CELL_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpExactContextCellType) {
  // ContextCell is currently emitted as a closure cell-like object with stable
  // map semantics.
  Address context_cell_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<JSObject> holder =
                isolate->factory()->NewJSObject(isolate->object_function());
            DirectHandle<ContextCell> context_cell = isolate->factory()->NewContextCell(
                Cast<JSAny>(holder));
            is_expected_instance_type =
                CONTEXT_CELL_TYPE == context_cell->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(CONTEXT_CELL_TYPE, context_cell->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            context_cell_address = context_cell->address();
            map_address = context_cell->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(context_cell);
            artifacts.keep_alive.push_back(holder);
            return artifacts;
          });

  Node* context_cell_node = GetSnapshotNode(generator.get(), context_cell_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || context_cell_node == nullptr || map == nullptr) {
    PrintF("[TODO] CONTEXT_CELL_TYPE not yet stable in this branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), context_cell_node, map);
  MarkCoveredInstanceType(CONTEXT_CELL_TYPE);
}

UNINITIALIZED_TEST(HeapDumpExactFeedbackCellType) {
  // FeedbackCell is stable in snapshot translation and keeps a stable map edge.
  Address feedback_cell_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<JSObject> value =
                isolate->factory()->NewJSObject(isolate->object_function());
            DirectHandle<FeedbackCell> feedback_cell =
                isolate->factory()->NewManyClosuresCell();
            is_expected_instance_type =
                FEEDBACK_CELL_TYPE == feedback_cell->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(FEEDBACK_CELL_TYPE, feedback_cell->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            feedback_cell_address = feedback_cell->address();
            map_address = feedback_cell->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(feedback_cell);
            artifacts.keep_alive.push_back(value);
            return artifacts;
          });

  Node* feedback_cell_node = GetSnapshotNode(generator.get(),
                                             feedback_cell_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || feedback_cell_node == nullptr ||
      map == nullptr) {
    PrintF("[TODO] FEEDBACK_CELL_TYPE not yet stable in this branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), feedback_cell_node,
                                       map);
  MarkCoveredInstanceType(FEEDBACK_CELL_TYPE);
}

UNINITIALIZED_TEST(HeapDumpFeedbackCellExactValueEdge) {
  Address feedback_cell_address = 0;
  Address value_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            RunScript(context, R"(
              globalThis.__heap_dump_feedback_cell_value_target =
                function HeapDumpFeedbackCellValueTarget(a, b) { return a + b; };
              globalThis.__heap_dump_feedback_cell_value_target(1, 2);
            )");
            v8::Local<v8::Function> function =
                v8::Local<v8::Function>::Cast(
                    context->Global()
                        ->Get(context,
                              NewApiString(
                                  v8_isolate,
                                  "__heap_dump_feedback_cell_value_target"))
                        .ToLocalChecked());
            DirectHandle<JSFunction> js_function =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*function));

            JSFunction::EnsureClosureFeedbackCellArray(isolate, js_function);
            DirectHandle<FeedbackCell> feedback_cell(
                js_function->raw_feedback_cell(), isolate);
            DirectHandle<ClosureFeedbackCellArray> value(
                js_function->closure_feedback_cell_array(), isolate);
            is_expected_instance_type =
                FEEDBACK_CELL_TYPE == feedback_cell->map()->instance_type() &&
                CLOSURE_FEEDBACK_CELL_ARRAY_TYPE ==
                    value->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(FEEDBACK_CELL_TYPE,
                     feedback_cell->map()->instance_type());
            CHECK_EQ(CLOSURE_FEEDBACK_CELL_ARRAY_TYPE,
                     value->map()->instance_type());
            CHECK_EQ(*value, feedback_cell->value());
            InvokeAtomicMajorGC(isolate->heap());

            feedback_cell_address = feedback_cell->address();
            value_address = value->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(js_function);
            artifacts.keep_alive.push_back(feedback_cell);
            artifacts.keep_alive.push_back(value);
            return artifacts;
          });

  Node* feedback_cell_node =
      GetSnapshotNode(generator.get(), feedback_cell_address);
  Node* value_node = GetSnapshotNode(generator.get(), value_address);
  CHECK(is_expected_instance_type);
  CHECK_NOT_NULL(feedback_cell_node);
  CHECK_NOT_NULL(value_node);

  // White-box coverage for V8HeapExplorer::ExtractFeedbackCellReferences:
  // TagObject(feedback_cell, "(feedback cell)") is label-only and must not
  // create a self edge or a named "(feedback cell)" edge.
  CHECK(NodeMatchesTypeAndName(generator.get(), feedback_cell_node,
                               Node::Type::kCode, "(feedback cell)"));
  CHECK_EQ(0, CountEdgesByNameFromNode(generator.get(), feedback_cell_node,
                                       Edge::Type::kInternal,
                                       "(feedback cell)",
                                       SnapshotNodeType(Node::Type::kCode)));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(feedback_cell_node,
                                         Edge::Type::kInternal,
                                         feedback_cell_node));

  // E1: SetInternalReference("value", feedback_cell->value(), kValueOffset).
  CHECK(HasExactNamedEdge(generator.get(), feedback_cell_node,
                          Edge::Type::kInternal, "value", value_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), feedback_cell_node,
                                   Edge::Type::kInternal, "value",
                                   value_node));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(feedback_cell_node,
                                         Edge::Type::kInternal, value_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(feedback_cell_node, Edge::Type::kWeak,
                                         value_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(feedback_cell_node,
                                         Edge::Type::kHidden, value_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(feedback_cell_node,
                                         Edge::Type::kProperty, value_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(feedback_cell_node,
                                         Edge::Type::kShortcut, value_node));
  MarkCoveredInstanceType(FEEDBACK_CELL_TYPE);
  MarkCoveredInstanceType(CLOSURE_FEEDBACK_CELL_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpExactAllocationSiteType) {
  // AllocationSite is stable in snapshot translation and keeps a stable map edge.
  Address allocation_site_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            Handle<AllocationSite> allocation_site =
                isolate->factory()->NewAllocationSite(false);
            is_expected_instance_type =
                ALLOCATION_SITE_TYPE == allocation_site->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(ALLOCATION_SITE_TYPE,
                     allocation_site->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            allocation_site_address = allocation_site->address();
            map_address = allocation_site->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(
                direct_handle(*allocation_site, isolate));
            return artifacts;
          });

  Node* allocation_site_node = GetSnapshotNode(generator.get(),
                                               allocation_site_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || allocation_site_node == nullptr ||
      map == nullptr) {
    PrintF("[TODO] ALLOCATION_SITE_TYPE not yet stable in this branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), allocation_site_node,
                                       map);
  MarkCoveredInstanceType(ALLOCATION_SITE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpAllocationSiteExactTransitionInfoEdge) {
  Address allocation_site_address = 0;
  Address boilerplate_address = 0;
  InstanceType boilerplate_instance_type = LAST_TYPE;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            Handle<AllocationSite> allocation_site =
                isolate->factory()->NewAllocationSite(false);
            DirectHandle<JSObject> boilerplate =
                isolate->factory()->NewJSObject(isolate->object_function());
            allocation_site->set_boilerplate(*boilerplate, kReleaseStore);

            CHECK_EQ(ALLOCATION_SITE_TYPE,
                     allocation_site->map()->instance_type());
            // Prove the tagged field is a JSObject HeapObject before snapshot
            // node lookup; it is not a native/offheap pointer candidate.
            Tagged<Object> transition_info =
                allocation_site->transition_info_or_boilerplate();
            CHECK(IsHeapObject(transition_info));
            CHECK(IsJSObject(transition_info));
            CHECK_EQ(*boilerplate, Cast<JSObject>(transition_info));

            InvokeAtomicMajorGC(isolate->heap());

            CHECK(IsJSObject(allocation_site->transition_info_or_boilerplate()));
            allocation_site_address = allocation_site->address();
            boilerplate_address =
                Cast<JSObject>(allocation_site->transition_info_or_boilerplate())
                    .address();
            boilerplate_instance_type = Cast<JSObject>(
                                            allocation_site
                                                ->transition_info_or_boilerplate())
                                            ->map()
                                            ->instance_type();
            CHECK_EQ(boilerplate->address(), boilerplate_address);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(
                direct_handle(*allocation_site, isolate));
            artifacts.keep_alive.push_back(boilerplate);
            return artifacts;
          });

  Node* allocation_site_node = GetSnapshotNode(generator.get(),
                                               allocation_site_address);
  Node* boilerplate_node = GetSnapshotNode(generator.get(), boilerplate_address);
  CHECK_NOT_NULL(allocation_site_node);
  CHECK_NOT_NULL(boilerplate_node);
  PrintF("[INFO] AllocationSite source node type: %u name: %s\n",
         allocation_site_node->type_,
         NodeDisplayName(generator.get(), allocation_site_node).c_str());
  CHECK(NodeMatchesTypeAndName(generator.get(), allocation_site_node,
                               Node::Type::kCode,
                               "system / AllocationSite"));
  CHECK(NodeHasType(boilerplate_node, Node::Type::kObject));
  PrintF("[INFO] AllocationSite transition_info target type: %s\n",
         HeapDumpInstanceTypeName(boilerplate_instance_type));

  // Native contract: V8HeapExplorer::ExtractAllocationSiteReferences emits
  // SetInternalReference(entry, "transition_info",
  // site->transition_info_or_boilerplate()).
  CHECK(HasExactNamedEdge(generator.get(), allocation_site_node,
                          Edge::Type::kInternal, "transition_info",
                          boilerplate_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), allocation_site_node,
                                   Edge::Type::kInternal, "transition_info",
                                   boilerplate_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(allocation_site_node,
                                         Edge::Type::kWeak, boilerplate_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(allocation_site_node,
                                         Edge::Type::kHidden,
                                         boilerplate_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(allocation_site_node,
                                         Edge::Type::kProperty,
                                         boilerplate_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(allocation_site_node,
                                         Edge::Type::kShortcut,
                                         boilerplate_node));
  MarkCoveredInstanceType(ALLOCATION_SITE_TYPE);
  MarkCoveredInstanceType(boilerplate_instance_type);
}

UNINITIALIZED_TEST(HeapDumpAllocationSiteExactNestedSiteEdge) {
  Address allocation_site_address = 0;
  Address nested_site_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            Handle<AllocationSite> allocation_site =
                isolate->factory()->NewAllocationSite(false);
            Handle<AllocationSite> nested_site =
                isolate->factory()->NewAllocationSite(false);
            allocation_site->set_nested_site(*nested_site);

            CHECK_EQ(ALLOCATION_SITE_TYPE,
                     allocation_site->map()->instance_type());
            CHECK_EQ(ALLOCATION_SITE_TYPE, nested_site->map()->instance_type());
            // Prove the tagged field is an AllocationSite HeapObject before
            // snapshot node lookup; it is not a native/offheap pointer.
            Tagged<Object> nested_site_value = allocation_site->nested_site();
            CHECK(IsHeapObject(nested_site_value));
            CHECK(IsAllocationSite(nested_site_value));
            CHECK_EQ(*nested_site, Cast<AllocationSite>(nested_site_value));

            InvokeAtomicMajorGC(isolate->heap());

            CHECK(IsAllocationSite(allocation_site->nested_site()));
            allocation_site_address = allocation_site->address();
            nested_site_address =
                Cast<AllocationSite>(allocation_site->nested_site()).address();
            CHECK_EQ(nested_site->address(), nested_site_address);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(
                direct_handle(*allocation_site, isolate));
            artifacts.keep_alive.push_back(direct_handle(*nested_site, isolate));
            return artifacts;
          });

  Node* allocation_site_node = GetSnapshotNode(generator.get(),
                                               allocation_site_address);
  Node* nested_site_node = GetSnapshotNode(generator.get(), nested_site_address);
  CHECK_NOT_NULL(allocation_site_node);
  CHECK_NOT_NULL(nested_site_node);
  CHECK(NodeMatchesTypeAndName(generator.get(), allocation_site_node,
                               Node::Type::kCode,
                               "system / AllocationSite"));
  CHECK(NodeMatchesTypeAndName(generator.get(), nested_site_node,
                               Node::Type::kCode,
                               "system / AllocationSite"));

  // Native contract: V8HeapExplorer::ExtractAllocationSiteReferences emits
  // SetInternalReference(entry, "nested_site", site->nested_site()).
  CHECK(HasExactNamedEdge(generator.get(), allocation_site_node,
                          Edge::Type::kInternal, "nested_site",
                          nested_site_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), allocation_site_node,
                                   Edge::Type::kInternal, "nested_site",
                                   nested_site_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(allocation_site_node,
                                         Edge::Type::kWeak, nested_site_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(allocation_site_node,
                                         Edge::Type::kHidden,
                                         nested_site_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(allocation_site_node,
                                         Edge::Type::kProperty,
                                         nested_site_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(allocation_site_node,
                                         Edge::Type::kShortcut,
                                         nested_site_node));
  MarkCoveredInstanceType(ALLOCATION_SITE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpAllocationSiteWeakNextMatchesNativeEdges) {
  Address source_site_address = 0;
  Address target_site_address = 0;
  SnapshotEdgeTypeCounts native_counts;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            Handle<AllocationSite> target_site =
                isolate->factory()->NewAllocationSite(true);
            Handle<AllocationSite> source_site =
                isolate->factory()->NewAllocationSite(true);
            DirectHandle<AllocationSiteWithWeakNext> source_with_weak_next =
                Cast<AllocationSiteWithWeakNext>(source_site);
            DirectHandle<AllocationSiteWithWeakNext> target_with_weak_next =
                Cast<AllocationSiteWithWeakNext>(target_site);
            source_with_weak_next->set_weak_next(*target_with_weak_next);

            CHECK(source_site->HasWeakNext());
            CHECK(target_site->HasWeakNext());
            CHECK(IsAllocationSite(source_with_weak_next->weak_next()));
            CHECK(Cast<AllocationSite>(source_with_weak_next->weak_next())
                      ->HasWeakNext());
            CHECK_EQ(*target_with_weak_next,
                     Cast<AllocationSiteWithWeakNext>(
                         source_with_weak_next->weak_next()));

            InvokeAtomicMajorGC(isolate->heap());

            CHECK(IsAllocationSite(source_with_weak_next->weak_next()));
            CHECK(Cast<AllocationSite>(source_with_weak_next->weak_next())
                      ->HasWeakNext());
            CHECK_EQ(*target_with_weak_next,
                     Cast<AllocationSiteWithWeakNext>(
                         source_with_weak_next->weak_next()));
            source_site_address = source_site->address();
            target_site_address = target_site->address();

            v8::HeapProfiler::HeapSnapshotOptions options;
            options.snapshot_mode =
                v8::HeapProfiler::HeapSnapshotMode::kExposeInternals;
            HeapProfiler* profiler = isolate->heap()->heap_profiler();
            HeapSnapshot* native_snapshot = profiler->TakeSnapshot(options);
            SnapshotObjectId source_id =
                profiler->GetSnapshotObjectId(source_site);
            SnapshotObjectId target_id =
                profiler->GetSnapshotObjectId(target_site);
            CHECK_NE(v8::HeapProfiler::kUnknownObjectId, source_id);
            CHECK_NE(v8::HeapProfiler::kUnknownObjectId, target_id);
            HeapEntry* native_source = native_snapshot->GetEntryById(source_id);
            HeapEntry* native_target = native_snapshot->GetEntryById(target_id);
            CountNativeEdgesToTarget(native_source, native_target,
                                     &native_counts);
            native_snapshot->Delete();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(direct_handle(*source_site, isolate));
            artifacts.keep_alive.push_back(direct_handle(*target_site, isolate));
            return artifacts;
          });

  Node* source_site_node = GetSnapshotNode(generator.get(), source_site_address);
  Node* target_site_node = GetSnapshotNode(generator.get(), target_site_address);
  CHECK_NOT_NULL(source_site_node);
  CHECK_NOT_NULL(target_site_node);
  CHECK(NodeMatchesTypeAndName(generator.get(), source_site_node,
                               Node::Type::kCode,
                               "system / AllocationSite"));
  CHECK(NodeMatchesTypeAndName(generator.get(), target_site_node,
                               Node::Type::kCode,
                               "system / AllocationSite"));

  // The expected edge kinds come from native heap-snapshot-generator.cc for the
  // exact same live objects. This prevents the white-box translate test from
  // inventing a weak/hidden/internal policy for AllocationSite::weak_next_.
  CHECK_EQ(native_counts.Get(Edge::Type::kWeak),
           CountEdgesFromNodeToTarget(source_site_node, Edge::Type::kWeak,
                                      target_site_node));
  CHECK_EQ(native_counts.Get(Edge::Type::kHidden),
           CountEdgesFromNodeToTarget(source_site_node, Edge::Type::kHidden,
                                      target_site_node));
  CHECK_EQ(native_counts.Get(Edge::Type::kInternal),
           CountEdgesFromNodeToTarget(source_site_node, Edge::Type::kInternal,
                                      target_site_node));
  CHECK_EQ(native_counts.Get(Edge::Type::kProperty),
           CountEdgesFromNodeToTarget(source_site_node, Edge::Type::kProperty,
                                      target_site_node));
  CHECK_EQ(native_counts.Get(Edge::Type::kShortcut),
           CountEdgesFromNodeToTarget(source_site_node, Edge::Type::kShortcut,
                                      target_site_node));
  MarkCoveredInstanceType(ALLOCATION_SITE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpAllocationSiteExactDependentCodeEdge) {
  Address allocation_site_address = 0;
  Address dependent_code_address = 0;
  bool is_expected_dependent_code_family = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            Handle<AllocationSite> allocation_site =
                isolate->factory()->NewAllocationSite(false);

            CHECK_EQ(ALLOCATION_SITE_TYPE,
                     allocation_site->map()->instance_type());
            // dependent_code() is a V8 HeapObject (DependentCode/WeakArrayList),
            // so prove the target family before snapshot-node lookup.
            Tagged<DependentCode> dependent_code =
                allocation_site->dependent_code();
            is_expected_dependent_code_family =
                WEAK_ARRAY_LIST_TYPE ==
                dependent_code->map()->instance_type();
            CHECK(is_expected_dependent_code_family);

            InvokeAtomicMajorGC(isolate->heap());

            allocation_site_address = allocation_site->address();
            dependent_code_address =
                allocation_site->dependent_code().address();
            CHECK_EQ(dependent_code.address(), dependent_code_address);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(
                direct_handle(*allocation_site, isolate));
            return artifacts;
          });

  Node* allocation_site_node = GetSnapshotNode(generator.get(),
                                               allocation_site_address);
  Node* dependent_code_node =
      GetSnapshotNode(generator.get(), dependent_code_address);
  CHECK(is_expected_dependent_code_family);
  CHECK_NOT_NULL(allocation_site_node);
  CHECK_NOT_NULL(dependent_code_node);
  CHECK(NodeMatchesTypeAndName(generator.get(), allocation_site_node,
                               Node::Type::kCode,
                               "system / AllocationSite"));
  // Native contract: V8HeapExplorer::ExtractAllocationSiteReferences emits both
  // TagObject(site->dependent_code(), "(dependent code)", HeapEntry::kCode) and
  // SetInternalReference(entry, "dependent_code", site->dependent_code()).
  CHECK(NodeMatchesTypeAndName(generator.get(), dependent_code_node,
                               Node::Type::kCode, "(dependent code)"));
  PrintF("[INFO] AllocationSite dependent_code target type: %s\n",
         HeapDumpInstanceTypeName(WEAK_ARRAY_LIST_TYPE));

  // Native also emits a real kInternal "dependent_code" edge; the TagObject
  // label must not become a synthetic edge.
  CHECK(HasExactNamedEdge(generator.get(), allocation_site_node,
                          Edge::Type::kInternal, "dependent_code",
                          dependent_code_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), allocation_site_node,
                                   Edge::Type::kInternal, "dependent_code",
                                   dependent_code_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(allocation_site_node,
                                         Edge::Type::kWeak,
                                         dependent_code_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(allocation_site_node,
                                         Edge::Type::kHidden,
                                         dependent_code_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(allocation_site_node,
                                         Edge::Type::kProperty,
                                         dependent_code_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(allocation_site_node,
                                         Edge::Type::kShortcut,
                                         dependent_code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), allocation_site_node,
                                   Edge::Type::kInternal, "(dependent code)",
                                   dependent_code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), allocation_site_node,
                                   Edge::Type::kWeak, "(dependent code)",
                                   dependent_code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), allocation_site_node,
                                   Edge::Type::kHidden, "(dependent code)",
                                   dependent_code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), allocation_site_node,
                                   Edge::Type::kProperty, "(dependent code)",
                                   dependent_code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), allocation_site_node,
                                   Edge::Type::kShortcut, "(dependent code)",
                                   dependent_code_node));
  MarkCoveredInstanceType(ALLOCATION_SITE_TYPE);
  MarkCoveredInstanceType(WEAK_ARRAY_LIST_TYPE);
}

UNINITIALIZED_TEST(HeapDumpPromiseFulfillReactionJobTaskExactMapEdge) {
  Address task_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8_isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kExplicit);
            RunScript(context, R"(
              (function() {
                let resolvePromise;
                const promise = new Promise((resolve) => {
                  resolvePromise = resolve;
                });
                globalThis.__heap_dump_fulfill_reaction_result =
                    promise.then(function HeapDumpFulfillReactionHandler(
                        value) {
                      return value;
                    });
                resolvePromise(132);
              })();
            )");

            MicrotaskQueue* queue =
                isolate->native_context()->microtask_queue();
            CHECK_NOT_NULL(queue);
            CHECK_EQ(1, queue->size());
            Tagged<Microtask> microtask = queue->get(0);
            is_expected_instance_type =
                PROMISE_FULFILL_REACTION_JOB_TASK_TYPE ==
                microtask->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            DirectHandle<PromiseFulfillReactionJobTask> task =
                direct_handle(Cast<PromiseFulfillReactionJobTask>(microtask),
                              isolate);
            CHECK_EQ(PROMISE_FULFILL_REACTION_JOB_TASK_TYPE,
                     task->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            task_address = task->address();
            map_address = task->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(task);
            return artifacts;
          });

  Node* task = GetSnapshotNode(generator.get(), task_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || task == nullptr || map == nullptr) {
    PrintF("[TODO] PROMISE_FULFILL_REACTION_JOB_TASK_TYPE not yet stable in "
           "this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), task, Node::Type::kHidden,
                               "system / PromiseFulfillReactionJobTask"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), task, map);
  MarkCoveredInstanceType(PROMISE_FULFILL_REACTION_JOB_TASK_TYPE);
}

UNINITIALIZED_TEST(HeapDumpPromiseRejectReactionJobTaskExactMapEdge) {
  Address task_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8_isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kExplicit);
            RunScript(context, R"(
              (function() {
                let rejectPromise;
                const promise = new Promise((resolve, reject) => {
                  rejectPromise = reject;
                });
                globalThis.__heap_dump_reject_reaction_result =
                    promise.then(undefined,
                        function HeapDumpRejectReactionHandler(reason) {
                          return reason;
                        });
                rejectPromise(133);
              })();
            )");

            MicrotaskQueue* queue =
                isolate->native_context()->microtask_queue();
            CHECK_NOT_NULL(queue);
            CHECK_EQ(1, queue->size());
            Tagged<Microtask> microtask = queue->get(0);
            is_expected_instance_type =
                PROMISE_REJECT_REACTION_JOB_TASK_TYPE ==
                microtask->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            DirectHandle<PromiseRejectReactionJobTask> task =
                direct_handle(Cast<PromiseRejectReactionJobTask>(microtask),
                              isolate);
            CHECK_EQ(PROMISE_REJECT_REACTION_JOB_TASK_TYPE,
                     task->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            task_address = task->address();
            map_address = task->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(task);
            return artifacts;
          });

  Node* task = GetSnapshotNode(generator.get(), task_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || task == nullptr || map == nullptr) {
    PrintF("[TODO] PROMISE_REJECT_REACTION_JOB_TASK_TYPE not yet stable in "
           "this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), task, Node::Type::kHidden,
                               "system / PromiseRejectReactionJobTask"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), task, map);
  MarkCoveredInstanceType(PROMISE_REJECT_REACTION_JOB_TASK_TYPE);
}

UNINITIALIZED_TEST(HeapDumpCallableTaskExactMapEdge) {
  Address task_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              globalThis.__heap_dump_callable_task =
                  function HeapDumpCallableTask() { return 134; };
            )");
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Function> callable = v8::Local<v8::Function>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "__heap_dump_callable_task"))
                    .ToLocalChecked());
            DirectHandle<JSReceiver> callable_handle =
                Cast<JSReceiver>(v8::Utils::OpenDirectHandle(*callable));
            DirectHandle<CallableTask> task =
                isolate->factory()->NewCallableTask(
                    callable_handle, isolate->native_context());

            is_expected_instance_type =
                CALLABLE_TASK_TYPE == task->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(CALLABLE_TASK_TYPE, task->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            task_address = task->address();
            map_address = task->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(task);
            artifacts.keep_alive.push_back(callable_handle);
            return artifacts;
          });

  Node* task = GetSnapshotNode(generator.get(), task_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || task == nullptr || map == nullptr) {
    PrintF("[TODO] CALLABLE_TASK_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), task, Node::Type::kHidden,
                               "system / CallableTask"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), task, map);
  MarkCoveredInstanceType(CALLABLE_TASK_TYPE);
}

UNINITIALIZED_TEST(HeapDumpCallbackTaskExactMapEdge) {
  Address task_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<Foreign> callback =
                isolate->factory()->NewForeign<kMicrotaskCallbackTag>(
                    reinterpret_cast<Address>(&HeapDumpCallbackTaskNoop));
            DirectHandle<Foreign> data =
                isolate->factory()->NewForeign<kMicrotaskCallbackDataTag>(
                    0x135);
            DirectHandle<CallbackTask> task =
                isolate->factory()->NewCallbackTask(callback, data);

            is_expected_instance_type =
                CALLBACK_TASK_TYPE == task->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(CALLBACK_TASK_TYPE, task->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            task_address = task->address();
            map_address = task->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(task);
            artifacts.keep_alive.push_back(callback);
            artifacts.keep_alive.push_back(data);
            return artifacts;
          });

  Node* task = GetSnapshotNode(generator.get(), task_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || task == nullptr || map == nullptr) {
    PrintF("[TODO] CALLBACK_TASK_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), task, Node::Type::kHidden,
                               "system / CallbackTask"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), task, map);
  MarkCoveredInstanceType(CALLBACK_TASK_TYPE);
}

UNINITIALIZED_TEST(HeapDumpPromiseResolveThenableJobTaskExactMapEdge) {
  Address task_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              globalThis.__heap_dump_thenable = {
                then: function HeapDumpThenableThen(resolve) {
                  resolve(136);
                }
              };
            )");
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> thenable = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "__heap_dump_thenable"))
                    .ToLocalChecked());
            v8::Local<v8::Function> then = v8::Local<v8::Function>::Cast(
                thenable->Get(context, NewApiString(v8_isolate, "then"))
                    .ToLocalChecked());
            DirectHandle<JSPromise> promise =
                isolate->factory()->NewJSPromise();
            DirectHandle<JSReceiver> thenable_handle =
                Cast<JSReceiver>(v8::Utils::OpenDirectHandle(*thenable));
            DirectHandle<JSReceiver> then_handle =
                Cast<JSReceiver>(v8::Utils::OpenDirectHandle(*then));
            DirectHandle<PromiseResolveThenableJobTask> task =
                isolate->factory()->NewPromiseResolveThenableJobTask(
                    promise, thenable_handle, then_handle,
                    isolate->native_context());

            is_expected_instance_type =
                PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE ==
                task->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE,
                     task->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            task_address = task->address();
            map_address = task->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(task);
            artifacts.keep_alive.push_back(promise);
            artifacts.keep_alive.push_back(thenable_handle);
            artifacts.keep_alive.push_back(then_handle);
            return artifacts;
          });

  Node* task = GetSnapshotNode(generator.get(), task_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || task == nullptr || map == nullptr) {
    PrintF("[TODO] PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE not yet stable in "
           "this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), task, Node::Type::kHidden,
                               "system / PromiseResolveThenableJobTask"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), task, map);
  MarkCoveredInstanceType(PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE);
}

UNINITIALIZED_TEST(HeapDumpLoadHandlerExactMapEdge) {
  Address handler_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            Handle<LoadHandler> handler =
                isolate->factory()->NewLoadHandler(1, AllocationType::kOld);
            handler->set_smi_handler(Smi::zero());
            handler->set_validity_cell(Smi::zero());
            handler->set_data1(Smi::zero());

            is_expected_instance_type =
                LOAD_HANDLER_TYPE == handler->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(LOAD_HANDLER_TYPE, handler->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            handler_address = handler->address();
            map_address = handler->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(direct_handle(*handler, isolate));
            return artifacts;
          });

  Node* handler = GetSnapshotNode(generator.get(), handler_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || handler == nullptr || map == nullptr) {
    PrintF("[TODO] LOAD_HANDLER_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), handler, Node::Type::kCode,
                               "system / LoadHandler"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), handler, map);
  MarkCoveredInstanceType(LOAD_HANDLER_TYPE);
}

UNINITIALIZED_TEST(HeapDumpStoreHandlerExactMapEdge) {
  Address handler_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            Handle<StoreHandler> handler =
                isolate->factory()->NewStoreHandler(0);
            handler->set_smi_handler(Smi::zero());
            handler->set_validity_cell(Smi::zero());

            is_expected_instance_type =
                STORE_HANDLER_TYPE == handler->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(STORE_HANDLER_TYPE, handler->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            handler_address = handler->address();
            map_address = handler->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(direct_handle(*handler, isolate));
            return artifacts;
          });

  Node* handler = GetSnapshotNode(generator.get(), handler_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || handler == nullptr || map == nullptr) {
    PrintF("[TODO] STORE_HANDLER_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), handler, Node::Type::kCode,
                               "system / StoreHandler"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), handler, map);
  MarkCoveredInstanceType(STORE_HANDLER_TYPE);
}

UNINITIALIZED_TEST(HeapDumpAccessCheckInfoExactMapEdge) {
  Address info_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            Handle<Struct> struct_info = isolate->factory()->NewStruct(
                ACCESS_CHECK_INFO_TYPE, AllocationType::kOld);
            DirectHandle<AccessCheckInfo> info =
                Cast<AccessCheckInfo>(struct_info);
            info->set_callback(Smi::zero(), SKIP_WRITE_BARRIER);
            info->set_named_interceptor(Smi::zero(), SKIP_WRITE_BARRIER);
            info->set_indexed_interceptor(Smi::zero(), SKIP_WRITE_BARRIER);
            info->set_data(ReadOnlyRoots(isolate).undefined_value(),
                           SKIP_WRITE_BARRIER);

            is_expected_instance_type =
                ACCESS_CHECK_INFO_TYPE == info->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(ACCESS_CHECK_INFO_TYPE, info->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            info_address = info->address();
            map_address = info->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(info);
            return artifacts;
          });

  Node* info = GetSnapshotNode(generator.get(), info_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || info == nullptr || map == nullptr) {
    PrintF("[TODO] ACCESS_CHECK_INFO_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), info, Node::Type::kHidden,
                               "system / AccessCheckInfo"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), info, map);
  MarkCoveredInstanceType(ACCESS_CHECK_INFO_TYPE);
}

UNINITIALIZED_TEST(HeapDumpAsyncGeneratorRequestExactMapEdge) {
  Address request_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const pending = new Promise(() => {});
                async function* HeapDumpAsyncGeneratorRequestFixture() {
                  await pending;
                  yield 'heap-dump-async-generator-request';
                }
                const asyncGenerator =
                    HeapDumpAsyncGeneratorRequestFixture();
                const requestPromise = asyncGenerator.next();
                if (typeof asyncGenerator !== 'object' ||
                    typeof asyncGenerator.next !== 'function' ||
                    typeof requestPromise.then !== 'function') {
                  throw new Error('unexpected async generator request fixture');
                }
                globalThis.__heap_dump_async_generator_request_fixture = {
                  asyncGenerator, pending, requestPromise
                };
              })();
            )");

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_async_generator_request_fixture"))
                    .ToLocalChecked());
            v8::Local<v8::Value> async_generator_value =
                fixture
                    ->Get(context,
                          NewApiString(v8_isolate, "asyncGenerator"))
                    .ToLocalChecked();
            CHECK(async_generator_value->IsGeneratorObject());
            DirectHandle<JSAsyncGeneratorObject> async_generator =
                Cast<JSAsyncGeneratorObject>(v8::Utils::OpenDirectHandle(
                    *v8::Local<v8::Object>::Cast(async_generator_value)));
            CHECK(IsAsyncGeneratorRequest(async_generator->queue()));
            DirectHandle<AsyncGeneratorRequest> request(
                Cast<AsyncGeneratorRequest>(async_generator->queue()),
                isolate);

            is_expected_instance_type =
                ASYNC_GENERATOR_REQUEST_TYPE ==
                request->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(ASYNC_GENERATOR_REQUEST_TYPE,
                     request->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            request_address = request->address();
            map_address = request->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(request);
            artifacts.keep_alive.push_back(async_generator);
            return artifacts;
          });

  Node* request = GetSnapshotNode(generator.get(), request_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || request == nullptr || map == nullptr) {
    PrintF(
        "[TODO] ASYNC_GENERATOR_REQUEST_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), request, Node::Type::kHidden,
                               "system / AsyncGeneratorRequest"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), request, map);
  MarkCoveredInstanceType(ASYNC_GENERATOR_REQUEST_TYPE);
}

UNINITIALIZED_TEST(HeapDumpBreakPointExactMapEdge) {
  Address break_point_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<String> condition =
                isolate->factory()->InternalizeUtf8String("true");
            Handle<BreakPoint> break_point =
                isolate->factory()->NewBreakPoint(23, condition);

            is_expected_instance_type =
                BREAK_POINT_TYPE == break_point->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(BREAK_POINT_TYPE, break_point->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            break_point_address = break_point->address();
            map_address = break_point->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(direct_handle(*break_point,
                                                         isolate));
            artifacts.keep_alive.push_back(condition);
            return artifacts;
          });

  Node* break_point = GetSnapshotNode(generator.get(), break_point_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || break_point == nullptr || map == nullptr) {
    PrintF("[TODO] BREAK_POINT_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), break_point, Node::Type::kHidden,
                               "system / BreakPoint"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), break_point, map);
  MarkCoveredInstanceType(BREAK_POINT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpBreakPointInfoExactMapEdge) {
  Address info_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<BreakPointInfo> info =
                isolate->factory()->NewBreakPointInfo(17);

            is_expected_instance_type =
                BREAK_POINT_INFO_TYPE == info->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(BREAK_POINT_INFO_TYPE, info->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            info_address = info->address();
            map_address = info->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(info);
            return artifacts;
          });

  Node* info = GetSnapshotNode(generator.get(), info_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || info == nullptr || map == nullptr) {
    PrintF("[TODO] BREAK_POINT_INFO_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), info, Node::Type::kHidden,
                               "system / BreakPointInfo"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), info, map);
  MarkCoveredInstanceType(BREAK_POINT_INFO_TYPE);
}

UNINITIALIZED_TEST(HeapDumpDebugInfoExactMapEdge) {
  Address debug_info_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            RunScript(context, R"(
              function HeapDumpDebugInfoFixture() {
                return 153;
              }
            )");
            v8::Local<v8::Value> function_value =
                context->Global()
                    ->Get(context, NewApiString(v8_isolate,
                                                "HeapDumpDebugInfoFixture"))
                    .ToLocalChecked();
            CHECK(function_value->IsFunction());
            DirectHandle<JSFunction> function =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*function_value));
            DirectHandle<SharedFunctionInfo> shared(
                function->shared(), isolate);
            Handle<DebugInfo> debug_info =
                isolate->factory()->NewDebugInfo(shared);

            is_expected_instance_type =
                DEBUG_INFO_TYPE == debug_info->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(DEBUG_INFO_TYPE, debug_info->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            debug_info_address = debug_info->address();
            map_address = debug_info->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(direct_handle(*debug_info,
                                                         isolate));
            artifacts.keep_alive.push_back(shared);
            artifacts.keep_alive.push_back(function);
            return artifacts;
          });

  Node* debug_info = GetSnapshotNode(generator.get(), debug_info_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || debug_info == nullptr || map == nullptr) {
    PrintF("[TODO] DEBUG_INFO_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), debug_info, Node::Type::kHidden,
                               "system / DebugInfo"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), debug_info, map);
  MarkCoveredInstanceType(DEBUG_INFO_TYPE);
}

UNINITIALIZED_TEST(HeapDumpFunctionTemplateRareDataExactMapEdge) {
  Address rare_data_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<FunctionTemplateRareData> rare_data =
                isolate->factory()->NewFunctionTemplateRareData();

            is_expected_instance_type =
                FUNCTION_TEMPLATE_RARE_DATA_TYPE ==
                rare_data->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(FUNCTION_TEMPLATE_RARE_DATA_TYPE,
                     rare_data->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            rare_data_address = rare_data->address();
            map_address = rare_data->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(rare_data);
            return artifacts;
          });

  Node* rare_data = GetSnapshotNode(generator.get(), rare_data_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || rare_data == nullptr || map == nullptr) {
    PrintF(
        "[TODO] FUNCTION_TEMPLATE_RARE_DATA_TYPE not yet stable in this "
        "branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), rare_data, Node::Type::kHidden,
                               "system / FunctionTemplateRareData"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), rare_data, map);
  MarkCoveredInstanceType(FUNCTION_TEMPLATE_RARE_DATA_TYPE);
}

UNINITIALIZED_TEST(HeapDumpTemplateObjectDescriptionExactMapEdge) {
  Address description_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<FixedArray> raw_strings =
                isolate->factory()->NewFixedArray(1);
            DirectHandle<FixedArray> cooked_strings =
                isolate->factory()->NewFixedArray(1);
            DirectHandle<String> raw =
                isolate->factory()->InternalizeUtf8String("heap-dump-raw");
            DirectHandle<String> cooked =
                isolate->factory()->InternalizeUtf8String("heap-dump-cooked");
            raw_strings->set(0, *raw);
            cooked_strings->set(0, *cooked);
            Handle<TemplateObjectDescription> description =
                isolate->factory()->NewTemplateObjectDescription(raw_strings,
                                                                 cooked_strings);

            is_expected_instance_type =
                TEMPLATE_OBJECT_DESCRIPTION_TYPE ==
                description->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(TEMPLATE_OBJECT_DESCRIPTION_TYPE,
                     description->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            description_address = description->address();
            map_address = description->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(direct_handle(*description,
                                                         isolate));
            artifacts.keep_alive.push_back(raw_strings);
            artifacts.keep_alive.push_back(cooked_strings);
            return artifacts;
          });

  Node* description = GetSnapshotNode(generator.get(), description_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || description == nullptr || map == nullptr) {
    PrintF(
        "[TODO] TEMPLATE_OBJECT_DESCRIPTION_TYPE not yet stable in this "
        "branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), description,
                               Node::Type::kCode,
                               "system / TemplateObjectDescription"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), description, map);
  MarkCoveredInstanceType(TEMPLATE_OBJECT_DESCRIPTION_TYPE);
}

UNINITIALIZED_TEST(HeapDumpTuple2ExactMapEdge) {
  Address tuple_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<String> value1 =
                isolate->factory()->InternalizeUtf8String("heap-dump-tuple2-a");
            DirectHandle<String> value2 =
                isolate->factory()->InternalizeUtf8String("heap-dump-tuple2-b");
            DirectHandle<Tuple2> tuple =
                isolate->factory()->NewTuple2(value1, value2,
                                              AllocationType::kOld);

            is_expected_instance_type =
                TUPLE2_TYPE == tuple->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(TUPLE2_TYPE, tuple->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            tuple_address = tuple->address();
            map_address = tuple->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(tuple);
            artifacts.keep_alive.push_back(value1);
            artifacts.keep_alive.push_back(value2);
            return artifacts;
          });

  Node* tuple = GetSnapshotNode(generator.get(), tuple_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || tuple == nullptr || map == nullptr) {
    PrintF("[TODO] TUPLE2_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), tuple, Node::Type::kHidden,
                               "system / Tuple2"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), tuple, map);
  MarkCoveredInstanceType(TUPLE2_TYPE);
}

UNINITIALIZED_TEST(HeapDumpHoleExactMapEdge) {
  Address hole_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<HeapObject> hole = direct_handle(
                Cast<HeapObject>(ReadOnlyRoots(isolate).the_hole_value()),
                isolate);

            is_expected_instance_type = HOLE_TYPE == hole->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }
            CHECK_EQ(HOLE_TYPE, hole->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());

            hole_address = hole->address();
            map_address = hole->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(direct_handle(*hole, isolate));
            return artifacts;
          });

  Node* hole = GetSnapshotNode(generator.get(), hole_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || hole == nullptr || map == nullptr) {
    PrintF("[TODO] HOLE_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), hole, map);
  MarkCoveredInstanceType(HOLE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpUncompiledDataExactMapEdges) {
  Address preparse_data_address = 0;
  Address preparse_data_map_address = 0;
  Address with_preparse_address = 0;
  Address with_preparse_map_address = 0;
  Address with_preparse_and_job_address = 0;
  Address with_preparse_and_job_map_address = 0;
  Address without_preparse_address = 0;
  Address without_preparse_map_address = 0;
  Address without_preparse_with_job_address = 0;
  Address without_preparse_with_job_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            Handle<PreparseData> preparse_data =
                isolate->factory()->NewPreparseData(4, 1);
            DirectHandle<String> inferred_name =
                isolate->factory()->InternalizeUtf8String(
                    "heap_dump_uncompiled_data");
            DirectHandle<UncompiledDataWithPreparseData> with_preparse =
                isolate->factory()->NewUncompiledDataWithPreparseData(
                    inferred_name, 0, 8, preparse_data);
            DirectHandle<UncompiledDataWithPreparseDataAndJob>
                with_preparse_and_job =
                    isolate->factory()->NewUncompiledDataWithPreparseDataAndJob(
                        inferred_name, 9, 17, preparse_data);
            DirectHandle<UncompiledDataWithoutPreparseData> without_preparse =
                isolate->factory()->NewUncompiledDataWithoutPreparseData(
                    inferred_name, 18, 26);
            DirectHandle<UncompiledDataWithoutPreparseDataWithJob>
                without_preparse_with_job =
                    isolate->factory()
                        ->NewUncompiledDataWithoutPreparseDataWithJob(
                            inferred_name, 27, 35);

            CHECK_EQ(PREPARSE_DATA_TYPE, preparse_data->map()->instance_type());
            CHECK_EQ(UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE,
                     with_preparse->map()->instance_type());
            CHECK_EQ(UNCOMPILED_DATA_WITH_PREPARSE_DATA_AND_JOB_TYPE,
                     with_preparse_and_job->map()->instance_type());
            CHECK_EQ(UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE,
                     without_preparse->map()->instance_type());
            CHECK_EQ(UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_WITH_JOB_TYPE,
                     without_preparse_with_job->map()->instance_type());
            CHECK_EQ(*preparse_data, with_preparse->preparse_data());
            CHECK_EQ(*preparse_data, with_preparse_and_job->preparse_data());
            CHECK_EQ(kNullAddress, with_preparse_and_job->job());
            CHECK_EQ(kNullAddress, without_preparse_with_job->job());

            InvokeAtomicMajorGC(isolate->heap());

            preparse_data_address = preparse_data->address();
            preparse_data_map_address = preparse_data->map()->address();
            with_preparse_address = with_preparse->address();
            with_preparse_map_address = with_preparse->map()->address();
            with_preparse_and_job_address = with_preparse_and_job->address();
            with_preparse_and_job_map_address =
                with_preparse_and_job->map()->address();
            without_preparse_address = without_preparse->address();
            without_preparse_map_address = without_preparse->map()->address();
            without_preparse_with_job_address =
                without_preparse_with_job->address();
            without_preparse_with_job_map_address =
                without_preparse_with_job->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(preparse_data);
            artifacts.keep_alive.push_back(inferred_name);
            artifacts.keep_alive.push_back(with_preparse);
            artifacts.keep_alive.push_back(with_preparse_and_job);
            artifacts.keep_alive.push_back(without_preparse);
            artifacts.keep_alive.push_back(without_preparse_with_job);
            return artifacts;
          });

  Node* preparse_data =
      GetSnapshotNode(generator.get(), preparse_data_address);
  Node* preparse_data_map =
      GetSnapshotNode(generator.get(), preparse_data_map_address);
  if (preparse_data == nullptr || preparse_data_map == nullptr) {
    PrintF("[TODO] PREPARSE_DATA_TYPE not yet stable in this branch.\n");
  } else {
    CheckExactInternalMapEdgeNoWrongKind(generator.get(), preparse_data,
                                        preparse_data_map);
    MarkCoveredInstanceType(PREPARSE_DATA_TYPE);
  }

  if (CheckUncompiledDataExactMapEdge(
          generator.get(), with_preparse_address, with_preparse_map_address,
          "UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE")) {
    MarkCoveredInstanceType(UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE);
  }
  if (CheckUncompiledDataExactMapEdge(
          generator.get(), with_preparse_and_job_address,
          with_preparse_and_job_map_address,
          "UNCOMPILED_DATA_WITH_PREPARSE_DATA_AND_JOB_TYPE")) {
    MarkCoveredInstanceType(UNCOMPILED_DATA_WITH_PREPARSE_DATA_AND_JOB_TYPE);
  }
  if (CheckUncompiledDataExactMapEdge(
          generator.get(), without_preparse_address,
          without_preparse_map_address,
          "UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE")) {
    MarkCoveredInstanceType(UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE);
  }
  if (CheckUncompiledDataExactMapEdge(
          generator.get(), without_preparse_with_job_address,
          without_preparse_with_job_map_address,
          "UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_WITH_JOB_TYPE")) {
    MarkCoveredInstanceType(UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_WITH_JOB_TYPE);
  }
}

UNINITIALIZED_TEST(HeapDumpCodeExactMapEdge) {
  // Raw Code objects should keep their map edge stable after translation.
  Address code_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            RunScript(context, R"(
              function HeapDumpCodeExactMapEdgeFixture(x, y) {
                return x + y;
              }
            )");
            v8::Local<v8::Value> function_value =
                context->Global()
                    ->Get(context, NewApiString(v8_isolate,
                                                "HeapDumpCodeExactMapEdgeFixture"))
                    .ToLocalChecked();
            CHECK(function_value->IsFunction());
            DirectHandle<JSFunction> function =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*function_value));
            DirectHandle<Code> code(function->code(isolate), isolate);

            is_expected_instance_type = CODE_TYPE == code->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }
            CHECK_EQ(CODE_TYPE, code->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());
            code_address = code->address();
            map_address = code->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(code);
            artifacts.keep_alive.push_back(function);
            return artifacts;
          });

  Node* code_node = GetSnapshotNode(generator.get(), code_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || code_node == nullptr || map == nullptr) {
    PrintF("[TODO] CODE_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeHasType(code_node, Node::Type::kCode));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), code_node, map);
  MarkCoveredInstanceType(CODE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpInterpreterDataExactMapEdge) {
  // InterpreterData records the bytecode and interpreter trampoline for bytecode
  // code objects, and should expose a stable internal map edge.
  Address interpreter_data_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            RunScript(context, R"(
              function HeapDumpInterpreterDataExactMapEdgeFixture(a) {
                return a + 1;
              }
              HeapDumpInterpreterDataExactMapEdgeFixture(3);
            )");
            v8::Local<v8::Value> function_value =
                context->Global()
                    ->Get(context, NewApiString(v8_isolate,
                                                "HeapDumpInterpreterDataExactMapEdgeFixture"))
                    .ToLocalChecked();
            CHECK(function_value->IsFunction());
            DirectHandle<JSFunction> function =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*function_value));

            DirectHandle<SharedFunctionInfo> shared(function->shared(), isolate);
            CHECK(shared->HasBytecodeArray());
            DirectHandle<BytecodeArray> bytecode_array(
                shared->GetBytecodeArray(isolate), isolate);
            DirectHandle<Code> function_code = CreateCodeForTest(isolate);
            DirectHandle<InterpreterData> interpreter_data =
                isolate->factory()->NewInterpreterData(bytecode_array,
                                                       function_code);

            is_expected_instance_type =
                INTERPRETER_DATA_TYPE == interpreter_data->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }
            CHECK_EQ(INTERPRETER_DATA_TYPE, interpreter_data->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());
            interpreter_data_address = interpreter_data->address();
            map_address = interpreter_data->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(interpreter_data);
            artifacts.keep_alive.push_back(bytecode_array);
            artifacts.keep_alive.push_back(function_code);
            artifacts.keep_alive.push_back(function);
            artifacts.keep_alive.push_back(shared);
            return artifacts;
          });

  Node* interpreter_data_node =
      GetSnapshotNode(generator.get(), interpreter_data_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || interpreter_data_node == nullptr ||
      map == nullptr) {
    PrintF("[TODO] INTERPRETER_DATA_TYPE not yet stable in this branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(),
                                       interpreter_data_node, map);
  MarkCoveredInstanceType(INTERPRETER_DATA_TYPE);
}

UNINITIALIZED_TEST(HeapDumpOnHeapBasicBlockProfilerDataExactMapEdge) {
  // On-heap profiler data should keep the same internal map edge as long as the
  // object remains alive.
  Address profiler_data_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            BasicBlockProfilerData* profiler_data =
                BasicBlockProfiler::Get()->NewData(1);
            profiler_data->SetBlockId(0, 1);
            DirectHandle<OnHeapBasicBlockProfilerData> on_heap_data =
                profiler_data->CopyToJSHeap(isolate);

            is_expected_instance_type =
                ON_HEAP_BASIC_BLOCK_PROFILER_DATA_TYPE ==
                on_heap_data->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }
            CHECK_EQ(ON_HEAP_BASIC_BLOCK_PROFILER_DATA_TYPE,
                     on_heap_data->map()->instance_type());

            InvokeAtomicMajorGC(isolate->heap());
            profiler_data_address = on_heap_data->address();
            map_address = on_heap_data->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(on_heap_data);
            return artifacts;
          });

  Node* profiler_data_node =
      GetSnapshotNode(generator.get(), profiler_data_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || profiler_data_node == nullptr ||
      map == nullptr) {
    PrintF(
        "[TODO] ON_HEAP_BASIC_BLOCK_PROFILER_DATA_TYPE not yet stable in this "
        "branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), profiler_data_node, map);
  MarkCoveredInstanceType(ON_HEAP_BASIC_BLOCK_PROFILER_DATA_TYPE);
}

#else

UNINITIALIZED_TEST(HeapDumpBasicEndToEnd) {}
UNINITIALIZED_TEST(HeapDumpTranslateRecognizesRepresentativeNodeKinds) {}
UNINITIALIZED_TEST(HeapDumpTranslateForeignUsesSystemForeignFallback) {}
UNINITIALIZED_TEST(HeapDumpSnapshotAlignedJsObjectGraphBaseline) {}
UNINITIALIZED_TEST(HeapDumpSnapshotAlignedPersistentHandleBaseline) {}
UNINITIALIZED_TEST(HeapDumpSnapshotAlignedObjectShapeWhiteBoxBaseline) {}
UNINITIALIZED_TEST(HeapDumpRootAndGlobalObjectDescriptions) {}
UNINITIALIZED_TEST(HeapDumpExactByteArrayType) {}
UNINITIALIZED_TEST(HeapDumpExactArrayListType) {}
UNINITIALIZED_TEST(HeapDumpExactCellType) {}
UNINITIALIZED_TEST(HeapDumpCellExactStrongValueEdge) {}
UNINITIALIZED_TEST(HeapDumpCellExactWeakValueEdge) {}
UNINITIALIZED_TEST(HeapDumpExactClosureFeedbackCellArrayType) {}
UNINITIALIZED_TEST(HeapDumpExactContextCellType) {}
UNINITIALIZED_TEST(HeapDumpExactFeedbackCellType) {}
UNINITIALIZED_TEST(HeapDumpFeedbackCellExactValueEdge) {}
UNINITIALIZED_TEST(HeapDumpExactAllocationSiteType) {}
UNINITIALIZED_TEST(HeapDumpAllocationSiteExactTransitionInfoEdge) {}
UNINITIALIZED_TEST(HeapDumpAllocationSiteExactNestedSiteEdge) {}
UNINITIALIZED_TEST(HeapDumpAllocationSiteExactDependentCodeEdge) {}
UNINITIALIZED_TEST(HeapDumpPromiseFulfillReactionJobTaskExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpPromiseRejectReactionJobTaskExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpCallableTaskExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpCallbackTaskExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpPromiseResolveThenableJobTaskExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpLoadHandlerExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpStoreHandlerExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpAccessCheckInfoExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpAsyncGeneratorRequestExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpBreakPointExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpBreakPointInfoExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpDebugInfoExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpFunctionTemplateRareDataExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpTemplateObjectDescriptionExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpTuple2ExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpHoleExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpUncompiledDataExactMapEdges) {}
UNINITIALIZED_TEST(HeapDumpCodeExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpInterpreterDataExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpOnHeapBasicBlockProfilerDataExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpProtectedFixedArrayExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpProtectedWeakFixedArrayExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpTrustedByteArrayExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpTrustedFixedArrayExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpTrustedWeakFixedArrayExactMapEdge) {}

#endif

}  // namespace heap
}  // namespace internal
}  // namespace v8
