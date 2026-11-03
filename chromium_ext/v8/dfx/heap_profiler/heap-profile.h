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

#ifndef DFX_HEAP_PROFILE_H
#define DFX_HEAP_PROFILE_H
#include <cstdint>
namespace v8 {
class Isolate;
namespace dfx {
enum class SnapshotDumpFormat : int32_t {
  kJson = 0,
  kBinary = 1,
  kOther = 2,
};

void DumpHeapsnapshot(v8::Isolate* isolate,
                      int fd,
                      bool own_fd,
                      SnapshotDumpFormat format);

void DumpHeapsnapshot(int fd,
                      bool own_fd, bool is_raw);

}  // namespace dfx
}  // namespace v8

#endif
