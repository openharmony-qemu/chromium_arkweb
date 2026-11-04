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

#include "extensions/browser/extension_log_reporter.h"

#include "base/logging.h"
#include "base/ohos/nweb_engine_event_logger.h"
#include "base/ohos/nweb_engine_event_logger_code.h"

namespace extensions {

// static
void ExtensionLogReporter::ReportStartupFailure(const std::string& id,
                                                const base::FilePath& path,
                                                const std::string& message) {
  LOG_FEEDBACK(INFO) << "failed to load extension " << id << ",path is " << path
                     << ",message is " << message;
}

// static
void ExtensionLogReporter::ReportStartupSuccess(const std::string& id,
                                                const base::FilePath& path) {
  LOG_FEEDBACK(INFO) << "succeed to load extension " << id << ",path is "
                     << path;
}

// static
void ExtensionLogReporter::ReportInstallationFailure(
    InstallerType type,
    const std::string& id,
    const base::FilePath& path,
    const std::string& message) {
  LOG_FEEDBACK(INFO) << "failed to install extension " << id << ",type is "
                     << static_cast<int>(type) << ",path is " << path
                     << ",message is " << message;
}

// static
void ExtensionLogReporter::ReportInstallationSuccess(
    InstallerType type,
    const std::string& id,
    const base::FilePath& path) {
  LOG_FEEDBACK(INFO) << "succeed to install extension " << id << ",type is "
                     << static_cast<int>(type) << ",path is " << path;
}

// static
void ExtensionLogReporter::ReportUninstallationSuccess(UninstallReason reason,
                                                       const std::string& id) {
  LOG_FEEDBACK(INFO) << "succeed to uninstall extension " << id << ",reason is "
                     << reason;
}

}  // namespace extensions
