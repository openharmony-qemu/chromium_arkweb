#include "arkweb/chromium_ext/v8/dfx/heap_dump/tests/cctest/heap-dump-test-utils.h"

#include <sstream>

#include "include/v8-script.h"
#include "src/flags/flags.h"

namespace v8 {
namespace internal {
namespace heap {

#if defined(OH_ENABLE_HEAP_DUMP_TEST) && defined(OH_ENABLE_HEAP_DUMP) && \
    defined(OH_ENABLE_HEAP_TRANSLATE) && V8_CAN_CREATE_SHARED_HEAP_BOOL && \
    !COMPRESS_POINTERS_IN_MULTIPLE_CAGES_BOOL

namespace {

using namespace heap_dump_test;

v8::Isolate::CreateParams CreateHeapDumpTestParams() {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  return create_params;
}

std::unique_ptr<SnapshotGenerator> CreateSharedVisitorCoverageSnapshot(
    VisitorIdCounts* counts_out) {
  i::v8_flags.shared_string_table = true;
  i::v8_flags.harmony_struct = true;
  i::FlagList::EnforceFlagImplications();

  v8::Isolate::CreateParams create_params = CreateHeapDumpTestParams();
  v8::Isolate* v8_isolate = v8::Isolate::New(create_params);
  Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);

  std::vector<uint8_t> bytes;
  {
    v8::Isolate::Scope isolate_scope(v8_isolate);
    v8::HandleScope handle_scope(v8_isolate);
    LocalContext env(v8_isolate);
    RunScript(env.local(), R"(
      globalThis.__heap_dump_shared_mutex = new Atomics.Mutex();
      globalThis.__heap_dump_shared_condition = new Atomics.Condition();
    )");
    InvokeAtomicMajorGC(isolate->heap());
    *counts_out = CollectVisitorIdCounts(isolate);
    bytes = DumpHeapBytes(isolate);
  }

  v8_isolate->Dispose();
  return TranslateHeapBytes(bytes);
}

std::string MissingVisitorIdsMessage(const VisitorIdCounts& counts) {
  return counts.find(VisitorId::kVisitJSSynchronizationPrimitive) ==
                 counts.end()
             ? "JSSynchronizationPrimitive"
             : std::string();
}

}  // namespace

UNINITIALIZED_TEST(HeapDumpVisitorIdsSharedCoverage) {
  VisitorIdCounts counts;
  std::unique_ptr<SnapshotGenerator> generator =
      CreateSharedVisitorCoverageSnapshot(&counts);
  CHECK_GT(generator->nodes().size(), 0u);

  std::string missing = MissingVisitorIdsMessage(counts);
  if (!missing.empty()) {
    std::cout << "HeapDump missing shared visitor ids: " << missing
              << std::endl;
  }
  CHECK(missing.empty());
}

UNINITIALIZED_TEST(HeapDumpVisitorIdsSharedCoverageReport) {
  VisitorIdCounts counts;
  std::unique_ptr<SnapshotGenerator> generator =
      CreateSharedVisitorCoverageSnapshot(&counts);
  USE(generator);
  std::string missing = MissingVisitorIdsMessage(counts);
  std::cout << "HeapDump shared visitor-id coverage missing: "
            << (missing.empty() ? "<none>" : missing) << std::endl;
}

#else

UNINITIALIZED_TEST(HeapDumpVisitorIdsSharedCoverage) {}
UNINITIALIZED_TEST(HeapDumpVisitorIdsSharedCoverageReport) {}

#endif

}  // namespace heap
}  // namespace internal
}  // namespace v8
