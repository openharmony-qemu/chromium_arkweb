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

#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-diagnostics.h"

#include <iostream>
#include <sstream>

namespace v8 {
namespace dfx {
namespace {

int SeverityRank(TranslateDiagnostics::Severity severity) {
  switch (severity) {
    case TranslateDiagnostics::Severity::kWarning:
      return 0;
    case TranslateDiagnostics::Severity::kUnsupported:
      return 1;
    case TranslateDiagnostics::Severity::kCorruptInput:
      return 2;
    case TranslateDiagnostics::Severity::kFatal:
      return 3;
  }
  return -1;
}

bool SameDiagnosticKey(const TranslateDiagnostics::Entry& entry,
                       TranslateDiagnostics::Severity severity,
                       const std::string& code,
                       const std::string& section) {
  return entry.severity == severity && entry.code == code &&
         entry.section == section;
}

}  // namespace

void TranslateDiagnostics::Add(Severity severity, std::string code,
                               std::string message, std::string section,
                               uint64_t object_address, uint64_t map_address,
                               uint32_t instance_type, uint32_t visitor_id,
                               uint32_t rawheap_offset) {
  issue_count_++;
  switch (severity) {
    case Severity::kWarning:
      warning_count_++;
      break;
    case Severity::kUnsupported:
      unsupported_count_++;
      break;
    case Severity::kCorruptInput:
      corrupt_count_++;
      break;
    case Severity::kFatal:
      fatal_count_++;
      break;
  }
  Entry entry{severity,       std::move(code), std::move(message),
              std::move(section), object_address, map_address,
              instance_type,  visitor_id,       rawheap_offset};
  // Keep only a bounded diagnostic sample. A corrupt or partially unsupported
  // rawheap can produce one issue per object; keeping every entry would make an
  // error path consume memory proportional to the rawheap size. Store one
  // sample per diagnostic key, and prefer higher-severity entries if the
  // bounded sample is already full.
  for (const Entry& sampled : entries_) {
    if (SameDiagnosticKey(sampled, entry.severity, entry.code,
                          entry.section)) {
      return;
    }
  }

  if (entries_.size() < kMaxStoredEntries) {
    entries_.push_back(std::move(entry));
    return;
  }

  const int new_rank = SeverityRank(entry.severity);
  for (Entry& sampled : entries_) {
    if (SeverityRank(sampled.severity) < new_rank) {
      sampled = std::move(entry);
      return;
    }
  }
}

void TranslateDiagnostics::Warning(std::string code, std::string message,
                                   std::string section, uint64_t object_address,
                                   uint64_t map_address, uint32_t instance_type,
                                   uint32_t visitor_id,
                                   uint32_t rawheap_offset) {
  Add(Severity::kWarning, std::move(code), std::move(message),
      std::move(section), object_address, map_address, instance_type,
      visitor_id, rawheap_offset);
}

void TranslateDiagnostics::Unsupported(
    std::string code, std::string message, std::string section,
    uint64_t object_address, uint64_t map_address, uint32_t instance_type,
    uint32_t visitor_id, uint32_t rawheap_offset) {
  Add(Severity::kUnsupported, std::move(code), std::move(message),
      std::move(section), object_address, map_address, instance_type,
      visitor_id, rawheap_offset);
}

void TranslateDiagnostics::CorruptInput(
    std::string code, std::string message, std::string section,
    uint64_t object_address, uint64_t map_address, uint32_t instance_type,
    uint32_t visitor_id, uint32_t rawheap_offset) {
  Add(Severity::kCorruptInput, std::move(code), std::move(message),
      std::move(section), object_address, map_address, instance_type,
      visitor_id, rawheap_offset);
}

void TranslateDiagnostics::Fatal(std::string code, std::string message,
                                 std::string section, uint64_t object_address,
                                 uint64_t map_address, uint32_t instance_type,
                                 uint32_t visitor_id, uint32_t rawheap_offset) {
  Add(Severity::kFatal, std::move(code), std::move(message), std::move(section),
      object_address, map_address, instance_type, visitor_id, rawheap_offset);
}

std::string TranslateDiagnostics::FormatEntry(const Entry& entry) const {
  std::stringstream ss;
  ss << SeverityName(entry.severity) << " " << entry.code << ": "
     << entry.message;
  if (!entry.section.empty()) {
    ss << " section=" << entry.section;
  }
  if (entry.rawheap_offset != 0) {
    ss << " rawheap_offset=" << entry.rawheap_offset;
  }
  if (entry.object_address != 0) {
    ss << " object=0x" << std::hex << entry.object_address << std::dec;
  }
  if (entry.map_address != 0) {
    ss << " map=0x" << std::hex << entry.map_address << std::dec;
  }
  if (entry.instance_type != 0) {
    ss << " instance_type=" << entry.instance_type;
  }
  if (entry.visitor_id != 0) {
    ss << " visitor_id=" << entry.visitor_id;
  }
  return ss.str();
}

void TranslateDiagnostics::PrintSummary(const char* prefix) const {
  if (entries_.empty()) {
    return;
  }
  std::cerr << prefix << " diagnostics: warnings=" << warning_count_
            << " unsupported=" << unsupported_count_
            << " corrupt_input=" << corrupt_count_ << " fatal=" << fatal_count_
            << std::endl;

  // Print a bounded sample.  Real rawheap corruption can cascade into many
  // repeated failures, and the first few entries usually identify the root
  // mismatch without flooding stderr.
  constexpr size_t kMaxEntriesToPrint = 50;
  size_t printed = 0;
  for (const Entry& entry : entries_) {
    if (printed >= kMaxEntriesToPrint) {
      break;
    }
    std::cerr << prefix << " " << FormatEntry(entry) << std::endl;
    printed++;
  }
  if (issue_count_ > printed) {
    std::cerr << prefix << " " << (issue_count_ - printed)
              << " additional diagnostics suppressed" << std::endl;
  }
}

const char* TranslateDiagnostics::SeverityName(Severity severity) {
  switch (severity) {
    case Severity::kWarning:
      return "warning";
    case Severity::kUnsupported:
      return "unsupported";
    case Severity::kCorruptInput:
      return "corrupt_input";
    case Severity::kFatal:
      return "fatal";
  }
  return "unknown";
}

}  // namespace dfx
}  // namespace v8
#endif
