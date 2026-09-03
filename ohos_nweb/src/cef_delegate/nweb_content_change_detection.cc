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
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
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
//   1. JSONWriter output (normalizedRuleJson, nodeTypesJson): control chars
//      (\t \b \f \v \0 U+2028 U+2029) are already escaped by JSONWriter as
//      \uXXXX, so only \\ and \' replacements are strictly needed; the
//      remaining replacements are harmless no-ops.
//   2. Raw input (ruleId): all replacements are necessary since the string
//      may contain literal control characters.
std::string EscapeForJsSingleQuotedString(const std::string& input) {
  std::string escaped = input;
  base::ReplaceChars(escaped, "\\", "\\\\", &escaped);
  base::ReplaceChars(escaped, "'", "\\'", &escaped);
  base::ReplaceChars(escaped, "\n", "\\n", &escaped);
  base::ReplaceChars(escaped, "\r", "\\r", &escaped);
  base::ReplaceChars(escaped, "\t", "\\t", &escaped);
  base::ReplaceChars(escaped, "\b", "\\b", &escaped);
  base::ReplaceChars(escaped, "\f", "\\f", &escaped);
  base::ReplaceChars(escaped, "\v", "\\v", &escaped);
  base::ReplaceChars(escaped, std::string(1, '\0'), "\\x00", &escaped);
  // U+2028 (Line Separator) and U+2029 (Paragraph Separator) are line
  // terminators in JS and would break single-quoted string literals.
  const std::string kUtf8LineSeparator("\xe2\x80\xa8");
  const std::string kUtf8ParagraphSeparator("\xe2\x80\xa9");
  base::ReplaceChars(escaped, kUtf8LineSeparator, "\\u2028", &escaped);
  base::ReplaceChars(escaped, kUtf8ParagraphSeparator, "\\u2029", &escaped);
  return escaped;
}

}  // namespace

namespace OHOS::NWeb {

// Input size limits to prevent excessively large JS injection.
constexpr size_t kMaxRuleJsonLength = 128 * 1024; // 128 KB
constexpr size_t kMaxRuleIdLength = 1024;
constexpr size_t kMaxNodeTypesCount = 128;
constexpr size_t kMaxNodeTypeLength = 1024;
// Maximum nesting depth for JSON parsing to prevent stack overflow.
constexpr int kJsonMaxNestingDepth = 32;

constexpr std::string_view kStartTemplate = "(function(window) { window.viewportFontMonitor.start($1, $2); })(window);";
constexpr std::string_view kStopTemplate = "(function(window) { window.viewportFontMonitor.stop(); })(window);";
constexpr std::string_view kRequestWebDomTemplate = "window.__arkWebDomTree.getWebDom();";
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

bool NWebContentChangeDetection::ValidatePageSceneQueryInput(
    const std::string& ruleJson,
    const std::string& ruleId,
    const std::vector<std::string>& nodeTypes,
    std::string& outNormalizedRuleJson) {
  if (!nweb_delegate_) {
    LOG(ERROR) << "RequestPageSceneQuery, nweb_delegate_ is nullptr";
    return false;
  }
  if (ruleJson.size() > kMaxRuleJsonLength) {
    LOG(ERROR) << "RequestPageSceneQuery, ruleJson exceeds max length: "
               << ruleJson.size() << " > " << kMaxRuleJsonLength;
    return false;
  }
  if (ruleId.size() > kMaxRuleIdLength) {
    LOG(ERROR) << "RequestPageSceneQuery, ruleId exceeds max length: "
               << ruleId.size() << " > " << kMaxRuleIdLength;
    return false;
  }
  if (nodeTypes.size() > kMaxNodeTypesCount) {
    LOG(ERROR) << "RequestPageSceneQuery, nodeTypes count exceeds max: "
               << nodeTypes.size() << " > " << kMaxNodeTypesCount;
    return false;
  }
  for (const auto& nodeType : nodeTypes) {
    if (nodeType.size() > kMaxNodeTypeLength) {
      LOG(ERROR) << "RequestPageSceneQuery, nodeType exceeds max length: "
                 << nodeType.size() << " > " << kMaxNodeTypeLength;
      return false;
    }
  }

  // Validate ruleJson is valid JSON and re-serialize for normalization (CHR-01).
  // Re-serialization ensures canonical JSON output (e.g. U+2028/U+2029 escaping)
  // and consistency with nodeTypes handling via JSONWriter.
  auto parsed = base::JSONReader::Read(ruleJson, base::JSON_PARSE_RFC,
                                       kJsonMaxNestingDepth);
  if (!parsed) {
    LOG(ERROR) << "RequestPageSceneQuery, ruleJson is not valid JSON";
    return false;
  }
  base::JSONWriter::Write(*parsed, &outNormalizedRuleJson);
  return true;
}

void NWebContentChangeDetection::RequestPageSceneQuery(
    const std::string& ruleJson,
    const std::string& ruleId,
    const std::vector<std::string>& nodeTypes,
    std::shared_ptr<NWebMessageValueCallback> callback) {
  LOG(INFO) << "NWebContentChangeDetection::RequestPageSceneQuery"
            << ", ruleId length: " << ruleId.size()
            << ", nodeTypes: " << nodeTypes.size();
  std::string normalizedRuleJson;
  if (!ValidatePageSceneQueryInput(ruleJson, ruleId, nodeTypes, normalizedRuleJson)) {
    return;
  }

  const auto& rb = ui::ResourceBundle::GetSharedInstance();
  std::string_view js_resource = rb.GetRawDataResource(IDR_PAGE_SCENE_QUERY_JS);
  if (js_resource.empty()) {
    LOG(ERROR) << "get page scene query script failed";
    return;
  }
  std::string escapedRuleJson = EscapeForJsSingleQuotedString(normalizedRuleJson);
  std::string escapedRuleId = EscapeForJsSingleQuotedString(ruleId);

  // Build nodeTypes JSON array using base::Value + JSONWriter (CHR-01).
  base::Value::List node_types_list;
  for (const auto& nodeType : nodeTypes) {
    node_types_list.Append(nodeType);
  }
  std::string nodeTypesJson;
  base::JSONWriter::Write(node_types_list, &nodeTypesJson);
  nodeTypesJson = EscapeForJsSingleQuotedString(nodeTypesJson);

  std::string call_js = base::ReplaceStringPlaceholders(
      kPageSceneQueryTemplate,
      {escapedRuleJson, escapedRuleId, nodeTypesJson},
      nullptr);
  std::string final_js = std::string(js_resource) + call_js;
  nweb_delegate_->ExecuteJavaScript(final_js, callback, false);
}

} // namespace OHOS::NWeb
