/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if (defined(OH_ENABLE_HEAP_TRANSLATE) || defined(OH_ENABLE_HEAP_DUMP_TEST))

#include "translate-roots.h"

#include <limits>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-diagnostics.h"
#include "snapshot-generator.h"
#include "src/base/logging.h"

// see more in RootsReferencesExtrator in heap-snapshot-generator.cc

namespace v8 {
namespace dfx {
namespace {

bool IsTranslatedNativeContextNode(SnapshotGenerator* generator,
                                   i::Address address) {
  Node* node = generator->GetNode(address);
  if (node == nullptr ||
      node->type_ != static_cast<uint32_t>(Node::Type::kHidden)) {
    return false;
  }
  auto it = generator->strings().find("system / NativeContext");
  return it != generator->strings().end() && node->name_ == it->second;
}

}  // namespace

void RootTranslator::ReportFatal(const char* code, const char* message) {
  if (diagnostics_) {
    diagnostics_->Fatal(code, message, "root_table");
  }
}

void RootTranslator::ReportCorrupt(const char* code, const char* message,
                                   uint32_t rawheap_offset) {
  if (diagnostics_) {
    diagnostics_->CorruptInput(code, message, "root_table", 0, 0, 0, 0,
                               rawheap_offset);
  }
}

RootTranslator::RootTranslator(uint32_t table_offset, uint32_t type_count,
                               uint32_t dump_size, BinaryReaderBase* reader,
                               SnapshotGenerator* generator,
                               TranslateDiagnostics* diagnostics)
    : table_offset_(table_offset),
      type_count_(type_count),
      dump_size_(dump_size),
      reader_(reader),
      generator_(generator),
      diagnostics_(diagnostics) {
  if (reader_ == nullptr || generator_ == nullptr) {
    is_valid_ = false;
    ReportFatal("root_null_dependency",
                "RootTranslator missing reader or generator");
    return;
  }
  if (reader_->CurrentPosition() != table_offset) {
    is_valid_ = false;
    ReportCorrupt("root_offset_mismatch",
                  "reader is not positioned at root table",
                  reader_->CurrentPosition());
    return;
  }
  if (!(dump_size < std::numeric_limits<uint32_t>::max() - table_offset &&
        dump_size + table_offset <= reader->BinarySize())) {
    is_valid_ = false;
    ReportCorrupt("root_table_out_of_bounds",
                  "root table extends beyond rawheap file", table_offset);
  }
}

bool RootTranslator::Translate() {
  if (!is_valid_) {
    return false;
  }
  if (i::v8_flags.log_heapdump) {
    std::cout << "-----start root translate-----" << std::endl;
  }
  // Align with heap-snapshot-generator.cc synthetic root shape: root node "",
  // "(GC roots)", then one synthetic subroot per i::Root value.
  generator_->AddSyntheticRootNodes();
  uint32_t type{0};
  uint32_t count{0};
  const uint32_t table_end = table_offset_ + dump_size_;
  for (uint32_t i = 0; i < type_count_; ++i) {
    uint32_t root_header_offset = reader_->CurrentPosition();
    if (root_header_offset > table_end ||
        sizeof(type) + sizeof(count) > table_end - root_header_offset ||
        !reader_->ReadData(sizeof(type), reinterpret_cast<uint8_t*>(&type),
                           sizeof(type)) ||
        !reader_->ReadData(sizeof(count), reinterpret_cast<uint8_t*>(&count),
                           sizeof(count))) {
      ReportCorrupt("root_record_truncated",
                    "failed to read root type/count record",
                    root_header_offset);
      return false;
    }
    if (type >= static_cast<uint32_t>(i::Root::kNumberOfRoots)) {
      ReportCorrupt("root_type_invalid", "root type is outside V8 root range",
                    root_header_offset);
      return false;
    }
    if (!TranslateRoot(type, count)) {
      return false;
    }
  }
  if (table_end != reader_->CurrentPosition()) {
    ReportCorrupt("root_table_size_mismatch",
                  "root table did not end at declared size",
                  reader_->CurrentPosition());
    return false;
  }
  if (i::v8_flags.log_heapdump) {
    std::cout << "-----end translate-----" << std::endl;
  }
  return true;
}

bool RootTranslator::TranslateRoot(uint32_t type, uint32_t count) {
  i::Root current_root = static_cast<i::Root>(type);
  if (i::v8_flags.log_heapdump) {
    std::cout << "-----" << i::RootVisitor::RootName(current_root) << "-----"
              << std::endl;
  }
  const uint32_t table_end = table_offset_ + dump_size_;
  for (uint32_t i = 0; i < count; ++i) {
    if (reader_->CurrentPosition() > table_end ||
        sizeof(i::Address) > table_end - reader_->CurrentPosition()) {
      ReportCorrupt("root_address_truncated",
                    "failed to read root object address",
                    reader_->CurrentPosition());
      return false;
    }
    i::Address root_addr;
    if (!reader_->ReadData(sizeof(root_addr),
                           reinterpret_cast<uint8_t*>(&root_addr),
                           sizeof(root_addr))) {
      ReportCorrupt("root_address_read_failed",
                    "failed to read root object address",
                    reader_->CurrentPosition());
      return false;
    }
#ifdef OH_ENABLE_HEAP_DUMP_TEST
    if (i::v8_flags.log_heapdump) {
      roots_info_[current_root].push_back(root_addr);
      std::cout << "object_address:" << root_addr << std::endl;
    }
#endif
    // NIY:description
    bool is_weak = false;
    if (root_addr & kRawHeapWeakObjectTag) {
      root_addr = root_addr & ~kRawHeapWeakObjectTag;
      is_weak = true;
    }
    if (!is_weak) {
      // Native V8 adds user-root shortcuts only for strong NativeContext roots.
      // Root translation runs before object translation, so classify candidates
      // only after translated object nodes have been initialized.
      strong_root_candidates_.push_back(root_addr);
    }
    generator_->SetGcSubrootReference(current_root, "" /*description*/, is_weak,
                                      root_addr);
  }
  return true;
}

void RootTranslator::EmitUserGlobalReferences() {
  for (i::Address address : strong_root_candidates_) {
    if (!IsTranslatedNativeContextNode(generator_, address)) {
      continue;
    }
    if (!user_global_roots_.insert(address).second) {
      continue;
    }
    generator_->SetUserGlobalReference(address);
  }
}
}  // namespace dfx
}  // namespace v8
#endif
