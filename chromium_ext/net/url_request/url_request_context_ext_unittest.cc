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

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "arkweb/build/features/features.h"
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "net/base/network_handle.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "url/gurl.h"

#if BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION) ||                 \
    BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK) ||                  \
    BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK_ON_DNS_HIJACKING) || \
    BUILDFLAG(ARKWEB_NETWORK_SERVICE) || BUILDFLAG(ARKWEB_EXT_NAVIGATION)

// clang-format off
#define private public
#include "net/url_request/url_request_context_ext.h"
#undef private
// clang-format on

namespace net {

// ---------------------------------------------------------------------------
// Unit tests for url_request_context_ext.cc - the ArkWeb URLRequestContext
// extension. ARKWEB_TEST keeps the base URLRequestContext accessors and
// setters public in test builds, so a context whose members have been reset
// with set_host_resolver(nullptr) etc. exercises every null-member
// early-return branch, while a fully-built URLRequestContextBuilder context
// exercises the happy paths.
//
// Branch coverage:
//   SetConnectTimeout:                  [A] null transaction factory
//                                       [B] null network session
//                                       [C] happy path
//   BindDnsToNetwork:                   [D] null transaction factory
//                                       [E] null network session
//                                       [F] happy path
//   SetConnectJobWithSecureDnsOnlyTimeout: [G] null transaction factory
//                                       [H] null network session
//                                       [I] happy path
//   CanUseSecureDnsFallback:            [J] null host resolver -> false
//                                       [K] happy path
//   IsDomainInWhitelist:                [L] null host resolver -> false
//                                       [M] happy path
//   IsIframeFallbackEnabled:            [N] null host resolver -> false
//                                       [O] happy path
//   NeedRetryDnsOnDnsHijack:            [P] null host resolver -> false
//                                       [Q] happy path
//   SetSocketIdleTimeout:               [R] null network session
//   SetShouldReportNewNavigationInfo:   [S] same value -> early return
//                                       [T] new value stored
//   ShouldReportNewNavigationInfo:      [U] returns stored value
// ---------------------------------------------------------------------------

// A minimal HttpTransactionFactory whose GetSession() returns nullptr, used to
// drive the "factory present but no network session" branch.
class NullSessionHttpTransactionFactory : public HttpTransactionFactory {
 public:
  std::unique_ptr<HttpTransaction> CreateTransaction(
      RequestPriority priority) override {
    return nullptr;
  }

  HttpCache* GetCache() override { return nullptr; }

  HttpNetworkSession* GetSession() override { return nullptr; }
};

// Restores a context's host resolver before it is destroyed. Used to drive the
// null-resolver early-return branches while keeping destruction safe: the
// resolver is moved out, then re-attached by this deleter right before the
// context is deleted.
struct RestoreHostResolverDeleter {
  void operator()(URLRequestContextExt* context) {
    if (context && saved_resolver) {
      context->set_host_resolver(
          std::unique_ptr<HostResolver>(saved_resolver.release()));
    }
    delete context;
  }

  std::unique_ptr<HostResolver> saved_resolver;
};

class URLRequestContextExtTest : public ::testing::Test {
 protected:
  // Builds a fully-populated URLRequestContextExt via the builder.
  std::unique_ptr<URLRequestContextExt> BuildFullContext() {
    URLRequestContextBuilder builder;
    std::unique_ptr<URLRequestContext> context = builder.Build();
    URLRequestContextExt* ext = context->AsURLRequestContextExt();
    // |context| is actually a URLRequestContextExt in this build; release it
    // and re-wrap so ownership is not double-managed.
    context.release();
    return base::WrapUnique(ext);
  }

  // Builds a context with no host resolver (drives the host_resolver null
  // early-return branches). URLRequestContext::~URLRequestContext() calls
  // host_resolver()->OnShutdown() unconditionally, so the resolver is moved
  // out (via release, which transfers ownership without freeing) and
  // re-attached by the deleter before the context is destroyed.
  std::unique_ptr<URLRequestContextExt, RestoreHostResolverDeleter>
  BuildNoHostResolverContext() {
    std::unique_ptr<URLRequestContextExt, RestoreHostResolverDeleter> context(
        BuildFullContext().release(), RestoreHostResolverDeleter{});
    // Release ownership out of the context (private access via #define private
    // public) and hand it to the deleter; the context now runs with a null
    // resolver and the deleter re-attaches it right before deletion.
    context.get_deleter().saved_resolver =
        std::move(context->host_resolver_);
    return context;
  }

  // Builds a context with no transaction factory (drives the transaction
  // factory null early-return branches).
  std::unique_ptr<URLRequestContextExt> BuildNoTransactionFactoryContext() {
    auto context = BuildFullContext();
    context->set_http_transaction_factory(nullptr);
    return context;
  }

  // Builds a context whose transaction factory's GetSession() returns nullptr
  // (drives the network session null early-return branches while keeping the
  // transaction factory non-null).
  std::unique_ptr<URLRequestContextExt> BuildNullSessionContext() {
    auto context = BuildFullContext();
    context->set_http_transaction_factory(
        std::make_unique<NullSessionHttpTransactionFactory>());
    return context;
  }

  base::test::TaskEnvironment task_environment_;
};

#if BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION)
// [A] SetConnectTimeout with no transaction factory returns early.
TEST_F(URLRequestContextExtTest, SetConnectTimeout_NullTransactionFactory) {
  auto context = BuildNoTransactionFactoryContext();
  EXPECT_NO_FATAL_FAILURE(context->SetConnectTimeout(30));
}

// [B] SetConnectTimeout with a session-less transaction factory returns early.
TEST_F(URLRequestContextExtTest, SetConnectTimeout_NullNetworkSession) {
  auto context = BuildNullSessionContext();
  EXPECT_NO_FATAL_FAILURE(context->SetConnectTimeout(30));
}

// [C] SetConnectTimeout happy path on a fully-built context.
TEST_F(URLRequestContextExtTest, SetConnectTimeout_HappyPath) {
  auto context = BuildFullContext();
  EXPECT_NO_FATAL_FAILURE(context->SetConnectTimeout(30));
}

// [D] BindDnsToNetwork with no transaction factory returns early.
TEST_F(URLRequestContextExtTest, BindDnsToNetwork_NullTransactionFactory) {
  auto context = BuildNoTransactionFactoryContext();
  EXPECT_NO_FATAL_FAILURE(
      context->BindDnsToNetwork(handles::kInvalidNetworkHandle));
}

// [E] BindDnsToNetwork with a session-less factory returns early.
TEST_F(URLRequestContextExtTest, BindDnsToNetwork_NullNetworkSession) {
  auto context = BuildNullSessionContext();
  EXPECT_NO_FATAL_FAILURE(
      context->BindDnsToNetwork(handles::kInvalidNetworkHandle));
}

// [F] BindDnsToNetwork happy path on a fully-built context.
TEST_F(URLRequestContextExtTest, BindDnsToNetwork_HappyPath) {
  auto context = BuildFullContext();
  EXPECT_NO_FATAL_FAILURE(
      context->BindDnsToNetwork(handles::kInvalidNetworkHandle));
}
#endif

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
// [G] SetConnectJobWithSecureDnsOnlyTimeout null transaction factory.
TEST_F(URLRequestContextExtTest,
       SetConnectJobWithSecureDnsOnlyTimeout_NullTransactionFactory) {
  auto context = BuildNoTransactionFactoryContext();
  EXPECT_NO_FATAL_FAILURE(context->SetConnectJobWithSecureDnsOnlyTimeout(5));
}

// [H] SetConnectJobWithSecureDnsOnlyTimeout null network session.
TEST_F(URLRequestContextExtTest,
       SetConnectJobWithSecureDnsOnlyTimeout_NullNetworkSession) {
  auto context = BuildNullSessionContext();
  EXPECT_NO_FATAL_FAILURE(context->SetConnectJobWithSecureDnsOnlyTimeout(5));
}

// [I] SetConnectJobWithSecureDnsOnlyTimeout happy path.
TEST_F(URLRequestContextExtTest,
       SetConnectJobWithSecureDnsOnlyTimeout_HappyPath) {
  auto context = BuildFullContext();
  EXPECT_NO_FATAL_FAILURE(context->SetConnectJobWithSecureDnsOnlyTimeout(5));
}

// [J] CanUseSecureDnsFallback with no host resolver returns false.
TEST_F(URLRequestContextExtTest, CanUseSecureDnsFallback_NullHostResolver) {
  auto context = BuildNoHostResolverContext();
  EXPECT_FALSE(context->CanUseSecureDnsFallback());
}

// [K] CanUseSecureDnsFallback happy path on a fully-built context.
TEST_F(URLRequestContextExtTest, CanUseSecureDnsFallback_HappyPath) {
  auto context = BuildFullContext();
  EXPECT_NO_FATAL_FAILURE(context->CanUseSecureDnsFallback());
}

// [L] IsDomainInWhitelist with no host resolver returns false.
TEST_F(URLRequestContextExtTest, IsDomainInWhitelist_NullHostResolver) {
  auto context = BuildNoHostResolverContext();
  EXPECT_FALSE(context->IsDomainInWhitelist("example.com"));
}

// [M] IsDomainInWhitelist happy path on a fully-built context.
TEST_F(URLRequestContextExtTest, IsDomainInWhitelist_HappyPath) {
  auto context = BuildFullContext();
  EXPECT_NO_FATAL_FAILURE(context->IsDomainInWhitelist("example.com"));
}

// [N] IsIframeFallbackEnabled with no host resolver returns false.
TEST_F(URLRequestContextExtTest, IsIframeFallbackEnabled_NullHostResolver) {
  auto context = BuildNoHostResolverContext();
  EXPECT_FALSE(context->IsIframeFallbackEnabled());
}

// [O] IsIframeFallbackEnabled happy path on a fully-built context.
TEST_F(URLRequestContextExtTest, IsIframeFallbackEnabled_HappyPath) {
  auto context = BuildFullContext();
  EXPECT_NO_FATAL_FAILURE(context->IsIframeFallbackEnabled());
}
#endif

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK_ON_DNS_HIJACKING)
// [P] NeedRetryDnsOnDnsHijack with no host resolver returns false.
TEST_F(URLRequestContextExtTest, NeedRetryDnsOnDnsHijack_NullHostResolver) {
  auto context = BuildNoHostResolverContext();
  EXPECT_FALSE(
      context->NeedRetryDnsOnDnsHijack(GURL("https://example.com"), "-3"));
}

// [Q] NeedRetryDnsOnDnsHijack happy path on a fully-built context.
TEST_F(URLRequestContextExtTest, NeedRetryDnsOnDnsHijack_HappyPath) {
  auto context = BuildFullContext();
  EXPECT_NO_FATAL_FAILURE(
      context->NeedRetryDnsOnDnsHijack(GURL("https://example.com"), "-3"));
}
#endif

#if BUILDFLAG(ARKWEB_NETWORK_SERVICE)
// [R] SetSocketIdleTimeout with no network session returns early.
TEST_F(URLRequestContextExtTest, SetSocketIdleTimeout_NullNetworkSession) {
  auto context = BuildFullContext();
  context->set_http_network_session(nullptr);
  EXPECT_NO_FATAL_FAILURE(context->SetSocketIdleTimeout(10));
}
#endif

#if BUILDFLAG(ARKWEB_EXT_NAVIGATION)
// [S] Setting the same value again is a no-op (early return).
TEST_F(URLRequestContextExtTest, SetShouldReportNewNavigationInfo_SameValue) {
  auto context = BuildFullContext();
  context->SetShouldReportNewNavigationInfo(true);
  // Second call with the same value hits the early-return branch.
  EXPECT_NO_FATAL_FAILURE(context->SetShouldReportNewNavigationInfo(true));
  EXPECT_TRUE(context->ShouldReportNewNavigationInfo());
}

// [T] Setting a new value stores it.
TEST_F(URLRequestContextExtTest, SetShouldReportNewNavigationInfo_NewValue) {
  auto context = BuildFullContext();
  EXPECT_FALSE(context->ShouldReportNewNavigationInfo());
  context->SetShouldReportNewNavigationInfo(true);
  EXPECT_TRUE(context->ShouldReportNewNavigationInfo());
  context->SetShouldReportNewNavigationInfo(false);
  EXPECT_FALSE(context->ShouldReportNewNavigationInfo());
}
#endif

}  // namespace net

#endif  // BUILDFLAG(...)
