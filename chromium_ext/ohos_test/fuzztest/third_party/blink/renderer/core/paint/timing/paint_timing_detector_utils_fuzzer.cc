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
#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "net/http/http_raw_request_headers.h"
#include "net/filter/source_stream.h"
#include "net/base/auth.h"
#include "url/origin.h"

#define private public
#define protected public
#include "net/base/isolation_info.h"
#include "net/base/io_buffer.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_info.h"
#include "arkweb/chromium_ext/services/network/prp_preload/src/preload_runner/prpp_request_loader_impl.h"
#include "arkweb/chromium_ext/net/base/page_res_request_info.h"
#undef protected
#undef private

namespace ohos_prp_preload {

constexpr int CACHE_BLOCK_SIZE_TEST = 64 * 1024;
constexpr int MAX_PRPP_BODY_CACHE_SIZE_TEST = 32;
constexpr int MIN_PRPP_BODY_CACHE_SIZE_TEST = 4;
constexpr int STATE_ENUM_TEST = 8;
constexpr size_t kMaxIterations = 10;

static std::unique_ptr<base::test::TaskEnvironment> task_environment_ = nullptr;
static std::unique_ptr<net::URLRequestContext> g_url_request_context = nullptr;

struct Environment {
    Environment() {
        base::CommandLine::Init(0, nullptr);
        task_environment_ = std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::DEFAULT);
        g_url_request_context = net::CreateTestURLRequestContextBuilder()->Build();
    }
    ~Environment() {
        g_url_request_context.reset();
        task_environment_.reset();
    }
};

void FuzzCreatePRPPRequestLoader(FuzzedDataProvider* fdp) {
    std::string main_url;
    std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
    net::IsolationInfo isolationInfo;
    
    if (!g_url_request_context) return;
    
    std::shared_ptr<PRPPRequestLoader> loader = PRPPRequestLoader::CreatePRPPRequestLoader(
        main_url, info, isolationInfo, g_url_request_context->GetWeakPtr(), ResPreloadedCB());
    (void)loader;
    
    PRPPRequestLoaderImpl impl(main_url, info, g_url_request_context->GetWeakPtr(), ResPreloadedCB());
    impl.prpp_req_info_ = nullptr;
    impl.Init(isolationInfo);
}

void FuzzInitAndStartUrlRequest(FuzzedDataProvider* fdp) {
    std::string main_url;
    std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
    net::IsolationInfo isolationInfo;
    
    if (!g_url_request_context) return;
    
    PRPPRequestLoaderImpl impl(main_url, info, g_url_request_context->GetWeakPtr(), ResPreloadedCB());

    std::shared_ptr<PRRequestInfo> info1 = std::make_shared<PRRequestInfo>();
    info1->set_url(GURL("http://test.test:111"));
    impl.InitAndStartUrlRequest(info1, isolationInfo, true);
    
    info1->set_type(PRRequestInfoType::TYPE_PAGE_PREFLIGHT);
    impl.InitAndStartUrlRequest(info1, isolationInfo, true);
    
    url::Origin origin = url::Origin::Create(GURL("http://example.com"));
    isolationInfo = net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther, origin, origin, net::SiteForCookies());
    impl.InitAndStartUrlRequest(info1, isolationInfo, false);
    
    info1->set_initiator(std::make_optional<url::Origin>());
    impl.InitAndStartUrlRequest(info1, isolationInfo, false);
    
    info1->set_allow_credentials(true);
    impl.InitAndStartUrlRequest(info1, isolationInfo, false);
}

void FuzzSetRawRequestHeadersAndNotify(FuzzedDataProvider* fdp) {
    std::string main_url;
    std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
    
    if (!g_url_request_context) return;
    
    PRPPRequestLoaderImpl impl(main_url, info, g_url_request_context->GetWeakPtr(), ResPreloadedCB());
    impl.SetRawRequestHeadersAndNotify(net::HttpRawRequestHeaders());
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_REQ_SENT;
    impl.SetRawRequestHeadersAndNotify(net::HttpRawRequestHeaders());
    
    impl.need_do_replay_self_ = true;
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_IDLE;
    impl.SetRawRequestHeadersAndNotify(net::HttpRawRequestHeaders());
}

void FuzzNotifyEarlyResponse(FuzzedDataProvider* fdp) {
    std::string main_url;
    std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
    
    if (!g_url_request_context) return;
    
    PRPPRequestLoaderImpl impl(main_url, info, g_url_request_context->GetWeakPtr(), ResPreloadedCB());
    scoped_refptr<const net::HttpResponseHeaders> headersRefptr;
    impl.NotifyEarlyResponse(headersRefptr);
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_ERALY_HEAD_RESPONSED;
    impl.NotifyEarlyResponse(headersRefptr);
    
    impl.need_do_replay_self_ = true;
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_IDLE;
    impl.NotifyEarlyResponse(headersRefptr);
}

void FuzzSetRawResponseHeaders(FuzzedDataProvider* fdp) {
    std::string main_url;
    std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
    
    if (!g_url_request_context) return;
    
    PRPPRequestLoaderImpl impl(main_url, info, g_url_request_context->GetWeakPtr(), ResPreloadedCB());
    scoped_refptr<const net::HttpResponseHeaders> headersRefptr;
    impl.SetRawResponseHeaders(headersRefptr);
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_HEAD_RESPONSED;
    impl.SetRawResponseHeaders(headersRefptr);
    
    impl.need_do_replay_self_ = true;
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_IDLE;
    impl.SetRawResponseHeaders(headersRefptr);
}

void FuzzOnConnected(FuzzedDataProvider* fdp) {
    std::string main_url;
    std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
    
    if (!g_url_request_context) return;
    
    PRPPRequestLoaderImpl impl(main_url, info, g_url_request_context->GetWeakPtr(), ResPreloadedCB());
    net::TransportInfo transportInfo;
    impl.OnConnected(nullptr, transportInfo, net::CompletionOnceCallback());
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_CONNECTED;
    impl.OnConnected(nullptr, transportInfo, net::CompletionOnceCallback());
    
    impl.need_do_replay_self_ = true;
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_IDLE;
    impl.OnConnected(nullptr, transportInfo, net::CompletionOnceCallback());
}

void FuzzOnReceivedRedirect(FuzzedDataProvider* fdp) {
    std::string main_url;
    std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
    
    if (!g_url_request_context) return;
    
    PRPPRequestLoaderImpl impl(main_url, info, g_url_request_context->GetWeakPtr(), ResPreloadedCB());

    net::RedirectInfo redirect_info;
    bool defer_redirect = true;
    net::AuthChallengeInfo authInfo;
    scoped_refptr<net::SSLCertRequestInfo> requestInfo = base::MakeRefCounted<net::SSLCertRequestInfo>();
    net::SSLInfo sslInfo;
    
    impl.OnReceivedRedirect(nullptr, redirect_info, &defer_redirect);
    impl.OnAuthRequired(nullptr, authInfo);
    impl.OnCertificateRequested(nullptr, requestInfo.get());
    impl.OnSSLCertificateError(nullptr, 1, sslInfo, true);
    
    impl.res_loaded_cb_ = base::BindOnce([](PRPPRequestLoader* prpp_loader) {});
    impl.OnReceivedRedirect(nullptr, redirect_info, &defer_redirect);
    impl.OnAuthRequired(nullptr, authInfo);
    impl.OnCertificateRequested(nullptr, requestInfo.get());
    impl.OnSSLCertificateError(nullptr, 1, sslInfo, true);
}

void FuzzOnResponseStarted(FuzzedDataProvider* fdp) {
    std::string main_url;
    std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
    net::IsolationInfo isolationInfo;
    
    if (!g_url_request_context) return;
    
    PRPPRequestLoaderImpl impl(main_url, info, g_url_request_context->GetWeakPtr(), ResPreloadedCB());

    std::shared_ptr<PRRequestInfo> info1 = std::make_shared<PRRequestInfo>();
    info1->set_url(GURL("http://test.test:111"));
    impl.InitAndStartUrlRequest(info1, isolationInfo, true);
    
    impl.OnResponseStarted(nullptr, net::OK);
    impl.OnResponseStarted(nullptr, net::ERR_CACHE_MISS);
    
    impl.real_load_flags_ = net::LOAD_ONLY_FROM_CACHE;
    impl.OnResponseStarted(nullptr, net::OK);
    impl.real_load_flags_ = net::OK;
    impl.OnResponseStarted(nullptr, net::OK);
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_IDLE;
    impl.OnResponseStarted(nullptr, net::LOAD_DISABLE_CACHE);
    
    impl.need_do_replay_self_ = true;
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_IDLE;
    impl.OnResponseStarted(nullptr, net::OK);
}

void FuzzReadMore(FuzzedDataProvider* fdp) {
    std::string main_url;
    std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
    net::IsolationInfo isolationInfo;
    
    if (!g_url_request_context) return;
    
    PRPPRequestLoaderImpl impl(main_url, info, g_url_request_context->GetWeakPtr(), ResPreloadedCB());

    std::shared_ptr<PRRequestInfo> info1 = std::make_shared<PRRequestInfo>();
    info1->set_url(GURL("http://test.test:111"));
    impl.InitAndStartUrlRequest(info1, isolationInfo, true);
    
    impl.ReadMore();
    
    if (impl.cur_write_block_) {
        impl.cur_write_block_->SetCapacity(0);
        impl.ReadMore();
        impl.cur_write_block_->SetCapacity(1);
        impl.ReadMore();
    }
}

void FuzzDidRead(FuzzedDataProvider* fdp) {
    std::string main_url;
    std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
    net::IsolationInfo isolationInfo;
    
    if (!g_url_request_context) return;
    
    PRPPRequestLoaderImpl impl(main_url, info, g_url_request_context->GetWeakPtr(), ResPreloadedCB());

    std::shared_ptr<PRRequestInfo> info1 = std::make_shared<PRRequestInfo>();
    info1->set_url(GURL("http://test.test:111"));
    impl.InitAndStartUrlRequest(info1, isolationInfo, true);
    
    impl.ReadMore();
    impl.DidRead(CACHE_BLOCK_SIZE_TEST + 1, false);
    impl.DidRead(-1, false);
    impl.DidRead(0, false);
    
    impl.total_size_ = 1;
    impl.DidRead(0, false);
    
    impl.res_loaded_cb_ = base::BindOnce([](PRPPRequestLoader* prpp_loader) {});
    impl.ReadMore();
    impl.DidRead(0, false);
    impl.ReadMore();
    impl.DidRead(1, false);
    impl.DidRead(1, true);
    
    for (int i = 0; i < MAX_PRPP_BODY_CACHE_SIZE_TEST + 1; i++) {
        impl.body_cache_.push(scoped_refptr<net::GrowableIOBuffer>());
    }
    impl.DidRead(1, true);
}

void FuzzOnReadCompleted(FuzzedDataProvider* fdp) {
    std::string main_url;
    std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
    net::IsolationInfo isolationInfo;
    
    if (!g_url_request_context) return;
    
    PRPPRequestLoaderImpl impl(main_url, info, g_url_request_context->GetWeakPtr(), ResPreloadedCB());

    impl.OnReadCompleted(nullptr, 0);
    
    std::shared_ptr<PRRequestInfo> info1 = std::make_shared<PRRequestInfo>();
    info1->set_url(GURL("http://test.test:111"));
    impl.InitAndStartUrlRequest(info1, isolationInfo, true);
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_UNSUPPORT;
    impl.OnReadCompleted(impl.url_request_.get(), 0);
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_ERROR;
    impl.OnReadCompleted(impl.url_request_.get(), 0);
    
    impl.ReadMore();
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_IDLE;
    impl.OnReadCompleted(impl.url_request_.get(), 0);
    
    impl.SetRequestDelegate(nullptr);
    impl.SetRequestHeadersCallback(base::RepeatingCallback<void(net::HttpRawRequestHeaders)>());
    impl.SetResponseHeadersCallback(base::RepeatingCallback<void(scoped_refptr<const net::HttpResponseHeaders>)>());
    impl.SetEarlyResponseHeadersCallback(base::RepeatingCallback<void(scoped_refptr<const net::HttpResponseHeaders>)>());
}

void FuzzStartAndDoReplay(FuzzedDataProvider* fdp) {
    std::string main_url;
    std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
    net::IsolationInfo isolationInfo;
    
    if (!g_url_request_context) return;
    
    PRPPRequestLoaderImpl impl(main_url, info, g_url_request_context->GetWeakPtr(), ResPreloadedCB());

    impl.SetRequestDelegate(&impl);
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_UNSUPPORT;
    impl.StartReplay();
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_ERROR;
    impl.StartReplay();
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_IDLE;
    impl.StartReplay();
    
    impl.need_update_req_info_ = true;
    impl.StartReplay();
    
    impl.SetUpdateResRequestInfoCallback(base::BindOnce([](const std::string& key,
        const std::shared_ptr<ohos_prp_preload::PRRequestInfo>& info) {}));
    impl.StartReplay();
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_RESPONSED;
    impl.DoReplay();
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_UNSUPPORT;
    impl.DoReplay();
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_ERROR;
    impl.DoReplay();
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_IDLE;
    impl.DoReplay();
    
    impl.rec_msg_list_.push(PRPPRequestLoaderImpl::PRPPRecorderMsg::MSG_RESPONSE_BODY);
    impl.DoReplay();
    
    impl.delegate_ = nullptr;
    impl.StartReplay();
    impl.DoReplay();
}

void FuzzProcessMessages(FuzzedDataProvider* fdp) {
    std::string main_url;
    std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
    
    if (!g_url_request_context) return;
    
    PRPPRequestLoaderImpl impl(main_url, info, g_url_request_context->GetWeakPtr(), ResPreloadedCB());
    impl.SetRequestDelegate(&impl);

    impl.rec_msg_list_.push(static_cast<PRPPRequestLoaderImpl::PRPPRecorderMsg>(STATE_ENUM_TEST));
    impl.ProcessMessages();
    
    impl.rec_msg_list_.push(PRPPRequestLoaderImpl::PRPPRecorderMsg::MSG_CONNECTED);
    impl.ProcessMessages();
    
    impl.rec_msg_list_.push(PRPPRequestLoaderImpl::PRPPRecorderMsg::MSG_RAW_REQ_HDR);
    impl.ProcessMessages();
    
    impl.rec_msg_list_.push(PRPPRequestLoaderImpl::PRPPRecorderMsg::MSG_EARLY_RESP_HDR);
    impl.ProcessMessages();
    
    impl.rec_msg_list_.push(PRPPRequestLoaderImpl::PRPPRecorderMsg::MSG_RAW_RESP_HDR);
    impl.ProcessMessages();
    
    impl.SetRequestHeadersCallback(base::BindRepeating([&](net::HttpRawRequestHeaders headers) {}));
    impl.rec_msg_list_.push(PRPPRequestLoaderImpl::PRPPRecorderMsg::MSG_RAW_REQ_HDR);
    impl.ProcessMessages();
    
    impl.SetEarlyResponseHeadersCallback(
        base::BindRepeating([&](scoped_refptr<const net::HttpResponseHeaders> headers) {}));
    impl.rec_msg_list_.push(PRPPRequestLoaderImpl::PRPPRecorderMsg::MSG_EARLY_RESP_HDR);
    impl.ProcessMessages();
    
    impl.SetResponseHeadersCallback(base::BindRepeating([&](scoped_refptr<const net::HttpResponseHeaders> headers) {}));
    impl.rec_msg_list_.push(PRPPRequestLoaderImpl::PRPPRecorderMsg::MSG_RAW_RESP_HDR);
    impl.ProcessMessages();

    impl.rec_msg_list_.push(PRPPRequestLoaderImpl::PRPPRecorderMsg::MSG_RESPONSE_BODY);
    impl.ProcessMessages();
    
    impl.rec_msg_list_.push(PRPPRequestLoaderImpl::PRPPRecorderMsg::MSG_ERROR);
    impl.ProcessMessages();
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_IDLE;
    impl.rec_msg_list_.push(PRPPRequestLoaderImpl::PRPPRecorderMsg::MSG_RESPONSE_STARTED);
    impl.ProcessMessages();
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_RESPONSE_STARTED;
    impl.rec_msg_list_.push(PRPPRequestLoaderImpl::PRPPRecorderMsg::MSG_RESPONSE_STARTED);
    impl.ProcessMessages();
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_RESPONSED;
    impl.rec_msg_list_.push(PRPPRequestLoaderImpl::PRPPRecorderMsg::MSG_RESPONSE_STARTED);
    impl.ProcessMessages();
}

void FuzzProcessMessages2(FuzzedDataProvider* fdp) {
    std::string main_url;
    std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
    
    if (!g_url_request_context) return;
    
    PRPPRequestLoaderImpl impl(main_url, info, g_url_request_context->GetWeakPtr(), ResPreloadedCB());
    impl.SetRequestDelegate(&impl);

    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_IDLE;
    impl.rec_msg_list_.push(PRPPRequestLoaderImpl::PRPPRecorderMsg::MSG_RAW_REQ_HDR);
    impl.ProcessMessages();
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_RESPONSE_STARTED;
    impl.rec_msg_list_.push(PRPPRequestLoaderImpl::PRPPRecorderMsg::MSG_RAW_REQ_HDR);
    impl.ProcessMessages();
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_RESPONSED;
    impl.rec_msg_list_.push(PRPPRequestLoaderImpl::PRPPRecorderMsg::MSG_RAW_REQ_HDR);
    impl.ProcessMessages();
    
    impl.rec_msg_list_.push(PRPPRequestLoaderImpl::PRPPRecorderMsg::MSG_RAW_REQ_HDR);
    impl.rec_msg_list_.push(PRPPRequestLoaderImpl::PRPPRecorderMsg::MSG_RAW_REQ_HDR);
    impl.ProcessMessages();
}

void FuzzRead(FuzzedDataProvider* fdp) {
    std::string main_url;
    std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
    
    if (!g_url_request_context) return;
    
    PRPPRequestLoaderImpl impl(main_url, info, g_url_request_context->GetWeakPtr(), ResPreloadedCB());

    impl.Read(nullptr, 0);
    
    impl.need_continue_read_ = true;
    impl.Read(nullptr, 0);
    
    impl.Read(nullptr, 1);
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_RESPONSED;
    impl.Read(nullptr, 1);
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_UNSUPPORT;
    impl.Read(nullptr, 1);
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_ERROR;
    impl.Read(nullptr, 1);

    impl.body_cache_.push(nullptr);
    impl.Read(nullptr, 1);
    
    impl.body_cache_.push(base::MakeRefCounted<net::GrowableIOBuffer>());
    impl.Read(nullptr, 1);
    
    impl.cur_read_offset_ = -1;
    impl.Read(nullptr, 1);
    
    impl.body_cache_.push(base::MakeRefCounted<net::GrowableIOBuffer>());

    auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);

    for (int i = 0; i < MIN_PRPP_BODY_CACHE_SIZE_TEST + 1; i++) {
        impl.body_cache_.push(base::MakeRefCounted<net::GrowableIOBuffer>());
    }
    impl.Read(nullptr, 0);
}

void FuzzUpdateResRequestInfo(FuzzedDataProvider* fdp) {
    std::string main_url;
    std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
    
    if (!g_url_request_context) return;
    
    PRPPRequestLoaderImpl impl(main_url, info, g_url_request_context->GetWeakPtr(), ResPreloadedCB());

    std::string key = "test";
    impl.UpdateResRequestInfo(key, info);
    
    impl.SetRequestDelegate(&impl);
    impl.UpdateResRequestInfo(key, info);
    
    impl.SetUpdateResRequestInfoCallback(base::BindOnce([](const std::string& key,
        const std::shared_ptr<ohos_prp_preload::PRRequestInfo>& info) {}));
    impl.UpdateResRequestInfo(key, info);

    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_UNSUPPORT;
    impl.PushFailure(PRPPRequestLoaderImpl::STATE_UNSUPPORT, false);
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_ERROR;
    impl.PushFailure(PRPPRequestLoaderImpl::STATE_ERROR, false);
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_IDLE;
    impl.PushFailure(PRPPRequestLoaderImpl::STATE_UNSUPPORT, false);
    
    impl.preload_state_ = PRPPRequestLoaderImpl::STATE_IDLE;
    impl.PushFailure(PRPPRequestLoaderImpl::STATE_IDLE, true);

    impl.ClearMsgList();
    impl.ClearBodyCache();
    impl.ClearLoader();
}

void FuzzGetLoadTimingInfo(FuzzedDataProvider* fdp) {
    std::string main_url;
    std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
    net::IsolationInfo isolationInfo;
    
    if (!g_url_request_context) return;
    
    PRPPRequestLoaderImpl impl(main_url, info, g_url_request_context->GetWeakPtr(), ResPreloadedCB());

    impl.GetLoadTimingInfo(nullptr);
    
    net::LoadTimingInfo load_timing_info;
    impl.GetLoadTimingInfo(&load_timing_info);
    
    std::shared_ptr<PRRequestInfo> info1 = std::make_shared<PRRequestInfo>();
    info1->set_url(GURL("http://test.test:111"));
    impl.InitAndStartUrlRequest(info1, isolationInfo, true);
    
    impl.GetLoadTimingInfo(&load_timing_info);
}

void FuzzClearLoaderCallback(FuzzedDataProvider* fdp) {
    std::string main_url;
    std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
    
    if (!g_url_request_context) return;
    
    PRPPRequestLoaderImpl impl(main_url, info, g_url_request_context->GetWeakPtr(), ResPreloadedCB());

    impl.ClearLoaderCallback(true);
    impl.ClearLoaderCallback(false);
}

} // namespace ohos_prp_preload

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if ((data == nullptr) || (size == 0)) {
        return 0;
    }

    static ohos_prp_preload::Environment env;

    FuzzedDataProvider fdp(data, size);
    
    size_t iterations = fdp.ConsumeIntegralInRange<size_t>(1, ohos_prp_preload::kMaxIterations);
    
    for (size_t i = 0; i < iterations && fdp.remaining_bytes() > 0; ++i) {
        int scenario = fdp.ConsumeIntegralInRange<int>(0, 16);
        
        switch (scenario) {
            case 0:
                ohos_prp_preload::FuzzCreatePRPPRequestLoader(&fdp);
                break;
            case 1:
                ohos_prp_preload::FuzzInitAndStartUrlRequest(&fdp);
                break;
            case 2:
                ohos_prp_preload::FuzzSetRawRequestHeadersAndNotify(&fdp);
                break;
            case 3:
                ohos_prp_preload::FuzzNotifyEarlyResponse(&fdp);
                break;
            case 4:
                ohos_prp_preload::FuzzSetRawResponseHeaders(&fdp);
                break;
            case 5:
                ohos_prp_preload::FuzzOnConnected(&fdp);
                break;
            case 6:
                ohos_prp_preload::FuzzOnReceivedRedirect(&fdp);
                break;
            case 7:
                ohos_prp_preload::FuzzOnResponseStarted(&fdp);
                break;
            case 8:
                ohos_prp_preload::FuzzReadMore(&fdp);
                break;
            case 9:
                ohos_prp_preload::FuzzDidRead(&fdp);
                break;
            case 10:
                ohos_prp_preload::FuzzOnReadCompleted(&fdp);
                break;
            case 11:
                ohos_prp_preload::FuzzStartAndDoReplay(&fdp);
                break;
            case 12:
                ohos_prp_preload::FuzzProcessMessages(&fdp);
                break;
            case 13:
                ohos_prp_preload::FuzzRead(&fdp);
                break;
            case 14:
                ohos_prp_preload::FuzzUpdateResRequestInfo(&fdp);
                break;
            case 15:
                ohos_prp_preload::FuzzClearLoaderCallback(&fdp);
                break;
            default:
                break;
        }
    }
    return 0;
}