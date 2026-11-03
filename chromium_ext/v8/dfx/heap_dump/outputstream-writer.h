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

#ifndef OUTPUTSTREAM_WRITER_H
#define OUTPUTSTREAM_WRITER_H
#include "arkweb/chromium_ext/v8/dfx/heap_dump/binary-writer-base.h"
#include "include/v8-profiler.h"
#include "src/base/logging.h"

namespace v8 {
namespace dfx {
class OutputStreamWriter : public BinaryWriterBase {
 public:
  explicit OutputStreamWriter(v8::OutputStream* stream)
      : stream_(stream),
        chunk_size_(stream_->GetChunkSize()),
        chunk_(chunk_size_) {}
  ~OutputStreamWriter() override;
  void OpenFile(const std::string& path) override { UNREACHABLE(); }
  const std::string& GetFilePath() override { UNREACHABLE(); }
  bool WriteBinBlock(const uint8_t* block, uint32_t block_size) override;
  void CloseFile() override { UNREACHABLE(); }
  void Flush() override;

 private:
  bool MaybeWriteChunk() override;
  bool WriteChunk() override;

  v8::OutputStream* stream_{nullptr};
  const uint32_t chunk_size_;

  uint32_t cur_size_{0};
  std::vector<uint8_t> chunk_;
};
}  // namespace dfx
}  // namespace v8
#endif
