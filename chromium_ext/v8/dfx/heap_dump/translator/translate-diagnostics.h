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

#ifndef TRANSLATE_DIAGNOSTICS_H
#define TRANSLATE_DIAGNOSTICS_H

#if (defined(OH_ENABLE_HEAP_TRANSLATE) || defined(OH_ENABLE_HEAP_DUMP_TEST))

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace v8 {
namespace dfx {

// Lightweight diagnostics for translate-v1.
//
// User-provided rawheap files can be incomplete or corrupt, so the translator
// reports friendly diagnostics for container boundaries such as the header,
// metadata, root table, and object table offsets/sizes.  After those tables are
// validated, object-body parsing relies on the dump contract that all required
// heap objects were exported.  Failures in that phase are treated as
// dumper/translator bugs and may remain CHECK/UNREACHABLE invariants instead of
// best-effort diagnostics.
class TranslateDiagnostics {
 public:
  enum class Severity {
    kWarning,
    kUnsupported,
    kCorruptInput,
    kFatal,
  };

  struct Entry {
    Severity severity;
    std::string code;
    std::string message;
    std::string section;
    uint64_t object_address{0};
    uint64_t map_address{0};
    uint32_t instance_type{0};
    uint32_t visitor_id{0};
    uint32_t rawheap_offset{0};
  };

  void Add(Severity severity, std::string code, std::string message,
           std::string section = std::string(), uint64_t object_address = 0,
           uint64_t map_address = 0, uint32_t instance_type = 0,
           uint32_t visitor_id = 0, uint32_t rawheap_offset = 0);

  void Warning(std::string code, std::string message,
               std::string section = std::string(), uint64_t object_address = 0,
               uint64_t map_address = 0, uint32_t instance_type = 0,
               uint32_t visitor_id = 0, uint32_t rawheap_offset = 0);
  void Unsupported(std::string code, std::string message,
                   std::string section = std::string(),
                   uint64_t object_address = 0, uint64_t map_address = 0,
                   uint32_t instance_type = 0, uint32_t visitor_id = 0,
                   uint32_t rawheap_offset = 0);
  void CorruptInput(std::string code, std::string message,
                    std::string section = std::string(),
                    uint64_t object_address = 0, uint64_t map_address = 0,
                    uint32_t instance_type = 0, uint32_t visitor_id = 0,
                    uint32_t rawheap_offset = 0);
  void Fatal(std::string code, std::string message,
             std::string section = std::string(), uint64_t object_address = 0,
             uint64_t map_address = 0, uint32_t instance_type = 0,
             uint32_t visitor_id = 0, uint32_t rawheap_offset = 0);

  bool has_fatal() const { return fatal_count_ != 0; }
  bool has_errors() const { return fatal_count_ != 0 || corrupt_count_ != 0; }
  size_t issue_count() const { return issue_count_; }
  const std::vector<Entry>& entries() const { return entries_; }

  std::string FormatEntry(const Entry& entry) const;
  void PrintSummary(const char* prefix = "[heapdump-translate]") const;

 private:
  static constexpr size_t kMaxStoredEntries = 50;
  static const char* SeverityName(Severity severity);

  std::vector<Entry> entries_;
  size_t issue_count_{0};
  size_t warning_count_{0};
  size_t unsupported_count_{0};
  size_t corrupt_count_{0};
  size_t fatal_count_{0};
};

}  // namespace dfx
}  // namespace v8
#endif
#endif
