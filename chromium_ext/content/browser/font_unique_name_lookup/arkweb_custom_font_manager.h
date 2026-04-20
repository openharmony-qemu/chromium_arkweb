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

#ifndef CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_ARKWEB_CUSTOM_FONT_MANAGER_H_
#define CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_ARKWEB_CUSTOM_FONT_MANAGER_H_

#include <map>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "ohos_nweb/src/capi/nweb_custom_font_info.h"

class ArkwebCustomFontManager {
public:
  static ArkwebCustomFontManager *GetInstance();

  bool SetCustomFonts(const NWebCustomFontInfoVector &customFonts);

  ::base::File GetFontByUniqueName(const std::string &name);
  std::vector<std::string> GetAllCustomFontsUniqueNames();
  base::flat_map<std::string, ::base::File> GetAllCustomFonts();

private:
  friend class base::NoDestructor<ArkwebCustomFontManager>;
  ArkwebCustomFontManager() = default;
  ~ArkwebCustomFontManager() = default;
  ArkwebCustomFontManager(const ArkwebCustomFontManager &) = delete;
  ArkwebCustomFontManager &operator=(const ArkwebCustomFontManager &) = delete;

  std::map<std::string, ::base::FilePath> GetCustomFonts();
  bool GetCustomFontPath(const std::string &fontName,
                         ::base::FilePath &fontPath);

  base::Lock lock_;
  std::map<std::string, ::base::FilePath> arkweb_custom_fonts_;
};
#endif  // CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_ARKWEB_CUSTOM_FONT_MANAGER_H_
