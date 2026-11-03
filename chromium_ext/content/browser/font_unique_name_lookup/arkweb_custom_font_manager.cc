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

#include "arkweb/chromium_ext/content/browser/font_unique_name_lookup/arkweb_custom_font_manager.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "third_party/blink/public/common/font_unique_name_lookup/icu_fold_case_util.h"

namespace {
constexpr size_t kCustomFontsCountLimit = 16;
}

// static
ArkwebCustomFontManager *ArkwebCustomFontManager::GetInstance() {
  static base::NoDestructor<ArkwebCustomFontManager> instance;
  return instance.get();
}

bool ArkwebCustomFontManager::SetCustomFonts(
    const NWebCustomFontInfoVector &customFonts) {
  LOG(DEBUG) << "set custom font size " << customFonts.size();
  base::AutoLock lock(lock_);
  arkweb_custom_fonts_.clear();
  for (auto &customFont : customFonts) {
    if (arkweb_custom_fonts_.size() >= kCustomFontsCountLimit) {
      LOG(WARNING) << "custom fonts over limit: " << kCustomFontsCountLimit;
      break;
    }
    ::base::FilePath file_path = ::base::FilePath(customFont.fontPath);
    if (file_path.Extension() == ".ttf" || file_path.Extension() == ".ttc" ||
        file_path.Extension() == ".otf") {
      LOG(DEBUG) << "add custom font {" << customFont.fontName << ": "
                 << customFont.fontPath << "}";
      arkweb_custom_fonts_.emplace(blink::IcuFoldCase(customFont.fontName),
                                   std::move(file_path));
    } else {
      LOG(WARNING) << "invalid font format: " << customFont.fontPath;
      continue;
    }
  }
  if (!customFonts.empty() && arkweb_custom_fonts_.empty()) {
    LOG(ERROR) << "set custom fonts failed";
    return false;
  }
  return true;
}

std::map<std::string, ::base::FilePath>
ArkwebCustomFontManager::GetCustomFonts() {
  base::AutoLock lock(lock_);
  return arkweb_custom_fonts_;
}

bool ArkwebCustomFontManager::GetCustomFontPath(const std::string &fontName,
                                                ::base::FilePath &fontPath) {
  base::AutoLock lock(lock_);
  auto iter = arkweb_custom_fonts_.find(fontName);
  if (iter != arkweb_custom_fonts_.end()) {
    fontPath = iter->second;
    return true;
  } else {
    return false;
  }
}

std::vector<std::string>
ArkwebCustomFontManager::GetAllCustomFontsUniqueNames() {
  auto customFonts = GetCustomFonts();
  std::vector<std::string> names;
  names.reserve(customFonts.size());
  std::transform(customFonts.begin(), customFonts.end(),
                 std::back_inserter(names),
                 [](const auto &pair) { return pair.first; });
  return names;
}

::base::File
ArkwebCustomFontManager::GetFontByUniqueName(const std::string &name) {
  ::base::FilePath fontPath;
  if (GetCustomFontPath(name, fontPath)) {
    ::base::File fontFile(fontPath,
                          ::base::File::FLAG_OPEN | ::base::File::FLAG_READ);
    if (fontFile.IsValid()) {
      LOG(DEBUG) << "found valid font file for " << name;
      return fontFile;
    } else {
      LOG(DEBUG) << "found invalid font file: " << fontPath.value();
    }
  }
  return ::base::File();
}

base::flat_map<std::string, ::base::File>
ArkwebCustomFontManager::GetAllCustomFonts() {
  auto customFonts = GetCustomFonts();
  base::flat_map<std::string, ::base::File> fonts;
  for (auto iter = customFonts.begin(); iter != customFonts.end(); iter++) {
    ::base::File fontFile(iter->second,
                          ::base::File::FLAG_OPEN | ::base::File::FLAG_READ);
    if (fontFile.IsValid()) {
      fonts.emplace(iter->first, std::move(fontFile));
    } else {
      LOG(DEBUG) << "invalid font file: " << iter->second.value();
    }
  }
  LOG(DEBUG) << "all custom font size " << fonts.size();
  return fonts;
}
