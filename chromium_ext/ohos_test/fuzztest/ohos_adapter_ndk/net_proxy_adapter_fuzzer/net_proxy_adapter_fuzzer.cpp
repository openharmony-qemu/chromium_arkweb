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

#include "net_proxy_adapter.h"

#include <fuzzer/FuzzedDataProvider.h>

#include "net_proxy_adapter_impl/net_proxy_adapter_impl.h"
#include "net_proxy_adapter_fuzz_mock.h"
#include "arkweb/ohos_adapter_ndk/mock_ndk_api/include/mock_ndk_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;
using namespace MockNdkApi;

namespace OHOS::NWeb {
bool LLVMFuzzerTestSetup() {
  return true;
}

class NetProxyEventCallbackAdapterMock : public NetProxyEventCallbackAdapter {
 public:
  NetProxyEventCallbackAdapterMock() = default;
  ~NetProxyEventCallbackAdapterMock() override = default;

  MOCK_METHOD(void,
              Changed,
              (const std::string& host,
               const uint16_t& port,
               const std::string& pacUrl,
               const std::vector<std::string>& exclusionList),
              (override));
};

namespace Base64 {
  extern std::string Encode(const std::string& source);
  extern std::string Decode(const std::string& source);
}

void FuzzOnReceiveEvent(FuzzedDataProvider* fdp)
{
    int testCase = fdp->ConsumeIntegralInRange<int>(0, 4);
    NetProxyAdapterImpl& ins = NetProxyAdapterImpl::GetInstance();
    std::shared_ptr<NiceMock<NetProxyEventCallbackAdapterMock>> eventCallback =
        std::make_shared<NiceMock<NetProxyEventCallbackAdapterMock>>();
    ins.RegNetProxyEvent(eventCallback);

    switch (testCase) {
        case 0: {
            CommonEvent_RcvData data;
            g_mock_OH_CommonEvent_GetEventFromRcvData = [](const CommonEvent_RcvData*) {
                return COMMON_EVENT_PACKAGE_ADDED;
            };
            NetProxyAdapterImpl::OnReceiveEvent(&data);
            g_mock_OH_CommonEvent_GetEventFromRcvData = nullptr;
            break;
        }
        case 1: {
            CommonEvent_RcvData data;
            g_mock_OH_CommonEvent_GetEventFromRcvData = [](const CommonEvent_RcvData*) {
                return COMMON_EVENT_HTTP_PROXY_CHANGE;
            };
            g_mock_OH_NetConn_GetDefaultHttpProxy = [](NetConn_HttpProxy *httpProxy) {
                return 1;
            };
            NetProxyAdapterImpl::OnReceiveEvent(&data);
            g_mock_OH_CommonEvent_GetEventFromRcvData = nullptr;
            g_mock_OH_NetConn_GetDefaultHttpProxy = nullptr;
            break;
        }
        case 2: {
            CommonEvent_RcvData data;
            g_mock_OH_CommonEvent_GetEventFromRcvData = [](const CommonEvent_RcvData*) {
                return COMMON_EVENT_HTTP_PROXY_CHANGE;
            };
            g_mock_OH_NetConn_GetDefaultHttpProxy = [](NetConn_HttpProxy *httpProxy) {
                httpProxy->host[0] = 'N';
                httpProxy->host[1] = 'O';
                httpProxy->host[2] = 'N';
                httpProxy->host[3] = 'E';
                httpProxy->host[4] = 0;
                httpProxy->exclusionListSize = 1;
                httpProxy->exclusionList[0][0] = 'N';
                httpProxy->exclusionList[0][1] = 'O';
                httpProxy->exclusionList[0][2] = 'N';
                httpProxy->exclusionList[0][3] = 'E';
                httpProxy->exclusionList[0][4] = 0;
                return 0;
            };
            NetProxyAdapterImpl::OnReceiveEvent(&data);
            g_mock_OH_CommonEvent_GetEventFromRcvData = nullptr;
            g_mock_OH_NetConn_GetDefaultHttpProxy = nullptr;
            break;
        }
        case 3: {
            CommonEvent_RcvData data;
            ins.RegNetProxyEvent(nullptr);
            g_mock_OH_CommonEvent_GetEventFromRcvData = [](const CommonEvent_RcvData*) {
                return COMMON_EVENT_HTTP_PROXY_CHANGE;
            };
            g_mock_OH_NetConn_GetDefaultHttpProxy = [](NetConn_HttpProxy *httpProxy) {
                httpProxy->host[0] = 'N';
                httpProxy->host[1] = 'O';
                httpProxy->host[2] = 'N';
                httpProxy->host[3] = 'E';
                httpProxy->host[4] = 0;
                httpProxy->exclusionListSize = 1;
                httpProxy->exclusionList[0][0] = 'N';
                httpProxy->exclusionList[0][1] = 'O';
                httpProxy->exclusionList[0][2] = 'N';
                httpProxy->exclusionList[0][3] = 'E';
                httpProxy->exclusionList[0][4] = 0;
                return 0;
            };
            NetProxyAdapterImpl::OnReceiveEvent(&data);
            g_mock_OH_CommonEvent_GetEventFromRcvData = nullptr;
            g_mock_OH_NetConn_GetDefaultHttpProxy = nullptr;
            ins.RegNetProxyEvent(eventCallback);
            break;
        }
        case 4: {
            CommonEvent_RcvData data;
            g_mock_OH_CommonEvent_GetEventFromRcvData = [](const CommonEvent_RcvData*) {
                return COMMON_EVENT_HTTP_PROXY_CHANGE;
            };
            g_mock_OH_NetConn_GetDefaultHttpProxy = [](NetConn_HttpProxy *httpProxy) {
                httpProxy->host[0] = 'p';
                httpProxy->host[1] = 'r';
                httpProxy->host[2] = 'o';
                httpProxy->host[3] = 'x';
                httpProxy->host[4] = 'y';
                httpProxy->host[5] = 0;
                httpProxy->port = 8080;
                httpProxy->exclusionListSize = 0;
                return 0;
            };
            NetProxyAdapterImpl::OnReceiveEvent(&data);
            g_mock_OH_CommonEvent_GetEventFromRcvData = nullptr;
            g_mock_OH_NetConn_GetDefaultHttpProxy = nullptr;
            break;
        }
        default:
            break;
    }
}

void FuzzAppProxyChange(FuzzedDataProvider* fdp)
{
    int testCase = fdp->ConsumeIntegralInRange<int>(0, 5);
    NetProxyAdapterImpl& ins = NetProxyAdapterImpl::GetInstance();
    std::shared_ptr<NiceMock<NetProxyEventCallbackAdapterMock>> eventCallback =
        std::make_shared<NiceMock<NetProxyEventCallbackAdapterMock>>();
    ins.RegNetProxyEvent(eventCallback);

    NetConn_HttpProxy receiveHttpProxy;
    switch (testCase) {
        case 0: {
            NetProxyAdapterImpl::AppProxyChange(nullptr);
            break;
        }
        case 1: {
            receiveHttpProxy.host[0] = 0;
            NetProxyAdapterImpl::AppProxyChange(&receiveHttpProxy);
            break;
        }
        case 2: {
            receiveHttpProxy.host[0] = 'N';
            receiveHttpProxy.host[1] = 'O';
            receiveHttpProxy.host[2] = 'N';
            receiveHttpProxy.host[3] = 'E';
            receiveHttpProxy.host[4] = 0;
            NetProxyAdapterImpl::AppProxyChange(&receiveHttpProxy);
            break;
        }
        case 3: {
            receiveHttpProxy.host[0] = 0;
            g_mock_OH_NetConn_GetDefaultHttpProxy = [](NetConn_HttpProxy *httpProxy) {
                return 1;
            };
            NetProxyAdapterImpl::AppProxyChange(&receiveHttpProxy);
            g_mock_OH_NetConn_GetDefaultHttpProxy = nullptr;
            break;
        }
        case 4: {
            receiveHttpProxy.host[0] = 0;
            g_mock_OH_NetConn_GetDefaultHttpProxy = [](NetConn_HttpProxy *httpProxy) {
                httpProxy->host[0] = 'N';
                httpProxy->host[1] = 'O';
                httpProxy->host[2] = 'N';
                httpProxy->host[3] = 'E';
                httpProxy->host[4] = 0;
                httpProxy->exclusionListSize = 1;
                httpProxy->exclusionList[0][0] = 'N';
                httpProxy->exclusionList[0][1] = 'O';
                httpProxy->exclusionList[0][2] = 'N';
                httpProxy->exclusionList[0][3] = 'E';
                httpProxy->exclusionList[0][4] = 0;
                return 0;
            };
            NetProxyAdapterImpl::AppProxyChange(&receiveHttpProxy);
            g_mock_OH_NetConn_GetDefaultHttpProxy = nullptr;
            break;
        }
        case 5: {
            ins.RegNetProxyEvent(nullptr);
            receiveHttpProxy.host[0] = 0;
            NetProxyAdapterImpl::AppProxyChange(&receiveHttpProxy);
            ins.RegNetProxyEvent(eventCallback);
            break;
        }
        default:
            break;
    }
}

void FuzzApi(const uint8_t* data, size_t size) {
  FuzzedDataProvider fdp(data, size);
  NetProxyAdapterImpl ins = NetProxyAdapterImpl::GetInstance();
  std::shared_ptr<NiceMock<NetProxyEventCallbackAdapterMock>> eventCallback =
      std::make_shared<NiceMock<NetProxyEventCallbackAdapterMock>>();
  ins.RegNetProxyEvent(eventCallback);
  ins.StartListen();
  ins.StopListen();
  std::string source = fdp.ConsumeRandomLengthString();
  Base64::Encode(source);
  Base64::Decode(source);

  FuzzOnReceiveEvent(&fdp);
  FuzzAppProxyChange(&fdp);
}
}  // namespace OHOS::NWeb

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if ((data == nullptr) || (size < sizeof(int32_t))) {
    return 0;
  }
  static bool init = OHOS::NWeb::LLVMFuzzerTestSetup();
  if (init) {
    OHOS::NWeb::FuzzApi(data, size);
  }
  return 0;
}