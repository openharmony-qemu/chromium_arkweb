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

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)

TEST_F(HostResolverManagerDnsTest, CanUseSecureDnsFallback) {
  EXPECT_FALSE(resolver_->CanUseSecureDnsFallback(resolve_context_.get()));

  resolver_->dns_client_ = nullptr;
  EXPECT_FALSE(resolver_->CanUseSecureDnsFallback(resolve_context_.get()));
}

TEST_F(HostResolverManagerDnsTest, SetHttpsDnsFallbackData) {
  auto request_context1 = CreateTestURLRequestContextBuilder()->Build();
  auto request_context2 = CreateTestURLRequestContextBuilder()->Build();
  ResolveContext context1(request_context1.get(), true);
  ResolveContext context2(request_context2.get(), true);

  resolver_->RegisterResolveContext(&context1);
  resolver_->RegisterResolveContext(&context2);
  resolver_->SetHttpsDnsFallbackData(true, "https://dns.example.com");
  EXPECT_FALSE(resolver_->warmup_httpdns_fallback_list_.empty());
  resolver_->DeregisterResolveContext(&context1);
  resolver_->DeregisterResolveContext(&context2);
  EXPECT_TRUE(resolver_->https_dns_fallback_enabled_);
}

TEST_F(HostResolverManagerDnsTest, SetSuspectIpListAndSourceHostList) {
  std::vector<std::string> ip_list = {"1.2.3.4", "5.6.7.8"};
  std::vector<std::string> host_list = {"example.com", "test.org"};

  resolver_->SetSuspectIpListAndSourceHostList(ip_list, host_list);
  // Verify the call doesn't crash - the actual storage is in global state
  // within secure_dns_fallback_utils.cc

  // Clean up global state
  resolver_->SetSuspectIpListAndSourceHostList({}, {});
}

TEST_F(HostResolverManagerDnsTest, GetLocalAddress_NoDnsClient) {
  IPEndPoint address;
  resolver_->dns_client_ = nullptr;
  // Should not crash when dns_client_ is nullptr
  EXPECT_NO_FATAL_FAILURE(resolver_->GetLocalAddress(&address));
}

TEST_F(HostResolverManagerDnsTest, GetLocalAddress_WithDnsClient) {
  IPEndPoint address;
  // resolver_->dns_client_ is set up by CreateResolverWithOptionsAndParams
  // in HostResolverManagerDnsTest fixture
  EXPECT_NO_FATAL_FAILURE(resolver_->GetLocalAddress(&address));
}

TEST_F(HostResolverManagerDnsTest,
       ReportSecureFallbackDnsResult_WithInsecureResults) {
  std::vector<IPEndPoint> cache_address = {{{192, 168, 1, 1}, 80},
                                           {{192, 167, 1, 1}, 80}};
  constexpr base::TimeDelta kTtl = base::Seconds(3600);
  HostCache::Entry insecure_entry(OK, cache_address, std::set<std::string>(),
                                  HostCache::Entry::Source::SOURCE_DNS, kTtl);
  std::optional<HostCache::Entry> insecure_results(insecure_entry);

  HostResolverDnsTask::Results secure_fallback_results;
  auto data_result = std::make_unique<HostResolverInternalDataResult>(
      std::string("example.com"), DnsQueryType::A, std::nullopt, base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::vector<IPEndPoint>{
          IPEndPoint(IPAddress(10, 0, 0, 1), 443),
          IPEndPoint(IPAddress(10, 0, 1, 1), 443),
      },
      std::vector<std::string>(), std::vector<HostPortPair>());
  secure_fallback_results.insert(std::move(data_result));

  constexpr base::TimeDelta duration = base::Seconds(100);
  HostCache::Entry secure_entry(secure_fallback_results, base::Time::Now(),
                                base::TimeTicks::Now());
  // This should not crash. The function reports statistics and logs.
  EXPECT_NO_FATAL_FAILURE(resolver_->ReportSecureFallbackDnsResult(
      insecure_results, secure_entry, "example.com", 0, duration));
}

TEST_F(HostResolverManagerDnsTest,
       ReportSecureFallbackDnsResult_EmptyInsecureResults) {
  std::vector<IPEndPoint> cache_address = {};
  constexpr base::TimeDelta kTtl = base::Seconds(3600);
  HostCache::Entry insecure_entry(OK, cache_address, std::set<std::string>(),
                                  HostCache::Entry::Source::SOURCE_DNS, kTtl);
  std::optional<HostCache::Entry> insecure_results(insecure_entry);

  // Secure fallback with error (no endpoints)
  HostResolverDnsTask::Results secure_fallback_results;
  auto error_result = std::make_unique<HostResolverInternalErrorResult>(
      std::string("example.com"), DnsQueryType::A, std::nullopt, std::nullopt,
      HostResolverInternalResult::Source::kDns, ERR_DNS_SERVER_FAILED);
  secure_fallback_results.insert(std::move(error_result));

  constexpr base::TimeDelta duration = base::Seconds(100);
  HostCache::Entry secure_entry(secure_fallback_results, base::Time::Now(),
                                base::TimeTicks::Now());
  EXPECT_NO_FATAL_FAILURE(resolver_->ReportSecureFallbackDnsResult(
      insecure_results, secure_entry, "example.com", 1, duration));
}

TEST_F(HostResolverManagerDnsTest,
       ReportSecureFallbackDnsResult_NoInsecureResults) {
  // No insecure results (nullopt)
  std::optional<HostCache::Entry> insecure_results;

  HostResolverDnsTask::Results secure_fallback_results;
  auto data_result = std::make_unique<HostResolverInternalDataResult>(
      std::string("example.com"), DnsQueryType::A, std::nullopt, base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::vector<IPEndPoint>{
          IPEndPoint(IPAddress(10, 0, 0, 1), 443),
      },
      std::vector<std::string>(), std::vector<HostPortPair>());
  secure_fallback_results.insert(std::move(data_result));

  constexpr base::TimeDelta duration = base::Seconds(100);
  HostCache::Entry secure_entry(secure_fallback_results, base::Time::Now(),
                                base::TimeTicks::Now());
  EXPECT_NO_FATAL_FAILURE(resolver_->ReportSecureFallbackDnsResult(
      insecure_results, secure_entry, "example.com", 0, duration));
}

TEST_F(HostResolverManagerDnsTest, ReportDnsTransactionResult) {
  // Index 0 = local DNS, index 1 = https DNS
  EXPECT_NO_FATAL_FAILURE(resolver_->ReportDnsTransactionResult(
      0, "example.com", OK, ERR_DNS_SERVER_FAILED));
  EXPECT_NO_FATAL_FAILURE(resolver_->ReportDnsTransactionResult(
      1, "test.org", ERR_NAME_NOT_RESOLVED, OK));
}

TEST_F(HostResolverManagerDnsTest, CleanLocalTask_AllLocal) {
  std::deque<HostResolverManager::TaskType> tasks;
  tasks.push_back(HostResolverManager::TaskType::CACHE_LOOKUP);
  tasks.push_back(HostResolverManager::TaskType::INSECURE_CACHE_LOOKUP);
  tasks.push_back(HostResolverManager::TaskType::HOSTS);

  resolver_->CleanLocalTask(tasks);
  EXPECT_TRUE(tasks.empty());
}

TEST_F(HostResolverManagerDnsTest, CleanLocalTask_Mixed) {
  std::deque<HostResolverManager::TaskType> tasks;
  tasks.push_back(HostResolverManager::TaskType::CACHE_LOOKUP);
  tasks.push_back(HostResolverManager::TaskType::INSECURE_CACHE_LOOKUP);
  tasks.push_back(HostResolverManager::TaskType::DNS);

  resolver_->CleanLocalTask(tasks);
  ASSERT_EQ(tasks.size(), 1u);
  EXPECT_EQ(tasks.front(), HostResolverManager::TaskType::DNS);
}

TEST_F(HostResolverManagerDnsTest, CleanLocalTask_NoLocalAtFront) {
  std::deque<HostResolverManager::TaskType> tasks;
  tasks.push_back(HostResolverManager::TaskType::DNS);
  tasks.push_back(HostResolverManager::TaskType::CACHE_LOOKUP);

  resolver_->CleanLocalTask(tasks);
  // Should not remove anything since front is not a local task
  ASSERT_EQ(tasks.size(), 2u);
  EXPECT_EQ(tasks.front(), HostResolverManager::TaskType::DNS);
}

TEST_F(HostResolverManagerDnsTest, CleanLocalTask_Empty) {
  std::deque<HostResolverManager::TaskType> tasks;
  resolver_->CleanLocalTask(tasks);
  EXPECT_TRUE(tasks.empty());
}
#endif  // BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK_ON_DNS_HIJACKING)

TEST_F(HostResolverManagerDnsTest, SetHttpsDnsFallbackDataOnDnsHijacking) {
  std::vector<std::string> protect_list = {"example.com", "test.org"};
  std::vector<std::string> errorcode_list = {"ERR_NAME_NOT_RESOLVED",
                                             "ERR_CONNECTION_TIMED_OUT"};

  resolver_->SetHttpsDnsFallbackDataOnDnsHijacking(protect_list,
                                                   errorcode_list);
  EXPECT_EQ(resolver_->dns_hijacking_protect_list_.size(), 2u);
  EXPECT_NE(resolver_->dns_hijacking_protect_list_.find("example.com"),
            resolver_->dns_hijacking_protect_list_.end());
  EXPECT_NE(resolver_->dns_hijacking_protect_list_.find("test.org"),
            resolver_->dns_hijacking_protect_list_.end());
  EXPECT_EQ(resolver_->dns_hijacking_errorcode_list_.size(), 2u);
  EXPECT_NE(
      resolver_->dns_hijacking_errorcode_list_.find("ERR_NAME_NOT_RESOLVED"),
      resolver_->dns_hijacking_errorcode_list_.end());
  EXPECT_NE(
      resolver_->dns_hijacking_errorcode_list_.find("ERR_CONNECTION_TIMED_OUT"),
      resolver_->dns_hijacking_errorcode_list_.end());
  resolver_->SetHttpsDnsFallbackDataOnDnsHijacking({}, {});
  EXPECT_TRUE(resolver_->dns_hijacking_protect_list_.empty());
  EXPECT_TRUE(resolver_->dns_hijacking_errorcode_list_.empty());
}

TEST_F(HostResolverManagerDnsTest, IsProtectedDomain) {
  std::vector<std::string> protect_list = {"example.com", "test.org",
                                           "sub.domain.com"};
  resolver_->SetHttpsDnsFallbackDataOnDnsHijacking(protect_list,
                                                   {"ERR_DNS_FAIL"});
  GURL empty_url;
  EXPECT_FALSE(resolver_->IsProtectedDomain(empty_url));
  GURL subdomain("http://sub.domain.com");
  EXPECT_TRUE(resolver_->IsProtectedDomain(subdomain));
  GURL not_match("http://notprotected.com");
  EXPECT_FALSE(resolver_->IsProtectedDomain(not_match));
  resolver_->SetHttpsDnsFallbackDataOnDnsHijacking({}, {});
  GURL test_url("http://example.com");
  EXPECT_FALSE(resolver_->IsProtectedDomain(test_url));
}

TEST_F(HostResolverManagerDnsTest, NeedRetryDnsOnDnsHijack) {
  std::vector<std::string> protect_list = {"example.com", "test.org"};
  std::vector<std::string> error_list = {"ERR_DNS_FAIL",
                                         "ERR_CONNECTION_TIMED_OUT"};
  resolver_->SetHttpsDnsFallbackDataOnDnsHijacking(protect_list, error_list);
  GURL protected_url("http://example.com");
  EXPECT_TRUE(
      resolver_->NeedRetryDnsOnDnsHijack(protected_url, "ERR_DNS_FAIL"));
  EXPECT_FALSE(
      resolver_->NeedRetryDnsOnDnsHijack(protected_url, "ERR_OTHER_ERROR"));
  GURL non_protected_url("http://notprotected.com");
  EXPECT_FALSE(
      resolver_->NeedRetryDnsOnDnsHijack(non_protected_url, "ERR_DNS_FAIL"));
}
#endif  // BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK_ON_DNS_HIJACKING)

#if BUILDFLAG(ARKWEB_EXT_NAVIGATION)
TEST_F(HostResolverManagerDnsTest, GetDnsServersString_NoDnsClient) {
  resolver_->dns_client_ = nullptr;
  EXPECT_EQ(resolver_->GetDnsServersString(), std::string());
}

TEST_F(HostResolverManagerDnsTest, GetDnsServersString_WithDnsClient) {
  // dns_client_ is set up by HostResolverManagerDnsTest fixture
  std::string result = resolver_->GetDnsServersString();
  // Result depends on the mock DNS client configuration
  EXPECT_NO_FATAL_FAILURE(resolver_->GetDnsServersString());
}
#endif  // BUILDFLAG(ARKWEB_EXT_NAVIGATION)
