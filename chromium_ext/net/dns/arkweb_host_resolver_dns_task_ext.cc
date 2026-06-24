// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "net/dns/host_resolver_dns_task.h"
#include "arkweb/chromium_ext/net/dns/arkweb_host_resolver_dns_task_ext.h"

#include <string_view>

#include "arkweb/chromium_ext/url/ohos/log_utils.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/time/tick_clock.h"
#include "net/base/features.h"
#include "net/dns/address_sorter.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_cache.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/public/util.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
#include "arkweb/chromium_ext/content/public/common/content_switches_ext.h"
#include "arkweb/chromium_ext/net/dns/secure_dns_fallback_utils.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/overrides/net/dns/secure_dns_fallback_utils.h"
#endif  // BUILDFLAG(IS_ARKWEB_EXT)
#endif

namespace net {

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
void ArkWebHostResolverDnsTaskExt::ArkWebSetNotNeedQueryType(int legacy_results_error, DnsQueryType dns_query_type) {
  if (legacy_results_error == OK && AnyAOrAAAATransactionRemain()) {
    SetNotNeedMoreAttemptIPQueryType(dns_query_type);
  }
}

bool ArkWebHostResolverDnsTaskExt::ArkWebFailedTransaction(
    int net_error, std::optional<DnsQueryType> failed_transaction_type) {
  if (failed_transaction_type.has_value() &&
      IsAddressType(failed_transaction_type.value())) {
    DnsQueryType dns_query_type = failed_transaction_type.value();
    hostResolverDnsTask->delegate_->AddTransactionResultForReport(dns_query_type, net_error);
    if (AnyAOrAAAATransactionRemain() || hostResolverDnsTask->saved_results_.empty()) {
      int completed_transaction_index = 1;
      if (hostResolverDnsTask->saved_results_.empty()) {
        completed_transaction_index = 2;
      }
      RecordFailedTransactionInfo(completed_transaction_index, net_error,
                                  dns_query_type);
      hostResolverDnsTask->OnTransactionsFinished(/*single_transaction_results=*/std::nullopt);
      return true;
    }
  }
  return false;
}

bool ArkWebHostResolverDnsTaskExt::AnyAOrAAAATransactionRemain() {
  auto is_specified_dns_query_type = [](DnsQueryType type) {
    return type == DnsQueryType::A || type == DnsQueryType::AAAA;
  };

  return std::ranges::any_of(hostResolverDnsTask->transactions_needed_, is_specified_dns_query_type,
                              &HostResolverDnsTask::TransactionInfo::type) ||
         std::ranges::any_of(hostResolverDnsTask->transactions_in_progress_,
                              is_specified_dns_query_type,
                              &HostResolverDnsTask::TransactionInfo::type);
}

void ArkWebHostResolverDnsTaskExt::RecordFailedTransactionInfo(
    int index,
    int net_error,
    DnsQueryType dns_query_type) {
#if BUILDFLAG(ARKWEB_LOGGER_REPORT)
  LOG_FEEDBACK(INFO)
      << "The completed transaction [" << index << "] is failed " << net_error
      << ", failedQueryType " << static_cast<int>(dns_query_type) << ", host "
      << std::string(hostResolverDnsTask->host_.GetHostnameWithoutBrackets())
      << ", and needed tranactions num is 2";
#endif
}

void ArkWebHostResolverDnsTaskExt::SetNotNeedMoreAttemptIPQueryType(
    DnsQueryType dns_query_type) {
  if (!hostResolverDnsTask->need_to_sniff_ip_result_ || (dns_query_type != DnsQueryType::A &&
                                    dns_query_type != DnsQueryType::AAAA)) {
    return;
  }

  for (auto& transaction_info : hostResolverDnsTask->transactions_in_progress_) {
    if (transaction_info.type == DnsQueryType::A ||
        transaction_info.type == DnsQueryType::AAAA) {
      transaction_info.transaction->SetNotNeedMoreAttemptIPQueryType(
          transaction_info.transaction->GetType());
    }
  }
}

void ArkWebHostResolverDnsTaskExt::MaybeModifyInsecureDnsTaskResolveResults(
    const std::string& host,
    bool secure_dns_fallback_available,
    Results& out_results) {
  if (hostResolverDnsTask->secure_fallback()) {
    return;
  }

  // Obtain all original IP addresses.
  std::vector<IPEndPoint> all_ip_endpoints;
  for (const auto& result : out_results) {
    if (result->type() == HostResolverInternalResult::Type::kData) {
      const auto& data_result = result->AsData();
      for (const auto& endpoint : data_result.endpoints()) {
        all_ip_endpoints.push_back(endpoint);
      }
    }
  }

  if (all_ip_endpoints.empty()) {
    return;
  }

  bool need_to_modify_resolve_result = false;
  std::vector<IPEndPoint> ip_endpoints_modified;
  bool need_to_replace_address = MaybeNeedToProcessAddressList(
      host, all_ip_endpoints, secure_dns_fallback_available,
      ip_endpoints_modified, need_to_modify_resolve_result);

  if (!need_to_replace_address) {
    return;
  }

#if BUILDFLAG(IS_ARKWEB_EXT)
  ReportDnsHijackHitInfo(host, RecordQueryType::UDP, all_ip_endpoints);
#endif

  // need to flush all IP addresses.
  if (need_to_modify_resolve_result) {
    for (auto& result : out_results) {
      if (result->type() == HostResolverInternalResult::Type::kData) {
        result->AsData().ClearEndpoints();
      }
    }
    return;
  }

  std::vector<IPEndPoint> ips_to_remove;
  std::set<IPEndPoint> keep_set(ip_endpoints_modified.begin(),
                                ip_endpoints_modified.end());
  for (const auto& ip : all_ip_endpoints) {
    if (keep_set.find(ip) == keep_set.end()) {
      ips_to_remove.push_back(ip);
    }
  }

  // Remove the IP to be deleted.
  for (auto& result : out_results) {
    if (result->type() == HostResolverInternalResult::Type::kData) {
      auto& data_result = result->AsData();
      for (const auto& ip_to_remove : ips_to_remove) {
        data_result.RemoveEndpoint(ip_to_remove);
      }
    }
  }
}

#endif  // BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)

}  // namespace net
