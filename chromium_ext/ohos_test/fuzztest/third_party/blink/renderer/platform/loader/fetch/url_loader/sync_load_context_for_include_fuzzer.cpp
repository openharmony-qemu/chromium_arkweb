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
#include "base/memory/ptr_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "third_party/bounds_checking_function/include/securec.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_sender.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/sync_load_response.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#define private public
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/sync_load_context.h"
#undef private
namespace blink {
namespace {
class FuzzSharedURLLoaderFactory : public network::TestURLLoaderFactory,
                                  public network::SharedURLLoaderFactory {
 public:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    network::TestURLLoaderFactory::CreateLoaderAndStart(
        std::move(receiver), request_id, options, url_request,
        std::move(client), traffic_annotation);
  }
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory>) override {
    NOTREACHED();
  }
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
    NOTREACHED();
  }
 private:
  friend class base::RefCounted<FuzzSharedURLLoaderFactory>;
  ~FuzzSharedURLLoaderFactory() override = default;
};
class FuzzPendingSharedURLLoaderFactory
    : public network::PendingSharedURLLoaderFactory {
 public:
  FuzzPendingSharedURLLoaderFactory()
      : factory_(base::MakeRefCounted<FuzzSharedURLLoaderFactory>()) {}
 protected:
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override {
    return factory_;
  }
  scoped_refptr<FuzzSharedURLLoaderFactory> factory_;
};
}  // namespace
static base::ReadOnlySharedMemoryRegion CreateTestSharedMemoryRegion(
    const std::string& data) {
  if (data.empty()) {
    return base::ReadOnlySharedMemoryRegion();
  }
  auto mapping_region =
      base::ReadOnlySharedMemoryRegion::Create(data.size());
  if (memcpy_s(mapping_region.mapping.GetMemoryAs<uint8_t>(),
               mapping_region.mapping.size(), data.c_str(),
               data.size()) != EOK) {
    return base::ReadOnlySharedMemoryRegion();
  }
  return std::move(mapping_region.region);
}
#if BUILDFLAG(ARKWEB_RESOURCE_INTERCEPTION)
static scoped_refptr<SyncLoadContext> CreateFuzzContext(
    network::ResourceRequest* request,
    SyncLoadResponse* response,
    SyncLoadContext** context_for_redirect,
    base::WaitableEvent* redirect_or_response_event,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return base::AdoptRef(new SyncLoadContext(
      request, std::make_unique<FuzzPendingSharedURLLoaderFactory>(), response,
      context_for_redirect, redirect_or_response_event,
      nullptr /* terminate_sync_load_event */,
      base::Seconds(60) /* timeout */,
      mojo::NullRemote() /* download_to_blob_registry */, task_runner,
      nullptr /* render_thread */));
}
void FuzzOnTransferDataWithSharedMemoryValid(FuzzedDataProvider* fdp) {
  std::string data = fdp->ConsumeRandomLengthString(1024);
  uint64_t buffer_size = fdp->ConsumeIntegralInRange<uint64_t>(0, data.size());
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("https://example.com");
  SyncLoadResponse response;
  SyncLoadContext* context_for_redirect = nullptr;
  base::WaitableEvent redirect_or_response_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  auto context = CreateFuzzContext(request.get(), &response,
                                   &context_for_redirect,
                                   &redirect_or_response_event, task_runner);
  auto region = CreateTestSharedMemoryRegion(data);
  context->OnTransferDataWithSharedMemory(std::move(region), buffer_size);
}
void FuzzOnTransferDataWithSharedMemoryInvalid(FuzzedDataProvider* fdp) {
  uint64_t buffer_size = fdp->ConsumeIntegralInRange<uint64_t>(0, 8192);
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("https://example.com");
  SyncLoadResponse response;
  SyncLoadContext* context_for_redirect = nullptr;
  base::WaitableEvent redirect_or_response_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  auto context = CreateFuzzContext(request.get(), &response,
                                   &context_for_redirect,
                                   &redirect_or_response_event, task_runner);
  base::ReadOnlySharedMemoryRegion invalid_region;
  context->OnTransferDataWithSharedMemory(std::move(invalid_region),
                                          buffer_size);
}
void FuzzOnTransferDataWithSharedMemorySizeMismatch(FuzzedDataProvider* fdp) {
  std::string data = fdp->ConsumeRandomLengthString(256);
  // buffer_size deliberately larger than the actual region to exercise the
  // base::span boundary logic in OnTransferDataWithSharedMemory.
  uint64_t buffer_size = fdp->ConsumeIntegralInRange<uint64_t>(
      data.size() + 1, data.size() + 4096);
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("https://example.com");
  SyncLoadResponse response;
  SyncLoadContext* context_for_redirect = nullptr;
  base::WaitableEvent redirect_or_response_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  auto context = CreateFuzzContext(request.get(), &response,
                                   &context_for_redirect,
                                   &redirect_or_response_event, task_runner);
  auto region = CreateTestSharedMemoryRegion(data);
  context->OnTransferDataWithSharedMemory(std::move(region), buffer_size);
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
  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 2);
  switch (selector) {
#if BUILDFLAG(ARKWEB_RESOURCE_INTERCEPTION)
    case 0:
      blink::FuzzOnTransferDataWithSharedMemoryValid(&fdp);
      break;
    case 1:
      blink::FuzzOnTransferDataWithSharedMemoryInvalid(&fdp);
      break;
    case 2:
      blink::FuzzOnTransferDataWithSharedMemorySizeMismatch(&fdp);
      break;
#endif
    default:
#if BUILDFLAG(ARKWEB_RESOURCE_INTERCEPTION)
      blink::FuzzOnTransferDataWithSharedMemoryValid(&fdp);
#endif
      break;
  }
  return 0;
}