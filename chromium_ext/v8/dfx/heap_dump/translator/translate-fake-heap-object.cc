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

#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-fake-heap-object.h"

#include <new>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/binary-reader-base.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-format.h"
#include "src/base/logging.h"

namespace v8 {
namespace dfx {

FakeHeapObject::FakeHeapObject(BinaryReaderBase* reader, uint32_t offset,
                               uint32_t size)
    : size_(size) {
  if (reader == nullptr || offset == 0 || size == 0) {
    return;
  }
  data_ = reinterpret_cast<uint8_t*>(::operator new(size, std::align_val_t{8}));
  if (!reader->ReadDataAt(offset, size, data_, size)) {
    ::operator delete(data_, std::align_val_t{8});
    data_ = nullptr;
    return;
  }
  valid_ = true;
}

FakeHeapObject::FakeHeapObject(BinaryReaderBase* reader, uint32_t size)
    : size_(size) {
  if (reader == nullptr || size == 0) {
    return;
  }
  data_ = reinterpret_cast<uint8_t*>(::operator new(size, std::align_val_t{8}));
  if (!reader->ReadData(size, data_, size)) {
    ::operator delete(data_, std::align_val_t{8});
    data_ = nullptr;
    return;
  }
  valid_ = true;
}

FakeHeapObject::~FakeHeapObject() {
  if (data_ != nullptr) {
    ::operator delete(data_, std::align_val_t{8});
    data_ = nullptr;
  }
}

i::Address FakeHeapObject::fake_address() {
  if (!valid_) {
    return 0;
  }
  i::Address addr = reinterpret_cast<i::Address>(data_);
  CHECK(!(addr & 0x7));
  return addr;
}

i::Tagged<i::HeapObject> FakeHeapObject::fake_object() {
  return i::HeapObject::FromAddress(fake_address());
}

bool FakeHeapObject::TryReadNoContentRealSize(uint32_t* real_size) const {
  if (!valid_ || real_size == nullptr || size_ != kNoContentObjectDumpSize) {
    return false;
  }
  *real_size = *reinterpret_cast<uint32_t*>(data_ + sizeof(i::Tagged_t));
  return true;
}

}  // namespace dfx
}  // namespace v8

#endif
