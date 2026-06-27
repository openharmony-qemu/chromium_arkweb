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
#include <string>
#include <vector>
#include <memory>

#include "ndk/arkweb_native_javascript_execute_callback.h"
#include "ndk/arkweb_native_web_message_callback.h"
#include "nweb_web_message.h"
#include "nweb_value.h"

namespace OHOS::NWeb {
bool LLVMFuzzerTestSetup() {
  return true;
}

void FuzzApi(const uint8_t* data, size_t size) {
  FuzzedDataProvider dataProvider(data, size);
  uint8_t op = dataProvider.ConsumeIntegralInRange<uint8_t>(0, 7);

  switch (op) {
    case 0: {
      // ArkWebNativeJavaScriptExecuteCallback with null result
      ArkWebNativeJavaScriptExecuteCallback cb(
          [](const std::vector<uint8_t>& data) {});
      cb.OnReceiveValue(nullptr);
      break;
    }
    case 1: {
      // ArkWebNativeJavaScriptExecuteCallback with null callback
      ArkWebNativeJavaScriptExecuteCallback cb;
      auto msg = std::make_shared<NWebMessage>();
      cb.OnReceiveValue(msg);
      break;
    }
    case 2: {
      // ArkWebNativeJavaScriptExecuteCallback::OnReceiveValue with STRING
      std::string fuzz_str = dataProvider.ConsumeRandomLengthString(256);
      ArkWebNativeJavaScriptExecuteCallback cb(
          [](const std::vector<uint8_t>& data) {
            (void)data;
          });
      auto msg = std::make_shared<NWebMessage>(fuzz_str);
      cb.OnReceiveValue(msg);
      break;
    }
    case 3: {
      // ArkWebNativeJavaScriptExecuteCallback::OnReceiveValue with BINARY
      // via NWebMessage(char*, len)
      std::string fuzz_bin = dataProvider.ConsumeRandomLengthString(256);
      ArkWebNativeJavaScriptExecuteCallback cb(
          [](const std::vector<uint8_t>& data) {
            (void)data;
          });
      auto msg =
          std::make_shared<NWebMessage>(fuzz_bin.data(), fuzz_bin.length());
      cb.OnReceiveValue(msg);
      break;
    }
    case 4: {
      // ArkWebNativeJavaScriptExecuteCallback::OnReceiveValue with BINARY
      // via SetBinary
      std::string fuzz_bin = dataProvider.ConsumeRandomLengthString(256);
      ArkWebNativeJavaScriptExecuteCallback cb(
          [](const std::vector<uint8_t>& data) {
            (void)data;
          });
      auto msg =
          std::make_shared<NWebMessage>(NWebValue::Type::BINARY);
      std::vector<uint8_t> bin_vec(fuzz_bin.begin(), fuzz_bin.end());
      msg->SetBinary(bin_vec);
      cb.OnReceiveValue(msg);
      break;
    }
    case 5: {
      // ArkWebNativeWebMessageCallback with null result
      ArkWebNativeWebMessageCallback cb(
          [](const ArkWeb_WebMessagePtr message) {});
      cb.OnReceiveValue(nullptr);
      break;
    }
    case 6: {
      // ArkWebNativeWebMessageCallback::OnReceiveValue with STRING
      std::string fuzz_str = dataProvider.ConsumeRandomLengthString(256);
      ArkWebNativeWebMessageCallback cb(
          [](const ArkWeb_WebMessagePtr message) {
            (void)message;
          });
      auto msg = std::make_shared<NWebMessage>(fuzz_str);
      cb.OnReceiveValue(msg);
      break;
    }
    case 7: {
      // ArkWebNativeWebMessageCallback::OnReceiveValue with BINARY
      std::string fuzz_bin = dataProvider.ConsumeRandomLengthString(256);
      ArkWebNativeWebMessageCallback cb(
          [](const ArkWeb_WebMessagePtr message) {
            (void)message;
          });
      auto msg =
          std::make_shared<NWebMessage>(NWebValue::Type::BINARY);
      std::vector<uint8_t> bin_vec(fuzz_bin.begin(), fuzz_bin.end());
      msg->SetBinary(bin_vec);
      cb.OnReceiveValue(msg);
      break;
    }
    default:
      break;
  }
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