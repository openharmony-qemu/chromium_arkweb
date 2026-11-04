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

#include "arkweb/chromium_ext/services/network/url_loader_factory_utils.h"

#include <memory>

#include "arkweb/build/features/features.h"
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#include "base/test/task_environment.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

class URLLoaderFactoryUtilsTest : public testing::Test {
 public:
  URLLoaderFactoryUtilsTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {}

  void TearDown() override {}

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(URLLoaderFactoryUtilsTest, ConstructorWithNullFactory) {
  URLLoaderFactoryUtils utils(nullptr);
  EXPECT_EQ(utils.urlLoaderFactory, nullptr);
}

TEST_F(URLLoaderFactoryUtilsTest, ConstructorSetsFactory) {
  URLLoaderFactory* raw_ptr = reinterpret_cast<URLLoaderFactory*>(0x1);
  URLLoaderFactoryUtils utils(raw_ptr);
  EXPECT_EQ(utils.urlLoaderFactory, raw_ptr);
}

#if BUILDFLAG(ARKWEB_PRP_PRELOAD)
TEST_F(URLLoaderFactoryUtilsTest, PreloadRequestInit_EmptyMainUrl) {
  URLLoaderFactoryUtils utils(nullptr);
  std::shared_ptr<ohos_prp_preload::PRPPRequestLoader> prpp_loader;
  std::shared_ptr<ohos_prp_preload::PRRequestInfo> preload_info;

  ResourceRequest resource_request;
  resource_request.main_url = GURL();
  EXPECT_NO_FATAL_FAILURE(
      utils.PreloadRequestInit(resource_request, prpp_loader, preload_info));
  // prpp_loader should remain nullptr when main_url is empty
  EXPECT_EQ(prpp_loader, nullptr);
}

TEST_F(URLLoaderFactoryUtilsTest,
       PreloadRequestInit_NonEmptyMainUrlNotPreloadMode) {
  URLLoaderFactoryUtils utils(nullptr);
  std::shared_ptr<ohos_prp_preload::PRPPRequestLoader> prpp_loader;
  std::shared_ptr<ohos_prp_preload::PRRequestInfo> preload_info;

  ResourceRequest resource_request;
  resource_request.main_url = GURL("https://example.com");
  // Default mode is not PRELOAD, so prpp_loader should remain nullptr
  EXPECT_NO_FATAL_FAILURE(
      utils.PreloadRequestInit(resource_request, prpp_loader, preload_info));
}

TEST_F(URLLoaderFactoryUtilsTest, PreloadRequestInit_WeakFactoryNull) {
  URLLoaderFactoryUtils utils(nullptr);
  std::shared_ptr<ohos_prp_preload::PRPPRequestLoader> prpp_loader;
  std::shared_ptr<ohos_prp_preload::PRRequestInfo> preload_info;

  ResourceRequest resource_request;
  resource_request.main_url = GURL("https://example.com");
  // weak_prpp_req_loader_fac_ is default-constructed (null weak ptr)
  EXPECT_NO_FATAL_FAILURE(
      utils.PreloadRequestInit(resource_request, prpp_loader, preload_info));
}
#endif  // BUILDFLAG(ARKWEB_PRP_PRELOAD)

}  // namespace network
