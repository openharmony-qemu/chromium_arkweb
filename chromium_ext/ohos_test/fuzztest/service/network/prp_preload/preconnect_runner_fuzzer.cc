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
#include "arkweb/chromium_ext/services/network/prp_preload/src/preload_runner/preconnect_runner.h"
#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"

namespace ohos_prp_preload {
constexpr size_t kMaxIterations = 10;
constexpr size_t kMaxUrlLen = 256;

static std::unique_ptr<base::test::TaskEnvironment> task_environment_ = nullptr;
static std::unique_ptr<net::URLRequestContext> g_url_request_context = nullptr;

std::string SanitizeUrlString(const std::string& input) {
    std::string result;
    for (char c : input) {
        if (c >= 0x20 && c <= 0x7E && c != '\0') {
            result += c;
        }
    }
    return result;
}

GURL CreateSafeUrl(FuzzedDataProvider* fdp, bool need_valid_url = true) {
    std::string url_str = SanitizeUrlString(fdp->ConsumeRandomLengthString(kMaxUrlLen));
    GURL url(url_str);
    if (need_valid_url && !url.is_valid()) {
        url = GURL("https://example.com");
    }
    return url;
}

struct Environment {
    Environment() {
        task_environment_ = std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::IO);
        g_url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
    }
    ~Environment() {
        g_url_request_context.reset();
        task_environment_.reset();
    }
};

void FuzzPreconnectSocketNullContextScenario(FuzzedDataProvider* fdp) {
    GURL url = CreateSafeUrl(fdp, false);
    
    bool allow_credentials = fdp->ConsumeBool();
    net::NetworkAnonymizationKey network_anonymization_key;
    
    base::WeakPtr<net::URLRequestContext> null_context;
    PreconnectRunner::PreconnectSocket(url, allow_credentials, 
        network_anonymization_key, null_context);
}

void FuzzPreconnectSocketHttpsFixedUrlWithCredentialsScenario(FuzzedDataProvider* fdp) {
    GURL url("https://example.com");
    bool allow_credentials = true;
    net::NetworkAnonymizationKey network_anonymization_key = 
        net::NetworkAnonymizationKey::CreateSameSite(net::SchemefulSite(url));
    
    PreconnectRunner::PreconnectSocket(url, allow_credentials, 
        network_anonymization_key, g_url_request_context->GetWeakPtr());
}

void FuzzPreconnectSocketHttpsFixedUrlWithoutCredentialsScenario(FuzzedDataProvider* fdp) {
    GURL url("https://example.com");
    bool allow_credentials = false;
    net::NetworkAnonymizationKey network_anonymization_key = 
        net::NetworkAnonymizationKey::CreateSameSite(net::SchemefulSite(url));
    
    PreconnectRunner::PreconnectSocket(url, allow_credentials, 
        network_anonymization_key, g_url_request_context->GetWeakPtr());
}

void FuzzPreconnectSocketHttpFixedUrlScenario(FuzzedDataProvider* fdp) {
    GURL url("http://example.com");
    bool allow_credentials = fdp->ConsumeBool();
    net::NetworkAnonymizationKey network_anonymization_key = 
        net::NetworkAnonymizationKey::CreateSameSite(net::SchemefulSite(url));
    
    PreconnectRunner::PreconnectSocket(url, allow_credentials, 
        network_anonymization_key, g_url_request_context->GetWeakPtr());
}

void FuzzPreconnectSocketEmptyUrlScenario(FuzzedDataProvider* fdp) {
    GURL empty_url;
    bool allow_credentials = fdp->ConsumeBool();
    net::NetworkAnonymizationKey network_anonymization_key;
    
    PreconnectRunner::PreconnectSocket(empty_url, allow_credentials, 
        network_anonymization_key, g_url_request_context->GetWeakPtr());
}

void FuzzGetHSTSRedirectValidUrlScenario(FuzzedDataProvider* fdp) {
    GURL url = CreateSafeUrl(fdp, true);
    
    GURL result = PreconnectRunner::GetHSTSRedirect(url, g_url_request_context->GetWeakPtr());
}

void FuzzGetHSTSRedirectHttpUrlScenario(FuzzedDataProvider* fdp) {
    std::string host = SanitizeUrlString(fdp->ConsumeRandomLengthString(64));
    std::string path = SanitizeUrlString(fdp->ConsumeRandomLengthString(64));
    if (host.empty()) {
        host = "example.com";
    }
    std::string url_str = "http://" + host + "/" + path;
    GURL url(url_str);
    
    if (!url.is_valid()) {
        return;
    }
    
    GURL result = PreconnectRunner::GetHSTSRedirect(url, g_url_request_context->GetWeakPtr());
}

void FuzzGetHSTSRedirectHttpsUrlScenario(FuzzedDataProvider* fdp) {
    std::string host = SanitizeUrlString(fdp->ConsumeRandomLengthString(64));
    std::string path = SanitizeUrlString(fdp->ConsumeRandomLengthString(64));
    if (host.empty()) {
        host = "example.com";
    }
    std::string url_str = "https://" + host + "/" + path;
    GURL url(url_str);
    
    if (!url.is_valid()) {
        return;
    }
    
    GURL result = PreconnectRunner::GetHSTSRedirect(url, g_url_request_context->GetWeakPtr());
}

void FuzzGetHSTSRedirectEmptyUrlScenario(FuzzedDataProvider* fdp) {
    GURL empty_url;
    
    GURL result = PreconnectRunner::GetHSTSRedirect(empty_url, g_url_request_context->GetWeakPtr());
}

void FuzzGetHSTSRedirectInvalidUrlScenario(FuzzedDataProvider* fdp) {
    GURL url = CreateSafeUrl(fdp, false);
    
    GURL result = PreconnectRunner::GetHSTSRedirect(url, g_url_request_context->GetWeakPtr());
}

void FuzzGetHSTSRedirectWithPortScenario(FuzzedDataProvider* fdp) {
    std::string host = SanitizeUrlString(fdp->ConsumeRandomLengthString(64));
    int port = fdp->ConsumeIntegralInRange<int>(1, 65535);
    if (host.empty()) {
        host = "example.com";
    }
    std::string url_str = "https://" + host + ":" + std::to_string(port);
    GURL url(url_str);
    
    if (!url.is_valid()) {
        return;
    }
    
    GURL result = PreconnectRunner::GetHSTSRedirect(url, g_url_request_context->GetWeakPtr());
}

void FuzzGetHSTSRedirectHttpWithQueryScenario(FuzzedDataProvider* fdp) {
    std::string host = SanitizeUrlString(fdp->ConsumeRandomLengthString(64));
    std::string query = SanitizeUrlString(fdp->ConsumeRandomLengthString(64));
    if (host.empty()) {
        host = "example.com";
    }
    std::string url_str = "http://" + host + "/path?" + query;
    GURL url(url_str);
    
    if (!url.is_valid()) {
        return;
    }
    
    GURL result = PreconnectRunner::GetHSTSRedirect(url, g_url_request_context->GetWeakPtr());
}
} //namespace ohos_prp_preload

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if ((data == nullptr) || (size == 0)) {
        return 0;
    }

    base::CommandLine::Init(0, nullptr);
    static ohos_prp_preload::Environment env;
    FuzzedDataProvider fdp(data, size);
    size_t iterations = std::min(fdp.ConsumeIntegralInRange<size_t>(1, ohos_prp_preload::kMaxIterations), size);
    
    for (size_t i = 0; i < iterations; ++i) {
        int scenario = fdp.ConsumeIntegralInRange<int>(0, 11);
        
        switch (scenario) {
            case 0:
                ohos_prp_preload::FuzzPreconnectSocketNullContextScenario(&fdp);
                break;
            case 1:
                ohos_prp_preload::FuzzPreconnectSocketHttpsFixedUrlWithCredentialsScenario(&fdp);
                break;
            case 2:
                ohos_prp_preload::FuzzPreconnectSocketHttpsFixedUrlWithoutCredentialsScenario(&fdp);
                break;
            case 3:
                ohos_prp_preload::FuzzPreconnectSocketHttpFixedUrlScenario(&fdp);
                break;
            case 4:
                ohos_prp_preload::FuzzPreconnectSocketEmptyUrlScenario(&fdp);
                break;
            case 5:
                ohos_prp_preload::FuzzGetHSTSRedirectValidUrlScenario(&fdp);
                break;
            case 6:
                ohos_prp_preload::FuzzGetHSTSRedirectHttpUrlScenario(&fdp);
                break;
            case 7:
                ohos_prp_preload::FuzzGetHSTSRedirectHttpsUrlScenario(&fdp);
                break;
            case 8:
                ohos_prp_preload::FuzzGetHSTSRedirectEmptyUrlScenario(&fdp);
                break;
            case 9:
                ohos_prp_preload::FuzzGetHSTSRedirectInvalidUrlScenario(&fdp);
                break;
            case 10:
                ohos_prp_preload::FuzzGetHSTSRedirectWithPortScenario(&fdp);
                break;
            case 11:
                ohos_prp_preload::FuzzGetHSTSRedirectHttpWithQueryScenario(&fdp);
                break;
            default:
                break;
        }
    }
    return 0;
}