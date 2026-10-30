/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "arkweb/build/features/features.h"  // nogncheck
#include "base/containers/span.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/load_flags.h"
#include "net/base/network_anonymization_key.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/cookie_settings.h"
#include "services/network/observer_wrapper.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/shared_resource_checker.h"
#include "services/network/test/url_loader_context_for_tests.h"
#include "services/network/url_loader_context.h"
#include "testing/gtest/include/gtest/gtest.h"

// clang-format off
// Make URLLoader private members accessible from the test fixture. This must
// follow all includes above so that heavy transitive headers (libc++ <ranges>,
// quiche intrusive lists, etc.) are parsed with the correct access specifier
// and their include guards make the re-inclusion inside the macro window a
// no-op.
#define private public
#include "services/network/url_loader.h"
#include "arkweb/chromium_ext/services/network/url_loader_ext.h"
#undef private
// clang-format on

#include "arkweb/chromium_ext/services/network/url_loader_utils.h"

#if BUILDFLAG(ARKWEB_PRP_PRELOAD)
#include "arkweb/chromium_ext/net/base/page_res_request_info.h"
#include "arkweb/chromium_ext/services/network/prp_preload/include/preload_runner/prpp_request_loader.h"
#include "arkweb/chromium_ext/services/network/prp_preload/src/preload_runner/prpp_request_loader_impl.h"
#endif

namespace network {
namespace {

using ::testing::Test;

class URLLoaderUtilsTest : public Test {
 public:
  URLLoaderUtilsTest() = default;
  ~URLLoaderUtilsTest() override = default;

  void SetUp() override {
    task_environment_ = std::make_unique<base::test::TaskEnvironment>(
        base::test::TaskEnvironment::MainThreadType::IO);
    context_ = std::make_unique<URLLoaderContextForTests>();
    url_request_context_ = net::CreateTestURLRequestContextBuilder()->Build();
    context_->set_url_request_context(url_request_context_.get());
    context_->mutable_factory_params().isolation_info =
        net::IsolationInfo::CreateForInternalRequest(
            url::Origin::Create(GURL("http://test.test/")));
  }

  void TearDown() override {
    url_request_context_.reset();
    context_.reset();
    task_environment_.reset();
  }

  std::unique_ptr<URLLoader> CreateURLLoader(
      ResourceRequest request = ResourceRequest()) {
    if (request.url.is_empty()) {
      request.url = GURL("http://test.test/");
    }
    if (request.method.empty()) {
      request.method = "GET";
    }
    mojo::PendingRemote<mojom::URLLoaderClient> client;
    mojo::Remote<mojom::URLLoader> remote;
    return std::make_unique<URLLoader>(
        *context_, base::BindOnce([](URLLoader*) {}),
        remote.BindNewPipeAndPassReceiver(), mojom::kURLLoadOptionNone, request,
        std::move(client), nullptr, TRAFFIC_ANNOTATION_FOR_TESTS, 0, 0, nullptr,
        nullptr, nullptr, nullptr,
        ObserverWrapper<mojom::CookieAccessObserver>(),
        ObserverWrapper<mojom::TrustTokenAccessObserver>(),
        ObserverWrapper<mojom::URLLoaderNetworkServiceObserver>(),
        ObserverWrapper<mojom::DevToolsObserver>(),
        ObserverWrapper<mojom::DeviceBoundSessionAccessObserver>(),
        mojo::NullRemote(), false,
#if BUILDFLAG(ARKWEB_PRP_PRELOAD)
        nullptr, std::string(), nullptr,
#endif
        *shared_resource_checker_, nullptr);
  }

  // Returns a non-const reference to the request's response info so tests can
  // seed cached/SPDY state before exercising the trace helpers.
  net::HttpResponseInfo& MutableResponseInfo(net::URLRequest* request) {
    return const_cast<net::HttpResponseInfo&>(request->response_info());
  }

 protected:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  std::unique_ptr<URLLoaderContextForTests> context_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  std::unique_ptr<SharedResourceChecker> shared_resource_checker_ =
      std::make_unique<SharedResourceChecker>(CookieSettings());
};

#if BUILDFLAG(ARKWEB_PERFORMANCE_NETWORK_TRACE)
TEST_F(URLLoaderUtilsTest, InMilliseconds_ReturnsMsString) {
  auto url_loader = CreateURLLoader();
  URLLoaderUtils utils(url_loader.get(), false);
  base::TimeTicks t = base::TimeTicks() + base::Milliseconds(123);
  EXPECT_EQ(utils.InMilliseconds(t), "123");
}

TEST_F(URLLoaderUtilsTest, PrintNetworkTimingInfo_HasRequest) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  URLLoaderUtils utils(url_loader.get(), false);
  utils.PrintNetworkTimingInfo();
}

TEST_F(URLLoaderUtilsTest, PrintNetworkCacheInfo_HasResponseHeaders) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  url_loader->response_ = mojom::URLResponseHead::New();
  url_loader->response_->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  URLLoaderUtils utils(url_loader.get(), false);
  utils.PrintNetworkCacheInfo();
}

TEST_F(URLLoaderUtilsTest, PrintNetworkInfo_HasRequestNoResponse) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  URLLoaderUtils utils(url_loader.get(), false);
  utils.PrintNetworkInfo();
}

TEST_F(URLLoaderUtilsTest, PrintNetworkInfo_HasResponseHeaders) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  url_loader->response_ = mojom::URLResponseHead::New();
  url_loader->response_->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  URLLoaderUtils utils(url_loader.get(), false);
  utils.PrintNetworkInfo();
}

TEST_F(URLLoaderUtilsTest, PrintNetworkInfo_RawResponseHeadersEnabled) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  url_loader->response_ = mojom::URLResponseHead::New();
  url_loader->response_->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  url_loader->raw_response_headers_ =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  url_loader->enable_reporting_raw_headers_ = true;
  URLLoaderUtils utils(url_loader.get(), false);
  utils.PrintNetworkInfo();
}

TEST_F(URLLoaderUtilsTest, PrintNetworkInfo_NullRequestNullResponse) {
  auto url_loader = CreateURLLoader();
  URLLoaderUtils utils(url_loader.get(), false);
  utils.PrintNetworkInfo();
}

TEST_F(URLLoaderUtilsTest, GetRequestHttpVersion_Spdy) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  MutableResponseInfo(req.get()).was_fetched_via_spdy = true;
  URLLoaderUtils utils(url_loader.get(), false);
  EXPECT_EQ(utils.GetRequestHttpVersion(), "http/2.0");
}

TEST_F(URLLoaderUtilsTest, GetRequestHttpVersion_Http09) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  MutableResponseInfo(req.get()).headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/0.9 200 OK");
  URLLoaderUtils utils(url_loader.get(), false);
  EXPECT_EQ(utils.GetRequestHttpVersion(), "http/0.9");
}

TEST_F(URLLoaderUtilsTest, GetRequestHttpVersion_Http10) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  MutableResponseInfo(req.get()).headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.0 200 OK");
  URLLoaderUtils utils(url_loader.get(), false);
  EXPECT_EQ(utils.GetRequestHttpVersion(), "http/1.0");
}

TEST_F(URLLoaderUtilsTest, GetRequestHttpVersion_Http11) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  MutableResponseInfo(req.get()).headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  URLLoaderUtils utils(url_loader.get(), false);
  EXPECT_EQ(utils.GetRequestHttpVersion(), "http/1.1");
}

TEST_F(URLLoaderUtilsTest, GetRequestHttpVersion_Default) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  MutableResponseInfo(req.get()).headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/2.0 200 OK");
  URLLoaderUtils utils(url_loader.get(), false);
  EXPECT_EQ(utils.GetRequestHttpVersion(), "http/2.0");
}
#endif  // ARKWEB_PERFORMANCE_NETWORK_TRACE

#if BUILDFLAG(ARKWEB_PRP_PRELOAD)
TEST_F(URLLoaderUtilsTest, UpdateResRequestInfo_FlagNone) {
  auto url_loader = CreateURLLoader();
  URLLoaderUtils utils(url_loader.get(), false);
  auto info = std::make_shared<ohos_prp_preload::PRRequestInfo>();
  info->set_preload_flag(ohos_prp_preload::PRPP_FLAGS_NONE);
  utils.UpdateResRequestInfo("key", info);
  EXPECT_FALSE(utils.already_update_info_);
}

TEST_F(URLLoaderUtilsTest, UpdateResRequestInfo_ImageResourceType) {
  ResourceRequest request;
  request.resource_type = RESOURCE_TYPE_IMAGE;
  auto url_loader = CreateURLLoader(std::move(request));
  URLLoaderUtils utils(url_loader.get(), false);
  auto info = std::make_shared<ohos_prp_preload::PRRequestInfo>();
  info->set_preload_flag(ohos_prp_preload::PRPP_FLAGS_NONE);
  utils.UpdateResRequestInfo("key", info);
  EXPECT_TRUE(utils.already_update_info_);
}

TEST_F(URLLoaderUtilsTest, ConfigureRequestForRollback_Basic) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> rollback =
      url_request_context_->CreateRequestForPrpp(
          GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
          TRAFFIC_ANNOTATION_FOR_TESTS);
  URLLoaderUtils utils(url_loader.get(), false);
  utils.url_request_rollback_ = rollback;
  utils.ConfigureRequestForRollback(
      GURL("http://test.test/"), "GET", net::SiteForCookies(), false, {},
      GURL(), net::ReferrerPolicy::NEVER_CLEAR, false, false, std::nullopt,
      false, net::SecureDnsPolicy::kAllow, net::HttpRequestHeaders(),
      std::nullopt, std::nullopt,
      net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL, 0, false,
      net::CookieSettingOverrides(), std::nullopt);
  EXPECT_EQ(rollback->method(), "GET");
}

TEST_F(URLLoaderUtilsTest, ConfigureRequestForRollback_WithUrlChain) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> rollback =
      url_request_context_->CreateRequestForPrpp(
          GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
          TRAFFIC_ANNOTATION_FOR_TESTS);
  URLLoaderUtils utils(url_loader.get(), false);
  utils.url_request_rollback_ = rollback;
  std::vector<GURL> url_chain = {GURL("http://test.test/a")};
  utils.ConfigureRequestForRollback(
      GURL("http://test.test/"), "GET", net::SiteForCookies(), false, url_chain,
      GURL("http://test.test/referrer"), net::ReferrerPolicy::NEVER_CLEAR, true,
      true,
      net::IsolationInfo::CreateForInternalRequest(
          url::Origin::Create(GURL("http://test.test/"))),
      true, net::SecureDnsPolicy::kDisable, net::HttpRequestHeaders(),
      std::nullopt, std::nullopt,
      net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT,
      net::LOAD_NORMAL, true, net::CookieSettingOverrides(), std::nullopt);
  EXPECT_EQ(rollback->method(), "GET");
}

TEST_F(URLLoaderUtilsTest, RollbackFromPPRP_SetsRequest) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> rollback =
      url_request_context_->CreateRequestForPrpp(
          GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
          TRAFFIC_ANNOTATION_FOR_TESTS);
  URLLoaderUtils utils(url_loader.get(), false);
  utils.url_request_rollback_ = rollback;
  utils.RollbackFromPPRP();
  EXPECT_EQ(url_loader->url_request_, rollback);
  EXPECT_EQ(utils.url_request_rollback_, nullptr);
}

TEST_F(URLLoaderUtilsTest, ResetUrlRequest_NoPrppLoader) {
  auto url_loader = CreateURLLoader();
  URLLoaderUtils utils(url_loader.get(), false);
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  utils.ResetUrlRequest(req);
}

TEST_F(URLLoaderUtilsTest, ResetUrlRequest_WithPrppLoader) {
  auto url_loader = CreateURLLoader();
  auto info = std::make_shared<ohos_prp_preload::PRRequestInfo>();
  std::shared_ptr<ohos_prp_preload::PRPPRequestLoader> loader =
      ohos_prp_preload::PRPPRequestLoader::CreatePRPPRequestLoader(
          std::string(), info, net::IsolationInfo(),
          url_request_context_->GetWeakPtr(),
          base::BindOnce([](ohos_prp_preload::PRPPRequestLoader*) {}));
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  URLLoaderUtils utils(url_loader.get(), false);
  utils.prpp_loader_ = loader;
  utils.ResetUrlRequest(req);
  EXPECT_EQ(url_loader->url_request_, req);
}

TEST_F(URLLoaderUtilsTest, SetPrppLoaderCallbacks_SetsRequest) {
  auto url_loader = CreateURLLoader();
  auto info = std::make_shared<ohos_prp_preload::PRRequestInfo>();
  std::shared_ptr<ohos_prp_preload::PRPPRequestLoader> loader =
      ohos_prp_preload::PRPPRequestLoader::CreatePRPPRequestLoader(
          std::string(), info, net::IsolationInfo(),
          url_request_context_->GetWeakPtr(),
          base::BindOnce([](ohos_prp_preload::PRPPRequestLoader*) {}));
  URLLoaderUtils utils(url_loader.get(), false);
  utils.SetPrppLoaderCallbacks(loader);
  EXPECT_NE(utils.prpp_loader_, nullptr);
  EXPECT_EQ(url_loader->url_request_, loader->GetURLRequest());
}

TEST_F(URLLoaderUtilsTest, ConfigureAndCreateURLRequest_SharedStorageEligible) {
  auto url_loader = CreateURLLoader();
  auto info = std::make_shared<ohos_prp_preload::PRRequestInfo>();
  URLLoaderUtils utils(url_loader.get(), false);
  ResourceRequest request;
  request.url = GURL("http://test.test/");
  request.method = "GET";
  utils.ConfigureAndCreateURLRequest(request, TRAFFIC_ANNOTATION_FOR_TESTS,
                                     true, info);
  EXPECT_NE(url_loader->url_request_, nullptr);
  EXPECT_TRUE(info->preload_flag() & ohos_prp_preload::PRPP_FLAGS_UNSUPPORT);
}

TEST_F(URLLoaderUtilsTest, UpdatePreconnectInfo_NullPreloadInfo) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  URLLoaderUtils utils(url_loader.get(), false);
  utils.UpdatePreconnectInfo();
}

TEST_F(URLLoaderUtilsTest, UpdatePreconnectInfo_WithPreloadInfo) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req =
      url_request_context_->CreateRequestForPrpp(
          GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
          TRAFFIC_ANNOTATION_FOR_TESTS);
  auto info = std::make_shared<ohos_prp_preload::PRRequestInfo>();
  req->set_preload_info(info);
  url_loader->url_request_ = req;
  URLLoaderUtils utils(url_loader.get(), false);
  utils.UpdatePreconnectInfo();
}

TEST_F(URLLoaderUtilsTest, SetPreloadFlagUnSupport_NullRequest) {
  auto url_loader = CreateURLLoader();
  URLLoaderUtils utils(url_loader.get(), false);
  utils.SetPreloadFlagUnSupport();
}

TEST_F(URLLoaderUtilsTest, SetPreloadFlagUnSupport_WithPreloadInfo) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req =
      url_request_context_->CreateRequestForPrpp(
          GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
          TRAFFIC_ANNOTATION_FOR_TESTS);
  auto info = std::make_shared<ohos_prp_preload::PRRequestInfo>();
  req->set_preload_info(info);
  url_loader->url_request_ = req;
  URLLoaderUtils utils(url_loader.get(), false);
  utils.already_update_info_ = true;
  utils.SetPreloadFlagUnSupport();
  EXPECT_TRUE(info->preload_flag() & ohos_prp_preload::PRPP_FLAGS_UNSUPPORT);
}

TEST_F(URLLoaderUtilsTest, SetPreloadFlagVisible_NotImage) {
  auto url_loader = CreateURLLoader();
  URLLoaderUtils utils(url_loader.get(), false);
  utils.SetPreloadFlagVisible(net::HIGHEST);
}

TEST_F(URLLoaderUtilsTest, SetPreloadFlagVisible_ImageLowPriority) {
  ResourceRequest request;
  request.resource_type = RESOURCE_TYPE_IMAGE;
  auto url_loader = CreateURLLoader(std::move(request));
  URLLoaderUtils utils(url_loader.get(), false);
  utils.SetPreloadFlagVisible(net::LOWEST);
}

TEST_F(URLLoaderUtilsTest, SetPreloadFlagVisible_ImageWithPreloadInfo) {
  ResourceRequest request;
  request.resource_type = RESOURCE_TYPE_IMAGE;
  auto url_loader = CreateURLLoader(std::move(request));
  std::shared_ptr<net::URLRequest> req =
      url_request_context_->CreateRequestForPrpp(
          GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
          TRAFFIC_ANNOTATION_FOR_TESTS);
  auto info = std::make_shared<ohos_prp_preload::PRRequestInfo>();
  info->set_preload_flag(ohos_prp_preload::PRPP_FLAGS_NONE);
  req->set_preload_info(info);
  url_loader->url_request_ = req;
  URLLoaderUtils utils(url_loader.get(), false);
  utils.already_update_info_ = true;
  utils.SetPreloadFlagVisible(net::MEDIUM);
  EXPECT_TRUE(info->preload_flag() & ohos_prp_preload::PRPP_FLAGS_VISIBLE);
}

TEST_F(URLLoaderUtilsTest, UpdateResponseTimes_NoPrppLoader) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  mojom::URLResponseHeadPtr response = mojom::URLResponseHead::New();
  net::LoadTimingInfo load_timing_info;
  URLLoaderUtils utils(url_loader.get(), false);
  utils.UpdateResponseTimes(response, load_timing_info);
}

TEST_F(URLLoaderUtilsTest, UpdateResponseTimes_WithPrppLoaderCached) {
  auto url_loader = CreateURLLoader();
  auto info = std::make_shared<ohos_prp_preload::PRRequestInfo>();
  std::shared_ptr<ohos_prp_preload::PRPPRequestLoader> loader =
      ohos_prp_preload::PRPPRequestLoader::CreatePRPPRequestLoader(
          std::string(), info, net::IsolationInfo(),
          url_request_context_->GetWeakPtr(),
          base::BindOnce([](ohos_prp_preload::PRPPRequestLoader*) {}));
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  MutableResponseInfo(req.get()).was_cached = true;
  mojom::URLResponseHeadPtr response = mojom::URLResponseHead::New();
  net::LoadTimingInfo load_timing_info;
  URLLoaderUtils utils(url_loader.get(), false);
  utils.prpp_loader_ = loader;
  utils.UpdateResponseTimes(response, load_timing_info);
}

TEST_F(URLLoaderUtilsTest, UpdateResponseTimes_WithPrppLoaderNotCached) {
  auto url_loader = CreateURLLoader();
  auto info = std::make_shared<ohos_prp_preload::PRRequestInfo>();
  std::shared_ptr<ohos_prp_preload::PRPPRequestLoader> loader =
      ohos_prp_preload::PRPPRequestLoader::CreatePRPPRequestLoader(
          std::string(), info, net::IsolationInfo(),
          url_request_context_->GetWeakPtr(),
          base::BindOnce([](ohos_prp_preload::PRPPRequestLoader*) {}));
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  mojom::URLResponseHeadPtr response = mojom::URLResponseHead::New();
  net::LoadTimingInfo load_timing_info;
  URLLoaderUtils utils(url_loader.get(), false);
  utils.prpp_loader_ = loader;
  utils.UpdateResponseTimes(response, load_timing_info);
}

TEST_F(URLLoaderUtilsTest, GetResponseLoadingTime_Disabled) {
  auto url_loader = CreateURLLoader();
  mojom::URLResponseHeadPtr response = mojom::URLResponseHead::New();
  URLLoaderUtils utils(url_loader.get(), false);
  utils.GetResponseLoadingTime(response);
}

TEST_F(URLLoaderUtilsTest, GetResponseLoadingTime_NoPrppLoader) {
  ResourceRequest request;
  request.enable_load_timing = true;
  auto url_loader = CreateURLLoader(std::move(request));
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  mojom::URLResponseHeadPtr response = mojom::URLResponseHead::New();
  URLLoaderUtils utils(url_loader.get(), false);
  utils.GetResponseLoadingTime(response);
}

TEST_F(URLLoaderUtilsTest, GetResponseLoadingTime_WithPrppLoader) {
  ResourceRequest request;
  request.enable_load_timing = true;
  auto url_loader = CreateURLLoader(std::move(request));
  auto info = std::make_shared<ohos_prp_preload::PRRequestInfo>();
  std::shared_ptr<ohos_prp_preload::PRPPRequestLoader> loader =
      ohos_prp_preload::PRPPRequestLoader::CreatePRPPRequestLoader(
          std::string(), info, net::IsolationInfo(),
          url_request_context_->GetWeakPtr(),
          base::BindOnce([](ohos_prp_preload::PRPPRequestLoader*) {}));
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  mojom::URLResponseHeadPtr response = mojom::URLResponseHead::New();
  URLLoaderUtils utils(url_loader.get(), false);
  utils.prpp_loader_ = loader;
  utils.GetResponseLoadingTime(response);
}

TEST_F(URLLoaderUtilsTest, SetRequestStartTime_NoPrppLoader) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  mojom::URLResponseHeadPtr response = mojom::URLResponseHead::New();
  net::LoadTimingInfo load_timing_info;
  URLLoaderUtils utils(url_loader.get(), false);
  utils.SetRequestStartTime(response, load_timing_info);
}

TEST_F(URLLoaderUtilsTest, SetRequestStartTime_WithPrppLoader) {
  auto url_loader = CreateURLLoader();
  auto info = std::make_shared<ohos_prp_preload::PRRequestInfo>();
  std::shared_ptr<ohos_prp_preload::PRPPRequestLoader> loader =
      ohos_prp_preload::PRPPRequestLoader::CreatePRPPRequestLoader(
          std::string(), info, net::IsolationInfo(),
          url_request_context_->GetWeakPtr(),
          base::BindOnce([](ohos_prp_preload::PRPPRequestLoader*) {}));
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  mojom::URLResponseHeadPtr response = mojom::URLResponseHead::New();
  net::LoadTimingInfo load_timing_info;
  URLLoaderUtils utils(url_loader.get(), false);
  utils.prpp_loader_ = loader;
  utils.SetRequestStartTime(response, load_timing_info);
}

TEST_F(URLLoaderUtilsTest, HandleRedirectUrl_EmptyMainUrl) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("http://test.test/new");
  URLLoaderUtils utils(url_loader.get(), false);
  utils.HandleRedirectUrl(redirect_info);
}

TEST_F(URLLoaderUtilsTest, HandleRedirectUrl_NonEmptyMainUrl) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  req->set_main_url(GURL("http://test.test/main"));
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("http://test.test/new");
  URLLoaderUtils utils(url_loader.get(), false);
  utils.HandleRedirectUrl(redirect_info);
}

TEST_F(URLLoaderUtilsTest, SetUrlRequestForPRPP_EmptyMainUrl) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  auto info = std::make_shared<ohos_prp_preload::PRRequestInfo>();
  ResourceRequest request;
  request.url = GURL("http://test.test/");
  request.method = "GET";
  URLLoaderUtils utils(url_loader.get(), false);
  utils.SetUrlRequestForPRPP(request, req, std::string(), info);
  EXPECT_EQ(req->preload_info(), nullptr);
}

TEST_F(URLLoaderUtilsTest, SetUrlRequestForPRPP_WithMainUrlNoFlags) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  auto info = std::make_shared<ohos_prp_preload::PRRequestInfo>();
  ResourceRequest request;
  request.url = GURL("http://test.test/");
  request.method = "GET";
  request.main_url = GURL("http://test.test/main");
  URLLoaderUtils utils(url_loader.get(), false);
  utils.SetUrlRequestForPRPP(request, req, std::string(), info);
  EXPECT_NE(req->preload_info(), nullptr);
  EXPECT_EQ(req->main_url(), GURL("http://test.test/main"));
}

TEST_F(URLLoaderUtilsTest, SetUrlRequestForPRPP_WithMainUrlFlagsAndOrgUrl) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  auto info = std::make_shared<ohos_prp_preload::PRRequestInfo>();
  ResourceRequest request;
  request.url = GURL("http://test.test/");
  request.method = "GET";
  request.main_url = GURL("http://test.test/main");
  request.allow_preload_record = true;
  request.is_preflight = true;
  URLLoaderUtils utils(url_loader.get(), false);
  utils.SetUrlRequestForPRPP(request, req, "http://test.test/org", info);
  EXPECT_NE(req->preload_info(), nullptr);
  EXPECT_EQ(req->main_url(), GURL("http://test.test/org"));
  EXPECT_EQ(req->allow_preload_record(), true);
}

TEST_F(URLLoaderUtilsTest, HandlePrppLoaderStartReplay_NullLoader) {
  auto url_loader = CreateURLLoader();
  auto info = std::make_shared<ohos_prp_preload::PRRequestInfo>();
  std::shared_ptr<ohos_prp_preload::PRPPRequestLoader> loader;
  ResourceRequest request;
  request.url = GURL("http://test.test/");
  request.method = "GET";
  URLLoaderUtils utils(url_loader.get(), false);
  bool result = utils.HandlePrppLoaderStartReplay(
      loader, *context_, request, TRAFFIC_ANNOTATION_FOR_TESTS, nullptr,
      std::string(), info, false);
  EXPECT_FALSE(result);
}

TEST_F(URLLoaderUtilsTest, GetLoadTimeInfo_NoPrppLoader) {
  auto url_loader = CreateURLLoader();
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  net::LoadTimingInfo load_timing_info;
  URLLoaderUtils utils(url_loader.get(), false);
  utils.GetLoadTimeInfo(&load_timing_info);
}

TEST_F(URLLoaderUtilsTest, GetLoadTimeInfo_WithPrppLoader) {
  auto url_loader = CreateURLLoader();
  auto info = std::make_shared<ohos_prp_preload::PRRequestInfo>();
  std::shared_ptr<ohos_prp_preload::PRPPRequestLoader> loader =
      ohos_prp_preload::PRPPRequestLoader::CreatePRPPRequestLoader(
          std::string(), info, net::IsolationInfo(),
          url_request_context_->GetWeakPtr(),
          base::BindOnce([](ohos_prp_preload::PRPPRequestLoader*) {}));
  std::shared_ptr<net::URLRequest> req = url_request_context_->CreateRequest(
      GURL("http://test.test/"), net::DEFAULT_PRIORITY, url_loader.get(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->url_request_ = req;
  net::LoadTimingInfo load_timing_info;
  URLLoaderUtils utils(url_loader.get(), false);
  utils.prpp_loader_ = loader;
  utils.GetLoadTimeInfo(&load_timing_info);
}

TEST_F(URLLoaderUtilsTest, ReadDataFromLoaderOrRequest_WithPrppLoader) {
  auto url_loader = CreateURLLoader();
  auto info = std::make_shared<ohos_prp_preload::PRRequestInfo>();
  std::shared_ptr<ohos_prp_preload::PRPPRequestLoader> loader =
      ohos_prp_preload::PRPPRequestLoader::CreatePRPPRequestLoader(
          std::string(), info, net::IsolationInfo(),
          url_request_context_->GetWeakPtr(),
          base::BindOnce([](ohos_prp_preload::PRPPRequestLoader*) {}));
  url_loader->pending_write_buffer_size_ = 1024;
  url_loader->pending_write_buffer_offset_ = 0;
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  MojoResult rv = mojo::CreateDataPipe(1024, producer, consumer);
  ASSERT_EQ(rv, MOJO_RESULT_OK);
  scoped_refptr<NetToMojoPendingBuffer> pending;
  rv = NetToMojoPendingBuffer::BeginWrite(&producer, &pending);
  ASSERT_EQ(rv, MOJO_RESULT_OK);
  scoped_refptr<NetToMojoIOBuffer> buf =
      base::MakeRefCounted<NetToMojoIOBuffer>(pending);
  URLLoaderUtils utils(url_loader.get(), false);
  utils.prpp_loader_ = loader;
  int bytes = utils.ReadDataFromLoaderOrRequest(buf);
  EXPECT_EQ(bytes, net::ERR_IO_PENDING);
}
#endif  // ARKWEB_PRP_PRELOAD

}  // namespace
}  // namespace network
