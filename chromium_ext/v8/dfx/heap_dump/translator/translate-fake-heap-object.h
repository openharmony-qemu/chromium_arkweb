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

#ifndef TRANSLATE_FAKE_HEAP_OBJECT_H
#define TRANSLATE_FAKE_HEAP_OBJECT_H

#if (defined(OH_ENABLE_HEAP_TRANSLATE) || defined(OH_ENABLE_HEAP_DUMP_TEST))

#include <cstdint>

#include "src/objects/heap-object.h"

namespace v8 {
namespace dfx {

class BinaryReaderBase;

class FakeHeapObject {
 public:
  explicit FakeHeapObject(BinaryReaderBase* reader, uint32_t offset,
                          uint32_t size);
  explicit FakeHeapObject(BinaryReaderBase* reader, uint32_t size);
  ~FakeHeapObject();

  i::Address fake_address();
  i::Tagged<i::HeapObject> fake_object();
  bool TryReadNoContentRealSize(uint32_t* real_size) const;
  bool is_valid() const { return valid_; }

 private:
  uint8_t* data_{nullptr};
  const uint32_t size_;
  bool valid_{false};
};

}  // namespace dfx
}  // namespace v8

#endif
#endif
