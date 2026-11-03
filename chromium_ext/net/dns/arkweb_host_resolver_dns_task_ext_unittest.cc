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

#include <gtest/gtest.h>
#ifdef UNSAFE_BUFFERS_BUILD
#pragma allow_unsafe_buffers
#endif

#include "base/containers/linked_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
// clang-format off
#undef NET_EXPORT_PRIVATE
#define private public
#include "net/dns/host_resolver_dns_task.h"
#include "net/dns/host_resolver_manager_job.h"
#include "arkweb/chromium_ext/net/dns/arkweb_host_resolver_dns_task_ext.h"
#include "arkweb/chromium_ext/net/dns/arkweb_host_resolver_manager_job_ext.h"
#undef private
// clang-format on
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/host_resolver_manager_unittest.h"
#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
#include "arkweb/chromium_ext/content/public/common/content_switches_ext.h"
#include "arkweb/chromium_ext/net/dns/secure_dns_fallback_utils.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#endif

namespace net {
#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
class MockDnsTransaction : public DnsTransaction {
 public:
  MockDnsTransaction(const std::string& name, uint16_t type)
      : name_(name), type_(type) {}
  ~MockDnsTransaction() override = default;

  const std::string& GetHostname() const override { return name_; }

  // Returns the |qtype|.
  uint16_t GetType() const override { return type_; }

  // Starts the transaction.  Always completes asynchronously.
  void Start(ResponseCallback callback) {}

  void SetRequestPriority(RequestPriority priority) override{};

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
  void SetNotNeedMoreAttemptIPQueryType(
      uint16_t not_need_more_another_ip_query_type) override {
    not_need_more_another_ip_query_type_ = not_need_more_another_ip_query_type;
  }
#endif
  uint16_t GetQueryType() { return not_need_more_another_ip_query_type_; }

 private:
  std::string name_;
  uint16_t type_;
  uint16_t not_need_more_another_ip_query_type_;
};

class ArkWebHostResolverDnsTest : public HostResolverManagerTest {
 public:
  explicit ArkWebHostResolverDnsTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::MOCK_TIME)
      : HostResolverManagerTest(time_source) {}

  ~ArkWebHostResolverDnsTest() override {}

  void CreateTaskWithoutRequest(
      absl::variant<url::SchemeHostPort, HostPortPair> host) {
    HostResolverManager::JobKey job_key(HostResolver::Host(std::move(host)),
                                        resolve_context_.get());
    auto job = resolver_->AddJobWithoutRequest(
        job_key, HostResolver::ResolveHostParameters::CacheUsage::ALLOWED,
        &host_cache_, {HostResolverManager::TaskType::SECURE_DNS},
        RequestPriority::LOW, NetLogWithSource());

    job->dns_task_ = std::make_unique<HostResolverDnsTask>(
        resolver_->dns_client_.get(), job->key().host,
        job->key().network_anonymization_key, job->key().query_types,
        &*job->key().resolve_context, false, job->key().secure_dns_mode, job,
        job->net_log_, job->tick_clock_,
        !job->tasks_.empty() /* fallback_available */,
#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
        false /* secure_fallback */,
#endif
        job->https_svcb_options_);
    dns_task_ = job->dns_task_.get();
    dns_task_ext_ = std::make_unique<ArkWebHostResolverDnsTaskExt>(dns_task_);
  }

  void SetTaskSniffIpResult(bool result) {
    dns_task_->need_to_sniff_ip_result_ = result;
  }

  void SetTaskSavedResults(int net_error) {
    HostCache::Entry results(net_error, HostCache::Entry::SOURCE_UNKNOWN,
                             std::nullopt);
    // dns_task_->saved_results_ = std::move(results);
  }
  void AddTransactionInfo(
      DnsQueryType type,
      HostResolverDnsTask::TransactionErrorBehavior error_behavior) {
    HostResolverDnsTask::TransactionInfo transaction_info(type, error_behavior);
    transaction_info.transaction = std::make_unique<MockDnsTransaction>(
        "test", DnsQueryTypeToQtype(transaction_info.type));
    dns_task_->transactions_in_progress_.insert(std::move(transaction_info));
  }

  void CheckTransactionInProcess() {
    for (auto& transaction_info : dns_task_->transactions_in_progress_) {
      if (transaction_info.type == DnsQueryType::A ||
          transaction_info.type == DnsQueryType::AAAA) {
        MockDnsTransaction* trans = static_cast<MockDnsTransaction*>(
            transaction_info.transaction.get());
        EXPECT_EQ(trans->GetQueryType(), trans->GetType());
      }
    }
  }

  void AddNeededTransactionInfo(
      DnsQueryType type,
      HostResolverDnsTask::TransactionErrorBehavior error_behavior) {
    dns_task_->transactions_needed_.emplace_back(type, error_behavior);
  }

 protected:
  void SetUp() override { HostResolverManagerTest::SetUp(); }
  void TearDown() override { HostResolverManagerTest::TearDown(); }

  ArkWebHostResolverDnsTaskExt* GetDnsTaskExt() { return dns_task_ext_.get(); }

 private:
  HostCache host_cache_{1};
  std::unique_ptr<ArkWebHostResolverDnsTaskExt> dns_task_ext_;
  HostResolverDnsTask* dns_task_;
};

TEST_F(ArkWebHostResolverDnsTest, ArkWebSetNotNeedQueryType) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);
  int legacy_results_error = 1;
  // legacy_results_error ok, no process
  EXPECT_NO_FATAL_FAILURE(dns_task->ArkWebFailedTransaction(
      legacy_results_error, DnsQueryType::HTTPS));

  // no switches::kEnableNwebExHttpDnsFallback
  legacy_results_error = OK;
  EXPECT_NO_FATAL_FAILURE(dns_task->ArkWebSetNotNeedQueryType(
      legacy_results_error, DnsQueryType::HTTPS));

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableNwebExHttpDnsFallback);
  // none AAA
  EXPECT_NO_FATAL_FAILURE(dns_task->ArkWebSetNotNeedQueryType(
      legacy_results_error, DnsQueryType::HTTPS));

  EXPECT_NO_FATAL_FAILURE(dns_task->ArkWebSetNotNeedQueryType(
      legacy_results_error, DnsQueryType::AAAA));
}

TEST_F(ArkWebHostResolverDnsTest, ArkWebFailedTransaction_Invalid) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);
  int net_error = 0;
  // no value, no process
  EXPECT_NO_FATAL_FAILURE(
      dns_task->ArkWebFailedTransaction(net_error, std::nullopt));

  // has value, but none AddressType
  EXPECT_NO_FATAL_FAILURE(dns_task->ArkWebFailedTransaction(
      net_error, std::make_optional<DnsQueryType>(DnsQueryType::HTTPS)));
}

TEST_F(ArkWebHostResolverDnsTest,
       ArkWebFailedTransaction_AnyAOrAAAATransactionRemain) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);
  int net_error = 0;
  AddTransactionInfo(
      DnsQueryType::A,
      HostResolverDnsTask::TransactionErrorBehavior::kFatalOrEmpty);
  AddTransactionInfo(
      DnsQueryType::AAAA,
      HostResolverDnsTask::TransactionErrorBehavior::kFatalOrEmpty);
  AddTransactionInfo(
      DnsQueryType::HTTPS,
      HostResolverDnsTask::TransactionErrorBehavior::kFatalOrEmpty);

  // When there are remaining A/AAAA transactions, ArkWebFailedTransaction
  // should return true and call RecordFailedTransactionInfo +
  // OnTransactionsFinished.
  EXPECT_TRUE(dns_task->ArkWebFailedTransaction(
      net_error, std::make_optional<DnsQueryType>(DnsQueryType::AAAA)));
}

TEST_F(ArkWebHostResolverDnsTest, ArkWebFailedTransaction_SavedResults) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);
  int net_error = 0;
  SetTaskSavedResults(true);
  AddTransactionInfo(
      DnsQueryType::A,
      HostResolverDnsTask::TransactionErrorBehavior::kFatalOrEmpty);
  testing::internal::CaptureStderr();
  EXPECT_NO_FATAL_FAILURE(dns_task->ArkWebFailedTransaction(
      net_error, std::make_optional<DnsQueryType>(DnsQueryType::AAAA)));
  std::string log_output = testing::internal::GetCapturedStderr();
  EXPECT_NE(log_output.find("The completed transaction [2] is failed"),
            std::string::npos);
}

TEST_F(ArkWebHostResolverDnsTest, RecordFailedTransactionInfo) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);
  // no process
  EXPECT_NO_FATAL_FAILURE(
      dns_task->RecordFailedTransactionInfo(0, 0, DnsQueryType::AAAA));
}

TEST_F(ArkWebHostResolverDnsTest, AnyAOrAAAATransactionRemain) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);
  EXPECT_FALSE(dns_task->AnyAOrAAAATransactionRemain());
  AddTransactionInfo(
      DnsQueryType::A,
      HostResolverDnsTask::TransactionErrorBehavior::kFatalOrEmpty);
  AddTransactionInfo(
      DnsQueryType::AAAA,
      HostResolverDnsTask::TransactionErrorBehavior::kFatalOrEmpty);
  AddTransactionInfo(
      DnsQueryType::HTTPS,
      HostResolverDnsTask::TransactionErrorBehavior::kFatalOrEmpty);
  EXPECT_TRUE(dns_task->AnyAOrAAAATransactionRemain());
}

TEST_F(ArkWebHostResolverDnsTest, SetNotNeedMoreAttemptIPQueryType_Reurn) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);
  // no process
  SetTaskSniffIpResult(false);
  EXPECT_NO_FATAL_FAILURE(
      dns_task->SetNotNeedMoreAttemptIPQueryType(DnsQueryType::AAAA));
  SetTaskSniffIpResult(true);
  EXPECT_NO_FATAL_FAILURE(
      dns_task->SetNotNeedMoreAttemptIPQueryType(DnsQueryType::HTTPS));
}

TEST_F(ArkWebHostResolverDnsTest, SetNotNeedMoreAttemptIPQueryType_WithAAAA) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);
  SetTaskSniffIpResult(true);
  AddTransactionInfo(
      DnsQueryType::A,
      HostResolverDnsTask::TransactionErrorBehavior::kFatalOrEmpty);
  AddTransactionInfo(
      DnsQueryType::AAAA,
      HostResolverDnsTask::TransactionErrorBehavior::kFatalOrEmpty);
  AddTransactionInfo(
      DnsQueryType::HTTPS,
      HostResolverDnsTask::TransactionErrorBehavior::kFatalOrEmpty);
  EXPECT_NO_FATAL_FAILURE(
      dns_task->SetNotNeedMoreAttemptIPQueryType(DnsQueryType::AAAA));
  CheckTransactionInProcess();
}

TEST_F(ArkWebHostResolverDnsTest, SetNotNeedMoreAttemptIPQueryType_WithA) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);
  SetTaskSniffIpResult(true);
  AddTransactionInfo(
      DnsQueryType::A,
      HostResolverDnsTask::TransactionErrorBehavior::kFatalOrEmpty);
  AddTransactionInfo(
      DnsQueryType::AAAA,
      HostResolverDnsTask::TransactionErrorBehavior::kFatalOrEmpty);
  AddTransactionInfo(
      DnsQueryType::HTTPS,
      HostResolverDnsTask::TransactionErrorBehavior::kFatalOrEmpty);
  EXPECT_NO_FATAL_FAILURE(
      dns_task->SetNotNeedMoreAttemptIPQueryType(DnsQueryType::A));
  CheckTransactionInProcess();

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableNwebExHttpDnsFallback);
  EXPECT_NO_FATAL_FAILURE(
      dns_task->ArkWebSetNotNeedQueryType(OK, DnsQueryType::AAAA));
  CheckTransactionInProcess();
}

TEST_F(ArkWebHostResolverDnsTest, AnyAOrAAAATransactionRemain_Needed) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);
  // No transactions in progress or needed initially.
  EXPECT_FALSE(dns_task->AnyAOrAAAATransactionRemain());

  // Add A/AAAA to transactions_needed_ (not in_progress).
  AddNeededTransactionInfo(
      DnsQueryType::A,
      HostResolverDnsTask::TransactionErrorBehavior::kFatalOrEmpty);
  EXPECT_TRUE(dns_task->AnyAOrAAAATransactionRemain());

  // Add HTTPS only to needed - should still be true due to A.
  AddNeededTransactionInfo(
      DnsQueryType::HTTPS,
      HostResolverDnsTask::TransactionErrorBehavior::kFatalOrEmpty);
  EXPECT_TRUE(dns_task->AnyAOrAAAATransactionRemain());
}

TEST_F(ArkWebHostResolverDnsTest, ArkWebFailedTransaction_NoRemainNoSaved) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);
  // No A/AAAA transactions remain, and no saved results.
  // Should return false because the condition
  // (AnyAOrAAAATransactionRemain() || saved_results_.empty()) is true but
  // only for the empty case. However, with empty saved_results_ and
  // no remaining transactions, the function enters the branch that calls
  // OnTransactionsFinished, so it returns true.
  // Actually, re-reading the code: if AnyAOrAAAATransactionRemain() is false
  // and saved_results_.empty() is false, it returns false.
  // Let's add a non-A/AAAA transaction to test the false return path.
  AddTransactionInfo(
      DnsQueryType::HTTPS,
      HostResolverDnsTask::TransactionErrorBehavior::kFatalOrEmpty);
  // No A/AAAA transactions remain in progress or needed.
  // With address type (A) as failed_transaction_type, but no A/AAAA
  // transactions remain and saved_results_ is empty:
  // The condition is: (AnyAOrAAAATransactionRemain() || saved_results_.empty())
  // Both: AnyAOrAAAATransactionRemain() = false, saved_results_.empty() = true
  // So the OR is true, and it enters the OnTransactionsFinished branch.
  // To get a false return, we need !AnyAOrAAAATransactionRemain() and
  // !saved_results_.empty(). That's hard to set up without more infrastructure.
  // For now test that with HTTPS transactions only, AAAA failure returns true.
  EXPECT_TRUE(dns_task->ArkWebFailedTransaction(
      ERR_DNS_SERVER_FAILED,
      std::make_optional<DnsQueryType>(DnsQueryType::AAAA)));
}

TEST_F(ArkWebHostResolverDnsTest, ArkWebFailedTransaction_TypeA) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);
  int net_error = ERR_DNS_SERVER_FAILED;
  AddTransactionInfo(
      DnsQueryType::AAAA,
      HostResolverDnsTask::TransactionErrorBehavior::kFatalOrEmpty);
  AddTransactionInfo(
      DnsQueryType::HTTPS,
      HostResolverDnsTask::TransactionErrorBehavior::kFatalOrEmpty);

  // When failed_transaction_type is A (an address type), and there are
  // remaining AAAA transactions, it should return true.
  EXPECT_TRUE(dns_task->ArkWebFailedTransaction(
      net_error, std::make_optional<DnsQueryType>(DnsQueryType::A)));
}

TEST_F(ArkWebHostResolverDnsTest,
       MaybeModifyInsecureDnsTaskResolveResults_SecureFallback) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);
  // Set secure_fallback_ to true so the method returns early.
  dns_task->hostResolverDnsTask->secure_fallback_ = true;
  ArkWebHostResolverDnsTaskExt::Results results;
  std::vector<IPEndPoint> truncation_results;
  EXPECT_NO_FATAL_FAILURE(dns_task->MaybeModifyInsecureDnsTaskResolveResults(
      "localhost", false, results, truncation_results));
}

TEST_F(ArkWebHostResolverDnsTest,
       MaybeModifyInsecureDnsTaskResolveResults_EmptyEndpoints) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);

  // Create a task that is not secure_fallback so the method proceeds.
  // The existing task was created with secure_fallback=false.
  ArkWebHostResolverDnsTaskExt::Results results;
  // Add a non-data result (no IP endpoints).
  auto error_result = std::make_unique<HostResolverInternalErrorResult>(
      "localhost", DnsQueryType::A, std::nullopt, std::nullopt,
      HostResolverInternalResult::Source::kDns, ERR_DNS_SERVER_FAILED);
  results.insert(std::move(error_result));

  std::vector<IPEndPoint> truncation_results;
  EXPECT_NO_FATAL_FAILURE(dns_task->MaybeModifyInsecureDnsTaskResolveResults(
      "localhost", false, results, truncation_results));
  // Results should be unchanged since no data endpoints.
  EXPECT_EQ(results.size(), 1u);
}

TEST_F(ArkWebHostResolverDnsTest,
       MaybeModifyInsecureDnsTaskResolveResults_WithDataEndpoints) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);

  // Create results with IP endpoints.
  ArkWebHostResolverDnsTaskExt::Results results;
  auto data_result = std::make_unique<HostResolverInternalDataResult>(
      std::string("localhost"), DnsQueryType::A, std::nullopt, base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::vector<IPEndPoint>{
          IPEndPoint(IPAddress(1, 2, 3, 4), 80),
          IPEndPoint(IPAddress(5, 6, 7, 8), 80),
      },
      std::vector<std::string>(), std::vector<HostPortPair>());
  results.insert(std::move(data_result));

  // Without setting up suspect IPs, MaybeNeedToProcessAddressList
  // should return false, and results should be unchanged.
  size_t initial_size = results.size();
  std::vector<IPEndPoint> truncation_results;
  EXPECT_NO_FATAL_FAILURE(dns_task->MaybeModifyInsecureDnsTaskResolveResults(
      "unknown.host", false, results, truncation_results));
  EXPECT_EQ(results.size(), initial_size);
}

TEST_F(ArkWebHostResolverDnsTest,
       MaybeModifyInsecureDnsTaskResolveResults_SuspectIPRemoveSpecific) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);

  // Set up suspect IP list and source host list.
  StoreSuspectIPListAndSourceHostList({"1.2.3.4"}, {"localhost"});

  // Create results with IP endpoints where 1.2.3.4 is a suspect IP.
  ArkWebHostResolverDnsTaskExt::Results results;
  auto data_result = std::make_unique<HostResolverInternalDataResult>(
      std::string("localhost"), DnsQueryType::A, std::nullopt, base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::vector<IPEndPoint>{
          IPEndPoint(IPAddress(1, 2, 3, 4), 80),  // suspect
          IPEndPoint(IPAddress(5, 6, 7, 8), 80),  // safe
      },
      std::vector<std::string>(), std::vector<HostPortPair>());
  results.insert(std::move(data_result));

  std::vector<IPEndPoint> truncation_results;
  EXPECT_NO_FATAL_FAILURE(dns_task->MaybeModifyInsecureDnsTaskResolveResults(
      "localhost", true, results, truncation_results));

  // After modification, the suspect IP should be removed.
  for (const auto& result : results) {
    if (result->type() == HostResolverInternalResult::Type::kData) {
      const auto& endpoints = result->AsData().endpoints();
      EXPECT_EQ(endpoints.size(), 1u);
      if (!endpoints.empty()) {
        EXPECT_EQ(endpoints[0].address(), IPAddress(5, 6, 7, 8));
      }
    }
  }

  // Clean up global state.
  StoreSuspectIPListAndSourceHostList({}, {});
}

TEST_F(ArkWebHostResolverDnsTest,
       MaybeModifyInsecureDnsTaskResolveResults_SuspectIPAllRemoved) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);

  // Set up suspect IP list where all IPs are suspect.
  StoreSuspectIPListAndSourceHostList({"1.2.3.4", "5.6.7.8"}, {"localhost"});

  // Create results where all IPs are suspect.
  ArkWebHostResolverDnsTaskExt::Results results;
  auto data_result = std::make_unique<HostResolverInternalDataResult>(
      std::string("localhost"), DnsQueryType::A, std::nullopt, base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::vector<IPEndPoint>{
          IPEndPoint(IPAddress(1, 2, 3, 4), 80),
          IPEndPoint(IPAddress(5, 6, 7, 8), 80),
      },
      std::vector<std::string>(), std::vector<HostPortPair>());
  results.insert(std::move(data_result));

  // With secure_dns_fallback_available=true, need_to_modify_result should be
  // true, and all endpoints should be cleared.
  std::vector<IPEndPoint> truncation_results;
  EXPECT_NO_FATAL_FAILURE(dns_task->MaybeModifyInsecureDnsTaskResolveResults(
      "localhost", true, results, truncation_results));

  for (const auto& result : results) {
    if (result->type() == HostResolverInternalResult::Type::kData) {
      EXPECT_TRUE(result->AsData().endpoints().empty());
    }
  }

  // Clean up global state.
  StoreSuspectIPListAndSourceHostList({}, {});
}

TEST_F(ArkWebHostResolverDnsTest,
       MaybeModifyInsecureDnsTaskResolveResults_SuspectIPAllRemovedNoFallback) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);

  // Set up suspect IP list where all IPs are suspect.
  StoreSuspectIPListAndSourceHostList({"1.2.3.4", "5.6.7.8"}, {"localhost"});

  // Create results where all IPs are suspect.
  ArkWebHostResolverDnsTaskExt::Results results;
  auto data_result = std::make_unique<HostResolverInternalDataResult>(
      std::string("localhost"), DnsQueryType::A, std::nullopt, base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::vector<IPEndPoint>{
          IPEndPoint(IPAddress(1, 2, 3, 4), 80),
          IPEndPoint(IPAddress(5, 6, 7, 8), 80),
      },
      std::vector<std::string>(), std::vector<HostPortPair>());
  results.insert(std::move(data_result));

  // With secure_dns_fallback_available=false, all IPs removed but no
  // fallback available, so the original addresses should be kept.
  std::vector<IPEndPoint> truncation_results;
  EXPECT_NO_FATAL_FAILURE(dns_task->MaybeModifyInsecureDnsTaskResolveResults(
      "localhost", false, results, truncation_results));

  // When all IPs are suspect and no fallback, the original addresses
  // should be restored (out_addresses = legacy_addresses).
  for (const auto& result : results) {
    if (result->type() == HostResolverInternalResult::Type::kData) {
      const auto& endpoints = result->AsData().endpoints();
      EXPECT_EQ(endpoints.size(), 2u);
    }
  }

  // Clean up global state.
  StoreSuspectIPListAndSourceHostList({}, {});
}

TEST_F(ArkWebHostResolverDnsTest,
       ArkWebFailedTransaction_NoRemainWithSavedResults_ReturnsFalse) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);

  // Add a non-A/AAAA transaction in progress (so AnyAOrAAAATransactionRemain
  // returns false).
  AddTransactionInfo(
      DnsQueryType::HTTPS,
      HostResolverDnsTask::TransactionErrorBehavior::kFatalOrEmpty);

  // Populate saved_results_ with a non-empty result so that
  // saved_results_.empty() returns false.
  auto data_result = std::make_unique<HostResolverInternalDataResult>(
      std::string("localhost"), DnsQueryType::A, std::nullopt, base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::vector<IPEndPoint>{IPEndPoint(IPAddress(1, 2, 3, 4), 80)},
      std::vector<std::string>(), std::vector<HostPortPair>());
  dns_task->hostResolverDnsTask->saved_results_.insert(std::move(data_result));

  // With no A/AAAA remaining and non-empty saved results, the outer OR
  // condition is false, so the function should return false.
  EXPECT_FALSE(dns_task->ArkWebFailedTransaction(
      ERR_DNS_SERVER_FAILED,
      std::make_optional<DnsQueryType>(DnsQueryType::AAAA)));
}

TEST_F(ArkWebHostResolverDnsTest,
       MaybeModifyInsecureDnsTaskResolveResults_MixedDataAndErrorResults) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);

  // Set up suspect IP list with 1.2.3.4 as suspect for localhost.
  StoreSuspectIPListAndSourceHostList({"1.2.3.4"}, {"localhost"});

  ArkWebHostResolverDnsTaskExt::Results results;

  // Add an error result (non-data).
  auto error_result = std::make_unique<HostResolverInternalErrorResult>(
      "localhost", DnsQueryType::A, std::nullopt, std::nullopt,
      HostResolverInternalResult::Source::kDns, ERR_DNS_SERVER_FAILED);
  results.insert(std::move(error_result));

  // Add a data result with a suspect IP and a safe IP.
  auto data_result = std::make_unique<HostResolverInternalDataResult>(
      std::string("localhost"), DnsQueryType::A, std::nullopt, base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::vector<IPEndPoint>{
          IPEndPoint(IPAddress(1, 2, 3, 4), 80),  // suspect
          IPEndPoint(IPAddress(5, 6, 7, 8), 80),  // safe
      },
      std::vector<std::string>(), std::vector<HostPortPair>());
  results.insert(std::move(data_result));

  std::vector<IPEndPoint> truncation_results;
  EXPECT_NO_FATAL_FAILURE(dns_task->MaybeModifyInsecureDnsTaskResolveResults(
      "localhost", true, results, truncation_results));

  // The suspect IP 1.2.3.4 should be removed, leaving only 5.6.7.8 in the
  // data result. The error result should be untouched.
  bool found_data = false;
  bool found_error = false;
  for (const auto& result : results) {
    if (result->type() == HostResolverInternalResult::Type::kData) {
      found_data = true;
      const auto& endpoints = result->AsData().endpoints();
      EXPECT_EQ(endpoints.size(), 1u);
      if (!endpoints.empty()) {
        EXPECT_EQ(endpoints[0].address(), IPAddress(5, 6, 7, 8));
      }
    } else if (result->type() == HostResolverInternalResult::Type::kError) {
      found_error = true;
    }
  }
  EXPECT_TRUE(found_data);
  EXPECT_TRUE(found_error);

  // Clean up global state.
  StoreSuspectIPListAndSourceHostList({}, {});
}

TEST_F(ArkWebHostResolverDnsTest,
       MaybeModifyInsecureDnsTaskResolveResults_MultipleDataResults) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);

  // Set up suspect IP where all IPs are suspect for localhost.
  StoreSuspectIPListAndSourceHostList({"1.2.3.4", "5.6.7.8"}, {"localhost"});

  ArkWebHostResolverDnsTaskExt::Results results;

  // Add two data results, each with endpoints.
  auto data_result1 = std::make_unique<HostResolverInternalDataResult>(
      std::string("localhost"), DnsQueryType::A, std::nullopt, base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::vector<IPEndPoint>{
          IPEndPoint(IPAddress(1, 2, 3, 4), 80),
      },
      std::vector<std::string>(), std::vector<HostPortPair>());
  results.insert(std::move(data_result1));

  auto data_result2 = std::make_unique<HostResolverInternalDataResult>(
      std::string("localhost"), DnsQueryType::AAAA, std::nullopt, base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::vector<IPEndPoint>{
          IPEndPoint(IPAddress(5, 6, 7, 8), 80),
      },
      std::vector<std::string>(), std::vector<HostPortPair>());
  results.insert(std::move(data_result2));

  // With secure_dns_fallback_available=true, all suspect IPs should cause
  // endpoints to be cleared from both data results.
  std::vector<IPEndPoint> truncation_results;
  EXPECT_NO_FATAL_FAILURE(dns_task->MaybeModifyInsecureDnsTaskResolveResults(
      "localhost", true, results, truncation_results));

  for (const auto& result : results) {
    if (result->type() == HostResolverInternalResult::Type::kData) {
      EXPECT_TRUE(result->AsData().endpoints().empty());
    }
  }

  // Clean up global state.
  StoreSuspectIPListAndSourceHostList({}, {});
}

TEST_F(ArkWebHostResolverDnsTest,
       MaybeModifyInsecureDnsTaskResolveResults_SuspectIPInSecondDataResult) {
  CreateTaskWithoutRequest(HostPortPair("localhost", 80));
  auto dns_task = GetDnsTaskExt();
  ASSERT_TRUE(dns_task);

  // Set up suspect IP list.
  StoreSuspectIPListAndSourceHostList({"5.6.7.8"}, {"localhost"});

  ArkWebHostResolverDnsTaskExt::Results results;

  // First data result: only safe IPs.
  auto data_result1 = std::make_unique<HostResolverInternalDataResult>(
      std::string("localhost"), DnsQueryType::A, std::nullopt, base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::vector<IPEndPoint>{
          IPEndPoint(IPAddress(1, 2, 3, 4), 80),
      },
      std::vector<std::string>(), std::vector<HostPortPair>());
  results.insert(std::move(data_result1));

  // Second data result: has a suspect IP mixed with safe IP.
  auto data_result2 = std::make_unique<HostResolverInternalDataResult>(
      std::string("localhost"), DnsQueryType::AAAA, std::nullopt, base::Time(),
      HostResolverInternalResult::Source::kDns,
      std::vector<IPEndPoint>{
          IPEndPoint(IPAddress(5, 6, 7, 8), 80),     // suspect
          IPEndPoint(IPAddress(9, 10, 11, 12), 80),  // safe
      },
      std::vector<std::string>(), std::vector<HostPortPair>());
  results.insert(std::move(data_result2));

  std::vector<IPEndPoint> truncation_results;
  EXPECT_NO_FATAL_FAILURE(dns_task->MaybeModifyInsecureDnsTaskResolveResults(
      "localhost", true, results, truncation_results));

  // First result should keep its safe IP.
  // Second result should have suspect IP removed, keeping only the safe one.
  int data_count = 0;
  for (const auto& result : results) {
    if (result->type() == HostResolverInternalResult::Type::kData) {
      data_count++;
      const auto& endpoints = result->AsData().endpoints();
      EXPECT_EQ(endpoints.size(), 1u);
    }
  }
  EXPECT_EQ(data_count, 2);

  // Clean up global state.
  StoreSuspectIPListAndSourceHostList({}, {});
}
#endif
}  // namespace net
