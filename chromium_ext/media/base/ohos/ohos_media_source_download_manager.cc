// Copyright 2026 The Huawei Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/ohos/ohos_media_source_download_manager.h"

#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/string_util.h"
#include "base/unguessable_token.h"
#include "net/base/isolation_info.h"
#include "net/base/net_errors.h"
#include "net/url_request/redirect_info.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace media {

namespace {
// FinishLoading error codes passed back to the OHOS player through
// PlayerAdapter::OnDataFinishLoading. The int32 is interpreted by the player
// framework (HiStreamer) whose `LoadingRequestError` enum lives in
// foundation/multimedia/player_framework/.../loading_request.h and is NOT
// visible from the Chromium layer.
//
// Contract: 0 == success / load complete; any non-zero value signals a failed
// read to the player (it surfaces a playback error rather than hanging).
constexpr int32_t kLoadingErrorOk = 0;
constexpr int32_t kLoadingErrorTimeout = -2;    // net timeout
constexpr int32_t kLoadingErrorNetwork = -3;    // connection / generic network
constexpr int32_t kLoadingErrorAbort = -4;      // net::ERR_ABORTED
constexpr int32_t kLoadingErrorForbidden = -5;  // HTTP 403
constexpr int32_t kLoadingErrorNotFound = -6;   // HTTP 404

int32_t MapNetErrorToLoadingRequestError(int net_error, int http_status) {
  if (http_status == 403) {
    return kLoadingErrorForbidden;
  }
  if (http_status == 404) {
    return kLoadingErrorNotFound;
  }
  if (http_status >= 400) {
    return kLoadingErrorNetwork;
  }
  switch (net_error) {
    case net::ERR_TIMED_OUT:
    case net::ERR_CONNECTION_TIMED_OUT:
      return kLoadingErrorTimeout;
    case net::ERR_ABORTED:
      return kLoadingErrorAbort;
    case net::OK:
      // No transport error and no mapped HTTP status -> treat as success.
      return kLoadingErrorOk;
    default:
      return kLoadingErrorNetwork;
  }
}
}  // namespace

MediaSourceDownloadManager::MediaSourceDownloadManager(
    OHOS::NWeb::PlayerAdapter* player,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    DevToolsNetworkEventSink* devtools_sink,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    MediaCorsMode cors_mode,
    const url::Origin& frame_origin,
    base::RepeatingCallback<void(bool tainted)> on_taint_changed)
    : task_runner_(std::move(task_runner)),
      url_loader_factory_(std::move(url_loader_factory)),
      player_(player),
      devtools_sink_(devtools_sink),
      site_for_cookies_(site_for_cookies),
      top_frame_origin_(top_frame_origin),
      cors_mode_(cors_mode),
      frame_origin_(frame_origin),
      on_taint_changed_(std::move(on_taint_changed)) {
  DCHECK(task_runner_);
}

MediaSourceDownloadManager::~MediaSourceDownloadManager() = default;

base::WeakPtr<MediaSourceDownloadManager>
MediaSourceDownloadManager::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// static
bool MediaSourceDownloadManager::ResponseTaintsOrigin(
    MediaCorsMode cors_mode,
    const url::Origin& frame_origin,
    const GURL& response_url,
    const net::HttpResponseHeaders* headers) {
  // Non-http(s) URLs: taint if frame_origin is opaque or not same-origin.
  // (Proxy-path HLS is always http/https, so this branch is theoretical.)
  if (!response_url.is_valid() ||
      (!response_url.SchemeIsHTTPOrHTTPS() && !response_url.SchemeIsWSOrWSS())) {
    bool same = !frame_origin.opaque() && frame_origin.IsSameOriginWith(response_url);
    LOG(DEBUG) << "[HLS WEBGL]: ResponseTaintsOrigin: non-http(s) same_origin="
               << same << " taint=" << !same;
    return !same;
  }

  // Same-origin → never taints (basic response).
  // An opaque frame_origin can never be same-origin with an http(s) URL,
  // so this check naturally falls through to the cors mode switch below.
  if (!frame_origin.opaque() && frame_origin.IsSameOriginWith(response_url)) {
    LOG(DEBUG) << "[HLS WEBGL]: ResponseTaintsOrigin: same-origin no taint";
    return false;
  }

  switch (cors_mode) {
    case MediaCorsMode::kUnspecified:
      // no-cors: cross-origin (or opaque initiator) → opaque → taint.
      // Opaque frame_origin with http URL is always cross-origin → taint.
      LOG(DEBUG) << "[HLS WEBGL]: ResponseTaintsOrigin: kUnspecified cross-origin taint";
      return true;
    case MediaCorsMode::kAnonymous:
    case MediaCorsMode::kUseCredentials: {
      // CORS check: need valid ACAO header matching frame_origin.
      // Fail-closed if no headers.
      // Opaque frame_origin is NOT short-circuited: it serializes to "null"
      // in CheckAccess, so a server returning ACAO: null yields clean —
      // aligning with the network layer's handling of sandboxed documents
      // (fetch spec's legal channel for opaque origins).
      if (!headers) {
        LOG(DEBUG) << "[HLS WEBGL]: ResponseTaintsOrigin: no response headers taint";
        return true;
      }
      auto acao = headers->GetNormalizedHeader("access-control-allow-origin");
      if (!acao.has_value()) {
        LOG(DEBUG) << "[HLS WEBGL]: ResponseTaintsOrigin: no ACAO header taint";
        return true;
      }
      // Credentials mode: anonymous → kSameOrigin (no ACAC required),
      // use-credentials → kInclude (ACAC must be "true").
      network::mojom::CredentialsMode credentials_mode =
          (cors_mode == MediaCorsMode::kUseCredentials)
              ? network::mojom::CredentialsMode::kInclude
              : network::mojom::CredentialsMode::kSameOrigin;
      auto allow_credentials_header =
          headers->GetNormalizedHeader("access-control-allow-credentials");
      auto result = network::cors::CheckAccess(
          response_url, acao,
          allow_credentials_header, credentials_mode, frame_origin);
      bool taints = !result.has_value();
      LOG(DEBUG) << "[HLS WEBGL]: ResponseTaintsOrigin: CheckAccess taints=" << taints;
      return taints;
    }
    default:
      // Fail-closed for unexpected/future enum values.
      LOG(DEBUG) << "[HLS WEBGL]: ResponseTaintsOrigin: unknown cors_mode taint";
      return true;
  }
}

void MediaSourceDownloadManager::Open(
    int64_t uuid,
    const std::string& url,
    const std::map<std::string, std::string>& header) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto& ctx = downloads_[uuid];
  ctx.url = url;
  ctx.header = header;
  ctx.is_active = true;
}

void MediaSourceDownloadManager::Read(int64_t uuid,
                                      int64_t offset,
                                      int64_t length) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto it = downloads_.find(uuid);
  if (it == downloads_.end() || !it->second.is_active) {
    LOG(ERROR) << "MediaSourceDownloadManager Read called on inactive/unknown";
    return;
  }
  StartDownload(uuid, it->second.url, offset, length, it->second.header);
}

void MediaSourceDownloadManager::Close(int64_t uuid) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto it = downloads_.find(uuid);
  if (it != downloads_.end()) {
    it->second.is_active = false;
    if (it->second.loader) {
      it->second.loader.reset();
    }
    downloads_.erase(it);
  }
}

void MediaSourceDownloadManager::StartDownload(
    int64_t uuid,
    const std::string& url,
    int64_t offset,
    int64_t length,
    const std::map<std::string, std::string>& extra_header) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto it = downloads_.find(uuid);
  if (it == downloads_.end() || !it->second.is_active) {
    LOG(WARNING) << "StartDownload on inactive/unknown";
    return;
  }
  if (!url_loader_factory_) {
    LOG(ERROR) << "No URLLoaderFactory available, aborting read";
    if (player_) {
      player_->OnDataFinishLoading(uuid, kLoadingErrorNetwork);
    }
    return;
  }

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(url);
  request->method = "GET";
  request->destination = network::mojom::RequestDestination::kVideo;
  // credentials_mode defaults to kInclude, so page cookies are sent.
  // Range header. length < 0 means "read to EOS" (open-ended range).
  if (offset > 0 || length >= 0) {
    std::string range = (length >= 0)
        ? base::StringPrintf("bytes=%lld-%lld",
                             static_cast<long long>(offset),
                             static_cast<long long>(offset + length - 1))
        : base::StringPrintf("bytes=%lld-", static_cast<long long>(offset));
    request->headers.SetHeader(net::HttpRequestHeaders::kRange, range);
  }
  // Caller-supplied headers (Cookie / User-Agent / Referer / Origin / ...),
  // collected from the page so the request shares its credentials.
  for (const auto& entry : extra_header) {
    if (base::EqualsCaseInsensitiveASCII(entry.first, net::HttpRequestHeaders::kReferer)) {
      request->referrer = GURL(entry.second);
      continue;
    }
    if (base::EqualsCaseInsensitiveASCII(entry.first, net::HttpRequestHeaders::kCookie)) {
      continue;
    }
    request->headers.SetHeader(entry.first, entry.second);
  }

  // request->mode stays kNoCors (network service would reject cross-origin
  // otherwise); we set Origin header + credentials_mode so the server returns
  // ACAO/ACAC headers for analysis, matching the renderer's own fetch behavior.
  // The Origin header is set AFTER the header copy loop above so page-supplied
  // Origin values cannot override the analysis initiator.
  if (cors_mode_ == MediaCorsMode::kAnonymous) {
    request->credentials_mode = network::mojom::CredentialsMode::kSameOrigin;
    if (!frame_origin_.opaque()) {
      request->headers.SetHeader(net::HttpRequestHeaders::kOrigin,
                                 frame_origin_.Serialize());
    }
  } else if (cors_mode_ == MediaCorsMode::kUseCredentials) {
    // credentials_mode stays kInclude (default), add Origin header.
    if (!frame_origin_.opaque()) {
      request->headers.SetHeader(net::HttpRequestHeaders::kOrigin,
                                 frame_origin_.Serialize());
    }
  }

  // Page cookie context. This is a browser-process SimpleURLLoader, so by
  // default it carries no frame context and the network service treats it as a
  // cross-site request → SameSite=Lax page cookies are EXCLUDEd (MGTV et al.
  // then 403). Setting site_for_cookies + request_initiator makes it a same-site
  // request from the page, so Lax cookies attach. credentials_mode already
  // defaults to kInclude (it gates whether credentials may be sent at all, not
  // SameSite context).
  request->site_for_cookies = site_for_cookies_;
  request->request_initiator = top_frame_origin_;
  request->trusted_params.emplace();
  request->trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, top_frame_origin_,
      top_frame_origin_, site_for_cookies_);
  // Stamp a DevTools tracking id and surface this browser-process fetch in the
  // Network panel via `devtools_sink_` (the bridge's client wires it to
  // devtools_instrumentation's browser-process Network event emission). The id
  // is carried across NotifyRequestWillBeSent / ResponseReceived / LoadingComplete
  // so the inspector can stitch the request lifecycle together. One id per read.
  request->devtools_request_id = base::UnguessableToken::Create().ToString();
  it->second.devtools_request_id = request->devtools_request_id.value();
  if (devtools_sink_) {
    devtools_sink_->NotifyRequestWillBeSent(it->second.devtools_request_id,
                                            *request);
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("hls_proxy_download", R"(
        semantics {
          sender: "OHOS Media Player Bridge"
          description:
            "Fetches HLS media data (manifest/segments) through the Chromium "
            "network stack so it shares cookies, headers and cache with the "
            "page. The fetched bytes are fed back to the OHOS player for "
            "decoding and playback."
          trigger: "Playing an HLS (m3u8) video with media proxy enabled."
          data: "HTTP Range requests for media segments; carries page cookies."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Enabled per-media via setMediaProxyEnabled (off by default)."
        })");

  auto loader = network::SimpleURLLoader::Create(std::move(request),
                                                 traffic_annotation);
  // Capture redirect responses for CORS analysis. Each redirect hop's
  // (url_before_redirect, headers) is stored in the DownloadContext so
  // OnDownloadComplete can analyze the full chain.
  loader->SetOnRedirectCallback(base::BindRepeating(
      &MediaSourceDownloadManager::OnRedirect, weak_factory_.GetWeakPtr(),
      uuid));
  // Deliver the body (and populate ResponseInfo) even on HTTP errors so we
  // can distinguish 403 (anti-leech) / 404 from generic failures.
  loader->SetAllowHttpErrorResults(true);
  // Cap at 64 MiB per Range read (single segment/chunk).
  constexpr size_t kMaxSegmentBodySize = 64 * 1024 * 1024;
  loader->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&MediaSourceDownloadManager::OnDownloadComplete,
                     weak_factory_.GetWeakPtr(), uuid),
      kMaxSegmentBodySize);

  it->second.pending_offset = offset;
  it->second.loader = std::move(loader);
}

void MediaSourceDownloadManager::OnRedirect(
    int64_t uuid,
    const GURL& url_before_redirect,
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* removed_headers) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto it = downloads_.find(uuid);
  if (it == downloads_.end()) {
    return;
  }
  auto& ctx = it->second;

  // Capture the redirect response (url + headers) for CORS analysis.
  // CORS check applies to each redirect response per fetch spec.
  scoped_refptr<net::HttpResponseHeaders> headers;
  if (response_head.headers) {
    headers = response_head.headers;
  }
  ctx.redirect_chain.emplace_back(url_before_redirect, headers);
}

bool MediaSourceDownloadManager::ComputeResponseTaint(
    const DownloadContext& ctx,
    const network::SimpleURLLoader* loader) const {
  // Analyze each redirect hop + the final response (monotonic OR aggregate).
  bool response_taints = false;
  for (const auto& [hop_url, hop_headers] : ctx.redirect_chain) {
    response_taints |= ResponseTaintsOrigin(cors_mode_, frame_origin_,
                                            hop_url, hop_headers.get());
  }

  // Analyze final response.
  GURL final_url = loader ? loader->GetFinalURL() : GURL(ctx.url);
  if (!final_url.is_valid()) {
    final_url = GURL(ctx.url);
  }
  net::HttpResponseHeaders* final_headers =
      (loader && loader->ResponseInfo()) ? loader->ResponseInfo()->headers.get()
                                         : nullptr;
  response_taints |= ResponseTaintsOrigin(cors_mode_, frame_origin_,
                                          final_url, final_headers);

  // kUnspecified + cross-origin redirect hop → taint (replicates
  // MultiBufferDataSource::WouldTaintOrigin HasSingleOrigin semantics,
  // multi_buffer_data_source.cc:233: single_origin_ is set false when
  // adjacent redirect hops have different origins).
  if (cors_mode_ == MediaCorsMode::kUnspecified && !ctx.redirect_chain.empty()) {
    GURL prev_url(ctx.url);
    bool cross_origin_hop = false;
    for (const auto& [hop_url, hop_headers] : ctx.redirect_chain) {
      if (!url::Origin::Create(prev_url).IsSameOriginWith(
              url::Origin::Create(hop_url))) {
        cross_origin_hop = true;
        break;
      }
      prev_url = hop_url;
    }
    if (!cross_origin_hop) {
      GURL final_url_check = loader ? loader->GetFinalURL() : GURL(ctx.url);
      if (!url::Origin::Create(prev_url).IsSameOriginWith(
              url::Origin::Create(final_url_check))) {
        cross_origin_hop = true;
      }
    }
    if (cross_origin_hop) {
      response_taints = true;
    }
  }

  return response_taints;
}

void MediaSourceDownloadManager::MaybePushTaint(bool tainted) {
  // Monotonic OR: can only flip clean→tainted, never back.
  any_response_tainted_ |= tainted;

  // Fire-and-forget: push verdict if it changed, then deliver bytes
  // immediately. No ACK, no park, no gating.
  if (any_response_tainted_ != last_sent_verdict_) {
    last_sent_verdict_ = any_response_tainted_;
    if (on_taint_changed_) {
      on_taint_changed_.Run(any_response_tainted_);
    }
  }
}

void MediaSourceDownloadManager::OnDownloadComplete(
    int64_t uuid,
    std::unique_ptr<std::string> response_body) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto it = downloads_.find(uuid);
  if (it == downloads_.end() || !it->second.is_active) {
    LOG(ERROR) << "OnDownloadComplete on inactive/unknown";
    return;
  }

  auto& ctx = it->second;
  network::SimpleURLLoader* loader = ctx.loader.get();
  int net_error = loader ? loader->NetError() : net::ERR_FAILED;
  int http_status = 0;
  if (loader && loader->ResponseInfo() && loader->ResponseInfo()->headers) {
    http_status = loader->ResponseInfo()->headers->response_code();
  }

  const int64_t offset = ctx.pending_offset;

  if (net_error != net::OK || http_status >= 400 || !response_body) {
    LOG(ERROR) << "HLS proxy download failed net_error=" << net_error << " http_status=" << http_status;
    if (devtools_sink_) {
      const int64_t body_len =
          response_body ? static_cast<int64_t>(response_body->size()) : 0;
      // HTTP errors still carry a response head (SetAllowHttpErrorResults) —
      // surface it so the panel shows the status code; transport errors have
      // no head. error_code stays net::OK for HTTP-level failures (the code is
      // in the head).
      if (loader && loader->ResponseInfo()) {
        devtools_sink_->NotifyResponseReceived(ctx.devtools_request_id,
                                               GURL(ctx.url), *loader->ResponseInfo());
      }
      devtools_sink_->NotifyLoadingComplete(ctx.devtools_request_id, net_error,
                                            body_len);
    }
    if (player_) {
      player_->OnDataFinishLoading(
          uuid, MapNetErrorToLoadingRequestError(net_error, http_status));
    }
    ctx.loader.reset();
    return;
  }

  // Analyze the full response chain, then aggregate + push on change.
  // Never touches the data path below — bytes are delivered immediately.
  MaybePushTaint(ComputeResponseTaint(ctx, loader));

  if (player_) {
    if (!ctx.header_sent) {
      std::map<std::string, std::string> resp_header;
      if (loader && loader->ResponseInfo() && loader->ResponseInfo()->headers) {
        net::HttpResponseHeaders* headers = loader->ResponseInfo()->headers.get();
        size_t iter = 0;
        std::string name;
        std::string value;
        while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
          resp_header[name] = value;
        }
      }
      player_->OnDataRespondHeader(uuid, resp_header, /*redirectUrl=*/"");
      ctx.header_sent = true;
    }

    std::vector<uint8_t> data(response_body->begin(), response_body->end());
    player_->OnDataRespondData(uuid, offset, data);
    player_->OnDataFinishLoading(uuid, kLoadingErrorOk);
  }

  if (devtools_sink_) {
    if (loader && loader->ResponseInfo()) {
      devtools_sink_->NotifyResponseReceived(ctx.devtools_request_id,
                                             GURL(ctx.url),
                                             *loader->ResponseInfo());
    }
    devtools_sink_->NotifyLoadingComplete(
        ctx.devtools_request_id, net::OK,
        static_cast<int64_t>(response_body->size()));
  }

  ctx.loader.reset();
}

}  // namespace media