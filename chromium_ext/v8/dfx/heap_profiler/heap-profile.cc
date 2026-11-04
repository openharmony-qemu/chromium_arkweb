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

#include "heap-profile.h"

#include "arkweb/chromium_ext/v8/dfx/heap_dump/heap-dump.h"
#include "arkweb/chromium_ext/v8/ohlog.h"
#include "include/v8-profiler.h"
#include "src/execution/isolate.h"
#include "stream-utils.h"

namespace v8 {
namespace dfx {

void DumpHeapsnapshot(v8::Isolate* isolate,
                      int fd,
                      bool own_fd,
                      SnapshotDumpFormat format) {
  HeapProfiler* profiler = isolate->GetHeapProfiler();
  SnapshotOutputStream stream(fd, own_fd);

  if (format == SnapshotDumpFormat::kJson) {
    const HeapSnapshot* snapshot = profiler->TakeHeapSnapshot();
    snapshot->Serialize(&stream);
  } else if (format == SnapshotDumpFormat::kBinary) {
    dfx::DumpRawHeap(reinterpret_cast<i::Isolate*>(isolate)->heap(), &stream);
  }

  StreamHilog("dfx::DumpHeapsnapshot exit");
}

void DumpHeapsnapshot(int fd, bool own_fd, bool is_raw) {
  v8::Isolate* isolate = v8::Isolate::TryGetCurrent();
  if (isolate) {
    DumpHeapsnapshot(
        isolate, fd, own_fd,
        is_raw ? SnapshotDumpFormat::kBinary : SnapshotDumpFormat::kJson);
  } else if (own_fd && fd >= 0) {
    StreamHilog("dfx::DumpHeapsnapshot empty isolate");
    close(fd);
  }
}
}  // namespace dfx
}  // namespace v8
