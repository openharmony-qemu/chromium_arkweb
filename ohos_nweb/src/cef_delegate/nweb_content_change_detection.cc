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

#include "nweb_content_change_detection.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ohos_resources/content_change_detection/grit/content_change_detection_resources.h"
#include "ohos_resources/dom_tree_extractor/grit/dom_tree_extractor_resources.h"
#include "ohos_resources/page_scene_query/grit/page_scene_query_resources.h"

namespace features {
BASE_FEATURE(kReportDomTreeAsync,
             "ReportDomTreeAsync",
             base::FEATURE_ENABLED_BY_DEFAULT);
}

namespace {

// Escape for embedding in a JS single-quoted string.
// IMPORTANT: backslash MUST be escaped first, before other characters
// that introduce backslashes (e.g. ' → \'), to avoid double-escaping.
std::string EscapeForJsSingleQuotedString(const std::string& input) {
  std::string escaped = input;
  base::ReplaceChars(escaped, "\\", "\\\\", &escaped);
  base::ReplaceChars(escaped, "'", "\\'", &escaped);
  base::ReplaceChars(escaped, "\n", "\\n", &escaped);
  base::ReplaceChars(escaped, "\r", "\\r", &escaped);
  return escaped;
}

// Escape a value for embedding inside a JSON double-quoted string
// (used when building nodeTypesJson array entries).
std::string EscapeForJsonStringValue(const std::string& input) {
  std::string escaped = input;
  base::ReplaceChars(escaped, "\\", "\\\\", &escaped);
  base::ReplaceChars(escaped, "\"", "\\\"", &escaped);
  return escaped;
}

}  // namespace

namespace OHOS::NWeb {

constexpr std::string_view kStartTemplate = "(function(window) { window.viewportFontMonitor.start($1, $2); })(window);";
constexpr std::string_view kStopTemplate = "(function(window) { window.viewportFontMonitor.stop(); })(window);";
constexpr std::string_view kRequestWebDomTemplate = "window.__arkWebDomTree.getWebDom();";
constexpr std::string_view kRequestWebDomViewportTemplate = "window.__arkWebDomTree.getWebDom();";
constexpr std::string_view kWebDomReportedTemplate = "window.__arkWebDomTree.addDomTreeReported();";
constexpr std::string_view kWebDomReportedAsyncTemplate = "window.__arkWebDomTree.addDomTreeReportedAsync();";
constexpr std::string_view kRemoveWebDomReportedTemplate = "window.__arkWebDomTree.removeDomTreeReported();";
// clang-format off
constexpr std::string_view kPageSceneQueryTemplate =
    "(function(){"
    "if(!window.__pageSceneQuery){window.__pageSceneQueryLoaded=false;}"
    "try{"
    "if(!window.__pageSceneQuery){"
    "return JSON.stringify({errorCode:3,controls:[],"
    "error:'__pageSceneQuery not defined after IIFE'});}"
    "return window.__pageSceneQuery.request('$1','$2','$3');}"
    "catch(e){return JSON.stringify({errorCode:2,controls:[],"
    "error:e.message||String(e)});}"
    "})();";
// clang-format on

NWebContentChangeDetection::NWebContentChangeDetection(
    base::WeakPtr<NWebDelegateInterface> nweb_delegate)
    : nweb_delegate_(nweb_delegate) {}

void NWebContentChangeDetection::SetContentChangeDetectionEnable(bool enable) {
    LOG(INFO) << "NWebContentChangeDetection::SetContentChangeDetectionEnable: " << enable;
  enable_ = enable;
  if (!nweb_delegate_) {
      LOG(ERROR) << "SetContentChangeDetectionEnable, nweb_delegate_ is nullptr";
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
  nweb_delegate_->RegisterOnLoadStartedCbForContentChange(std::move(callback));
}

void NWebContentChangeDetection::SetContentChangeDetectionConfig(
    int32_t min_report_time, float text_content_ratio) {
  LOG(INFO) << "NWebContentChangeDetection::SetContentChangeDetectionConfig, min_report_time: "
            << min_report_time << ", text_content_ratio: " << text_content_ratio;
  min_report_time_ = min_report_time;
  text_content_ratio_ = text_content_ratio;
}

void NWebContentChangeDetection::SetDomExtractionConfig(bool reportDomTree) {
  LOG(INFO) << "NWebContentChangeDetection::SetDomExtractionConfig, report_dom_tree: " << reportDomTree;
  report_dom_tree_ = reportDomTree;
}

void NWebContentChangeDetection::StartDetection() {
  if (!enable_ || !nweb_delegate_) {
    return;
  }
  LOG(INFO) << "NWebContentChangeDetection::StartDetection";
  const auto& rb = ui::ResourceBundle::GetSharedInstance();
  auto script_data = rb.GetRawDataResource(IDR_START_CONTENT_CHANGE_DETECTION_JS);
  if (script_data.empty()) {
    LOG(ERROR) << "get script_data failed";
    return;
  }
  std::string start_js = base::ReplaceStringPlaceholders(kStartTemplate,
      {std::to_string(text_content_ratio_), std::to_string(min_report_time_)}, nullptr);

  std::string final_js = std::string(script_data) + start_js;
  nweb_delegate_->ExecuteJavaScript(final_js);

  if (report_dom_tree_) {
    StartRunDomExtraction();
  }
}

void NWebContentChangeDetection::StopDetection() {
  if (enable_ || !nweb_delegate_) {
    return;
  }
  LOG(INFO) << "NWebContentChangeDetection::StopDetection";
  std::string final_js = std::string(kStopTemplate);
  nweb_delegate_->ExecuteJavaScript(final_js);
  std::string dom_final_js = std::string(kRemoveWebDomReportedTemplate);
  nweb_delegate_->ExecuteJavaScript(dom_final_js);
}

void NWebContentChangeDetection::RequestWebDomJsonString(std::shared_ptr<NWebMessageValueCallback> callback) {
  LOG(INFO) << "NWebContentChangeDetection::RequestWebDomJsonString";
  const auto& rb = ui::ResourceBundle::GetSharedInstance();
  std::string_view js_dom_extrator = rb.GetRawDataResource(IDR_DOM_TREE_EXTRACTOR_JS);
  if (js_dom_extrator.empty()) {
    LOG(ERROR) << "get dom tree extractor script failed";
    return;
  }
  std::string dom_script_extractor(js_dom_extrator);
  std::string dom_final_js = dom_script_extractor + std::string(kRequestWebDomTemplate);
  nweb_delegate_->ExecuteJavaScript(dom_final_js, callback, false);
}

void NWebContentChangeDetection::RequestWebDomJsonStringWithOptions(
    std::shared_ptr<NWebMessageValueCallback> callback,
    int32_t mode) {
  LOG(INFO) << "NWebContentChangeDetection::RequestWebDomJsonStringWithOptions, mode: " << mode;
  const auto& rb = ui::ResourceBundle::GetSharedInstance();

  if (mode == static_cast<int32_t>(OHOS::NWeb::NWebDomExtractionMode::VIEWPORT_ONLY)) {
    // Viewport-only mode: load viewport JS extractor
    std::string_view js_dom_extractor = rb.GetRawDataResource(IDR_DOM_TREE_EXTRACTOR_VIEWPORT_JS);
    if (js_dom_extractor.empty()) {
      LOG(ERROR) << "get dom tree viewport extractor script failed";
      return;
    }
    std::string dom_script_extractor(js_dom_extractor);
    std::string dom_final_js = dom_script_extractor + std::string(kRequestWebDomViewportTemplate);
    nweb_delegate_->ExecuteJavaScript(dom_final_js, callback, false);
  } else {
    // Default (FULL_TREE) mode: same as RequestWebDomJsonString
    std::string_view js_dom_extractor = rb.GetRawDataResource(IDR_DOM_TREE_EXTRACTOR_JS);
    if (js_dom_extractor.empty()) {
      LOG(ERROR) << "get dom tree extractor script failed";
      return;
    }
    std::string dom_script_extractor(js_dom_extractor);
    std::string dom_final_js = dom_script_extractor + std::string(kRequestWebDomTemplate);
    nweb_delegate_->ExecuteJavaScript(dom_final_js, callback, false);
  }
}

void NWebContentChangeDetection::StartRunDomExtraction() {
  const auto& rb = ui::ResourceBundle::GetSharedInstance();
  LOG(INFO) << "NWebContentChangeDetection::start run dom tree extractor";
  std::string_view js_dom_extrator = rb.GetRawDataResource(IDR_DOM_TREE_EXTRACTOR_JS);
  if (js_dom_extrator.empty()) {
    LOG(ERROR) << "get dom tree extractor script failed";
    return;
  }
  
  std::string dom_script_extractor(js_dom_extrator);
  std::string dom_final_js;
  if (base::FeatureList::IsEnabled(features::kReportDomTreeAsync)) {
    LOG(INFO) << "NWebContentChangeDetection run web dom tree report async";
    dom_final_js = dom_script_extractor + std::string(kWebDomReportedAsyncTemplate);
  } else {
    dom_final_js = dom_script_extractor + std::string(kWebDomReportedTemplate);
  }

  nweb_delegate_->ExecuteJavaScript(dom_final_js);
}

void NWebContentChangeDetection::RequestPageSceneQuery(
    const std::string& ruleJson,
    const std::string& ruleId,
    const std::vector<std::string>& nodeTypes,
    std::shared_ptr<NWebMessageValueCallback> callback) {
  LOG(INFO) << "NWebContentChangeDetection::RequestPageSceneQuery"
            << ", ruleId: " << ruleId
            << ", nodeTypes: " << nodeTypes.size();
  if (!nweb_delegate_) {
    LOG(ERROR) << "RequestPageSceneQuery, nweb_delegate_ is nullptr";
    return;
  }
  const auto& rb = ui::ResourceBundle::GetSharedInstance();
  std::string_view js_resource = rb.GetRawDataResource(IDR_PAGE_SCENE_QUERY_JS);
  if (js_resource.empty()) {
    LOG(ERROR) << "get page scene query script failed";
    return;
  }
  std::string escapedRuleJson = EscapeForJsSingleQuotedString(ruleJson);
  std::string escapedRuleId = EscapeForJsSingleQuotedString(ruleId);

  // Build nodeTypes JSON array string ([] when empty → JS side skips observer)
  // Each value is escaped for JSON double-quoted string context, then the
  // whole array is escaped for JS single-quoted string context ($3 placeholder).
  std::string nodeTypesJson = "[";
  for (size_t i = 0; i < nodeTypes.size(); i++) {
    if (i > 0) {
      nodeTypesJson += ",";
    }
    nodeTypesJson += "\"" + EscapeForJsonStringValue(nodeTypes[i]) + "\"";
  }
  nodeTypesJson += "]";
  nodeTypesJson = EscapeForJsSingleQuotedString(nodeTypesJson);

  std::string call_js = base::ReplaceStringPlaceholders(
      kPageSceneQueryTemplate,
      {escapedRuleJson, escapedRuleId, nodeTypesJson},
      nullptr);
  std::string final_js = std::string(js_resource) + call_js;
  nweb_delegate_->ExecuteJavaScript(final_js, callback, false);
}

} // namespace OHOS::NWeb
