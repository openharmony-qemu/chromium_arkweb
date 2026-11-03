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

#include "arkweb/chromium_ext/third_party/blink/renderer/platform/fonts/font_platform_data_utils.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <cstdint>
#include <memory>
#include <string>

#include "arkweb/build/features/features.h"
#include "base/command_line.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"

#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)

namespace blink {

namespace {
constexpr uint8_t MAX_STRING_LENGTH = 255;
}  // namespace

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();
  }

  ~Environment() {}

  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

void FuzzFindCompressionConfigWithFont(FuzzedDataProvider* fdp) {
  FontPlatformData* font_data = MakeGarbageCollected<FontPlatformData>();
  FontPlatformDataUtils* utils =
      MakeGarbageCollected<FontPlatformDataUtils>(font_data);

  std::string family_name = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);

  const ScaleParam& result = utils->FindCompressionConfigWithFont(family_name);
}

}  // namespace blink

#endif  // BUILDFLAG(ARKWEB_COMPOSITE_RENDER)

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
  static blink::Environment env;

  FuzzedDataProvider fdp(data, size);

  blink::FuzzFindCompressionConfigWithFont(&fdp);
#endif

  return 0;
}