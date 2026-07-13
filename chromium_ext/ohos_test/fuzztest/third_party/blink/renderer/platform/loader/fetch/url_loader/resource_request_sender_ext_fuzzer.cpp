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
#include <fuzzer/FuzzedDataProvider.h>
#include <cstring>
#include <memory>
#include <string>
#include "arkweb/build/features/features.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/bounds_checking_function/include/securec.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_client.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#define private public
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_sender.h"
#undef private
namespace blink {
class FuzzResourceRequestClient : public ResourceRequestClient {
 public:
  FuzzResourceRequestClient() = default;
  ~FuzzResourceRequestClient() override = default;
  using FollowRedirectCallback =
      base::OnceCallback<void(std::vector<std::string> removed_headers,
                              net::HttpRequestHeaders modified_headers)>;
  void OnUploadProgress(uint64_t position, uint64_t size) override {}
  void OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          network::mojom::URLResponseHeadPtr head,
                          FollowRedirectCallback follow_redirect_callback) override {}
  void OnReceivedResponse(network::mojom::URLResponseHeadPtr head,
                          mojo::ScopedDataPipeConsumerHandle body,
                          std::optional<mojo_base::BigBuffer> cached_metadata) override {}
#if BUILDFLAG(ARKWEB_RESOURCE_INTERCEPTION)
  void OnTransferDataWithSharedMemory(base::ReadOnlySharedMemoryRegion region,
                                      uint64_t buffer_size) override {}
#endif
  void OnTransferSizeUpdated(int transfer_size_diff) override {}
  void OnCompletedRequest(const network::URLLoaderCompletionStatus& status) override {}
};
static base::ReadOnlySharedMemoryRegion CreateTestSharedMemoryRegion(
    const std::string& data) {
  if (data.empty()) {
    return base::ReadOnlySharedMemoryRegion();
  }
  auto writable_region = base::WritableSharedMemoryRegion::Create(data.size());
  auto mapping = writable_region.Map();
  if (memcpy_s(mapping.memory(), mapping.size(), data.c_str(), data.size()) !=
      0) {
    return base::ReadOnlySharedMemoryRegion();
  }
  return base::WritableSharedMemoryRegion::ConvertToReadOnly(
      std::move(writable_region));
}
#if BUILDFLAG(ARKWEB_RESOURCE_INTERCEPTION)
void FuzzOnTransferDataWithSharedMemoryNullRequestInfo(FuzzedDataProvider* fdp,
                                                      ResourceRequestSender* sender) {
  std::string data = fdp->ConsumeRandomLengthString(1024);
  uint64_t buffer_size = fdp->ConsumeIntegralInRange<uint64_t>(0, data.size());
  auto region = CreateTestSharedMemoryRegion(data);
  sender->OnTransferDataWithSharedMemory(std::move(region), buffer_size);
}
void FuzzOnTransferDataWithSharedMemoryWithClient(FuzzedDataProvider* fdp,
                                                  ResourceRequestSender* sender) {
  std::string data = fdp->ConsumeRandomLengthString(1024);
  uint64_t buffer_size = fdp->ConsumeIntegralInRange<uint64_t>(0, data.size());
  auto region = CreateTestSharedMemoryRegion(data);
  auto mock_client = base::MakeRefCounted<FuzzResourceRequestClient>();
  sender->request_info_ =
      std::make_unique<ResourceRequestSender::PendingRequestInfo>(
          mock_client, network::mojom::RequestDestination::kDocument,
          KURL("https://example.com"), nullptr);
  sender->OnTransferDataWithSharedMemory(std::move(region), buffer_size);
}
void FuzzOnTransferDataWithSharedMemoryWithNotifier(FuzzedDataProvider* fdp,
                                                    ResourceRequestSender* sender) {
  std::string data = fdp->ConsumeRandomLengthString(1024);
  uint64_t buffer_size = fdp->ConsumeIntegralInRange<uint64_t>(0, data.size());
  auto region = CreateTestSharedMemoryRegion(data);
  auto mock_client = base::MakeRefCounted<FuzzResourceRequestClient>();
  auto notifier = std::make_unique<ResourceLoadInfoNotifierWrapper>(
      base::WeakPtr<WeakWrapperResourceLoadInfoNotifier>(nullptr));
  sender->request_info_ =
      std::make_unique<ResourceRequestSender::PendingRequestInfo>(
          mock_client, network::mojom::RequestDestination::kDocument,
          KURL("https://example.com"), std::move(notifier));
  sender->OnTransferDataWithSharedMemory(std::move(region), buffer_size);
}
void FuzzOnTransferDataWithSharedMemoryEmptyRegion(FuzzedDataProvider* fdp,
                                                   ResourceRequestSender* sender) {
  uint64_t buffer_size = fdp->ConsumeIntegralInRange<uint64_t>(0, 4096);
  base::ReadOnlySharedMemoryRegion empty_region;
  auto mock_client = base::MakeRefCounted<FuzzResourceRequestClient>();
  sender->request_info_ =
      std::make_unique<ResourceRequestSender::PendingRequestInfo>(
          mock_client, network::mojom::RequestDestination::kDocument,
          KURL("https://example.com"), nullptr);
  sender->OnTransferDataWithSharedMemory(std::move(empty_region), buffer_size);
}
#endif  // BUILDFLAG(ARKWEB_RESOURCE_INTERCEPTION)
}  // namespace blink
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(int32_t)) {
    return 0;
  }
  static blink::BlinkFuzzerTestSupport test_support;
  blink::test::TaskEnvironment task_environment;
  FuzzedDataProvider fdp(data, size);
  auto sender = std::make_unique<blink::ResourceRequestSender>();
  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 3);
  switch (selector) {
#if BUILDFLAG(ARKWEB_RESOURCE_INTERCEPTION)
    case 0:
      blink::FuzzOnTransferDataWithSharedMemoryNullRequestInfo(&fdp, sender.get());
      break;
    case 1:
      blink::FuzzOnTransferDataWithSharedMemoryWithClient(&fdp, sender.get());
      break;
    case 2:
      blink::FuzzOnTransferDataWithSharedMemoryWithNotifier(&fdp, sender.get());
      break;
    case 3:
      blink::FuzzOnTransferDataWithSharedMemoryEmptyRegion(&fdp, sender.get());
      break;
#endif
    default:
#if BUILDFLAG(ARKWEB_RESOURCE_INTERCEPTION)
      blink::FuzzOnTransferDataWithSharedMemoryNullRequestInfo(&fdp, sender.get());
#endif
      break;
  }
  return 0;
}