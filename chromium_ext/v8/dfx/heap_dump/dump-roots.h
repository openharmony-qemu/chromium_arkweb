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

#ifndef DUMP_ROOTS_H
#define DUMP_ROOTS_H

#if defined(OH_ENABLE_HEAP_DUMP)
#include <set>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/binary-writer-base.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-format.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/heap-dump-log.h"
#include "src/heap/combined-heap.h"
#include "src/heap/heap-visitor-inl.h"
#include "src/heap/heap.h"
#include "src/objects/visitors.h"

namespace v8 {
namespace dfx {

class RootDumper {
 public:
  explicit RootDumper(i::Heap* heap, BinaryWriterBase* writer);
  ~RootDumper() = default;
  inline void AddRoot(i::Root root, i::Address addr, bool is_weak);

  void PreVisit();

  void Dump();

  uint32_t RootKindCount() const;  // root_kind_count_
  uint32_t RootDumpSize() const;   // root_dump_size_

 private:
  uint32_t root_kind_count_{0};
  uint32_t root_dump_size_{0};

  i::Heap* heap_;
  BinaryWriterBase* writer_{nullptr};
  std::unordered_map<i::Root, std::set<i::Address>> roots_info_;
};
}  // namespace dfx
}  // namespace v8
#endif

#endif
