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

#ifndef EXTENSIONS_BROWSER_EXTENSION_LOG_REPORTER_H_
#define EXTENSIONS_BROWSER_EXTENSION_LOG_REPORTER_H_

#include <string>

#include "base/files/file_path.h"
#include "extensions/browser/uninstall_reason.h"

namespace extensions {

enum class InstallerType { INSTALLER_TYPE_CRX = 0, INSTALLER_TYPE_DIR };

class ExtensionLogReporter {
 public:
  static void ReportStartupFailure(const std::string& id,
                                   const base::FilePath& path,
                                   const std::string& message);

  static void ReportStartupSuccess(const std::string& id,
                                   const base::FilePath& path);

  static void ReportInstallationFailure(InstallerType type,
                                        const std::string& id,
                                        const base::FilePath& path,
                                        const std::string& message);

  static void ReportInstallationSuccess(InstallerType type,
                                        const std::string& id,
                                        const base::FilePath& path);

  static void ReportUninstallationSuccess(UninstallReason reason,
                                          const std::string& id);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_LOG_REPORTER_H_
