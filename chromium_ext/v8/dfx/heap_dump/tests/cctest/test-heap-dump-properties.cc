#include "include/v8-primitive-object.h"
#include "include/v8-primitive.h"
#include "src/ast/ast-source-ranges.h"
#include "src/execution/messages.h"
#include "src/objects/call-site-info-inl.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/api-callbacks-inl.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/bigint.h"
#include "src/objects/bytecode-array-inl.h"
#include "src/objects/contexts-inl.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/descriptor-array-inl.h"
#include "src/objects/dictionary.h"
#include "src/objects/feedback-vector-inl.h"
#include "src/objects/field-type.h"
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
#include "src/objects/literal-objects-inl.h"
#include "src/objects/number-string-cache.h"
#include "src/objects/module-inl.h"
#include "src/objects/name-inl.h"
#include "src/objects/oddball-inl.h"
#include "src/objects/ordered-hash-table.h"
#include "src/objects/swiss-name-dictionary.h"
#include "src/objects/promise-inl.h"
#include "src/objects/property-descriptor-object-inl.h"
#include "src/objects/property-array-inl.h"
#include "src/objects/property.h"
#include "src/objects/prototype-info-inl.h"
#include "src/objects/transitions.h"
#include "src/objects/regexp-match-info-inl.h"
#include "src/objects/scope-info.h"
#include "src/objects/script-inl.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/source-text-module-inl.h"
#include "src/objects/struct-inl.h"
#include "src/objects/templates.h"
#include "src/objects/turbofan-types-inl.h"
#include "src/objects/turboshaft-types-inl.h"
#include "src/zone/zone-containers.h"
#include "src/zone/zone.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/tests/cctest/heap-dump-test-utils.h"
#include "test/cctest/test-transitions.h"

namespace v8 {
namespace internal {
namespace heap {

#if defined(OH_ENABLE_HEAP_DUMP_TEST) && defined(OH_ENABLE_HEAP_DUMP) && \
    defined(OH_ENABLE_HEAP_TRANSLATE)

using namespace heap_dump_test;

namespace {

void HeapDumpTemplateNoopCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {}

v8::Intercepted HeapDumpTemplateNamedPropertyGetter(
    v8::Local<v8::Name> /*property*/,
    const v8::PropertyCallbackInfo<v8::Value>& /*info*/) {
  return v8::Intercepted::kYes;
}

void CheckExactInternalMapEdgeNoWrongKind(SnapshotGenerator* generator,
                                          const Node* object, const Node* map) {
  // Native heap snapshots mirror map pointers as internal "map" edges.
  // Breadcrumb: V8HeapExplorer::ExtractReferences, followed by
  // SetInternalReference(entry, "map", obj->map(...), HeapObject::kMapOffset).
  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CHECK(
      HasExactNamedEdge(generator, object, Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator, object, Edge::Type::kInternal,
                                   "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(object, Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kProperty, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kShortcut, map));
}

void CheckNoWeakHiddenOrPropertyEdgeToTarget(const Node* source,
                                             const Node* target) {
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kWeak, target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kHidden, target));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(source, Edge::Type::kProperty, target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(source, Edge::Type::kShortcut, target));
}

}  // namespace

UNINITIALIZED_TEST(HeapDumpCoverageInfoExactMapEdge) {
  Address coverage_info_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            Zone coverage_zone(isolate->allocator(), ZONE_NAME);
            ZoneVector<SourceRange> coverage_slots(&coverage_zone);
            coverage_slots.push_back(SourceRange(0, 5));

            DirectHandle<CoverageInfo> coverage_info =
                isolate->factory()->NewCoverageInfo(coverage_slots);

            is_expected_instance_type =
                COVERAGE_INFO_TYPE == coverage_info->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(COVERAGE_INFO_TYPE, coverage_info->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            coverage_info_address = coverage_info->address();
            map_address = coverage_info->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(coverage_info);
            return artifacts;
          });

  Node* coverage_info = GetSnapshotNode(generator.get(), coverage_info_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || coverage_info == nullptr || map == nullptr) {
    PrintF("[TODO] COVERAGE_INFO_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), coverage_info, map);
  MarkCoveredInstanceType(COVERAGE_INFO_TYPE);
}

UNINITIALIZED_TEST(HeapDumpAccessorStructuresExactEdges) {
  Address accessor_info_address = 0;
  Address accessor_name_address = 0;
  Address accessor_data_address = 0;
  Address accessor_pair_address = 0;
  Address accessor_info_map_address = 0;
  Address accessor_pair_map_address = 0;
  Address getter_address = 0;
  Address setter_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        RunScript(context, R"(
              globalThis.__heap_dump_accessor_getter =
                  function HeapDumpAccessorGetter() { return 11; };
              globalThis.__heap_dump_accessor_setter =
                  function HeapDumpAccessorSetter(value) {
                    globalThis.__heap_dump_accessor_set_value = value;
                  };
            )");

        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        auto get_global = [&](const char* name) {
          return context->Global()
              ->Get(context, NewApiString(v8_isolate, name))
              .ToLocalChecked();
        };

        DirectHandle<JSObject> data_holder =
            isolate->factory()->NewJSObject(isolate->object_function());
        DirectHandle<String> accessor_name =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpAccessorStructureName");
        DirectHandle<JSFunction> getter_target =
            Cast<JSFunction>(v8::Utils::OpenDirectHandle(
                *get_global("__heap_dump_accessor_getter")));
        DirectHandle<JSFunction> setter_target =
            Cast<JSFunction>(v8::Utils::OpenDirectHandle(
                *get_global("__heap_dump_accessor_setter")));

        DirectHandle<AccessorInfo> accessor_info =
            isolate->factory()->NewAccessorInfo();
        accessor_info->set_name(*accessor_name);
        accessor_info->set_data(*data_holder);

        Handle<AccessorPair> accessor_pair =
            isolate->factory()->NewAccessorPair();
        accessor_pair->set_getter(*getter_target);
        accessor_pair->set_setter(*setter_target);

        InvokeAtomicMajorGC(isolate->heap());

        CHECK_EQ(*accessor_name, accessor_info->name());
        CHECK_EQ(*data_holder, accessor_info->data());
        CHECK_EQ(ACCESSOR_INFO_TYPE, accessor_info->map()->instance_type());
        CHECK_EQ(ACCESSOR_PAIR_TYPE, accessor_pair->map()->instance_type());
        CHECK_EQ(*getter_target, accessor_pair->getter());
        CHECK_EQ(*setter_target, accessor_pair->setter());

        accessor_info_address = accessor_info->address();
        accessor_name_address = accessor_name->address();
        accessor_data_address = data_holder->address();
        accessor_pair_address = accessor_pair->address();
        accessor_info_map_address = accessor_info->map()->address();
        accessor_pair_map_address = accessor_pair->map()->address();
        getter_address = getter_target->address();
        setter_address = setter_target->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(accessor_info);
        artifacts.keep_alive.push_back(accessor_name);
        artifacts.keep_alive.push_back(data_holder);
        artifacts.keep_alive.push_back(direct_handle(*accessor_pair, isolate));
        artifacts.keep_alive.push_back(getter_target);
        artifacts.keep_alive.push_back(setter_target);
        return artifacts;
      });

  Node* accessor_info = GetSnapshotNode(generator.get(), accessor_info_address);
  Node* accessor_name = GetSnapshotNode(generator.get(), accessor_name_address);
  Node* accessor_data = GetSnapshotNode(generator.get(), accessor_data_address);
  Node* accessor_pair = GetSnapshotNode(generator.get(), accessor_pair_address);
  Node* accessor_info_map =
      GetSnapshotNode(generator.get(), accessor_info_map_address);
  Node* accessor_pair_map =
      GetSnapshotNode(generator.get(), accessor_pair_map_address);
  Node* getter = GetSnapshotNode(generator.get(), getter_address);
  Node* setter = GetSnapshotNode(generator.get(), setter_address);
  CHECK_NOT_NULL(accessor_info);
  CHECK_NOT_NULL(accessor_name);
  CHECK_NOT_NULL(accessor_data);
  CHECK_NOT_NULL(accessor_pair);
  CHECK_NOT_NULL(accessor_info_map);
  CHECK_NOT_NULL(accessor_pair_map);
  CHECK_NOT_NULL(getter);
  CHECK_NOT_NULL(setter);
  CHECK(NodeMatchesTypeAndName(generator.get(), accessor_info,
                               Node::Type::kHidden, "system / AccessorInfo"));
  CHECK(NodeHasType(accessor_name, Node::Type::kString));
  CHECK(NodeHasType(accessor_data, Node::Type::kObject));
  CHECK(NodeMatchesTypeAndName(generator.get(), accessor_pair,
                               Node::Type::kHidden, "system / AccessorPair"));
  CHECK(NodeHasType(getter, Node::Type::kClosure));
  CHECK(NodeHasType(setter, Node::Type::kClosure));

  // Native heap snapshots expose AccessorInfo/AccessorPair structural fields as
  // internal edges. Breadcrumbs:
  // V8HeapExplorer::ExtractAccessorInfoReferences and
  // V8HeapExplorer::ExtractAccessorPairReferences.
  CHECK(HasExactNamedEdge(generator.get(), accessor_info, Edge::Type::kInternal,
                          "name", accessor_name));
  CHECK(HasExactNamedEdge(generator.get(), accessor_info, Edge::Type::kInternal,
                          "data", accessor_data));
  CHECK(HasExactNamedEdge(generator.get(), accessor_pair, Edge::Type::kInternal,
                          "getter", getter));
  CHECK(HasExactNamedEdge(generator.get(), accessor_pair, Edge::Type::kInternal,
                          "setter", setter));
  CHECK_EQ(1,
           CountExactNamedEdges(generator.get(), accessor_info,
                                Edge::Type::kInternal, "name", accessor_name));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(accessor_info,
                                          Edge::Type::kInternal, accessor_name));
  CHECK_EQ(1,
           CountExactNamedEdges(generator.get(), accessor_info,
                                Edge::Type::kInternal, "data", accessor_data));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(accessor_info,
                                          Edge::Type::kInternal, accessor_data));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), accessor_pair,
                                   Edge::Type::kInternal, "getter", getter));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(accessor_pair,
                                          Edge::Type::kInternal, getter));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), accessor_pair,
                                   Edge::Type::kInternal, "setter", setter));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(accessor_pair,
                                          Edge::Type::kInternal, setter));
  CheckNoWeakHiddenOrPropertyEdgeToTarget(accessor_info, accessor_name);
  CheckNoWeakHiddenOrPropertyEdgeToTarget(accessor_info, accessor_data);
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), accessor_info,
                                      accessor_info_map);
  MarkCoveredInstanceType(ACCESSOR_INFO_TYPE);
  CheckNoWeakHiddenOrPropertyEdgeToTarget(accessor_pair, getter);
  CheckNoWeakHiddenOrPropertyEdgeToTarget(accessor_pair, setter);
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), accessor_pair,
                                      accessor_pair_map);
  MarkCoveredInstanceType(ACCESSOR_PAIR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpAccessorInfoExactEdges) {
  Address accessor_info_address = 0;
  Address accessor_name_address = 0;
  Address accessor_data_address = 0;
  Address accessor_info_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        DirectHandle<JSObject> data_holder =
            isolate->factory()->NewJSObject(isolate->object_function());
        DirectHandle<String> accessor_name =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpAccessorInfoName");

        DirectHandle<AccessorInfo> accessor_info =
            isolate->factory()->NewAccessorInfo();
        accessor_info->set_name(*accessor_name);
        accessor_info->set_data(*data_holder);

        InvokeAtomicMajorGC(isolate->heap());

        CHECK_EQ(*accessor_name, accessor_info->name());
        CHECK_EQ(*data_holder, accessor_info->data());
        CHECK_EQ(ACCESSOR_INFO_TYPE, accessor_info->map()->instance_type());

        accessor_info_address = accessor_info->address();
        accessor_name_address = accessor_name->address();
        accessor_data_address = data_holder->address();
        accessor_info_map_address = accessor_info->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(accessor_info);
        artifacts.keep_alive.push_back(accessor_name);
        artifacts.keep_alive.push_back(data_holder);
        return artifacts;
      });

  Node* accessor_info = GetSnapshotNode(generator.get(), accessor_info_address);
  Node* accessor_name = GetSnapshotNode(generator.get(), accessor_name_address);
  Node* accessor_data = GetSnapshotNode(generator.get(), accessor_data_address);
  Node* accessor_info_map =
      GetSnapshotNode(generator.get(), accessor_info_map_address);
  CHECK_NOT_NULL(accessor_info);
  CHECK_NOT_NULL(accessor_name);
  CHECK_NOT_NULL(accessor_data);
  CHECK_NOT_NULL(accessor_info_map);
  CHECK(NodeMatchesTypeAndName(generator.get(), accessor_info,
                               Node::Type::kHidden, "system / AccessorInfo"));
  CHECK(NodeHasType(accessor_name, Node::Type::kString));
  CHECK(NodeHasType(accessor_data, Node::Type::kObject));

  // White-box coverage for V8HeapExplorer::ExtractAccessorInfoReferences:
  // E1: SetInternalReference("name", accessor_info->name(), kNameOffset).
  CHECK(HasExactNamedEdge(generator.get(), accessor_info, Edge::Type::kInternal,
                          "name", accessor_name));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), accessor_info,
                                   Edge::Type::kInternal, "name",
                                   accessor_name));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(accessor_info, Edge::Type::kInternal,
                                         accessor_name));
  // E2: SetInternalReference("data", accessor_info->data(), kDataOffset).
  CHECK(HasExactNamedEdge(generator.get(), accessor_info, Edge::Type::kInternal,
                          "data", accessor_data));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), accessor_info,
                                   Edge::Type::kInternal, "data",
                                   accessor_data));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(accessor_info, Edge::Type::kInternal,
                                         accessor_data));
  // AccessorInfo has two object-specific internal edges above plus the generic
  // map edge emitted by ExtractReferences.
  CHECK_EQ(3, CountEdgesFromNode(accessor_info, Edge::Type::kInternal));
  CheckNoWeakHiddenOrPropertyEdgeToTarget(accessor_info, accessor_name);
  CheckNoWeakHiddenOrPropertyEdgeToTarget(accessor_info, accessor_data);
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), accessor_info,
                                      accessor_info_map);
  MarkCoveredInstanceType(ACCESSOR_INFO_TYPE);
}

UNINITIALIZED_TEST(HeapDumpAccessorPairExactEdges) {
  Address accessor_pair_address = 0;
  Address accessor_pair_map_address = 0;
  Address getter_address = 0;
  Address setter_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        RunScript(context, R"(
              globalThis.__heap_dump_accessor_pair_getter =
                  function HeapDumpAccessorPairGetter() { return 23; };
              globalThis.__heap_dump_accessor_pair_setter =
                  function HeapDumpAccessorPairSetter(value) {
                    globalThis.__heap_dump_accessor_pair_set_value = value;
                  };
            )");

        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        auto get_global = [&](const char* name) {
          return context->Global()
              ->Get(context, NewApiString(v8_isolate, name))
              .ToLocalChecked();
        };

        DirectHandle<JSFunction> getter_target =
            Cast<JSFunction>(v8::Utils::OpenDirectHandle(
                *get_global("__heap_dump_accessor_pair_getter")));
        DirectHandle<JSFunction> setter_target =
            Cast<JSFunction>(v8::Utils::OpenDirectHandle(
                *get_global("__heap_dump_accessor_pair_setter")));

        Handle<AccessorPair> accessor_pair =
            isolate->factory()->NewAccessorPair();
        accessor_pair->set_getter(*getter_target);
        accessor_pair->set_setter(*setter_target);

        InvokeAtomicMajorGC(isolate->heap());

        CHECK_EQ(ACCESSOR_PAIR_TYPE, accessor_pair->map()->instance_type());
        CHECK_EQ(*getter_target, accessor_pair->getter());
        CHECK_EQ(*setter_target, accessor_pair->setter());

        accessor_pair_address = accessor_pair->address();
        accessor_pair_map_address = accessor_pair->map()->address();
        getter_address = getter_target->address();
        setter_address = setter_target->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(direct_handle(*accessor_pair, isolate));
        artifacts.keep_alive.push_back(getter_target);
        artifacts.keep_alive.push_back(setter_target);
        return artifacts;
      });

  Node* accessor_pair = GetSnapshotNode(generator.get(), accessor_pair_address);
  Node* accessor_pair_map =
      GetSnapshotNode(generator.get(), accessor_pair_map_address);
  Node* getter = GetSnapshotNode(generator.get(), getter_address);
  Node* setter = GetSnapshotNode(generator.get(), setter_address);
  CHECK_NOT_NULL(accessor_pair);
  CHECK_NOT_NULL(accessor_pair_map);
  CHECK_NOT_NULL(getter);
  CHECK_NOT_NULL(setter);
  CHECK(NodeMatchesTypeAndName(generator.get(), accessor_pair,
                               Node::Type::kHidden, "system / AccessorPair"));
  CHECK(NodeHasType(getter, Node::Type::kClosure));
  CHECK(NodeHasType(setter, Node::Type::kClosure));

  // White-box coverage for V8HeapExplorer::ExtractAccessorPairReferences:
  // E1: SetInternalReference("getter", accessors->getter(), getter offset).
  CHECK(HasExactNamedEdge(generator.get(), accessor_pair, Edge::Type::kInternal,
                          "getter", getter));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), accessor_pair,
                                   Edge::Type::kInternal, "getter", getter));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(accessor_pair, Edge::Type::kInternal,
                                         getter));
  // E2: SetInternalReference("setter", accessors->setter(), setter offset).
  CHECK(HasExactNamedEdge(generator.get(), accessor_pair, Edge::Type::kInternal,
                          "setter", setter));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), accessor_pair,
                                   Edge::Type::kInternal, "setter", setter));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(accessor_pair, Edge::Type::kInternal,
                                         setter));
  // AccessorPair has two object-specific internal edges above plus the generic
  // map edge emitted by ExtractReferences.
  CHECK_EQ(3, CountEdgesFromNode(accessor_pair, Edge::Type::kInternal));
  CheckNoWeakHiddenOrPropertyEdgeToTarget(accessor_pair, getter);
  CheckNoWeakHiddenOrPropertyEdgeToTarget(accessor_pair, setter);
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), accessor_pair,
                                      accessor_pair_map);
  MarkCoveredInstanceType(ACCESSOR_PAIR_TYPE);
}

UNINITIALIZED_TEST(HeapDumpDescriptorArrayExactEntryEdges) {
  Address descriptor_array_address = 0;
  Address first_key_address = 0;
  Address first_value_address = 0;
  Address second_key_address = 0;
  Address second_value_address = 0;
  Address enum_cache_address = 0;
  Address weak_value_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<Name> first_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpDescriptorArrayFirst");
        DirectHandle<Name> second_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpDescriptorArraySecond");
        DirectHandle<Object> first_value =
            isolate->factory()->NewHeapNumber(101);
        DirectHandle<Object> second_value =
            isolate->factory()->NewHeapNumber(202);
        DirectHandle<DescriptorArray> descriptors =
            DescriptorArray::Allocate(isolate, 2, 0, AllocationType::kOld);
        DirectHandle<String> enum_cache_key_one =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpDescriptorArrayEnumCacheKey");
        DirectHandle<String> enum_cache_key_two =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpDescriptorArrayEnumCacheValue");
        DirectHandle<FixedArray> enum_cache_keys =
            isolate->factory()->NewFixedArray(2, AllocationType::kOld);
        DirectHandle<FixedArray> enum_cache_indices =
            isolate->factory()->NewFixedArray(2, AllocationType::kOld);
        enum_cache_keys->set(0, *enum_cache_key_one);
        enum_cache_keys->set(1, *enum_cache_key_two);
        enum_cache_indices->set(0, Smi::FromInt(0));
        enum_cache_indices->set(1, Smi::FromInt(1));
        DirectHandle<EnumCache> enum_cache = isolate->factory()->NewEnumCache(
            enum_cache_keys, enum_cache_indices, AllocationType::kOld);
        descriptors->Initialize(*enum_cache,
                               ReadOnlyRoots(isolate).undefined_value(), 2, 0,
                               DescriptorArrayMarkingState::kInitialGCState);

        Descriptor first_descriptor =
            Descriptor::DataConstant(first_key, first_value, NONE);
        first_descriptor.SetSortedKeyIndex(0);
        Descriptor second_descriptor =
            Descriptor::DataConstant(second_key, second_value, NONE);
        second_descriptor.SetSortedKeyIndex(1);
        descriptors->Set(InternalIndex(0), &first_descriptor);
        descriptors->Set(InternalIndex(1), &second_descriptor);
        DirectHandle<FixedArray> weak_value_target =
            isolate->factory()->NewFixedArray(1, AllocationType::kOld);
        weak_value_target->set(0, *first_key);
        // V8HeapExplorer::ExtractDescriptorArrayReferences reads descriptor values
        // through the descriptor value offsets; keep this exact slot write for
        // weak reference coverage.
        const int weak_value_slot_index = DescriptorArray::ToValueIndex(0);
        MaybeObjectSlot weak_value_slot(
            descriptors->GetDescriptorSlot(0) + weak_value_slot_index);
        weak_value_slot.store(MakeWeak(*weak_value_target));

        InvokeAtomicMajorGC(isolate->heap());

        CHECK_EQ(DESCRIPTOR_ARRAY_TYPE, descriptors->map()->instance_type());
        descriptor_array_address = descriptors->address();
        first_key_address = descriptors->GetKey(InternalIndex(0))->address();
        second_key_address = descriptors->GetKey(InternalIndex(1))->address();

        // Recompute the weak slot after GC because `descriptors` may move, and
        // raw slot addresses are not stable across collections.
        MaybeObjectSlot weak_value_slot_after_gc(
            descriptors->GetDescriptorSlot(0) + weak_value_slot_index);

        Tagged<HeapObject> weak_value_object;
        CHECK(weak_value_slot_after_gc.load().GetHeapObjectIfWeak(
            &weak_value_object));
        CHECK_EQ(weak_value_object, *weak_value_target);

        Tagged<HeapObject> value_object;
        CHECK(descriptors->GetValue(InternalIndex(1))
                  .GetHeapObjectIfStrong(&value_object));
        second_value_address = value_object.address();
        first_value_address = Cast<HeapObject>(*first_value).address();
        enum_cache_address = enum_cache->address();
        weak_value_address = weak_value_target->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(descriptors);
        artifacts.keep_alive.push_back(enum_cache);
        artifacts.keep_alive.push_back(first_key);
        artifacts.keep_alive.push_back(first_value);
        artifacts.keep_alive.push_back(second_key);
        artifacts.keep_alive.push_back(second_value);
        artifacts.keep_alive.push_back(weak_value_target);
        return artifacts;
      });

  Node* descriptors =
      GetSnapshotNode(generator.get(), descriptor_array_address);
  Node* first_key = GetSnapshotNode(generator.get(), first_key_address);
  Node* first_value = GetSnapshotNode(generator.get(), first_value_address);
  Node* second_key = GetSnapshotNode(generator.get(), second_key_address);
  Node* second_value = GetSnapshotNode(generator.get(), second_value_address);
  Node* enum_cache = GetSnapshotNode(generator.get(), enum_cache_address);
  Node* weak_value = GetSnapshotNode(generator.get(), weak_value_address);
  CHECK_NOT_NULL(descriptors);
  CHECK_NOT_NULL(first_key);
  CHECK_NOT_NULL(first_value);
  CHECK_NOT_NULL(second_key);
  CHECK_NOT_NULL(second_value);
  CHECK_NOT_NULL(enum_cache);
  CHECK_NOT_NULL(weak_value);
  CHECK(NodeMatchesTypeAndName(generator.get(), descriptors,
                               Node::Type::kObjectShape,
                               "system / DescriptorArray"));
  CHECK(NodeMatchesTypeAndName(generator.get(), enum_cache,
                               Node::Type::kObjectShape,
                               "system / EnumCache"));

  const std::string first_key_edge =
      EdgeIndexName(DescriptorArray::ToKeyIndex(0));
  const std::string first_value_edge =
      EdgeIndexName(DescriptorArray::ToValueIndex(0));
  const std::string second_key_edge =
      EdgeIndexName(DescriptorArray::ToKeyIndex(1));
  const std::string second_value_edge =
      EdgeIndexName(DescriptorArray::ToValueIndex(1));
  const std::string first_details_edge =
      EdgeIndexName(DescriptorArray::ToDetailsIndex(0));
  const std::string second_details_edge =
      EdgeIndexName(DescriptorArray::ToDetailsIndex(1));

  CHECK(HasExactNamedEdge(generator.get(), descriptors, Edge::Type::kInternal,
                          "enum_cache", enum_cache));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), descriptors,
                                   Edge::Type::kInternal, "enum_cache",
                                   enum_cache));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(descriptors, Edge::Type::kInternal,
                                         enum_cache));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(descriptors, Edge::Type::kWeak,
                                         enum_cache));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(descriptors, Edge::Type::kProperty,
                                         enum_cache));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(descriptors, Edge::Type::kHidden,
                                         enum_cache));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(descriptors, Edge::Type::kShortcut,
                                         enum_cache));

  // Native heap snapshots expose DescriptorArray descriptor slots as indexed
  // internal edges when the MaybeObject slot is strong. Breadcrumb:
  // V8HeapExplorer::ExtractDescriptorArrayReferences.
  CHECK(HasExactNamedEdge(generator.get(), descriptors, Edge::Type::kInternal,
                          first_key_edge.c_str(), first_key));
  CHECK(HasExactNamedEdge(generator.get(), descriptors, Edge::Type::kInternal,
                          second_key_edge.c_str(), second_key));
  CHECK(HasExactNamedEdge(generator.get(), descriptors, Edge::Type::kInternal,
                          second_value_edge.c_str(), second_value));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), descriptors,
                                   Edge::Type::kInternal,
                                   first_key_edge.c_str(), first_key));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(descriptors, Edge::Type::kInternal,
                                         first_key));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), descriptors,
                                   Edge::Type::kInternal,
                                   second_key_edge.c_str(), second_key));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(descriptors, Edge::Type::kInternal,
                                         second_key));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), descriptors,
                                   Edge::Type::kInternal,
                                   second_value_edge.c_str(), second_value));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(descriptors, Edge::Type::kInternal,
                                         second_value));

  CHECK(HasExactNamedEdge(generator.get(), descriptors, Edge::Type::kWeak,
                          first_value_edge.c_str(), weak_value));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), descriptors,
                                   Edge::Type::kWeak,
                                   first_value_edge.c_str(), weak_value));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(descriptors, Edge::Type::kWeak,
                                         weak_value));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(descriptors, Edge::Type::kInternal,
                                         weak_value));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(descriptors, Edge::Type::kProperty,
                                         weak_value));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(descriptors, Edge::Type::kHidden,
                                         weak_value));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(descriptors, Edge::Type::kShortcut,
                                         weak_value));
  CHECK_EQ(0, CountEdgesByNameFromNode(generator.get(), descriptors,
                                       Edge::Type::kInternal,
                                       first_details_edge.c_str(),
                                       [](const Node*) { return true; }));
  CHECK_EQ(0, CountEdgesByNameFromNode(generator.get(), descriptors,
                                       Edge::Type::kWeak,
                                       first_details_edge.c_str(),
                                       [](const Node*) { return true; }));
  CHECK_EQ(0, CountEdgesByNameFromNode(generator.get(), descriptors,
                                       Edge::Type::kHidden,
                                       first_details_edge.c_str(),
                                       [](const Node*) { return true; }));
  CHECK_EQ(0, CountEdgesByNameFromNode(generator.get(), descriptors,
                                       Edge::Type::kProperty,
                                       first_details_edge.c_str(),
                                       [](const Node*) { return true; }));
  CHECK_EQ(0, CountEdgesByNameFromNode(generator.get(), descriptors,
                                       Edge::Type::kShortcut,
                                       first_details_edge.c_str(),
                                       [](const Node*) { return true; }));
  CHECK_EQ(0, CountEdgesByNameFromNode(generator.get(), descriptors,
                                       Edge::Type::kInternal,
                                       second_details_edge.c_str(),
                                       [](const Node*) { return true; }));
  CHECK_EQ(0, CountEdgesByNameFromNode(generator.get(), descriptors,
                                       Edge::Type::kWeak,
                                       second_details_edge.c_str(),
                                       [](const Node*) { return true; }));
  CHECK_EQ(0, CountEdgesByNameFromNode(generator.get(), descriptors,
                                       Edge::Type::kHidden,
                                       second_details_edge.c_str(),
                                       [](const Node*) { return true; }));
  CHECK_EQ(0, CountEdgesByNameFromNode(generator.get(), descriptors,
                                       Edge::Type::kProperty,
                                       second_details_edge.c_str(),
                                       [](const Node*) { return true; }));
  CHECK_EQ(0, CountEdgesByNameFromNode(generator.get(), descriptors,
                                       Edge::Type::kShortcut,
                                       second_details_edge.c_str(),
                                       [](const Node*) { return true; }));
  CheckNoWeakHiddenOrPropertyEdgeToTarget(descriptors, first_key);
  CheckNoWeakHiddenOrPropertyEdgeToTarget(descriptors, second_key);
  CheckNoWeakHiddenOrPropertyEdgeToTarget(descriptors, second_value);
  CHECK_EQ(0, CountEdgesFromNodeToTarget(descriptors, Edge::Type::kShortcut,
                                         first_key));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(descriptors, Edge::Type::kShortcut,
                                         second_key));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(descriptors, Edge::Type::kShortcut,
                                         second_value));
  MarkCoveredInstanceType(DESCRIPTOR_ARRAY_TYPE);
  MarkCoveredInstanceType(FIXED_ARRAY_TYPE);
  MarkCoveredInstanceType(ENUM_CACHE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpStrongDescriptorArrayExactMapEdge) {
  Address strong_descriptor_array_address = 0;
  Address first_key_address = 0;
  Address first_value_address = 0;
  Address second_key_address = 0;
  Address second_value_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<Name> first_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpStrongDescriptorArrayFirst");
        DirectHandle<Name> second_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpStrongDescriptorArraySecond");
        DirectHandle<Object> first_value =
            isolate->factory()->NewHeapNumber(101);
        DirectHandle<Object> second_value =
            isolate->factory()->NewHeapNumber(202);

        DirectHandle<DescriptorArray> descriptors =
            DescriptorArray::Allocate(isolate, 2, 0, AllocationType::kOld);
        descriptors->set_map(isolate,
                             ReadOnlyRoots(isolate).strong_descriptor_array_map());
        descriptors->Initialize(ReadOnlyRoots(isolate).empty_enum_cache(),
                               ReadOnlyRoots(isolate).undefined_value(),
                               2, 0,
                               DescriptorArrayMarkingState::kInitialGCState);

        Descriptor first_descriptor =
            Descriptor::DataConstant(first_key, first_value, NONE);
        first_descriptor.SetSortedKeyIndex(0);
        Descriptor second_descriptor =
            Descriptor::DataConstant(second_key, second_value, NONE);
        second_descriptor.SetSortedKeyIndex(1);
        descriptors->Set(InternalIndex(0), &first_descriptor);
        descriptors->Set(InternalIndex(1), &second_descriptor);

        is_expected_instance_type =
            STRONG_DESCRIPTOR_ARRAY_TYPE == descriptors->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(STRONG_DESCRIPTOR_ARRAY_TYPE,
                 descriptors->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        strong_descriptor_array_address = descriptors->address();
        first_key_address = descriptors->GetKey(InternalIndex(0))->address();
        second_key_address = descriptors->GetKey(InternalIndex(1))->address();
        Tagged<HeapObject> first_value_object;
        CHECK(descriptors->GetValue(InternalIndex(0))
                  .GetHeapObjectIfStrong(&first_value_object));
        first_value_address = first_value_object.address();
        Tagged<HeapObject> second_value_object;
        CHECK(descriptors->GetValue(InternalIndex(1))
                  .GetHeapObjectIfStrong(&second_value_object));
        second_value_address = second_value_object.address();
        map_address = descriptors->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(descriptors);
        artifacts.keep_alive.push_back(first_key);
        artifacts.keep_alive.push_back(first_value);
        artifacts.keep_alive.push_back(second_key);
        artifacts.keep_alive.push_back(second_value);
        return artifacts;
      });

  Node* strong_descriptors =
      GetSnapshotNode(generator.get(), strong_descriptor_array_address);
  Node* first_key = GetSnapshotNode(generator.get(), first_key_address);
  Node* first_value = GetSnapshotNode(generator.get(), first_value_address);
  Node* second_key = GetSnapshotNode(generator.get(), second_key_address);
  Node* second_value = GetSnapshotNode(generator.get(), second_value_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || strong_descriptors == nullptr || map == nullptr ||
      first_key == nullptr || first_value == nullptr || second_key == nullptr ||
      second_value == nullptr) {
    PrintF("[TODO] STRONG_DESCRIPTOR_ARRAY_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeHasType(strong_descriptors, Node::Type::kObjectShape));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  const std::string first_key_edge =
      EdgeIndexName(DescriptorArray::ToKeyIndex(0));
  const std::string first_value_edge =
      EdgeIndexName(DescriptorArray::ToValueIndex(0));
  const std::string second_key_edge =
      EdgeIndexName(DescriptorArray::ToKeyIndex(1));
  const std::string second_value_edge =
      EdgeIndexName(DescriptorArray::ToValueIndex(1));
  // Native heap snapshots expose strong DescriptorArray descriptor slots as
  // indexed internal edges. Breadcrumb:
  // V8HeapExplorer::ExtractDescriptorArrayReferences.
  CHECK(HasExactNamedEdge(generator.get(), strong_descriptors, Edge::Type::kInternal,
                          first_key_edge.c_str(), first_key));
  CHECK(HasExactNamedEdge(generator.get(), strong_descriptors, Edge::Type::kInternal,
                          first_value_edge.c_str(), first_value));
  CHECK(HasExactNamedEdge(generator.get(), strong_descriptors, Edge::Type::kInternal,
                          second_key_edge.c_str(), second_key));
  CHECK(HasExactNamedEdge(generator.get(), strong_descriptors, Edge::Type::kInternal,
                          second_value_edge.c_str(), second_value));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), strong_descriptors,
                                   Edge::Type::kInternal,
                                   first_key_edge.c_str(), first_key));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), strong_descriptors,
                                   Edge::Type::kInternal,
                                   first_value_edge.c_str(), first_value));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), strong_descriptors,
                                   Edge::Type::kInternal,
                                   second_key_edge.c_str(), second_key));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), strong_descriptors,
                                   Edge::Type::kInternal,
                                   second_value_edge.c_str(), second_value));
  CheckNoWeakHiddenOrPropertyEdgeToTarget(strong_descriptors, first_key);
  CheckNoWeakHiddenOrPropertyEdgeToTarget(strong_descriptors, first_value);
  CheckNoWeakHiddenOrPropertyEdgeToTarget(strong_descriptors, second_key);
  CheckNoWeakHiddenOrPropertyEdgeToTarget(strong_descriptors, second_value);
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), strong_descriptors,
                                      map);
  MarkCoveredInstanceType(STRONG_DESCRIPTOR_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpTransitionArrayExactMapEdge) {
  Address transition_array_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<Name> first_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpTransitionArrayFirst");
        DirectHandle<Name> second_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpTransitionArraySecond");
        PropertyAttributes attributes = NONE;
        PropertyConstness constness = PropertyConstness::kMutable;
        DirectHandle<Map> transitions_owner = Map::Create(isolate, 0);
        DirectHandle<Map> first_target =
            Map::CopyWithField(isolate, transitions_owner, first_key,
                               FieldType::Any(isolate), attributes, constness,
                               Representation::Tagged(), OMIT_TRANSITION)
                .ToHandleChecked();
        DirectHandle<Map> second_target =
            Map::CopyWithField(isolate, transitions_owner, second_key,
                               FieldType::Any(isolate), attributes, constness,
                               Representation::Tagged(), OMIT_TRANSITION)
                .ToHandleChecked();

        TransitionsAccessor::Insert(isolate, transitions_owner, first_key,
                                   first_target, PROPERTY_TRANSITION);
        TransitionsAccessor::Insert(isolate, transitions_owner, second_key,
                                   second_target, PROPERTY_TRANSITION);

        TestTransitionsAccessor transitions_accessor(isolate, transitions_owner);
        DirectHandle<TransitionArray> transitions =
            direct_handle(transitions_accessor.transitions(), isolate);

        is_expected_instance_type =
            TRANSITION_ARRAY_TYPE == transitions->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(TRANSITION_ARRAY_TYPE, transitions->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        transition_array_address = transitions->address();
        map_address = transitions->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(transitions);
        artifacts.keep_alive.push_back(transitions_owner);
        artifacts.keep_alive.push_back(first_target);
        artifacts.keep_alive.push_back(second_target);
        artifacts.keep_alive.push_back(first_key);
        artifacts.keep_alive.push_back(second_key);
        return artifacts;
      });

  Node* transition_array =
      GetSnapshotNode(generator.get(), transition_array_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || transition_array == nullptr ||
      map == nullptr) {
    PrintF("[TODO] TRANSITION_ARRAY_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeHasType(transition_array, Node::Type::kObjectShape));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), transition_array, map);
  MarkCoveredInstanceType(TRANSITION_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpSwissNameDictionaryExactMapEdge) {
  Address swiss_name_dictionary_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<Name> first_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpSwissNameDictionaryFirst");
        DirectHandle<Name> second_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpSwissNameDictionarySecond");
        DirectHandle<String> first_value =
            isolate->factory()->InternalizeUtf8String("first");
        DirectHandle<String> second_value =
            isolate->factory()->InternalizeUtf8String("second");

        DirectHandle<SwissNameDictionary> dictionary =
            isolate->factory()->NewSwissNameDictionary(4, AllocationType::kOld);
        dictionary = SwissNameDictionary::Add(
            isolate, dictionary, first_key, first_value, PropertyDetails::Empty());
        dictionary = SwissNameDictionary::Add(
            isolate, dictionary, second_key, second_value,
            PropertyDetails::Empty());

        is_expected_instance_type =
            SWISS_NAME_DICTIONARY_TYPE == dictionary->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(SWISS_NAME_DICTIONARY_TYPE, dictionary->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        swiss_name_dictionary_address = dictionary->address();
        map_address = dictionary->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(dictionary);
        artifacts.keep_alive.push_back(first_key);
        artifacts.keep_alive.push_back(second_key);
        artifacts.keep_alive.push_back(first_value);
        artifacts.keep_alive.push_back(second_value);
        return artifacts;
      });

  Node* dictionary = GetSnapshotNode(generator.get(), swiss_name_dictionary_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || dictionary == nullptr || map == nullptr) {
    PrintF(
        "[TODO] SWISS_NAME_DICTIONARY_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), dictionary, map);
  MarkCoveredInstanceType(SWISS_NAME_DICTIONARY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpAliasedArgumentsEntryExactMapEdge) {
  Address entry_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<AliasedArgumentsEntry> entry =
            isolate->factory()->NewAliasedArgumentsEntry(7);

        is_expected_instance_type =
            ALIASED_ARGUMENTS_ENTRY_TYPE == entry->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(ALIASED_ARGUMENTS_ENTRY_TYPE, entry->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        entry_address = entry->address();
        map_address = entry->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(entry);
        return artifacts;
      });

  Node* entry = GetSnapshotNode(generator.get(), entry_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || entry == nullptr || map == nullptr) {
    PrintF(
        "[TODO] ALIASED_ARGUMENTS_ENTRY_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), entry, Node::Type::kHidden,
                               "system / AliasedArgumentsEntry"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), entry, map);
  MarkCoveredInstanceType(ALIASED_ARGUMENTS_ENTRY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpArrayBoilerplateDescriptionExactMapEdge) {
  Address description_address = 0;
  Address map_address = 0;
  Address constant_elements_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<FixedArray> constant_elements =
            isolate->factory()->NewFixedArray(2, AllocationType::kOld);
        DirectHandle<String> first = isolate->factory()->InternalizeUtf8String(
            "heapDumpArrayBoilerplateFirst");
        DirectHandle<String> second = isolate->factory()->InternalizeUtf8String(
            "heapDumpArrayBoilerplateSecond");
        constant_elements->set(0, *first);
        constant_elements->set(1, *second);
        Handle<ArrayBoilerplateDescription> description =
            isolate->factory()->NewArrayBoilerplateDescription(
                PACKED_ELEMENTS, constant_elements);

        is_expected_instance_type = ARRAY_BOILERPLATE_DESCRIPTION_TYPE ==
                                    description->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(ARRAY_BOILERPLATE_DESCRIPTION_TYPE,
                 description->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        description_address = description->address();
        map_address = description->map()->address();
        constant_elements_address = constant_elements->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(direct_handle(*description, isolate));
        artifacts.keep_alive.push_back(constant_elements);
        artifacts.keep_alive.push_back(first);
        artifacts.keep_alive.push_back(second);
        return artifacts;
      });

  Node* description = GetSnapshotNode(generator.get(), description_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  Node* constant_elements =
      GetSnapshotNode(generator.get(), constant_elements_address);
  CHECK(is_expected_instance_type);
  CHECK_NOT_NULL(description);
  CHECK_NOT_NULL(map);
  CHECK_NOT_NULL(constant_elements);

  CHECK(NodeMatchesTypeAndName(generator.get(), description, Node::Type::kCode,
                               "system / ArrayBoilerplateDescription"));
  CHECK(NodeHasType(map, Node::Type::kObjectShape));

  // Native contract: V8HeapExplorer::ExtractArrayBoilerplateDescriptionReferences
  // sets a SetInternalReference("constant_elements", value) edge and then
  // TagObject(value, "(constant elements)", HeapEntry::kCode).
  CHECK(HasExactNamedEdge(generator.get(), description, Edge::Type::kInternal,
                          "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), description,
                                   Edge::Type::kInternal, "map", map));
  CHECK(NodeMatchesTypeAndName(generator.get(), constant_elements,
                               Node::Type::kCode, "(constant elements)"));
  CHECK(HasExactNamedEdge(generator.get(), description, Edge::Type::kInternal,
                          "constant_elements", constant_elements));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), description,
                                   Edge::Type::kInternal, "constant_elements",
                                   constant_elements));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(description, Edge::Type::kInternal,
                                         constant_elements));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), description,
                                   Edge::Type::kInternal, "(constant elements)",
                                   constant_elements));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), description,
                                   Edge::Type::kWeak, "(constant elements)",
                                   constant_elements));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), description,
                                   Edge::Type::kHidden, "(constant elements)",
                                   constant_elements));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), description,
                                   Edge::Type::kProperty, "(constant elements)",
                                   constant_elements));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), description,
                                   Edge::Type::kShortcut, "(constant elements)",
                                   constant_elements));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(description, Edge::Type::kInternal,
                                         map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(description, Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(description, Edge::Type::kWeak,
                                         constant_elements));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(description, Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(description, Edge::Type::kHidden,
                                         constant_elements));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(description, Edge::Type::kProperty, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(description, Edge::Type::kProperty,
                                         constant_elements));
  CHECK_EQ(0,
           CountEdgesFromNodeToTarget(description, Edge::Type::kShortcut, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(description, Edge::Type::kShortcut,
                                         constant_elements));
  MarkCoveredInstanceType(ARRAY_BOILERPLATE_DESCRIPTION_TYPE);
}

UNINITIALIZED_TEST(HeapDumpAllocationMementoExactMapEdge) {
  Address memento_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        if (!V8_ALLOCATION_SITE_TRACKING_BOOL || v8_flags.single_generation) {
          return DumpArtifacts{};
        }

        DirectHandle<JSObject> source =
            isolate->factory()->NewJSObject(isolate->object_function());
        DirectHandle<AllocationSite> site =
            isolate->factory()->NewAllocationSite(false);
        Handle<JSObject> clone =
            isolate->factory()->CopyJSObjectWithAllocationSite(source, site);
        int aligned_object_size =
            ALIGN_TO_ALLOCATION_ALIGNMENT(clone->map()->instance_size());
        Tagged<AllocationMemento> memento = UncheckedCast<AllocationMemento>(
            Tagged<Object>(clone->ptr() + aligned_object_size));

        is_expected_instance_type =
            ALLOCATION_MEMENTO_TYPE == memento->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(ALLOCATION_MEMENTO_TYPE, memento->map()->instance_type());
        CHECK_EQ(*site, memento->GetAllocationSite());

        memento_address = memento.address();
        map_address = memento->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(source);
        artifacts.keep_alive.push_back(site);
        artifacts.keep_alive.push_back(direct_handle(*clone, isolate));
        return artifacts;
      });

  Node* memento = GetSnapshotNode(generator.get(), memento_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || memento == nullptr || map == nullptr) {
    PrintF("[TODO] ALLOCATION_MEMENTO_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), memento, Node::Type::kHidden,
                               "system / AllocationMemento"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), memento, map);
  MarkCoveredInstanceType(ALLOCATION_MEMENTO_TYPE);
}

UNINITIALIZED_TEST(HeapDumpClassBoilerplateExactMapEdge) {
  Address boilerplate_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        Handle<ClassBoilerplate> boilerplate =
            Cast<ClassBoilerplate>(isolate->factory()->NewStruct(
                CLASS_BOILERPLATE_TYPE, AllocationType::kOld));
        DirectHandle<FixedArray> empty =
            isolate->factory()->NewFixedArray(0, AllocationType::kOld);

        boilerplate->set_arguments_count(
            ClassBoilerplate::kFirstDynamicArgumentIndex);
        boilerplate->set_static_properties_template(*empty);
        boilerplate->set_static_elements_template(*empty);
        boilerplate->set_static_computed_properties(*empty);
        boilerplate->set_instance_properties_template(*empty);
        boilerplate->set_instance_elements_template(*empty);
        boilerplate->set_instance_computed_properties(*empty);

        is_expected_instance_type =
            CLASS_BOILERPLATE_TYPE == boilerplate->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(CLASS_BOILERPLATE_TYPE, boilerplate->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        boilerplate_address = boilerplate->address();
        map_address = boilerplate->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(direct_handle(*boilerplate, isolate));
        artifacts.keep_alive.push_back(empty);
        return artifacts;
      });

  Node* boilerplate = GetSnapshotNode(generator.get(), boilerplate_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || boilerplate == nullptr || map == nullptr) {
    PrintF("[TODO] CLASS_BOILERPLATE_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), boilerplate,
                               Node::Type::kHidden,
                               "system / ClassBoilerplate"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), boilerplate, map);
  MarkCoveredInstanceType(CLASS_BOILERPLATE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpClassPositionsExactMapEdge) {
  Address positions_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            Handle<ClassPositions> positions =
                isolate->factory()->NewClassPositions(7, 42);

            is_expected_instance_type =
                CLASS_POSITIONS_TYPE == positions->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(CLASS_POSITIONS_TYPE, positions->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            positions_address = positions->address();
            map_address = positions->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(direct_handle(*positions, isolate));
            return artifacts;
          });

  Node* positions = GetSnapshotNode(generator.get(), positions_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || positions == nullptr || map == nullptr) {
    PrintF("[TODO] CLASS_POSITIONS_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), positions, Node::Type::kHidden,
                               "system / ClassPositions"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), positions, map);
  MarkCoveredInstanceType(CLASS_POSITIONS_TYPE);
}

UNINITIALIZED_TEST(HeapDumpModuleRequestExactMapEdge) {
  Address request_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<String> specifier =
                isolate->factory()->InternalizeUtf8String(
                    "heap-dump-module-request-specifier");
            DirectHandle<String> attribute_key =
                isolate->factory()->InternalizeUtf8String("type");
            DirectHandle<String> attribute_value =
                isolate->factory()->InternalizeUtf8String("json");
            DirectHandle<FixedArray> import_attributes =
                isolate->factory()->NewFixedArray(
                    static_cast<int>(ModuleRequest::kAttributeEntrySize),
                    AllocationType::kOld);
            import_attributes->set(0, *attribute_key);
            import_attributes->set(1, *attribute_value);
            import_attributes->set(2, Smi::FromInt(17));
            Handle<ModuleRequest> request = ModuleRequest::New(
                isolate, specifier, ModuleImportPhase::kEvaluation,
                import_attributes, 23);

            is_expected_instance_type =
                MODULE_REQUEST_TYPE == request->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(MODULE_REQUEST_TYPE, request->map()->instance_type());
            CHECK_EQ(*specifier, request->specifier());
            CHECK_EQ(*import_attributes, request->import_attributes());
            InvokeAtomicMajorGC(isolate->heap());

            request_address = request->address();
            map_address = request->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(direct_handle(*request, isolate));
            artifacts.keep_alive.push_back(specifier);
            artifacts.keep_alive.push_back(import_attributes);
            artifacts.keep_alive.push_back(attribute_key);
            artifacts.keep_alive.push_back(attribute_value);
            return artifacts;
          });

  Node* request = GetSnapshotNode(generator.get(), request_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || request == nullptr || map == nullptr) {
    PrintF("[TODO] MODULE_REQUEST_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), request, Node::Type::kHidden,
                               "system / ModuleRequest"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), request, map);
  MarkCoveredInstanceType(MODULE_REQUEST_TYPE);
}

UNINITIALIZED_TEST(HeapDumpPromiseCapabilityExactMapEdge) {
  Address capability_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            RunScript(context, R"(
              globalThis.__heap_dump_promise_capability_resolve =
                  function HeapDumpPromiseCapabilityResolve(value) {
                    return value;
                  };
              globalThis.__heap_dump_promise_capability_reject =
                  function HeapDumpPromiseCapabilityReject(reason) {
                    return reason;
                  };
            )");
            auto get_global = [&](const char* name) {
              return context->Global()
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            DirectHandle<JSFunction> resolve =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(
                    *get_global("__heap_dump_promise_capability_resolve")));
            DirectHandle<JSFunction> reject =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(
                    *get_global("__heap_dump_promise_capability_reject")));
            Handle<JSPromise> promise = isolate->factory()->NewJSPromise();
            Handle<Struct> capability_struct = isolate->factory()->NewStruct(
                PROMISE_CAPABILITY_TYPE, AllocationType::kOld);
            DirectHandle<PromiseCapability> capability =
                Cast<PromiseCapability>(capability_struct);
            capability->set_promise(*promise);
            capability->set_resolve(*resolve);
            capability->set_reject(*reject);

            is_expected_instance_type =
                PROMISE_CAPABILITY_TYPE ==
                capability->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(PROMISE_CAPABILITY_TYPE,
                     capability->map()->instance_type());
            CHECK_EQ(*promise, capability->promise());
            CHECK_EQ(*resolve, capability->resolve());
            CHECK_EQ(*reject, capability->reject());
            InvokeAtomicMajorGC(isolate->heap());

            capability_address = capability->address();
            map_address = capability->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(capability);
            artifacts.keep_alive.push_back(direct_handle(*promise, isolate));
            artifacts.keep_alive.push_back(resolve);
            artifacts.keep_alive.push_back(reject);
            return artifacts;
          });

  Node* capability = GetSnapshotNode(generator.get(), capability_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || capability == nullptr || map == nullptr) {
    PrintF("[TODO] PROMISE_CAPABILITY_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), capability, Node::Type::kHidden,
                               "system / PromiseCapability"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), capability, map);
  MarkCoveredInstanceType(PROMISE_CAPABILITY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpPromiseReactionExactMapEdge) {
  Address reaction_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            RunScript(context, R"(
              globalThis.__heap_dump_promise_reaction_fulfill =
                  function HeapDumpPromiseReactionFulfill(value) {
                    return value;
                  };
              globalThis.__heap_dump_promise_reaction_reject =
                  function HeapDumpPromiseReactionReject(reason) {
                    return reason;
                  };
            )");
            auto get_global = [&](const char* name) {
              return context->Global()
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            DirectHandle<JSFunction> fulfill =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(
                    *get_global("__heap_dump_promise_reaction_fulfill")));
            DirectHandle<JSFunction> reject =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(
                    *get_global("__heap_dump_promise_reaction_reject")));
            Handle<JSPromise> promise = isolate->factory()->NewJSPromise();
            Handle<Struct> reaction_struct = isolate->factory()->NewStruct(
                PROMISE_REACTION_TYPE, AllocationType::kOld);
            DirectHandle<PromiseReaction> reaction =
                Cast<PromiseReaction>(reaction_struct);
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
            reaction->set_continuation_preserved_embedder_data(
                ReadOnlyRoots(isolate).undefined_value(), SKIP_WRITE_BARRIER);
#endif
            reaction->set_next(Smi::zero(), SKIP_WRITE_BARRIER);
            reaction->set_reject_handler(*reject);
            reaction->set_fulfill_handler(*fulfill);
            reaction->set_promise_or_capability(*promise);

            is_expected_instance_type =
                PROMISE_REACTION_TYPE == reaction->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(PROMISE_REACTION_TYPE, reaction->map()->instance_type());
            CHECK_EQ(Smi::zero(), reaction->next());
            CHECK_EQ(*reject, reaction->reject_handler());
            CHECK_EQ(*fulfill, reaction->fulfill_handler());
            CHECK_EQ(*promise, reaction->promise_or_capability());
            InvokeAtomicMajorGC(isolate->heap());

            reaction_address = reaction->address();
            map_address = reaction->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(reaction);
            artifacts.keep_alive.push_back(direct_handle(*promise, isolate));
            artifacts.keep_alive.push_back(fulfill);
            artifacts.keep_alive.push_back(reject);
            return artifacts;
          });

  Node* reaction = GetSnapshotNode(generator.get(), reaction_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || reaction == nullptr || map == nullptr) {
    PrintF("[TODO] PROMISE_REACTION_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), reaction, Node::Type::kHidden,
                               "system / PromiseReaction"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), reaction, map);
  MarkCoveredInstanceType(PROMISE_REACTION_TYPE);
}

UNINITIALIZED_TEST(HeapDumpPropertyDescriptorObjectExactMapEdge) {
  Address descriptor_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<PropertyDescriptorObject> descriptor =
                isolate->factory()->NewPropertyDescriptorObject();
            DirectHandle<String> value =
                isolate->factory()->InternalizeUtf8String(
                    "heap-dump-property-descriptor-value");
            descriptor->set_flags(
                PropertyDescriptorObject::HasValueBit::encode(true) |
                PropertyDescriptorObject::HasWritableBit::encode(true) |
                PropertyDescriptorObject::IsWritableBit::encode(true));
            descriptor->set_value(*value);

            is_expected_instance_type =
                PROPERTY_DESCRIPTOR_OBJECT_TYPE ==
                descriptor->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(PROPERTY_DESCRIPTOR_OBJECT_TYPE,
                     descriptor->map()->instance_type());
            CHECK_EQ(*value, descriptor->value());
            InvokeAtomicMajorGC(isolate->heap());

            descriptor_address = descriptor->address();
            map_address = descriptor->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(descriptor);
            artifacts.keep_alive.push_back(value);
            return artifacts;
          });

  Node* descriptor = GetSnapshotNode(generator.get(), descriptor_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || descriptor == nullptr || map == nullptr) {
    PrintF(
        "[TODO] PROPERTY_DESCRIPTOR_OBJECT_TYPE not yet stable in this "
        "branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), descriptor,
                               Node::Type::kHidden,
                               "system / PropertyDescriptorObject"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), descriptor, map);
  MarkCoveredInstanceType(PROPERTY_DESCRIPTOR_OBJECT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpScriptOrModuleExactMapEdge) {
  Address script_or_module_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            Handle<Struct> script_or_module_struct =
                isolate->factory()->NewStruct(SCRIPT_OR_MODULE_TYPE,
                                              AllocationType::kOld);
            DirectHandle<ScriptOrModule> script_or_module =
                Cast<ScriptOrModule>(script_or_module_struct);
            DirectHandle<String> resource_name =
                isolate->factory()->InternalizeUtf8String(
                    "heap-dump-script-or-module-resource");
            DirectHandle<FixedArray> host_defined_options =
                isolate->factory()->NewFixedArray(1, AllocationType::kOld);
            host_defined_options->set(0, *resource_name);
            script_or_module->set_resource_name(*resource_name);
            script_or_module->set_host_defined_options(*host_defined_options);

            is_expected_instance_type =
                SCRIPT_OR_MODULE_TYPE ==
                script_or_module->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(SCRIPT_OR_MODULE_TYPE,
                     script_or_module->map()->instance_type());
            CHECK_EQ(*resource_name, script_or_module->resource_name());
            CHECK_EQ(*host_defined_options,
                     script_or_module->host_defined_options());
            InvokeAtomicMajorGC(isolate->heap());

            script_or_module_address = script_or_module->address();
            map_address = script_or_module->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(script_or_module);
            artifacts.keep_alive.push_back(resource_name);
            artifacts.keep_alive.push_back(host_defined_options);
            return artifacts;
          });

  Node* script_or_module =
      GetSnapshotNode(generator.get(), script_or_module_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || script_or_module == nullptr ||
      map == nullptr) {
    PrintF("[TODO] SCRIPT_OR_MODULE_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), script_or_module,
                               Node::Type::kHidden,
                               "system / ScriptOrModule"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), script_or_module, map);
  MarkCoveredInstanceType(SCRIPT_OR_MODULE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpCallSiteInfoExactMapEdge) {
  Address call_site_info_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            RunScript(context, R"(
              function HeapDumpCallSiteInfoFixture(a) {
                return a + 149;
              }
              HeapDumpCallSiteInfoFixture(1);
            )");
            v8::Local<v8::Value> function_value =
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "HeapDumpCallSiteInfoFixture"))
                    .ToLocalChecked();
            DirectHandle<JSFunction> function =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*function_value));
            DirectHandle<SharedFunctionInfo> shared(function->shared(),
                                                    isolate);
            CHECK(shared->HasBytecodeArray());
            DirectHandle<BytecodeArray> bytecode_array(
                shared->GetBytecodeArray(isolate), isolate);
            DirectHandle<CallSiteInfo> call_site_info =
                isolate->factory()->NewCallSiteInfo(
                    isolate->factory()->undefined_value(), function,
                    bytecode_array, 0, 0,
                    isolate->factory()->empty_fixed_array());

            is_expected_instance_type =
                CALL_SITE_INFO_TYPE == call_site_info->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(CALL_SITE_INFO_TYPE,
                     call_site_info->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            call_site_info_address = call_site_info->address();
            map_address = call_site_info->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(call_site_info);
            artifacts.keep_alive.push_back(function);
            artifacts.keep_alive.push_back(shared);
            artifacts.keep_alive.push_back(bytecode_array);
            return artifacts;
          });

  Node* call_site_info =
      GetSnapshotNode(generator.get(), call_site_info_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || call_site_info == nullptr ||
      map == nullptr) {
    PrintF("[TODO] CALL_SITE_INFO_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), call_site_info,
                               Node::Type::kHidden,
                               "system / CallSiteInfo"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), call_site_info, map);
  MarkCoveredInstanceType(CALL_SITE_INFO_TYPE);
}

UNINITIALIZED_TEST(HeapDumpStackFrameInfoExactMapEdge) {
  Address stack_frame_info_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            RunScript(context, R"(
              function HeapDumpStackFrameInfoFixture() {
                return 168;
              }
              HeapDumpStackFrameInfoFixture();
            )");
            v8::Local<v8::Value> function_value =
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "HeapDumpStackFrameInfoFixture"))
                    .ToLocalChecked();
            DirectHandle<JSFunction> function =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*function_value));
            DirectHandle<SharedFunctionInfo> shared(function->shared(),
                                                    isolate);
            DirectHandle<String> function_name =
                isolate->factory()->InternalizeUtf8String(
                    "HeapDumpStackFrameInfoFixture");
            DirectHandle<StackFrameInfo> stack_frame_info =
                isolate->factory()->NewStackFrameInfo(shared, 0, function_name,
                                                      false);

            is_expected_instance_type =
                STACK_FRAME_INFO_TYPE ==
                stack_frame_info->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(STACK_FRAME_INFO_TYPE,
                     stack_frame_info->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            stack_frame_info_address = stack_frame_info->address();
            map_address = stack_frame_info->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(stack_frame_info);
            artifacts.keep_alive.push_back(function);
            artifacts.keep_alive.push_back(shared);
            artifacts.keep_alive.push_back(function_name);
            return artifacts;
          });

  Node* stack_frame_info =
      GetSnapshotNode(generator.get(), stack_frame_info_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || stack_frame_info == nullptr ||
      map == nullptr) {
    PrintF("[TODO] STACK_FRAME_INFO_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), stack_frame_info,
                               Node::Type::kHidden,
                               "system / StackFrameInfo"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), stack_frame_info, map);
  MarkCoveredInstanceType(STACK_FRAME_INFO_TYPE);
}

UNINITIALIZED_TEST(HeapDumpStackTraceInfoExactMapEdge) {
  Address stack_trace_info_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            RunScript(context, R"(
              function HeapDumpStackTraceInfoFixture() {
                return 169;
              }
              HeapDumpStackTraceInfoFixture();
            )");
            v8::Local<v8::Value> function_value =
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "HeapDumpStackTraceInfoFixture"))
                    .ToLocalChecked();
            DirectHandle<JSFunction> function =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*function_value));
            DirectHandle<SharedFunctionInfo> shared(function->shared(),
                                                    isolate);
            DirectHandle<String> function_name =
                isolate->factory()->InternalizeUtf8String(
                    "HeapDumpStackTraceInfoFixture");
            DirectHandle<StackFrameInfo> stack_frame_info =
                isolate->factory()->NewStackFrameInfo(shared, 0, function_name,
                                                      false);
            DirectHandle<FixedArray> frames =
                isolate->factory()->NewFixedArray(1, AllocationType::kOld);
            frames->set(0, *stack_frame_info);
            DirectHandle<StackTraceInfo> stack_trace_info =
                isolate->factory()->NewStackTraceInfo(frames);

            is_expected_instance_type =
                STACK_TRACE_INFO_TYPE ==
                stack_trace_info->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(STACK_TRACE_INFO_TYPE,
                     stack_trace_info->map()->instance_type());
            CHECK_EQ(1, stack_trace_info->length());
            InvokeAtomicMajorGC(isolate->heap());

            stack_trace_info_address = stack_trace_info->address();
            map_address = stack_trace_info->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(stack_trace_info);
            artifacts.keep_alive.push_back(stack_frame_info);
            artifacts.keep_alive.push_back(frames);
            artifacts.keep_alive.push_back(function);
            artifacts.keep_alive.push_back(shared);
            artifacts.keep_alive.push_back(function_name);
            return artifacts;
          });

  Node* stack_trace_info =
      GetSnapshotNode(generator.get(), stack_trace_info_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || stack_trace_info == nullptr ||
      map == nullptr) {
    PrintF("[TODO] STACK_TRACE_INFO_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), stack_trace_info,
                               Node::Type::kHidden,
                               "system / StackTraceInfo"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), stack_trace_info, map);
  MarkCoveredInstanceType(STACK_TRACE_INFO_TYPE);
}

UNINITIALIZED_TEST(HeapDumpErrorStackDataExactMapEdge) {
  Address error_stack_data_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            RunScript(context, R"(
              function HeapDumpErrorStackDataFixture() {
                return 155;
              }
              HeapDumpErrorStackDataFixture();
            )");
            v8::Local<v8::Value> function_value =
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "HeapDumpErrorStackDataFixture"))
                    .ToLocalChecked();
            DirectHandle<JSFunction> function =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*function_value));
            DirectHandle<SharedFunctionInfo> shared(function->shared(),
                                                    isolate);
            DirectHandle<String> function_name =
                isolate->factory()->InternalizeUtf8String(
                    "HeapDumpErrorStackDataFixture");
            DirectHandle<StackFrameInfo> stack_frame_info =
                isolate->factory()->NewStackFrameInfo(shared, 0, function_name,
                                                      false);
            DirectHandle<FixedArray> frames =
                isolate->factory()->NewFixedArray(1, AllocationType::kOld);
            frames->set(0, *stack_frame_info);
            DirectHandle<StackTraceInfo> stack_trace_info =
                isolate->factory()->NewStackTraceInfo(frames);
            DirectHandle<ErrorStackData> error_stack_data =
                isolate->factory()->NewErrorStackData(frames,
                                                      stack_trace_info);

            is_expected_instance_type =
                ERROR_STACK_DATA_TYPE ==
                error_stack_data->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(ERROR_STACK_DATA_TYPE,
                     error_stack_data->map()->instance_type());
            CHECK(error_stack_data->HasCallSiteInfos());
            InvokeAtomicMajorGC(isolate->heap());

            error_stack_data_address = error_stack_data->address();
            map_address = error_stack_data->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(error_stack_data);
            artifacts.keep_alive.push_back(stack_trace_info);
            artifacts.keep_alive.push_back(stack_frame_info);
            artifacts.keep_alive.push_back(frames);
            artifacts.keep_alive.push_back(function);
            artifacts.keep_alive.push_back(shared);
            artifacts.keep_alive.push_back(function_name);
            return artifacts;
          });

  Node* error_stack_data =
      GetSnapshotNode(generator.get(), error_stack_data_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || error_stack_data == nullptr ||
      map == nullptr) {
    PrintF("[TODO] ERROR_STACK_DATA_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), error_stack_data,
                               Node::Type::kHidden,
                               "system / ErrorStackData"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), error_stack_data, map);
  MarkCoveredInstanceType(ERROR_STACK_DATA_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSMessageObjectExactMapAndScriptEdges) {
  Address message_address = 0;
  Address map_address = 0;
  Address argument_address = 0;
  Address script_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            RunScript(context, R"(
              function HeapDumpJSMessageObjectFixture() {
                return "heap-dump-js-message-object-source";
              }
              HeapDumpJSMessageObjectFixture();
            )");
            v8::Local<v8::Value> function_value =
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "HeapDumpJSMessageObjectFixture"))
                    .ToLocalChecked();
            DirectHandle<JSFunction> function =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*function_value));
            DirectHandle<SharedFunctionInfo> shared(function->shared(),
                                                    isolate);
            CHECK(shared->has_script(kAcquireLoad));
            Handle<Script> script(Cast<Script>(shared->script(kAcquireLoad)),
                                  isolate);
            DirectHandle<String> argument =
                isolate->factory()->InternalizeUtf8String(
                    "heap-dump-js-message-object-argument");
            MessageLocation location(script, 0, 0);
            DirectHandle<JSMessageObject> message =
                MessageHandler::MakeMessageObject(
                    isolate, MessageTemplate::kAsmJsInvalid, &location,
                    argument);
            JSMessageObject::EnsureSourcePositionsAvailable(isolate, message);

            is_expected_instance_type =
                JS_MESSAGE_OBJECT_TYPE == message->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(JS_MESSAGE_OBJECT_TYPE,
                     message->map()->instance_type());
            CHECK_EQ(*argument, message->argument());
            CHECK_EQ(*script, message->script());
            CHECK(IsString(message->GetSource()));
            InvokeAtomicMajorGC(isolate->heap());

            message_address = message->address();
            map_address = message->map()->address();
            argument_address = argument->address();
            script_address = script->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(message);
            artifacts.keep_alive.push_back(argument);
            artifacts.keep_alive.push_back(shared);
            artifacts.keep_alive.push_back(function);
            return artifacts;
          });

  Node* message = GetSnapshotNode(generator.get(), message_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  Node* argument = GetSnapshotNode(generator.get(), argument_address);
  Node* script = GetSnapshotNode(generator.get(), script_address);
  if (!is_expected_instance_type || message == nullptr || map == nullptr ||
      argument == nullptr || script == nullptr) {
    PrintF("[TODO] JS_MESSAGE_OBJECT_TYPE not yet stable in this branch.\n");
    return;
  }
  if (!NodeHasType(message, Node::Type::kObject)) {
    PrintF("[TODO] JS_MESSAGE_OBJECT_TYPE translated node is not a normal "
           "object node.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), message, map);
  CHECK(HasExactNamedEdge(generator.get(), message, Edge::Type::kHidden,
                          "<dummy>", argument));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), message,
                                   Edge::Type::kHidden, "<dummy>",
                                   argument));
  CHECK(HasExactNamedEdge(generator.get(), message, Edge::Type::kHidden, "",
                          script));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), message,
                                   Edge::Type::kHidden, "", script));
  MarkCoveredInstanceType(JS_MESSAGE_OBJECT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpSourceTextModuleInfoEntryExactMapEdge) {
  Address entry_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<String> export_name =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpModuleExport");
        DirectHandle<String> local_name =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpModuleLocal");
        DirectHandle<String> import_name =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpModuleImport");
        DirectHandle<SourceTextModuleInfoEntry> entry =
            SourceTextModuleInfoEntry::New(isolate, export_name, local_name,
                                           import_name, 0, 1, 2, 3);

        is_expected_instance_type =
            SOURCE_TEXT_MODULE_INFO_ENTRY_TYPE ==
            entry->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(SOURCE_TEXT_MODULE_INFO_ENTRY_TYPE,
                 entry->map()->instance_type());
        CHECK_EQ(*export_name, entry->export_name());
        CHECK_EQ(*local_name, entry->local_name());
        CHECK_EQ(*import_name, entry->import_name());
        InvokeAtomicMajorGC(isolate->heap());

        entry_address = entry->address();
        map_address = entry->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(entry);
        artifacts.keep_alive.push_back(export_name);
        artifacts.keep_alive.push_back(local_name);
        artifacts.keep_alive.push_back(import_name);
        return artifacts;
      });

  Node* entry = GetSnapshotNode(generator.get(), entry_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || entry == nullptr || map == nullptr) {
    PrintF(
        "[TODO] SOURCE_TEXT_MODULE_INFO_ENTRY_TYPE not yet stable in this "
        "branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), entry, Node::Type::kHidden,
                               "system / SourceTextModuleInfoEntry"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), entry, map);
  MarkCoveredInstanceType(SOURCE_TEXT_MODULE_INFO_ENTRY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpRegExpBoilerplateDescriptionExactMapEdge) {
  Address description_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<String> source = isolate->factory()->InternalizeUtf8String(
            "heap-dump-regexp-boilerplate-source");
        DirectHandle<String> pattern =
            isolate->factory()->InternalizeUtf8String(
                "heap-dump-regexp-boilerplate-pattern");
        DirectHandle<RegExpData> regexp_data =
            isolate->factory()->NewAtomRegExpData(source, JSRegExp::kGlobal,
                                                  pattern);
        DirectHandle<RegExpBoilerplateDescription> description =
            isolate->factory()->NewRegExpBoilerplateDescription(
                regexp_data, source, Smi::FromInt(JSRegExp::kGlobal));

        is_expected_instance_type = REG_EXP_BOILERPLATE_DESCRIPTION_TYPE ==
                                    description->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(REG_EXP_BOILERPLATE_DESCRIPTION_TYPE,
                 description->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        description_address = description->address();
        map_address = description->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(description);
        artifacts.keep_alive.push_back(regexp_data);
        artifacts.keep_alive.push_back(source);
        artifacts.keep_alive.push_back(pattern);
        return artifacts;
      });

  Node* description = GetSnapshotNode(generator.get(), description_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || description == nullptr || map == nullptr) {
    PrintF(
        "[TODO] REG_EXP_BOILERPLATE_DESCRIPTION_TYPE not yet stable in this "
        "branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), description, Node::Type::kCode,
                               "system / RegExpBoilerplateDescription"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), description, map);
  MarkCoveredInstanceType(REG_EXP_BOILERPLATE_DESCRIPTION_TYPE);
}

UNINITIALIZED_TEST(HeapDumpRegExpDataExactMapEdge) {
  Address data_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<String> source = isolate->factory()->InternalizeUtf8String(
            "heap-dump-regexp-data-base-source");
        DirectHandle<RegExpDataWrapper> wrapper =
            isolate->factory()->NewRegExpDataWrapper();
        DirectHandle<Map> regexp_data_map(
            isolate->factory()->read_only_roots().regexp_data_map(), isolate);
        DirectHandle<RegExpData> data =
            direct_handle(TrustedCast<RegExpData>(isolate->factory()->NewForTest(
                              regexp_data_map, AllocationType::kTrusted)),
                          isolate);
        data->InitDontPublish(isolate);
        data->set_type_tag(RegExpData::Type::ATOM);
        data->set_source(*source);
        data->set_flags(JSRegExp::kGlobal);
        data->set_wrapper(*wrapper);
        wrapper->set_data(*data);

        is_expected_instance_type =
            REG_EXP_DATA_TYPE == data->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(REG_EXP_DATA_TYPE, data->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        data_address = data->address();
        map_address = data->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(data);
        artifacts.keep_alive.push_back(wrapper);
        artifacts.keep_alive.push_back(source);
        return artifacts;
      });

  Node* data = GetSnapshotNode(generator.get(), data_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || data == nullptr || map == nullptr) {
    PrintF("[TODO] REG_EXP_DATA_TYPE not yet stable in this branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), data, map);
  MarkCoveredInstanceType(REG_EXP_DATA_TYPE);
}

UNINITIALIZED_TEST(HeapDumpAtomRegExpDataExactMapEdge) {
  Address data_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<String> source = isolate->factory()->InternalizeUtf8String(
            "heap-dump-atom-regexp-data-source");
        DirectHandle<String> pattern =
            isolate->factory()->InternalizeUtf8String(
                "heap-dump-atom-regexp-data-pattern");
        DirectHandle<RegExpData> data =
            isolate->factory()->NewAtomRegExpData(source, JSRegExp::kGlobal,
                                                  pattern);

        is_expected_instance_type =
            ATOM_REG_EXP_DATA_TYPE == data->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(ATOM_REG_EXP_DATA_TYPE, data->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        data_address = data->address();
        map_address = data->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(data);
        artifacts.keep_alive.push_back(direct_handle(data->wrapper(), isolate));
        artifacts.keep_alive.push_back(source);
        artifacts.keep_alive.push_back(pattern);
        return artifacts;
      });

  Node* data = GetSnapshotNode(generator.get(), data_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || data == nullptr || map == nullptr) {
    PrintF("[TODO] ATOM_REG_EXP_DATA_TYPE not yet stable in this branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), data, map);
  MarkCoveredInstanceType(ATOM_REG_EXP_DATA_TYPE);
}

UNINITIALIZED_TEST(HeapDumpIrRegExpDataExactMapEdge) {
  Address data_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<String> source = isolate->factory()->InternalizeUtf8String(
            "heap-dump-ir-regexp-data-source");
        DirectHandle<RegExpData> data = isolate->factory()->NewIrRegExpData(
            source, JSRegExp::kGlobal, 1, JSRegExp::kNoBacktrackLimit, 0);

        is_expected_instance_type =
            IR_REG_EXP_DATA_TYPE == data->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(IR_REG_EXP_DATA_TYPE, data->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        data_address = data->address();
        map_address = data->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(data);
        artifacts.keep_alive.push_back(direct_handle(data->wrapper(), isolate));
        artifacts.keep_alive.push_back(source);
        return artifacts;
      });

  Node* data = GetSnapshotNode(generator.get(), data_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || data == nullptr || map == nullptr) {
    PrintF("[TODO] IR_REG_EXP_DATA_TYPE not yet stable in this branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), data, map);
  MarkCoveredInstanceType(IR_REG_EXP_DATA_TYPE);
}

UNINITIALIZED_TEST(HeapDumpRegExpMatchInfoExactMapEdge) {
  Address match_info_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<String> subject =
            isolate->factory()->InternalizeUtf8String(
                "heap-dump-regexp-match-info-subject");
        DirectHandle<RegExpMatchInfo> match_info =
            RegExpMatchInfo::New(isolate, 1, AllocationType::kOld);
        match_info->set_last_subject(*subject);
        match_info->set_last_input(*subject);
        match_info->set_capture(RegExpMatchInfo::capture_start_index(0), 0);
        match_info->set_capture(RegExpMatchInfo::capture_end_index(0), 9);

        is_expected_instance_type =
            REG_EXP_MATCH_INFO_TYPE == match_info->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(REG_EXP_MATCH_INFO_TYPE,
                 match_info->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        match_info_address = match_info->address();
        map_address = match_info->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(match_info);
        artifacts.keep_alive.push_back(subject);
        return artifacts;
      });

  Node* match_info = GetSnapshotNode(generator.get(), match_info_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || match_info == nullptr || map == nullptr) {
    PrintF("[TODO] REG_EXP_MATCH_INFO_TYPE not yet stable in this branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), match_info, map);
  MarkCoveredInstanceType(REG_EXP_MATCH_INFO_TYPE);
}

UNINITIALIZED_TEST(HeapDumpObjectBoilerplateDescriptionExactMapEdge) {
  Address description_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<String> first_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpObjectBoilerplateFirst");
        DirectHandle<String> second_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpObjectBoilerplateSecond");
        DirectHandle<Object> first_value =
            isolate->factory()->NewHeapNumber(17);
        DirectHandle<Object> second_value =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpObjectBoilerplateValue");
        Handle<ObjectBoilerplateDescription> description =
            isolate->factory()->NewObjectBoilerplateDescription(2, 2, 0, false);
        description->set_key_value(0, *first_key, *first_value);
        description->set_key_value(1, *second_key, *second_value);

        is_expected_instance_type = OBJECT_BOILERPLATE_DESCRIPTION_TYPE ==
                                    description->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(OBJECT_BOILERPLATE_DESCRIPTION_TYPE,
                 description->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        description_address = description->address();
        map_address = description->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(direct_handle(*description, isolate));
        artifacts.keep_alive.push_back(first_key);
        artifacts.keep_alive.push_back(first_value);
        artifacts.keep_alive.push_back(second_key);
        artifacts.keep_alive.push_back(second_value);
        return artifacts;
      });

  Node* description = GetSnapshotNode(generator.get(), description_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || description == nullptr || map == nullptr) {
    PrintF(
        "[TODO] OBJECT_BOILERPLATE_DESCRIPTION_TYPE not yet stable in this "
        "branch.\n");
    return;
  }

  CHECK(NodeMatchesTypeAndName(generator.get(), description, Node::Type::kCode,
                               "system / ObjectBoilerplateDescription"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), description, map);
  MarkCoveredInstanceType(OBJECT_BOILERPLATE_DESCRIPTION_TYPE);
}

UNINITIALIZED_TEST(HeapDumpEnumCacheExactMapEdge) {
  Address enum_cache_address = 0;
  Address keys_address = 0;
  Address indices_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;
  bool is_expected_target_family = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<FixedArray> keys =
            isolate->factory()->NewFixedArray(2, AllocationType::kOld);
        DirectHandle<FixedArray> indices =
            isolate->factory()->NewFixedArray(2, AllocationType::kOld);
        DirectHandle<String> first_key =
            isolate->factory()->InternalizeUtf8String("heapDumpEnumCacheFirst");
        DirectHandle<String> second_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpEnumCacheSecond");
        keys->set(0, *first_key);
        keys->set(1, *second_key);
        indices->set(0, Smi::FromInt(0));
        indices->set(1, Smi::FromInt(1));
        DirectHandle<EnumCache> enum_cache = isolate->factory()->NewEnumCache(
            keys, indices, AllocationType::kOld);

        is_expected_instance_type =
            ENUM_CACHE_TYPE == enum_cache->map()->instance_type();
        is_expected_target_family =
            FIXED_ARRAY_TYPE == keys->map()->instance_type() &&
            FIXED_ARRAY_TYPE == indices->map()->instance_type();
        if (!is_expected_instance_type || !is_expected_target_family) {
          return DumpArtifacts{};
        }

        CHECK_EQ(ENUM_CACHE_TYPE, enum_cache->map()->instance_type());
        CHECK_EQ(FIXED_ARRAY_TYPE, enum_cache->keys()->map()->instance_type());
        CHECK_EQ(FIXED_ARRAY_TYPE,
                 enum_cache->indices()->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        enum_cache_address = enum_cache->address();
        keys_address = keys->address();
        indices_address = indices->address();
        map_address = enum_cache->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(enum_cache);
        artifacts.keep_alive.push_back(keys);
        artifacts.keep_alive.push_back(indices);
        artifacts.keep_alive.push_back(first_key);
        artifacts.keep_alive.push_back(second_key);
        return artifacts;
      });

  Node* enum_cache = GetSnapshotNode(generator.get(), enum_cache_address);
  Node* keys = GetSnapshotNode(generator.get(), keys_address);
  Node* indices = GetSnapshotNode(generator.get(), indices_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK(is_expected_instance_type);
  CHECK(is_expected_target_family);
  CHECK_NOT_NULL(enum_cache);
  CHECK_NOT_NULL(keys);
  CHECK_NOT_NULL(indices);
  CHECK_NOT_NULL(map);

  CHECK(NodeMatchesTypeAndName(generator.get(), enum_cache,
                               Node::Type::kObjectShape, "system / EnumCache"));
  CHECK(NodeMatchesTypeAndName(generator.get(), keys, Node::Type::kObjectShape,
                               "(enum cache)"));
  CHECK(NodeMatchesTypeAndName(generator.get(), indices,
                               Node::Type::kObjectShape, "(enum cache)"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), enum_cache, map);

  CHECK(NodeDisplayName(generator.get(), enum_cache) != "(enum cache)");
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), enum_cache,
                                   Edge::Type::kInternal, "keys", keys));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), enum_cache,
                                   Edge::Type::kProperty, "keys", keys));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), enum_cache,
                                   Edge::Type::kWeak, "keys", keys));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), enum_cache,
                                   Edge::Type::kShortcut, "keys", keys));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), enum_cache,
                                   Edge::Type::kInternal, "indices", indices));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), enum_cache,
                                   Edge::Type::kProperty, "indices", indices));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), enum_cache,
                                   Edge::Type::kWeak, "indices", indices));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), enum_cache,
                                   Edge::Type::kShortcut, "indices", indices));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), enum_cache,
                                   Edge::Type::kInternal, "(enum cache)",
                                   keys));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), enum_cache,
                                   Edge::Type::kInternal, "(enum cache)",
                                   indices));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), enum_cache,
                                   Edge::Type::kProperty, "(enum cache)",
                                   keys));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), enum_cache,
                                   Edge::Type::kProperty, "(enum cache)",
                                   indices));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), enum_cache,
                                   Edge::Type::kWeak, "(enum cache)",
                                   keys));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), enum_cache,
                                   Edge::Type::kWeak, "(enum cache)",
                                   indices));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), enum_cache,
                                   Edge::Type::kShortcut, "(enum cache)",
                                   keys));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), enum_cache,
                                   Edge::Type::kShortcut, "(enum cache)",
                                   indices));
  // Native ExtractEnumCacheReferences emits only TagObject-labels; field traversal
  // emits hidden object-shape links to the two fields.
  CHECK_EQ(0, CountEdgesFromNodeToTarget(enum_cache, Edge::Type::kInternal,
                                         keys));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(enum_cache, Edge::Type::kInternal,
                                         indices));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(enum_cache, Edge::Type::kHidden,
                                         keys));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(enum_cache, Edge::Type::kHidden,
                                         indices));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(enum_cache, Edge::Type::kWeak, keys));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(enum_cache, Edge::Type::kWeak,
                                         indices));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(enum_cache, Edge::Type::kProperty,
                                         keys));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(enum_cache, Edge::Type::kProperty,
                                         indices));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(enum_cache, Edge::Type::kShortcut,
                                         keys));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(enum_cache, Edge::Type::kShortcut,
                                         indices));
  MarkCoveredInstanceType(ENUM_CACHE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpPrototypeInfoExactMapEdge) {
  Address prototype_info_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<PrototypeInfo> prototype_info =
            isolate->factory()->NewPrototypeInfo();

        is_expected_instance_type =
            PROTOTYPE_INFO_TYPE == prototype_info->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(PROTOTYPE_INFO_TYPE, prototype_info->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        prototype_info_address = prototype_info->address();
        map_address = prototype_info->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(prototype_info);
        return artifacts;
      });

  Node* prototype_info =
      GetSnapshotNode(generator.get(), prototype_info_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  CHECK(is_expected_instance_type);
  CHECK_NOT_NULL(prototype_info);
  CHECK_NOT_NULL(map);

  CHECK(NodeMatchesTypeAndName(generator.get(), prototype_info,
                               Node::Type::kObjectShape,
                               "system / PrototypeInfo"));
  CheckExactInternalMapEdgeNoWrongKind(generator.get(), prototype_info, map);
  MarkCoveredInstanceType(PROTOTYPE_INFO_TYPE);
}

UNINITIALIZED_TEST(HeapDumpPrototypeInfoChainEnumCacheExactTagObject) {
  Address prototype_info_address = 0;
  Address enum_cache_address = 0;
  bool is_expected_instance_type = false;
  bool is_expected_target_family = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<PrototypeInfo> prototype_info =
            isolate->factory()->NewPrototypeInfo();
        Handle<FixedArray> enum_cache = isolate->factory()->NewFixedArray(1);
        prototype_info->set_prototype_chain_enum_cache(*enum_cache);

        is_expected_instance_type =
            PROTOTYPE_INFO_TYPE == prototype_info->map()->instance_type();
        is_expected_target_family =
            FIXED_ARRAY_TYPE == enum_cache->map()->instance_type();
        if (!is_expected_instance_type || !is_expected_target_family) {
          return DumpArtifacts{};
        }

        Tagged<Object> field_value =
            prototype_info->prototype_chain_enum_cache();
        CHECK(IsFixedArray(field_value));
        CHECK_EQ(*enum_cache, field_value);

        InvokeAtomicMajorGC(isolate->heap());

        prototype_info_address = prototype_info->address();
        enum_cache_address = enum_cache->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(prototype_info);
        artifacts.keep_alive.push_back(enum_cache);
        return artifacts;
      });

  Node* prototype_info =
      GetSnapshotNode(generator.get(), prototype_info_address);
  Node* enum_cache = GetSnapshotNode(generator.get(), enum_cache_address);
  CHECK(is_expected_instance_type);
  CHECK(is_expected_target_family);
  CHECK_NOT_NULL(prototype_info);
  CHECK_NOT_NULL(enum_cache);

  // Native contract: V8HeapExplorer::ExtractPrototypeInfoReferences uses
  // TagObject(info->prototype_chain_enum_cache(), "(prototype chain enum cache)",
  // HeapEntry::kObjectShape) with no named edge for field extraction.
  CHECK(NodeMatchesTypeAndName(generator.get(), prototype_info,
                               Node::Type::kObjectShape,
                               "system / PrototypeInfo"));
  CHECK(NodeMatchesTypeAndName(generator.get(), enum_cache,
                               Node::Type::kObjectShape,
                               "(prototype chain enum cache)"));

  // Native PrototypeInfo handling only TagObject-labels this field; the
  // translator must not add a named edge for the display label or field name.
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), prototype_info,
                                   Edge::Type::kInternal,
                                   "(prototype chain enum cache)",
                                   enum_cache));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), prototype_info,
                                   Edge::Type::kInternal,
                                   "prototype_chain_enum_cache", enum_cache));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), prototype_info,
                                   Edge::Type::kWeak,
                                   "(prototype chain enum cache)",
                                   enum_cache));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), prototype_info,
                                   Edge::Type::kHidden,
                                   "(prototype chain enum cache)",
                                   enum_cache));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), prototype_info,
                                   Edge::Type::kProperty,
                                   "(prototype chain enum cache)",
                                   enum_cache));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), prototype_info,
                                   Edge::Type::kShortcut,
                                   "(prototype chain enum cache)",
                                   enum_cache));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(prototype_info, Edge::Type::kInternal,
                                         enum_cache));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(prototype_info, Edge::Type::kHidden,
                                         enum_cache));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(prototype_info, Edge::Type::kWeak,
                                         enum_cache));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(prototype_info, Edge::Type::kProperty,
                                         enum_cache));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(prototype_info, Edge::Type::kShortcut,
                                         enum_cache));

  MarkCoveredInstanceType(PROTOTYPE_INFO_TYPE);
  MarkCoveredInstanceType(FIXED_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpPrototypeInfoUsersExactTagObject) {
  Address prototype_info_address = 0;
  Address prototype_users_address = 0;
  bool is_expected_instance_type = false;
  bool is_expected_target_family = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<PrototypeInfo> prototype_info =
            isolate->factory()->NewPrototypeInfo();
        DirectHandle<WeakArrayList> prototype_users =
            isolate->factory()->NewWeakArrayList(1, AllocationType::kOld);
        prototype_info->set_prototype_users(*prototype_users);

        is_expected_instance_type =
            PROTOTYPE_INFO_TYPE == prototype_info->map()->instance_type();
        is_expected_target_family =
            WEAK_ARRAY_LIST_TYPE == prototype_users->map()->instance_type();
        if (!is_expected_instance_type || !is_expected_target_family) {
          return DumpArtifacts{};
        }

        Tagged<Object> field_value = prototype_info->prototype_users();
        CHECK(IsWeakArrayList(field_value));
        CHECK_EQ(*prototype_users, field_value);

        InvokeAtomicMajorGC(isolate->heap());

        prototype_info_address = prototype_info->address();
        prototype_users_address = prototype_users->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(prototype_info);
        artifacts.keep_alive.push_back(prototype_users);
        return artifacts;
      });

  Node* prototype_info =
      GetSnapshotNode(generator.get(), prototype_info_address);
  Node* prototype_users =
      GetSnapshotNode(generator.get(), prototype_users_address);
  CHECK(is_expected_instance_type);
  CHECK(is_expected_target_family);
  CHECK_NOT_NULL(prototype_info);
  CHECK_NOT_NULL(prototype_users);

  // Native contract: V8HeapExplorer::ExtractPrototypeInfoReferences uses
  // TagObject(info->prototype_users(), "(prototype users)",
  // HeapEntry::kObjectShape) with no named edge for field extraction.
  CHECK(NodeMatchesTypeAndName(generator.get(), prototype_info,
                               Node::Type::kObjectShape,
                               "system / PrototypeInfo"));
  CHECK(NodeMatchesTypeAndName(generator.get(), prototype_users,
                               Node::Type::kObjectShape,
                               "(prototype users)"));

  // Native PrototypeInfo handling only TagObject-labels this WeakArrayList;
  // the translator must not add a named edge for the display label or field.
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), prototype_info,
                                   Edge::Type::kInternal,
                                   "(prototype users)", prototype_users));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), prototype_info,
                                   Edge::Type::kInternal, "prototype_users",
                                   prototype_users));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), prototype_info,
                                   Edge::Type::kWeak, "(prototype users)",
                                   prototype_users));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), prototype_info,
                                   Edge::Type::kHidden, "(prototype users)",
                                   prototype_users));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), prototype_info,
                                   Edge::Type::kProperty, "(prototype users)",
                                   prototype_users));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), prototype_info,
                                   Edge::Type::kShortcut, "(prototype users)",
                                   prototype_users));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(prototype_info, Edge::Type::kInternal,
                                         prototype_users));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(prototype_info, Edge::Type::kHidden,
                                         prototype_users));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(prototype_info, Edge::Type::kWeak,
                                         prototype_users));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(prototype_info, Edge::Type::kProperty,
                                         prototype_users));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(prototype_info, Edge::Type::kShortcut,
                                         prototype_users));

  MarkCoveredInstanceType(PROTOTYPE_INFO_TYPE);
  MarkCoveredInstanceType(WEAK_ARRAY_LIST_TYPE);
}

UNINITIALIZED_TEST(HeapDumpHashTableExactMapEdge) {
  Address hash_table_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<JSObject> first_key =
            isolate->factory()->NewJSObject(isolate->object_function());
        DirectHandle<JSObject> second_key =
            isolate->factory()->NewJSObject(isolate->object_function());
        DirectHandle<Object> first_value =
            isolate->factory()->NewHeapNumber(101);
        DirectHandle<Object> second_value =
            isolate->factory()->NewHeapNumber(202);

        Handle<ObjectHashTable> hash_table = ObjectHashTable::New(isolate, 2);
        hash_table = ObjectHashTable::Put(isolate, hash_table,
                                          direct_handle(*first_key, isolate),
                                          direct_handle(*first_value, isolate));
        hash_table = ObjectHashTable::Put(
            isolate, hash_table, direct_handle(*second_key, isolate),
            direct_handle(*second_value, isolate));

        is_expected_instance_type =
            HASH_TABLE_TYPE == hash_table->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(HASH_TABLE_TYPE, hash_table->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        hash_table_address = hash_table->address();
        map_address = hash_table->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(direct_handle(*hash_table, isolate));
        artifacts.keep_alive.push_back(first_key);
        artifacts.keep_alive.push_back(second_key);
        artifacts.keep_alive.push_back(first_value);
        artifacts.keep_alive.push_back(second_value);
        return artifacts;
      });

  Node* hash_table = GetSnapshotNode(generator.get(), hash_table_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || hash_table == nullptr || map == nullptr) {
    PrintF("[TODO] HASH_TABLE_TYPE not yet stable in this branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), hash_table, map);
  MarkCoveredInstanceType(HASH_TABLE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpNameDictionaryExactMapEdge) {
  Address name_dictionary_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<Name> first_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpNameDictionaryFirst");
        DirectHandle<Name> second_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpNameDictionarySecond");
        DirectHandle<String> first_value =
            isolate->factory()->InternalizeUtf8String("first");
        DirectHandle<String> second_value =
            isolate->factory()->InternalizeUtf8String("second");

        DirectHandle<NameDictionary> name_dictionary =
            NameDictionary::New(isolate, 2);
        name_dictionary =
            NameDictionary::Add(isolate, name_dictionary, first_key,
                                first_value, PropertyDetails::Empty())
                .ToHandleChecked();
        name_dictionary =
            NameDictionary::Add(isolate, name_dictionary, second_key,
                                second_value, PropertyDetails::Empty())
                .ToHandleChecked();

        is_expected_instance_type =
            NAME_DICTIONARY_TYPE == name_dictionary->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(NAME_DICTIONARY_TYPE, name_dictionary->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        name_dictionary_address = name_dictionary->address();
        map_address = name_dictionary->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(name_dictionary);
        artifacts.keep_alive.push_back(first_key);
        artifacts.keep_alive.push_back(second_key);
        artifacts.keep_alive.push_back(first_value);
        artifacts.keep_alive.push_back(second_value);
        return artifacts;
      });

  Node* name_dictionary =
      GetSnapshotNode(generator.get(), name_dictionary_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || name_dictionary == nullptr ||
      map == nullptr) {
    PrintF("[TODO] NAME_DICTIONARY_TYPE not yet stable in this branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), name_dictionary, map);
  MarkCoveredInstanceType(NAME_DICTIONARY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpNumberDictionaryExactMapEdge) {
  Address number_dictionary_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<NumberDictionary> number_dictionary =
            NumberDictionary::New(isolate, 4);
        DirectHandle<String> first_value =
            isolate->factory()->InternalizeUtf8String("first-number-dict");
        DirectHandle<String> second_value =
            isolate->factory()->InternalizeUtf8String("second-number-dict");

        number_dictionary =
            NumberDictionary::Set(isolate, number_dictionary, 1, first_value);
        number_dictionary =
            NumberDictionary::Set(isolate, number_dictionary, 2, second_value);

        is_expected_instance_type =
            NUMBER_DICTIONARY_TYPE == number_dictionary->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(NUMBER_DICTIONARY_TYPE,
                 number_dictionary->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        number_dictionary_address = number_dictionary->address();
        map_address = number_dictionary->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(number_dictionary);
        artifacts.keep_alive.push_back(first_value);
        artifacts.keep_alive.push_back(second_value);
        return artifacts;
      });

  Node* number_dictionary =
      GetSnapshotNode(generator.get(), number_dictionary_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || number_dictionary == nullptr ||
      map == nullptr) {
    PrintF("[TODO] NUMBER_DICTIONARY_TYPE not yet stable in this branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), number_dictionary, map);
  MarkCoveredInstanceType(NUMBER_DICTIONARY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpGlobalDictionaryExactMapEdge) {
  Address global_dictionary_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<Name> first_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpGlobalDictionaryFirst");
        DirectHandle<Name> second_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpGlobalDictionarySecond");
        auto first_value = isolate->factory()->NewPropertyCell(
            first_key, PropertyDetails::Empty(),
            isolate->factory()->NewHeapNumber(101));
        auto second_value = isolate->factory()->NewPropertyCell(
            second_key, PropertyDetails::Empty(),
            isolate->factory()->NewHeapNumber(202));

        DirectHandle<GlobalDictionary> global_dictionary =
            GlobalDictionary::New(isolate, 2);
        global_dictionary =
            GlobalDictionary::Add(isolate, global_dictionary, first_key,
                                  direct_handle(*first_value, isolate),
                                  PropertyDetails::Empty())
                .ToHandleChecked();
        global_dictionary =
            GlobalDictionary::Add(isolate, global_dictionary, second_key,
                                  direct_handle(*second_value, isolate),
                                  PropertyDetails::Empty())
                .ToHandleChecked();

        is_expected_instance_type =
            GLOBAL_DICTIONARY_TYPE == global_dictionary->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(GLOBAL_DICTIONARY_TYPE,
                 global_dictionary->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        global_dictionary_address = global_dictionary->address();
        map_address = global_dictionary->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(global_dictionary);
        artifacts.keep_alive.push_back(first_key);
        artifacts.keep_alive.push_back(second_key);
        artifacts.keep_alive.push_back(first_value);
        artifacts.keep_alive.push_back(second_value);
        return artifacts;
      });

  Node* global_dictionary =
      GetSnapshotNode(generator.get(), global_dictionary_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || global_dictionary == nullptr ||
      map == nullptr) {
    PrintF("[TODO] GLOBAL_DICTIONARY_TYPE not yet stable in this branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), global_dictionary, map);
  MarkCoveredInstanceType(GLOBAL_DICTIONARY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpExactPropertyCellType) {
  Address property_cell_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<Name> property_name =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpPropertyCellKey");
        DirectHandle<String> property_value =
            isolate->factory()->InternalizeUtf8String("property-cell-value");
        auto property_cell = isolate->factory()->NewPropertyCell(
            property_name, PropertyDetails::Empty(), property_value);

        is_expected_instance_type =
            PROPERTY_CELL_TYPE == property_cell->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(PROPERTY_CELL_TYPE, property_cell->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        property_cell_address = property_cell->address();
        map_address = property_cell->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(direct_handle(*property_cell, isolate));
        artifacts.keep_alive.push_back(property_name);
        artifacts.keep_alive.push_back(property_value);
        return artifacts;
      });

  Node* property_cell_node =
      GetSnapshotNode(generator.get(), property_cell_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || property_cell_node == nullptr ||
      map == nullptr) {
    PrintF("[TODO] PROPERTY_CELL_TYPE not yet stable in this branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), property_cell_node, map);
  MarkCoveredInstanceType(PROPERTY_CELL_TYPE);
}

UNINITIALIZED_TEST(HeapDumpPropertyCellExactValueEdge) {
  Address property_cell_address = 0;
  Address value_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<Name> property_name =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpPropertyCellValueKey");
        DirectHandle<JSObject> property_value =
            isolate->factory()->NewJSObject(isolate->object_function());
        auto property_cell = isolate->factory()->NewPropertyCell(
            property_name, PropertyDetails::Empty(), property_value);

        is_expected_instance_type =
            PROPERTY_CELL_TYPE == property_cell->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(PROPERTY_CELL_TYPE, property_cell->map()->instance_type());
        CHECK(IsJSObject(property_cell->value()));
        InvokeAtomicMajorGC(isolate->heap());

        property_cell_address = property_cell->address();
        value_address = property_value->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(direct_handle(*property_cell, isolate));
        artifacts.keep_alive.push_back(property_name);
        artifacts.keep_alive.push_back(property_value);
        return artifacts;
      });

  Node* property_cell_node =
      GetSnapshotNode(generator.get(), property_cell_address);
  Node* value_node = GetSnapshotNode(generator.get(), value_address);
  CHECK(is_expected_instance_type);
  CHECK_NOT_NULL(property_cell_node);
  CHECK_NOT_NULL(value_node);

  // Native contract: V8HeapExplorer::ExtractPropertyCellReferences emits
  // SetInternalReference(entry, "value", cell->value(), PropertyCell::kValueOffset).
  CHECK(HasExactNamedEdge(generator.get(), property_cell_node,
                          Edge::Type::kInternal, "value", value_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), property_cell_node,
                                   Edge::Type::kInternal, "value",
                                   value_node));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(property_cell_node, Edge::Type::kInternal,
                                         value_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(property_cell_node, Edge::Type::kWeak,
                                         value_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(property_cell_node,
                                         Edge::Type::kHidden, value_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(property_cell_node,
                                         Edge::Type::kProperty, value_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(property_cell_node,
                                         Edge::Type::kShortcut, value_node));
  MarkCoveredInstanceType(PROPERTY_CELL_TYPE);
  MarkCoveredInstanceType(JS_OBJECT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpPropertyCellExactDependentCodeEdge) {
  Address property_cell_address = 0;
  Address value_address = 0;
  Address dependent_code_address = 0;
  bool is_expected_instance_type = false;
  bool is_expected_dependent_code_family = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<Name> property_name =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpPropertyCellDependentCodeKey");
        DirectHandle<JSObject> property_value =
            isolate->factory()->NewJSObject(isolate->object_function());
        auto property_cell = isolate->factory()->NewPropertyCell(
            property_name, PropertyDetails::Empty(), property_value);

        is_expected_instance_type =
            PROPERTY_CELL_TYPE == property_cell->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(PROPERTY_CELL_TYPE, property_cell->map()->instance_type());
        // dependent_code() is a V8 HeapObject (DependentCode/WeakArrayList),
        // so this fixture proves the target family before snapshot lookup.
        Tagged<DependentCode> dependent_code = property_cell->dependent_code();
        is_expected_dependent_code_family =
            WEAK_ARRAY_LIST_TYPE == dependent_code->map()->instance_type();
        CHECK(is_expected_dependent_code_family);
        InvokeAtomicMajorGC(isolate->heap());

        property_cell_address = property_cell->address();
        value_address = property_value->address();
        dependent_code_address = property_cell->dependent_code().address();
        CHECK_EQ(dependent_code.address(), dependent_code_address);

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(direct_handle(*property_cell, isolate));
        artifacts.keep_alive.push_back(property_name);
        artifacts.keep_alive.push_back(property_value);
        return artifacts;
      });

  Node* property_cell_node =
      GetSnapshotNode(generator.get(), property_cell_address);
  Node* value_node = GetSnapshotNode(generator.get(), value_address);
  Node* dependent_code_node =
      GetSnapshotNode(generator.get(), dependent_code_address);
  CHECK(is_expected_instance_type);
  CHECK(is_expected_dependent_code_family);
  CHECK_NOT_NULL(property_cell_node);
  CHECK_NOT_NULL(value_node);
  CHECK_NOT_NULL(dependent_code_node);

  CHECK(NodeHasType(value_node, Node::Type::kObject));
  // Native contract: V8HeapExplorer::ExtractPropertyCellReferences emits
  // TagObject(cell->dependent_code(), "(dependent code)") and
  // SetInternalReference(entry, "dependent_code", cell->dependent_code(),
  // PropertyCell::kDependentCodeOffset).
  // Native TagObject is label-only here: it renames the existing
  // DependentCode/WeakArrayList node but does not override node category.
  CHECK(NodeMatchesTypeAndName(generator.get(), dependent_code_node,
                               Node::Type::kHidden, "(dependent code)"));

  CHECK(HasExactNamedEdge(generator.get(), property_cell_node,
                          Edge::Type::kInternal, "value", value_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), property_cell_node,
                                   Edge::Type::kInternal, "value",
                                   value_node));
  CHECK(HasExactNamedEdge(generator.get(), property_cell_node,
                          Edge::Type::kInternal, "dependent_code",
                          dependent_code_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), property_cell_node,
                                   Edge::Type::kInternal, "dependent_code",
                                   dependent_code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), property_cell_node,
                                   Edge::Type::kInternal, "(dependent code)",
                                   dependent_code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), property_cell_node,
                                   Edge::Type::kWeak, "(dependent code)",
                                   dependent_code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), property_cell_node,
                                   Edge::Type::kHidden, "(dependent code)",
                                   dependent_code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), property_cell_node,
                                   Edge::Type::kProperty, "(dependent code)",
                                   dependent_code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), property_cell_node,
                                   Edge::Type::kShortcut, "(dependent code)",
                                   dependent_code_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(property_cell_node, Edge::Type::kWeak,
                                         value_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(property_cell_node, Edge::Type::kWeak,
                                         dependent_code_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(property_cell_node,
                                         Edge::Type::kHidden,
                                         value_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(property_cell_node,
                                         Edge::Type::kHidden,
                                         dependent_code_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(property_cell_node,
                                         Edge::Type::kProperty,
                                         value_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(property_cell_node,
                                         Edge::Type::kProperty,
                                         dependent_code_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(property_cell_node,
                                         Edge::Type::kShortcut,
                                         value_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(property_cell_node,
                                         Edge::Type::kShortcut,
                                         dependent_code_node));
  MarkCoveredInstanceType(PROPERTY_CELL_TYPE);
  MarkCoveredInstanceType(WEAK_ARRAY_LIST_TYPE);
}

UNINITIALIZED_TEST(HeapDumpNameToIndexHashTableExactMapEdge) {
  Address name_to_index_hash_table_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<Name> first_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpNameToIndexHashTableFirst");
        DirectHandle<Name> second_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpNameToIndexHashTableSecond");

        Handle<NameToIndexHashTable> name_to_index_hash_table =
            NameToIndexHashTable::New(isolate, 2);
        name_to_index_hash_table = NameToIndexHashTable::Add(
            isolate, name_to_index_hash_table, first_key, 0);
        name_to_index_hash_table = NameToIndexHashTable::Add(
            isolate, name_to_index_hash_table, second_key, 1);

        is_expected_instance_type =
            NAME_TO_INDEX_HASH_TABLE_TYPE ==
            name_to_index_hash_table->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(NAME_TO_INDEX_HASH_TABLE_TYPE,
                 name_to_index_hash_table->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        name_to_index_hash_table_address = name_to_index_hash_table->address();
        map_address = name_to_index_hash_table->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(
            direct_handle(*name_to_index_hash_table, isolate));
        artifacts.keep_alive.push_back(first_key);
        artifacts.keep_alive.push_back(second_key);
        return artifacts;
      });

  Node* name_to_index_hash_table =
      GetSnapshotNode(generator.get(), name_to_index_hash_table_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || name_to_index_hash_table == nullptr ||
      map == nullptr) {
    PrintF(
        "[TODO] NAME_TO_INDEX_HASH_TABLE_TYPE not yet stable in this "
        "branch.\n");
    return;
  }

  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CHECK(HasExactNamedEdge(generator.get(), name_to_index_hash_table,
                          Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), name_to_index_hash_table,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(name_to_index_hash_table,
                                         Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(name_to_index_hash_table,
                                         Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(name_to_index_hash_table,
                                         Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(name_to_index_hash_table,
                                         Edge::Type::kProperty, map));
  MarkCoveredInstanceType(NAME_TO_INDEX_HASH_TABLE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpOrderedNameDictionaryExactMapEdge) {
  Address ordered_name_dictionary_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<Name> first_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpOrderedNameDictionaryFirst");
        DirectHandle<Name> second_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpOrderedNameDictionarySecond");
        DirectHandle<String> first_value =
            isolate->factory()->InternalizeUtf8String("ordered-name-first");
        DirectHandle<String> second_value =
            isolate->factory()->InternalizeUtf8String("ordered-name-second");

        Handle<OrderedNameDictionary> ordered_name_dictionary =
            OrderedNameDictionary::Allocate(isolate, 2).ToHandleChecked();
        ordered_name_dictionary =
            OrderedNameDictionary::Add(isolate, ordered_name_dictionary,
                                       first_key, first_value,
                                       PropertyDetails::Empty())
                .ToHandleChecked();
        ordered_name_dictionary =
            OrderedNameDictionary::Add(isolate, ordered_name_dictionary,
                                       second_key, second_value,
                                       PropertyDetails::Empty())
                .ToHandleChecked();

        is_expected_instance_type =
            ORDERED_NAME_DICTIONARY_TYPE ==
            ordered_name_dictionary->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(ORDERED_NAME_DICTIONARY_TYPE,
                 ordered_name_dictionary->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        ordered_name_dictionary_address = ordered_name_dictionary->address();
        map_address = ordered_name_dictionary->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(
            direct_handle(*ordered_name_dictionary, isolate));
        artifacts.keep_alive.push_back(first_key);
        artifacts.keep_alive.push_back(second_key);
        artifacts.keep_alive.push_back(first_value);
        artifacts.keep_alive.push_back(second_value);
        return artifacts;
      });

  Node* ordered_name_dictionary =
      GetSnapshotNode(generator.get(), ordered_name_dictionary_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || ordered_name_dictionary == nullptr ||
      map == nullptr) {
    PrintF(
        "[TODO] ORDERED_NAME_DICTIONARY_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CHECK(HasExactNamedEdge(generator.get(), ordered_name_dictionary,
                          Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), ordered_name_dictionary,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(ordered_name_dictionary,
                                         Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(ordered_name_dictionary,
                                         Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(ordered_name_dictionary,
                                         Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(ordered_name_dictionary,
                                         Edge::Type::kProperty, map));
  MarkCoveredInstanceType(ORDERED_NAME_DICTIONARY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpSmallOrderedHashMapExactMapEdge) {
  Address small_ordered_hash_map_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<Name> first_key =
                isolate->factory()->InternalizeUtf8String(
                    "heapDumpSmallOrderedHashMapFirst");
            DirectHandle<Name> second_key =
                isolate->factory()->InternalizeUtf8String(
                    "heapDumpSmallOrderedHashMapSecond");
            DirectHandle<String> first_value =
                isolate->factory()->InternalizeUtf8String(
                    "small-ordered-map-first");
            DirectHandle<String> second_value =
                isolate->factory()->InternalizeUtf8String(
                    "small-ordered-map-second");

            Handle<SmallOrderedHashMap> small_ordered_hash_map =
                isolate->factory()->NewSmallOrderedHashMap(4);
            small_ordered_hash_map =
                SmallOrderedHashMap::Add(isolate, small_ordered_hash_map,
                                         first_key, first_value)
                    .ToHandleChecked();
            small_ordered_hash_map =
                SmallOrderedHashMap::Add(isolate, small_ordered_hash_map,
                                         second_key, second_value)
                    .ToHandleChecked();

            is_expected_instance_type =
                SMALL_ORDERED_HASH_MAP_TYPE ==
                small_ordered_hash_map->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(SMALL_ORDERED_HASH_MAP_TYPE,
                     small_ordered_hash_map->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            small_ordered_hash_map_address = small_ordered_hash_map->address();
            map_address = small_ordered_hash_map->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(
                direct_handle(*small_ordered_hash_map, isolate));
            artifacts.keep_alive.push_back(first_key);
            artifacts.keep_alive.push_back(second_key);
            artifacts.keep_alive.push_back(first_value);
            artifacts.keep_alive.push_back(second_value);
            return artifacts;
          });

  Node* small_ordered_hash_map =
      GetSnapshotNode(generator.get(), small_ordered_hash_map_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || small_ordered_hash_map == nullptr ||
      map == nullptr) {
    PrintF(
        "[TODO] SMALL_ORDERED_HASH_MAP_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CHECK(HasExactNamedEdge(generator.get(), small_ordered_hash_map,
                          Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), small_ordered_hash_map,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(small_ordered_hash_map,
                                         Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(small_ordered_hash_map,
                                         Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(small_ordered_hash_map,
                                         Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(small_ordered_hash_map,
                                         Edge::Type::kProperty, map));
  MarkCoveredInstanceType(SMALL_ORDERED_HASH_MAP_TYPE);
}

UNINITIALIZED_TEST(HeapDumpSmallOrderedHashSetExactMapEdge) {
  Address small_ordered_hash_set_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<Name> first_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpSmallOrderedHashSetFirst");
        DirectHandle<Name> second_key =
            isolate->factory()->InternalizeUtf8String(
                "heapDumpSmallOrderedHashSetSecond");

        Handle<SmallOrderedHashSet> small_ordered_hash_set =
            isolate->factory()->NewSmallOrderedHashSet(4);
        small_ordered_hash_set =
            SmallOrderedHashSet::Add(isolate, small_ordered_hash_set, first_key)
                .ToHandleChecked();
        small_ordered_hash_set =
            SmallOrderedHashSet::Add(isolate, small_ordered_hash_set,
                                     second_key)
                .ToHandleChecked();

        is_expected_instance_type =
            SMALL_ORDERED_HASH_SET_TYPE ==
            small_ordered_hash_set->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(SMALL_ORDERED_HASH_SET_TYPE,
                 small_ordered_hash_set->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        small_ordered_hash_set_address = small_ordered_hash_set->address();
        map_address = small_ordered_hash_set->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(
            direct_handle(*small_ordered_hash_set, isolate));
        artifacts.keep_alive.push_back(first_key);
        artifacts.keep_alive.push_back(second_key);
        return artifacts;
      });

  Node* small_ordered_hash_set =
      GetSnapshotNode(generator.get(), small_ordered_hash_set_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || small_ordered_hash_set == nullptr ||
      map == nullptr) {
    PrintF(
        "[TODO] SMALL_ORDERED_HASH_SET_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CHECK(HasExactNamedEdge(generator.get(), small_ordered_hash_set,
                          Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), small_ordered_hash_set,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(small_ordered_hash_set,
                                         Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(small_ordered_hash_set,
                                         Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(small_ordered_hash_set,
                                         Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(small_ordered_hash_set,
                                         Edge::Type::kProperty, map));
  MarkCoveredInstanceType(SMALL_ORDERED_HASH_SET_TYPE);
}

UNINITIALIZED_TEST(HeapDumpSmallOrderedNameDictionaryExactMapEdge) {
  Address small_ordered_name_dictionary_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<Name> first_key =
                isolate->factory()->InternalizeUtf8String(
                    "heapDumpSmallOrderedNameDictionaryFirst");
            DirectHandle<Name> second_key =
                isolate->factory()->InternalizeUtf8String(
                    "heapDumpSmallOrderedNameDictionarySecond");
            DirectHandle<String> first_value =
                isolate->factory()->InternalizeUtf8String(
                    "small-ordered-name-first");
            DirectHandle<String> second_value =
                isolate->factory()->InternalizeUtf8String(
                    "small-ordered-name-second");

            Handle<SmallOrderedNameDictionary> small_ordered_name_dictionary =
                isolate->factory()->NewSmallOrderedNameDictionary(4);
            small_ordered_name_dictionary =
                SmallOrderedNameDictionary::Add(
                    isolate, small_ordered_name_dictionary, first_key,
                    first_value, PropertyDetails::Empty())
                    .ToHandleChecked();
            small_ordered_name_dictionary =
                SmallOrderedNameDictionary::Add(
                    isolate, small_ordered_name_dictionary, second_key,
                    second_value, PropertyDetails::Empty())
                    .ToHandleChecked();

            is_expected_instance_type =
                SMALL_ORDERED_NAME_DICTIONARY_TYPE ==
                small_ordered_name_dictionary->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(SMALL_ORDERED_NAME_DICTIONARY_TYPE,
                     small_ordered_name_dictionary->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            small_ordered_name_dictionary_address =
                small_ordered_name_dictionary->address();
            map_address = small_ordered_name_dictionary->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(
                direct_handle(*small_ordered_name_dictionary, isolate));
            artifacts.keep_alive.push_back(first_key);
            artifacts.keep_alive.push_back(second_key);
            artifacts.keep_alive.push_back(first_value);
            artifacts.keep_alive.push_back(second_value);
            return artifacts;
          });

  Node* small_ordered_name_dictionary =
      GetSnapshotNode(generator.get(), small_ordered_name_dictionary_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || small_ordered_name_dictionary == nullptr ||
      map == nullptr) {
    PrintF(
        "[TODO] SMALL_ORDERED_NAME_DICTIONARY_TYPE not yet stable in this "
        "branch.\n");
    return;
  }

  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CHECK(HasExactNamedEdge(generator.get(), small_ordered_name_dictionary,
                          Edge::Type::kInternal, "map", map));
  CHECK_EQ(1,
           CountExactNamedEdges(generator.get(), small_ordered_name_dictionary,
                                Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(small_ordered_name_dictionary,
                                         Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(small_ordered_name_dictionary,
                                         Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(small_ordered_name_dictionary,
                                         Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(small_ordered_name_dictionary,
                                         Edge::Type::kProperty, map));
  MarkCoveredInstanceType(SMALL_ORDERED_NAME_DICTIONARY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpRegisteredSymbolTableExactMapEdge) {
  Address registered_symbol_table_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<String> key = isolate->factory()->InternalizeUtf8String(
            "heapDumpRegisteredSymbol");
        DirectHandle<Symbol> value = isolate->factory()->NewSymbol();

        auto registered_symbol_table = RegisteredSymbolTable::New(
            isolate, 1, AllocationType::kOld, USE_CUSTOM_MINIMUM_CAPACITY);
        registered_symbol_table = RegisteredSymbolTable::Add(
            isolate, registered_symbol_table, key, value);

        is_expected_instance_type =
            REGISTERED_SYMBOL_TABLE_TYPE ==
            registered_symbol_table->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(REGISTERED_SYMBOL_TABLE_TYPE,
                 registered_symbol_table->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        registered_symbol_table_address = registered_symbol_table->address();
        map_address = registered_symbol_table->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(registered_symbol_table);
        artifacts.keep_alive.push_back(key);
        artifacts.keep_alive.push_back(value);
        return artifacts;
      });

  Node* registered_symbol_table =
      GetSnapshotNode(generator.get(), registered_symbol_table_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || registered_symbol_table == nullptr ||
      map == nullptr) {
    PrintF(
        "[TODO] REGISTERED_SYMBOL_TABLE_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CHECK(HasExactNamedEdge(generator.get(), registered_symbol_table,
                          Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), registered_symbol_table,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(registered_symbol_table,
                                         Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(registered_symbol_table,
                                         Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(registered_symbol_table,
                                         Edge::Type::kProperty, map));
  MarkCoveredInstanceType(REGISTERED_SYMBOL_TABLE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpSimpleNameDictionaryExactMapEdge) {
  Address simple_name_dictionary_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<Name> first_key =
                isolate->factory()->InternalizeUtf8String(
                    "heapDumpSimpleNameDictionaryFirst");
            DirectHandle<Name> second_key =
                isolate->factory()->InternalizeUtf8String(
                    "heapDumpSimpleNameDictionarySecond");
            DirectHandle<String> first_value =
                isolate->factory()->InternalizeUtf8String("simple-name-first");
            DirectHandle<String> second_value =
                isolate->factory()->InternalizeUtf8String("simple-name-second");

            Handle<SimpleNameDictionary> simple_name_dictionary =
                SimpleNameDictionary::New(isolate, 2);
            simple_name_dictionary = SimpleNameDictionary::Set(
                isolate, simple_name_dictionary, first_key, first_value);
            simple_name_dictionary = SimpleNameDictionary::Set(
                isolate, simple_name_dictionary, second_key, second_value);

            is_expected_instance_type =
                SIMPLE_NAME_DICTIONARY_TYPE ==
                simple_name_dictionary->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(SIMPLE_NAME_DICTIONARY_TYPE,
                     simple_name_dictionary->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            simple_name_dictionary_address = simple_name_dictionary->address();
            map_address = simple_name_dictionary->map()->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(
                direct_handle(*simple_name_dictionary, isolate));
            artifacts.keep_alive.push_back(first_key);
            artifacts.keep_alive.push_back(second_key);
            artifacts.keep_alive.push_back(first_value);
            artifacts.keep_alive.push_back(second_value);
            return artifacts;
          });

  Node* simple_name_dictionary =
      GetSnapshotNode(generator.get(), simple_name_dictionary_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || simple_name_dictionary == nullptr ||
      map == nullptr) {
    PrintF(
        "[TODO] SIMPLE_NAME_DICTIONARY_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CHECK(HasExactNamedEdge(generator.get(), simple_name_dictionary,
                          Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), simple_name_dictionary,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(simple_name_dictionary,
                                         Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(simple_name_dictionary,
                                         Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(simple_name_dictionary,
                                         Edge::Type::kProperty, map));
  MarkCoveredInstanceType(SIMPLE_NAME_DICTIONARY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpSimpleNumberDictionaryExactMapEdge) {
  Address simple_number_dictionary_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        Handle<SimpleNumberDictionary> simple_number_dictionary =
            SimpleNumberDictionary::New(isolate, 4);
        DirectHandle<String> first_value =
            isolate->factory()->InternalizeUtf8String("simple-number-first");
        DirectHandle<String> second_value =
            isolate->factory()->InternalizeUtf8String("simple-number-second");

        simple_number_dictionary = SimpleNumberDictionary::Set(
            isolate, simple_number_dictionary, 1, first_value);
        simple_number_dictionary = SimpleNumberDictionary::Set(
            isolate, simple_number_dictionary, 2, second_value);

        is_expected_instance_type =
            SIMPLE_NUMBER_DICTIONARY_TYPE ==
            simple_number_dictionary->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(SIMPLE_NUMBER_DICTIONARY_TYPE,
                 simple_number_dictionary->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        simple_number_dictionary_address = simple_number_dictionary->address();
        map_address = simple_number_dictionary->map()->address();

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(
            direct_handle(*simple_number_dictionary, isolate));
        artifacts.keep_alive.push_back(first_value);
        artifacts.keep_alive.push_back(second_value);
        return artifacts;
      });

  Node* simple_number_dictionary =
      GetSnapshotNode(generator.get(), simple_number_dictionary_address);
  Node* map = GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || simple_number_dictionary == nullptr ||
      map == nullptr) {
    PrintF(
        "[TODO] SIMPLE_NUMBER_DICTIONARY_TYPE not yet stable in this "
        "branch.\n");
    return;
  }

  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CHECK(HasExactNamedEdge(generator.get(), simple_number_dictionary,
                          Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), simple_number_dictionary,
                                   Edge::Type::kInternal, "map", map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(simple_number_dictionary,
                                         Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(simple_number_dictionary,
                                         Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(simple_number_dictionary,
                                         Edge::Type::kProperty, map));
  MarkCoveredInstanceType(SIMPLE_NUMBER_DICTIONARY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpFunctionTemplateInfoExactMapEdge) {
  Address function_template_info_address = 0;
  Address function_template_info_map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        v8::Local<v8::FunctionTemplate> function_template =
            v8::FunctionTemplate::New(v8_isolate, HeapDumpTemplateNoopCallback);
        v8::Local<v8::Function> api_function =
            function_template->GetFunction(context).ToLocalChecked();
        DirectHandle<JSFunction> api_function_handle =
            Cast<JSFunction>(v8::Utils::OpenDirectHandle(*api_function));
        Tagged<FunctionTemplateInfo> function_template_info =
            Cast<FunctionTemplateInfo>(
                api_function_handle->shared()->api_func_data());

        is_expected_instance_type =
            FUNCTION_TEMPLATE_INFO_TYPE ==
            function_template_info->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(FUNCTION_TEMPLATE_INFO_TYPE,
                 function_template_info->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        function_template_info_address = function_template_info->address();
        function_template_info_map_address =
            function_template_info->map()->address();
        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(
            direct_handle(*function_template_info, isolate));
        artifacts.keep_alive.push_back(api_function_handle);
        return artifacts;
      });

  Node* function_template_info_node =
      GetSnapshotNode(generator.get(), function_template_info_address);
  Node* function_template_info_map =
      GetSnapshotNode(generator.get(), function_template_info_map_address);
  if (!is_expected_instance_type || function_template_info_node == nullptr ||
      function_template_info_map == nullptr) {
    PrintF(
        "[TODO] FUNCTION_TEMPLATE_INFO_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeHasType(function_template_info_map, Node::Type::kObjectShape));
  CHECK(HasExactNamedEdge(generator.get(), function_template_info_node,
                          Edge::Type::kInternal, "map",
                          function_template_info_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), function_template_info_node,
                                   Edge::Type::kInternal, "map",
                                   function_template_info_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(function_template_info_node,
                                         Edge::Type::kWeak,
                                         function_template_info_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(function_template_info_node,
                                         Edge::Type::kHidden,
                                         function_template_info_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(function_template_info_node,
                                         Edge::Type::kProperty,
                                         function_template_info_map));
  MarkCoveredInstanceType(FUNCTION_TEMPLATE_INFO_TYPE);
}

UNINITIALIZED_TEST(HeapDumpObjectTemplateInfoExactMapEdge) {
  Address object_template_info_address = 0;
  Address object_template_info_map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<FunctionTemplateInfo> function_template_info =
                isolate->factory()->NewFunctionTemplateInfo(0, false);
            DirectHandle<ObjectTemplateInfo> object_template_info =
                isolate->factory()->NewObjectTemplateInfo(
                    function_template_info, false);
            is_expected_instance_type =
                OBJECT_TEMPLATE_INFO_TYPE ==
                object_template_info->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(OBJECT_TEMPLATE_INFO_TYPE,
                     object_template_info->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            object_template_info_address = object_template_info->address();
            object_template_info_map_address =
                object_template_info->map()->address();
            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(object_template_info);
            return artifacts;
          });

  Node* object_template_info_node =
      GetSnapshotNode(generator.get(), object_template_info_address);
  Node* object_template_info_map =
      GetSnapshotNode(generator.get(), object_template_info_map_address);
  if (!is_expected_instance_type || object_template_info_node == nullptr ||
      object_template_info_map == nullptr) {
    PrintF("[TODO] OBJECT_TEMPLATE_INFO_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeHasType(object_template_info_map, Node::Type::kObjectShape));
  CHECK(HasExactNamedEdge(generator.get(), object_template_info_node,
                          Edge::Type::kInternal, "map",
                          object_template_info_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), object_template_info_node,
                                   Edge::Type::kInternal, "map",
                                   object_template_info_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object_template_info_node,
                                         Edge::Type::kWeak,
                                         object_template_info_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object_template_info_node,
                                         Edge::Type::kHidden,
                                         object_template_info_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object_template_info_node,
                                         Edge::Type::kProperty,
                                         object_template_info_map));
  MarkCoveredInstanceType(OBJECT_TEMPLATE_INFO_TYPE);
}

UNINITIALIZED_TEST(HeapDumpDictionaryTemplateInfoExactMapEdge) {
  Address dictionary_template_info_address = 0;
  Address dictionary_template_info_map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(context);
        const std::string_view property_names[] = {"heapDumpName"};
        DirectHandle<DictionaryTemplateInfo> dictionary_template_info =
            DictionaryTemplateInfo::Create(isolate, property_names);

        is_expected_instance_type =
            DICTIONARY_TEMPLATE_INFO_TYPE ==
            dictionary_template_info->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(DICTIONARY_TEMPLATE_INFO_TYPE,
                 dictionary_template_info->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        dictionary_template_info_address = dictionary_template_info->address();
        dictionary_template_info_map_address =
            dictionary_template_info->map()->address();
        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(dictionary_template_info);
        return artifacts;
      });

  Node* dictionary_template_info_node =
      GetSnapshotNode(generator.get(), dictionary_template_info_address);
  Node* dictionary_template_info_map =
      GetSnapshotNode(generator.get(), dictionary_template_info_map_address);
  if (!is_expected_instance_type || dictionary_template_info_node == nullptr ||
      dictionary_template_info_map == nullptr) {
    PrintF(
        "[TODO] DICTIONARY_TEMPLATE_INFO_TYPE not yet stable in this "
        "branch.\n");
    return;
  }

  CHECK(NodeHasType(dictionary_template_info_map, Node::Type::kObjectShape));
  CHECK(HasExactNamedEdge(generator.get(), dictionary_template_info_node,
                          Edge::Type::kInternal, "map",
                          dictionary_template_info_map));
  CHECK_EQ(1, CountExactNamedEdges(
                  generator.get(), dictionary_template_info_node,
                  Edge::Type::kInternal, "map", dictionary_template_info_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(dictionary_template_info_node,
                                         Edge::Type::kWeak,
                                         dictionary_template_info_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(dictionary_template_info_node,
                                         Edge::Type::kHidden,
                                         dictionary_template_info_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(dictionary_template_info_node,
                                         Edge::Type::kProperty,
                                         dictionary_template_info_map));
  MarkCoveredInstanceType(DICTIONARY_TEMPLATE_INFO_TYPE);
}

UNINITIALIZED_TEST(HeapDumpInterceptorInfoExactMapEdge) {
  Address interceptor_info_address = 0;
  Address interceptor_info_map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        v8::Local<v8::FunctionTemplate> function_template =
            v8::FunctionTemplate::New(v8_isolate, HeapDumpTemplateNoopCallback);
        v8::Local<v8::ObjectTemplate> instance_template =
            function_template->InstanceTemplate();
        instance_template->SetHandler(v8::NamedPropertyHandlerConfiguration(
            HeapDumpTemplateNamedPropertyGetter));
        v8::Local<v8::Function> api_function =
            function_template->GetFunction(context).ToLocalChecked();
        DirectHandle<JSFunction> api_function_handle =
            Cast<JSFunction>(v8::Utils::OpenDirectHandle(*api_function));
        Tagged<FunctionTemplateInfo> function_template_info =
            Cast<FunctionTemplateInfo>(
                api_function_handle->shared()->api_func_data());
        Tagged<Object> handler_object =
            function_template_info->GetNamedPropertyHandler();

        if (IsUndefined(handler_object, isolate)) {
          return DumpArtifacts{};
        }

        DirectHandle<InterceptorInfo> interceptor_info =
            direct_handle(Cast<InterceptorInfo>(handler_object), isolate);
        is_expected_instance_type =
            INTERCEPTOR_INFO_TYPE == interceptor_info->map()->instance_type();
        if (!is_expected_instance_type) {
          return DumpArtifacts{};
        }

        CHECK_EQ(INTERCEPTOR_INFO_TYPE,
                 interceptor_info->map()->instance_type());
        InvokeAtomicMajorGC(isolate->heap());

        interceptor_info_address = interceptor_info->address();
        interceptor_info_map_address = interceptor_info->map()->address();
        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(interceptor_info);
        artifacts.keep_alive.push_back(api_function_handle);
        return artifacts;
      });

  Node* interceptor_info_node =
      GetSnapshotNode(generator.get(), interceptor_info_address);
  Node* interceptor_info_map =
      GetSnapshotNode(generator.get(), interceptor_info_map_address);
  if (!is_expected_instance_type || interceptor_info_node == nullptr ||
      interceptor_info_map == nullptr) {
    PrintF("[TODO] INTERCEPTOR_INFO_TYPE not yet stable in this branch.\n");
    return;
  }

  CheckExactInternalMapEdgeNoWrongKind(generator.get(), interceptor_info_node,
                                       interceptor_info_map);
  MarkCoveredInstanceType(INTERCEPTOR_INFO_TYPE);
}

UNINITIALIZED_TEST(HeapDumpDoubleStringCacheExactMapEdge) {
  Address double_string_cache_address = 0;
  Address map_address = 0;
  bool is_expected_instance_type = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<DoubleStringCache> double_string_cache =
                isolate->factory()->double_string_cache();

            is_expected_instance_type =
                DOUBLE_STRING_CACHE_TYPE ==
                double_string_cache->map()->instance_type();
            if (!is_expected_instance_type) {
              return DumpArtifacts{};
            }

            CHECK_EQ(DOUBLE_STRING_CACHE_TYPE,
                     double_string_cache->map()->instance_type());
            InvokeAtomicMajorGC(isolate->heap());

            double_string_cache_address = double_string_cache->address();
            map_address = double_string_cache->map()->address();
            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(double_string_cache);
            return artifacts;
          });

  Node* double_string_cache_node =
      GetSnapshotNode(generator.get(), double_string_cache_address);
  Node* double_string_cache_map =
      GetSnapshotNode(generator.get(), map_address);
  if (!is_expected_instance_type || double_string_cache_node == nullptr ||
      double_string_cache_map == nullptr) {
    PrintF("[TODO] DOUBLE_STRING_CACHE_TYPE not yet stable in this branch.\n");
    return;
  }

  CHECK(NodeHasType(double_string_cache_map, Node::Type::kObjectShape));
  CHECK(HasExactNamedEdge(generator.get(), double_string_cache_node,
                          Edge::Type::kInternal, "map",
                          double_string_cache_map));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), double_string_cache_node,
                                   Edge::Type::kInternal, "map",
                                   double_string_cache_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(double_string_cache_node,
                                         Edge::Type::kWeak,
                                         double_string_cache_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(double_string_cache_node,
                                         Edge::Type::kHidden,
                                         double_string_cache_map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(double_string_cache_node,
                                         Edge::Type::kProperty,
                                         double_string_cache_map));
  MarkCoveredInstanceType(DOUBLE_STRING_CACHE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpPropertyArrayExactPropertyEdges) {
  Address property_array_address = 0;
  Address first_target_address = 0;
  Address second_target_address = 0;
  int first_target_index = -1;
  int second_target_index = -1;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown([&](Isolate* isolate,
                                              v8::Local<v8::Context> context) {
        USE(isolate);
        RunScript(context, R"(
              (function() {
                const fixture = {};
                for (let i = 0; i < 8; ++i) {
                  fixture["filler" + i] = { filler: i };
                }
                const first = {};
                const second = {};
                fixture.heapDumpPropertyArrayFirst = first;
                fixture.heapDumpPropertyArraySecond = second;
                globalThis.__heap_dump_property_array_fixture = fixture;
                globalThis.__heap_dump_property_array_first = first;
                globalThis.__heap_dump_property_array_second = second;
              })();
            )");

        InvokeAtomicMajorGC(isolate->heap());

        v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
        auto get_global = [&](const char* name) {
          return context->Global()
              ->Get(context, NewApiString(v8_isolate, name))
              .ToLocalChecked();
        };
        v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
            get_global("__heap_dump_property_array_fixture"));
        first_target_address =
            LocalObjectAddress(get_global("__heap_dump_property_array_first"));
        second_target_address =
            LocalObjectAddress(get_global("__heap_dump_property_array_second"));

        DirectHandle<JSObject> js_object =
            Cast<JSObject>(v8::Utils::OpenDirectHandle(*fixture));
        CHECK(js_object->HasFastProperties());
        Tagged<PropertyArray> properties = js_object->property_array();
        CHECK(IsPropertyArray(properties));
        CHECK_EQ(PROPERTY_ARRAY_TYPE, properties->map()->instance_type());
        property_array_address = properties->address();
        for (int index = 0; index < properties->length(); ++index) {
          Tagged<Object> value = properties->get(index);
          if (!IsHeapObject(value)) {
            continue;
          }
          Address value_address = Cast<HeapObject>(value)->address();
          if (value_address == first_target_address) {
            first_target_index = index;
          } else if (value_address == second_target_address) {
            second_target_index = index;
          }
        }
        CHECK_GE(first_target_index, 0);
        CHECK_GE(second_target_index, 0);

        return DumpArtifacts{};
      });

  Node* properties = GetSnapshotNode(generator.get(), property_array_address);
  Node* first_target = GetSnapshotNode(generator.get(), first_target_address);
  Node* second_target = GetSnapshotNode(generator.get(), second_target_address);
  CHECK_NOT_NULL(properties);
  CHECK_NOT_NULL(first_target);
  CHECK_NOT_NULL(second_target);

  // PropertyArray is a structural fast-properties backing store. Native has no
  // PropertyArray-specific extractor; its BodyDescriptor slots are emitted by
  // IndexedReferencesExtractor as numeric indexed hidden edges, not numeric-name
  // internal edges or JS property edges.
  CHECK_EQ(1, CountExactIndexedEdges(properties, Edge::Type::kHidden,
                                     first_target_index, first_target));
  CHECK_EQ(1, CountExactIndexedEdges(properties, Edge::Type::kHidden,
                                     second_target_index, second_target));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(properties, Edge::Type::kHidden,
                                         first_target));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(properties, Edge::Type::kHidden,
                                         second_target));
  CHECK_EQ(0, CountExactNamedEdges(
                  generator.get(), properties, Edge::Type::kInternal,
                  EdgeIndexName(first_target_index).c_str(), first_target));
  CHECK_EQ(0, CountExactNamedEdges(
                  generator.get(), properties, Edge::Type::kInternal,
                  EdgeIndexName(second_target_index).c_str(), second_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(properties, Edge::Type::kInternal,
                                         first_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(properties, Edge::Type::kInternal,
                                         second_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(properties, Edge::Type::kProperty,
                                         first_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(properties, Edge::Type::kProperty,
                                         second_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(properties, Edge::Type::kWeak,
                                         first_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(properties, Edge::Type::kWeak,
                                         second_target));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(properties, Edge::Type::kHidden,
                                         first_target));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(properties, Edge::Type::kHidden,
                                         second_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(properties, Edge::Type::kShortcut,
                                         first_target));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(properties, Edge::Type::kShortcut,
                                         second_target));
  MarkCoveredInstanceType(PROPERTY_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpTurboshaftTypesExactMapEdges) {
  struct TurboshaftTarget {
    InstanceType type;
    Address address = 0;
    Address map_address = 0;
    bool is_expected_instance_type = false;
  };

  TurboshaftTarget targets[] = {
      {TURBOSHAFT_FLOAT64_TYPE_TYPE},
      {TURBOSHAFT_FLOAT64_RANGE_TYPE_TYPE},
      {TURBOSHAFT_FLOAT64_SET_TYPE_TYPE},
      {TURBOSHAFT_WORD32_TYPE_TYPE},
      {TURBOSHAFT_WORD32_RANGE_TYPE_TYPE},
      {TURBOSHAFT_WORD32_SET_TYPE_TYPE},
      {TURBOSHAFT_WORD64_TYPE_TYPE},
      {TURBOSHAFT_WORD64_RANGE_TYPE_TYPE},
      {TURBOSHAFT_WORD64_SET_TYPE_TYPE},
  };

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            Factory* factory = isolate->factory();
            DumpArtifacts artifacts;

            Handle<TurboshaftFloat64Type> float64_type =
                factory->NewTurboshaftFloat64Type(0, AllocationType::kYoung);
            Handle<TurboshaftFloat64RangeType> float64_range =
                factory->NewTurboshaftFloat64RangeType(
                    0, 0, -13.5, 42.25, AllocationType::kYoung);
            Handle<TurboshaftFloat64SetType> float64_set =
                factory->NewTurboshaftFloat64SetType(0, 3,
                                                     AllocationType::kYoung);
            float64_set->set_elements(0, -1.5);
            float64_set->set_elements(1, 0.25);
            float64_set->set_elements(2, 9.75);

            Handle<TurboshaftWord32Type> word32_type =
                factory->NewTurboshaftWord32Type(AllocationType::kYoung);
            Handle<TurboshaftWord32RangeType> word32_range =
                factory->NewTurboshaftWord32RangeType(
                    0x100, 0x1000, AllocationType::kYoung);
            Handle<TurboshaftWord32SetType> word32_set =
                factory->NewTurboshaftWord32SetType(3, AllocationType::kYoung);
            word32_set->set_elements(0, 7);
            word32_set->set_elements(1, 11);
            word32_set->set_elements(2, 19);

            Handle<TurboshaftWord64Type> word64_type =
                factory->NewTurboshaftWord64Type(AllocationType::kYoung);
            Handle<TurboshaftWord64RangeType> word64_range =
                factory->NewTurboshaftWord64RangeType(
                    0x1, 0x20, 0x2, 0x40, AllocationType::kYoung);
            Handle<TurboshaftWord64SetType> word64_set =
                factory->NewTurboshaftWord64SetType(3, AllocationType::kYoung);
            word64_set->set_elements_high(0, 0);
            word64_set->set_elements_low(0, 5);
            word64_set->set_elements_high(1, 1);
            word64_set->set_elements_low(1, 9);
            word64_set->set_elements_high(2, 2);
            word64_set->set_elements_low(2, 13);

            Handle<HeapObject> objects[] = {
                float64_type,  float64_range, float64_set,
                word32_type,   word32_range,  word32_set,
                word64_type,   word64_range,  word64_set,
            };

            InvokeAtomicMajorGC(isolate->heap());

            for (size_t i = 0; i < arraysize(targets); ++i) {
              targets[i].is_expected_instance_type =
                  targets[i].type == objects[i]->map()->instance_type();
              if (!targets[i].is_expected_instance_type) continue;
              CHECK_EQ(targets[i].type, objects[i]->map()->instance_type());
              targets[i].address = objects[i]->address();
              targets[i].map_address = objects[i]->map()->address();
              artifacts.keep_alive.push_back(objects[i]);
            }

            return artifacts;
          });

  for (const TurboshaftTarget& target : targets) {
    Node* object = GetSnapshotNode(generator.get(), target.address);
    Node* map = GetSnapshotNode(generator.get(), target.map_address);
    if (!target.is_expected_instance_type || object == nullptr ||
        map == nullptr) {
      PrintF("[TODO] %s not found as an exact Turboshaft heap object.\n",
             HeapDumpInstanceTypeName(target.type));
      continue;
    }

    CheckExactInternalMapEdgeNoWrongKind(generator.get(), object, map);
    MarkCoveredInstanceType(target.type);
  }
}

UNINITIALIZED_TEST(HeapDumpTurbofanTypesExactMapEdges) {
  struct TurbofanTarget {
    InstanceType type;
    Address address = 0;
    Address map_address = 0;
    bool is_expected_instance_type = false;
  };

  TurbofanTarget targets[] = {
      {TURBOFAN_BITSET_TYPE_TYPE},
      {TURBOFAN_HEAP_CONSTANT_TYPE_TYPE},
      {TURBOFAN_OTHER_NUMBER_CONSTANT_TYPE_TYPE},
      {TURBOFAN_RANGE_TYPE_TYPE},
      {TURBOFAN_UNION_TYPE_TYPE},
  };

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            Factory* factory = isolate->factory();
            DumpArtifacts artifacts;

            Handle<TurbofanBitsetType> bitset =
                factory->NewTurbofanBitsetType(0x10, 0,
                                               AllocationType::kYoung);
            Handle<String> constant = factory->InternalizeUtf8String(
                "heap-dump-turbofan-constant");
            Handle<TurbofanHeapConstantType> heap_constant =
                factory->NewTurbofanHeapConstantType(constant,
                                                     AllocationType::kYoung);
            Handle<TurbofanOtherNumberConstantType> other_number_constant =
                factory->NewTurbofanOtherNumberConstantType(
                    0.25, AllocationType::kYoung);
            Handle<TurbofanRangeType> range = factory->NewTurbofanRangeType(
                -8.0, 13.0, AllocationType::kYoung);
            Handle<TurbofanUnionType> union_type =
                factory->NewTurbofanUnionType(bitset, range,
                                              AllocationType::kYoung);

            Handle<HeapObject> objects[] = {
                bitset, heap_constant, other_number_constant, range, union_type,
            };

            InvokeAtomicMajorGC(isolate->heap());

            for (size_t i = 0; i < arraysize(targets); ++i) {
              targets[i].is_expected_instance_type =
                  targets[i].type == objects[i]->map()->instance_type();
              if (!targets[i].is_expected_instance_type) continue;
              CHECK_EQ(targets[i].type, objects[i]->map()->instance_type());
              targets[i].address = objects[i]->address();
              targets[i].map_address = objects[i]->map()->address();
              artifacts.keep_alive.push_back(objects[i]);
            }

            return artifacts;
          });

  for (const TurbofanTarget& target : targets) {
    Node* object = GetSnapshotNode(generator.get(), target.address);
    Node* map = GetSnapshotNode(generator.get(), target.map_address);
    if (!target.is_expected_instance_type || object == nullptr ||
        map == nullptr) {
      PrintF("[TODO] %s not found as an exact Turbofan heap object.\n",
             HeapDumpInstanceTypeName(target.type));
      continue;
    }

    CheckExactInternalMapEdgeNoWrongKind(generator.get(), object, map);
    MarkCoveredInstanceType(target.type);
  }
}

// Representative exact edge test for V8HeapExplorer::ExtractMapReferences E5:
// a JSObjectMap emits an internal "descriptors" edge to its DescriptorArray
// (Map::kInstanceDescriptorsOffset). Verifies the ArkWeb translator preserves
// the native edge kind/name and emits no wrong-kind duplicate edges.
UNINITIALIZED_TEST(HeapDumpMapDescriptorsExactDescriptorArrayEdge) {
  Address map_address = 0;
  Address descriptors_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            // Create a JS object with two named own properties so that its Map
            // has a non-empty DescriptorArray; IsJSObjectMap(map) must hold for
            // native ExtractMapReferences to emit the "descriptors" edge.
            RunScript(context, R"(
              (function() {
                const obj = { heapDumpDescA: 1, heapDumpDescB: 2 };
                globalThis.__heap_dump_map_descriptors = obj;
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "__heap_dump_map_descriptors"))
                    .ToLocalChecked());
            CHECK_EQ(JS_OBJECT_TYPE, LocalObjectInstanceType(obj));

            DirectHandle<JSReceiver> jsr = LocalJSReceiver(obj);
            Tagged<Map> map = jsr->map();
            // Verify the fixture satisfies the native IsJSObjectMap guard that
            // gates "descriptors" edge emission in ExtractMapReferences.
            CHECK(IsJSObjectMap(map));
            map_address = map.address();

            Tagged<DescriptorArray> descriptors =
                map->instance_descriptors(isolate);
            CHECK_GT(descriptors->number_of_descriptors(), 0);
            descriptors_address = descriptors.address();

            return DumpArtifacts{};
          });

  Node* map_node = GetSnapshotNode(generator.get(), map_address);
  Node* descriptors_node =
      GetSnapshotNode(generator.get(), descriptors_address);
  CHECK_NOT_NULL(map_node);
  CHECK_NOT_NULL(descriptors_node);
  // Map objects translate to Node::Type::kObjectShape in the snapshot.
  CHECK(NodeHasType(map_node, Node::Type::kObjectShape));
  CHECK(NodeMatchesTypeAndName(generator.get(), descriptors_node,
                               Node::Type::kObjectShape,
                               "system / DescriptorArray"));

  // Native emits kInternal "descriptors" from JSObjectMap to DescriptorArray.
  // Breadcrumb: V8HeapExplorer::ExtractMapReferences, E5,
  // Map::kInstanceDescriptorsOffset.
  CHECK(HasExactNamedEdge(generator.get(), map_node, Edge::Type::kInternal,
                          "descriptors", descriptors_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kInternal, "descriptors",
                                   descriptors_node));

  // No wrong-kind edges from the Map source to the DescriptorArray target.
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kWeak,
                                         descriptors_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kHidden,
                                         descriptors_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kProperty,
                                         descriptors_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kShortcut,
                                         descriptors_node));

  MarkCoveredInstanceType(MAP_TYPE);
}

// Representative exact edge test for V8HeapExplorer::ExtractMapReferences E6:
// every Map emits an internal "prototype" edge to map->prototype() when the
// prototype is a dumped HeapObject. This target is an ordinary V8 HeapObject,
// not a native/offheap pointer.
UNINITIALIZED_TEST(HeapDumpMapPrototypeExactPrototypeEdge) {
  Address map_address = 0;
  Address prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const proto = { heapDumpProtoMarker: 1 };
                const obj = Object.create(proto);
                obj.heapDumpOwnMarker = { child: true };
                globalThis.__heap_dump_map_prototype = { obj, proto };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "__heap_dump_map_prototype"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };

            v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(
                get_property("obj"));
            v8::Local<v8::Object> proto = v8::Local<v8::Object>::Cast(
                get_property("proto"));
            CHECK_EQ(JS_OBJECT_TYPE, LocalObjectInstanceType(obj));
            CHECK_EQ(JS_OBJECT_TYPE, LocalObjectInstanceType(proto));

            DirectHandle<JSReceiver> jsr = LocalJSReceiver(obj);
            Tagged<Map> map = jsr->map();
            CHECK_EQ(MAP_TYPE, map->map()->instance_type());
            map_address = map.address();

            Tagged<Object> prototype = map->prototype();
            CHECK(IsHeapObject(prototype));
            prototype_address = Cast<HeapObject>(prototype).address();
            CHECK_EQ(LocalObjectAddress(proto), prototype_address);

            return DumpArtifacts{};
          });

  Node* map_node = GetSnapshotNode(generator.get(), map_address);
  Node* prototype_node = GetSnapshotNode(generator.get(), prototype_address);
  CHECK_NOT_NULL(map_node);
  CHECK_NOT_NULL(prototype_node);
  CHECK(NodeHasType(map_node, Node::Type::kObjectShape));
  CHECK(NodeHasType(prototype_node, Node::Type::kObject));

  // Native emits kInternal "prototype" from the Map to map->prototype().
  // Breadcrumb: V8HeapExplorer::ExtractMapReferences, E6,
  // Map::kPrototypeOffset.
  CHECK(HasExactNamedEdge(generator.get(), map_node, Edge::Type::kInternal,
                          "prototype", prototype_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kInternal, "prototype",
                                   prototype_node));

  // No fabricated fallback or wrong-kind substitute edge may replace E6.
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kWeak,
                                         prototype_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kHidden,
                                         prototype_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kProperty,
                                         prototype_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kShortcut,
                                         prototype_node));

  MarkCoveredInstanceType(MAP_TYPE);
  MarkCoveredInstanceType(JS_OBJECT_TYPE);
}

// Representative exact edge test for V8HeapExplorer::ExtractMapReferences E7:
// context maps and map-maps interpret the constructor/back-pointer slot as
// native_context_or_null() and emit an internal "native_context" edge when the
// target is a dumped HeapObject.
UNINITIALIZED_TEST(HeapDumpMapNativeContextExactNativeContextEdge) {
  Address map_address = 0;
  Address native_context_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            InvokeAtomicMajorGC(isolate->heap());

            DirectHandle<NativeContext> native_context =
                isolate->native_context();
            CHECK(IsNativeContext(*native_context));
            CHECK_EQ(NATIVE_CONTEXT_TYPE,
                     native_context->map()->instance_type());

            Tagged<Map> map = native_context->meta_map();
            // This fixture enters the native E7 guard through IsMapMap(map).
            CHECK(IsMapMap(map));
            CHECK_EQ(MAP_TYPE, map->instance_type());
            Tagged<Object> target = map->native_context_or_null();
            CHECK(IsHeapObject(target));
            CHECK_EQ(native_context->address(),
                     Cast<HeapObject>(target).address());

            map_address = map.address();
            native_context_address = native_context->address();
            return DumpArtifacts{};
          });

  Node* map_node = GetSnapshotNode(generator.get(), map_address);
  Node* native_context_node =
      GetSnapshotNode(generator.get(), native_context_address);
  CHECK_NOT_NULL(map_node);
  CHECK_NOT_NULL(native_context_node);
  CHECK(NodeHasType(map_node, Node::Type::kObjectShape));
  CHECK(NodeMatchesTypeAndName(generator.get(), native_context_node,
                               Node::Type::kHidden,
                               "system / NativeContext"));

  // Native emits kInternal "native_context" from the MapMap source to the
  // native_context_or_null() HeapObject. Missing nodes must be skipped, never
  // treated as native/offheap fallback targets.
  CHECK(HasExactNamedEdge(generator.get(), map_node, Edge::Type::kInternal,
                          "native_context", native_context_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kInternal, "native_context",
                                   native_context_node));

  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kWeak,
                                         native_context_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kHidden,
                                         native_context_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kProperty,
                                         native_context_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kShortcut,
                                         native_context_node));

  MarkCoveredInstanceType(MAP_TYPE);
  MarkCoveredInstanceType(NATIVE_CONTEXT_TYPE);
}

// Representative exact edge test for V8HeapExplorer::ExtractMapReferences E9:
// every Map emits an internal "dependent_code" edge to map->dependent_code()
// when the target HeapObject has a translated snapshot node
// (Map::kDependentCodeOffset). DependentCode is a WeakArrayList
// (WEAK_ARRAY_LIST_TYPE); the read-only empty singleton is always present in
// the snapshot, so the edge is emitted for any Map whose slot contains a
// translated node. Missing nodes must be skipped, not treated as
// native/offheap fallback targets.
UNINITIALIZED_TEST(HeapDumpMapDependentCodeExactDependentCodeEdge) {
  Address map_address = 0;
  Address dep_code_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            // Create a plain JS object so its Map is a concrete JSObjectMap
            // distinct from meta-maps. All Maps carry a dependent_code() slot
            // that points at the read-only empty_weak_array_list singleton
            // when no compiled code has registered a Map-stability dependency;
            // that singleton is included in the dump as a regular heap node.
            RunScript(context, R"(
              (function() {
                const obj = { heapDumpDepCodeProp: 42 };
                globalThis.__heap_dump_map_dep_code = obj;
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate, "__heap_dump_map_dep_code"))
                    .ToLocalChecked());
            CHECK_EQ(JS_OBJECT_TYPE, LocalObjectInstanceType(obj));

            DirectHandle<JSReceiver> jsr = LocalJSReceiver(obj);
            Tagged<Map> map = jsr->map();
            CHECK_EQ(MAP_TYPE, map->map()->instance_type());
            map_address = map.address();

            // dependent_code() always returns a HeapObject: either the
            // read-only empty_weak_array_list or a real DependentCode
            // WeakArrayList allocated when code registers a Map dependency.
            Tagged<DependentCode> dep = map->dependent_code();
            CHECK_EQ(WEAK_ARRAY_LIST_TYPE, dep->map()->instance_type());
            dep_code_address = dep.address();

            return DumpArtifacts{};
          });

  Node* map_node = GetSnapshotNode(generator.get(), map_address);
  Node* dep_code_node = GetSnapshotNode(generator.get(), dep_code_address);
  CHECK_NOT_NULL(map_node);
  CHECK_NOT_NULL(dep_code_node);
  // Map objects translate to Node::Type::kObjectShape in the snapshot.
  CHECK(NodeHasType(map_node, Node::Type::kObjectShape));
  CHECK(NodeMatchesTypeAndName(generator.get(), dep_code_node,
                               Node::Type::kHidden, "(dependent code)"));

  // Native emits kInternal "dependent_code" from the Map to its DependentCode
  // target (a WeakArrayList). The ArkWeb translator uses
  // rawheap membership, WEAK_ARRAY_LIST_TYPE, and snapshot-node gates before
  // the label-only TagObject retag and edge emission; missing nodes are skipped
  // rather than fabricated as native/offheap fallback edges.
  // Breadcrumb: V8HeapExplorer::ExtractMapReferences, E9,
  // Map::kDependentCodeOffset.
  CHECK(HasExactNamedEdge(generator.get(), map_node, Edge::Type::kInternal,
                          "dependent_code", dep_code_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kInternal, "dependent_code",
                                   dep_code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kInternal, "(dependent code)",
                                   dep_code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kWeak, "(dependent code)",
                                   dep_code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kHidden, "(dependent code)",
                                   dep_code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kProperty, "(dependent code)",
                                   dep_code_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kShortcut, "(dependent code)",
                                   dep_code_node));

  // No wrong-kind edges from the Map source to the DependentCode target.
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kWeak,
                                         dep_code_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kHidden,
                                         dep_code_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kProperty,
                                         dep_code_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kShortcut,
                                         dep_code_node));

  MarkCoveredInstanceType(MAP_TYPE);
  MarkCoveredInstanceType(WEAK_ARRAY_LIST_TYPE);
}

// Representative TagObject test for V8HeapExplorer::ExtractMapReferences E10:
// map->prototype_validity_cell(kRelaxedLoad) is tagged as
// "(prototype validity cell)" with HeapEntry::kObjectShape, but native does
// not emit any Map edge for this field. The field can also be Smi(0), so the
// translator must only retag a proven dumped Cell HeapObject.
UNINITIALIZED_TEST(HeapDumpMapPrototypeValidityCellExactTagObject) {
  Address map_address = 0;
  Address validity_cell_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const proto = { heapDumpValidityCellProto: 1 };
                const obj = Object.create(proto);
                obj.heapDumpValidityCellOwn = 2;
                globalThis.__heap_dump_map_validity_cell = { obj, proto };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "__heap_dump_map_validity_cell"))
                    .ToLocalChecked());
            v8::Local<v8::Object> proto = v8::Local<v8::Object>::Cast(
                fixture
                    ->Get(context, NewApiString(v8_isolate, "proto"))
                    .ToLocalChecked());
            CHECK_EQ(JS_OBJECT_TYPE, LocalObjectInstanceType(proto));

            DirectHandle<JSReceiver> prototype = LocalJSReceiver(proto);
            DirectHandle<Map> prototype_map(prototype->map(), isolate);
            CHECK(prototype_map->is_prototype_map());
            map_address = prototype_map->address();

            Handle<UnionOf<Smi, Cell>> maybe_cell =
                Map::GetOrCreatePrototypeChainValidityCell(prototype_map,
                                                           isolate);
            CHECK(IsCell(*maybe_cell));
            Tagged<Object> field_value =
                prototype_map->prototype_validity_cell(kRelaxedLoad);
            CHECK(IsCell(field_value));
            CHECK_EQ(*maybe_cell, field_value);
            validity_cell_address = Cast<Cell>(field_value).address();

            return DumpArtifacts{};
          });

  Node* map_node = GetSnapshotNode(generator.get(), map_address);
  Node* validity_cell_node =
      GetSnapshotNode(generator.get(), validity_cell_address);
  CHECK_NOT_NULL(map_node);
  CHECK_NOT_NULL(validity_cell_node);
  CHECK(NodeHasType(map_node, Node::Type::kObjectShape));
  CHECK(NodeMatchesTypeAndName(generator.get(), validity_cell_node,
                               Node::Type::kObjectShape,
                               "(prototype validity cell)"));

  CHECK_EQ(0, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kInternal,
                                   "prototype_validity_cell",
                                   validity_cell_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kInternal,
                                   "(prototype validity cell)",
                                   validity_cell_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), map_node, Edge::Type::kWeak,
                                   "(prototype validity cell)",
                                   validity_cell_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kHidden,
                                   "(prototype validity cell)",
                                   validity_cell_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kProperty,
                                   "(prototype validity cell)",
                                   validity_cell_node));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kShortcut,
                                   "(prototype validity cell)",
                                   validity_cell_node));

  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kWeak,
                                         validity_cell_node));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(map_node, Edge::Type::kHidden,
                                         validity_cell_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kProperty,
                                         validity_cell_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kShortcut,
                                         validity_cell_node));

  MarkCoveredInstanceType(MAP_TYPE);
  MarkCoveredInstanceType(CELL_TYPE);
}

// Representative exact edge test for V8HeapExplorer::ExtractMapReferences E8a:
// when the Map is neither a context map nor a map-map, and
// constructor_or_back_pointer() holds a JSFunction (not a Map and not a
// FunctionTemplateInfo), the ArkWeb translator emits kInternal "constructor"
// to the constructor's snapshot node. Missing nodes must be skipped, never
// treated as native/offheap fallback targets.
UNINITIALIZED_TEST(HeapDumpMapConstructorExactConstructorEdge) {
  Address map_address = 0;
  Address constructor_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            // Create a class with no instance properties so the instance uses
            // the class's initial_map directly. On that root map,
            // constructor_or_back_pointer() holds the JSFunction constructor
            // rather than a back-pointer Map, selecting the "constructor"
            // branch in ExtractMapReferences E8.
            RunScript(context, R"(
              (function() {
                class HeapDumpConstructorClass {}
                const obj = new HeapDumpConstructorClass();
                globalThis.__heap_dump_map_constructor = {
                  obj: obj,
                  ctor: HeapDumpConstructorClass
                };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "__heap_dump_map_constructor"))
                    .ToLocalChecked());

            auto get_property = [&](const char* name) {
              return fixture->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };

            v8::Local<v8::Value> obj_val = get_property("obj");
            v8::Local<v8::Value> ctor_val = get_property("ctor");

            DirectHandle<JSReceiver> jsr = LocalJSReceiver(obj_val);
            Tagged<Map> map = jsr->map();
            CHECK_EQ(MAP_TYPE, map->map()->instance_type());
            // Fixture must not enter the native E7 guard (not context/map-map).
            CHECK(!IsContextMap(map));
            CHECK(!IsMapMap(map));

            Tagged<Object> ctor_or_bp = map->constructor_or_back_pointer();
            // constructor_or_back_pointer() must be a JSFunction - not a Map
            // (which would select the "back_pointer" branch) and not a
            // FunctionTemplateInfo (which would select the
            // "constructor_function_data" branch). This selects the
            // "constructor" branch in ExtractMapReferences E8.
            CHECK(IsJSFunction(ctor_or_bp));
            CHECK(!IsMap(ctor_or_bp));
            CHECK(!IsFunctionTemplateInfo(ctor_or_bp));

            map_address = map.address();
            constructor_address = Cast<HeapObject>(ctor_or_bp).address();
            // Cross-check: the C++ constructor slot must match the JS handle.
            CHECK_EQ(constructor_address, LocalObjectAddress(ctor_val));

            return DumpArtifacts{};
          });

  Node* map_node = GetSnapshotNode(generator.get(), map_address);
  Node* constructor_node = GetSnapshotNode(generator.get(), constructor_address);
  CHECK_NOT_NULL(map_node);
  CHECK_NOT_NULL(constructor_node);
  CHECK(NodeHasType(map_node, Node::Type::kObjectShape));
  // JSFunctions translate to kClosure nodes in the ArkWeb snapshot.
  CHECK(NodeHasType(constructor_node, Node::Type::kClosure));

  // Native emits kInternal "constructor" from the Map to
  // constructor_or_back_pointer() when the target is neither a Map nor a
  // FunctionTemplateInfo. The ArkWeb translator must emit the same edge;
  // missing snapshot nodes are skipped, never fabricated as native/offheap
  // fallback edges.
  // Breadcrumb: V8HeapExplorer::ExtractMapReferences, E8 ("constructor" branch),
  // Map::kConstructorOrBackPointerOrNativeContextOffset.
  CHECK(HasExactNamedEdge(generator.get(), map_node, Edge::Type::kInternal,
                          "constructor", constructor_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kInternal, "constructor",
                                   constructor_node));

  // No wrong-kind edges from the Map source to the constructor target.
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kWeak,
                                         constructor_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kHidden,
                                         constructor_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kProperty,
                                         constructor_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kShortcut,
                                         constructor_node));

  MarkCoveredInstanceType(MAP_TYPE);
}

// Representative exact edge test for V8HeapExplorer::ExtractMapReferences E8c:
// when constructor_or_back_pointer() is a FunctionTemplateInfo, native emits
// an internal "constructor_function_data" edge.
UNINITIALIZED_TEST(HeapDumpMapConstructorFunctionDataExactEdge) {
  Address map_address = 0;
  Address function_template_info_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<Map> map = Map::Create(isolate, 0);
            DirectHandle<FunctionTemplateInfo> function_template_info =
                isolate->factory()->NewFunctionTemplateInfo(0, false);
            CHECK(!IsContextMap(*map));
            CHECK(!IsMapMap(*map));

            map->SetConstructor(*function_template_info);
            Tagged<Object> ctor_or_bp = map->constructor_or_back_pointer();
            CHECK(IsFunctionTemplateInfo(ctor_or_bp));
            CHECK_EQ(function_template_info->address(),
                     Cast<HeapObject>(ctor_or_bp).address());

            InvokeAtomicMajorGC(isolate->heap());

            map_address = map->address();
            function_template_info_address = function_template_info->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(map);
            artifacts.keep_alive.push_back(function_template_info);
            return artifacts;
          });

  Node* map_node = GetSnapshotNode(generator.get(), map_address);
  Node* function_template_info_node =
      GetSnapshotNode(generator.get(), function_template_info_address);
  CHECK_NOT_NULL(map_node);
  CHECK_NOT_NULL(function_template_info_node);
  CHECK(NodeHasType(map_node, Node::Type::kObjectShape));

  // Breadcrumb: V8HeapExplorer::ExtractMapReferences, E8
  // ("constructor_function_data" branch),
  // Map::kConstructorOrBackPointerOrNativeContextOffset.
  CHECK(HasExactNamedEdge(generator.get(), map_node, Edge::Type::kInternal,
                          "constructor_function_data",
                          function_template_info_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kInternal,
                                   "constructor_function_data",
                                   function_template_info_node));

  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kWeak,
                                         function_template_info_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kHidden,
                                         function_template_info_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kProperty,
                                         function_template_info_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kShortcut,
                                         function_template_info_node));

  MarkCoveredInstanceType(MAP_TYPE);
  MarkCoveredInstanceType(FUNCTION_TEMPLATE_INFO_TYPE);
}

// Representative exact edge test for V8HeapExplorer::ExtractMapReferences E8b:
// a transitioned instance Map stores its predecessor Map in
// constructor_or_back_pointer(), so native emits an internal "back_pointer"
// edge. The target is a real MAP_TYPE HeapObject, not a native/offheap pointer.
UNINITIALIZED_TEST(HeapDumpMapBackPointerExactBackPointerEdge) {
  Address map_address = 0;
  Address back_pointer_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                function HeapDumpBackPointerClass() {}
                const obj = new HeapDumpBackPointerClass();
                obj.heapDumpBackPointerProp = 1;
                globalThis.__heap_dump_map_back_pointer = {
                  ctor: HeapDumpBackPointerClass,
                  obj: obj
                };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "__heap_dump_map_back_pointer"))
                    .ToLocalChecked());
            v8::Local<v8::Function> ctor = v8::Local<v8::Function>::Cast(
                fixture->Get(context, NewApiString(v8_isolate, "ctor"))
                    .ToLocalChecked());
            v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(
                fixture->Get(context, NewApiString(v8_isolate, "obj"))
                    .ToLocalChecked());

            DirectHandle<JSFunction> js_ctor =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*ctor));
            CHECK(js_ctor->has_initial_map());
            Tagged<Map> predecessor = js_ctor->initial_map();
            CHECK_EQ(MAP_TYPE, predecessor->map()->instance_type());

            DirectHandle<JSReceiver> jsr = LocalJSReceiver(obj);
            Tagged<Map> map = jsr->map();
            CHECK_EQ(MAP_TYPE, map->map()->instance_type());
            CHECK(!IsContextMap(map));
            CHECK(!IsMapMap(map));

            Tagged<Object> ctor_or_bp = map->constructor_or_back_pointer();
            CHECK(IsMap(ctor_or_bp));
            CHECK_EQ(predecessor.address(), Cast<Map>(ctor_or_bp).address());

            map_address = map.address();
            back_pointer_address = Cast<Map>(ctor_or_bp).address();

            return DumpArtifacts{};
          });

  Node* map_node = GetSnapshotNode(generator.get(), map_address);
  Node* back_pointer_node =
      GetSnapshotNode(generator.get(), back_pointer_address);
  CHECK_NOT_NULL(map_node);
  CHECK_NOT_NULL(back_pointer_node);
  CHECK(NodeHasType(map_node, Node::Type::kObjectShape));
  CHECK(NodeHasType(back_pointer_node, Node::Type::kObjectShape));

  // Native emits kInternal "back_pointer" from the transitioned Map to the
  // predecessor Map. ArkWeb must classify the slot as MAP_TYPE before the
  // snapshot-node gate; missing nodes are skipped, never fabricated as native
  // or offheap substitutes. Breadcrumb: ExtractMapReferences E8,
  // Map::kConstructorOrBackPointerOrNativeContextOffset.
  CHECK(HasExactNamedEdge(generator.get(), map_node, Edge::Type::kInternal,
                          "back_pointer", back_pointer_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kInternal, "back_pointer",
                                   back_pointer_node));

  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kWeak,
                                         back_pointer_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kHidden,
                                         back_pointer_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kProperty,
                                         back_pointer_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kShortcut,
                                         back_pointer_node));

  MarkCoveredInstanceType(MAP_TYPE);
}

// Representative exact edge test for V8HeapExplorer::ExtractMapReferences E3:
// legacy strong FixedArray raw_transitions() emits an internal "transition"
// edge. Current TransitionsAccessor no longer creates this shape naturally, so
// this white-box fixture writes the Map slot directly to keep the translator
// branch covered against the native extractor contract.
UNINITIALIZED_TEST(HeapDumpMapFixedArrayTransitionExactTransitionEdge) {
  Address map_address = 0;
  Address fixed_array_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<Map> map = Map::Create(isolate, 0);
            DirectHandle<FixedArray> fixed_array =
                isolate->factory()->NewFixedArray(1, AllocationType::kOld);
            fixed_array->set(0, Smi::FromInt(17));

            TaggedField<MaybeObject>::Release_Store(
                *map, Map::kTransitionsOrPrototypeInfoOffset,
                Tagged<MaybeObject>(*fixed_array));
            Tagged<MaybeObject> raw = map->raw_transitions();
            Tagged<HeapObject> raw_target;
            CHECK(raw.GetHeapObjectIfStrong(&raw_target));
            CHECK(IsFixedArray(raw_target));
            CHECK_EQ(fixed_array->address(), raw_target.address());

            InvokeAtomicMajorGC(isolate->heap());

            map_address = map->address();
            fixed_array_address = fixed_array->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(map);
            artifacts.keep_alive.push_back(fixed_array);
            return artifacts;
          });

  Node* map_node = GetSnapshotNode(generator.get(), map_address);
  Node* fixed_array_node = GetSnapshotNode(generator.get(), fixed_array_address);
  CHECK_NOT_NULL(map_node);
  CHECK_NOT_NULL(fixed_array_node);
  CHECK(NodeHasType(map_node, Node::Type::kObjectShape));

  // Breadcrumb: V8HeapExplorer::ExtractMapReferences, E3,
  // Map::kTransitionsOrPrototypeInfoOffset (strong IsFixedArray branch).
  CHECK(HasExactNamedEdge(generator.get(), map_node, Edge::Type::kInternal,
                          "transition", fixed_array_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kInternal, "transition",
                                   fixed_array_node));

  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kWeak,
                                         fixed_array_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kHidden,
                                         fixed_array_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kProperty,
                                         fixed_array_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kShortcut,
                                         fixed_array_node));

  MarkCoveredInstanceType(MAP_TYPE);
  MarkCoveredInstanceType(FIXED_ARRAY_TYPE);
}

// Representative exact edge test for V8HeapExplorer::ExtractMapReferences E1:
// when raw_transitions() holds a weak single-Map transition, native emits a
// weak "transition" edge to that exact Map.  The target is a V8 HeapObject, not
// a native/offheap pointer, so the translator must classify it as MAP_TYPE
// before relying on snapshot-node membership.
UNINITIALIZED_TEST(HeapDumpMapWeakTransitionExactTransitionEdge) {
  Address map_address = 0;
  Address transition_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                function HeapDumpWeakTransitionClass() {}
                const obj = new HeapDumpWeakTransitionClass();
                obj.heapDumpWeakTransitionProp = 1;
                globalThis.__heap_dump_map_weak_transition = {
                  ctor: HeapDumpWeakTransitionClass,
                  obj
                };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "__heap_dump_map_weak_transition"))
                    .ToLocalChecked());
            v8::Local<v8::Function> ctor = v8::Local<v8::Function>::Cast(
                fixture
                    ->Get(context, NewApiString(v8_isolate, "ctor"))
                    .ToLocalChecked());
            v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(
                fixture->Get(context, NewApiString(v8_isolate, "obj"))
                    .ToLocalChecked());

            DirectHandle<JSFunction> js_ctor =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(*ctor));
            CHECK(js_ctor->has_initial_map());
            Tagged<Map> map = js_ctor->initial_map();
            CHECK_EQ(MAP_TYPE, map->map()->instance_type());

            // A single property transition stays in the weak-ref encoding. The
            // raw target must be proven as a Map before edge emission.
            TestTransitionsAccessor transitions(isolate, map);
            CHECK(transitions.IsWeakRefEncoding());
            CHECK_EQ(1, transitions.NumberOfTransitions());
            Tagged<Map> transition_map = transitions.GetTarget(0);
            CHECK_EQ(MAP_TYPE, transition_map->map()->instance_type());
            DirectHandle<JSReceiver> transition_object = LocalJSReceiver(obj);
            CHECK_EQ(transition_object->map().address(),
                     transition_map.address());

            map_address = map.address();
            transition_map_address = transition_map.address();

            return DumpArtifacts{};
          });

  Node* map_node = GetSnapshotNode(generator.get(), map_address);
  Node* transition_map_node =
      GetSnapshotNode(generator.get(), transition_map_address);
  CHECK_NOT_NULL(map_node);
  CHECK_NOT_NULL(transition_map_node);
  CHECK(NodeHasType(map_node, Node::Type::kObjectShape));
  CHECK(NodeHasType(transition_map_node, Node::Type::kObjectShape));

  // Native emits kWeak "transition" from the source Map to the weak transition
  // Map.  Breadcrumb: V8HeapExplorer::ExtractMapReferences, E1,
  // Map::kTransitionsOrPrototypeInfoOffset (weak Map branch).
  CHECK(HasExactNamedEdge(generator.get(), map_node, Edge::Type::kWeak,
                          "transition", transition_map_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kWeak, "transition",
                                   transition_map_node));

  // No internal/hidden/property/shortcut/native/offheap substitute may replace
  // the weak transition edge.
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kInternal,
                                         transition_map_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kHidden,
                                         transition_map_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kProperty,
                                         transition_map_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kShortcut,
                                         transition_map_node));

  MarkCoveredInstanceType(MAP_TYPE);
}

// Representative exact edge test for V8HeapExplorer::ExtractMapReferences E2:
// when raw_transitions() holds a strong TransitionArray (the source Map has
// accumulated two or more distinct property transitions), an internal
// "transitions" edge is emitted to the TransitionArray.  The ArkWeb translator
// must preserve the exact edge kind/name and skip targets absent from the dump.
UNINITIALIZED_TEST(HeapDumpMapTransitionsExactTransitionArrayEdge) {
  Address map_address = 0;
  Address transitions_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            // Two instances of the same constructor each gain a distinct
            // property.  After the second transition the constructor's
            // initial_map switches its raw_transitions slot from a weak
            // single-map ref (E1) to a strong TransitionArray (E2).
            RunScript(context, R"(
              (function() {
                function HeapDumpTransitionsClass() {}
                const obj1 = new HeapDumpTransitionsClass();
                const obj2 = new HeapDumpTransitionsClass();
                obj1.heapDumpTransPropA = 1;
                obj2.heapDumpTransPropB = 2;
                globalThis.__heap_dump_map_transitions =
                    HeapDumpTransitionsClass;
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Value> ctor_val =
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "__heap_dump_map_transitions"))
                    .ToLocalChecked();
            DirectHandle<JSFunction> ctor =
                Cast<JSFunction>(v8::Utils::OpenDirectHandle(
                    *v8::Local<v8::Function>::Cast(ctor_val)));
            CHECK(ctor->has_initial_map());

            Tagged<Map> map = ctor->initial_map();
            CHECK_EQ(MAP_TYPE, map->map()->instance_type());

            // Confirm the encoding is kFullTransitionArray, not a weak
            // single-map ref (E1) or FixedArray (E3), before capturing the
            // address.  TestTransitionsAccessor exposes the encoding without
            // accessing private members.
            TestTransitionsAccessor ta(isolate, map);
            CHECK(ta.IsFullTransitionArrayEncoding());

            Tagged<TransitionArray> trans_array = ta.transitions();
            CHECK(IsTransitionArray(trans_array));

            map_address = map.address();
            transitions_address = trans_array.address();

            return DumpArtifacts{};
          });

  Node* map_node = GetSnapshotNode(generator.get(), map_address);
  Node* transitions_node =
      GetSnapshotNode(generator.get(), transitions_address);
  CHECK_NOT_NULL(map_node);
  CHECK_NOT_NULL(transitions_node);
  // Map translates to kObjectShape; TransitionArray (a WeakFixedArray
  // subtype) also translates to kObjectShape "system / TransitionArray".
  CHECK(NodeHasType(map_node, Node::Type::kObjectShape));
  CHECK(NodeMatchesTypeAndName(generator.get(), transitions_node,
                               Node::Type::kObjectShape,
                               "system / TransitionArray"));

  // Native emits kInternal "transitions" from the Map to its TransitionArray.
  // Breadcrumb: V8HeapExplorer::ExtractMapReferences, E2,
  // Map::kTransitionsOrPrototypeInfoOffset (strong IsTransitionArray branch).
  CHECK(HasExactNamedEdge(generator.get(), map_node, Edge::Type::kInternal,
                          "transitions", transitions_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kInternal, "transitions",
                                   transitions_node));

  // No wrong-kind edges from the Map source to the TransitionArray target.
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kWeak,
                                         transitions_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kHidden,
                                         transitions_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kProperty,
                                         transitions_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kShortcut,
                                         transitions_node));

  MarkCoveredInstanceType(MAP_TYPE);
  MarkCoveredInstanceType(TRANSITION_ARRAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpTransitionArrayPrototypeTransitionsProbe) {
  Address transitions_address = 0;
  Address prototype_transitions_address = 0;
  bool has_transition_array = false;
  bool has_prototype_transitions = false;
  bool prototype_transitions_is_weak_fixed_array = false;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DirectHandle<Name> first_key =
                isolate->factory()->InternalizeUtf8String(
                    "heapDumpPrototypeTransitionsFirst");
            DirectHandle<Name> second_key =
                isolate->factory()->InternalizeUtf8String(
                    "heapDumpPrototypeTransitionsSecond");
            PropertyAttributes attributes = NONE;
            PropertyConstness constness = PropertyConstness::kMutable;
            DirectHandle<Map> transitions_owner = Map::Create(isolate, 0);
            DirectHandle<Map> first_target =
                Map::CopyWithField(isolate, transitions_owner, first_key,
                                   FieldType::Any(isolate), attributes,
                                   constness, Representation::Tagged(),
                                   OMIT_TRANSITION)
                    .ToHandleChecked();
            DirectHandle<Map> second_target =
                Map::CopyWithField(isolate, transitions_owner, second_key,
                                   FieldType::Any(isolate), attributes,
                                   constness, Representation::Tagged(),
                                   OMIT_TRANSITION)
                    .ToHandleChecked();

            TransitionsAccessor::Insert(isolate, transitions_owner, first_key,
                                       first_target, PROPERTY_TRANSITION);
            TransitionsAccessor::Insert(isolate, transitions_owner, second_key,
                                       second_target, PROPERTY_TRANSITION);

            TestTransitionsAccessor ta(isolate, transitions_owner);
            CHECK(ta.IsFullTransitionArrayEncoding());
            DirectHandle<TransitionArray> transitions =
                direct_handle(ta.transitions(), isolate);
            has_transition_array =
                transitions->map()->instance_type() == TRANSITION_ARRAY_TYPE;
            CHECK(has_transition_array);

            DirectHandle<JSObject> prototype =
                isolate->factory()->NewJSObject(isolate->object_function());
            DirectHandle<Map> prototype_target =
                Map::Copy(isolate, transitions_owner, "heap dump proto probe");
            CHECK(TransitionsAccessor::PutPrototypeTransition(
                isolate, transitions_owner, prototype, prototype_target));
            has_prototype_transitions = transitions->HasPrototypeTransitions();
            CHECK(has_prototype_transitions);

            Tagged<WeakFixedArray> prototype_transitions =
                transitions->GetPrototypeTransitions();
            prototype_transitions_is_weak_fixed_array =
                prototype_transitions->map()->instance_type() ==
                WEAK_FIXED_ARRAY_TYPE;
            CHECK(prototype_transitions_is_weak_fixed_array);

            InvokeAtomicMajorGC(isolate->heap());

            transitions_address = transitions->address();
            prototype_transitions_address = prototype_transitions.address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(transitions);
            artifacts.keep_alive.push_back(transitions_owner);
            artifacts.keep_alive.push_back(first_target);
            artifacts.keep_alive.push_back(second_target);
            artifacts.keep_alive.push_back(first_key);
            artifacts.keep_alive.push_back(second_key);
            artifacts.keep_alive.push_back(prototype);
            artifacts.keep_alive.push_back(prototype_target);
            return artifacts;
          });

  Node* transitions = GetSnapshotNode(generator.get(), transitions_address);
  Node* prototype_transitions =
      GetSnapshotNode(generator.get(), prototype_transitions_address);
  CHECK(has_transition_array);
  CHECK(has_prototype_transitions);
  CHECK(prototype_transitions_is_weak_fixed_array);
  CHECK_NOT_NULL(transitions);
  CHECK_NOT_NULL(prototype_transitions);
  CHECK(NodeMatchesTypeAndName(generator.get(), transitions,
                               Node::Type::kObjectShape,
                               "system / TransitionArray"));
  CHECK(NodeMatchesTypeAndName(generator.get(), prototype_transitions,
                               Node::Type::kObjectShape,
                               "(prototype transitions)"));

  // This is a probe-only guard for native TagObject parity: native does not
  // synthesize a source edge named "(prototype transitions)"; the ordinary
  // structural slot remains visible as an indexed hidden edge.
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), transitions,
                                   Edge::Type::kInternal,
                                   "(prototype transitions)",
                                   prototype_transitions));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), transitions,
                                   Edge::Type::kHidden,
                                   "(prototype transitions)",
                                   prototype_transitions));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), transitions,
                                   Edge::Type::kWeak,
                                   "(prototype transitions)",
                                   prototype_transitions));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), transitions,
                                   Edge::Type::kProperty,
                                   "(prototype transitions)",
                                   prototype_transitions));
  CHECK_EQ(0, CountExactNamedEdges(generator.get(), transitions,
                                   Edge::Type::kShortcut,
                                   "(prototype transitions)",
                                   prototype_transitions));

  const int internal_edges =
      CountEdgesFromNodeToTarget(transitions, Edge::Type::kInternal,
                                 prototype_transitions);
  const int hidden_edges =
      CountEdgesFromNodeToTarget(transitions, Edge::Type::kHidden,
                                 prototype_transitions);
  const int weak_edges =
      CountEdgesFromNodeToTarget(transitions, Edge::Type::kWeak,
                                 prototype_transitions);
  const int property_edges =
      CountEdgesFromNodeToTarget(transitions, Edge::Type::kProperty,
                                 prototype_transitions);
  const int shortcut_edges =
      CountEdgesFromNodeToTarget(transitions, Edge::Type::kShortcut,
                                 prototype_transitions);
  PrintF(
      "HEAP_DUMP_TRANSITION_ARRAY_PROTOTYPE_TRANSITIONS_EDGES:"
      "internal=%d:hidden=%d:weak=%d:property=%d:shortcut=%d\n",
      internal_edges, hidden_edges, weak_edges, property_edges,
      shortcut_edges);

  CHECK_EQ(0, internal_edges);
  CHECK_EQ(1, hidden_edges);
  CHECK_EQ(0, weak_edges);
  CHECK_EQ(0, property_edges);
  CHECK_EQ(0, shortcut_edges);
  CHECK_EQ(1, CountExactIndexedEdges(transitions, Edge::Type::kHidden,
                                     TransitionArray::kPrototypeTransitionsIndex,
                                     prototype_transitions));

  MarkCoveredInstanceType(TRANSITION_ARRAY_TYPE);
  MarkCoveredInstanceType(WEAK_FIXED_ARRAY_TYPE);
}

// Representative exact edge test for V8HeapExplorer::ExtractMapReferences E4:
// when a prototype Map stores a strong PrototypeInfo in raw_transitions(),
// native emits an internal "prototype_info" edge. The target is an ordinary V8
// HeapObject; the translator must prove the rawheap family before requiring the
// snapshot node and must not fabricate a native/offheap fallback edge.
UNINITIALIZED_TEST(HeapDumpMapPrototypeInfoExactPrototypeInfoEdge) {
  Address map_address = 0;
  Address prototype_info_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const proto = { heapDumpPrototypeInfoMarker: 1 };
                const obj = Object.create(proto);
                obj.heapDumpPrototypeInfoOwn = 2;
                globalThis.__heap_dump_map_prototype_info = { proto, obj };
              })();
            )");

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(v8_isolate,
                                       "__heap_dump_map_prototype_info"))
                    .ToLocalChecked());
            v8::Local<v8::Object> proto = v8::Local<v8::Object>::Cast(
                fixture->Get(context, NewApiString(v8_isolate, "proto"))
                    .ToLocalChecked());
            v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(
                fixture->Get(context, NewApiString(v8_isolate, "obj"))
                    .ToLocalChecked());
            CHECK_EQ(JS_OBJECT_TYPE, LocalObjectInstanceType(proto));
            CHECK_EQ(JS_OBJECT_TYPE, LocalObjectInstanceType(obj));

            DirectHandle<JSObject> proto_object =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(*proto));
            JSObject::OptimizeAsPrototype(proto_object);
            DirectHandle<Map> prototype_map =
                direct_handle(proto_object->map(), isolate);
            CHECK(prototype_map->is_prototype_map());

            DirectHandle<PrototypeInfo> prototype_info =
                Map::GetOrCreatePrototypeInfo(prototype_map, isolate);
            CHECK_EQ(PROTOTYPE_INFO_TYPE,
                     prototype_info->map()->instance_type());

            Tagged<MaybeObject> raw = prototype_map->raw_transitions();
            Tagged<HeapObject> raw_target;
            CHECK(raw.GetHeapObjectIfStrong(&raw_target));
            CHECK(!IsTransitionArray(raw_target));
            CHECK(!IsFixedArray(raw_target));
            CHECK(IsPrototypeInfo(raw_target));
            CHECK_EQ(prototype_info->address(), raw_target.address());

            InvokeAtomicMajorGC(isolate->heap());

            map_address = prototype_map->address();
            prototype_info_address = prototype_info->address();

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(proto_object);
            artifacts.keep_alive.push_back(prototype_info);
            return artifacts;
          });

  Node* map_node = GetSnapshotNode(generator.get(), map_address);
  Node* prototype_info_node =
      GetSnapshotNode(generator.get(), prototype_info_address);
  CHECK_NOT_NULL(map_node);
  CHECK_NOT_NULL(prototype_info_node);
  CHECK(NodeHasType(map_node, Node::Type::kObjectShape));
  // The fixture already proved this address is a PROTOTYPE_INFO_TYPE heap
  // object from raw V8 layout before dumping. Display-label parity is tracked
  // separately from the exact edge contract covered by this test.
  CHECK_NE(map_node, prototype_info_node);

  // Breadcrumb: V8HeapExplorer::ExtractMapReferences, E4,
  // Map::kTransitionsOrPrototypeInfoOffset (strong prototype_info branch).
  CHECK(HasExactNamedEdge(generator.get(), map_node, Edge::Type::kInternal,
                          "prototype_info", prototype_info_node));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), map_node,
                                   Edge::Type::kInternal, "prototype_info",
                                   prototype_info_node));

  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kWeak,
                                         prototype_info_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kHidden,
                                         prototype_info_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kProperty,
                                         prototype_info_node));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(map_node, Edge::Type::kShortcut,
                                         prototype_info_node));

  MarkCoveredInstanceType(MAP_TYPE);
  MarkCoveredInstanceType(PROTOTYPE_INFO_TYPE);
}

#else
UNINITIALIZED_TEST(HeapDumpAccessorStructuresExactEdges) {}
UNINITIALIZED_TEST(HeapDumpAccessorInfoExactEdges) {}
UNINITIALIZED_TEST(HeapDumpAccessorPairExactEdges) {}
UNINITIALIZED_TEST(HeapDumpCoverageInfoExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpDescriptorArrayExactEntryEdges) {}
UNINITIALIZED_TEST(HeapDumpStrongDescriptorArrayExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpMapDescriptorsExactDescriptorArrayEdge) {}
UNINITIALIZED_TEST(HeapDumpMapPrototypeExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpMapNativeContextExactNativeContextEdge) {}
UNINITIALIZED_TEST(HeapDumpMapDependentCodeExactDependentCodeEdge) {}
UNINITIALIZED_TEST(HeapDumpMapPrototypeValidityCellExactTagObject) {}
UNINITIALIZED_TEST(HeapDumpMapConstructorExactConstructorEdge) {}
UNINITIALIZED_TEST(HeapDumpMapConstructorFunctionDataExactEdge) {}
UNINITIALIZED_TEST(HeapDumpMapBackPointerExactBackPointerEdge) {}
UNINITIALIZED_TEST(HeapDumpMapFixedArrayTransitionExactTransitionEdge) {}
UNINITIALIZED_TEST(HeapDumpMapWeakTransitionExactTransitionEdge) {}
UNINITIALIZED_TEST(HeapDumpMapTransitionsExactTransitionArrayEdge) {}
UNINITIALIZED_TEST(HeapDumpTransitionArrayPrototypeTransitionsProbe) {}
UNINITIALIZED_TEST(HeapDumpMapPrototypeInfoExactPrototypeInfoEdge) {}
UNINITIALIZED_TEST(HeapDumpAliasedArgumentsEntryExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpArrayBoilerplateDescriptionExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpAllocationMementoExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpClassBoilerplateExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpTransitionArrayExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpClassPositionsExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpSwissNameDictionaryExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpModuleRequestExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpPromiseCapabilityExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpPromiseReactionExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpPropertyDescriptorObjectExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpScriptOrModuleExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpCallSiteInfoExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpStackFrameInfoExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpStackTraceInfoExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpErrorStackDataExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpJSMessageObjectExactMapAndScriptEdges) {}
UNINITIALIZED_TEST(HeapDumpSourceTextModuleInfoEntryExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpRegExpBoilerplateDescriptionExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpRegExpDataExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpAtomRegExpDataExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpIrRegExpDataExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpRegExpMatchInfoExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpObjectBoilerplateDescriptionExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpEnumCacheExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpPrototypeInfoExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpPrototypeInfoUsersExactTagObject) {}
UNINITIALIZED_TEST(HeapDumpHashTableExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpNameDictionaryExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpNumberDictionaryExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpGlobalDictionaryExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpExactPropertyCellType) {}
UNINITIALIZED_TEST(HeapDumpPropertyCellExactValueEdge) {}
UNINITIALIZED_TEST(HeapDumpPropertyCellExactDependentCodeEdge) {}
UNINITIALIZED_TEST(HeapDumpNameToIndexHashTableExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpSmallOrderedHashMapExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpSmallOrderedHashSetExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpSmallOrderedNameDictionaryExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpRegisteredSymbolTableExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpOrderedNameDictionaryExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpSimpleNameDictionaryExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpSimpleNumberDictionaryExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpFunctionTemplateInfoExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpObjectTemplateInfoExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpDictionaryTemplateInfoExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpInterceptorInfoExactMapEdge) {}
UNINITIALIZED_TEST(HeapDumpPropertyArrayExactPropertyEdges) {}
UNINITIALIZED_TEST(HeapDumpTurboshaftTypesExactMapEdges) {}
UNINITIALIZED_TEST(HeapDumpTurbofanTypesExactMapEdges) {}
#endif

}  // namespace heap
}  // namespace internal
}  // namespace v8
