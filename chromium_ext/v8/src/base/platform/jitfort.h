/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#ifndef V8_BASE_PLATFORM_JITFORT_H_
#define V8_BASE_PLATFORM_JITFORT_H_

#include "include/v8-platform.h"

namespace v8 {
namespace base {

class JITFort {
public:
    static void SetJITMemoryWritable();
    static void SetJITMemoryExecutable();
    static void Init();
    static void EnableJitfort();
};

class RwxMemoryNonWriteScope {
public:
  explicit RwxMemoryNonWriteScope(const char* comment);
~RwxMemoryNonWriteScope();

  // Disable copy constructor and copy-assignment operator, since this manages
  // a resource and implicit copying of the scope can yield surprising errors.
  RwxMemoryNonWriteScope(const RwxMemoryNonWriteScope&) = delete;
  RwxMemoryNonWriteScope& operator=(const RwxMemoryNonWriteScope&) = delete;
};
}
}

#endif // !V8_BASE_PLATFORM_JITFORT_H_