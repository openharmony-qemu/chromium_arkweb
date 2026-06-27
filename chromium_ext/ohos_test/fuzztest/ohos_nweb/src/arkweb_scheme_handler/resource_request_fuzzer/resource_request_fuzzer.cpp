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

#include "ndk/scheme_handler/resource_request.h"
#include "ndk/scheme_handler/mock_objects.h"
#include "cef_request.h"

namespace OHOS::NWeb {
bool LLVMFuzzerTestSetup() {
  return true;
}

void FuzzApi(const uint8_t* data, size_t size) {
  FuzzedDataProvider dataProvider(data, size);
  uint8_t op = dataProvider.ConsumeIntegralInRange<uint8_t>(0, 10);

  switch (op) {
    case 0: {
      // GetFrameUrl with null cef_request
      ArkWeb_ResourceRequest_* rr_null = new ArkWeb_ResourceRequest_(CefRefPtr<CefRequest>());
      char* frame_url = nullptr;
      rr_null->GetFrameUrl(&frame_url);
      if (frame_url) free(frame_url);
      delete rr_null;
      break;
    }
    case 1: {
      // GetFrameUrl with MockCefRequest and fuzz-derived frame_url
      CefRefPtr<CefRequest> mock_req = new MockCefRequest(nullptr);
      auto ext = mock_req->AsArkWebRequestExt();
      auto* mock_ext = static_cast<MockArkWebRequestImplExt*>(ext.get());
      std::string fuzz_url = dataProvider.ConsumeRandomLengthString(256);
      mock_ext->SetFrameUrlString(fuzz_url);
      ArkWeb_ResourceRequest_* rr = new ArkWeb_ResourceRequest_(mock_req);
      char* frame_url = nullptr;
      rr->GetFrameUrl(&frame_url);
      if (frame_url) free(frame_url);
      delete rr;
      break;
    }
    case 2: {
      // ArkWeb_RequestHeaderList_ with null request
      ArkWeb_RequestHeaderList_* hl_null = new ArkWeb_RequestHeaderList_(nullptr);
      delete hl_null;
      break;
    }
    case 3: {
      // ArkWeb_RequestHeaderList_ with MockCefRequest and fuzz headers
      CefRefPtr<CefRequest> mock_req = new MockCefRequest(nullptr);
      CefRequest::HeaderMap headers;
      uint8_t num_headers = dataProvider.ConsumeIntegralInRange<uint8_t>(0, 10);
      for (uint8_t i = 0; i < num_headers; i++) {
        std::string key = dataProvider.ConsumeRandomLengthString(64);
        std::string value = dataProvider.ConsumeRandomLengthString(128);
        if (!key.empty()) {
          headers.insert(std::make_pair(key, value));
        }
      }
      mock_req->SetHeaderMap(headers);
      ArkWeb_ResourceRequest_* rr = new ArkWeb_ResourceRequest_(mock_req);
      ArkWeb_RequestHeaderList_* hl = new ArkWeb_RequestHeaderList_(
          reinterpret_cast<ArkWeb_ResourceRequest*>(rr));
      hl->GetSize();
      delete hl;
      delete rr;
      break;
    }
    case 4: {
      // GetHeader with fuzz index (in-bounds, out-of-bounds, negative)
      CefRefPtr<CefRequest> mock_req = new MockCefRequest(nullptr);
      CefRequest::HeaderMap headers;
      uint8_t num_headers = dataProvider.ConsumeIntegralInRange<uint8_t>(1, 10);
      for (uint8_t i = 0; i < num_headers; i++) {
        std::string key = dataProvider.ConsumeRandomLengthString(64);
        std::string value = dataProvider.ConsumeRandomLengthString(128);
        if (!key.empty()) {
          headers.insert(std::make_pair(key, value));
        }
      }
      mock_req->SetHeaderMap(headers);
      ArkWeb_ResourceRequest_* rr = new ArkWeb_ResourceRequest_(mock_req);
      ArkWeb_RequestHeaderList_* hl = new ArkWeb_RequestHeaderList_(
          reinterpret_cast<ArkWeb_ResourceRequest*>(rr));

      // Fuzz index (can be any int, including negative and out-of-bounds)
      int fuzz_index = dataProvider.ConsumeIntegral<int>();
      char* key = nullptr;
      char* value = nullptr;
      hl->GetHeader(fuzz_index, &key, &value);
      if (key) free(key);
      if (value) free(value);

      // Also test valid in-bounds index
      int32_t sz = hl->GetSize();
      if (sz > 0) {
        int valid_index = dataProvider.ConsumeIntegralInRange<int>(0, sz - 1);
        key = nullptr;
        value = nullptr;
        hl->GetHeader(valid_index, &key, &value);
        if (key) free(key);
        if (value) free(value);
      }

      delete hl;
      delete rr;
      break;
    }
    case 5: {
      // GetHttpBodyStream: null cef_request + null upload stream + valid
      // null cef_request path
      ArkWeb_ResourceRequest_* rr_null_hbs =
          new ArkWeb_ResourceRequest_(CefRefPtr<CefRequest>());
      ArkWeb_HttpBodyStream* stream_null_hbs = nullptr;
      rr_null_hbs->GetHttpBodyStream(&stream_null_hbs);
      delete rr_null_hbs;

      // null upload stream path (MockCefRequest with nullptr stream)
      CefRefPtr<CefRequest> mock_req_no_stream = new MockCefRequest(nullptr);
      ArkWeb_ResourceRequest_* rr_no_stream =
          new ArkWeb_ResourceRequest_(mock_req_no_stream);
      ArkWeb_HttpBodyStream* stream_no_upload = nullptr;
      rr_no_stream->GetHttpBodyStream(&stream_no_upload);
      delete rr_no_stream;

      // valid upload stream path
      auto* mock_stream = new MockPostDataStream();
      CefRefPtr<CefRequest> mock_req_stream = new MockCefRequest(mock_stream);
      ArkWeb_ResourceRequest_* rr_stream =
          new ArkWeb_ResourceRequest_(mock_req_stream);
      ArkWeb_HttpBodyStream* stream_valid = nullptr;
      rr_stream->GetHttpBodyStream(&stream_valid);
      if (stream_valid) {
        // Release the extra ref added by GetHttpBodyStream
        stream_valid->Release();
      }
      // Null out post_data_stream before deleting to avoid dangling
      // CefRefPtr (MockPostDataStream has no-op AddRef/Release)
      rr_stream->http_body_stream->post_data_stream = nullptr;
      delete rr_stream;
      delete mock_stream;
      break;
    }
    case 6: {
      // GetMethod: null cef_request + valid with fuzz method
      ArkWeb_ResourceRequest_* rr_null_method =
          new ArkWeb_ResourceRequest_(CefRefPtr<CefRequest>());
      char* method_null = nullptr;
      rr_null_method->GetMethod(&method_null);
      delete rr_null_method;

      // valid path with fuzz method string
      CefRefPtr<CefRequest> mock_req_method = new MockCefRequest(nullptr);
      auto* mock_ptr = static_cast<MockCefRequest*>(mock_req_method.get());
      std::string fuzz_method = dataProvider.ConsumeRandomLengthString(128);
      mock_ptr->SetMethodString(fuzz_method);
      ArkWeb_ResourceRequest_* rr_method =
          new ArkWeb_ResourceRequest_(mock_req_method);
      char* method = nullptr;
      rr_method->GetMethod(&method);
      if (method) free(method);
      delete rr_method;
      break;
    }
    case 7: {
      // GetUrl: null cef_request + valid
      ArkWeb_ResourceRequest_* rr_null_url =
          new ArkWeb_ResourceRequest_(CefRefPtr<CefRequest>());
      char* url_null = nullptr;
      rr_null_url->GetUrl(&url_null);
      delete rr_null_url;

      // valid path (MockCefRequest::GetURL returns empty CefString)
      CefRefPtr<CefRequest> mock_req_url = new MockCefRequest(nullptr);
      ArkWeb_ResourceRequest_* rr_url =
          new ArkWeb_ResourceRequest_(mock_req_url);
      char* url = nullptr;
      rr_url->GetUrl(&url);
      if (url) free(url);
      delete rr_url;
      break;
    }
    case 8: {
      // IsRedirect + IsMainFrame + HasUserGesture: null + valid
      ArkWeb_ResourceRequest_* rr_null_bool =
          new ArkWeb_ResourceRequest_(CefRefPtr<CefRequest>());
      rr_null_bool->IsRedirect();
      rr_null_bool->IsMainFrame();
      rr_null_bool->HasUserGesture();
      delete rr_null_bool;

      // valid path with fuzz-controlled flags
      CefRefPtr<CefRequest> mock_req_bool = new MockCefRequest(nullptr);
      auto ext = mock_req_bool->AsArkWebRequestExt();
      auto* mock_ext = static_cast<MockArkWebRequestImplExt*>(ext.get());
      bool fuzz_main_frame = dataProvider.ConsumeBool();
      bool fuzz_user_gesture = dataProvider.ConsumeBool();
      mock_ext->SetMainFrame(fuzz_main_frame);
      mock_ext->SetUserGesture(fuzz_user_gesture);
      ArkWeb_ResourceRequest_* rr_bool =
          new ArkWeb_ResourceRequest_(mock_req_bool);
      rr_bool->IsRedirect();
      rr_bool->IsMainFrame();
      rr_bool->HasUserGesture();
      delete rr_bool;
      break;
    }
    case 9: {
      // GetReferrer: null cef_request + valid with fuzz referrer
      ArkWeb_ResourceRequest_* rr_null_ref =
          new ArkWeb_ResourceRequest_(CefRefPtr<CefRequest>());
      char* referrer_null = nullptr;
      rr_null_ref->GetReferrer(&referrer_null);
      delete rr_null_ref;

      // valid path with fuzz referrer string
      CefRefPtr<CefRequest> mock_req_ref = new MockCefRequest(nullptr);
      auto* mock_ptr_ref = static_cast<MockCefRequest*>(mock_req_ref.get());
      std::string fuzz_referrer = dataProvider.ConsumeRandomLengthString(256);
      mock_ptr_ref->SetReferrerString(fuzz_referrer);
      ArkWeb_ResourceRequest_* rr_ref =
          new ArkWeb_ResourceRequest_(mock_req_ref);
      char* referrer = nullptr;
      rr_ref->GetReferrer(&referrer);
      if (referrer) free(referrer);
      delete rr_ref;
      break;
    }
    case 10: {
      // GetRequestResourceType: null cef_request + valid
      ArkWeb_ResourceRequest_* rr_null_rt =
          new ArkWeb_ResourceRequest_(CefRefPtr<CefRequest>());
      rr_null_rt->GetRequestResourceType();
      delete rr_null_rt;

      // valid path with fuzz resource type
      CefRefPtr<CefRequest> mock_req_rt = new MockCefRequest(nullptr);
      auto* mock_ptr_rt = static_cast<MockCefRequest*>(mock_req_rt.get());
      uint8_t fuzz_rt = dataProvider.ConsumeIntegralInRange<uint8_t>(0, 14);
      mock_ptr_rt->SetResourceType(
          static_cast<CefRequest::ResourceType>(fuzz_rt));
      ArkWeb_ResourceRequest_* rr_rt =
          new ArkWeb_ResourceRequest_(mock_req_rt);
      rr_rt->GetRequestResourceType();
      delete rr_rt;
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