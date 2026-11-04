#include <cstddef>
#include <cstring>
#include <limits>
#include <string>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/tests/cctest/heap-dump-test-utils.h"

#if defined(OH_ENABLE_HEAP_TRANSLATE)
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-header.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-objects.h"
#endif
#include "src/common/ptr-compr-inl.h"
#include "src/objects/bytecode-array-inl.h"
#include "src/objects/map-inl.h"
#include "test/common/flag-utils.h"

namespace v8 {
namespace internal {
namespace heap {

#if defined(OH_ENABLE_HEAP_DUMP_TEST) && defined(OH_ENABLE_HEAP_DUMP)

namespace {

using namespace heap_dump_test;

#if defined(OH_ENABLE_HEAP_TRANSLATE)

constexpr uint32_t kRawHeapFormatVersionV1ForTest = 1;
constexpr uint32_t kRawHeapFormatVersionV2ForTest = 2;

struct RawHeapHeaderV1ForTest {
  uint64_t magic_;
  uint32_t dump_format_version_;
  uint32_t flags_;
  uint64_t timestamp_;

  uint32_t v8_major_version_;
  uint32_t v8_minor_version_;
  uint32_t v8_build_number_;
  uint32_t v8_patch_level_;

  Address cage_base_;
  Address code_cage_base_;

  uint32_t root_table_offset_;
  uint32_t root_table_size_;
  uint32_t root_type_count_;

  uint32_t string_table_offset_;
  uint32_t string_table_size_;
  uint32_t string_count_;

  uint32_t object_table_offset_;
  uint32_t object_table_size_;
  uint32_t object_count_;

  uint32_t metadata_offset_;
  uint32_t metadata_size_;
};

struct RawHeapHeaderV2ForTest {
  uint64_t magic_;
  uint32_t dump_format_version_;
  uint32_t flags_;
  uint64_t timestamp_;

  uint32_t v8_major_version_;
  uint32_t v8_minor_version_;
  uint32_t v8_build_number_;
  uint32_t v8_patch_level_;

  Address cage_base_;
  Address code_cage_base_;

  uint32_t root_table_offset_;
  uint32_t root_table_size_;
  uint32_t root_type_count_;

  uint32_t string_table_offset_;
  uint32_t string_table_size_;
  uint32_t string_count_;

  uint32_t object_table_offset_;
  uint32_t object_table_size_;
  uint32_t object_count_;

  uint32_t offheap_table_offset_;
  uint32_t offheap_table_size_;
  uint32_t offheap_table_count_;

  uint32_t metadata_offset_;
  uint32_t metadata_size_;
};

static_assert(sizeof(RawHeapHeaderV1ForTest) < sizeof(RawHeapHeader));
static_assert(sizeof(RawHeapHeaderV2ForTest) < sizeof(RawHeapHeader));
bool HasDiagnostic(const TranslateDiagnostics& diagnostics,
                   TranslateDiagnostics::Severity severity, const char* code,
                   const char* section) {
  for (const TranslateDiagnostics::Entry& entry : diagnostics.entries()) {
    if (entry.severity == severity && entry.code == code &&
        entry.section == section) {
      return true;
    }
  }
  return false;
}

bool HasDiagnosticMessageContaining(const TranslateDiagnostics& diagnostics,
                                    TranslateDiagnostics::Severity severity,
                                    const char* code, const char* section,
                                    const char* text) {
  for (const TranslateDiagnostics::Entry& entry : diagnostics.entries()) {
    if (entry.severity == severity && entry.code == code &&
        entry.section == section &&
        entry.message.find(text) != std::string::npos) {
      return true;
    }
  }
  return false;
}

class SparseObjectTableReader final : public dfx::BinaryReaderBase {
 public:
  SparseObjectTableReader(uint8_t* data, uint32_t data_size,
                          uint32_t binary_size)
      : data_(data), data_size_(data_size), binary_size_(binary_size) {}

  bool ReadData(uint32_t read_size, uint8_t* out, uint32_t out_size) override {
    if (read_size > out_size || cursor_ > data_size_ ||
        read_size > data_size_ - cursor_) {
      return false;
    }
    memcpy(out, data_ + cursor_, read_size);
    cursor_ += read_size;
    return true;
  }

  bool ReadDataAt(uint32_t offset, uint32_t read_size, uint8_t* out,
                  uint32_t out_size) override {
    if (read_size > out_size || offset > data_size_ ||
        read_size > data_size_ - offset) {
      return false;
    }
    memcpy(out, data_ + offset, read_size);
    return true;
  }

  uint32_t BinarySize() const override { return binary_size_; }
  uint32_t CurrentPosition() override { return cursor_; }
  void SetPosition(uint32_t pos) override {
    if (pos <= binary_size_) {
      cursor_ = pos;
    }
  }

 private:
  uint8_t* data_;
  uint32_t data_size_;
  uint32_t binary_size_;
  uint32_t cursor_{0};
};

Tagged_t EncodeTaggedHeapObjectForRawHeapFieldForDumpCategoryTest(
    const RawHeapHeader& header, Address address) {
  Address tagged = address + dfx::RawHeapFlagHeapObjectTag(header.flags_);
  if (dfx::RawHeapFlagCompressedPointers(header.flags_)) {
    CHECK_GE(tagged, header.cage_base_);
    tagged -= header.cage_base_;
  }
  CHECK_LE(tagged, static_cast<Address>(std::numeric_limits<Tagged_t>::max()));
  return static_cast<Tagged_t>(tagged);
}

int CountDumpCategoryEdgesFromNodeToTarget(const Node* source, Edge::Type type,
                                           const Node* target) {
  CHECK_NOT_NULL(source);
  int count = 0;
  for (const Edge* edge : source->childs_) {
    if (edge->type_ == static_cast<uint32_t>(type) &&
        edge->to_node_ == target) {
      ++count;
    }
  }
  return count;
}

template <typename T>
DirectHandle<T> GetGlobalDirectHandle(Isolate* isolate,
                                      v8::Local<v8::Context> context,
                                      const char* key) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8::Local<v8::Value> value = context->Global()
                                   ->Get(context, NewApiString(v8_isolate, key))
                                   .ToLocalChecked();
  return Cast<T>(v8::Utils::OpenDirectHandle(*value));
}

void CheckMapOnlyObjectHasExactInternalMapEdge(SnapshotGenerator* generator,
                                               const Node* object,
                                               const Node* map) {
  // Breadcrumb: V8HeapExplorer::IterateAndExtractReferences calls
  // ExtractReferences(entry, obj), then SetInternalReference(entry, "map",
  // obj->map(...), HeapObject::kMapOffset).
  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CHECK(
      HasExactNamedEdge(generator, object, Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator, object, Edge::Type::kInternal,
                                   "map", map));
  CHECK_EQ(1, CountDumpCategoryEdgesFromNodeToTarget(
                  object, Edge::Type::kInternal, map));
  CHECK_EQ(0, CountDumpCategoryEdgesFromNodeToTarget(object, Edge::Type::kWeak,
                                                     map));
  CHECK_EQ(0, CountDumpCategoryEdgesFromNodeToTarget(object,
                                                     Edge::Type::kHidden, map));
  CHECK_EQ(0, CountDumpCategoryEdgesFromNodeToTarget(
                  object, Edge::Type::kProperty, map));
}

std::vector<uint8_t> DumpBaselineHeapBytes() {
  return DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        CreateBaselineObjects(context);
        InvokeAtomicMajorGC(isolate->heap());
        return DumpArtifacts{};
      });
}

#if defined(V8_ENABLE_SANDBOX)
struct BytecodeArrayProtectedPointerState {
  Address bytecode_array = 0;
  Address constant_pool = 0;
  Address handler_table = 0;
  bool saw_bytecode_array_type = false;
  bool saw_constant_pool_type = false;
  bool saw_handler_table_type = false;
};

std::vector<uint8_t> DumpBytecodeArrayProtectedPointerHeapBytes(
    BytecodeArrayProtectedPointerState* state) {
  return DumpWithIsolateTeardown([state](Isolate* isolate,
                                         v8::Local<v8::Context> context) {
    FlagScope<bool> allow_natives_syntax(&v8_flags.allow_natives_syntax, true);
    RunScript(context, R"(
          globalThis.__heap_dump_sandbox_v2_bytecode_fn =
              function HeapDumpSandboxProtectedPointerV2Edges(x) {
            const y = x + 1;
            return y * 2;
          };
          %PrepareFunctionForOptimization(
              globalThis.__heap_dump_sandbox_v2_bytecode_fn);
          globalThis.__heap_dump_sandbox_v2_bytecode_fn(20);
        )");

    DirectHandle<JSFunction> function = GetGlobalDirectHandle<JSFunction>(
        isolate, context, "__heap_dump_sandbox_v2_bytecode_fn");
    DirectHandle<SharedFunctionInfo> shared(function->shared(), isolate);
    CHECK(shared->HasBytecodeArray());
    DirectHandle<BytecodeArray> bytecode_array(
        shared->GetBytecodeArray(isolate), isolate);
    Tagged<TrustedFixedArray> constant_pool = bytecode_array->constant_pool();
    Tagged<TrustedByteArray> handler_table = bytecode_array->handler_table();

    CHECK_EQ(BYTECODE_ARRAY_TYPE, bytecode_array->map()->instance_type());
    CHECK_EQ(TRUSTED_FIXED_ARRAY_TYPE, constant_pool->map()->instance_type());
    CHECK_EQ(TRUSTED_BYTE_ARRAY_TYPE, handler_table->map()->instance_type());

    state->saw_bytecode_array_type = true;
    state->saw_constant_pool_type = true;
    state->saw_handler_table_type = true;
    state->bytecode_array = bytecode_array->address();
    state->constant_pool = constant_pool.address();
    state->handler_table = handler_table.address();

    DumpArtifacts artifacts;
    artifacts.keep_alive.push_back(function);
    artifacts.keep_alive.push_back(shared);
    artifacts.keep_alive.push_back(bytecode_array);
    return artifacts;
  });
}
#endif

void WriteRawHeapHeader(std::vector<uint8_t>* bytes,
                        const RawHeapHeader& header) {
  CHECK_GE(bytes->size(), sizeof(header));
  memcpy(bytes->data(), &header, sizeof(header));
}

void WriteRawHeapHeaderV1ForTest(std::vector<uint8_t>* bytes,
                                 const RawHeapHeaderV1ForTest& header) {
  CHECK_GE(bytes->size(), sizeof(header));
  memcpy(bytes->data(), &header, sizeof(header));
}

void WriteRawHeapHeaderV2ForTest(std::vector<uint8_t>* bytes,
                                 const RawHeapHeaderV2ForTest& header) {
  CHECK_GE(bytes->size(), sizeof(header));
  memcpy(bytes->data(), &header, sizeof(header));
}

template <typename T>
void WritePodAt(std::vector<uint8_t>* bytes, uint32_t offset, const T& value) {
  CHECK_LE(static_cast<size_t>(offset) + sizeof(value), bytes->size());
  memcpy(bytes->data() + offset, &value, sizeof(value));
}

uint32_t RebaseOffsetForRemovedHeaderBytes(uint32_t offset, uint32_t delta) {
  if (offset == 0) {
    return 0;
  }
  CHECK_GE(offset, delta);
  return offset - delta;
}

void CheckOffheapTableHeaderLayout(const std::vector<uint8_t>& bytes,
                                   const RawHeapHeader& header) {
  const uint32_t object_table_end =
      header.object_table_offset_ + header.object_table_size_;
  if (!dfx::HasRawHeapOffheapTableSection(header)) {
    CHECK_EQ(0u, header.offheap_table_size_);
    CHECK_EQ(0u, header.offheap_table_count_);
    if (header.offheap_table_offset_ != 0) {
      CHECK_EQ(object_table_end, header.offheap_table_offset_);
    }
    return;
  }

  // Offheap tables are not sandbox-only: current no-sandbox dumps may also
  // export shared process tables such as JSDispatchTable / Wasm metadata.
  CHECK_EQ(object_table_end, header.offheap_table_offset_);
  CHECK_GT(header.offheap_table_size_, 0u);
  CHECK_GT(header.offheap_table_count_, 0u);
  CHECK_LE(static_cast<size_t>(header.offheap_table_offset_) +
               header.offheap_table_size_,
           bytes.size());
  const uint64_t directory_size =
      static_cast<uint64_t>(header.offheap_table_count_) *
      sizeof(dfx::RawHeapOffheapTableHeader);
  CHECK_LE(directory_size, header.offheap_table_size_);
  if (header.metadata_offset_ != 0) {
    CHECK_LE(header.offheap_table_offset_ + header.offheap_table_size_,
             header.metadata_offset_);
  }
  for (uint32_t index = 0; index < header.offheap_table_count_; ++index) {
    const uint64_t entry_offset =
        static_cast<uint64_t>(header.offheap_table_offset_) +
        index * sizeof(dfx::RawHeapOffheapTableHeader);
    CHECK_LE(entry_offset + sizeof(dfx::RawHeapOffheapTableHeader),
             bytes.size());
    dfx::RawHeapOffheapTableHeader table =
        ReadPod<dfx::RawHeapOffheapTableHeader>(
            bytes, static_cast<uint32_t>(entry_offset));
    CHECK_LE(table.offset_, header.offheap_table_size_);
    CHECK_LE(table.size_, header.offheap_table_size_ - table.offset_);
  }
}

std::vector<uint8_t> RemoveRawHeapOffheapSection(
    const std::vector<uint8_t>& source_bytes) {
  std::vector<uint8_t> bytes = source_bytes;
  RawHeapHeader header = ReadRawHeapHeader(bytes);
  if (!dfx::HasRawHeapOffheapTableSection(header)) {
    return bytes;
  }

  CHECK_LE(static_cast<size_t>(header.offheap_table_offset_) +
               header.offheap_table_size_,
           bytes.size());
  bytes.erase(bytes.begin() + header.offheap_table_offset_,
              bytes.begin() + header.offheap_table_offset_ +
                  header.offheap_table_size_);
  if (header.metadata_offset_ != 0) {
    CHECK_GE(header.metadata_offset_,
             header.offheap_table_offset_ + header.offheap_table_size_);
    header.metadata_offset_ -= header.offheap_table_size_;
  }
  header.offheap_table_offset_ =
      header.object_table_offset_ + header.object_table_size_;
  header.offheap_table_size_ = 0;
  header.offheap_table_count_ = 0;
  WriteRawHeapHeader(&bytes, header);
  return bytes;
}

std::vector<uint8_t> ConvertCurrentRawHeapToLegacyV1(
    const std::vector<uint8_t>& current_bytes) {
  std::vector<uint8_t> bytes = RemoveRawHeapOffheapSection(current_bytes);
  RawHeapHeader header = ReadRawHeapHeader(bytes);
  CHECK_EQ(dfx::kDumpFormatVersionCurrent, header.dump_format_version_);
  CHECK(!dfx::HasRawHeapOffheapTableSection(header));

  constexpr uint32_t kHeaderSizeDelta = static_cast<uint32_t>(
      sizeof(RawHeapHeader) - sizeof(RawHeapHeaderV1ForTest));
  RawHeapHeaderV1ForTest legacy{};
  legacy.magic_ = header.magic_;
  legacy.dump_format_version_ = kRawHeapFormatVersionV1ForTest;
  legacy.flags_ = header.flags_;
  legacy.timestamp_ = header.timestamp_;
  legacy.v8_major_version_ = header.v8_major_version_;
  legacy.v8_minor_version_ = header.v8_minor_version_;
  legacy.v8_build_number_ = header.v8_build_number_;
  legacy.v8_patch_level_ = header.v8_patch_level_;
  legacy.cage_base_ = header.cage_base_;
  legacy.code_cage_base_ = header.code_cage_base_;
  legacy.root_table_offset_ = RebaseOffsetForRemovedHeaderBytes(
      header.root_table_offset_, kHeaderSizeDelta);
  legacy.root_table_size_ = header.root_table_size_;
  legacy.root_type_count_ = header.root_type_count_;
  legacy.string_table_offset_ = RebaseOffsetForRemovedHeaderBytes(
      header.string_table_offset_, kHeaderSizeDelta);
  legacy.string_table_size_ = header.string_table_size_;
  legacy.string_count_ = header.string_count_;
  legacy.object_table_offset_ = RebaseOffsetForRemovedHeaderBytes(
      header.object_table_offset_, kHeaderSizeDelta);
  legacy.object_table_size_ = header.object_table_size_;
  legacy.object_count_ = header.object_count_;
  legacy.metadata_offset_ = RebaseOffsetForRemovedHeaderBytes(
      header.metadata_offset_, kHeaderSizeDelta);
  legacy.metadata_size_ = header.metadata_size_;

  bytes.erase(bytes.begin() + sizeof(RawHeapHeaderV1ForTest),
              bytes.begin() + sizeof(RawHeapHeader));
  WriteRawHeapHeaderV1ForTest(&bytes, legacy);
  return bytes;
}

std::vector<uint8_t> ConvertCurrentRawHeapToLegacyV2(
    const std::vector<uint8_t>& current_bytes) {
  std::vector<uint8_t> bytes = current_bytes;
  RawHeapHeader header = ReadRawHeapHeader(bytes);
  CHECK_EQ(dfx::kDumpFormatVersionCurrent, header.dump_format_version_);

  constexpr uint32_t kHeaderSizeDelta = static_cast<uint32_t>(
      sizeof(RawHeapHeader) - sizeof(RawHeapHeaderV2ForTest));
  RawHeapHeaderV2ForTest legacy{};
  legacy.magic_ = header.magic_;
  legacy.dump_format_version_ = kRawHeapFormatVersionV2ForTest;
  legacy.flags_ = header.flags_;
  legacy.timestamp_ = header.timestamp_;
  legacy.v8_major_version_ = header.v8_major_version_;
  legacy.v8_minor_version_ = header.v8_minor_version_;
  legacy.v8_build_number_ = header.v8_build_number_;
  legacy.v8_patch_level_ = header.v8_patch_level_;
  legacy.cage_base_ = header.cage_base_;
  legacy.code_cage_base_ = header.code_cage_base_;
  legacy.root_table_offset_ = RebaseOffsetForRemovedHeaderBytes(
      header.root_table_offset_, kHeaderSizeDelta);
  legacy.root_table_size_ = header.root_table_size_;
  legacy.root_type_count_ = header.root_type_count_;
  legacy.string_table_offset_ = RebaseOffsetForRemovedHeaderBytes(
      header.string_table_offset_, kHeaderSizeDelta);
  legacy.string_table_size_ = header.string_table_size_;
  legacy.string_count_ = header.string_count_;
  legacy.object_table_offset_ = RebaseOffsetForRemovedHeaderBytes(
      header.object_table_offset_, kHeaderSizeDelta);
  legacy.object_table_size_ = header.object_table_size_;
  legacy.object_count_ = header.object_count_;
  legacy.offheap_table_offset_ = RebaseOffsetForRemovedHeaderBytes(
      header.offheap_table_offset_, kHeaderSizeDelta);
  legacy.offheap_table_size_ = header.offheap_table_size_;
  legacy.offheap_table_count_ = header.offheap_table_count_;
  legacy.metadata_offset_ = RebaseOffsetForRemovedHeaderBytes(
      header.metadata_offset_, kHeaderSizeDelta);
  legacy.metadata_size_ = header.metadata_size_;

  bytes.erase(bytes.begin() + sizeof(RawHeapHeaderV2ForTest),
              bytes.begin() + sizeof(RawHeapHeader));
  WriteRawHeapHeaderV2ForTest(&bytes, legacy);
  return bytes;
}

void InsertOffheapSection(std::vector<uint8_t>* bytes,
                          const std::vector<uint8_t>& section,
                          uint32_t table_count) {
  *bytes = RemoveRawHeapOffheapSection(*bytes);
  RawHeapHeader header = ReadRawHeapHeader(*bytes);
  CHECK(!dfx::HasRawHeapOffheapTableSection(header));
  const uint32_t insert_offset =
      header.object_table_offset_ + header.object_table_size_;
  CHECK_LE(insert_offset, bytes->size());
  CHECK_LE(section.size(),
           static_cast<size_t>(std::numeric_limits<uint32_t>::max()));
  bytes->insert(bytes->begin() + insert_offset, section.begin(), section.end());

  header.offheap_table_offset_ = insert_offset;
  header.offheap_table_size_ = static_cast<uint32_t>(section.size());
  header.offheap_table_count_ = table_count;
  if (header.metadata_offset_ != 0) {
    header.metadata_offset_ += static_cast<uint32_t>(section.size());
  }
  WriteRawHeapHeader(bytes, header);
}

std::vector<uint8_t> BuildOffheapDirectorySection(
    const std::vector<dfx::RawHeapOffheapTableHeader>& directory) {
  std::vector<uint8_t> section(directory.size() *
                               sizeof(dfx::RawHeapOffheapTableHeader));
  if (!directory.empty()) {
    memcpy(section.data(), directory.data(), section.size());
  }
  return section;
}

std::vector<uint8_t> PrepareOffheapTestBytesWithDefaultTable() {
  std::vector<uint8_t> bytes =
      RemoveRawHeapOffheapSection(DumpBaselineHeapBytes());
  RawHeapHeader header = ReadRawHeapHeader(bytes);
  CHECK(!dfx::HasRawHeapOffheapTableSection(header));

  dfx::RawHeapOffheapTableHeader table_header{};
  table_header.kind_ = static_cast<uint32_t>(
      dfx::RawHeapOffheapTableKind::kWasmCanonicalTypeTable);
  table_header.offset_ = sizeof(dfx::RawHeapOffheapTableHeader);
  table_header.size_ = sizeof(dfx::RawHeapWasmCanonicalStructRecord);
  table_header.count_ = 1;
  std::vector<uint8_t> section = BuildOffheapDirectorySection({table_header});
  InsertOffheapSection(&bytes, section, 1);
  return bytes;
}

void RemoveRawObjectRecord(std::vector<uint8_t>* bytes,
                           const RawObjectRecord& record) {
  const uint32_t record_offset = record.payload_offset -
                                 static_cast<uint32_t>(sizeof(Address)) -
                                 static_cast<uint32_t>(sizeof(uint32_t));
  const uint32_t record_size = static_cast<uint32_t>(sizeof(Address)) +
                               static_cast<uint32_t>(sizeof(uint32_t)) +
                               record.dump_size;
  const size_t erase_end = record_offset + record_size;
  CHECK_LE(erase_end, bytes->size());
  bytes->erase(bytes->begin() + record_offset, bytes->begin() + erase_end);

  RawHeapHeader header = ReadRawHeapHeader(*bytes);
  CHECK_GT(header.object_count_, 0u);
  CHECK_GE(header.object_table_size_, record_size);
  header.object_count_--;
  header.object_table_size_ -= record_size;
  if (header.offheap_table_offset_ != 0) {
    CHECK_GE(header.offheap_table_offset_, erase_end);
    header.offheap_table_offset_ -= record_size;
  }
  if (header.metadata_offset_ != 0) {
    CHECK_GE(header.metadata_offset_, erase_end);
    header.metadata_offset_ -= record_size;
  }
  WriteRawHeapHeader(bytes, header);
}

void GrowRawObjectPayload(std::vector<uint8_t>* bytes,
                          const RawObjectRecord& record, uint32_t new_dump_size) {
  CHECK_GT(new_dump_size, record.dump_size);
  const uint32_t delta = new_dump_size - record.dump_size;
  const uint32_t size_offset =
      record.payload_offset - static_cast<uint32_t>(sizeof(uint32_t));
  const uint32_t insert_offset = record.payload_offset + record.dump_size;
  CHECK_LE(insert_offset, bytes->size());
  bytes->insert(bytes->begin() + insert_offset, delta, 0);
  WritePodAt(bytes, size_offset, new_dump_size);

  RawHeapHeader header = ReadRawHeapHeader(*bytes);
  CHECK_LE(header.object_table_size_, std::numeric_limits<uint32_t>::max() -
                                         delta);
  header.object_table_size_ += delta;
  if (header.offheap_table_offset_ != 0) {
    CHECK_GE(header.offheap_table_offset_, insert_offset);
    header.offheap_table_offset_ += delta;
  }
  if (header.metadata_offset_ != 0) {
    CHECK_GE(header.metadata_offset_, insert_offset);
    header.metadata_offset_ += delta;
  }
  WriteRawHeapHeader(bytes, header);
}

void AppendInstructionStreamRelocRecord(std::vector<uint8_t>* bytes,
                                        const RawObjectRecord& record,
                                        Address target_address, bool is_weak) {
  constexpr uint32_t kRelocRecordSize =
      static_cast<uint32_t>(sizeof(Address) + sizeof(int8_t));
  const uint32_t size_offset =
      record.payload_offset - static_cast<uint32_t>(sizeof(uint32_t));
  const uint32_t insert_offset = record.payload_offset + record.dump_size;
  CHECK_LE(insert_offset, bytes->size());
  CHECK_LE(record.dump_size,
           std::numeric_limits<uint32_t>::max() - kRelocRecordSize);
  bytes->insert(bytes->begin() + insert_offset, kRelocRecordSize, 0);

  const uint32_t new_dump_size = record.dump_size + kRelocRecordSize;
  WritePodAt(bytes, size_offset, new_dump_size);

  Address encoded_target = target_address;
  if (is_weak) {
    encoded_target |= dfx::kRawHeapWeakObjectTag;
  }
  WritePodAt(bytes, insert_offset, encoded_target);
  const int8_t mode = static_cast<int8_t>(RelocInfo::FULL_EMBEDDED_OBJECT);
  WritePodAt(bytes, insert_offset + static_cast<uint32_t>(sizeof(Address)),
             mode);

  RawHeapHeader header = ReadRawHeapHeader(*bytes);
  CHECK_LE(header.object_table_size_,
           std::numeric_limits<uint32_t>::max() - kRelocRecordSize);
  header.object_table_size_ += kRelocRecordSize;
  if (header.offheap_table_offset_ != 0) {
    CHECK_GE(header.offheap_table_offset_, insert_offset);
    header.offheap_table_offset_ += kRelocRecordSize;
  }
  if (header.metadata_offset_ != 0) {
    CHECK_GE(header.metadata_offset_, insert_offset);
    header.metadata_offset_ += kRelocRecordSize;
  }
  WriteRawHeapHeader(bytes, header);
}

#endif  // defined(OH_ENABLE_HEAP_TRANSLATE)

}  // namespace

UNINITIALIZED_TEST(HeapDumpObjectCountSkipsFreeSpaceAndFiller) {
  ExpectedDumpObjectCounts expected_counts;
  Address filler_address = 0;
  Address free_space_address = 0;
  std::vector<uint8_t> bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        CreateBaselineObjects(context);
        InvokeAtomicMajorGC(isolate->heap());
        DirectHandle<HeapObject> filler =
            isolate->factory()->NewFillerObject(
                kTaggedSize, kTaggedAligned, AllocationType::kOld);
        DirectHandle<HeapObject> free_space =
            isolate->factory()->NewFillerObject(
                3 * kTaggedSize, kTaggedAligned, AllocationType::kOld);
        CHECK_EQ(FILLER_TYPE, filler->map()->instance_type());
        CHECK_EQ(FREE_SPACE_TYPE, free_space->map()->instance_type());
        filler_address = filler->address();
        free_space_address = free_space->address();
        expected_counts = CountExpectedDumpObjectStats(isolate);
        return DumpArtifacts{};
      });

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  CHECK_EQ(expected_counts.dumped, header.object_count_);
  for (const RawObjectRecord& record : CollectRawObjects(bytes)) {
    CHECK_NE(filler_address, record.address);
    CHECK_NE(free_space_address, record.address);
  }
  MarkCoveredInstanceType(FILLER_TYPE);
  MarkCoveredInstanceType(FREE_SPACE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpHeaderPrefixFieldsAreInitializedAndCompatible) {
  std::vector<uint8_t> bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        CreateBaselineObjects(context);
        InvokeAtomicMajorGC(isolate->heap());
        return DumpArtifacts{};
      });

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  CHECK_EQ(dfx::kRawHeapMagic, header.magic_);
  CHECK_EQ(dfx::kDumpFormatVersion, header.dump_format_version_);
  CHECK_EQ(dfx::kDumpFormatVersionCurrent, header.dump_format_version_);
  CHECK_EQ(sizeof(dfx::RawHeapHeader), header.root_table_offset_);
#if defined(V8_ENABLE_SANDBOX)
  CHECK_NE(static_cast<Address>(0), header.trusted_cage_base_);
#else
  CHECK_EQ(header.cage_base_, header.trusted_cage_base_);
#endif
  CheckOffheapTableHeaderLayout(bytes, header);
  CHECK_EQ(dfx::GetCurrentRawHeapFlags(), header.flags_);
  CHECK_NE(static_cast<uint64_t>(0), header.timestamp_);
  CHECK_EQ(dfx::kDumpFormatVersion, header.dump_format_version_);
  CHECK_EQ(header.offheap_table_offset_ + header.offheap_table_size_,
           header.metadata_offset_);
  CHECK_EQ(0u, header.metadata_size_);
  BinaryReaderTest reader(bytes.data(), static_cast<uint32_t>(bytes.size()));
  TranslateDiagnostics diagnostics;
  CHECK(dfx::ValidateRawHeapHeaderForTranslate(&reader, header, &diagnostics));
  CHECK(!diagnostics.has_errors());
}

#if defined(OH_ENABLE_HEAP_TRANSLATE)
UNINITIALIZED_TEST(HeapDumpRawHeapHeaderV3IncludesTrustedCageBase) {
  std::vector<uint8_t> bytes = DumpBaselineHeapBytes();
  RawHeapHeader header = ReadRawHeapHeader(bytes);

  CHECK_EQ(dfx::kDumpFormatVersionWithTrustedCageBase,
           header.dump_format_version_);
  CHECK_EQ(dfx::kDumpFormatVersionCurrent, header.dump_format_version_);
#if defined(V8_ENABLE_SANDBOX)
  CHECK_NE(static_cast<Address>(0), header.trusted_cage_base_);
  CHECK_EQ(header.trusted_cage_base_,
           i::TrustedSpaceCompressionScheme::GetPtrComprCageBaseAddress(
               header.trusted_cage_base_));
#else
  CHECK_EQ(header.cage_base_, header.trusted_cage_base_);
#endif
}

UNINITIALIZED_TEST(HeapDumpRawHeapHeaderTrustedCageBaseValidation) {
  std::vector<uint8_t> bytes = DumpBaselineHeapBytes();
  RawHeapHeader header = ReadRawHeapHeader(bytes);
#if defined(V8_ENABLE_SANDBOX)
  CHECK_NE(static_cast<Address>(0), header.trusted_cage_base_);
  header.trusted_cage_base_ += 1;
  WriteRawHeapHeader(&bytes, header);

  BinaryReaderTest reader(bytes.data(), static_cast<uint32_t>(bytes.size()));
  TranslateDiagnostics diagnostics;
  CHECK(!dfx::ValidateRawHeapHeaderForTranslate(&reader, header, &diagnostics));
  CHECK(diagnostics.has_fatal());
  CHECK(HasDiagnostic(diagnostics, TranslateDiagnostics::Severity::kFatal,
                      "trusted_cage_base_mismatch", "header"));
#else
  BinaryReaderTest reader(bytes.data(), static_cast<uint32_t>(bytes.size()));
  TranslateDiagnostics diagnostics;
  CHECK(dfx::ValidateRawHeapHeaderForTranslate(&reader, header, &diagnostics));
  CHECK(!diagnostics.has_errors());
#endif
}
#endif

UNINITIALIZED_TEST(HeapDumpTranslateDiagnosticsStoreBoundedSample) {
  dfx::TranslateDiagnostics diagnostics;
  constexpr size_t kIssueCount = 64;
  for (size_t i = 0; i < kIssueCount; ++i) {
    diagnostics.Unsupported("test_unsupported",
                            "synthetic unsupported diagnostic", "unit_test");
  }

  CHECK_EQ(kIssueCount, diagnostics.issue_count());
  CHECK_EQ(static_cast<size_t>(1), diagnostics.entries().size());
  CHECK(!diagnostics.has_errors());
}

UNINITIALIZED_TEST(HeapDumpTranslateDiagnosticsKeepDistinctBoundedSamples) {
  dfx::TranslateDiagnostics diagnostics;
  constexpr size_t kUnsupportedIssueCount = 64;
  for (size_t i = 0; i < kUnsupportedIssueCount; ++i) {
    diagnostics.Unsupported("repeated_unsupported",
                            "synthetic unsupported diagnostic", "object_body");
  }
  diagnostics.Unsupported("distinct_unsupported",
                          "distinct unsupported diagnostic", "object_body");
  diagnostics.CorruptInput("distinct_corrupt", "distinct corrupt diagnostic",
                           "object_body");

  CHECK_EQ(kUnsupportedIssueCount + 2, diagnostics.issue_count());
  CHECK_EQ(static_cast<size_t>(3), diagnostics.entries().size());
  CHECK(diagnostics.has_errors());
  CHECK(HasDiagnostic(diagnostics,
                      dfx::TranslateDiagnostics::Severity::kUnsupported,
                      "distinct_unsupported", "object_body"));
  CHECK(HasDiagnostic(diagnostics,
                      dfx::TranslateDiagnostics::Severity::kCorruptInput,
                      "distinct_corrupt", "object_body"));
}

UNINITIALIZED_TEST(HeapDumpRawObjectCategoriesAreEncodedAsExpected) {
  Address heap_number_address = 0;
  uint32_t heap_number_size = 0;
  Address object_address = 0;
  uint32_t object_size = 0;
  Address instruction_stream_address = 0;
  uint32_t expected_instruction_stream_dump_size = 0;
  Address external_string_address = 0;
  uint32_t external_string_size = 0;

  std::vector<uint8_t> bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        DirectHandle<HeapNumber> heap_number =
            isolate->factory()->NewHeapNumber(7.25);
        DirectHandle<JSObject> object =
            isolate->factory()->NewJSObject(isolate->object_function());
        DirectHandle<InstructionStream> instruction_stream =
            CreateInstructionStreamForTest(isolate);
        DirectHandle<String> external_string =
            CreateExternalStringForTest(isolate, context);

        BinaryWriterTest writer;
        ObjectDumper dumper(isolate->heap(), &writer);

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(heap_number);
        artifacts.keep_alive.push_back(object);
        artifacts.keep_alive.push_back(instruction_stream);
        artifacts.keep_alive.push_back(external_string);

        InvokeAtomicMajorGC(isolate->heap());

        heap_number_address = heap_number->address();
        heap_number_size = static_cast<uint32_t>(heap_number->Size());
        object_address = object->address();
        object_size = static_cast<uint32_t>(object->Size());
        instruction_stream_address = instruction_stream->address();
        expected_instruction_stream_dump_size =
            dumper.PreVisitInstructionStream(*instruction_stream);
        external_string_address = external_string->address();
        external_string_size = static_cast<uint32_t>(external_string->Size());
        return artifacts;
      });

  RawObjectRecord heap_number_record =
      FindRawObject(bytes, heap_number_address);
  CHECK_EQ(dfx::kNoContentObjectDumpSize, heap_number_record.dump_size);
  CHECK_EQ(heap_number_size, ReadRawObjectRealSize(bytes, heap_number_record));
  CHECK_NE(static_cast<Tagged_t>(0),
           ReadRawObjectMapPointer(bytes, heap_number_record));

  RawObjectRecord object_record = FindRawObject(bytes, object_address);
  CHECK_EQ(object_size, object_record.dump_size);

  RawObjectRecord instruction_stream_record =
      FindRawObject(bytes, instruction_stream_address);
  CHECK_EQ(expected_instruction_stream_dump_size,
           instruction_stream_record.dump_size);
  CHECK_GT(instruction_stream_record.dump_size, dfx::kNoContentObjectDumpSize);

  RawObjectRecord external_string_record =
      FindRawObject(bytes, external_string_address);
  CHECK_EQ(dfx::kNoContentObjectDumpSize, external_string_record.dump_size);
  CHECK_EQ(external_string_size,
           ReadRawObjectRealSize(bytes, external_string_record));
}

#if defined(OH_ENABLE_HEAP_TRANSLATE)
UNINITIALIZED_TEST(HeapDumpTranslateRejectsTruncatedHeaderGracefully) {
  std::vector<uint8_t> bytes(sizeof(RawHeapHeader) - 1, 0);
  TranslateDiagnostics diagnostics;

  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_fatal());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnostic(diagnostics, TranslateDiagnostics::Severity::kFatal,
                      "header_read_failed", "header"));
}

UNINITIALIZED_TEST(HeapDumpTranslateRejectsBadMagicGracefully) {
  std::vector<uint8_t> bytes = DumpBaselineHeapBytes();

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  header.magic_ = dfx::kRawHeapMagic + 1;
  WriteRawHeapHeader(&bytes, header);

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_fatal());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnosticMessageContaining(
      diagnostics, TranslateDiagnostics::Severity::kFatal, "header_read_failed",
      "header", "rawheap magic mismatch"));
}

UNINITIALIZED_TEST(HeapDumpTranslateRejectsDumpVersionMismatchGracefully) {
  std::vector<uint8_t> bytes = DumpBaselineHeapBytes();

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  header.dump_format_version_ = dfx::kDumpFormatVersion + 1;
  WriteRawHeapHeader(&bytes, header);

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_fatal());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnosticMessageContaining(
      diagnostics, TranslateDiagnostics::Severity::kFatal, "header_read_failed",
      "header", "unsupported rawheap dump format version"));
  CHECK(HasDiagnosticMessageContaining(
      diagnostics, TranslateDiagnostics::Severity::kFatal, "header_read_failed",
      "header", std::to_string(dfx::kDumpFormatVersion + 1).c_str()));
  CHECK(HasDiagnosticMessageContaining(
      diagnostics, TranslateDiagnostics::Severity::kFatal, "header_read_failed",
      "header", dfx::GetRawHeapDumpFormatVersionsForTranslate().c_str()));
}

UNINITIALIZED_TEST(HeapDumpTranslateSupportsLegacyV1AndCurrentVersions) {
  CHECK(dfx::IsRawHeapDumpFormatVersionSupportedForTranslate(
      dfx::kDumpFormatVersionV1));
  CHECK(dfx::IsRawHeapDumpFormatVersionSupportedForTranslate(
      dfx::kDumpFormatVersionWithOffheapTables));
  CHECK(dfx::IsRawHeapDumpFormatVersionSupportedForTranslate(
      dfx::kDumpFormatVersionCurrent));
  CHECK(!dfx::IsRawHeapDumpFormatVersionSupportedForTranslate(
      dfx::kDumpFormatVersionCurrent + 1));
  CHECK_NE(std::string::npos,
           dfx::GetRawHeapDumpFormatVersionsForTranslate().find(
               std::to_string(dfx::kDumpFormatVersionV1)));
  CHECK_NE(std::string::npos,
           dfx::GetRawHeapDumpFormatVersionsForTranslate().find(
               std::to_string(dfx::kDumpFormatVersionWithOffheapTables)));
  CHECK_NE(std::string::npos,
           dfx::GetRawHeapDumpFormatVersionsForTranslate().find(
               std::to_string(dfx::kDumpFormatVersionCurrent)));
}

UNINITIALIZED_TEST(HeapDumpTranslateAcceptsLegacyV2HeaderGracefully) {
  std::vector<uint8_t> bytes =
      ConvertCurrentRawHeapToLegacyV2(DumpBaselineHeapBytes());

  dfx::BinaryReaderTest reader(bytes.data(),
                               static_cast<uint32_t>(bytes.size()));
  RawHeapHeader migrated_header{};
  std::string error;
  CHECK(dfx::ReadRawHeapHeaderForTranslate(&reader, &migrated_header, &error));
  CHECK(error.empty());
  CHECK_EQ(kRawHeapFormatVersionV2ForTest,
           migrated_header.dump_format_version_);
  CHECK_EQ(sizeof(RawHeapHeaderV2ForTest), reader.CurrentPosition());
  CHECK_EQ(sizeof(RawHeapHeaderV2ForTest), migrated_header.root_table_offset_);
  CHECK_EQ(static_cast<Address>(0), migrated_header.trusted_cage_base_);
  CheckOffheapTableHeaderLayout(bytes, migrated_header);
  CHECK(dfx::IsRawHeapDumpFormatVersionSupportedForTranslate(
      migrated_header.dump_format_version_));

  TranslateDiagnostics diagnostics;
  CHECK(TryTranslateHeapBytes(bytes, &diagnostics));
#if defined(V8_ENABLE_SANDBOX)
  CHECK(HasDiagnostic(diagnostics, TranslateDiagnostics::Severity::kWarning,
                      "sandbox_protected_pointer_missing_trusted_cage_base",
                      "object_body"));
#else
  CHECK(!diagnostics.has_errors());
#endif
}

UNINITIALIZED_TEST(
    HeapDumpRawHeapHeaderV2MigrationPreservesSectionsAndMarksTrustedBaseUnavailable) {
  std::vector<uint8_t> current_bytes = DumpBaselineHeapBytes();
  RawHeapHeader current_header = ReadRawHeapHeader(current_bytes);
  std::vector<uint8_t> bytes = ConvertCurrentRawHeapToLegacyV2(current_bytes);
  RawHeapHeaderV2ForTest v2_header = ReadPod<RawHeapHeaderV2ForTest>(bytes, 0);

  dfx::BinaryReaderTest reader(bytes.data(),
                               static_cast<uint32_t>(bytes.size()));
  RawHeapHeader migrated_header{};
  std::string error;
  CHECK(dfx::ReadRawHeapHeaderForTranslate(&reader, &migrated_header, &error));
  CHECK(error.empty());

  CHECK_EQ(kRawHeapFormatVersionV2ForTest, v2_header.dump_format_version_);
  CHECK_EQ(kRawHeapFormatVersionV2ForTest,
           migrated_header.dump_format_version_);
  CHECK_EQ(static_cast<Address>(0), migrated_header.trusted_cage_base_);
  CHECK_EQ(sizeof(RawHeapHeaderV2ForTest), reader.CurrentPosition());
  CHECK_EQ(current_header.magic_, migrated_header.magic_);
  CHECK_EQ(current_header.flags_, migrated_header.flags_);
  CHECK_EQ(current_header.cage_base_, migrated_header.cage_base_);
  CHECK_EQ(current_header.code_cage_base_, migrated_header.code_cage_base_);
  CHECK_EQ(current_header.root_table_size_, migrated_header.root_table_size_);
  CHECK_EQ(current_header.root_type_count_, migrated_header.root_type_count_);
  CHECK_EQ(current_header.string_table_size_,
           migrated_header.string_table_size_);
  CHECK_EQ(current_header.string_count_, migrated_header.string_count_);
  CHECK_EQ(current_header.object_table_size_,
           migrated_header.object_table_size_);
  CHECK_EQ(current_header.object_count_, migrated_header.object_count_);
  CHECK_EQ(current_header.offheap_table_size_,
           migrated_header.offheap_table_size_);
  CHECK_EQ(current_header.offheap_table_count_,
           migrated_header.offheap_table_count_);
  CHECK_EQ(current_header.metadata_size_, migrated_header.metadata_size_);
  CHECK_EQ(v2_header.root_table_offset_, migrated_header.root_table_offset_);
  CHECK_EQ(v2_header.string_table_offset_,
           migrated_header.string_table_offset_);
  CHECK_EQ(v2_header.object_table_offset_,
           migrated_header.object_table_offset_);
  CHECK_EQ(v2_header.offheap_table_offset_,
           migrated_header.offheap_table_offset_);
  CHECK_EQ(v2_header.metadata_offset_, migrated_header.metadata_offset_);
}

#if defined(V8_ENABLE_SANDBOX)
UNINITIALIZED_TEST(
    HeapDumpSandboxProtectedPointerV2SkipsWithTrustedBaseDiagnostic) {
  BytecodeArrayProtectedPointerState state;
  std::vector<uint8_t> bytes = ConvertCurrentRawHeapToLegacyV2(
      DumpBytecodeArrayProtectedPointerHeapBytes(&state));
  CHECK(state.saw_bytecode_array_type);
  CHECK(state.saw_constant_pool_type);
  CHECK(state.saw_handler_table_type);

  TranslateDiagnostics diagnostics;
  bool ok = false;
  std::unique_ptr<SnapshotGenerator> generator =
      TranslateHeapBytes(bytes, &diagnostics, &ok);
  CHECK(ok);
  CHECK(!diagnostics.has_errors());
  CHECK(HasDiagnostic(diagnostics, TranslateDiagnostics::Severity::kWarning,
                      "sandbox_protected_pointer_missing_trusted_cage_base",
                      "object_body"));
  CHECK(!HasDiagnostic(diagnostics,
                       TranslateDiagnostics::Severity::kUnsupported,
                       "VisitProtectedPointer in sandbox", "object_body"));

  const Node* bytecode_array =
      GetSnapshotNode(generator.get(), state.bytecode_array);
  const Node* constant_pool =
      GetSnapshotNode(generator.get(), state.constant_pool);
  const Node* handler_table =
      GetSnapshotNode(generator.get(), state.handler_table);
  CHECK_NOT_NULL(bytecode_array);
  CHECK_NOT_NULL(constant_pool);
  CHECK_NOT_NULL(handler_table);
  CHECK_EQ(0, CountDumpCategoryEdgesFromNodeToTarget(
                  bytecode_array, Edge::Type::kHidden, constant_pool));
  CHECK_EQ(0, CountDumpCategoryEdgesFromNodeToTarget(
                  bytecode_array, Edge::Type::kHidden, handler_table));
}
#else
UNINITIALIZED_TEST(
    HeapDumpSandboxProtectedPointerV2SkipsWithTrustedBaseDiagnostic) {
  std::vector<uint8_t> bytes =
      ConvertCurrentRawHeapToLegacyV2(DumpBaselineHeapBytes());
  TranslateDiagnostics diagnostics;
  CHECK(TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(!diagnostics.has_errors());
  CHECK(!HasDiagnostic(diagnostics, TranslateDiagnostics::Severity::kUnsupported,
                       "sandbox_protected_pointer_missing_trusted_cage_base",
                       "object_body"));
}
#endif

UNINITIALIZED_TEST(HeapDumpTranslateAcceptsLegacyV1HeaderGracefully) {
  std::vector<uint8_t> bytes =
      ConvertCurrentRawHeapToLegacyV1(DumpBaselineHeapBytes());

  dfx::BinaryReaderTest reader(bytes.data(),
                               static_cast<uint32_t>(bytes.size()));
  RawHeapHeader migrated_header{};
  std::string error;
  CHECK(dfx::ReadRawHeapHeaderForTranslate(&reader, &migrated_header, &error));
  CHECK(error.empty());
  CHECK_EQ(kRawHeapFormatVersionV1ForTest,
           migrated_header.dump_format_version_);
  CHECK_EQ(sizeof(RawHeapHeaderV1ForTest), reader.CurrentPosition());
  CHECK_EQ(sizeof(RawHeapHeaderV1ForTest), migrated_header.root_table_offset_);
  CHECK(!dfx::HasRawHeapOffheapTableSection(migrated_header));
  CHECK_EQ(0u, migrated_header.offheap_table_size_);
  CHECK_EQ(0u, migrated_header.offheap_table_count_);
  CHECK_EQ(
      migrated_header.object_table_offset_ + migrated_header.object_table_size_,
      migrated_header.offheap_table_offset_);
  if (migrated_header.metadata_size_ == 0) {
    CHECK_EQ(migrated_header.offheap_table_offset_,
             migrated_header.metadata_offset_);
  }
  CHECK(dfx::IsRawHeapDumpFormatVersionSupportedForTranslate(
      migrated_header.dump_format_version_));

  TranslateDiagnostics diagnostics;
  CHECK(TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(!diagnostics.has_errors());
}

UNINITIALIZED_TEST(HeapDumpTranslateLegacyV1NormalizesMetadataOffsetCorrectly) {
  std::vector<uint8_t> bytes =
      ConvertCurrentRawHeapToLegacyV1(DumpBaselineHeapBytes());

  dfx::BinaryReaderTest reader(bytes.data(),
                               static_cast<uint32_t>(bytes.size()));
  RawHeapHeader migrated_header{};
  std::string error;
  CHECK(dfx::ReadRawHeapHeaderForTranslate(&reader, &migrated_header, &error));
  CHECK(error.empty());

  CHECK_EQ(0u, migrated_header.metadata_size_);
  CHECK_EQ(migrated_header.offheap_table_offset_,
           migrated_header.metadata_offset_);
}

UNINITIALIZED_TEST(
    HeapDumpTranslateLegacyV1RejectsCorruptedObjectTableOffsetGracefully) {
  std::vector<uint8_t> bytes =
      ConvertCurrentRawHeapToLegacyV1(DumpBaselineHeapBytes());

  RawHeapHeaderV1ForTest v1_header = ReadPod<RawHeapHeaderV1ForTest>(bytes, 0);
  CHECK_GT(v1_header.object_table_offset_, sizeof(RawHeapHeaderV1ForTest));
  v1_header.object_table_offset_ = static_cast<uint32_t>(bytes.size()) + 100;
  WriteRawHeapHeaderV1ForTest(&bytes, v1_header);

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_errors());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnostic(diagnostics,
                      TranslateDiagnostics::Severity::kCorruptInput,
                      "object_offset_mismatch", "object_table"));
}

UNINITIALIZED_TEST(
    HeapDumpTranslateLegacyV1RejectsCorruptedObjectCountGracefully) {
  std::vector<uint8_t> bytes =
      ConvertCurrentRawHeapToLegacyV1(DumpBaselineHeapBytes());

  RawHeapHeaderV1ForTest v1_header = ReadPod<RawHeapHeaderV1ForTest>(bytes, 0);
  CHECK_GT(v1_header.object_count_, 0u);
  v1_header.object_count_ += 100;
  WriteRawHeapHeaderV1ForTest(&bytes, v1_header);

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_errors());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnostic(diagnostics,
                      TranslateDiagnostics::Severity::kCorruptInput,
                      "object_record_truncated", "object_table"));
}

UNINITIALIZED_TEST(
    HeapDumpTranslateCurrentHeaderRejectsOffheapOverlappingObjectTableGracefully) {
  std::vector<uint8_t> bytes = PrepareOffheapTestBytesWithDefaultTable();

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  CHECK_GT(header.object_table_offset_, 0u);
  CHECK_GT(header.object_table_size_, 0u);
  const uint32_t object_table_end =
      header.object_table_offset_ + header.object_table_size_;
  header.offheap_table_offset_ = object_table_end - 1;
  WriteRawHeapHeader(&bytes, header);

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_fatal());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnostic(diagnostics, TranslateDiagnostics::Severity::kFatal,
                      "offheap_table_invalid", "offheap_table"));
}

UNINITIALIZED_TEST(
    HeapDumpTranslateCurrentHeaderRejectsOffheapOutOfBoundsGracefully) {
  std::vector<uint8_t> bytes = PrepareOffheapTestBytesWithDefaultTable();

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  header.offheap_table_size_ =
      static_cast<uint32_t>(bytes.size()) - header.offheap_table_offset_ + 100;
  WriteRawHeapHeader(&bytes, header);

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_fatal());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnostic(diagnostics, TranslateDiagnostics::Severity::kFatal,
                      "offheap_table_invalid", "offheap_table"));
}

UNINITIALIZED_TEST(
    HeapDumpTranslateCurrentHeaderRejectsZeroOffsetWithNonzeroSizeGracefully) {
  std::vector<uint8_t> bytes =
      RemoveRawHeapOffheapSection(DumpBaselineHeapBytes());

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  CHECK(!dfx::HasRawHeapOffheapTableSection(header));

  header.offheap_table_offset_ = 0;
  header.offheap_table_size_ = 100;
  header.offheap_table_count_ = 1;
  WriteRawHeapHeader(&bytes, header);

  dfx::BinaryReaderTest reader(bytes.data(),
                               static_cast<uint32_t>(bytes.size()));
  RawHeapHeader read_header{};
  std::string error;
  CHECK(dfx::ReadRawHeapHeaderForTranslate(&reader, &read_header, &error));
  CHECK(!dfx::ValidateRawHeapOffheapTableSection(
      read_header, static_cast<uint32_t>(bytes.size()), &error));
  CHECK(!error.empty());

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_fatal());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnostic(diagnostics, TranslateDiagnostics::Severity::kFatal,
                      "offheap_table_invalid", "offheap_table"));
}

UNINITIALIZED_TEST(
    HeapDumpTranslateCurrentHeaderRejectsSizeZeroWithNonzeroCountGracefully) {
  std::vector<uint8_t> bytes =
      RemoveRawHeapOffheapSection(DumpBaselineHeapBytes());

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  CHECK(!dfx::HasRawHeapOffheapTableSection(header));

  header.offheap_table_offset_ =
      header.object_table_offset_ + header.object_table_size_;
  header.offheap_table_size_ = 0;
  header.offheap_table_count_ = 1;
  WriteRawHeapHeader(&bytes, header);

  dfx::BinaryReaderTest reader(bytes.data(),
                               static_cast<uint32_t>(bytes.size()));
  RawHeapHeader read_header{};
  std::string error;
  CHECK(dfx::ReadRawHeapHeaderForTranslate(&reader, &read_header, &error));
  CHECK(!dfx::ValidateRawHeapOffheapTableSection(
      read_header, static_cast<uint32_t>(bytes.size()), &error));
  CHECK(!error.empty());

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_fatal());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnostic(diagnostics, TranslateDiagnostics::Severity::kFatal,
                      "offheap_table_invalid", "offheap_table"));
}

UNINITIALIZED_TEST(
    HeapDumpTranslateCurrentHeaderRejectsCountZeroWithNonzeroSizeGracefully) {
  std::vector<uint8_t> bytes =
      RemoveRawHeapOffheapSection(DumpBaselineHeapBytes());

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  CHECK(!dfx::HasRawHeapOffheapTableSection(header));

  header.offheap_table_offset_ =
      header.object_table_offset_ + header.object_table_size_;
  header.offheap_table_size_ = sizeof(dfx::RawHeapOffheapTableHeader);
  header.offheap_table_count_ = 0;
  WriteRawHeapHeader(&bytes, header);

  dfx::BinaryReaderTest reader(bytes.data(),
                               static_cast<uint32_t>(bytes.size()));
  RawHeapHeader read_header{};
  std::string error;
  CHECK(dfx::ReadRawHeapHeaderForTranslate(&reader, &read_header, &error));
  CHECK(!dfx::ValidateRawHeapOffheapTableSection(
      read_header, static_cast<uint32_t>(bytes.size()), &error));
  CHECK(!error.empty());

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_fatal());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnostic(diagnostics, TranslateDiagnostics::Severity::kFatal,
                      "offheap_table_invalid", "offheap_table"));
}

UNINITIALIZED_TEST(
    HeapDumpTranslateCurrentHeaderAcceptsAbsentOffheapSectionConsistentFields) {
  std::vector<uint8_t> bytes =
      RemoveRawHeapOffheapSection(DumpBaselineHeapBytes());

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  CHECK(!dfx::HasRawHeapOffheapTableSection(header));

  CHECK_EQ(header.object_table_offset_ + header.object_table_size_,
           header.offheap_table_offset_);
  CHECK_EQ(0u, header.offheap_table_size_);
  CHECK_EQ(0u, header.offheap_table_count_);
  CHECK_EQ(header.offheap_table_offset_ + header.offheap_table_size_,
           header.metadata_offset_);

  TranslateDiagnostics diagnostics;
  CHECK(TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(!diagnostics.has_errors());
}

UNINITIALIZED_TEST(HeapDumpTranslateRejectsMissingTimestampGracefully) {
  std::vector<uint8_t> bytes = DumpBaselineHeapBytes();

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  header.timestamp_ = 0;
  WriteRawHeapHeader(&bytes, header);

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_fatal());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnostic(diagnostics, TranslateDiagnostics::Severity::kFatal,
                      "timestamp_missing", "header"));
}

UNINITIALIZED_TEST(HeapDumpTranslateRejectsV8VersionMismatchGracefully) {
  std::vector<uint8_t> bytes = DumpBaselineHeapBytes();

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  header.v8_patch_level_ += 1;
  WriteRawHeapHeader(&bytes, header);

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_fatal());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnostic(diagnostics, TranslateDiagnostics::Severity::kFatal,
                      "v8_version_mismatch", "header"));
}

UNINITIALIZED_TEST(HeapDumpTranslateRejectsFlagMismatchGracefully) {
  std::vector<uint8_t> bytes = DumpBaselineHeapBytes();

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  constexpr uint32_t kRawHeapSandboxFlagMaskForTest = 1u << 3;
  uint32_t mismatched_flags = header.flags_ ^ kRawHeapSandboxFlagMaskForTest;
  CHECK_NE(dfx::RawHeapFlagSandbox(header.flags_),
           dfx::RawHeapFlagSandbox(mismatched_flags));
  header.flags_ = mismatched_flags;
  WriteRawHeapHeader(&bytes, header);

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_fatal());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnosticMessageContaining(
      diagnostics, TranslateDiagnostics::Severity::kFatal,
      "rawheap_flags_mismatch", "header", "sandbox:"));
}

UNINITIALIZED_TEST(HeapDumpTranslateRejectsRootOffsetMismatchGracefully) {
  std::vector<uint8_t> bytes = DumpBaselineHeapBytes();

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  CHECK_LT(
      static_cast<size_t>(header.root_table_offset_) + header.root_table_size_,
      bytes.size());
  header.root_table_offset_ += 1;
  WriteRawHeapHeader(&bytes, header);

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_errors());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnostic(diagnostics,
                      TranslateDiagnostics::Severity::kCorruptInput,
                      "root_offset_mismatch", "root_table"));
}

UNINITIALIZED_TEST(HeapDumpTranslateRejectsRootTableOutOfBoundsGracefully) {
  std::vector<uint8_t> bytes = DumpBaselineHeapBytes();

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  CHECK_LT(static_cast<size_t>(header.root_table_offset_), bytes.size());
  size_t out_of_bounds_size = bytes.size() - header.root_table_offset_ + 1;
  CHECK_LE(out_of_bounds_size,
           static_cast<size_t>(std::numeric_limits<uint32_t>::max()));
  header.root_table_size_ = static_cast<uint32_t>(out_of_bounds_size);
  WriteRawHeapHeader(&bytes, header);

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_errors());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnostic(diagnostics,
                      TranslateDiagnostics::Severity::kCorruptInput,
                      "root_table_out_of_bounds", "root_table"));
}

UNINITIALIZED_TEST(HeapDumpTranslateRejectsRootTypeCountBeyondTableGracefully) {
  std::vector<uint8_t> bytes = DumpBaselineHeapBytes();

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  CHECK_GT(header.root_type_count_, 0u);
  header.root_type_count_ += 1;
  WriteRawHeapHeader(&bytes, header);

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_errors());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnostic(diagnostics,
                      TranslateDiagnostics::Severity::kCorruptInput,
                      "root_record_truncated", "root_table"));
}

UNINITIALIZED_TEST(HeapDumpTranslateRejectsInvalidRootTypeGracefully) {
  std::vector<uint8_t> bytes = DumpBaselineHeapBytes();

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  CHECK_GT(header.root_type_count_, 0u);
  uint32_t invalid_type = static_cast<uint32_t>(Root::kNumberOfRoots);
  WritePodAt(&bytes, header.root_table_offset_, invalid_type);

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_errors());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnostic(diagnostics,
                      TranslateDiagnostics::Severity::kCorruptInput,
                      "root_type_invalid", "root_table"));
}

UNINITIALIZED_TEST(HeapDumpTranslateRejectsOffheapPayloadOverlappingDirectory) {
  std::vector<uint8_t> bytes = DumpBaselineHeapBytes();

  dfx::RawHeapOffheapTableHeader table_header{};
  table_header.kind_ = static_cast<uint32_t>(
      dfx::RawHeapOffheapTableKind::kWasmCanonicalTypeTable);
  table_header.offset_ = 0;
  table_header.size_ = 0;
  table_header.count_ = 0;
  std::vector<uint8_t> section = BuildOffheapDirectorySection({table_header});
  InsertOffheapSection(&bytes, section, 1);

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_fatal());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnostic(diagnostics, TranslateDiagnostics::Severity::kFatal,
                      "offheap_table_payload_overlaps_directory",
                      "offheap_table"));
}

UNINITIALIZED_TEST(HeapDumpTranslateSkipsDuplicateUnknownOffheapKinds) {
  std::vector<uint8_t> bytes = DumpBaselineHeapBytes();

  constexpr uint32_t kFutureUnknownTableKindForTest = 0x80000000u;
  dfx::RawHeapOffheapTableHeader table_header{};
  table_header.kind_ = kFutureUnknownTableKindForTest;
  table_header.offset_ = 0;
  table_header.size_ = 0;
  table_header.count_ = 0;
  std::vector<uint8_t> section =
      BuildOffheapDirectorySection({table_header, table_header});
  InsertOffheapSection(&bytes, section, 2);

  TranslateDiagnostics diagnostics;
  CHECK(TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(!diagnostics.has_errors());
  CHECK(HasDiagnostic(diagnostics, TranslateDiagnostics::Severity::kWarning,
                      "offheap_table_unknown_kind", "offheap_table"));
}

UNINITIALIZED_TEST(HeapDumpTranslateRejectsTruncatedObjectTableGracefully) {
  std::vector<uint8_t> bytes =
      RemoveRawHeapOffheapSection(DumpBaselineHeapBytes());

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  CHECK_GT(header.object_table_size_, 0u);
  bytes.resize(header.object_table_offset_ + header.object_table_size_ - 1);

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_errors());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnostic(diagnostics,
                      TranslateDiagnostics::Severity::kCorruptInput,
                      "object_table_out_of_bounds", "object_table"));
}

UNINITIALIZED_TEST(HeapDumpTranslateRejectsObjectCountBeyondTableGracefully) {
  std::vector<uint8_t> bytes = DumpBaselineHeapBytes();

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  CHECK_GT(header.object_count_, 0u);
  header.object_count_ += 1;
  WriteRawHeapHeader(&bytes, header);

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_errors());
  CHECK_GT(diagnostics.issue_count(), 0u);
  CHECK(HasDiagnostic(diagnostics,
                      TranslateDiagnostics::Severity::kCorruptInput,
                      "object_record_truncated", "object_table"));
}

UNINITIALIZED_TEST(HeapDumpTranslateMapOnlyObjectsExposeOnlyMapEdge) {
  Address heap_number_address = 0;
  Address heap_number_map_address = 0;
  Address external_string_address = 0;
  Address external_string_map_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            DirectHandle<HeapNumber> heap_number =
                isolate->factory()->NewHeapNumber(7.25);
            DirectHandle<String> external_string =
                CreateExternalStringForTest(isolate, context);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(heap_number);
            artifacts.keep_alive.push_back(external_string);

            InvokeAtomicMajorGC(isolate->heap());
            heap_number_address = heap_number->address();
            heap_number_map_address = heap_number->map()->address();
            external_string_address = external_string->address();
            external_string_map_address = external_string->map()->address();
            return artifacts;
          });

  Node* heap_number_node = generator->GetNode(heap_number_address);
  CHECK_NOT_NULL(heap_number_node);
  CHECK_EQ(static_cast<uint32_t>(Node::Type::kHeapNumber),
           heap_number_node->type_);
  Node* heap_number_map = generator->GetNode(heap_number_map_address);
  CHECK_NOT_NULL(heap_number_map);
  CheckMapOnlyObjectHasExactInternalMapEdge(generator.get(), heap_number_node,
                                            heap_number_map);

  Node* external_string_node = generator->GetNode(external_string_address);
  CHECK_NOT_NULL(external_string_node);
  CHECK_EQ(static_cast<uint32_t>(Node::Type::kString),
           external_string_node->type_);
  Node* external_string_map = generator->GetNode(external_string_map_address);
  CHECK_NOT_NULL(external_string_map);
  CheckMapOnlyObjectHasExactInternalMapEdge(
      generator.get(), external_string_node, external_string_map);
}

UNINITIALIZED_TEST(HeapDumpTranslateFakeObjectLookupUsesRawheapTable) {
  Address object_address = 0;
  Address expected_map_address = 0;

  std::vector<uint8_t> bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<JSObject> object =
            isolate->factory()->NewJSObject(isolate->object_function());

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(object);

        InvokeAtomicMajorGC(isolate->heap());
        object_address = object->address();
        expected_map_address = object->map()->address();
        return artifacts;
      });

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  V8HeapCompressionScheme::InitBase(header.cage_base_);
  ExternalCodeCompressionScheme::InitBase(header.code_cage_base_);

  BinaryReaderTest reader(bytes.data(), static_cast<uint32_t>(bytes.size()));
  reader.SetPosition(header.object_table_offset_);
  SnapshotGenerator generator;
  TranslateDiagnostics diagnostics;
  dfx::ObjectTranslator translator(
      &reader, header.object_table_offset_, header.object_table_size_,
      header.object_count_, PtrComprCageBase(header.cage_base_),
      PtrComprCageBase(header.code_cage_base_),
      header.dump_format_version_ >=
              dfx::kDumpFormatVersionWithTrustedCageBase &&
          header.trusted_cage_base_ != 0,
      &generator, &diagnostics);

  CHECK(translator.Translate());
  CHECK(!diagnostics.has_errors());

  Address actual_map_address = 0;
  CHECK(translator.TestLookupFakeObjectMapAddress(object_address,
                                                  &actual_map_address));
  CHECK_EQ(expected_map_address, actual_map_address);
  actual_map_address = expected_map_address;
  CHECK(!translator.TestLookupFakeObjectMapAddress(object_address + 1,
                                                   &actual_map_address));
  CHECK_EQ(static_cast<Address>(0), actual_map_address);
  actual_map_address = expected_map_address;
  CHECK(!translator.TestLookupFakeObjectMapAddress(0, &actual_map_address));
  CHECK_EQ(static_cast<Address>(0), actual_map_address);
}

UNINITIALIZED_TEST(HeapDumpTranslateUnknownInstanceTypeDoesNotAbort) {
  Address heap_number_address = 0;
  Address heap_number_map_address = 0;

  std::vector<uint8_t> bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<HeapNumber> heap_number =
            isolate->factory()->NewHeapNumber(19.75);

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(heap_number);

        InvokeAtomicMajorGC(isolate->heap());
        heap_number_address = heap_number->address();
        heap_number_map_address = heap_number->map()->address();
        return artifacts;
      });

  RawObjectRecord map_record = FindRawObject(bytes, heap_number_map_address);
  constexpr uint32_t kMapPayloadMinimumSizeForInstanceType =
      static_cast<uint32_t>(Map::kInstanceTypeOffset + sizeof(uint16_t));
  constexpr uint32_t kMapPayloadSizeForInstanceType =
      ((kMapPayloadMinimumSizeForInstanceType +
        static_cast<uint32_t>(kTaggedSize) - 1) /
       static_cast<uint32_t>(kTaggedSize)) *
      static_cast<uint32_t>(kTaggedSize);
  if (map_record.dump_size < kMapPayloadSizeForInstanceType) {
    GrowRawObjectPayload(&bytes, map_record, kMapPayloadSizeForInstanceType);
    map_record = FindRawObject(bytes, heap_number_map_address);
  }
  constexpr uint16_t kUnknownInstanceTypeForTest =
      std::numeric_limits<uint16_t>::max();
  WritePodAt(&bytes, map_record.payload_offset + Map::kInstanceTypeOffset,
             kUnknownInstanceTypeForTest);

  TranslateDiagnostics diagnostics;
  RawHeapHeader header = ReadRawHeapHeader(bytes);
  BinaryReaderTest reader(bytes.data(), static_cast<uint32_t>(bytes.size()));
  SnapshotGenerator generator;
  dfx::ObjectTranslator translator(
      &reader, header.object_table_offset_, header.object_table_size_,
      header.object_count_, PtrComprCageBase(header.cage_base_),
      PtrComprCageBase(header.code_cage_base_),
      header.dump_format_version_ >=
              dfx::kDumpFormatVersionWithTrustedCageBase &&
          header.trusted_cage_base_ != 0,
      &generator, &diagnostics);
  CHECK_EQ(nullptr, translator.TestGetSystemEntryName(
                        static_cast<InstanceType>(kUnknownInstanceTypeForTest)));

  bool ok = TryTranslateHeapBytes(bytes, &diagnostics);
  CHECK(ok);
  USE(heap_number_address);
}

UNINITIALIZED_TEST(HeapDumpTranslateRejectsOversizedObjectPayload) {
  std::vector<uint8_t> bytes(sizeof(Address) + sizeof(uint32_t) +
                             sizeof(Tagged_t));
  const Address object_address = 0x100000000u;
  const uint32_t oversized_payload =
      static_cast<uint32_t>(std::numeric_limits<int>::max()) + 1u;
  const uint32_t virtual_object_table_size =
      static_cast<uint32_t>(sizeof(Address) + sizeof(uint32_t)) +
      oversized_payload;
  WritePodAt(&bytes, 0, object_address);
  WritePodAt(&bytes, sizeof(Address), oversized_payload);

  SparseObjectTableReader reader(bytes.data(),
                                 static_cast<uint32_t>(bytes.size()),
                                 virtual_object_table_size);
  SnapshotGenerator generator;
  TranslateDiagnostics diagnostics;
  dfx::ObjectTranslator translator(
      &reader, 0, virtual_object_table_size, 1,
      PtrComprCageBase(static_cast<Address>(0)),
      PtrComprCageBase(static_cast<Address>(0)), false, &generator,
      &diagnostics);

  CHECK(!translator.Translate());
  CHECK(HasDiagnostic(diagnostics,
                      TranslateDiagnostics::Severity::kCorruptInput,
                      "object_payload_too_large", "object_table"));
}

UNINITIALIZED_TEST(HeapDumpTranslateRejectsMalformedMapOnlyPayload) {
  Address heap_number_address = 0;

  std::vector<uint8_t> bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<HeapNumber> heap_number =
            isolate->factory()->NewHeapNumber(31.5);

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(heap_number);

        InvokeAtomicMajorGC(isolate->heap());
        heap_number_address = heap_number->address();
        return artifacts;
      });

  RawObjectRecord heap_number_record =
      FindRawObject(bytes, heap_number_address);
  CHECK_EQ(dfx::kNoContentObjectDumpSize, heap_number_record.dump_size);
  GrowRawObjectPayload(
      &bytes, heap_number_record,
      dfx::kNoContentObjectDumpSize + static_cast<uint32_t>(kTaggedSize));

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(HasDiagnostic(diagnostics,
                      TranslateDiagnostics::Severity::kCorruptInput,
                      "map_only_payload_size_invalid", "object_table"));
}

UNINITIALIZED_TEST(HeapDumpTranslateRejectsMapOnlyVisitorTypeMismatch) {
  Address object_map_address = 0;

  std::vector<uint8_t> bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        RunScript(context,
                  "globalThis.__heap_dump_map_only_mismatch = { x: 42 };");

        DirectHandle<JSObject> object = GetGlobalDirectHandle<JSObject>(
            isolate, context, "__heap_dump_map_only_mismatch");

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(object);

        InvokeAtomicMajorGC(isolate->heap());
        object_map_address = object->map()->address();
        return artifacts;
      });

  RawObjectRecord map_record = FindRawObject(bytes, object_map_address);
  constexpr uint32_t kMapPayloadMinimumSizeForVisitorId =
      static_cast<uint32_t>(Map::kVisitorIdOffset + sizeof(uint8_t));
  constexpr uint32_t kMapPayloadSizeForVisitorId =
      ((kMapPayloadMinimumSizeForVisitorId +
        static_cast<uint32_t>(kTaggedSize) - 1) /
       static_cast<uint32_t>(kTaggedSize)) *
      static_cast<uint32_t>(kTaggedSize);
  if (map_record.dump_size < kMapPayloadSizeForVisitorId) {
    GrowRawObjectPayload(&bytes, map_record, kMapPayloadSizeForVisitorId);
    map_record = FindRawObject(bytes, object_map_address);
  }

  const uint8_t bad_visitor_id =
      static_cast<uint8_t>(VisitorId::kVisitHeapNumber);
  WritePodAt(&bytes, map_record.payload_offset + Map::kVisitorIdOffset,
             bad_visitor_id);

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(HasDiagnostic(diagnostics,
                      TranslateDiagnostics::Severity::kCorruptInput,
                      "map_only_instance_type_mismatch", "object_table"));
}

UNINITIALIZED_TEST(HeapDumpTranslatePrimitiveWrapperUnexpectedValueDoesNotAbort) {
  Address wrapper_address = 0;
  Address target_address = 0;

  std::vector<uint8_t> bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        RunScript(context, R"(
          globalThis.__heap_dump_wrapper_unexpected_value =
              new String('heap-dump-wrapper-unexpected-value');
          globalThis.__heap_dump_wrapper_unexpected_target = {};
        )");

        DirectHandle<JSPrimitiveWrapper> wrapper =
            GetGlobalDirectHandle<JSPrimitiveWrapper>(
                isolate, context, "__heap_dump_wrapper_unexpected_value");
        DirectHandle<JSObject> target = GetGlobalDirectHandle<JSObject>(
            isolate, context, "__heap_dump_wrapper_unexpected_target");

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(wrapper);
        artifacts.keep_alive.push_back(target);

        InvokeAtomicMajorGC(isolate->heap());
        wrapper_address = wrapper->address();
        target_address = target->address();
        return artifacts;
      });

  RawHeapHeader header = ReadRawHeapHeader(bytes);
  RawObjectRecord wrapper_record = FindRawObject(bytes, wrapper_address);
  const Tagged_t encoded_target =
      EncodeTaggedHeapObjectForRawHeapFieldForDumpCategoryTest(header,
                                                               target_address);
  WritePodAt(&bytes, wrapper_record.payload_offset +
                         JSPrimitiveWrapper::kValueOffset,
             encoded_target);

  TranslateDiagnostics diagnostics;
  CHECK(TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(HasDiagnostic(diagnostics, TranslateDiagnostics::Severity::kWarning,
                      "primitive_wrapper_value_unexpected_type",
                      "object_table"));
}

UNINITIALIZED_TEST(HeapDumpTranslateRejectsMissingFunctionSfiGracefully) {
  Address sfi_address = 0;

  std::vector<uint8_t> bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<String> name =
            isolate->factory()->InternalizeUtf8String("HeapDumpMissingSfi");
        DirectHandle<JSFunction> function =
            isolate->factory()->NewFunctionForTesting(name);

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(function);

        InvokeAtomicMajorGC(isolate->heap());
        sfi_address = function->shared()->address();
        return artifacts;
      });

  RawObjectRecord sfi_record = FindRawObject(bytes, sfi_address);
  RemoveRawObjectRecord(&bytes, sfi_record);

  TranslateDiagnostics diagnostics;
  CHECK(!TryTranslateHeapBytes(bytes, &diagnostics));
  CHECK(diagnostics.has_errors());
  CHECK(HasDiagnostic(diagnostics,
                      TranslateDiagnostics::Severity::kCorruptInput,
                      "js_function_sfi_missing", "object_body"));
}

UNINITIALIZED_TEST(HeapDumpTranslateInstructionStreamUsesSpecialPath) {
  Address instruction_stream_address = 0;
  Address code_address = 0;
  Address relocation_info_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context>) {
            DirectHandle<InstructionStream> instruction_stream =
                CreateInstructionStreamForTest(isolate);

            DumpArtifacts artifacts;
            artifacts.keep_alive.push_back(instruction_stream);

            InvokeAtomicMajorGC(isolate->heap());
            CHECK_EQ(INSTRUCTION_STREAM_TYPE,
                     instruction_stream->map()->instance_type());
            MarkCoveredInstanceType(INSTRUCTION_STREAM_TYPE);
            instruction_stream_address = instruction_stream->address();
            code_address = instruction_stream->code(kAcquireLoad)->address();
            relocation_info_address =
                instruction_stream->relocation_info()->address();
            return artifacts;
          });

  Node* instruction_stream =
      GetSnapshotNode(generator.get(), instruction_stream_address);
  Node* code = GetSnapshotNode(generator.get(), code_address);
  Node* relocation_info =
      GetSnapshotNode(generator.get(), relocation_info_address);
  CHECK_NOT_NULL(instruction_stream);
  CHECK_NOT_NULL(code);
  CHECK_NOT_NULL(relocation_info);
  CHECK_EQ(static_cast<uint32_t>(Node::Type::kCode), instruction_stream->type_);

  // V8HeapExplorer::ExtractInstructionStreamReferences emits the
  // heap-snapshot-aligned named/internal InstructionStream edges.
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), instruction_stream,
                                   Edge::Type::kInternal, "code", code));
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), instruction_stream,
                                   Edge::Type::kInternal, "relocation_info",
                                   relocation_info));

  // Code::instruction_stream is encoded in external code space and must be
  // decoded with code_cage_base_ by the object-specific Code path, not by the
  // generic main-cage body visitor.
  CHECK_EQ(1, CountExactNamedEdges(generator.get(), code, Edge::Type::kInternal,
                                   "instruction_stream", instruction_stream));

  // Fixed protected pointers are represented by the snapshot-aligned named
  // internal edges above. The special InstructionStream payload uses hidden or
  // weak edges only for reloc records, not duplicate fixed fields.
  CHECK_EQ(0, CountDumpCategoryEdgesFromNodeToTarget(
                  instruction_stream, Edge::Type::kHidden, code));
  CHECK_EQ(0, CountDumpCategoryEdgesFromNodeToTarget(
                  instruction_stream, Edge::Type::kHidden, relocation_info));
}

UNINITIALIZED_TEST(
    HeapDumpTranslateInstructionStreamStrongRelocTargetUsesHiddenEdge) {
  Address instruction_stream_address = 0;
  Address target_address = 0;

  std::vector<uint8_t> bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<InstructionStream> instruction_stream =
            CreateInstructionStreamForTest(isolate);
        DirectHandle<JSObject> target =
            isolate->factory()->NewJSObject(isolate->object_function());

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(instruction_stream);
        artifacts.keep_alive.push_back(target);

        InvokeAtomicMajorGC(isolate->heap());
        instruction_stream_address = instruction_stream->address();
        target_address = target->address();
        return artifacts;
      });

  RawObjectRecord instruction_stream_record =
      FindRawObject(bytes, instruction_stream_address);
  // Appended rawheap custom payload relocation records translate strong
  // FULL_EMBEDDED_OBJECT targets as hidden edges.
  AppendInstructionStreamRelocRecord(&bytes, instruction_stream_record,
                                     target_address, false);

  std::unique_ptr<SnapshotGenerator> generator = TranslateHeapBytes(bytes);
  Node* instruction_stream =
      GetSnapshotNode(generator.get(), instruction_stream_address);
  Node* target = GetSnapshotNode(generator.get(), target_address);
  CHECK_NOT_NULL(instruction_stream);
  CHECK_NOT_NULL(target);

  CHECK_EQ(1, CountDumpCategoryEdgesFromNodeToTarget(
                  instruction_stream, Edge::Type::kHidden, target));
  CHECK_EQ(0, CountDumpCategoryEdgesFromNodeToTarget(
                  instruction_stream, Edge::Type::kWeak, target));
}

UNINITIALIZED_TEST(
    HeapDumpTranslateInstructionStreamWeakRelocTargetUsesWeakEdge) {
  Address instruction_stream_address = 0;
  Address target_address = 0;

  std::vector<uint8_t> bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<InstructionStream> instruction_stream =
            CreateInstructionStreamForTest(isolate);
        DirectHandle<JSObject> target =
            isolate->factory()->NewJSObject(isolate->object_function());

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(instruction_stream);
        artifacts.keep_alive.push_back(target);

        InvokeAtomicMajorGC(isolate->heap());
        instruction_stream_address = instruction_stream->address();
        target_address = target->address();
        return artifacts;
      });

  RawObjectRecord instruction_stream_record =
      FindRawObject(bytes, instruction_stream_address);
  // Appended rawheap custom payload relocation records translate weak-tagged
  // FULL_EMBEDDED_OBJECT targets as weak edges.
  AppendInstructionStreamRelocRecord(&bytes, instruction_stream_record,
                                     target_address, true);

  std::unique_ptr<SnapshotGenerator> generator = TranslateHeapBytes(bytes);
  Node* instruction_stream =
      GetSnapshotNode(generator.get(), instruction_stream_address);
  Node* target = GetSnapshotNode(generator.get(), target_address);
  CHECK_NOT_NULL(instruction_stream);
  CHECK_NOT_NULL(target);

  CHECK_EQ(1, CountDumpCategoryEdgesFromNodeToTarget(
                  instruction_stream, Edge::Type::kWeak, target));
  CHECK_EQ(0, CountDumpCategoryEdgesFromNodeToTarget(
                  instruction_stream, Edge::Type::kHidden, target));
}

UNINITIALIZED_TEST(
    HeapDumpTranslateInstructionStreamRelocMissingTargetDoesNotFabricateEdge) {
  Address instruction_stream_address = 0;
  Address target_address = 0;

  std::vector<uint8_t> bytes = DumpWithIsolateTeardown(
      [&](Isolate* isolate, v8::Local<v8::Context> context) {
        USE(context);
        DirectHandle<InstructionStream> instruction_stream =
            CreateInstructionStreamForTest(isolate);
        DirectHandle<JSObject> target =
            isolate->factory()->NewJSObject(isolate->object_function());

        DumpArtifacts artifacts;
        artifacts.keep_alive.push_back(instruction_stream);
        artifacts.keep_alive.push_back(target);

        InvokeAtomicMajorGC(isolate->heap());
        instruction_stream_address = instruction_stream->address();
        target_address = target->address();
        return artifacts;
      });

  RawObjectRecord target_record = FindRawObject(bytes, target_address);
  RemoveRawObjectRecord(&bytes, target_record);
  RawObjectRecord instruction_stream_record =
      FindRawObject(bytes, instruction_stream_address);
  AppendInstructionStreamRelocRecord(&bytes, instruction_stream_record,
                                     target_address, false);
  instruction_stream_record = FindRawObject(bytes, instruction_stream_address);
  AppendInstructionStreamRelocRecord(&bytes, instruction_stream_record,
                                     target_address, true);

  TranslateDiagnostics diagnostics;
  bool ok = false;
  // Regression guard: offline InstructionStream relocation payload should be
  // handled by TranslateInstructionStream. The BodyDescriptor relocation
  // callbacks are defensive fallbacks that normal offline translation does not
  // directly enter; if future/corrupt layouts reach them, they must stay
  // diagnostic-only (no live-deref crash path).
  std::unique_ptr<SnapshotGenerator> generator =
      TranslateHeapBytes(bytes, &diagnostics, &ok);
  CHECK(ok);
  CHECK_NOT_NULL(generator);
  CHECK(!diagnostics.has_errors());
  CHECK(!diagnostics.has_fatal());
  CHECK(HasDiagnostic(diagnostics, TranslateDiagnostics::Severity::kWarning,
                      "instruction_stream_reloc_target_not_dumped",
                      "object_body"));

  Node* instruction_stream =
      GetSnapshotNode(generator.get(), instruction_stream_address);
  CHECK_NOT_NULL(instruction_stream);
  Node* target = generator->GetNode(target_address);
  if (target != nullptr) {
    CHECK_EQ(0, CountDumpCategoryEdgesFromNodeToTarget(
                    instruction_stream, Edge::Type::kHidden, target));
    CHECK_EQ(0, CountDumpCategoryEdgesFromNodeToTarget(
                    instruction_stream, Edge::Type::kWeak, target));
  }
}
#endif  // defined(OH_ENABLE_HEAP_TRANSLATE)

#else

UNINITIALIZED_TEST(HeapDumpObjectCountSkipsFreeSpaceAndFiller) {}
UNINITIALIZED_TEST(HeapDumpHeaderPrefixFieldsAreInitializedAndCompatible) {}
UNINITIALIZED_TEST(HeapDumpTranslateDiagnosticsStoreBoundedSample) {}
UNINITIALIZED_TEST(HeapDumpRawObjectCategoriesAreEncodedAsExpected) {}
#if defined(OH_ENABLE_HEAP_TRANSLATE)
UNINITIALIZED_TEST(HeapDumpRawHeapHeaderTrustedCageBaseValidation) {}
UNINITIALIZED_TEST(
    HeapDumpRawHeapHeaderV2MigrationPreservesSectionsAndMarksTrustedBaseUnavailable) {}
UNINITIALIZED_TEST(HeapDumpRawHeapHeaderV3IncludesTrustedCageBase) {}
UNINITIALIZED_TEST(HeapDumpTranslateAcceptsLegacyV2HeaderGracefully) {}
UNINITIALIZED_TEST(
    HeapDumpSandboxProtectedPointerV2SkipsWithTrustedBaseDiagnostic) {}
UNINITIALIZED_TEST(HeapDumpTranslateRejectsTruncatedHeaderGracefully) {}
UNINITIALIZED_TEST(HeapDumpTranslateRejectsBadMagicGracefully) {}
UNINITIALIZED_TEST(HeapDumpTranslateRejectsDumpVersionMismatchGracefully) {}
UNINITIALIZED_TEST(HeapDumpTranslateSupportsLegacyV1AndCurrentVersions) {}
UNINITIALIZED_TEST(HeapDumpTranslateAcceptsLegacyV1HeaderGracefully) {}
UNINITIALIZED_TEST(HeapDumpTranslateLegacyV1NormalizesMetadataOffsetCorrectly) {
}
UNINITIALIZED_TEST(
    HeapDumpTranslateLegacyV1RejectsCorruptedObjectTableOffsetGracefully) {}
UNINITIALIZED_TEST(
    HeapDumpTranslateLegacyV1RejectsCorruptedObjectCountGracefully) {}
UNINITIALIZED_TEST(
    HeapDumpTranslateCurrentHeaderRejectsOffheapOverlappingObjectTableGracefully) {
}
UNINITIALIZED_TEST(
    HeapDumpTranslateCurrentHeaderRejectsOffheapOutOfBoundsGracefully) {}
UNINITIALIZED_TEST(
    HeapDumpTranslateCurrentHeaderRejectsZeroOffsetWithNonzeroSizeGracefully) {}
UNINITIALIZED_TEST(
    HeapDumpTranslateCurrentHeaderRejectsSizeZeroWithNonzeroCountGracefully) {}
UNINITIALIZED_TEST(
    HeapDumpTranslateCurrentHeaderRejectsCountZeroWithNonzeroSizeGracefully) {}
UNINITIALIZED_TEST(
    HeapDumpTranslateCurrentHeaderAcceptsAbsentOffheapSectionConsistentFields) {
}
UNINITIALIZED_TEST(HeapDumpTranslateRejectsMissingTimestampGracefully) {}
UNINITIALIZED_TEST(HeapDumpTranslateRejectsV8VersionMismatchGracefully) {}
UNINITIALIZED_TEST(HeapDumpTranslateRejectsFlagMismatchGracefully) {}
UNINITIALIZED_TEST(HeapDumpTranslateRejectsRootOffsetMismatchGracefully) {}
UNINITIALIZED_TEST(HeapDumpTranslateRejectsRootTableOutOfBoundsGracefully) {}
UNINITIALIZED_TEST(HeapDumpTranslateRejectsRootTypeCountBeyondTableGracefully) {
}
UNINITIALIZED_TEST(HeapDumpTranslateRejectsInvalidRootTypeGracefully) {}
UNINITIALIZED_TEST(HeapDumpTranslateRejectsOffheapPayloadOverlappingDirectory) {
}
UNINITIALIZED_TEST(HeapDumpTranslateSkipsDuplicateUnknownOffheapKinds) {}
UNINITIALIZED_TEST(HeapDumpTranslateRejectsTruncatedObjectTableGracefully) {}
UNINITIALIZED_TEST(HeapDumpTranslateRejectsObjectCountBeyondTableGracefully) {}
UNINITIALIZED_TEST(HeapDumpTranslateMapOnlyObjectsExposeOnlyMapEdge) {}
UNINITIALIZED_TEST(HeapDumpTranslateInstructionStreamUsesSpecialPath) {}
UNINITIALIZED_TEST(
    HeapDumpTranslateInstructionStreamStrongRelocTargetUsesHiddenEdge) {}
UNINITIALIZED_TEST(
    HeapDumpTranslateInstructionStreamWeakRelocTargetUsesWeakEdge) {}
UNINITIALIZED_TEST(
    HeapDumpTranslateInstructionStreamRelocMissingTargetDoesNotFabricateEdge) {}
#endif

#endif

}  // namespace heap
}  // namespace internal
}  // namespace v8
