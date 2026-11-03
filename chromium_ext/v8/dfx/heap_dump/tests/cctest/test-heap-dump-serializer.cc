#include "arkweb/chromium_ext/v8/dfx/heap_dump/tests/cctest/heap-dump-test-utils.h"

#include <cerrno>
#include <cstdio>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

namespace v8 {
namespace internal {
namespace heap {

#if defined(OH_ENABLE_HEAP_DUMP_TEST) && defined(OH_ENABLE_HEAP_DUMP) && \
    defined(OH_ENABLE_HEAP_TRANSLATE)

namespace {

using namespace heap_dump_test;

std::string TempPathTemplate(const char* prefix) {
  if (mkdir("tmp", 0755) != 0) {
    CHECK_EQ(EEXIST, errno);
  }
  return std::string("tmp/") + prefix + "-XXXXXX";
}

}  // namespace

UNINITIALIZED_TEST(HeapDumpSerializerProducesExpectedHeapSnapshotSkeleton) {
  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [](Isolate* isolate, v8::Local<v8::Context> context) {
            CreateBaselineObjects(context);
            CreateExtendedObjects(context);
            InvokeAtomicMajorGC(isolate->heap());
            return DumpArtifacts{};
          });

  dfx::SnapshotJSONSerializer serializer(generator.get());
  // Use a unique temp file path to avoid collisions in parallel test runs.
  std::string output_path_template =
      TempPathTemplate("heap-dump-cctest-snapshot");
  int fd = mkstemp(output_path_template.data());
  CHECK_NE(-1, fd);
  CHECK_EQ(0, close(fd));
  const std::string output_path = output_path_template;
  serializer.SetOutputFile(output_path);
  serializer.Serialize();

  std::string snapshot = ReadTextFile(output_path);
  CHECK_NE(std::string::npos, snapshot.find("\"snapshot\""));
  CHECK_NE(std::string::npos, snapshot.find("\"nodes\""));
  CHECK_NE(std::string::npos, snapshot.find("\"edges\""));
  CHECK_NE(std::string::npos, snapshot.find("\"strings\""));
  CHECK_NE(std::string::npos, snapshot.find("\"node_count\""));
  CHECK_NE(std::string::npos, snapshot.find("\"edge_count\""));
  CHECK_NE(std::string::npos, snapshot.find("native_bind"));
  CHECK_NE(std::string::npos, snapshot.find("bigint"));
  CHECK_EQ(0, std::remove(output_path.c_str()));
}

#else

UNINITIALIZED_TEST(HeapDumpSerializerProducesExpectedHeapSnapshotSkeleton) {}

#endif

}  // namespace heap
}  // namespace internal
}  // namespace v8
