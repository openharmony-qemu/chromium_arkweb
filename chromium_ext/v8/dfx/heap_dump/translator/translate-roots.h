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

#ifndef TRANSLATE_ROOTS_H
#define TRANSLATE_ROOTS_H

#if (defined(OH_ENABLE_HEAP_TRANSLATE) || defined(OH_ENABLE_HEAP_DUMP_TEST))

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/binary-reader-base.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-format.h"
#include "include/v8-internal.h"
#include "src/objects/visitors.h"

namespace v8 {
namespace dfx {

class SnapshotGenerator;
class TranslateDiagnostics;

class RootTranslator {
 public:
  explicit RootTranslator(uint32_t table_offset, uint32_t type_count,
                          uint32_t dump_size, BinaryReaderBase* reader,
                          SnapshotGenerator* generator,
                          TranslateDiagnostics* diagnostics);
  bool Translate();
  bool TranslateRoot(uint32_t type, uint32_t count);
  void EmitUserGlobalReferences();

 private:
  void ReportFatal(const char* code, const char* message);
  void ReportCorrupt(const char* code, const char* message,
                     uint32_t rawheap_offset);

  const uint32_t table_offset_;
  const uint32_t type_count_;
  const uint32_t dump_size_;
  BinaryReaderBase* reader_{nullptr};
  SnapshotGenerator* generator_{nullptr};
  TranslateDiagnostics* diagnostics_{nullptr};
  bool is_valid_{true};
  std::vector<i::Address> strong_root_candidates_;
  std::unordered_set<i::Address> user_global_roots_;

#ifdef OH_ENABLE_HEAP_DUMP_TEST
  // output
  std::unordered_map<i::Root, std::vector<i::Address>>
      roots_info_;
#endif
};
}  // namespace dfx
}  // namespace v8
#endif
#endif
