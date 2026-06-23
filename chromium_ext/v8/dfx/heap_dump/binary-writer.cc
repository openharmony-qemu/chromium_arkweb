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

#if defined(OH_ENABLE_HEAP_DUMP) &&                                            \
    (defined(OH_ENABLE_HEAP_TRANSLATE) || defined(OH_ENABLE_HEAP_DUMP_TEST) || \
     defined(USING_OHOS))

#include "arkweb/chromium_ext/v8/dfx/heap_dump/binary-writer.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "arkweb/chromium_ext/v8/dfx/heap_dump/heap-dump-log.h"

namespace v8 {
namespace dfx {
BinaryWriter::BinaryWriter() : BinaryWriter(kChunkSize) {}
BinaryWriter::BinaryWriter(const uint32_t chunk_size)
    : chunk_size_(chunk_size), chunk_(chunk_size) {}

BinaryWriter::~BinaryWriter() {
  CloseFile();
}

bool BinaryWriter::WriteBinBlock(const uint8_t* block, uint32_t block_size) {
  if (fd_ < 0) {
    LogInfo("fd_ didn't open");
    return false;
  }
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
    memcpy(chunk_.data() + cur_size_, block + cursor, write_size);
    cursor += write_size;
    block_size -= write_size;
    cur_size_ += write_size;
  }
  return true;
}

bool BinaryWriter::MaybeWriteChunk() {
  CHECK(cur_size_ <= chunk_size_);
  if (V8_UNLIKELY(cur_size_ == chunk_size_)) {
    return WriteChunk();
  }
  return true;
}

bool BinaryWriter::Write() {
  uint8_t* data = chunk_.data();
  while (cur_size_) {
#if defined(_WIN32)
    DWORD written = 0;
    HANDLE hFile = reinterpret_cast<HANDLE>(static_cast<intptr_t>(fd_));
    BOOL result = WriteFile(hFile, data, cur_size_, &written, nullptr);
    CHECK(result && written > 0);
    data += static_cast<size_t>(written);
    cur_size_ -= static_cast<size_t>(written);
#else
    ssize_t r = ::write(fd_, data, cur_size_);
    CHECK(r > 0);
    data += static_cast<size_t>(r);
    cur_size_ -= static_cast<size_t>(r);
#endif
  }
  return true;
}

bool BinaryWriter::WriteChunk() {
  if (fd_ < 0) {
    LogInfo("fd_ didn't open");
    return false;
  }
  CHECK(cur_size_ == chunk_size_);
  return Write();
}

void BinaryWriter::OpenFile(const std::string& path) {
  if (fd_ >= 0) {
    LogInfo("HeapDump already opened!");
    return;
  }
  path_ = path;
#if defined(_WIN32)
  fd_ = static_cast<int>(reinterpret_cast<intptr_t>(CreateFileA(path.c_str(), GENERIC_WRITE,
                                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                nullptr, CREATE_ALWAYS,
                                                FILE_ATTRIBUTE_NORMAL, nullptr)));
  if (fd_ < 0) {
    LogInfo("HeapDump open file failed!");
    CHECK(false);
  }
#else
  fd_ = ::open(path.c_str(),
               O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC,
               S_IRUSR | S_IWUSR);
  if (fd_ < 0) {
    LogInfo("HeapDump open file failed!");
    CHECK(false);
  }
#endif
}

const std::string& BinaryWriter::GetFilePath() {
  return path_;
}

void BinaryWriter::CloseFile() {
  if (fd_ < 0) {
    return;
  }
  Flush();
#if defined(_WIN32)
  CloseHandle(reinterpret_cast<HANDLE>(static_cast<intptr_t>(fd_)));
#else
  ::close(fd_);
#endif
  fd_ = -1;
}

void BinaryWriter::Flush() {
  // write rest
  if (cur_size_) {
    Write();
  }
}

}  // namespace dfx
}  // namespace v8
#endif
