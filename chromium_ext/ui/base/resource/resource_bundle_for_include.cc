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

#include "arkweb/build/features/features.h"
#include <unordered_set>

#if BUILDFLAG(ARKWEB_HAP_DECOMPRESSED)
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"
#include "ohos_nweb/src/nweb_common.h"
#endif

namespace ui {

const base::NoDestructor<std::unordered_set<std::string>> supportLocaleList({
  "resources",
  "chrome_100_percent",
  "chrome_200_percent",
#ifdef WEARABLE_SUPPORT_MULTI_LANGUAGE
  "ar",
  "be",
  "bg",
  "bn",
  "cs",
  "da",
  "de",
  "el",
  "en-GB",
  "en-US",
  "es",
  "es-419",
  "et",
  "fa",
  "fi",
  "fil",
  "fr",
  "he",
  "hi",
  "hr",
  "hu",
  "id",
  "it",
  "ja",
  "ka",
  "kk",
  "km",
  "ko",
  "lt",
  "lv",
  "mk",
  "ms",
  "my",
  "nb",
  "nl",
  "pl",
  "pt-BR",
  "pt-PT",
  "ro",
  "ru",
  "sk",
  "sl",
  "sr",
  "sv",
  "th",
  "tr",
  "uk",
  "uz",
  "vi",
  "zh-CN",
  "zh-TW",
  "zh-HK",
#else
  "ar",
  "bo-CN",
  "de",
  "en-GB",
  "en-US",
  "es",
  "es-419",
  "fr",
  "id",
  "it",
  "ja",
  "ko",
  "lo",
  "ms",
  "my",
  "pl",
  "pt-BR",
  "pt-PT",
  "ru",
  "th",
  "tr",
  "ug",
  "vi",
  "zh-CN",
  "zh-TW",
  "zh-HK",
#endif
});

#if BUILDFLAG(ARKWEB_HAP_DECOMPRESSED)
bool LocaleDataPakExistsExt(std::string_view locale) {
  if (locale.empty()) {
    return false;
  }
  const auto path = ResourceBundle::GetLocaleFilePath(locale);
  // If the hap package is not decompressed, the directory does not exist.
  ScopedAllowBlockingForNwebInit allow_blocking_for_using_path;
  if (path.empty() || !base::PathExists(path)) {
    if (supportLocaleList->count(std::string(locale)) > 0) {
      return true;
    }
    const std::optional<std::string> pakLocale =
        l10n_util::CheckAndResolveLocale(locale, l10n_util::CheckLocaleMode::kUseKnownLocalesList);
    if (!pakLocale) {
      LOG(ERROR) << "CheckAndResolveLocale false, locale:" << locale;
      return false;
    }
    return supportLocaleList->count(pakLocale.value()) > 0;
  } else {
    return true;
  }
}
#endif

// static
base::FilePath ResourceBundle::GetLocaleFilePath(std::string_view locale) {
  if (locale.empty()) {
    return base::FilePath();
  }
  std::optional<std::string> pakLocale;
  if (supportLocaleList->count(std::string(locale)) > 0) {
    pakLocale = locale;
  } else {
    pakLocale = l10n_util::CheckAndResolveLocale(locale, l10n_util::CheckLocaleMode::kUseKnownLocalesList);
    if (!pakLocale) {
      LOG(ERROR) << "CheckAndResolveLocale false, locale:" << locale;
      return base::FilePath();
    }
  }

  base::FilePath locale_file_path;

  if (base::PathService::Get(base::DIR_ASSETS, &locale_file_path)) {
    std::string locale_path = "locales/";
    locale_path += pakLocale.value();
    locale_path += ".pak";
    locale_file_path = locale_file_path.AppendASCII(locale_path);
  }

  LOG(INFO) << "GetLocaleFilePath pakLocale:" << pakLocale.value();
  // Don't try to load from paths that are not absolute.
  return locale_file_path.IsAbsolute() ? locale_file_path : base::FilePath();
}

}  // namespace ui
