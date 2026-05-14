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

#include "arkweb/chromium_ext/third_party/blink/renderer/core/v8/v8_code_cache_utils.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <cstdint>
#include <memory>
#include <string>

#include "arkweb/build/features/features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

#if BUILDFLAG(ARKWEB_PRECOMPILE)

namespace blink {

namespace {
constexpr uint8_t MAX_STRING_LENGTH = 255;
}  // namespace

void FuzzGenerateCodeCache(FuzzedDataProvider* fdp) {
  test::TaskEnvironment task_environment;
  auto page_holder = std::make_unique<DummyPageHolder>();
  page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);

  ScriptState* script_state =
      ToScriptStateForMainWorld(&page_holder->GetFrame());
  if (!script_state) {
    return;
  }

  std::string url_str = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  String url = String::FromUTF8(url_str.c_str());

  std::string script_str = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  String script = String::FromUTF8(script_str.c_str());

  size_t header_count = fdp->ConsumeIntegralInRange<size_t>(0, 5);
  HashMap<String, String> headers;
  for (size_t i = 0; i < header_count; ++i) {
    String key =
        String::FromUTF8(fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH).c_str());
    String value =
        String::FromUTF8(fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH).c_str());
    headers.insert(key, value);
  }

  bool is_module = fdp->ConsumeBool();
  bool is_top_level = fdp->ConsumeBool();

  V8CodeCacheUtils::CacheOptions cache_options(headers, is_module, is_top_level);

  V8CodeCacheUtils::GenerateCodeCache(script_state, url, script, cache_options);
}

}  // namespace blink

#endif  // BUILDFLAG(ARKWEB_PRECOMPILE)

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
#if BUILDFLAG(ARKWEB_PRECOMPILE)
  static blink::BlinkFuzzerTestSupport fuzzer_support(*argc, *argv);
#endif
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

#if BUILDFLAG(ARKWEB_PRECOMPILE)
  FuzzedDataProvider fdp(data, size);
  blink::FuzzGenerateCodeCache(&fdp);
#endif

  return 0;
}