// Copyright 2026 The Huawei Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_OHOS_MEDIA_SOURCE_DOWNLOAD_MANAGER_H_
#define MEDIA_BASE_OHOS_MEDIA_SOURCE_DOWNLOAD_MANAGER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "arkweb/chromium_ext/media/base/media_player_url_params.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/ohos/devtools_network_event_sink.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/media_adapter.h"
#include "url/origin.h"

namespace media {

// MediaSourceDownloadManager is the arkweb-side core that performs HLS proxy
// downloads through the Chromium network stack (HTTP Range requests) and pushes
// the fetched bytes back to the OHOS player via PlayerAdapter::OnData*.
//
// Lifecycle / threading:
//  - Owned by OHOSMediaPlayerBridge as a unique_ptr; constructed and destroyed
//    on the bridge's task_runner_. Because it never outlives the bridge (which
//    also owns the PlayerAdapter), the non-owning `player_` raw pointer stays
//    valid for the manager's entire lifetime.
//  - It does NOT cross the webview boundary itself. The boundary callback is
//    MediaSourceDataHandlerImpl (its own file), which mirrors OHOSMediaPlayer
//    Callback: it holds a WeakPtr to this manager (via GetWeakPtr()) and
//    PostTasks Open/Read/Close onto task_runner_. Once this manager is
//    destroyed the WeakPtr is invalidated, so any in-flight or late call from
//    the player side becomes a safe no-op — no dangling `this` (the UAF that
//    previously crashed Close while building its BindOnce).
//
// All public methods run on task_runner_ only; each DCHECKs the sequence.
class MediaSourceDownloadManager {
 public:
  MediaSourceDownloadManager(
      OHOS::NWeb::PlayerAdapter* player,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      // Surfaces each proxy download as Network.* events in DevTools. May be
      // null (clients that don't implement it) — emission is then a no-op.
      DevToolsNetworkEventSink* devtools_sink,
      // Page cookie context, stamped onto each ResourceRequest so the network
      // service treats this browser-process proxy download as a same-site
      // request and attaches SameSite=Lax page cookies.
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& top_frame_origin,
      // CORS mode and frame origin (fetch initiator) from the media element,
      // used for aggregated taint analysis of proxy responses.
      MediaCorsMode cors_mode,
      const url::Origin& frame_origin,
      // Callback invoked when the aggregated taint verdict changes.
      base::RepeatingCallback<void(bool tainted)> on_taint_changed);
  ~MediaSourceDownloadManager();

  MediaSourceDownloadManager(const MediaSourceDownloadManager&) = delete;
  MediaSourceDownloadManager& operator=(const MediaSourceDownloadManager&) =
      delete;

  // Weak handle captured at construction (on task_runner_) so the boundary
  // callback can dispatch onto task_runner_ and have its calls safely no-op
  // once this manager is destroyed.
  base::WeakPtr<MediaSourceDownloadManager> GetWeakPtr();

  // Static CORS analysis: returns true if the given response would taint the
  // frame origin. Aligns with network::cors::CheckAccess semantics.
  // - Non-http(s) URLs: taint if frame_origin is opaque or not same-origin.
  // - kUnspecified: taint if not same-origin (no-cors cross-origin → opaque);
  //   opaque frame_origin is always cross-origin with http(s) URLs → taint.
  // - kAnonymous/kUseCredentials: same-origin → clean; cross-origin → check
  //   ACAO/ACAC headers per CORS spec (fail-closed if headers missing).
  //   Opaque frame_origin is NOT short-circuited: it serializes to "null"
  //   in CheckAccess, so ACAO: null yields clean (aligns with sandboxed
  //   document handling in the network layer).
  static bool ResponseTaintsOrigin(MediaCorsMode cors_mode,
                                   const url::Origin& frame_origin,
                                   const GURL& response_url,
                                   const net::HttpResponseHeaders* headers);

  // task_runner_-only entry points, invoked via PostTask from
  // MediaSourceDataHandlerImpl. FIFO ordering on the single task_runner_
  // guarantees an Open insert for a uuid runs before its Read/Close lookup.
  void Open(int64_t uuid,
            const std::string& url,
            const std::map<std::string, std::string>& header);
  void Read(int64_t uuid, int64_t offset, int64_t length);
  void Close(int64_t uuid);

 private:
  struct DownloadContext {
    std::string url;
    std::map<std::string, std::string> header;
    // The active HTTP Range loader for the current read. Reset on Close or
    // when the read completes.
    std::unique_ptr<network::SimpleURLLoader> loader;
    // Byte offset of the in-flight read; echoed back via OnDataRespondData so
    // the player can place the chunk correctly (Range reads / seeks).
    int64_t pending_offset = -1;
    bool is_active = true;
    // Whether OnDataRespondHeader has already been sent for this handle. The
    // OHOS MediaSource contract requires RespondHeader exactly once, before the
    // first RespondData. Guarded so ranged/multi reads on one handle don't
    // resend it.
    bool header_sent = false;
    // DevTools request id stamped on the ResourceRequest and carried across
    // NotifyRequestWillBeSent / NotifyResponseReceived / NotifyLoadingComplete
    // so the inspector can stitch the request lifecycle together. One per read.
    std::string devtools_request_id;
    // Redirect chain captured for CORS analysis. Each entry is
    // (url_before_redirect, response_headers of the redirect response).
    std::vector<std::pair<GURL, scoped_refptr<net::HttpResponseHeaders>>>
        redirect_chain;
  };

  void StartDownload(int64_t uuid,
                     const std::string& url,
                     int64_t offset,
                     int64_t length,
                     const std::map<std::string, std::string>& extra_header);
  void OnDownloadComplete(int64_t uuid,
                          std::unique_ptr<std::string> response_body);
  void OnRedirect(int64_t uuid,
                  const GURL& url_before_redirect,
                  const net::RedirectInfo& redirect_info,
                  const network::mojom::URLResponseHead& response_head,
                  std::vector<std::string>* removed_headers);
  // Analyzes the full response chain (redirect hops + final response) and
  // returns whether it would taint the frame origin. Pure computation —
  // no side effects on member state.
  bool ComputeResponseTaint(const DownloadContext& ctx,
                            const network::SimpleURLLoader* loader) const;
  // Aggregates |tainted| into the monotonic any_response_tainted_ and pushes
  // a fire-and-forget verdict via on_taint_changed_ if the aggregated value
  // changed since the last push.
  void MaybePushTaint(bool tainted);

  std::map<int64_t, DownloadContext> downloads_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  raw_ptr<OHOS::NWeb::PlayerAdapter> player_;  // Non-owning; owned by the bridge.

  // Non-owning. Surfaces proxy downloads in DevTools. Owned by the bridge's
  // client (the renderer), which outlives this manager. May be null.
  raw_ptr<DevToolsNetworkEventSink> devtools_sink_;

  // Page cookie context (see ctor). Stamped onto each ResourceRequest so the
  // network service attaches same-site page cookies.
  net::SiteForCookies site_for_cookies_;
  url::Origin top_frame_origin_;

  // HLS proxy CORS analysis fields
  MediaCorsMode cors_mode_ = MediaCorsMode::kUnspecified;
  url::Origin frame_origin_;
  base::RepeatingCallback<void(bool tainted)> on_taint_changed_;
  // Monotonic OR aggregate within a pipeline lifecycle: starts clean, can
  // only flip to tainted.
  bool any_response_tainted_ = false;
  // Last verdict sent to the renderer. Fail-closed initial: true (matches
  // renderer-side default — a constant-tainted stream sends zero messages).
  bool last_sent_verdict_ = true;

  // Must be the LAST member (Chromium rule): members destruct in reverse
  // declaration order, so this is destroyed first and invalidates all WeakPtrs
  // before any other member tears down — preventing UAF from in-flight
  // callbacks bound through GetWeakPtr().
  base::WeakPtrFactory<MediaSourceDownloadManager> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_BASE_OHOS_MEDIA_SOURCE_DOWNLOAD_MANAGER_H_