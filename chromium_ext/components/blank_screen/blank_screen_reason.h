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

#ifndef ARKWEB_CHROMIUM_EXT_COMPONENTS_BLANK_SCREEN_BLANK_SCREEN_REASON_H_
#define ARKWEB_CHROMIUM_EXT_COMPONENTS_BLANK_SCREEN_BLANK_SCREEN_REASON_H_

#include <string>

#include "url/gurl.h"

namespace blank_screen {

// Error codes organized by layer.
// Network: 1xx, Resource: 2xx, Rendering/JS: 3xx,
// Kernel/Process: 4xx, Container/Host: 5xx

enum BlankScreenReason {
  // Network layer (1xx)
  kDnsResolveFailed = 101,
  kRequestTimedOut = 102,
  kConnectionRefused = 103,
  kConnectionReset = 104,
  kSslError = 105,
  kCertError = 106,
  kInternetDisconnected = 107,
  kRedirectLoop = 108,
  kProxyError = 109,
  kOtherNetworkError = 199,

  // Resource layer (2xx)
  kMainDocEmpty = 201,
  kHttp4xxError = 202,
  kHttp5xxError = 203,
  kJSGlobalBundleLoadFailed = 210,
  kCssLoadFailed = 211,
  kMultiResourceLoadFailed = 212,
  kCorsBlocked = 213,
  kCspBlocked = 214,
  kMixedContentBlocked = 215,

  // Rendering/JS layer (3xx)
  kJsRuntimeError = 301,
  kNoFirstContentfulPaint = 312,

  // Kernel/Process layer (4xx)
  kRendererCrashed = 401,
  kRendererKilled = 402,
  kGpuProcessCrashed = 403,
  kGpuProcessWatchdogTimeout = 404,

  // Container/Host layer (5xx)
  kWebViewZeroSize = 501,
};

enum class RootCauseSource {
  kNetwork,
  kResource,
  kRendering,
  kProcess,
  kContainer,
  kUnknown,
};

const char* BlankScreenReasonToString(BlankScreenReason reason);
const char* RootCauseSourceToString(RootCauseSource source);
RootCauseSource BlankScreenReasonToSource(BlankScreenReason reason);

// Full attribution report produced by AttributionEngine.
struct RootCauseReport {
  BlankScreenReason reason = kNoFirstContentfulPaint;
  int confidence = 0;               // 0-100
  RootCauseSource source = RootCauseSource::kUnknown;
  std::string description;

  // Original evidence
  GURL url;
  int net_error = 0;
  int http_status = 0;
  int renderer_exit_status = 0;
  bool page_visible = true;
  bool webview_size_empty = false;

  // Frame analysis evidence
  int total_quads = 0;
  int content_quads = 0;
  float white_area_ratio = 0.0f;
  float content_area_ratio = 0.0f;
  bool has_text_like_content = false;

  std::string ToString() const;
  std::string ToJson() const;
};

}  // namespace blank_screen

#endif  // ARKWEB_CHROMIUM_EXT_COMPONENTS_BLANK_SCREEN_BLANK_SCREEN_REASON_H_
