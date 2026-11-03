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

#include "outputstream-writer.h"

#include "arkweb/chromium_ext/v8/dfx/heap_dump/heap-dump-log.h"
#include "src/utils/memcopy.h"

namespace v8 {
namespace dfx {
OutputStreamWriter::~OutputStreamWriter() = default;

bool OutputStreamWriter::WriteBinBlock(const uint8_t* block,
                                       uint32_t block_size) {
  uint32_t cursor = 0;
  while (block_size) {
    if (!MaybeWriteChunk()) {
      return false;
    }
    uint32_t dst_size = chunk_size_ - cur_size_;
    uint32_t write_size = std::min(dst_size, block_size);
    if (write_size > dst_size || (cur_size_ + write_size > chunk_size_)) {
      LogInfo("Fail to memcpy.");
      return false;
    }
    i::MemCopy(chunk_.data() + cur_size_, block + cursor, write_size);
    cursor += write_size;
    block_size -= write_size;
    cur_size_ += write_size;
  }
  return true;
}

bool OutputStreamWriter::MaybeWriteChunk() {
  CHECK(cur_size_ <= chunk_size_);
  if (V8_UNLIKELY(cur_size_ == chunk_size_)) {
    return WriteChunk();
  }
  return true;
}

bool OutputStreamWriter::WriteChunk() {
  CHECK(cur_size_ <= chunk_size_);
  // use v8::OutputStream to write, but not always Ascii
  v8::OutputStream::WriteResult res = stream_->WriteAsciiChunk(
      reinterpret_cast<char*>(chunk_.data()), cur_size_);
  cur_size_ = 0;
  return res == v8::OutputStream::WriteResult::kContinue;
}

void OutputStreamWriter::Flush() {
  if (!WriteChunk()) {
    LogInfo("OutputStreamWriter::Flush WriteChunk failed");
  }
}

}  // namespace dfx
}  // namespace v8
