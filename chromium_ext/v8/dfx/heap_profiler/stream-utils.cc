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

#include "stream-utils.h"

#include <fcntl.h>
#include <src/base/logging.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "arkweb/chromium_ext/v8/ohlog.h"

namespace v8 {
namespace dfx {
v8::OutputStream::WriteResult SnapshotOutputStream::WriteAsciiChunk(char* data,
                                                                    int size) {
  if (fd_ < 0) {
    StreamHilog("WriteAsciiChunk Abort");
    return kAbort;
  }
  write_bytes_ += size;
  while (size > 0) {
#ifdef USING_OHOS_WEB
    ssize_t n = TEMP_FAILURE_RETRY(send(fd_, data, size, MSG_NOSIGNAL));
#else
    ssize_t n = TEMP_FAILURE_RETRY(write(fd_, data, size));
#endif
    if (n <= 0) {
      StreamHilog("WriteAsciiChunk Abort:n <= 0");
      return kAbort;
    }
    data += n;
    size -= static_cast<size_t>(n);
  }
  return kContinue;
}

SnapshotOutputStream::~SnapshotOutputStream() {
  StreamHilog("total write " + std::to_string(write_bytes_));
  EndOfStream();
}

void SnapshotOutputStream::EndOfStream() {
  if (own_fd_ && fd_ >= 0) {
    close(fd_);
    StreamHilog("EndOfStream::close(fd) " + std::to_string(fd_));
  }
  fd_ = -1;
}

}  // namespace dfx
}  // namespace v8
