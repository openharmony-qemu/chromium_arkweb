// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <cstdint>
#include <string>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/logging.h"
#include "base/i18n/icu_util.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/common/url_constants.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

// ---------------------------------------------------------------------------
// Compile-time diagnostic: emit a message so the build log shows whether
// FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION is actually defined.
// ---------------------------------------------------------------------------
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
#pragma message( \
    "FUZZ BUILD: FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION is DEFINED")
#else
#pragma message( \
    "FUZZ BUILD: FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION is NOT DEFINED")
#endif

// One-time initialization: ICU must be initialized before GURL construction,
// otherwise IDN processing in url canonicalization will crash via NOTREACHED().
extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  base::i18n::InitializeICU();
  return 0;
}

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION

namespace content_settings {

class ArkWebContentSettingsAgentImplExt;

namespace fuzz_testing {

extern bool FuzzStaticIsWhitelisted(const blink::WebSecurityOrigin& origin,
                                    const blink::WebURL& url);

}  // namespace fuzz_testing

}  // namespace content_settings

namespace {

void FuzzStaticIsWhitelistedRandom(FuzzedDataProvider* fdp) {
  std::string url_str = fdp->ConsumeRandomLengthString(256);
  GURL gurl(url_str);
  blink::WebURL web_url(gurl);
  auto origin = url::Origin::Create(gurl);
  blink::WebSecurityOrigin web_origin(origin);

  content_settings::fuzz_testing::FuzzStaticIsWhitelisted(web_origin, web_url);
}

void FuzzStaticIsWhitelistedTargeted(FuzzedDataProvider* fdp) {
  static const char* kTargetUrls[] = {
      "chrome://settings",
      "devtools://devtools",
      "file:///tmp/",
      "file:///tmp/test.txt",
      "http://example.com",
      "about:blank",
      "data:text/html,hello",
      content::kUnreachableWebDataURL,
      "chrome-error://chromewebdata/",
      "",
  };
  uint8_t idx = fdp->ConsumeIntegralInRange<uint8_t>(
      0, sizeof(kTargetUrls) / sizeof(kTargetUrls[0]) - 1);
  std::string extra = fdp->ConsumeRandomLengthString(64);
  std::string url_str = std::string(kTargetUrls[idx]) + extra;

  GURL gurl(url_str);
  blink::WebURL web_url(gurl);
  auto origin = url::Origin::Create(gurl);
  blink::WebSecurityOrigin web_origin(origin);

  content_settings::fuzz_testing::FuzzStaticIsWhitelisted(web_origin, web_url);
}

}  // namespace

#endif  // FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(uint8_t)) {
    return 0;
  }

  // Runtime diagnostic: log once whether the macro was active at compile time.
  static bool logged = false;
  if (!logged) {
    logged = true;
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    LOG(INFO) << "arkweb_content_settings_agent_impl_ext_fuzzer: "
              << "FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION is DEFINED "
              << "- testing static IsWhitelistedForContentSettings";
#else
    LOG(INFO) << "arkweb_content_settings_agent_impl_ext_fuzzer: "
              << "FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION is NOT DEFINED "
              << "- fuzzer body is empty, only macro diagnostic runs";
#endif
  }

  FuzzedDataProvider fdp(data, size);

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
  // Only test the static method IsWhitelistedForContentSettings(origin, url).
  // Instance methods (ShouldAllowlistForContentSettings,
  // IsWhitelistedForContentSettings() const, SetContentSettingRules) require a
  // valid RenderFrame, which cannot be safely created in a fuzz environment
  // without modifying the upstream ContentSettingsAgentImpl constructor.
  constexpr uint8_t kMaxScenario = 2;
#else
  // When the fuzz macro is not defined the fuzz_testing helpers are not
  // compiled, so there is nothing to exercise.
  constexpr uint8_t kMaxScenario = 0;
#endif  // FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION

  uint8_t scenario = fdp.ConsumeIntegralInRange<uint8_t>(0, kMaxScenario);

  switch (scenario) {
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    case 0:
      FuzzStaticIsWhitelistedRandom(&fdp);
      break;
    case 1:
      FuzzStaticIsWhitelistedTargeted(&fdp);
      break;
    case 2:
      FuzzStaticIsWhitelistedTargeted(&fdp);
      FuzzStaticIsWhitelistedRandom(&fdp);
      break;
#endif  // FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    default:
      break;
  }

  return 0;
}
