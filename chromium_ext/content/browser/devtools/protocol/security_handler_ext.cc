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

#include "content/browser/devtools/protocol/security_handler.h"

#include "arkweb/build/features/features.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {
namespace protocol {

#if BUILDFLAG(ARKWEB_DEVTOOLS)
void SecurityHandler::CloseIdleConnectionsForArkWeb() {
  if (!host_) {
    return;
  }
  WebContents* web_contents = WebContents::FromRenderFrameHost(host_.get());
  if (!web_contents) {
    return;
  }
  auto* browser_context = web_contents->GetBrowserContext();
  if (!browser_context) {
    return;
  }
  auto* storage_partition = browser_context->GetDefaultStoragePartition();
  if (!storage_partition) {
    return;
  }
  auto* network_context = storage_partition->GetNetworkContext();
  if (!network_context) {
    return;
  }
  // Close all idle connections so that TLS sockets established while
  // ignore=true was in effect are not reused after switching back to
  // ignore=false. Without this, pooled sockets that already completed
  // their TLS handshake would bypass certificate validation until they
  // time out (see stateful_ssl_host_state_delegate.cc comment on
  // RevokeUserAllowExceptionsHard).
  network_context->CloseIdleConnections(base::NullCallback());
}
#endif  // BUILDFLAG(ARKWEB_DEVTOOLS)

}  // namespace protocol
}  // namespace content
