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
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"

#define private public
#define protected public
#include "arkweb/chromium_ext/services/network/prp_preload/src/res_preload_scheduler.h"
#include "arkweb/chromium_ext/services/network/prp_preload/include/preload_runner/prpp_request_loader_factory.h"
#include "arkweb/chromium_ext/net/base/page_res_request_info.h"
#undef protected
#undef private

namespace ohos_prp_preload {

constexpr size_t kMaxIterations = 15;
constexpr size_t kMaxUrlLen = 256;

static std::unique_ptr<base::test::TaskEnvironment> task_environment_ = nullptr;

struct Environment {
    Environment() {
        task_environment_ = std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::DEFAULT,
            base::test::TaskEnvironment::ThreadingMode::MAIN_THREAD_ONLY);
    }
    ~Environment() {
        task_environment_.reset();
    }
};

void RunUntilIdle() {
    if (task_environment_) {
        task_environment_->RunUntilIdle();
    }
}

std::shared_ptr<PRPPReqInfoTreeNode> CreateTreeNode(FuzzedDataProvider* fdp) {
    auto node = std::make_shared<PRPPReqInfoTreeNode>();
    std::string url_str = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    if (url_str.empty()) {
        url_str = "https://example.com";
    }
    node->req_info_ = std::make_shared<PRRequestInfo>(GURL(url_str), true);
    node->req_info_->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
    node->req_info_->set_page_origin(fdp->ConsumeRandomLengthString(kMaxUrlLen));
    
    size_t child_count = fdp->ConsumeIntegralInRange<size_t>(0, 5);
    for (size_t i = 0; i < child_count; ++i) {
        auto child = std::make_shared<PRPPReqInfoTreeNode>();
        std::string child_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
        child->req_info_ = std::make_shared<PRRequestInfo>(GURL(child_url), true);
        child->req_info_->set_method(fdp->ConsumeBool() ? "GET" : "POST");
        child->req_info_->set_preload_flag(static_cast<PRRequestFlags>(fdp->ConsumeIntegral<uint32_t>()));
        node->children_.push_back(child);
    }
    return node;
}

PRPPPreconnectInfoList CreatePreconnectList(FuzzedDataProvider* fdp) {
    PRPPPreconnectInfoList list;
    size_t count = fdp->ConsumeIntegralInRange<size_t>(0, 35);
    for (size_t i = 0; i < count; ++i) {
        PRPPPreconnectInfo info;
        info.origin_url_ = GURL(fdp->ConsumeRandomLengthString(kMaxUrlLen));
        info.allow_credential_ = fdp->ConsumeBool();
        info.net_anonymization_key_ = net::NetworkAnonymizationKey();
        list.push_back(info);
    }
    return list;
}

void FuzzStopPreloadScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    scheduler.StopPreload();
}

void FuzzSetPRPPReqLoaderFacScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    std::shared_ptr<PRPPRequestLoaderFactory> factory =
        PRPPRequestLoaderFactory::CreatePRPPRequestLoaderFactory(url, 
            url_request_context->GetWeakPtr());
    scheduler.SetPRPPReqLoaderFac(factory->GetWeak(), fdp->ConsumeBool());
}

void FuzzSetPRPPReqLoaderFacNullScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    scheduler.SetPRPPReqLoaderFac(base::WeakPtr<PRPPRequestLoaderFactory>(), fdp->ConsumeBool());
}

void FuzzSchedulePreloadsScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb = base::BindRepeating([](const std::string&, const std::string&){});
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    PRPPPreconnectInfoList list = CreatePreconnectList(fdp);
    std::shared_ptr<PRPPReqInfoTreeNode> node = CreateTreeNode(fdp);
    std::set<std::string> urls;
    
    size_t url_count = fdp->ConsumeIntegralInRange<size_t>(0, 5);
    for (size_t i = 0; i < url_count; ++i) {
        urls.insert(fdp->ConsumeRandomLengthString(kMaxUrlLen));
    }
    
    scheduler.SchedulePreloads(list, node, urls);
}

void FuzzSchedulePreloadsTwiceScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    PRPPPreconnectInfoList list = CreatePreconnectList(fdp);
    std::shared_ptr<PRPPReqInfoTreeNode> node = CreateTreeNode(fdp);
    
    scheduler.SchedulePreloads(list, node, {});
    scheduler.SchedulePreloads(list, node, {});
}

void FuzzSchedulePreloadsNullTreeScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    PRPPPreconnectInfoList list = CreatePreconnectList(fdp);
    
    scheduler.SchedulePreloads(list, nullptr, {});
}

void FuzzSchedulePreloadsNoPageOriginScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    PRPPPreconnectInfoList list = CreatePreconnectList(fdp);
    auto node = std::make_shared<PRPPReqInfoTreeNode>();
    node->req_info_ = std::make_shared<PRRequestInfo>(GURL("https://test.com"), true);
    node->req_info_->set_type(PRRequestInfoType::TYPE_DEFAULT);
    
    scheduler.SchedulePreloads(list, node, {});
}

void FuzzSchedulePreloadsEmptyChildrenScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    PRPPPreconnectInfoList list = CreatePreconnectList(fdp);
    auto node = std::make_shared<PRPPReqInfoTreeNode>();
    node->req_info_ = std::make_shared<PRRequestInfo>(GURL("https://test.com"), true);
    node->req_info_->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
    node->req_info_->set_page_origin("test_origin");
    
    scheduler.SchedulePreloads(list, node, {});
}

void FuzzSchedulePreloadsWithCallbackScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb = base::BindRepeating([](const std::string& url, const std::string& origin){});
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    PRPPPreconnectInfoList list;
    auto node = std::make_shared<PRPPReqInfoTreeNode>();
    node->req_info_ = std::make_shared<PRRequestInfo>(GURL("https://test.com"), true);
    node->req_info_->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
    node->req_info_->set_page_origin("test_origin");
    
    auto child1 = std::make_shared<PRPPReqInfoTreeNode>();
    child1->req_info_ = std::make_shared<PRRequestInfo>(GURL("https://child1.com"), true);
    node->children_.push_back(child1);
    
    scheduler.SchedulePreloads(list, node, {});
}

void FuzzUpdateIdlePrerequestCountScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    scheduler.UpdateIdlePrerequestCount();
    scheduler.UpdateIdlePrerequestCount();
    scheduler.UpdateIdlePrerequestCount();
}

void FuzzSchedulePreconnectsScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    scheduler.prpp_preconnect_info_list_ = CreatePreconnectList(fdp);
    scheduler.preload_triggered_ = true;
    scheduler.SchedulePreconnects();
}

void FuzzSchedulePreconnectsNullRunnerScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, nullptr, 
        base::WeakPtr<net::URLRequestContext>(), on_page_origin_cb);
    
    scheduler.prpp_preconnect_info_list_ = CreatePreconnectList(fdp);
    scheduler.SchedulePreconnects();
}

void FuzzContinueSchedulePreconnectsScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    scheduler.prpp_preconnect_info_list_ = CreatePreconnectList(fdp);
    scheduler.preload_triggered_ = true;
    scheduler.info_list_version_ = 1;
    
    if (!scheduler.prpp_preconnect_info_list_.empty()) {
        auto iter = scheduler.prpp_preconnect_info_list_.begin();
        scheduler.ContinueSchedulePreconnects(iter, 1);
    }
}

void FuzzContinueSchedulePreconnectsVersionMismatch(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    scheduler.prpp_preconnect_info_list_ = CreatePreconnectList(fdp);
    scheduler.preload_triggered_ = true;
    scheduler.info_list_version_ = 1;
    
    if (!scheduler.prpp_preconnect_info_list_.empty()) {
        auto iter = scheduler.prpp_preconnect_info_list_.begin();
        scheduler.ContinueSchedulePreconnects(iter, 2);
    }
}

void FuzzSchedulePrerequestsScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    auto node = CreateTreeNode(fdp);
    scheduler.preload_info_tree_ = node;
    scheduler.cur_parent_ = node;
    if (!node->children_.empty()) {
        scheduler.cur_node_iter_ = node->children_.begin();
    }
    scheduler.preload_triggered_ = true;
    scheduler.info_list_version_ = 1;
    scheduler.loader_fac_weak_valid_ = true;
    
    scheduler.SchedulePrerequests(12, 1);
}

void FuzzSchedulePrerequestsZeroLimit(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    auto node = CreateTreeNode(fdp);
    scheduler.preload_info_tree_ = node;
    scheduler.cur_parent_ = node;
    if (!node->children_.empty()) {
        scheduler.cur_node_iter_ = node->children_.begin();
    }
    scheduler.preload_triggered_ = true;
    scheduler.info_list_version_ = 1;
    scheduler.loader_fac_weak_valid_ = true;
    scheduler.idle_prerequest_count_ = 0;
    
    scheduler.SchedulePrerequests(0, 1);
}

void FuzzSchedulePrerequestsVersionMismatch(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    scheduler.preload_triggered_ = true;
    scheduler.info_list_version_ = 1;
    scheduler.loader_fac_weak_valid_ = true;
    
    scheduler.SchedulePrerequests(12, 2);
}

void FuzzCreateReqLoaderAndStartScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    std::shared_ptr<PRPPRequestLoaderFactory> factory =
        PRPPRequestLoaderFactory::CreatePRPPRequestLoaderFactory(url, 
            url_request_context->GetWeakPtr());
    scheduler.SetPRPPReqLoaderFac(factory->GetWeak(), true);
    
    auto info = std::make_shared<PRRequestInfo>(GURL("https://child.com"), true);
    scheduler.CreateReqLoaderAndStart(info);
}

void FuzzCreateReqLoaderAndStartPendingScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    scheduler.loader_fac_weak_valid_ = false;
    
    auto info = std::make_shared<PRRequestInfo>(GURL("https://child.com"), true);
    scheduler.CreateReqLoaderAndStart(info);
}

void FuzzFullWorkflowScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb = base::BindRepeating([](const std::string&, const std::string&){});
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    std::shared_ptr<PRPPRequestLoaderFactory> factory =
        PRPPRequestLoaderFactory::CreatePRPPRequestLoaderFactory(url, 
            url_request_context->GetWeakPtr());
    scheduler.SetPRPPReqLoaderFac(factory->GetWeak(), true);
    
    PRPPPreconnectInfoList list = CreatePreconnectList(fdp);
    auto node = CreateTreeNode(fdp);
    
    scheduler.SchedulePreloads(list, node, {"https://record.com"});
}

void FuzzStopAndRestartScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    std::shared_ptr<PRPPRequestLoaderFactory> factory =
        PRPPRequestLoaderFactory::CreatePRPPRequestLoaderFactory(url, 
            url_request_context->GetWeakPtr());
    scheduler.SetPRPPReqLoaderFac(factory->GetWeak(), true);
    
    PRPPPreconnectInfoList list = CreatePreconnectList(fdp);
    auto node = CreateTreeNode(fdp);
    
    scheduler.SchedulePreloads(list, node, {});
    scheduler.StopPreload();
    
    scheduler.SetPRPPReqLoaderFac(factory->GetWeak(), true);
    scheduler.SchedulePreloads(list, node, {});
}

void FuzzIdlePrerequestCountIncrement(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    scheduler.idle_prerequest_count_ = fdp->ConsumeIntegralInRange<int>(0, 10);
    scheduler.UpdateIdlePrerequestCount();
}

void FuzzSchedulePrerequestsWithChildren(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    auto node = std::make_shared<PRPPReqInfoTreeNode>();
    node->req_info_ = std::make_shared<PRRequestInfo>(GURL("https://parent.com"), true);
    node->req_info_->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
    
    for (int i = 0; i < fdp->ConsumeIntegralInRange<int>(1, 10); ++i) {
        auto child = std::make_shared<PRPPReqInfoTreeNode>();
        std::string child_url = "https://child" + std::to_string(i) + ".com";
        child->req_info_ = std::make_shared<PRRequestInfo>(GURL(child_url), true);
        child->req_info_->set_method("GET");
        uint32_t flags = fdp->ConsumeIntegral<uint32_t>();
        child->req_info_->set_preload_flag(static_cast<PRRequestFlags>(flags));
        node->children_.push_back(child);
    }
    
    scheduler.preload_info_tree_ = node;
    scheduler.cur_parent_ = node;
    scheduler.cur_node_iter_ = node->children_.begin();
    scheduler.preload_triggered_ = true;
    scheduler.info_list_version_ = 1;
    scheduler.loader_fac_weak_valid_ = true;
    
    scheduler.SchedulePrerequests(fdp->ConsumeIntegralInRange<uint32_t>(1, 12), 1);
}

void FuzzSchedulePrerequestsNotTriggered(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    scheduler.preload_triggered_ = false;
    scheduler.SchedulePrerequests(12, 1);
}

void FuzzSchedulePrerequestsNoLoaderFac(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    scheduler.preload_triggered_ = true;
    scheduler.loader_fac_weak_valid_ = false;
    scheduler.SchedulePrerequests(12, 1);
}

void FuzzContinueSchedulePreconnectsEnd(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    scheduler.prpp_preconnect_info_list_.clear();
    scheduler.preload_triggered_ = true;
    scheduler.info_list_version_ = 1;
    
    auto iter = scheduler.prpp_preconnect_info_list_.end();
    scheduler.ContinueSchedulePreconnects(iter, 1);
}

void FuzzContinueSchedulePreconnectsNotTriggered(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    PRPPPreconnectInfoList list;
    PRPPPreconnectInfo info;
    info.origin_url_ = GURL("https://test.com");
    list.push_back(info);
    
    scheduler.prpp_preconnect_info_list_ = list;
    scheduler.preload_triggered_ = false;
    scheduler.info_list_version_ = 1;
    
    auto iter = scheduler.prpp_preconnect_info_list_.begin();
    scheduler.ContinueSchedulePreconnects(iter, 1);
}

void FuzzCreateReqLoaderAndStartNullRunner(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, nullptr, 
        base::WeakPtr<net::URLRequestContext>(), on_page_origin_cb);
    
    auto info = std::make_shared<PRRequestInfo>(GURL("https://child.com"), true);
    scheduler.CreateReqLoaderAndStart(info);
}

void FuzzNeedRecordHeaderUrlsScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    std::set<std::string> urls;
    for (int i = 0; i < fdp->ConsumeIntegralInRange<int>(0, 5); ++i) {
        urls.insert(fdp->ConsumeRandomLengthString(kMaxUrlLen));
    }
    scheduler.need_record_header_urls_ = urls;
    
    std::shared_ptr<PRPPRequestLoaderFactory> factory =
        PRPPRequestLoaderFactory::CreatePRPPRequestLoaderFactory(url, 
            url_request_context->GetWeakPtr());
    scheduler.SetPRPPReqLoaderFac(factory->GetWeak(), true);
    
    auto info = std::make_shared<PRRequestInfo>(GURL("https://child.com"), true);
    scheduler.CreateReqLoaderAndStart(info);
}

void FuzzSetPRPPReqLoaderFacAfterPreload(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    PRPPPreconnectInfoList list;
    auto node = CreateTreeNode(fdp);
    scheduler.SchedulePreloads(list, node, {});
    
    std::shared_ptr<PRPPRequestLoaderFactory> factory =
        PRPPRequestLoaderFactory::CreatePRPPRequestLoaderFactory(url, 
            url_request_context->GetWeakPtr());
    scheduler.SetPRPPReqLoaderFac(factory->GetWeak(), true);
}

void FuzzPrerequestInfoListScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    scheduler.loader_fac_weak_valid_ = false;
    
    for (int i = 0; i < fdp->ConsumeIntegralInRange<int>(1, 5); ++i) {
        auto info = std::make_shared<PRRequestInfo>(GURL("https://info" + std::to_string(i) + ".com"), true);
        scheduler.prerequest_info_list_.push_back(info);
    }
    
    std::shared_ptr<PRPPRequestLoaderFactory> factory =
        PRPPRequestLoaderFactory::CreatePRPPRequestLoaderFactory(url, 
            url_request_context->GetWeakPtr());
    scheduler.SetPRPPReqLoaderFac(factory->GetWeak(), true);
}

void FuzzIdleConnectListScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    for (int i = 0; i < fdp->ConsumeIntegralInRange<int>(0, 5); ++i) {
        scheduler.idle_connect_list_["key" + std::to_string(i)] = fdp->ConsumeIntegral<int>();
    }
    
    scheduler.StopPreload();
}

void FuzzSchedulePreconnectsOverLimit(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    PRPPPreconnectInfoList list;
    for (int i = 0; i < 5; ++i) {
        PRPPPreconnectInfo info;
        info.origin_url_ = GURL("https://origin" + std::to_string(i) + ".com");
        info.allow_credential_ = true;
        list.push_back(info);
    }
    
    scheduler.prpp_preconnect_info_list_ = list;
    scheduler.preload_triggered_ = true;
    scheduler.SchedulePreconnects();
}

void FuzzNullTaskRunnerSchedulePreloads(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, nullptr, 
        base::WeakPtr<net::URLRequestContext>(), on_page_origin_cb);
    
    PRPPPreconnectInfoList list;
    auto node = CreateTreeNode(fdp);
    
    scheduler.SchedulePreloads(list, node, {});
}

void FuzzNullTaskRunnerContinueSchedulePreconnects(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, nullptr, 
        base::WeakPtr<net::URLRequestContext>(), on_page_origin_cb);
    
    PRPPPreconnectInfoList list;
    PRPPPreconnectInfo info;
    info.origin_url_ = GURL("https://test.com");
    list.push_back(info);
    
    scheduler.prpp_preconnect_info_list_ = list;
    scheduler.preload_triggered_ = true;
    scheduler.info_list_version_ = 1;
    
    auto iter = scheduler.prpp_preconnect_info_list_.begin();
    scheduler.ContinueSchedulePreconnects(iter, 1);
}

void FuzzSchedulePrerequestsNullParent(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    scheduler.preload_triggered_ = true;
    scheduler.info_list_version_ = 1;
    scheduler.loader_fac_weak_valid_ = true;
    scheduler.cur_parent_ = nullptr;
    
    scheduler.SchedulePrerequests(12, 1);
}

void FuzzSchedulePrerequestsNullTaskRunner(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, nullptr, 
        base::WeakPtr<net::URLRequestContext>(), on_page_origin_cb);
    
    scheduler.preload_triggered_ = true;
    scheduler.info_list_version_ = 1;
    scheduler.loader_fac_weak_valid_ = true;
    
    scheduler.SchedulePrerequests(12, 1);
}

void FuzzEmptyUrlScenario(FuzzedDataProvider* fdp) {
    std::string url = "";
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    scheduler.StopPreload();
}

void FuzzLongUrlScenario(FuzzedDataProvider* fdp) {
    std::string url;
    for (int i = 0; i < 1000; ++i) {
        url += "a";
    }
    auto net_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    PRPPOnPageOriginCB on_page_origin_cb;
    
    ResPreloadScheduler scheduler(url, net_task_runner, 
        url_request_context->GetWeakPtr(), on_page_origin_cb);
    
    scheduler.StopPreload();
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
    
    for (size_t i = 0; i < iterations && fdp.remaining_bytes() > 0; ++i) {
        int scenario = fdp.ConsumeIntegralInRange<int>(0, 37);
        
        switch (scenario) {
            case 0:
                ohos_prp_preload::FuzzStopPreloadScenario(&fdp);
                break;
            case 1:
                ohos_prp_preload::FuzzSetPRPPReqLoaderFacScenario(&fdp);
                break;
            case 2:
                ohos_prp_preload::FuzzSchedulePreloadsScenario(&fdp);
                break;
            case 3:
                ohos_prp_preload::FuzzUpdateIdlePrerequestCountScenario(&fdp);
                break;
            case 4:
                ohos_prp_preload::FuzzSetPRPPReqLoaderFacNullScenario(&fdp);
                break;
            case 5:
                ohos_prp_preload::FuzzSchedulePreloadsTwiceScenario(&fdp);
                break;
            case 6:
                ohos_prp_preload::FuzzSchedulePreloadsNullTreeScenario(&fdp);
                break;
            case 7:
                ohos_prp_preload::FuzzSchedulePreloadsNoPageOriginScenario(&fdp);
                break;
            case 8:
                ohos_prp_preload::FuzzSchedulePreloadsEmptyChildrenScenario(&fdp);
                break;
            case 9:
                ohos_prp_preload::FuzzSchedulePreloadsWithCallbackScenario(&fdp);
                break;
            case 10:
                ohos_prp_preload::FuzzSchedulePreconnectsScenario(&fdp);
                break;
            case 11:
                ohos_prp_preload::FuzzSchedulePreconnectsNullRunnerScenario(&fdp);
                break;
            case 12:
                ohos_prp_preload::FuzzContinueSchedulePreconnectsScenario(&fdp);
                break;
            case 13:
                ohos_prp_preload::FuzzContinueSchedulePreconnectsVersionMismatch(&fdp);
                break;
            case 14:
                ohos_prp_preload::FuzzSchedulePrerequestsScenario(&fdp);
                break;
            case 15:
                ohos_prp_preload::FuzzSchedulePrerequestsZeroLimit(&fdp);
                break;
            case 16:
                ohos_prp_preload::FuzzSchedulePrerequestsVersionMismatch(&fdp);
                break;
            case 17:
                ohos_prp_preload::FuzzCreateReqLoaderAndStartScenario(&fdp);
                break;
            case 18:
                ohos_prp_preload::FuzzCreateReqLoaderAndStartPendingScenario(&fdp);
                break;
            case 19:
                ohos_prp_preload::FuzzFullWorkflowScenario(&fdp);
                break;
            case 20:
                ohos_prp_preload::FuzzStopAndRestartScenario(&fdp);
                break;
            case 21:
                ohos_prp_preload::FuzzIdlePrerequestCountIncrement(&fdp);
                break;
            case 22:
                ohos_prp_preload::FuzzSchedulePrerequestsWithChildren(&fdp);
                break;
            case 23:
                ohos_prp_preload::FuzzSchedulePrerequestsNotTriggered(&fdp);
                break;
            case 24:
                ohos_prp_preload::FuzzSchedulePrerequestsNoLoaderFac(&fdp);
                break;
            case 25:
                ohos_prp_preload::FuzzContinueSchedulePreconnectsEnd(&fdp);
                break;
            case 26:
                ohos_prp_preload::FuzzContinueSchedulePreconnectsNotTriggered(&fdp);
                break;
            case 27:
                ohos_prp_preload::FuzzCreateReqLoaderAndStartNullRunner(&fdp);
                break;
            case 28:
                ohos_prp_preload::FuzzNeedRecordHeaderUrlsScenario(&fdp);
                break;
            case 29:
                ohos_prp_preload::FuzzSetPRPPReqLoaderFacAfterPreload(&fdp);
                break;
            case 30:
                ohos_prp_preload::FuzzPrerequestInfoListScenario(&fdp);
                break;
            case 31:
                ohos_prp_preload::FuzzIdleConnectListScenario(&fdp);
                break;
            case 32:
                ohos_prp_preload::FuzzSchedulePreconnectsOverLimit(&fdp);
                break;
            case 33:
                ohos_prp_preload::FuzzNullTaskRunnerSchedulePreloads(&fdp);
                break;
            case 34:
                ohos_prp_preload::FuzzNullTaskRunnerContinueSchedulePreconnects(&fdp);
                break;
            case 35:
                ohos_prp_preload::FuzzSchedulePrerequestsNullParent(&fdp);
                break;
            case 36:
                ohos_prp_preload::FuzzSchedulePrerequestsNullTaskRunner(&fdp);
                break;
            case 37:
                ohos_prp_preload::FuzzEmptyUrlScenario(&fdp);
                break;
            case 38:
                ohos_prp_preload::FuzzLongUrlScenario(&fdp);
                break;
            default:
                break;
        }
    }
    ohos_prp_preload::RunUntilIdle();
    return 0;
}