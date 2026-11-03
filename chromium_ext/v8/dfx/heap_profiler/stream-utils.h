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

#ifndef STREAM_UTILS_H
#define STREAM_UTILS_H

#include <errno.h>
#include <fcntl.h>

#include "include/v8-profiler.h"

namespace v8 {
namespace dfx {
class SnapshotOutputStream : public v8::OutputStream {
 public:
  // maybe fd is socket
  explicit SnapshotOutputStream(int fd, bool own_fd)
      : fd_(fd), own_fd_{own_fd} {}
  ~SnapshotOutputStream() override;
  WriteResult WriteAsciiChunk(char* data, int size) override;
  void EndOfStream() override;

 private:
  int fd_{-1};
  bool own_fd_{false};
  uint32_t write_bytes_{0};
};

}  // namespace dfx
}  // namespace v8

#endif
