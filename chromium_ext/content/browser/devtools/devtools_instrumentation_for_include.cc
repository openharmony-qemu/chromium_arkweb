// Copyright 2026 The Huawei Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace content {
namespace devtools_instrumentation {

void OnBrowserProcessNetworkResponseReceived(
    FrameTreeNodeId frame_tree_node_id,
    const std::string& request_id,
    const std::string& loader_id,
    const GURL& request_url,
    const network::mojom::URLResponseHead& headers,
    const char* resource_type) {
  network::mojom::URLResponseHeadDevToolsInfoPtr head_info =
      network::ExtractDevToolsInfo(headers);
  DispatchToAgents(frame_tree_node_id,
                   &protocol::NetworkHandler::ResponseReceived, request_id,
                   loader_id, request_url, resource_type, *head_info,
                   base::ToString(frame_tree_node_id));
}
 
void OnBrowserProcessNetworkRequestComplete(
    FrameTreeNodeId frame_tree_node_id,
    const std::string& request_id,
    const network::URLLoaderCompletionStatus& status,
    const char* resource_type) {
  DispatchToAgents(frame_tree_node_id,
                   &protocol::NetworkHandler::LoadingComplete, request_id,
                   resource_type, status);
}

} // devtools_instrumentation
} // content