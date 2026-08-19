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

#include "arkweb/chromium_ext/services/network/prp_preload/src/res_request_info_updater.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "base/test/task_environment.h"
namespace ohos_prp_preload {

class ResRequestInfoUpdaterTest : public testing::Test {
public:
  static void SetUpTestCase();
  static void TearDownTestCase();
  void SetUp();
  void TearDown();
protected:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
};

void ResRequestInfoUpdaterTest::SetUpTestCase() {}

void ResRequestInfoUpdaterTest::TearDownTestCase() {}

void ResRequestInfoUpdaterTest::SetUp() {
    task_environment_ = std::make_unique<base::test::TaskEnvironment>(
      base::test::TaskEnvironment::MainThreadType::DEFAULT);
}

void ResRequestInfoUpdaterTest::TearDown(){
  task_environment_.reset();
}


TEST_F(ResRequestInfoUpdaterTest, ResRequestInfoUpdaterTest_001)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList& preconnect_info_list,
      const std::shared_ptr<PRPPReqInfoTreeNode>& preload_info_tree,
      const std::set<std::string>& need_record_header_urls){
      LOG(INFO) << "callback";
  });
  ResRequestInfoUpdater updater("www.test.com",networkAnonymizationKey,disk_cache_backend_factory,preload_infos_cb);
  EXPECT_NE(updater.res_req_info_cache_mgr_, nullptr);
  updater.Start();
  GURL request_url("preflight+https://example.com/path/to/resource");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  updater.UpdateResRequestInfo(request_info);
  updater.Stop();
  updater.SetPageOrigin("test");
  updater.res_req_info_cache_mgr_ = nullptr;
  updater.Start();
  updater.UpdateResRequestInfo(request_info);
  updater.Stop();
  updater.SetPageOrigin("test");
}

TEST_F(ResRequestInfoUpdaterTest, OnResRequestInfoCacheLoadedTest_001)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb;
  ResRequestInfoUpdater updater("www.test.com",networkAnonymizationKey,disk_cache_backend_factory,preload_infos_cb);
  EXPECT_NE(updater.res_req_info_cache_mgr_, nullptr);
  LinkedHashMap load_info_list;
  LinkedHashMap preconnect_limit_info_list;
  updater.OnResRequestInfoCacheLoaded(load_info_list,preconnect_limit_info_list,networkAnonymizationKey,"test");
}

TEST_F(ResRequestInfoUpdaterTest, OnResRequestInfoCacheLoadedTest_002)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList& preconnect_info_list,
      const std::shared_ptr<PRPPReqInfoTreeNode>& preload_info_tree,
      const std::set<std::string>& need_record_header_urls){
      LOG(INFO) << "callback";
  });
  ResRequestInfoUpdater updater("www.test.com",networkAnonymizationKey,disk_cache_backend_factory,preload_infos_cb);
  EXPECT_NE(updater.res_req_info_cache_mgr_, nullptr);
  LinkedHashMap load_info_list;
  LinkedHashMap preconnect_limit_info_list;

  GURL request_url("preflight+https://example.com/path/to/resource");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  load_info_list.insert({request_info->url().spec(),request_info});

  GURL request_url2("preflight+https://example.com/path/to/resource2");
  std::shared_ptr<PRRequestInfo> request_info2 = std::make_shared<PRRequestInfo>(request_url2, false);
  request_info2->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
  load_info_list.insert({request_info2->url().spec(),request_info2});

  GURL request_url3("preflight+https://example.com/path/to/resource3");
  std::shared_ptr<PRRequestInfo> request_info3 = std::make_shared<PRRequestInfo>(request_url3, false);
  request_info3->set_preload_seq_num("test");
  request_info3->set_request_end_time(100);
  load_info_list.insert({request_info3->url().spec(),request_info3});

  GURL request_url4("preflight+https://example.com/path/to/resource4");
  std::shared_ptr<PRRequestInfo> request_info4 = std::make_shared<PRRequestInfo>(request_url4, false);
  request_info4->set_preload_seq_num("test");
  request_info4->set_request_start_time(101);
  load_info_list.insert({request_info4->url().spec(),request_info4});

  GURL request_url5("preflight+https://example.com/path/to/resource5");
  std::shared_ptr<PRRequestInfo> request_info5 = std::make_shared<PRRequestInfo>(request_url5, false);
  request_info5->set_preload_seq_num("test");
  request_info5->set_request_start_time(101);
  request_info5->set_request_end_time(100);
  request_info5->set_preload_flag(PRRequestFlags::PRPP_FLAGS_HDR_DYNAMIC);
  load_info_list.insert({request_info5->url().spec(),request_info5});
  EXPECT_EQ(load_info_list.size(), 5);

  GURL request_url6("preflight+https://example.com/path/to/resource6");
  std::shared_ptr<PRRequestInfo> request_info6 = std::make_shared<PRRequestInfo>(request_url6, false);
  request_info6->set_request_end_time(100);
  load_info_list.insert({request_info6->url().spec(),request_info6});
  EXPECT_EQ(load_info_list.size(), 6);

  updater.OnResRequestInfoCacheLoaded(load_info_list,preconnect_limit_info_list,networkAnonymizationKey,"test");
}

TEST_F(ResRequestInfoUpdaterTest, BuildPreloadTreeTest_001)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList& preconnect_info_list,
      const std::shared_ptr<PRPPReqInfoTreeNode>& preload_info_tree,
      const std::set<std::string>& need_record_header_urls){
      LOG(INFO) << "callback";
  });
  ResRequestInfoUpdater updater("www.test.com",networkAnonymizationKey,disk_cache_backend_factory,preload_infos_cb);
  EXPECT_NE(updater.res_req_info_cache_mgr_, nullptr);

  GURL request_url2("preflight+https://example.com/path/to/resource2");
  std::shared_ptr<PRRequestInfo> request_info2 = std::make_shared<PRRequestInfo>(request_url2, false);
  request_info2->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);

  std::shared_ptr<PRPPReqInfoTreeNode> cur_parent = std::make_shared<PRPPReqInfoTreeNode>();
  std::shared_ptr<PRPPReqInfoTreeNode> cur_first = std::make_shared<PRPPReqInfoTreeNode>();
  cur_first->req_info_ = request_info2;
  std::shared_ptr<PRPPReqInfoTreeNode> current = nullptr;

  GURL request_url3("preflight+https://example.com/path/to/resource3");
  std::shared_ptr<PRRequestInfo> request_info3 = std::make_shared<PRRequestInfo>(request_url3, false);
  request_info3->set_preload_seq_num("test");
  request_info3->set_request_end_time(100);
  LinkedHashMap load_info_list;
  load_info_list.insert({request_info3->url().spec(),request_info3});
}

TEST_F(ResRequestInfoUpdaterTest, BuildPreconnectListTest_001)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList& preconnect_info_list,
      const std::shared_ptr<PRPPReqInfoTreeNode>& preload_info_tree,
      const std::set<std::string>& need_record_header_urls){
      LOG(INFO) << "callback";
  });
  ResRequestInfoUpdater updater("www.test.com",networkAnonymizationKey,disk_cache_backend_factory,preload_infos_cb);
  EXPECT_NE(updater.res_req_info_cache_mgr_, nullptr);

  LinkedHashMap preconnect_limit_info_list;
  GURL request_url("https://example.com");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  request_info->set_method("GET");
  request_info->set_cache_type(PRRequestCacheType::FORCE_CACHE);
  request_info->set_preconnect_num(5);
  GURL origin_url;
  std::string key;
  ResReqInfoCacheMgr::GetPreconnectOriginUrl(request_info, origin_url);
  ResReqInfoCacheMgr::GetPreconnectLimitListKey(origin_url, request_info->allow_credentials(),key);
  preconnect_limit_info_list.insert({key,request_info});
  updater.preconnect_org_url_map_[key] = 1;
  updater.BuildPreconnectList(request_info, preconnect_limit_info_list, networkAnonymizationKey, "test");
}

TEST_F(ResRequestInfoUpdaterTest, BuildPreloadTreeTest_01)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList& preconnect_info_list,
      const std::shared_ptr<PRPPReqInfoTreeNode>& preload_info_tree,
      const std::set<std::string>& need_record_header_urls){
      LOG(INFO) << "callback";
  });
  ResRequestInfoUpdater updater("www.test.com",networkAnonymizationKey,disk_cache_backend_factory,preload_infos_cb);
  EXPECT_NE(updater.res_req_info_cache_mgr_, nullptr);

  std::shared_ptr<PRPPReqInfoTreeNode> current = std::make_shared<PRPPReqInfoTreeNode>();
  std::shared_ptr<PRPPReqInfoTreeNode> cur_parent = std::make_shared<PRPPReqInfoTreeNode>();
  std::shared_ptr<PRPPReqInfoTreeNode> cur_first = std::make_shared<PRPPReqInfoTreeNode>();

  GURL request_url("https://example.com");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  request_info->set_request_start_time(100000*2);
  request_info->set_preload_seq_num("test");
  request_info->set_preload_flag(PRRequestFlags::PRPP_FLAGS_HDR_DYNAMIC);
  current->req_info_ = request_info;

  GURL request_url2("preflight+https://example.com/path/to/resource2");
  std::shared_ptr<PRRequestInfo> request_info2 = std::make_shared<PRRequestInfo>(request_url2, false);
  cur_first->req_info_ = request_info2;

  EXPECT_EQ(current->req_info_->request_start_time(),100000*2);
  EXPECT_EQ(cur_first->req_info_->request_start_time()+100000,100000);
  int64_t end_time = 1;
  updater.BuildPreloadTree(request_info,current,cur_first,cur_parent,end_time,"test");
}

TEST_F(ResRequestInfoUpdaterTest, BuildPreloadTreeTest_02)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList& preconnect_info_list,
      const std::shared_ptr<PRPPReqInfoTreeNode>& preload_info_tree,
      const std::set<std::string>& need_record_header_urls){
      LOG(INFO) << "callback";
  });
  ResRequestInfoUpdater updater("www.test.com",networkAnonymizationKey,disk_cache_backend_factory,preload_infos_cb);
  EXPECT_NE(updater.res_req_info_cache_mgr_, nullptr);

  std::shared_ptr<PRPPReqInfoTreeNode> current = std::make_shared<PRPPReqInfoTreeNode>();
  std::shared_ptr<PRPPReqInfoTreeNode> cur_parent = std::make_shared<PRPPReqInfoTreeNode>();
  std::shared_ptr<PRPPReqInfoTreeNode> cur_first = std::make_shared<PRPPReqInfoTreeNode>();

  GURL request_url("https://example.com");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  request_info->set_request_start_time(0);
  request_info->set_preload_seq_num("test");
  request_info->set_request_end_time(10);
  current->req_info_ = request_info;

  GURL request_url2("preflight+https://example.com/path/to/resource2");
  std::shared_ptr<PRRequestInfo> request_info2 = std::make_shared<PRRequestInfo>(request_url2, false);
  cur_first->req_info_ = request_info2;
  int64_t end_time = 0;
  updater.BuildPreloadTree(request_info,current,cur_first,cur_parent,end_time,"test");
}

TEST_F(ResRequestInfoUpdaterTest, UpdateResRequestInfoForDynamicHeaders_001)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList& preconnect_info_list,
      const std::shared_ptr<PRPPReqInfoTreeNode>& preload_info_tree,
      const std::set<std::string>& need_record_header_urls){
      LOG(INFO) << "callback";
  });
  ResRequestInfoUpdater updater("www.test.com",networkAnonymizationKey,disk_cache_backend_factory,preload_infos_cb);
  EXPECT_NE(updater.res_req_info_cache_mgr_, nullptr);

  std::shared_ptr<PRPPReqInfoTreeNode> parent = std::make_shared<PRPPReqInfoTreeNode>();
  std::shared_ptr<PRPPReqInfoTreeNode> grandPa = std::make_shared<PRPPReqInfoTreeNode>();

  std::list<std::shared_ptr<PRPPReqInfoTreeNode>> children;
  auto child1 = std::make_shared<PRPPReqInfoTreeNode>();
  auto child2 = std::make_shared<PRPPReqInfoTreeNode>();
  auto child3 = std::make_shared<PRPPReqInfoTreeNode>();

  GURL request_url("https://example.com");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  request_info->set_cache_type(PRRequestCacheType::FORCE_CACHE);
  child1->req_info_ = request_info;

  GURL request_url2("https://example.com");
  std::shared_ptr<PRRequestInfo> request_info2 = std::make_shared<PRRequestInfo>(request_url2, false);
  child2->req_info_ = request_info2;

  GURL request_url3("https://example.com");
  std::shared_ptr<PRRequestInfo> request_info3 = std::make_shared<PRRequestInfo>(request_url3, false);
  child3->req_info_ = request_info3;

  children.push_back(child1);
  children.push_back(child2);
  children.push_back(child3);

  grandPa->children_ = children;
  parent->parent_ = grandPa.get();

  GURL request_url1("https://example.com");
  std::shared_ptr<PRRequestInfo> child_info = std::make_shared<PRRequestInfo>(request_url1, false);
  std::set<std::string> dynamic_header_keys;
  dynamic_header_keys.insert("hello");
  child_info->set_dynamic_header_keys(dynamic_header_keys);

  updater.UpdateResRequestInfoForDynamicHeaders(parent,child_info);
}

TEST_F(ResRequestInfoUpdaterTest, ResRequestInfoUpdaterTest_002)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory = nullptr;
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList& preconnect_info_list,
      const std::shared_ptr<PRPPReqInfoTreeNode>& preload_info_tree,
      const std::set<std::string>& need_record_header_urls){
      LOG(INFO) << "callback";
  });
  ResRequestInfoUpdater updater("www.test.com",networkAnonymizationKey,disk_cache_backend_factory,preload_infos_cb);
  EXPECT_NE(updater.res_req_info_cache_mgr_, nullptr);
  updater.Start();
  GURL request_url("preflight+https://example.com/path/to/resource");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  updater.UpdateResRequestInfo(request_info);
  updater.Stop();
  updater.SetPageOrigin("test");
}

TEST_F(ResRequestInfoUpdaterTest, OnResRequestInfoCacheLoadedTest_003)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList& preconnect_info_list,
      const std::shared_ptr<PRPPReqInfoTreeNode>& preload_info_tree,
      const std::set<std::string>& need_record_header_urls){
      LOG(INFO) << "callback";
  });
  ResRequestInfoUpdater updater("www.test.com",networkAnonymizationKey,disk_cache_backend_factory,preload_infos_cb);
  EXPECT_NE(updater.res_req_info_cache_mgr_, nullptr);

  std::shared_ptr<PRPPReqInfoTreeNode> existing_tree = std::make_shared<PRPPReqInfoTreeNode>();
  GURL origin_url("https://example.com");
  std::shared_ptr<PRRequestInfo> origin_info = std::make_shared<PRRequestInfo>(origin_url, false);
  origin_info->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
  existing_tree->req_info_ = origin_info;
  updater.preload_info_tree_ = existing_tree;

  LinkedHashMap load_info_list;
  std::shared_ptr<PRRequestInfo> second_origin = std::make_shared<PRRequestInfo>(origin_url, false);
  second_origin->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
  load_info_list.insert({origin_url.spec(), second_origin});

  LinkedHashMap preconnect_limit_info_list;
  updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, networkAnonymizationKey, "1, ");
}

TEST_F(ResRequestInfoUpdaterTest, BuildPreconnectListTest_002)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList& preconnect_info_list,
      const std::shared_ptr<PRPPReqInfoTreeNode>& preload_info_tree,
      const std::set<std::string>& need_record_header_urls){
      LOG(INFO) << "callback";
  });
  ResRequestInfoUpdater updater("www.test.com",networkAnonymizationKey,disk_cache_backend_factory,preload_infos_cb);
  EXPECT_NE(updater.res_req_info_cache_mgr_, nullptr);

  LinkedHashMap preconnect_limit_info_list;
  GURL request_url("https://example.com");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  request_info->set_method("POST");
  request_info->set_cache_type(PRRequestCacheType::FORCE_CACHE);
  GURL origin_url;
  std::string key;
  ResReqInfoCacheMgr::GetPreconnectOriginUrl(request_info, origin_url);
  ResReqInfoCacheMgr::GetPreconnectLimitListKey(origin_url, request_info->allow_credentials(),key);
  updater.preconnect_org_url_map_[key] = 1;
  updater.BuildPreconnectList(request_info, preconnect_limit_info_list, networkAnonymizationKey, "test");
}

TEST_F(ResRequestInfoUpdaterTest, BuildPreconnectListTest_003)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList& preconnect_info_list,
      const std::shared_ptr<PRPPReqInfoTreeNode>& preload_info_tree,
      const std::set<std::string>& need_record_header_urls){
      LOG(INFO) << "callback";
  });
  ResRequestInfoUpdater updater("www.test.com",networkAnonymizationKey,disk_cache_backend_factory,preload_infos_cb);
  EXPECT_NE(updater.res_req_info_cache_mgr_, nullptr);

  LinkedHashMap preconnect_limit_info_list;
  GURL request_url("https://example.com");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  request_info->set_method("GET");
  request_info->set_cache_type(PRRequestCacheType::FORCE_CACHE);
  GURL origin_url;
  std::string key;
  ResReqInfoCacheMgr::GetPreconnectOriginUrl(request_info, origin_url);
  ResReqInfoCacheMgr::GetPreconnectLimitListKey(origin_url, request_info->allow_credentials(),key);
  updater.BuildPreconnectList(request_info, preconnect_limit_info_list, networkAnonymizationKey, "test");
}

TEST_F(ResRequestInfoUpdaterTest, BuildPreconnectListTest_004)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList& preconnect_info_list,
      const std::shared_ptr<PRPPReqInfoTreeNode>& preload_info_tree,
      const std::set<std::string>& need_record_header_urls){
      LOG(INFO) << "callback";
  });
  ResRequestInfoUpdater updater("www.test.com",networkAnonymizationKey,disk_cache_backend_factory,preload_infos_cb);
  EXPECT_NE(updater.res_req_info_cache_mgr_, nullptr);

  LinkedHashMap preconnect_limit_info_list;
  GURL request_url("https://example.com");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  request_info->set_method("GET");
  request_info->set_cache_type(PRRequestCacheType::FORCE_CACHE);
  request_info->set_freshness_life_times(0);
  GURL origin_url;
  std::string key;
  ResReqInfoCacheMgr::GetPreconnectOriginUrl(request_info, origin_url);
  ResReqInfoCacheMgr::GetPreconnectLimitListKey(origin_url, request_info->allow_credentials(),key);
  updater.BuildPreconnectList(request_info, preconnect_limit_info_list, networkAnonymizationKey, "test");
}

TEST_F(ResRequestInfoUpdaterTest, BuildPreloadTreeTest_SeqNumNotMatch)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
       const std::set<std::string>&){});
  ResRequestInfoUpdater updater("www.test.com", networkAnonymizationKey,
    disk_cache_backend_factory, preload_infos_cb);
  EXPECT_NE(updater.res_req_info_cache_mgr_, nullptr);

  std::shared_ptr<PRPPReqInfoTreeNode> cur_parent = std::make_shared<PRPPReqInfoTreeNode>();
  std::shared_ptr<PRPPReqInfoTreeNode> cur_first = nullptr;
  std::shared_ptr<PRPPReqInfoTreeNode> current = nullptr;

  GURL request_url("https://example.com/res1");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  request_info->set_preload_seq_num("seq99");
  request_info->set_cache_type(PRRequestCacheType::FORCE_CACHE);
  request_info->set_freshness_life_times(base::Time::Now().ToInternalValue() + 1000000000);

  int64_t cur_level_end_time = 0;
  size_t children_before = cur_parent->children_.size();
  updater.BuildPreloadTree(request_info, current, cur_first, cur_parent, cur_level_end_time, "seq42");
  EXPECT_EQ(cur_parent->children_.size(), children_before);
}

TEST_F(ResRequestInfoUpdaterTest, BuildPreloadTreeTest_CacheTypeNotForceCache)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
       const std::set<std::string>&){});
  ResRequestInfoUpdater updater("www.test.com", networkAnonymizationKey,
    disk_cache_backend_factory, preload_infos_cb);

  std::shared_ptr<PRPPReqInfoTreeNode> cur_parent = std::make_shared<PRPPReqInfoTreeNode>();
  std::shared_ptr<PRPPReqInfoTreeNode> cur_first = nullptr;
  std::shared_ptr<PRPPReqInfoTreeNode> current = nullptr;

  GURL request_url("https://example.com/res1");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  request_info->set_preload_seq_num("seq");
  request_info->set_cache_type(PRRequestCacheType::NEGOTIATION_CACHE);
  request_info->set_freshness_life_times(base::Time::Now().ToInternalValue() + 1000000000);

  int64_t cur_level_end_time = 0;
  size_t children_before = cur_parent->children_.size();
  updater.BuildPreloadTree(request_info, current, cur_first, cur_parent, cur_level_end_time, "seq");
  EXPECT_EQ(cur_parent->children_.size(), children_before);
}

TEST_F(ResRequestInfoUpdaterTest, BuildPreloadTreeTest_FreshnessExpired)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
       const std::set<std::string>&){});
  ResRequestInfoUpdater updater("www.test.com", networkAnonymizationKey,
    disk_cache_backend_factory, preload_infos_cb);

  std::shared_ptr<PRPPReqInfoTreeNode> cur_parent = std::make_shared<PRPPReqInfoTreeNode>();
  std::shared_ptr<PRPPReqInfoTreeNode> cur_first = nullptr;
  std::shared_ptr<PRPPReqInfoTreeNode> current = nullptr;

  GURL request_url("https://example.com/res1");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  request_info->set_preload_seq_num("seq");
  request_info->set_cache_type(PRRequestCacheType::FORCE_CACHE);
  request_info->set_freshness_life_times(0);

  int64_t cur_level_end_time = 0;
  size_t children_before = cur_parent->children_.size();
  updater.BuildPreloadTree(request_info, current, cur_first, cur_parent, cur_level_end_time, "seq");
  EXPECT_EQ(cur_parent->children_.size(), children_before);
}

TEST_F(ResRequestInfoUpdaterTest, BuildPreconnectListTest_CountReachLimit)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
       const std::set<std::string>&){});
  ResRequestInfoUpdater updater("www.test.com", networkAnonymizationKey,
    disk_cache_backend_factory, preload_infos_cb);

  LinkedHashMap preconnect_limit_info_list;
  GURL request_url("https://example.com");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  request_info->set_method("GET");
  request_info->set_cache_type(PRRequestCacheType::FORCE_CACHE);
  request_info->set_freshness_life_times(0);

  GURL origin_url;
  std::string key;
  ResReqInfoCacheMgr::GetPreconnectOriginUrl(request_info, origin_url);
  ResReqInfoCacheMgr::GetPreconnectLimitListKey(origin_url, request_info->allow_credentials(), key);

  request_info->set_preconnect_num(2);
  preconnect_limit_info_list.insert({key, request_info});
  updater.preconnect_org_url_map_[key] = 2;

  size_t list_size_before = updater.prpp_preconnect_info_list_.size();
  updater.BuildPreconnectList(request_info, preconnect_limit_info_list, networkAnonymizationKey, "test");
  EXPECT_EQ(updater.prpp_preconnect_info_list_.size(), list_size_before);
}

TEST_F(ResRequestInfoUpdaterTest, BuildPreconnectListTest_NoLimitEntry)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
       const std::set<std::string>&){});
  ResRequestInfoUpdater updater("www.test.com", networkAnonymizationKey,
    disk_cache_backend_factory, preload_infos_cb);

  LinkedHashMap preconnect_limit_info_list;
  GURL request_url("https://no-limit-example.com");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  request_info->set_method("POST");

  size_t list_size_before = updater.prpp_preconnect_info_list_.size();
  updater.BuildPreconnectList(request_info, preconnect_limit_info_list, networkAnonymizationKey, "test");
  EXPECT_EQ(updater.prpp_preconnect_info_list_.size(), list_size_before + 1);
}

TEST_F(ResRequestInfoUpdaterTest, BuildPreconnectListTest_NegotiationCache)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
       const std::set<std::string>&){});
  ResRequestInfoUpdater updater("www.test.com", networkAnonymizationKey,
    disk_cache_backend_factory, preload_infos_cb);

  LinkedHashMap preconnect_limit_info_list;
  GURL request_url("https://negotiation-example.com");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  request_info->set_method("GET");
  request_info->set_cache_type(PRRequestCacheType::NEGOTIATION_CACHE);
  request_info->set_freshness_life_times(base::Time::Now().ToInternalValue() + 1000000000);

  size_t list_size_before = updater.prpp_preconnect_info_list_.size();
  updater.BuildPreconnectList(request_info, preconnect_limit_info_list, networkAnonymizationKey, "test");
  EXPECT_EQ(updater.prpp_preconnect_info_list_.size(), list_size_before + 1);
}

TEST_F(ResRequestInfoUpdaterTest, BuildPreconnectListTest_VerifyResult)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
       const std::set<std::string>&){});
  ResRequestInfoUpdater updater("www.test.com", networkAnonymizationKey,
    disk_cache_backend_factory, preload_infos_cb);

  LinkedHashMap preconnect_limit_info_list;
  GURL request_url("https://verify-example.com");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, true);
  request_info->set_method("POST");

  updater.BuildPreconnectList(request_info, preconnect_limit_info_list, networkAnonymizationKey, "test");

  ASSERT_EQ(updater.prpp_preconnect_info_list_.size(), 1u);
  const auto& info = updater.prpp_preconnect_info_list_.front();
  GURL expected_origin = url::Origin::Create(request_url).GetURL();
  EXPECT_EQ(info.origin_url_, expected_origin);
  EXPECT_EQ(info.allow_credential_, true);
}

TEST_F(ResRequestInfoUpdaterTest, BuildPreloadTreeTest_FirstNode)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
       const std::set<std::string>&){});
  ResRequestInfoUpdater updater("www.test.com", networkAnonymizationKey,
    disk_cache_backend_factory, preload_infos_cb);
  EXPECT_NE(updater.res_req_info_cache_mgr_, nullptr);

  std::shared_ptr<PRPPReqInfoTreeNode> cur_parent = std::make_shared<PRPPReqInfoTreeNode>();
  std::shared_ptr<PRPPReqInfoTreeNode> cur_first = nullptr;
  std::shared_ptr<PRPPReqInfoTreeNode> current = nullptr;

  GURL request_url("https://example.com/res1");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  request_info->set_preload_seq_num("1, ");
  request_info->set_cache_type(PRRequestCacheType::FORCE_CACHE);
  request_info->set_freshness_life_times(base::Time::Now().ToInternalValue() + 1000000000);
  request_info->set_request_start_time(100);
  request_info->set_request_end_time(200);

  int64_t cur_level_end_time = 0;
  updater.BuildPreloadTree(request_info, current, cur_first, cur_parent, cur_level_end_time, "1, ");
  EXPECT_NE(cur_first, nullptr);
  EXPECT_EQ(cur_parent->children_.size(), 1u);
  EXPECT_EQ(cur_level_end_time, 200);
}

TEST_F(ResRequestInfoUpdaterTest, BuildPreloadTreeTest_DisableCacheEarlyReturn)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
       const std::set<std::string>&){});
  ResRequestInfoUpdater updater("www.test.com", networkAnonymizationKey,
    disk_cache_backend_factory, preload_infos_cb);

  std::shared_ptr<PRPPReqInfoTreeNode> cur_parent = std::make_shared<PRPPReqInfoTreeNode>();
  std::shared_ptr<PRPPReqInfoTreeNode> cur_first = nullptr;
  std::shared_ptr<PRPPReqInfoTreeNode> current = nullptr;

  GURL request_url("https://example.com/res1");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  request_info->set_preload_seq_num("1, ");
  request_info->set_cache_type(PRRequestCacheType::DISABLE_CACHE);
  request_info->set_freshness_life_times(base::Time::Now().ToInternalValue() + 1000000000);

  int64_t cur_level_end_time = 0;
  size_t children_before = cur_parent->children_.size();
  updater.BuildPreloadTree(request_info, current, cur_first, cur_parent, cur_level_end_time, "1, ");
  EXPECT_EQ(cur_parent->children_.size(), children_before);
}

TEST_F(ResRequestInfoUpdaterTest, BuildPreconnectListTest_DisableCache)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
       const std::set<std::string>&){});
  ResRequestInfoUpdater updater("www.test.com", networkAnonymizationKey,
    disk_cache_backend_factory, preload_infos_cb);

  LinkedHashMap preconnect_limit_info_list;
  GURL request_url("https://example.com");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  request_info->set_method("GET");
  request_info->set_cache_type(PRRequestCacheType::DISABLE_CACHE);

  size_t list_size_before = updater.prpp_preconnect_info_list_.size();
  updater.BuildPreconnectList(request_info, preconnect_limit_info_list, networkAnonymizationKey, "test");
  EXPECT_EQ(updater.prpp_preconnect_info_list_.size(), list_size_before + 1);
}

TEST_F(ResRequestInfoUpdaterTest, BuildPreconnectListTest_AllowCredential)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
       const std::set<std::string>&){});
  ResRequestInfoUpdater updater("www.test.com", networkAnonymizationKey,
    disk_cache_backend_factory, preload_infos_cb);

  LinkedHashMap preconnect_limit_info_list;
  GURL request_url("https://example.com");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, true);
  request_info->set_method("POST");

  updater.BuildPreconnectList(request_info, preconnect_limit_info_list, networkAnonymizationKey, "test");
  ASSERT_EQ(updater.prpp_preconnect_info_list_.size(), 1u);
  EXPECT_TRUE(updater.prpp_preconnect_info_list_.front().allow_credential_);
}

TEST_F(ResRequestInfoUpdaterTest, UpdateResRequestInfoForDynamicHeaders_002)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
       const std::set<std::string>&){});
  ResRequestInfoUpdater updater("www.test.com", networkAnonymizationKey,
    disk_cache_backend_factory, preload_infos_cb);

  std::shared_ptr<PRPPReqInfoTreeNode> parent = std::make_shared<PRPPReqInfoTreeNode>();
  std::shared_ptr<PRPPReqInfoTreeNode> grandPa = std::make_shared<PRPPReqInfoTreeNode>();

  auto sibling = std::make_shared<PRPPReqInfoTreeNode>();
  GURL sibling_url("https://example.com/sibling");
  sibling->req_info_ = std::make_shared<PRRequestInfo>(sibling_url, false);
  sibling->req_info_->set_cache_type(PRRequestCacheType::NEGOTIATION_CACHE);
  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Custom", "value1");
  sibling->req_info_->set_extra_request_headers(headers);

  grandPa->children_.push_back(sibling);
  parent->parent_ = grandPa.get();

  GURL request_url("https://example.com");
  std::shared_ptr<PRRequestInfo> child_info = std::make_shared<PRRequestInfo>(request_url, false);
  std::set<std::string> keys;
  keys.insert("X-Custom");
  child_info->set_dynamic_header_keys(keys);

  updater.UpdateResRequestInfoForDynamicHeaders(parent, child_info);
  EXPECT_EQ(child_info->parent_for_dynamic_header(), sibling_url);
}

TEST_F(ResRequestInfoUpdaterTest, UpdateResRequestInfoForDynamicHeaders_003)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
       const std::set<std::string>&){});
  ResRequestInfoUpdater updater("www.test.com", networkAnonymizationKey,
    disk_cache_backend_factory, preload_infos_cb);

  std::shared_ptr<PRPPReqInfoTreeNode> parent = std::make_shared<PRPPReqInfoTreeNode>();
  std::shared_ptr<PRPPReqInfoTreeNode> grandPa = std::make_shared<PRPPReqInfoTreeNode>();

  auto sibling = std::make_shared<PRPPReqInfoTreeNode>();
  GURL sibling_url("https://example.com/sibling");
  sibling->req_info_ = std::make_shared<PRRequestInfo>(sibling_url, false);
  sibling->req_info_->set_cache_type(PRRequestCacheType::NEGOTIATION_CACHE);
  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Other", "value2");
  sibling->req_info_->set_extra_request_headers(headers);

  grandPa->children_.push_back(sibling);
  parent->parent_ = grandPa.get();

  GURL request_url("https://example.com");
  std::shared_ptr<PRRequestInfo> child_info = std::make_shared<PRRequestInfo>(request_url, false);
  std::set<std::string> keys;
  keys.insert("X-Custom");
  child_info->set_dynamic_header_keys(keys);

  updater.UpdateResRequestInfoForDynamicHeaders(parent, child_info);
  EXPECT_TRUE(child_info->parent_for_dynamic_header().is_empty());
}

TEST_F(ResRequestInfoUpdaterTest, UpdateResRequestInfoForDynamicHeaders_004)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
       const std::set<std::string>&){});
  ResRequestInfoUpdater updater("www.test.com", networkAnonymizationKey,
    disk_cache_backend_factory, preload_infos_cb);

  std::shared_ptr<PRPPReqInfoTreeNode> parent = std::make_shared<PRPPReqInfoTreeNode>();
  std::shared_ptr<PRPPReqInfoTreeNode> grandPa = std::make_shared<PRPPReqInfoTreeNode>();

  auto sibling = std::make_shared<PRPPReqInfoTreeNode>();
  GURL sibling_url("https://example.com/sibling");
  sibling->req_info_ = std::make_shared<PRRequestInfo>(sibling_url, false);
  sibling->req_info_->set_cache_type(PRRequestCacheType::NEGOTIATION_CACHE);

  grandPa->children_.push_back(sibling);
  parent->parent_ = grandPa.get();

  GURL request_url("https://example.com");
  std::shared_ptr<PRRequestInfo> child_info = std::make_shared<PRRequestInfo>(request_url, false);

  updater.UpdateResRequestInfoForDynamicHeaders(parent, child_info);
  EXPECT_EQ(child_info->parent_for_dynamic_header(), sibling_url);
}

TEST_F(ResRequestInfoUpdaterTest, SetPageOriginTest)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
       const std::set<std::string>&){});
  ResRequestInfoUpdater updater("www.test.com", networkAnonymizationKey,
    disk_cache_backend_factory, preload_infos_cb);

  updater.SetPageOrigin("https://new-origin.com");
  updater.SetPageOrigin("");
}

TEST_F(ResRequestInfoUpdaterTest, OnResRequestInfoCacheLoadedTest_EmptyCallback)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb;
  ResRequestInfoUpdater updater("www.test.com", networkAnonymizationKey,
    disk_cache_backend_factory, preload_infos_cb);

  LinkedHashMap load_info_list;
  LinkedHashMap preconnect_limit_info_list;
  GURL request_url("https://example.com");
  std::shared_ptr<PRRequestInfo> request_info = std::make_shared<PRRequestInfo>(request_url, false);
  request_info->set_method("POST");
  load_info_list.insert({request_info->url().spec(), request_info});
  updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list,
    networkAnonymizationKey, "1, ");
}

TEST_F(ResRequestInfoUpdaterTest, OnResRequestInfoCacheLoadedTest_PageOriginType)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
       const std::set<std::string>&){});
  ResRequestInfoUpdater updater("www.test.com", networkAnonymizationKey,
    disk_cache_backend_factory, preload_infos_cb);

  LinkedHashMap load_info_list;
  LinkedHashMap preconnect_limit_info_list;
  GURL origin_url("https://example.com");
  std::shared_ptr<PRRequestInfo> origin_info = std::make_shared<PRRequestInfo>(origin_url, false);
  origin_info->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
  load_info_list.insert({origin_url.spec(), origin_info});

  GURL res_url("https://example.com/res");
  std::shared_ptr<PRRequestInfo> res_info = std::make_shared<PRRequestInfo>(res_url, false);
  res_info->set_method("POST");
  load_info_list.insert({res_url.spec(), res_info});

  updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list,
    networkAnonymizationKey, "1, ");
}

TEST_F(ResRequestInfoUpdaterTest, OnResRequestInfoCacheLoadedTest_PreflightType)
{
  net::NetworkAnonymizationKey networkAnonymizationKey = new net::NetworkAnonymizationKey();
  DiskCacheBackendFactory* factory = new DiskCacheBackendFactory();
  scoped_refptr<DiskCacheBackendFactory> disk_cache_backend_factory(factory);
  ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
    [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
       const std::set<std::string>&){});
  ResRequestInfoUpdater updater("www.test.com", networkAnonymizationKey,
    disk_cache_backend_factory, preload_infos_cb);

  LinkedHashMap load_info_list;
  LinkedHashMap preconnect_limit_info_list;
  GURL preflight_url("preflight+https://example.com/api");
  std::shared_ptr<PRRequestInfo> preflight_info = std::make_shared<PRRequestInfo>(preflight_url, false);
  preflight_info->set_type(PRRequestInfoType::TYPE_PAGE_PREFLIGHT);
  preflight_info->set_method("POST");
  load_info_list.insert({preflight_url.spec(), preflight_info});

  updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list,
  networkAnonymizationKey, "1, ");
}

} // namespace ohos_prp_preload
