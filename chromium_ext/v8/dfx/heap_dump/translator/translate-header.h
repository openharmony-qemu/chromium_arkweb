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

#ifndef TRANSLATOR_TRANSLATE_HEADER_H
#define TRANSLATOR_TRANSLATE_HEADER_H

#if defined(OH_ENABLE_HEAP_TRANSLATE) || defined(OH_ENABLE_HEAP_DUMP_TEST)

#include <string>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-format.h"

namespace v8 {
namespace dfx {

class BinaryReaderBase;
class TranslateDiagnostics;

bool ReadRawHeapHeaderForTranslate(BinaryReaderBase* reader,
                                   RawHeapHeader* header,
                                   std::string* error = nullptr);
bool ValidateRawHeapHeaderForTranslate(BinaryReaderBase* reader,
                                       const RawHeapHeader& header,
                                       TranslateDiagnostics* diagnostics);
bool IsRawHeapDumpFormatVersionSupportedForTranslate(
    uint32_t dump_format_version);
std::string GetRawHeapDumpFormatVersionsForTranslate();

}  // namespace dfx
}  // namespace v8

#endif  // defined(OH_ENABLE_HEAP_TRANSLATE) ||
        // defined(OH_ENABLE_HEAP_DUMP_TEST)
#endif  // TRANSLATOR_TRANSLATE_HEADER_H
