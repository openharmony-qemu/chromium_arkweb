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

#include "nweb_image_change_detection.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ohos_resources/image_change_detection/grit/image_change_detection_resources.h"
#include "ohos_resources/dom_tree_extractor/grit/dom_tree_extractor_resources.h"

namespace OHOS::NWeb {

constexpr std::string_view kStartTemplate = "(function(window) { window.arkImageDetector.start(); })(window);";
constexpr std::string_view kStopTemplate = "(function(window) { window.arkImageDetector.stop(); })(window);";

NWebImageChangeDetection::NWebImageChangeDetection(
    base::WeakPtr<NWebDelegate> nweb_delegate)
    : nweb_delegate_(nweb_delegate) {}

void NWebImageChangeDetection::SetImageChangeDetectionEnable(bool enable) {
    LOG(INFO) << "NWebImageChangeDetection::SetImageChangeDetectionEnable: " << enable;
  enable_ = enable;
  if (!nweb_delegate_) {
      LOG(ERROR) << "SetImageChangeDetectionEnable, nweb_delegate_ is nullptr";
      return;
  }

  std::function<void(void)> callback = nullptr;
  if (enable) {
    callback = [weak = weak_factory_.GetWeakPtr()]() {
      if (!weak) {
        LOG(ERROR) << "weak ptr is nullptr";
        return;
      }
      weak->StartDetection();
    };
    StartDetection();
  } else {
    StopDetection();
  }
  nweb_delegate_->RegisterOnLoadStartedCbForImageChange(std::move(callback));
}

void NWebImageChangeDetection::StartDetection() {
  if (!enable_ || !nweb_delegate_) {
    return;
  }
  LOG(INFO) << "NWebImageChangeDetection::StartDetection";
  const auto& rb = ui::ResourceBundle::GetSharedInstance();
  auto script_data = rb.GetRawDataResource(IDR_START_IMAGE_CHANGE_DETECTION_JS);
  if (script_data.empty()) {
    LOG(ERROR) << "get script_data failed";
    return;
  }

  std::string final_js = std::string(script_data);
  final_js.append("\n");
  final_js.append(kStartTemplate);
  nweb_delegate_->ExecuteJavaScript(final_js);
}

void NWebImageChangeDetection::StopDetection() {
  if (enable_ || !nweb_delegate_) {
    return;
  }
  LOG(INFO) << "NWebImageChangeDetection::StopDetection";
  std::string final_js = std::string(kStopTemplate);
  nweb_delegate_->ExecuteJavaScript(final_js);
}

} // namespace OHOS::NWeb