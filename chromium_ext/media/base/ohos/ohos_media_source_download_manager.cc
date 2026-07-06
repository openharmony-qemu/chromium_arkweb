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
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
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
    const url::Origin& top_frame_origin)
    : task_runner_(std::move(task_runner)),
      url_loader_factory_(std::move(url_loader_factory)),
      player_(player),
      devtools_sink_(devtools_sink),
      site_for_cookies_(site_for_cookies),
      top_frame_origin_(top_frame_origin) {
  DCHECK(task_runner_);
}

MediaSourceDownloadManager::~MediaSourceDownloadManager() = default;

base::WeakPtr<MediaSourceDownloadManager>
MediaSourceDownloadManager::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
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

  // Success: feed the fetched bytes back to the player.
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
      devtools_sink_->NotifyResponseReceived(ctx.devtools_request_id, GURL(ctx.url),
                                             *loader->ResponseInfo());
    }
    devtools_sink_->NotifyLoadingComplete(
        ctx.devtools_request_id, net::OK,
        static_cast<int64_t>(response_body->size()));
  }

  ctx.loader.reset();
}

}  // namespace media