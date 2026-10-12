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

#ifndef ARKWEB_CHROMIUM_EXT_COMPONENTS_BLANK_SCREEN_ATTRIBUTION_ENGINE_H_
#define ARKWEB_CHROMIUM_EXT_COMPONENTS_BLANK_SCREEN_ATTRIBUTION_ENGINE_H_

#include <string>
#include <vector>

#include "arkweb/chromium_ext/components/blank_screen/blank_screen_reason.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace blank_screen {

// Evidence context collected during the white screen detection lifecycle.
struct AttributionContext {
  // Network layer evidence
  int net_error = net::OK;
  int http_status = 0;
  bool has_response_headers = false;
  int64_t content_length = -1;  // -1 = unknown
  GURL url;

  // Process evidence
  bool renderer_gone = false;
  int renderer_exit_status = 0;

  // Container evidence
  bool page_visible = true;
  bool webview_size_empty = false;

  // Frame analysis evidence
  int total_quads = 0;
  int content_quads = 0;
  float white_area_ratio = 0.0f;
  float content_area_ratio = 0.0f;
  bool has_text_like_content = false;

  // Subresource loading evidence
  int failed_js_count = 0;
  int failed_css_count = 0;
  bool has_cors_blocked = false;
  bool has_csp_blocked = false;
  bool has_mixed_content_blocked = false;
  // CSP header present on main document. CSP-blocked subresources don't
  // trigger ResourceLoadComplete, so CSP header + console errors are used.
  bool has_csp_header = false;
  int csp_violation_count = 0;
  // Cross-origin JS/CSS failed with ERR_FAILED (non-standard CORS variant).
  bool has_cross_origin_failed = false;
  std::vector<std::string> failed_js_urls;
  std::vector<std::string> failed_css_urls;
  std::vector<std::string> blocked_resource_messages;

  // JS exception evidence (path A+B: weight 1.0, path C: weight 0.5)
  double js_exception_weighted_score = 0.0;
  int js_exception_count_path_ab = 0;
  int js_exception_count_path_c = 0;
  std::vector<std::string> js_exception_messages;

  // GPU process evidence
  bool gpu_process_crashed = false;
  bool gpu_watchdog_timeout = false;
  int gpu_exit_code = 0;
  std::string gpu_crash_reason;

  void Reset() {
    // Clear all per-navigation evidence.
    // Container state (page_visible, webview_size_empty) is NOT reset —
    // it reflects live state at Report() time.
    net_error = net::OK;
    http_status = 0;
    has_response_headers = false;
    content_length = -1;
    url = GURL();
    renderer_gone = false;
    renderer_exit_status = 0;
    total_quads = 0;
    content_quads = 0;
    white_area_ratio = 0.0f;
    content_area_ratio = 0.0f;
    has_text_like_content = false;
    failed_js_count = 0;
    failed_css_count = 0;
    has_cors_blocked = false;
    has_csp_blocked = false;
    has_mixed_content_blocked = false;
    has_csp_header = false;
    has_cross_origin_failed = false;
    csp_violation_count = 0;
    js_exception_weighted_score = 0.0;
    js_exception_count_path_ab = 0;
    js_exception_count_path_c = 0;
    js_exception_messages.clear();
    gpu_process_crashed = false;
    gpu_watchdog_timeout = false;
    gpu_exit_code = 0;
    gpu_crash_reason.clear();
    failed_js_urls.clear();
    failed_css_urls.clear();
    blocked_resource_messages.clear();
  }
};

// Pure function: given an AttributionContext, produce a RootCauseReport.
// Rules are evaluated in priority order; first match wins.
class AttributionEngine {
 public:
  static RootCauseReport Attribute(const AttributionContext& ctx);

 private:
  // Five-layer attribution dispatch. Each layer handles its error code range.
  // Layers are evaluated in priority order; first match wins.
  static bool AttributeContainer(const AttributionContext& ctx,
                                 RootCauseReport& report);  // 5xx
  static bool AttributeProcess(const AttributionContext& ctx,
                               RootCauseReport& report);    // 4xx
  static bool AttributeNetwork(const AttributionContext& ctx,
                               RootCauseReport& report);    // 1xx
  static bool AttributeResource(const AttributionContext& ctx,
                                RootCauseReport& report);   // 2xx
  static bool AttributeRendering(const AttributionContext& ctx,
                                 RootCauseReport& report);  // 3xx
  static bool MatchContainerAnomaly(const AttributionContext& ctx,
                                    RootCauseReport& report);
  static bool MatchRendererGone(const AttributionContext& ctx,
                                RootCauseReport& report);
  static bool MatchNetworkError(const AttributionContext& ctx,
                                RootCauseReport& report);
  static bool MatchHttpError(const AttributionContext& ctx,
                             RootCauseReport& report);
  static bool MatchMainDocEmpty(const AttributionContext& ctx,
                                RootCauseReport& report);
  static bool MatchCorsBlocked(const AttributionContext& ctx,
                               RootCauseReport& report);
  static bool MatchCrossOriginFailed(const AttributionContext& ctx,
                                     RootCauseReport& report);
  static bool MatchCspBlocked(const AttributionContext& ctx,
                              RootCauseReport& report);
  static bool MatchMixedContentBlocked(const AttributionContext& ctx,
                                       RootCauseReport& report);
  static bool MatchMultiResourceFailed(const AttributionContext& ctx,
                                       RootCauseReport& report);
  static bool MatchJSGlobalBundleFailed(const AttributionContext& ctx,
                                        RootCauseReport& report);
  static bool MatchCssLoadFailed(const AttributionContext& ctx,
                                 RootCauseReport& report);
  static bool MatchJsRuntimeError(const AttributionContext& ctx,
                                  RootCauseReport& report);
  static bool MatchGpuProcessCrashed(const AttributionContext& ctx,
                                     RootCauseReport& report);
  static bool MatchGpuWatchdogTimeout(const AttributionContext& ctx,
                                      RootCauseReport& report);
  static bool MatchNoFcp(const AttributionContext& ctx,
                         RootCauseReport& report);

  static BlankScreenReason NetErrorToReason(int net_error);
  static void AppendFailedUrls(const AttributionContext& ctx,
                               RootCauseReport& report);
  static void FillReport(RootCauseReport& report,
                         BlankScreenReason reason,
                         int confidence,
                         RootCauseSource source,
                         const std::string& description,
                         const GURL& url,
                         const AttributionContext& ctx,
                         bool append_failed_urls);
};

}  // namespace blank_screen

#endif  // ARKWEB_CHROMIUM_EXT_COMPONENTS_BLANK_SCREEN_ATTRIBUTION_ENGINE_H_
