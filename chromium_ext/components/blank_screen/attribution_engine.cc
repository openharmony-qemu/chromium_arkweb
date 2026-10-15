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

#include "arkweb/chromium_ext/components/blank_screen/attribution_engine.h"

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "base/json/json_writer.h"
#include "net/base/net_errors.h"

namespace blank_screen {

namespace {

// Confidence levels — higher = stronger evidence for the true root cause.
constexpr int kConfRendererGone   = 98;
constexpr int kConfContainerAnomaly = 95;
constexpr int kConfNetworkError   = 95;
constexpr int kConfHttp5xx        = 90;
constexpr int kConfHttp4xx        = 85;
constexpr int kConfMainDocEmpty   = 80;
constexpr int kConfCorsBlocked    = 75;
constexpr int kConfCspBlocked     = 75;
constexpr int kConfMultiResource  = 75;
constexpr int kConfMixedContent   = 70;
constexpr int kConfJsBundleFailed = 70;
constexpr int kConfCssLoadFailed  = 65;
constexpr int kConfJsRuntimeError = 60;
constexpr int kConfGpuCrash       = 85;
constexpr int kConfGpuWatchdog    = 70;
constexpr int kConfNoFcp          = 50;
constexpr int kConfUnknown        = 10;

constexpr double kJsExceptionScoreThreshold = 2.0;

// base::TerminationStatus values — must match base/process/kill.h enum
constexpr int kTerminationStatusNormalTermination   = 0;
constexpr int kTerminationStatusAbnormalTermination = 1;
constexpr int kTerminationStatusProcessWasKilled    = 2;
constexpr int kTerminationStatusProcessCrashed      = 3;
constexpr int kTerminationStatusStillRunning        = 4;
constexpr int kTerminationStatusLaunchFailed        = 7;

const char* TerminationStatusToString(int status) {
  switch (status) {
    case kTerminationStatusNormalTermination:
      return "NORMAL_TERMINATION";
    case kTerminationStatusAbnormalTermination:
      return "ABNORMAL_TERMINATION";
    case kTerminationStatusProcessWasKilled:
      return "PROCESS_WAS_KILLED";
    case kTerminationStatusProcessCrashed:
      return "PROCESS_CRASHED";
    case kTerminationStatusStillRunning:
      return "STILL_RUNNING";
    case kTerminationStatusLaunchFailed:
      return "LAUNCH_FAILED";
    default:
      return "UNKNOWN";
  }
}

}  // namespace

const char* BlankScreenReasonToString(BlankScreenReason reason) {
  switch (reason) {
    case kDnsResolveFailed:    return "DNS_RESOLVE_FAILED";
    case kRequestTimedOut:    return "REQUEST_TIMED_OUT";
    case kConnectionRefused:   return "CONNECTION_REFUSED";
    case kConnectionReset:     return "CONNECTION_RESET";
    case kSslError:           return "SSL_ERROR";
    case kCertError:          return "CERT_ERROR";
    case kInternetDisconnected: return "INTERNET_DISCONNECTED";
    case kRedirectLoop:       return "REDIRECT_LOOP";
    case kProxyError:         return "PROXY_ERROR";
    case kOtherNetworkError:  return "OTHER_NETWORK_ERROR";
    case kMainDocEmpty:       return "MAIN_DOC_EMPTY";
    case kHttp4xxError:       return "HTTP_4XX_ERROR";
    case kHttp5xxError:       return "HTTP_5XX_ERROR";
    case kJSGlobalBundleLoadFailed: return "JS_BUNDLE_LOAD_FAILED";
    case kCssLoadFailed:      return "CSS_LOAD_FAILED";
    case kMultiResourceLoadFailed: return "MULTI_RESOURCE_LOAD_FAILED";
    case kCorsBlocked:        return "CORS_BLOCKED";
    case kCspBlocked:         return "CSP_BLOCKED";
    case kMixedContentBlocked: return "MIXED_CONTENT_BLOCKED";
    case kJsRuntimeError:     return "JS_RUNTIME_ERROR";
    case kNoFirstContentfulPaint: return "NO_FIRST_CONTENTFUL_PAINT";
    case kRendererCrashed:    return "RENDERER_CRASHED";
    case kRendererKilled:     return "RENDERER_KILLED";
    case kGpuProcessCrashed:  return "GPU_PROCESS_CRASHED";
    case kGpuProcessWatchdogTimeout: return "GPU_WATCHDOG_TIMEOUT";
    case kWebViewZeroSize:    return "WEBVIEW_ZERO_SIZE";
    default:                  return "UNKNOWN";
  }
}

RootCauseSource BlankScreenReasonToSource(BlankScreenReason reason) {
  int r = static_cast<int>(reason);
  if (r >= 100 && r < 200) return RootCauseSource::kNetwork;
  if (r >= 200 && r < 300) return RootCauseSource::kResource;
  if (r >= 300 && r < 400) return RootCauseSource::kRendering;
  if (r >= 400 && r < 500) return RootCauseSource::kProcess;
  if (r >= 500 && r < 600) return RootCauseSource::kContainer;
  return RootCauseSource::kUnknown;
}

const char* RootCauseSourceToString(RootCauseSource source) {
  switch (source) {
    case RootCauseSource::kNetwork:   return "Network";
    case RootCauseSource::kResource:  return "Resource";
    case RootCauseSource::kRendering: return "Rendering";
    case RootCauseSource::kProcess:   return "Process";
    case RootCauseSource::kContainer: return "Container";
    case RootCauseSource::kUnknown:   return "Unknown";
  }
  return "Unknown";
}

std::string RootCauseReport::ToString() const {
  return base::StringPrintf(
      "RootCauseReport{code=%d, reason=%s, confidence=%d, source=%s, "
      "net_error=%d, http_status=%d, renderer_exit_status=%d, "
      "page_visible=%d, webview_size_empty=%d, "
      "total_quads=%d, content_quads=%d, "
      "white_area_ratio=%.4f, content_area_ratio=%.4f, "
      "has_text_like_content=%d, description=%s}",
      static_cast<int>(reason), BlankScreenReasonToString(reason),
      confidence, RootCauseSourceToString(source),
      net_error, http_status,
      renderer_exit_status,
      page_visible ? 1 : 0, webview_size_empty ? 1 : 0,
      total_quads, content_quads,
      white_area_ratio, content_area_ratio,
      has_text_like_content ? 1 : 0,
      description.c_str());
}

std::string RootCauseReport::ToJson() const {
  base::Value::Dict dict;
  dict.Set("code", static_cast<int>(reason));
  dict.Set("reason", BlankScreenReasonToString(reason));
  dict.Set("confidence", confidence);
  dict.Set("source", RootCauseSourceToString(source));
  dict.Set("description", description);
  dict.Set("net_error", net_error);
  dict.Set("http_status", http_status);
  dict.Set("renderer_exit_status", renderer_exit_status);
  dict.Set("page_visible", page_visible);
  dict.Set("webview_size_empty", webview_size_empty);
  dict.Set("total_quads", total_quads);
  dict.Set("content_quads", content_quads);
  dict.Set("white_area_ratio", white_area_ratio);
  dict.Set("content_area_ratio", content_area_ratio);
  dict.Set("has_text_like_content", has_text_like_content);

  std::string json;
  base::JSONWriter::Write(dict, &json);
  return json;
}

RootCauseReport AttributionEngine::Attribute(const AttributionContext& ctx) {
  RootCauseReport report;

  if (AttributeContainer(ctx, report))
    return report;
  if (AttributeProcess(ctx, report))
    return report;
  if (AttributeNetwork(ctx, report))
    return report;
  if (AttributeResource(ctx, report))
    return report;
  if (AttributeRendering(ctx, report))
    return report;

  // Safety net — MatchNoFcp always matches, so this is unreachable.
  report.reason = kNoFirstContentfulPaint;
  report.confidence = kConfUnknown;
  report.source = RootCauseSource::kUnknown;
  report.description = "White screen confirmed but no root cause identified";
  return report;
}

// --- Layer 1: Container (5xx) ---
// Container anomalies invalidate all other evidence — if the WebView is
// zero-size, frame analysis is unreliable and downstream rules are skipped.
bool AttributionEngine::AttributeContainer(const AttributionContext& ctx,
                                           RootCauseReport& report) {
  return MatchContainerAnomaly(ctx, report);
}

// --- Layer 2: Process (4xx) ---
// Process crashes are deterministic root causes. Renderer crash/kill is
// checked before GPU crash/watchdog because a dead renderer makes GPU
// evidence unreachable.
bool AttributionEngine::AttributeProcess(const AttributionContext& ctx,
                                         RootCauseReport& report) {
  if (MatchRendererGone(ctx, report))
    return true;
  if (MatchGpuProcessCrashed(ctx, report))
    return true;
  if (MatchGpuWatchdogTimeout(ctx, report))
    return true;
  return false;
}

// --- Layer 3: Network (1xx) ---
// Network-layer failures (DNS, connection, SSL, proxy, etc.). ERR_ABORTED
// and ERR_HTTP_RESPONSE_CODE_FAILURE are excluded — the latter is handled
// by the Resource layer.
bool AttributionEngine::AttributeNetwork(const AttributionContext& ctx,
                                         RootCauseReport& report) {
  return MatchNetworkError(ctx, report);
}

// --- Layer 4: Resource (2xx) ---
// HTTP status errors, empty main document, and subresource load failures
// (CORS, CSP, mixed content, JS bundle, CSS). Rules are ordered by
// confidence: HTTP status > main doc empty > block policies > resource counts.
bool AttributionEngine::AttributeResource(const AttributionContext& ctx,
                                          RootCauseReport& report) {
  if (MatchHttpError(ctx, report))
    return true;
  if (MatchMainDocEmpty(ctx, report))
    return true;
  if (MatchCorsBlocked(ctx, report))
    return true;
  if (MatchCrossOriginFailed(ctx, report))
    return true;
  if (MatchCspBlocked(ctx, report))
    return true;
  if (MatchMixedContentBlocked(ctx, report))
    return true;
  if (MatchMultiResourceFailed(ctx, report))
    return true;
  if (MatchJSGlobalBundleFailed(ctx, report))
    return true;
  if (MatchCssLoadFailed(ctx, report))
    return true;
  return false;
}

// --- Layer 5: Rendering (3xx) ---
// JS runtime errors and FCP absence. Lowest confidence — these are
// correlation-based, not causation-based. MatchNoFcp always matches,
// acting as the fallback when no other rule fires.
bool AttributionEngine::AttributeRendering(const AttributionContext& ctx,
                                           RootCauseReport& report) {
  if (MatchJsRuntimeError(ctx, report))
    return true;
  if (MatchNoFcp(ctx, report))
    return true;
  return false;
}

bool AttributionEngine::MatchContainerAnomaly(const AttributionContext& ctx,
                                               RootCauseReport& report) {
  // Skip when renderer is gone — zero-size is likely a consequence, not the cause.
  if (ctx.renderer_gone)
    return false;

  if (ctx.webview_size_empty) {
    FillReport(report, kWebViewZeroSize, kConfContainerAnomaly,
               RootCauseSource::kContainer,
               "WebView size is zero — frame analysis unreliable",
               GURL(), ctx, false);
    return true;
  }

  return false;
}

bool AttributionEngine::MatchRendererGone(const AttributionContext& ctx,
                                           RootCauseReport& report) {
  if (!ctx.renderer_gone)
    return false;

  if (ctx.renderer_exit_status == kTerminationStatusProcessWasKilled) {
    FillReport(report, kRendererKilled, kConfRendererGone,
               RootCauseSource::kProcess,
               base::StringPrintf(
                   "Renderer process was killed by the system (status=%d/%s)",
                   ctx.renderer_exit_status,
                   TerminationStatusToString(ctx.renderer_exit_status)),
               ctx.url, ctx, false);
    return true;
  }

  FillReport(report, kRendererCrashed, kConfRendererGone,
             RootCauseSource::kProcess,
             base::StringPrintf(
                 "Renderer process crashed (status=%d/%s)",
                 ctx.renderer_exit_status,
                 TerminationStatusToString(ctx.renderer_exit_status)),
             ctx.url, ctx, false);
  return true;
}

bool AttributionEngine::MatchNetworkError(const AttributionContext& ctx,
                                           RootCauseReport& report) {
  // ERR_ABORTED: navigation superseded, not a real error.
  if (ctx.net_error == net::ERR_ABORTED)
    return false;

  // ERR_HTTP_RESPONSE_CODE_FAILURE is an HTTP-layer error — let MatchHttpError handle it.
  if (ctx.net_error == net::ERR_HTTP_RESPONSE_CODE_FAILURE)
    return false;

  if (ctx.net_error == net::OK)
    return false;

  FillReport(report, NetErrorToReason(ctx.net_error), kConfNetworkError,
             RootCauseSource::kNetwork,
             base::StringPrintf("Network error: %s",
                                net::ErrorToString(ctx.net_error)),
             ctx.url, ctx, false);
  return true;
}

bool AttributionEngine::MatchHttpError(const AttributionContext& ctx,
                                        RootCauseReport& report) {
  // Accept both net::OK and ERR_HTTP_RESPONSE_CODE_FAILURE.
  if (ctx.net_error != net::OK && ctx.net_error != net::ERR_HTTP_RESPONSE_CODE_FAILURE)
    return false;

  if (ctx.http_status < 400)
    return false;

  if (ctx.http_status >= 500) {
    FillReport(report, kHttp5xxError, kConfHttp5xx, RootCauseSource::kResource,
               "HTTP 5xx server error", ctx.url, ctx, false);
    return true;
  }

  FillReport(report, kHttp4xxError, kConfHttp4xx, RootCauseSource::kResource,
             "HTTP 4xx client error", ctx.url, ctx, false);
  return true;
}

bool AttributionEngine::MatchMainDocEmpty(const AttributionContext& ctx,
                                           RootCauseReport& report) {
  if (ctx.net_error != net::OK)
    return false;

  // HTTP 200 with Content-Length == 0.
  if (ctx.http_status == 200 && ctx.has_response_headers &&
      ctx.content_length == 0) {
    FillReport(report, kMainDocEmpty, kConfMainDocEmpty,
               RootCauseSource::kResource,
               "Main document response body is empty (Content-Length: 0)",
               ctx.url, ctx, false);
    return true;
  }

  return false;
}

bool AttributionEngine::MatchCorsBlocked(const AttributionContext& ctx,
                                         RootCauseReport& report) {
  if (!ctx.has_cors_blocked)
    return false;
  FillReport(report, kCorsBlocked, kConfCorsBlocked, RootCauseSource::kResource,
             "Subresource blocked by CORS policy", ctx.url, ctx, true);
  return true;
}

bool AttributionEngine::MatchCrossOriginFailed(const AttributionContext& ctx,
                                                RootCauseReport& report) {
  // Some browsers report ERR_FAILED instead of ERR_BLOCKED_BY_RESPONSE for CORS blocks.
  if (!ctx.has_cross_origin_failed)
    return false;
  FillReport(report, kCorsBlocked, kConfCorsBlocked, RootCauseSource::kResource,
             "Cross-origin subresource load failed (likely CORS block)",
             ctx.url, ctx, true);
  return true;
}

bool AttributionEngine::MatchCspBlocked(const AttributionContext& ctx,
                                        RootCauseReport& report) {
  // Direct signal: ResourceLoadComplete saw ERR_BLOCKED_BY_CSP.
  if (ctx.has_csp_blocked) {
    FillReport(report, kCspBlocked, kConfCspBlocked, RootCauseSource::kResource,
               "Subresource blocked by CSP policy", ctx.url, ctx, true);
    return true;
  }
  // Indirect signal: CSP header present + console errors (CSP violations).
  if (ctx.has_csp_header && ctx.csp_violation_count > 0) {
    FillReport(report, kCspBlocked, kConfCspBlocked, RootCauseSource::kResource,
               "CSP header present and console errors indicate CSP violation",
               ctx.url, ctx, true);
    return true;
  }
  return false;
}

bool AttributionEngine::MatchMixedContentBlocked(const AttributionContext& ctx,
                                                  RootCauseReport& report) {
  if (!ctx.has_mixed_content_blocked)
    return false;
  FillReport(report, kMixedContentBlocked, kConfMixedContent,
             RootCauseSource::kResource,
             "Subresource blocked as mixed content", ctx.url, ctx, true);
  return true;
}

bool AttributionEngine::MatchMultiResourceFailed(const AttributionContext& ctx,
                                                  RootCauseReport& report) {
  if (ctx.failed_js_count == 0 || ctx.failed_css_count == 0)
    return false;
  FillReport(report, kMultiResourceLoadFailed, kConfMultiResource,
             RootCauseSource::kResource,
             base::StringPrintf("Multiple resource types failed (js=%d, css=%d)",
                                ctx.failed_js_count, ctx.failed_css_count),
             ctx.url, ctx, true);
  return true;
}

bool AttributionEngine::MatchJSGlobalBundleFailed(const AttributionContext& ctx,
                                                   RootCauseReport& report) {
  if (ctx.failed_js_count == 0 || ctx.failed_css_count > 0)
    return false;
  FillReport(report, kJSGlobalBundleLoadFailed, kConfJsBundleFailed,
             RootCauseSource::kResource,
             base::StringPrintf("JS bundle load failed (count=%d)",
                                ctx.failed_js_count),
             ctx.url, ctx, true);
  return true;
}

bool AttributionEngine::MatchCssLoadFailed(const AttributionContext& ctx,
                                            RootCauseReport& report) {
  if (ctx.failed_css_count == 0 || ctx.failed_js_count > 0)
    return false;
  FillReport(report, kCssLoadFailed, kConfCssLoadFailed,
             RootCauseSource::kResource,
             base::StringPrintf("CSS stylesheet load failed (count=%d)",
                                ctx.failed_css_count),
             ctx.url, ctx, true);
  return true;
}

bool AttributionEngine::MatchJsRuntimeError(const AttributionContext& ctx,
                                             RootCauseReport& report) {
  if (ctx.js_exception_weighted_score < kJsExceptionScoreThreshold)
    return false;
  std::string desc = base::StringPrintf(
      "JS runtime errors detected (score=%.1f, path_ab=%d, path_c=%d",
      ctx.js_exception_weighted_score, ctx.js_exception_count_path_ab,
      ctx.js_exception_count_path_c);
  if (!ctx.js_exception_messages.empty()) {
    desc += "; errors: " + base::JoinString(ctx.js_exception_messages, " | ");
  }
  desc += ")";
  FillReport(report, kJsRuntimeError, kConfJsRuntimeError,
             RootCauseSource::kRendering,
             desc, ctx.url, ctx, false);
  return true;
}

bool AttributionEngine::MatchGpuProcessCrashed(const AttributionContext& ctx,
                                                RootCauseReport& report) {
  if (!ctx.gpu_process_crashed)
    return false;
  FillReport(report, kGpuProcessCrashed, kConfGpuCrash,
             RootCauseSource::kProcess,
             base::StringPrintf(
                 "GPU process crashed, rendering pipeline unavailable "
                 "(exit_code=%d, reason=%s)",
                 ctx.gpu_exit_code,
                 ctx.gpu_crash_reason.c_str()),
             ctx.url, ctx, false);
  return true;
}

bool AttributionEngine::MatchGpuWatchdogTimeout(const AttributionContext& ctx,
                                                 RootCauseReport& report) {
  if (!ctx.gpu_watchdog_timeout)
    return false;
  FillReport(report, kGpuProcessWatchdogTimeout, kConfGpuWatchdog,
             RootCauseSource::kProcess,
             base::StringPrintf(
                 "GPU watchdog timeout — GPU process hung and was terminated "
                 "(exit_code=%d, reason=%s)",
                 ctx.gpu_exit_code,
                 ctx.gpu_crash_reason.c_str()),
             ctx.url, ctx, false);
  return true;
}

bool AttributionEngine::MatchNoFcp(const AttributionContext& ctx,
                                    RootCauseReport& report) {
  FillReport(report, kNoFirstContentfulPaint, kConfNoFcp,
             RootCauseSource::kRendering,
             "No First Contentful Paint — page rendered blank, "
             "root cause not identified by other rules",
             ctx.url, ctx, false);
  return true;
}

BlankScreenReason AttributionEngine::NetErrorToReason(int net_error) {
  switch (net_error) {
    case net::ERR_NAME_NOT_RESOLVED:
      return kDnsResolveFailed;
    case net::ERR_CONNECTION_TIMED_OUT:
    case net::ERR_TIMED_OUT:
      return kRequestTimedOut;
    case net::ERR_CONNECTION_REFUSED:
      return kConnectionRefused;
    case net::ERR_CONNECTION_RESET:
    case net::ERR_CONNECTION_CLOSED:
    case net::ERR_CONNECTION_ABORTED:
    case net::ERR_EMPTY_RESPONSE:
      return kConnectionReset;
    case net::ERR_SSL_PROTOCOL_ERROR:
    case net::ERR_SSL_VERSION_OR_CIPHER_MISMATCH:
      return kSslError;
    case net::ERR_CERT_COMMON_NAME_INVALID:
    case net::ERR_CERT_DATE_INVALID:
    case net::ERR_CERT_AUTHORITY_INVALID:
    case net::ERR_CERT_REVOKED:
    case net::ERR_CERT_INVALID:
      return kCertError;
    case net::ERR_INTERNET_DISCONNECTED:
    case net::ERR_NETWORK_CHANGED:
      return kInternetDisconnected;
    case net::ERR_TOO_MANY_REDIRECTS:
      return kRedirectLoop;
    case net::ERR_PROXY_CONNECTION_FAILED:
    case net::ERR_PROXY_AUTH_UNSUPPORTED:
      return kProxyError;
    default:
      return kOtherNetworkError;
  }
}

void AttributionEngine::AppendFailedUrls(const AttributionContext& ctx,
                                         RootCauseReport& report) {
  auto append_list = [&](const std::vector<std::string>& items,
                         const char* prefix, const char* separator) {
    if (!items.empty()) {
      report.description += prefix + base::JoinString(items, separator);
    }
  };
  append_list(ctx.failed_js_urls, "; failed_js: ", ", ");
  append_list(ctx.failed_css_urls, "; failed_css: ", ", ");
  append_list(ctx.blocked_resource_messages, "; blocked: ", " | ");
}

void AttributionEngine::FillReport(RootCauseReport& report,
                                    BlankScreenReason reason,
                                    int confidence,
                                    RootCauseSource source,
                                    const std::string& description,
                                    const GURL& url,
                                    const AttributionContext& ctx,
                                    bool append_failed_urls) {
  report.reason = reason;
  report.confidence = confidence;
  report.source = source;
  report.description = description;
  report.url = url;
  // Copy all evidence fields so every matched rule has complete context.
  report.net_error = ctx.net_error;
  report.http_status = ctx.http_status;
  report.renderer_exit_status = ctx.renderer_exit_status;
  report.page_visible = ctx.page_visible;
  report.webview_size_empty = ctx.webview_size_empty;
  report.total_quads = ctx.total_quads;
  report.content_quads = ctx.content_quads;
  report.white_area_ratio = ctx.white_area_ratio;
  report.content_area_ratio = ctx.content_area_ratio;
  report.has_text_like_content = ctx.has_text_like_content;
  if (append_failed_urls)
    AppendFailedUrls(ctx, report);
}

}  // namespace blank_screen
