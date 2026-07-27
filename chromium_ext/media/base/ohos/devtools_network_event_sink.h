// Copyright 2026 The Huawei Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_OHOS_DEVTOOLS_NETWORK_EVENT_SINK_H_
#define MEDIA_BASE_OHOS_DEVTOOLS_NETWORK_EVENT_SINK_H_

#include <cstdint>
#include <string>

#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace media {

// Lets MediaSourceDownloadManager (media layer, cannot depend on
// content/browser/devtools) surface an HLS proxy download as Network.* events
// in DevTools. The concrete impl lives in the content layer
// (OHOSMediaPlayerRenderer) and forwards each call to devtools_instrumentation's
// browser-process Network event emission (OnAuctionWorkletNetworkRequest*).
// All methods are invoked on the manager's task_runner_ (UI); impls DCHECK UI.
// A null sink (default) makes every call a no-op.
class DevToolsNetworkEventSink {
 public:
  virtual ~DevToolsNetworkEventSink() = default;
  virtual void NotifyRequestWillBeSent(
      const std::string& request_id,
      const network::ResourceRequest& request) = 0;
  virtual void NotifyResponseReceived(
      const std::string& request_id,
      const GURL& url,
      const network::mojom::URLResponseHead& head) = 0;
  // Primitives by design so the media layer need not depend on
  // network::URLLoaderCompletionStatus.
  virtual void NotifyLoadingComplete(const std::string& request_id,
                                     int net_error_code,
                                     int64_t encoded_data_length) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_OHOS_DEVTOOLS_NETWORK_EVENT_SINK_H_