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

#if defined(OH_ENABLE_HEAP_TRANSLATE)

#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/binary-reader.h"

#include <cstdint>
#include <vector>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-format.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/heap-dump-log.h"
#include "src/base/logging.h"
#include "src/flags/flags.h"

namespace v8 {
namespace dfx {
BinaryReader::BinaryReader(std::string path)
    : path_(path), file_(path, std::ios::binary | std::ios::in) {
  if (file_.is_open()) {
    file_.seekg(0, std::ios::end);
    if (!file_.good()) {
      LogInfo("Failed to seek rawheap file end.");
      file_.close();
      return;
    }
    std::streampos tmp_size = file_.tellg();
    if (tmp_size < 0 || tmp_size > static_cast<std::streampos>(
                                       std::numeric_limits<uint32_t>::max())) {
      LogInfo("Rawheap file is too large for this reader.");
      file_.close();
      return;
    }
    file_size_ = static_cast<uint32_t>(tmp_size);
    if (i::v8_flags.log_heapdump) {
      std::cout << "binary reader file size:" << file_size_ << std::endl;
    }
    file_.seekg(0, std::ios::beg);
    if (!file_.good()) {
      LogInfo("Failed to seek rawheap file begin.");
      file_.close();
      file_size_ = 0;
      return;
    }
  } else {
    LogInfo("Failed to open file!");
  }
}

bool BinaryReader::IsValid() const { return file_.is_open(); }

uint32_t BinaryReader::BinarySize() const { return file_size_; }

bool BinaryReader::ReadData(uint32_t read_size, uint8_t* out,
                            uint32_t out_size) {
  if (!IsValid() || !file_.good() || out_size < read_size) {
    return false;
  }
  std::streampos offset = file_.tellg();
  if (offset < 0 || (static_cast<uint32_t>(offset) > file_size_ ||
                     read_size > file_size_ - static_cast<uint32_t>(offset))) {
    return false;
  }

  file_.read(reinterpret_cast<char*>(out), read_size);
  if (file_.gcount() != static_cast<std::streamsize>(read_size)) {
    return false;
  }
  return true;
}

bool BinaryReader::ReadDataAt(uint32_t offset, uint32_t read_size, uint8_t* out,
                              uint32_t out_size) {
  if (!IsValid() || !file_.good()) {
    return false;
  }
  // record
  std::streampos cur_pos = file_.tellg();
  if (cur_pos < 0 || offset > file_size_ || read_size > file_size_ - offset) {
    return false;
  }
  if (out_size < read_size) {
    return false;
  }

  file_.seekg(offset, std::ios::beg);
  if (!file_.good()) {
    file_.clear();
    file_.seekg(cur_pos, std::ios::beg);
    return false;
  }
  file_.read(reinterpret_cast<char*>(out), read_size);
  if (file_.gcount() != static_cast<std::streamsize>(read_size)) {
    file_.clear();
    file_.seekg(cur_pos, std::ios::beg);
    return false;
  }
  // restore
  file_.seekg(cur_pos, std::ios::beg);
  if (!file_.good()) {
    return false;
  }
  return true;
}

uint32_t BinaryReader::CurrentPosition() {
  std::streampos tmp_size = file_.tellg();
  if (tmp_size < 0 || tmp_size > static_cast<std::streampos>(
                                     std::numeric_limits<uint32_t>::max())) {
    return file_size_;
  }
  return static_cast<uint32_t>(tmp_size);
}

void BinaryReader::SetPosition(uint32_t pos) {
  if (!IsValid() || pos > file_size_) {
    return;
  }
  file_.seekg(pos, std::ios::beg);
}

BinaryReader::~BinaryReader() {
  if (IsValid()) {
    file_.close();
  }
}
}  // namespace dfx
}  // namespace v8
#endif
