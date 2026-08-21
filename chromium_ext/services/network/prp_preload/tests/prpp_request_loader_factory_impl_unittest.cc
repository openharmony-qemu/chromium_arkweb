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

#include "testing/gtest/include/gtest/gtest.h"

#include "arkweb/chromium_ext/services/network/prp_preload/src/preload_runner/prpp_request_loader_factory_impl.h"
#include "base/test/task_environment.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/test/url_loader_context_for_tests.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"

namespace ohos_prp_preload {

class PRPPRequestLoaderFactoryImplTest : public testing::Test {
public:
 static void SetUpTestCase();
 static void TearDownTestCase();
 void SetUp();
 void TearDown();
protected:
 std::unique_ptr<base::test::TaskEnvironment> task_environment_;
};

void PRPPRequestLoaderFactoryImplTest::SetUpTestCase() {}

void PRPPRequestLoaderFactoryImplTest::TearDownTestCase() {}

void PRPPRequestLoaderFactoryImplTest::SetUp() {
  task_environment_ = std::make_unique<base::test::TaskEnvironment>(
    base::test::TaskEnvironment::MainThreadType::DEFAULT);
}

void PRPPRequestLoaderFactoryImplTest::TearDown() {
  task_environment_.reset();
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_001)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  std::shared_ptr<PRPPRequestLoaderFactory> factory_shared =
    PRPPRequestLoaderFactory::CreatePRPPRequestLoaderFactory("page",
    url_request_context->GetWeakPtr());
  EXPECT_TRUE(factory_shared != nullptr);
  
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  std::shared_ptr<PRRequestInfo> info_nullptr = nullptr;
  std::set<std::string> need_record_header_urls;
  factory.CreateReqLoaderAndStart(info_nullptr, need_record_header_urls);

  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_url(GURL("https://some.other.origin/echoall"));
  info->set_preload_flag(PRPP_FLAGS_HDR_DYNAMIC);
  factory.CreateReqLoaderAndStart(info, need_record_header_urls);

  info->set_parent_for_dynamic_header(GURL("https://some.other.origin/echoall"));
  (void)factory.can_reuse_headers_map_.emplace("https://some.other.origin/echoall",
    net::HttpRequestHeaders());
  factory.CreateReqLoaderAndStart(info, need_record_header_urls);

  (void)need_record_header_urls.insert("https://some.other.origin/echoall");
  factory.CreateReqLoaderAndStart(info, need_record_header_urls);
  EXPECT_EQ(factory.need_record_header_urls_.count("https://some.other.origin/echoall"), 1);

  (void)factory.requests_already_start_set_.emplace("https://some.other.origin/echoall");
  factory.CreateReqLoaderAndStart(info, need_record_header_urls);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_002)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  network::URLLoaderContextForTests url_loader_context_for_tests;
  network::ResourceRequest resource_request;
  resource_request.method = "GET";
  resource_request.url = GURL("https://some.other.origin/echoall");
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_method("GET");
  std::shared_ptr<PRRequestInfo> info_binding = std::make_shared<PRRequestInfo>();
  std::shared_ptr<PRPPRequestLoader> loader = PRPPRequestLoader::CreatePRPPRequestLoader(
    "page", info, net::IsolationInfo(), url_request_context->GetWeakPtr(),
    base::BindOnce(&PRPPRequestLoaderFactoryImpl::OnResPreloaded, factory.weak_factory_.GetWeakPtr()));
  (void)factory.prpp_req_loaders_.emplace("https://some.other.origin/echoall", loader);
  EXPECT_EQ(factory.GetPRPPReqLoader(url_loader_context_for_tests, resource_request,
    info_binding), loader);

  EXPECT_EQ(factory.GetPRPPReqLoader(url_loader_context_for_tests, resource_request,
    info_binding), nullptr);

  std::shared_ptr<PRPPRequestLoader> loader_nullptr = nullptr;
  (void)factory.prpp_req_loaders_.emplace("https://some.other.origin/echoall", loader_nullptr);
  EXPECT_EQ(factory.GetPRPPReqLoader(url_loader_context_for_tests, resource_request,
    info_binding), nullptr);

  resource_request.is_preflight = true;
  resource_request.method = "HEAD";
  (void)factory.prpp_req_loaders_.emplace("preflight+https://some.other.origin/echoall", loader);
  EXPECT_EQ(factory.GetPRPPReqLoader(url_loader_context_for_tests, resource_request,
    info_binding), nullptr);

  url::Origin frame_origin = url::Origin::Create(GURL("frame_origin"));
  net::IsolationInfo isolation_info = net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
    frame_origin, frame_origin, net::SiteForCookies::FromOrigin(frame_origin));
  factory.SetPRPPIsolation(isolation_info);
  url_loader_context_for_tests.mutable_factory_params().isolation_info = isolation_info;
  EXPECT_EQ(factory.GetPRPPReqLoader(url_loader_context_for_tests, resource_request,
    info_binding), nullptr);

  resource_request.method = "POST";
  EXPECT_EQ(factory.GetPRPPReqLoader(url_loader_context_for_tests, resource_request,
    info_binding), nullptr);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_003)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  network::URLLoaderContextForTests url_loader_context_for_tests;
  network::ResourceRequest resource_request;
  std::shared_ptr<PRRequestInfo> info_nullptr = nullptr;
  std::shared_ptr<PRRequestInfo> info_binding_nullptr = nullptr;
  EXPECT_EQ(factory.IsInfoMatched(info_nullptr, url_loader_context_for_tests, resource_request,
    info_binding_nullptr), false);

  std::shared_ptr<PRRequestInfo> info_resource = std::make_shared<PRRequestInfo>();
  std::shared_ptr<PRRequestInfo> info_resource_binding = std::make_shared<PRRequestInfo>();
  resource_request.method = "GET";
  info_resource->set_method("GET");
  EXPECT_EQ(factory.IsInfoMatched(info_resource, url_loader_context_for_tests, resource_request,
    info_resource_binding), true);

  resource_request.trust_token_params = network::OptionalTrustTokenParams(
    network::mojom::TrustTokenParams::New());
  EXPECT_EQ(factory.IsInfoMatched(info_resource, url_loader_context_for_tests, resource_request,
    info_resource_binding), false);

  resource_request.request_body = scoped_refptr<network::ResourceRequestBody>(
    new network::ResourceRequestBody());
  EXPECT_EQ(factory.IsInfoMatched(info_resource, url_loader_context_for_tests, resource_request,
    info_resource_binding), false);

  info_resource->set_method("POST");
  EXPECT_EQ(factory.IsInfoMatched(info_resource, url_loader_context_for_tests, resource_request,
    info_resource_binding), false);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_004)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());

  network::ResourceRequest resource_request;
  std::shared_ptr<PRRequestInfo> info_resource = std::make_shared<PRRequestInfo>();
  std::shared_ptr<PRRequestInfo> info_resource_binding = std::make_shared<PRRequestInfo>();
  EXPECT_EQ(factory.MatchRequestHeaders(info_resource, resource_request, info_resource_binding), true);

  net::HttpRequestHeaders resource_request_headers;
  resource_request_headers.SetHeader("header_key_1", "header_value_1");
  resource_request.headers = resource_request_headers;
  EXPECT_EQ(factory.MatchRequestHeaders(info_resource, resource_request, info_resource_binding), false);

  net::HttpRequestHeaders info_resource_headers;
  info_resource_headers.SetHeader("header_key_1", "header_value_1");
  info_resource->set_extra_request_headers(info_resource_headers);
  EXPECT_EQ(factory.MatchRequestHeaders(info_resource, resource_request, info_resource_binding), true);

  resource_request_headers.SetHeader("header_key_2", "header_value_2");
  resource_request.headers = resource_request_headers;
  info_resource_headers.SetHeader("header_key_2", "header_value_not_2");
  info_resource->set_extra_request_headers(info_resource_headers);
  EXPECT_EQ(factory.MatchRequestHeaders(info_resource, resource_request, info_resource_binding), false);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_005)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  EXPECT_EQ(factory.GetMainUrl(), "page");

  PRPPRequestLoader* loader_nullptr = nullptr;
  factory.OnResPreloaded(loader_nullptr);

  std::shared_ptr<PRPPRequestLoader> loader = PRPPRequestLoader::CreatePRPPRequestLoader(
    "page", std::make_shared<PRRequestInfo>(), net::IsolationInfo(), url_request_context->GetWeakPtr(),
    base::BindOnce(&PRPPRequestLoaderFactoryImpl::OnResPreloaded, factory.weak_factory_.GetWeakPtr()));
  factory.OnResPreloaded(loader.get());

  net::IsolationInfo isolation_info;
  factory.SetPRPPIsolation(isolation_info);

  url::Origin frame_origin = url::Origin::Create(GURL("frame_origin"));
  factory.SetPRPPIsolation(net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
    frame_origin, frame_origin, net::SiteForCookies::FromOrigin(frame_origin)));

  factory.DoPendingCreateTask("https://some.other.origin/parent");
  std::shared_ptr<PRRequestInfo> info_resource = std::make_shared<PRRequestInfo>();
  info_resource->set_parent_for_dynamic_header(GURL("https://some.other.origin/parent"));
  factory.RecordPendingPRPPLoader(info_resource);
  factory.RecordPendingPRPPLoader(info_resource);
  factory.DoPendingCreateTask("https://some.other.origin/parent");

  info_resource->set_url(GURL("https://some.other.origin/echoall"));
  factory.CreatePendingReqLoaderAndStart(info_resource);

  (void)factory.can_reuse_headers_map_.emplace("https://some.other.origin/parent",
    net::HttpRequestHeaders());
  factory.CreatePendingReqLoaderAndStart(info_resource);

  (void)factory.requests_already_start_set_.emplace("https://some.other.origin/echoall");
  factory.CreatePendingReqLoaderAndStart(info_resource);

  factory.url_request_context_ = nullptr;
  factory.CreatePendingReqLoaderAndStart(info_resource);

  net::HttpRequestHeaders resource_request_headers;
  resource_request_headers.SetHeader("header_key_1", "header_value_1");
  info_resource->set_dynamic_header_keys(std::set<std::string>({"header_key_1"}));
  factory.ReplaceHeaders(resource_request_headers, info_resource);

  (void)factory.need_record_header_urls_.insert("https://some.other.origin/echoall");
  network::ResourceRequest resource_request;
  resource_request.url = GURL("https://some.other.origin/echoall");
  factory.RecordNeedRecordHeaderUrls(resource_request);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_006)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  base::WeakPtr<PRPPRequestLoaderFactory> weak_ptr = factory.GetWeak();
  EXPECT_TRUE(weak_ptr);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_007)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  factory.url_request_context_ = nullptr;
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_url(GURL("https://some.other.origin/echoall"));
  info->set_method("GET");
  std::set<std::string> need_record_header_urls;
  factory.CreateReqLoaderAndStart(info, need_record_header_urls);
  EXPECT_EQ(factory.prpp_req_loaders_.size(), 0u);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_008)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  network::URLLoaderContextForTests url_loader_context_for_tests;
  network::ResourceRequest resource_request;
  resource_request.method = "DELETE";
  resource_request.url = GURL("https://some.other.origin/echoall");
  std::shared_ptr<PRRequestInfo> info_binding = std::make_shared<PRRequestInfo>();
  EXPECT_EQ(factory.GetPRPPReqLoader(url_loader_context_for_tests, resource_request,
    info_binding), nullptr);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_009)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  network::URLLoaderContextForTests url_loader_context_for_tests;
  network::ResourceRequest resource_request;
  resource_request.method = "GET";
  resource_request.url = GURL("https://some.other.origin/notfound");
  std::shared_ptr<PRRequestInfo> info_binding = std::make_shared<PRRequestInfo>();
  EXPECT_EQ(factory.GetPRPPReqLoader(url_loader_context_for_tests, resource_request,
    info_binding), nullptr);
  EXPECT_NE(factory.requests_already_start_set_.find("https://some.other.origin/notfound"),
    factory.requests_already_start_set_.end());
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_011)
{
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("", base::WeakPtr<net::URLRequestContext>());
  EXPECT_EQ(factory.GetMainUrl(), "");
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_012)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  net::IsolationInfo empty_isolation;
  EXPECT_TRUE(empty_isolation.IsEmpty());
  factory.SetPRPPIsolation(empty_isolation);
  EXPECT_TRUE(factory.isolation_info_.IsEmpty());
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_013)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  url::Origin frame_origin = url::Origin::Create(GURL("https://origin-a.com"));
  net::IsolationInfo isolation_info = net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
    frame_origin, frame_origin, net::SiteForCookies::FromOrigin(frame_origin));
  factory.SetPRPPIsolation(isolation_info);
  EXPECT_FALSE(factory.isolation_info_.IsEmpty());
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_014)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  url::Origin origin_a = url::Origin::Create(GURL("https://origin-a.com"));
  url::Origin origin_b = url::Origin::Create(GURL("https://origin-b.com"));
  net::IsolationInfo factory_isolation = net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
    origin_a, origin_a, net::SiteForCookies::FromOrigin(origin_a));
  net::IsolationInfo context_isolation = net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
    origin_b, origin_b, net::SiteForCookies::FromOrigin(origin_b));
  factory.SetPRPPIsolation(factory_isolation);
  network::URLLoaderContextForTests url_loader_context_for_tests;
  url_loader_context_for_tests.mutable_factory_params().isolation_info = context_isolation;
  network::ResourceRequest resource_request;
  resource_request.method = "GET";
  resource_request.url = GURL("https://some.other.origin/echoall");
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_method("GET");
  std::shared_ptr<PRPPRequestLoader> loader = PRPPRequestLoader::CreatePRPPRequestLoader(
    "page", info, net::IsolationInfo(), url_request_context->GetWeakPtr(),
    base::BindOnce(&PRPPRequestLoaderFactoryImpl::OnResPreloaded, factory.weak_factory_.GetWeakPtr()));
  (void)factory.prpp_req_loaders_.emplace("https://some.other.origin/echoall", loader);
  std::shared_ptr<PRRequestInfo> info_binding = std::make_shared<PRRequestInfo>();
  EXPECT_EQ(factory.GetPRPPReqLoader(url_loader_context_for_tests, resource_request,
    info_binding), nullptr);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_015)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_url(GURL("https://some.other.origin/echoall"));
  info->set_method("GET");
  std::set<std::string> need_record_header_urls;
  need_record_header_urls.insert("https://some.other.origin/echoall");
  factory.CreateReqLoaderAndStart(info, need_record_header_urls);
  EXPECT_EQ(factory.need_record_header_urls_.size(), 1u);
  EXPECT_EQ(factory.need_record_header_urls_.count("https://some.other.origin/echoall"), 1);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_016)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  (void)factory.need_record_header_urls_.insert("https://existing.url/path");
  std::set<std::string> new_urls;
  new_urls.insert("https://new.url/path");
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_url(GURL("https://some.other.origin/echoall"));
  info->set_method("GET");
  factory.CreateReqLoaderAndStart(info, new_urls);
  EXPECT_EQ(factory.need_record_header_urls_.count("https://existing.url/path"), 1);
  EXPECT_EQ(factory.need_record_header_urls_.count("https://new.url/path"), 0);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_017)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_url(GURL("https://some.other.origin/echoall"));
  info->set_method("GET");
  info->set_preload_flag(PRPP_FLAGS_HDR_DYNAMIC);
  info->set_parent_for_dynamic_header(GURL("https://some.other.origin/parent"));
  std::set<std::string> need_record_header_urls;
  factory.CreateReqLoaderAndStart(info, need_record_header_urls);
  EXPECT_NE(factory.pending_prpp_loader_list_.find("https://some.other.origin/parent"),
    factory.pending_prpp_loader_list_.end());
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_019)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  factory.DoPendingCreateTask("nonexist_key");
  EXPECT_EQ(factory.pending_prpp_loader_list_.size(), 0u);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_020)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  std::shared_ptr<PRRequestInfo> info1 = std::make_shared<PRRequestInfo>();
  info1->set_parent_for_dynamic_header(GURL("https://some.other.origin/parent"));
  factory.RecordPendingPRPPLoader(info1);
  std::shared_ptr<PRRequestInfo> info2 = std::make_shared<PRRequestInfo>();
  info2->set_parent_for_dynamic_header(GURL("https://some.other.origin/parent"));
  factory.RecordPendingPRPPLoader(info2);
  EXPECT_EQ(factory.pending_prpp_loader_list_.size(), 1u);
  EXPECT_EQ(factory.pending_prpp_loader_list_["https://some.other.origin/parent"].size(), 2u);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_021)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  std::shared_ptr<PRRequestInfo> info1 = std::make_shared<PRRequestInfo>();
  info1->set_parent_for_dynamic_header(GURL("https://some.other.origin/parent_a"));
  factory.RecordPendingPRPPLoader(info1);
  std::shared_ptr<PRRequestInfo> info2 = std::make_shared<PRRequestInfo>();
  info2->set_parent_for_dynamic_header(GURL("https://some.other.origin/parent_b"));
  factory.RecordPendingPRPPLoader(info2);
  EXPECT_EQ(factory.pending_prpp_loader_list_.size(), 2u);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_022)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  network::ResourceRequest resource_request;
  resource_request.url = GURL("https://some.other.origin/echoall");
  factory.RecordNeedRecordHeaderUrls(resource_request);
  EXPECT_EQ(factory.can_reuse_headers_map_.size(), 0u);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_023)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  (void)factory.need_record_header_urls_.insert("https://some.other.origin/echoall");
  network::ResourceRequest resource_request;
  resource_request.url = GURL("https://some.other.origin/echoall");
  net::HttpRequestHeaders req_headers;
  req_headers.SetHeader("Content-Type", "text/html");
  resource_request.headers = req_headers;
  factory.RecordNeedRecordHeaderUrls(resource_request);
  EXPECT_EQ(factory.can_reuse_headers_map_.count("https://some.other.origin/echoall"), 1u);
  EXPECT_EQ(factory.need_record_header_urls_.count("https://some.other.origin/echoall"), 0u);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_024)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  net::HttpRequestHeaders recorded_headers;
  recorded_headers.SetHeader("Authorization", "new_token");
  recorded_headers.SetHeader("X-Custom", "custom_value");
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_dynamic_header_keys(std::set<std::string>({"Authorization"}));
  net::HttpRequestHeaders info_headers;
  info_headers.SetHeader("Authorization", "old_token");
  info_headers.SetHeader("X-Custom", "original_value");
  info->set_extra_request_headers(info_headers);
  factory.ReplaceHeaders(recorded_headers, info);
  net::HttpRequestHeaders result_headers = info->extra_request_headers();
  std::optional<std::string> auth_value = result_headers.GetHeader("Authorization");
  EXPECT_TRUE(auth_value.has_value());
  EXPECT_EQ(auth_value.value(), "new_token");
  std::optional<std::string> custom_value = result_headers.GetHeader("X-Custom");
  EXPECT_TRUE(custom_value.has_value());
  EXPECT_EQ(custom_value.value(), "original_value");
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_025)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  net::HttpRequestHeaders recorded_headers;
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_dynamic_header_keys(std::set<std::string>({"X-Missing"}));
  net::HttpRequestHeaders info_headers;
  info_headers.SetHeader("X-Missing", "old_value");
  info->set_extra_request_headers(info_headers);
  factory.ReplaceHeaders(recorded_headers, info);
  net::HttpRequestHeaders result_headers = info->extra_request_headers();
  std::optional<std::string> missing_value = result_headers.GetHeader("X-Missing");
  EXPECT_FALSE(missing_value.has_value());
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_026)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  net::HttpRequestHeaders recorded_headers;
  recorded_headers.SetHeader("Accept", "text/html");
  recorded_headers.SetHeader("Authorization", "replaced_token");
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_dynamic_header_keys(std::set<std::string>({"Authorization", "X-Remove-Me"}));
  net::HttpRequestHeaders info_headers;
  info_headers.SetHeader("Authorization", "old_token");
  info_headers.SetHeader("X-Remove-Me", "value");
  info_headers.SetHeader("Accept", "text/html");
  info->set_extra_request_headers(info_headers);
  factory.ReplaceHeaders(recorded_headers, info);
  net::HttpRequestHeaders result_headers = info->extra_request_headers();
  std::optional<std::string> auth_value = result_headers.GetHeader("Authorization");
  EXPECT_TRUE(auth_value.has_value());
  EXPECT_EQ(auth_value.value(), "replaced_token");
  std::optional<std::string> remove_value = result_headers.GetHeader("X-Remove-Me");
  EXPECT_FALSE(remove_value.has_value());
  std::optional<std::string> accept_value = result_headers.GetHeader("Accept");
  EXPECT_TRUE(accept_value.has_value());
  EXPECT_EQ(accept_value.value(), "text/html");
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_027)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  network::URLLoaderContextForTests url_loader_context_for_tests;
  network::ResourceRequest resource_request;
  resource_request.method = "GET";
  resource_request.url = GURL("https://some.other.origin/echoall");
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_method("GET");
  std::shared_ptr<PRRequestInfo> info_binding = std::make_shared<PRRequestInfo>();
  std::shared_ptr<PRPPRequestLoader> loader = PRPPRequestLoader::CreatePRPPRequestLoader(
    "page", info, net::IsolationInfo(), url_request_context->GetWeakPtr(),
    base::BindOnce(&PRPPRequestLoaderFactoryImpl::OnResPreloaded, factory.weak_factory_.GetWeakPtr()));
  (void)factory.prpp_req_loaders_.emplace("https://some.other.origin/echoall", loader);
  EXPECT_EQ(factory.GetPRPPReqLoader(url_loader_context_for_tests, resource_request,
    info_binding), loader);
  EXPECT_EQ(factory.prpp_req_loaders_.size(), 0u);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_028)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  network::URLLoaderContextForTests url_loader_context_for_tests;
  network::ResourceRequest resource_request;
  resource_request.method = "PUT";
  resource_request.url = GURL("https://some.other.origin/echoall");
  std::shared_ptr<PRRequestInfo> info_binding = std::make_shared<PRRequestInfo>();
  EXPECT_EQ(factory.GetPRPPReqLoader(url_loader_context_for_tests, resource_request,
    info_binding), nullptr);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_029)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_url(GURL("https://some.other.origin/echoall"));
  info->set_method("GET");
  std::set<std::string> need_record_header_urls;
  factory.CreateReqLoaderAndStart(info, need_record_header_urls);
  (void)factory.requests_already_start_set_.emplace("https://some.other.origin/echoall");
  factory.CreateReqLoaderAndStart(info, need_record_header_urls);
  EXPECT_EQ(factory.prpp_req_loaders_.size(), 1u);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_031)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_url(GURL("https://some.other.origin/echoall"));
  info->set_method("GET");
  std::set<std::string> empty_urls;
  factory.CreateReqLoaderAndStart(info, empty_urls);
  EXPECT_EQ(factory.need_record_header_urls_.size(), 0u);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_032)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  network::URLLoaderContextForTests url_loader_context_for_tests;
  network::ResourceRequest resource_request;
  resource_request.method = "GET";
  resource_request.url = GURL("https://some.other.origin/echoall");
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_method("GET");
  std::shared_ptr<PRRequestInfo> info_binding = std::make_shared<PRRequestInfo>();
  net::HttpRequestHeaders req_headers;
  req_headers.SetHeader("Accept", "text/html");
  resource_request.headers = req_headers;
  net::HttpRequestHeaders info_headers;
  info_headers.SetHeader("Accept", "text/html");
  info->set_extra_request_headers(info_headers);
  std::shared_ptr<PRPPRequestLoader> loader = PRPPRequestLoader::CreatePRPPRequestLoader(
    "page", info, net::IsolationInfo(), url_request_context->GetWeakPtr(),
    base::BindOnce(&PRPPRequestLoaderFactoryImpl::OnResPreloaded, factory.weak_factory_.GetWeakPtr()));
  (void)factory.prpp_req_loaders_.emplace("https://some.other.origin/echoall", loader);
  EXPECT_EQ(factory.GetPRPPReqLoader(url_loader_context_for_tests, resource_request,
    info_binding), loader);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_033)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  network::URLLoaderContextForTests url_loader_context_for_tests;
  network::ResourceRequest resource_request;
  resource_request.method = "GET";
  resource_request.url = GURL("https://some.other.origin/echoall");
  net::HttpRequestHeaders req_headers;
  req_headers.SetHeader("Accept", "text/html");
  req_headers.SetHeader("Authorization", "Bearer token123");
  resource_request.headers = req_headers;
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_method("GET");
  net::HttpRequestHeaders info_headers;
  info_headers.SetHeader("Accept", "text/html");
  info_headers.SetHeader("Authorization", "Bearer old_token");
  info->set_extra_request_headers(info_headers);
  std::shared_ptr<PRRequestInfo> info_binding = std::make_shared<PRRequestInfo>();
  EXPECT_EQ(factory.IsInfoMatched(info, url_loader_context_for_tests, resource_request,
    info_binding), false);
  EXPECT_NE((info_binding->preload_flag() & PRPP_FLAGS_HDR_DYNAMIC), 0u);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_034)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  network::ResourceRequest resource_request;
  net::HttpRequestHeaders req_headers;
  req_headers.SetHeader("Key-A", "Value-A");
  req_headers.SetHeader("Key-B", "Value-B");
  resource_request.headers = req_headers;
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  net::HttpRequestHeaders info_headers;
  info_headers.SetHeader("Key-A", "Value-A");
  info_headers.SetHeader("Key-B", "Value-B");
  info->set_extra_request_headers(info_headers);
  std::shared_ptr<PRRequestInfo> info_binding = std::make_shared<PRRequestInfo>();
  EXPECT_EQ(factory.MatchRequestHeaders(info, resource_request, info_binding), true);
  EXPECT_EQ(info_binding->preload_flag(), PRPP_FLAGS_NONE);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_035)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  network::ResourceRequest resource_request;
  net::HttpRequestHeaders req_headers;
  req_headers.SetHeader("Key-A", "Value-A");
  req_headers.SetHeader("Key-B", "Value-B");
  resource_request.headers = req_headers;
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  net::HttpRequestHeaders info_headers;
  info_headers.SetHeader("Key-A", "Value-A");
  info_headers.SetHeader("Key-B", "Different-Value");
  info->set_extra_request_headers(info_headers);
  std::shared_ptr<PRRequestInfo> info_binding = std::make_shared<PRRequestInfo>();
  EXPECT_EQ(factory.MatchRequestHeaders(info, resource_request, info_binding), false);
  EXPECT_NE((info_binding->preload_flag() & PRPP_FLAGS_HDR_DYNAMIC), 0u);
  EXPECT_NE(info_binding->dynamic_header_keys().find("Key-B"), info_binding->dynamic_header_keys().end());
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_036)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  network::ResourceRequest resource_request;
  net::HttpRequestHeaders req_headers;
  req_headers.SetHeader("Key-A", "Value-A");
  resource_request.headers = req_headers;
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  net::HttpRequestHeaders info_headers;
  info_headers.SetHeader("Key-A", "Value-A");
  info_headers.SetHeader("Key-Extra", "Extra-Value");
  info->set_extra_request_headers(info_headers);
  std::shared_ptr<PRRequestInfo> info_binding = std::make_shared<PRRequestInfo>();
  EXPECT_EQ(factory.MatchRequestHeaders(info, resource_request, info_binding), false);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_037)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  network::URLLoaderContextForTests url_loader_context_for_tests;
  network::ResourceRequest resource_request;
  resource_request.method = "GET";
  resource_request.url = GURL("https://some.other.origin/echoall");
  net::HttpRequestHeaders req_headers;
  req_headers.SetHeader("Authorization", "Bearer token123");
  resource_request.headers = req_headers;
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_method("GET");
  net::HttpRequestHeaders info_headers;
  info_headers.SetHeader("Authorization", "Bearer old_token");
  info->set_extra_request_headers(info_headers);
  std::shared_ptr<PRPPRequestLoader> loader = PRPPRequestLoader::CreatePRPPRequestLoader(
    "page", info, net::IsolationInfo(), url_request_context->GetWeakPtr(),
    base::BindOnce(&PRPPRequestLoaderFactoryImpl::OnResPreloaded, factory.weak_factory_.GetWeakPtr()));
  (void)factory.prpp_req_loaders_.emplace("https://some.other.origin/echoall", loader);
  std::shared_ptr<PRRequestInfo> info_binding = std::make_shared<PRRequestInfo>();
  EXPECT_EQ(factory.GetPRPPReqLoader(url_loader_context_for_tests, resource_request,
    info_binding), nullptr);
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_039)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_url(GURL("https://some.other.origin/echoall"));
  info->set_method("GET");
  std::set<std::string> need_record_header_urls;
  factory.CreateReqLoaderAndStart(info, need_record_header_urls);
  EXPECT_NE(factory.prpp_req_loaders_.find("https://some.other.origin/echoall"),
    factory.prpp_req_loaders_.end());
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_040)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_url(GURL("https://some.other.origin/echoall"));
  info->set_method("GET");
  factory.CreatePendingReqLoaderAndStart(info);
  auto it = factory.prpp_req_loaders_.find("https://some.other.origin/echoall");
  EXPECT_NE(it, factory.prpp_req_loaders_.end());
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_041)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_url(GURL("https://some.other.origin/echoall"));
  info->set_method("GET");
  (void)factory.requests_already_start_set_.emplace("https://some.other.origin/echoall");
  factory.CreatePendingReqLoaderAndStart(info);
  EXPECT_EQ(factory.prpp_req_loaders_.find("https://some.other.origin/echoall"),
    factory.prpp_req_loaders_.end());
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_042)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
  info->set_url(GURL("https://some.other.origin/echoall"));
  info->set_method("GET");
  info->set_preload_flag(PRPP_FLAGS_VISIBLE);
  std::set<std::string> need_record_header_urls;
  factory.CreateReqLoaderAndStart(info, need_record_header_urls);
  EXPECT_NE(factory.prpp_req_loaders_.find("https://some.other.origin/echoall"),
    factory.prpp_req_loaders_.end());
}

TEST_F(PRPPRequestLoaderFactoryImplTest, PRPPRequestLoaderFactoryImplTest_043)
{
  std::unique_ptr<net::URLRequestContext> url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
  PRPPRequestLoaderFactoryImpl factory =
    PRPPRequestLoaderFactoryImpl("page", url_request_context->GetWeakPtr());
  (void)factory.need_record_header_urls_.insert("https://some.other.origin/echoall");
  (void)factory.need_record_header_urls_.insert("https://some.other.origin/second");
  network::ResourceRequest resource_request;
  resource_request.url = GURL("https://some.other.origin/echoall");
  net::HttpRequestHeaders req_headers;
  req_headers.SetHeader("Accept", "text/html");
  resource_request.headers = req_headers;
  factory.RecordNeedRecordHeaderUrls(resource_request);
  EXPECT_EQ(factory.can_reuse_headers_map_.count("https://some.other.origin/echoall"), 1u);
  EXPECT_EQ(factory.need_record_header_urls_.size(), 1u);
  EXPECT_EQ(factory.need_record_header_urls_.count("https://some.other.origin/second"), 1u);
}

} // namespace ohos_prp_preload