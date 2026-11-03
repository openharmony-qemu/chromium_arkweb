#include "arkweb/chromium_ext/v8/dfx/heap_dump/tests/cctest/heap-dump-test-utils.h"

#include <sstream>
#include <vector>

#include "src/heap/factory-inl.h"
#include "src/objects/js-objects.h"
#include "src/objects/tagged-field-inl.h"
#include "src/objects/torque-defined-classes-inl.h"
#include "src/roots/roots.h"

namespace v8 {
namespace internal {
namespace heap {

#if defined(OH_ENABLE_HEAP_DUMP_TEST) && defined(OH_ENABLE_HEAP_DUMP) && \
    defined(OH_ENABLE_HEAP_TRANSLATE) && defined(V8_ENABLE_TURBOFAN)

namespace {

using namespace heap_dump_test;

template <typename T>
void KeepAlive(std::vector<DirectHandle<Object>>* keep_alive,
               DirectHandle<T> object) {
  keep_alive->push_back(Cast<Object>(object));
}

template <typename T>
DirectHandle<T> AllocateTorqueObject(Isolate* isolate, RootIndex map_root) {
  ReadOnlyRoots roots(isolate);
  DirectHandle<Map> map(Cast<Map>(roots.object_at(map_root)), isolate);
  Tagged<HeapObject> raw =
      isolate->factory()->NewForTest(map, AllocationType::kOld);
  return direct_handle(Cast<T>(raw), isolate);
}

DirectHandle<InternalClass> CreateTorqueInternalClassForTest(Isolate* isolate) {
  DirectHandle<InternalClass> object =
      AllocateTorqueObject<InternalClass>(isolate, RootIndex::kInternalClassMap);
  TaggedField<Smi>::store(*object, InternalClass::kAOffset, Smi::FromInt(5));
  TaggedField<Object>::store(*object, InternalClass::kBOffset, Smi::FromInt(6));
  return object;
}

DirectHandle<SmiPair> CreateTorqueSmiPairForTest(Isolate* isolate) {
  DirectHandle<SmiPair> object =
      AllocateTorqueObject<SmiPair>(isolate, RootIndex::kSmiPairMap);
  TaggedField<Smi>::store(*object, SmiPair::kAOffset, Smi::FromInt(7));
  TaggedField<Smi>::store(*object, SmiPair::kBOffset, Smi::FromInt(2));
  return object;
}

DirectHandle<SmiBox> CreateTorqueSmiBoxForTest(Isolate* isolate) {
  DirectHandle<SmiBox> object =
      AllocateTorqueObject<SmiBox>(isolate, RootIndex::kSmiBoxMap);
  TaggedField<Smi>::store(*object, SmiBox::kValueOffset, Smi::FromInt(123));
  TaggedField<Smi>::store(*object, SmiBox::kUnrelatedOffset, Smi::FromInt(0));
  return object;
}

DirectHandle<ExportedSubClassBase> CreateTorqueExportedSubClassBaseForTest(
    Isolate* isolate) {
  ReadOnlyRoots roots(isolate);
  DirectHandle<ExportedSubClassBase> object =
      AllocateTorqueObject<ExportedSubClassBase>(
          isolate, RootIndex::kExportedSubClassBaseMap);
  TaggedField<Object>::store(*object, ExportedSubClassBase::kAOffset,
                             roots.null_value());
  TaggedField<Object>::store(*object, ExportedSubClassBase::kBOffset,
                             roots.null_value());
  return object;
}

DirectHandle<ExportedSubClass> CreateTorqueExportedSubClassForTest(
    Isolate* isolate) {
  ReadOnlyRoots roots(isolate);
  DirectHandle<ExportedSubClass> object =
      AllocateTorqueObject<ExportedSubClass>(isolate,
                                             RootIndex::kExportedSubClassMap);
  TaggedField<Object>::store(*object, ExportedSubClass::kAOffset,
                             roots.null_value());
  TaggedField<Object>::store(*object, ExportedSubClass::kBOffset,
                             roots.null_value());
  object->template WriteField<int32_t>(ExportedSubClass::kCFieldOffset, 7);
  object->template WriteField<int32_t>(ExportedSubClass::kDFieldOffset, 8);
  TaggedField<Smi>::store(*object, ExportedSubClass::kEFieldOffset,
                          Smi::FromInt(9));
  return object;
}

DirectHandle<AbstractInternalClassSubclass1>
CreateTorqueAbstractInternalClassSubclass1ForTest(Isolate* isolate) {
  return AllocateTorqueObject<AbstractInternalClassSubclass1>(
      isolate, RootIndex::kAbstractInternalClassSubclass1Map);
}

DirectHandle<AbstractInternalClassSubclass2>
CreateTorqueAbstractInternalClassSubclass2ForTest(Isolate* isolate) {
  return AllocateTorqueObject<AbstractInternalClassSubclass2>(
      isolate, RootIndex::kAbstractInternalClassSubclass2Map);
}

DirectHandle<InternalClassWithStructElements>
CreateTorqueInternalClassWithStructElementsForTest(Isolate* isolate) {
  ReadOnlyRoots roots(isolate);
  constexpr int kEntryCount = 1;
  constexpr int kObjectSize =
      InternalClassWithStructElements::SizeFor(kEntryCount);
  Handle<ByteArray> donor =
      isolate->factory()->NewByteArray(ByteArray::LengthFor(kObjectSize),
                                       AllocationType::kOld);
  DirectHandle<Map> map(
      Cast<Map>(roots.object_at(RootIndex::kInternalClassWithStructElementsMap)),
      isolate);
  donor->set_map_after_allocation(isolate, *map, SKIP_WRITE_BARRIER);
  DirectHandle<InternalClassWithStructElements> object =
      direct_handle(Cast<InternalClassWithStructElements>(*donor), isolate);
  object->template WriteField<int32_t>(
      InternalClassWithStructElements::kDummy1Offset, 10);
  object->template WriteField<int32_t>(
      InternalClassWithStructElements::kDummy2Offset, 11);
  TaggedField<Smi>::store(*object, InternalClassWithStructElements::kCountOffset,
                          Smi::FromInt(kEntryCount));
  TaggedField<Smi>::store(*object, InternalClassWithStructElements::kDataOffset,
                          Smi::FromInt(12));
  TaggedField<Object>::store(*object,
                             InternalClassWithStructElements::kObjectOffset,
                             roots.null_value());
  const int entries_offset = InternalClassWithStructElements::kEntriesOffset;
  const int more_entries_offset = entries_offset + kEntryCount * kTaggedSize;
  TaggedField<Smi>::store(*object, entries_offset, Smi::FromInt(13));
  TaggedField<Smi>::store(*object, more_entries_offset, Smi::FromInt(14));
  TaggedField<Smi>::store(*object, more_entries_offset + kTaggedSize,
                          Smi::FromInt(15));
  return object;
}

DirectHandle<ExportedSubClass2> CreateTorqueExportedSubClass2ForTest(
    Isolate* isolate) {
  ReadOnlyRoots roots(isolate);
  DirectHandle<ExportedSubClass2> object =
      AllocateTorqueObject<ExportedSubClass2>(isolate,
                                              RootIndex::kExportedSubClass2Map);
  TaggedField<Object>::store(*object, ExportedSubClass2::kAOffset,
                             roots.null_value());
  TaggedField<Object>::store(*object, ExportedSubClass2::kBOffset,
                             roots.null_value());
  object->template WriteField<int32_t>(ExportedSubClass2::kXFieldOffset, 3);
  object->template WriteField<int32_t>(ExportedSubClass2::kYFieldOffset, 4);
  TaggedField<Smi>::store(*object, ExportedSubClass2::kZFieldOffset,
                          Smi::FromInt(5));
  return object;
}

DirectHandle<SortState> CreateTorqueSortStateForTest(Isolate* isolate) {
  ReadOnlyRoots roots(isolate);
  DirectHandle<SortState> object =
      AllocateTorqueObject<SortState>(isolate, RootIndex::kSortStateMap);
  DirectHandle<JSObject> receiver =
      isolate->factory()->NewJSObject(isolate->object_function(),
                                      AllocationType::kOld);
  DirectHandle<FixedArray> empty = isolate->factory()->empty_fixed_array();
  TaggedField<JSReceiver>::store(*object, SortState::kReceiverOffset, *receiver);
  TaggedField<Map>::store(*object, SortState::kInitialReceiverMapOffset,
                          receiver->map());
  TaggedField<Number>::store(*object, SortState::kInitialReceiverLengthOffset,
                             Smi::FromInt(0));
  TaggedField<Object>::store(*object, SortState::kUserCmpFnOffset,
                             roots.undefined_value());
  TaggedField<Boolean>::store(*object, SortState::kIsResetToGenericOffset,
                              roots.false_value());
  TaggedField<Smi>::store(*object, SortState::kMinGallopOffset,
                          Smi::FromInt(7));
  TaggedField<Smi>::store(*object, SortState::kPendingRunsSizeOffset,
                          Smi::FromInt(0));
  TaggedField<FixedArray>::store(*object, SortState::kPendingRunsOffset,
                                 *empty);
  TaggedField<FixedArray>::store(*object, SortState::kWorkArrayOffset, *empty);
  TaggedField<FixedArray>::store(*object, SortState::kTempArrayOffset, *empty);
  TaggedField<Smi>::store(*object, SortState::kSortLengthOffset,
                          Smi::FromInt(0));
  TaggedField<Smi>::store(*object, SortState::kNumberOfUndefinedOffset,
                          Smi::FromInt(0));
  return object;
}

struct VisitorIdExpectation {
  VisitorId id;
  const char* name;
};

struct TorqueObjectDebugState {
  int internal_struct_instance_size = 0;
  int internal_struct_allocated_size = 0;
  int internal_struct_visitor_id = -1;
};

constexpr VisitorIdExpectation kTorqueStableVisitorExpectations[] = {
    {VisitorId::kVisitInternalClass, "InternalClass"},
    {VisitorId::kVisitSmiPair, "SmiPair"},
    {VisitorId::kVisitSmiBox, "SmiBox"},
    {VisitorId::kVisitExportedSubClassBase, "ExportedSubClassBase"},
    {VisitorId::kVisitExportedSubClass, "ExportedSubClass"},
    {VisitorId::kVisitAbstractInternalClassSubclass1,
     "AbstractInternalClassSubclass1"},
    {VisitorId::kVisitAbstractInternalClassSubclass2,
     "AbstractInternalClassSubclass2"},
    {VisitorId::kVisitInternalClassWithStructElements,
     "InternalClassWithStructElements"},
    {VisitorId::kVisitExportedSubClass2, "ExportedSubClass2"},
    {VisitorId::kVisitSortState, "SortState"},
};

constexpr VisitorIdExpectation kTorqueReportExpectations[] = {
    {VisitorId::kVisitInternalClass, "InternalClass"},
    {VisitorId::kVisitSmiPair, "SmiPair"},
    {VisitorId::kVisitSmiBox, "SmiBox"},
    {VisitorId::kVisitExportedSubClassBase, "ExportedSubClassBase"},
    {VisitorId::kVisitExportedSubClass, "ExportedSubClass"},
    {VisitorId::kVisitAbstractInternalClassSubclass1,
     "AbstractInternalClassSubclass1"},
    {VisitorId::kVisitAbstractInternalClassSubclass2,
     "AbstractInternalClassSubclass2"},
    {VisitorId::kVisitInternalClassWithStructElements,
     "InternalClassWithStructElements"},
    {VisitorId::kVisitExportedSubClass2, "ExportedSubClass2"},
    {VisitorId::kVisitSortState, "SortState"},
};

std::string MissingVisitorIdsMessage(
    const VisitorIdCounts& counts,
    const VisitorIdExpectation* expectations,
    size_t expectation_count) {
  std::ostringstream out;
  bool first = true;
  for (size_t i = 0; i < expectation_count; ++i) {
    const VisitorIdExpectation& item = expectations[i];
    if (counts.find(item.id) != counts.end()) continue;
    if (!first) out << ", ";
    out << item.name;
    first = false;
  }
  return out.str();
}

void CheckExactInternalMapEdgeNoWrongKind(SnapshotGenerator* generator,
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

void CheckTranslatedTorqueObjectMapEdge(SnapshotGenerator* generator,
                                       Address object_address,
                                       Address map_address) {
  Node* object = GetSnapshotNode(generator, object_address);
  Node* map = GetSnapshotNode(generator, map_address);
  CHECK(object != nullptr);
  CHECK(map != nullptr);
  CheckExactInternalMapEdgeNoWrongKind(generator, object, map);
}

std::unique_ptr<SnapshotGenerator> CreateTorqueCoverageSnapshot(
    VisitorIdCounts* counts_out, TorqueObjectDebugState* debug_out) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  LocalContext env(CcTest::isolate());
  USE(env);

  std::vector<DirectHandle<Object>> keep_alive;
  DirectHandle<InternalClass> internal_class =
      CreateTorqueInternalClassForTest(isolate);
  DirectHandle<SmiPair> smi_pair = CreateTorqueSmiPairForTest(isolate);
  DirectHandle<ExportedSubClassBase> exported_subclass_base =
      CreateTorqueExportedSubClassBaseForTest(isolate);
  DirectHandle<ExportedSubClass> exported_subclass =
      CreateTorqueExportedSubClassForTest(isolate);
  DirectHandle<SmiBox> smi_box = CreateTorqueSmiBoxForTest(isolate);
  DirectHandle<AbstractInternalClassSubclass1> subclass1 =
      CreateTorqueAbstractInternalClassSubclass1ForTest(isolate);
  DirectHandle<AbstractInternalClassSubclass2> subclass2 =
      CreateTorqueAbstractInternalClassSubclass2ForTest(isolate);
  DirectHandle<InternalClassWithStructElements> struct_elements =
      CreateTorqueInternalClassWithStructElementsForTest(isolate);
  DirectHandle<ExportedSubClass2> exported_subclass2 =
      CreateTorqueExportedSubClass2ForTest(isolate);
  DirectHandle<SortState> sort_state = CreateTorqueSortStateForTest(isolate);
  KeepAlive(&keep_alive, internal_class);
  KeepAlive(&keep_alive, smi_pair);
  KeepAlive(&keep_alive, exported_subclass_base);
  KeepAlive(&keep_alive, exported_subclass);
  KeepAlive(&keep_alive, smi_box);
  KeepAlive(&keep_alive, subclass1);
  KeepAlive(&keep_alive, subclass2);
  KeepAlive(&keep_alive, struct_elements);
  KeepAlive(&keep_alive, exported_subclass2);
  KeepAlive(&keep_alive, sort_state);

  debug_out->internal_struct_instance_size = struct_elements->map()->instance_size();
  debug_out->internal_struct_allocated_size = struct_elements->AllocatedSize();
  debug_out->internal_struct_visitor_id =
      static_cast<int>(struct_elements->map()->visitor_id());

  USE(keep_alive);
  InvokeAtomicMajorGC(isolate->heap());
  *counts_out = CollectVisitorIdCounts(isolate);
  return TranslateHeapBytes(DumpHeapBytes(isolate));
}

}  // namespace

UNINITIALIZED_TEST(HeapDumpTorqueInternalClassSmiBoxSmiPairExactMapEdge) {
  Address internal_class_address = 0;
  Address internal_class_map_address = 0;
  Address exported_subclass_base_address = 0;
  Address exported_subclass_base_map_address = 0;
  Address exported_subclass_address = 0;
  Address exported_subclass_map_address = 0;
  Address abstract_internal_class_subclass1_address = 0;
  Address abstract_internal_class_subclass1_map_address = 0;
  Address abstract_internal_class_subclass2_address = 0;
  Address abstract_internal_class_subclass2_map_address = 0;
  Address internal_class_with_struct_elements_address = 0;
  Address internal_class_with_struct_elements_map_address = 0;
  Address exported_subclass2_address = 0;
  Address exported_subclass2_map_address = 0;
  Address smi_box_address = 0;
  Address smi_box_map_address = 0;
  Address smi_pair_address = 0;
  Address smi_pair_map_address = 0;
  Address sort_state_address = 0;
  Address sort_state_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            USE(context);
            DumpArtifacts artifacts;

            DirectHandle<InternalClass> internal_class =
                CreateTorqueInternalClassForTest(isolate);
            DirectHandle<ExportedSubClassBase> exported_subclass_base =
                CreateTorqueExportedSubClassBaseForTest(isolate);
            DirectHandle<ExportedSubClass> exported_subclass =
                CreateTorqueExportedSubClassForTest(isolate);
            DirectHandle<AbstractInternalClassSubclass1> abstract_internal_class_subclass1 =
                CreateTorqueAbstractInternalClassSubclass1ForTest(isolate);
            DirectHandle<AbstractInternalClassSubclass2> abstract_internal_class_subclass2 =
                CreateTorqueAbstractInternalClassSubclass2ForTest(isolate);
            DirectHandle<InternalClassWithStructElements>
                internal_class_with_struct_elements =
                    CreateTorqueInternalClassWithStructElementsForTest(isolate);
            DirectHandle<SmiBox> smi_box = CreateTorqueSmiBoxForTest(isolate);
            DirectHandle<SmiPair> smi_pair = CreateTorqueSmiPairForTest(isolate);
            DirectHandle<ExportedSubClass2> exported_subclass2 =
                CreateTorqueExportedSubClass2ForTest(isolate);
            DirectHandle<SortState> sort_state =
                CreateTorqueSortStateForTest(isolate);

            CHECK_EQ(INTERNAL_CLASS_TYPE, internal_class->map()->instance_type());
            CHECK_EQ(EXPORTED_SUB_CLASS_BASE_TYPE,
                     exported_subclass_base->map()->instance_type());
            CHECK_EQ(EXPORTED_SUB_CLASS_TYPE,
                     exported_subclass->map()->instance_type());
            CHECK_EQ(ABSTRACT_INTERNAL_CLASS_SUBCLASS1_TYPE,
                     abstract_internal_class_subclass1->map()->instance_type());
            CHECK_EQ(ABSTRACT_INTERNAL_CLASS_SUBCLASS2_TYPE,
                     abstract_internal_class_subclass2->map()->instance_type());
            CHECK_EQ(INTERNAL_CLASS_WITH_STRUCT_ELEMENTS_TYPE,
                     internal_class_with_struct_elements->map()->instance_type());
            CHECK_EQ(EXPORTED_SUB_CLASS2_TYPE,
                     exported_subclass2->map()->instance_type());
            CHECK_EQ(SMI_BOX_TYPE, smi_box->map()->instance_type());
            CHECK_EQ(SMI_PAIR_TYPE, smi_pair->map()->instance_type());
            CHECK_EQ(SORT_STATE_TYPE, sort_state->map()->instance_type());

            internal_class_address = internal_class->address();
            internal_class_map_address = internal_class->map()->address();
            exported_subclass_base_address = exported_subclass_base->address();
            exported_subclass_base_map_address =
                exported_subclass_base->map()->address();
            exported_subclass_address = exported_subclass->address();
            exported_subclass_map_address = exported_subclass->map()->address();
            abstract_internal_class_subclass1_address =
                abstract_internal_class_subclass1->address();
            abstract_internal_class_subclass1_map_address =
                abstract_internal_class_subclass1->map()->address();
            abstract_internal_class_subclass2_address =
                abstract_internal_class_subclass2->address();
            abstract_internal_class_subclass2_map_address =
                abstract_internal_class_subclass2->map()->address();
            internal_class_with_struct_elements_address =
                internal_class_with_struct_elements->address();
            internal_class_with_struct_elements_map_address =
                internal_class_with_struct_elements->map()->address();
            exported_subclass2_address = exported_subclass2->address();
            exported_subclass2_map_address = exported_subclass2->map()->address();
            smi_box_address = smi_box->address();
            smi_box_map_address = smi_box->map()->address();
            smi_pair_address = smi_pair->address();
            smi_pair_map_address = smi_pair->map()->address();
            sort_state_address = sort_state->address();
            sort_state_map_address = sort_state->map()->address();

            artifacts.keep_alive.push_back(internal_class);
            artifacts.keep_alive.push_back(exported_subclass_base);
            artifacts.keep_alive.push_back(exported_subclass);
            artifacts.keep_alive.push_back(abstract_internal_class_subclass1);
            artifacts.keep_alive.push_back(abstract_internal_class_subclass2);
            artifacts.keep_alive.push_back(internal_class_with_struct_elements);
            artifacts.keep_alive.push_back(exported_subclass2);
            artifacts.keep_alive.push_back(smi_box);
            artifacts.keep_alive.push_back(smi_pair);
            artifacts.keep_alive.push_back(sort_state);
            InvokeAtomicMajorGC(isolate->heap());
            return artifacts;
          });

  CheckTranslatedTorqueObjectMapEdge(generator.get(), internal_class_address,
                                    internal_class_map_address);
  CheckTranslatedTorqueObjectMapEdge(
      generator.get(), exported_subclass_base_address,
      exported_subclass_base_map_address);
  CheckTranslatedTorqueObjectMapEdge(generator.get(), exported_subclass_address,
                                    exported_subclass_map_address);
  CheckTranslatedTorqueObjectMapEdge(
      generator.get(), abstract_internal_class_subclass1_address,
      abstract_internal_class_subclass1_map_address);
  CheckTranslatedTorqueObjectMapEdge(
      generator.get(), abstract_internal_class_subclass2_address,
      abstract_internal_class_subclass2_map_address);
  CheckTranslatedTorqueObjectMapEdge(
      generator.get(), internal_class_with_struct_elements_address,
      internal_class_with_struct_elements_map_address);
  CheckTranslatedTorqueObjectMapEdge(generator.get(), exported_subclass2_address,
                                    exported_subclass2_map_address);
  CheckTranslatedTorqueObjectMapEdge(generator.get(), smi_box_address,
                                    smi_box_map_address);
  CheckTranslatedTorqueObjectMapEdge(generator.get(), smi_pair_address,
                                    smi_pair_map_address);
  CheckTranslatedTorqueObjectMapEdge(generator.get(), sort_state_address,
                                    sort_state_map_address);

  MarkCoveredInstanceType(INTERNAL_CLASS_TYPE);
  MarkCoveredInstanceType(EXPORTED_SUB_CLASS_BASE_TYPE);
  MarkCoveredInstanceType(EXPORTED_SUB_CLASS_TYPE);
  MarkCoveredInstanceType(ABSTRACT_INTERNAL_CLASS_SUBCLASS1_TYPE);
  MarkCoveredInstanceType(ABSTRACT_INTERNAL_CLASS_SUBCLASS2_TYPE);
  MarkCoveredInstanceType(INTERNAL_CLASS_WITH_STRUCT_ELEMENTS_TYPE);
  MarkCoveredInstanceType(EXPORTED_SUB_CLASS2_TYPE);
  MarkCoveredInstanceType(SMI_BOX_TYPE);
  MarkCoveredInstanceType(SMI_PAIR_TYPE);
  MarkCoveredInstanceType(SORT_STATE_TYPE);
}

TEST(HeapDumpVisitorIdsTorqueCoverage) {
  VisitorIdCounts counts;
  TorqueObjectDebugState debug_state;
  std::unique_ptr<SnapshotGenerator> generator =
      CreateTorqueCoverageSnapshot(&counts, &debug_state);

  CHECK_GT(generator->nodes().size(), 0u);
  std::string missing =
      MissingVisitorIdsMessage(counts, kTorqueStableVisitorExpectations,
                               arraysize(kTorqueStableVisitorExpectations));
  if (!missing.empty()) {
    std::cout << "HeapDump missing torque visitor ids: " << missing
              << std::endl;
  }
  CHECK(missing.empty());
}

TEST(HeapDumpVisitorIdsTorqueCoverageReport) {
  VisitorIdCounts counts;
  TorqueObjectDebugState debug_state;
  std::unique_ptr<SnapshotGenerator> generator =
      CreateTorqueCoverageSnapshot(&counts, &debug_state);

  USE(generator);
  std::string missing =
      MissingVisitorIdsMessage(counts, kTorqueReportExpectations,
                               arraysize(kTorqueReportExpectations));
  std::cout << "HeapDump torque visitor-id coverage missing: "
            << (missing.empty() ? "<none>" : missing) << std::endl;
  std::cout << "HeapDump internal struct debug: instance_size="
            << debug_state.internal_struct_instance_size
            << ", allocated_size=" << debug_state.internal_struct_allocated_size
            << ", visitor_id=" << debug_state.internal_struct_visitor_id
            << std::endl;
}

#else

TEST(HeapDumpVisitorIdsTorqueCoverage) {}
TEST(HeapDumpVisitorIdsTorqueCoverageReport) {}

#endif

}  // namespace heap
}  // namespace internal
}  // namespace v8
