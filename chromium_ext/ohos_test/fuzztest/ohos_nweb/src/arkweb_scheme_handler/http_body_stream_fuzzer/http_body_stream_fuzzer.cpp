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

#include "ndk/scheme_handler/http_body_stream.h"
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
      // Constructor path: null resource_request (post_data_stream stays null)
      CefRefPtr<ArkWeb_HttpBodyStream_> stream_null =
          new ArkWeb_HttpBodyStream_(nullptr);
      break;
    }
    case 1: {
      // Read paths
      // null post_data_stream path
      CefRefPtr<ArkWeb_HttpBodyStream_> stream_null =
          new ArkWeb_HttpBodyStream_(nullptr);
      int64_t fuzz_buf_len_1 = dataProvider.ConsumeIntegral<int64_t>();
      stream_null->Read(nullptr, fuzz_buf_len_1);

      // valid post_data_stream path
      CefRefPtr<ArkWeb_HttpBodyStream_> stream =
          new ArkWeb_HttpBodyStream_(nullptr);
      // MockPostDataStream has no-op AddRef/Release, so CefRefPtr assignment
      // is a no-op AddRef. We must NOT delete the mock while CefRefPtr
      // still holds a pointer to it. Clear the CefRefPtr before deleting.
      auto* mock_post_data = new MockPostDataStream();
      stream->post_data_stream = mock_post_data;

      // nullptr buffer path
      int64_t fuzz_buf_len_2 = dataProvider.ConsumeIntegral<int64_t>();
      stream->Read(nullptr, fuzz_buf_len_2);

      // negative buf_len path
      char buffer_neg[64];
      int64_t negative_buf_len =
          dataProvider.ConsumeIntegralInRange<int64_t>(-1000, -1);
      stream->Read(buffer_neg, negative_buf_len);

      // normal path
      char buffer_normal[64];
      int64_t positive_buf_len =
          dataProvider.ConsumeIntegralInRange<int64_t>(1, 64);
      stream->Read(buffer_normal, positive_buf_len);

      // Clear CefRefPtr before deleting mock (dtor calls Release which is
      // no-op on MockPostDataStream, but the pointer must still be valid)
      stream->post_data_stream = nullptr;
      delete mock_post_data;
      break;
    }
    case 2: {
      // AsyncRead paths
      CefRefPtr<ArkWeb_HttpBodyStream_> stream_null =
          new ArkWeb_HttpBodyStream_(nullptr);
      char async_buffer_null[64];
      stream_null->AsyncRead(async_buffer_null, 64);

      // valid post_data_stream path
      CefRefPtr<ArkWeb_HttpBodyStream_> stream =
          new ArkWeb_HttpBodyStream_(nullptr);
      auto* mock_post_data = new MockPostDataStream();
      stream->post_data_stream = mock_post_data;

      // nullptr buffer path
      stream->AsyncRead(nullptr, 64);

      // normal path
      char async_buffer[64];
      int64_t async_buf_len =
          dataProvider.ConsumeIntegralInRange<int64_t>(1, 64);
      stream->AsyncRead(async_buffer, async_buf_len);

      stream->post_data_stream = nullptr;
      delete mock_post_data;
      break;
    }
    case 3: {
      // OnReadComplete paths (is_ets_ must stay false — CEF_POST_TASK
      // requires BrowserTaskEnvironment which is unavailable in fuzzer)
      CefRefPtr<ArkWeb_HttpBodyStream_> stream_null_cb =
          new ArkWeb_HttpBodyStream_(nullptr);
      auto* mock_post_data_3 = new MockPostDataStream();
      stream_null_cb->post_data_stream = mock_post_data_3;
      stream_null_cb->read_callback = nullptr;
      int fuzz_bytes_read_null = dataProvider.ConsumeIntegral<int>();
      stream_null_cb->OnReadComplete(nullptr, fuzz_bytes_read_null);

      // normal path: is_ets_ = false (direct callback invocation)
      CefRefPtr<ArkWeb_HttpBodyStream_> stream_normal =
          new ArkWeb_HttpBodyStream_(nullptr);
      auto* mock_post_data_3b = new MockPostDataStream();
      stream_normal->post_data_stream = mock_post_data_3b;
      ArkWeb_HttpBodyStreamReadCallback test_read_callback =
          [](const ArkWeb_HttpBodyStream* httpBodyStream, uint8_t* buffer,
             int bytesRead) {};
      stream_normal->read_callback = test_read_callback;
      stream_normal->is_ets_ = false;
      int fuzz_bytes_read_normal = dataProvider.ConsumeIntegral<int>();
      char on_read_buffer[64];
      stream_normal->OnReadComplete(on_read_buffer, fuzz_bytes_read_normal);

      stream_null_cb->post_data_stream = nullptr;
      stream_normal->post_data_stream = nullptr;
      delete mock_post_data_3;
      delete mock_post_data_3b;
      break;
    }
    case 4: {
      // OnAsyncReadComplete paths
      CefRefPtr<ArkWeb_HttpBodyStream_> stream_null_async_cb =
          new ArkWeb_HttpBodyStream_(nullptr);
      auto* mock_post_data_4 = new MockPostDataStream();
      stream_null_async_cb->post_data_stream = mock_post_data_4;
      stream_null_async_cb->read_async_callback = nullptr;
      int fuzz_bytes_read_async_null = dataProvider.ConsumeIntegral<int>();
      stream_null_async_cb->OnAsyncReadComplete(nullptr,
                                                 fuzz_bytes_read_async_null);

      // normal path
      CefRefPtr<ArkWeb_HttpBodyStream_> stream_async =
          new ArkWeb_HttpBodyStream_(nullptr);
      auto* mock_post_data_4b = new MockPostDataStream();
      stream_async->post_data_stream = mock_post_data_4b;
      ArkWeb_HttpBodyStreamAsyncReadCallback test_async_callback =
          [](const ArkWeb_HttpBodyStream* httpBodyStream, uint8_t* buffer,
             int bytesRead) {};
      stream_async->read_async_callback = test_async_callback;
      int fuzz_bytes_read_async = dataProvider.ConsumeIntegral<int>();
      char on_async_read_buffer[64];
      stream_async->OnAsyncReadComplete(on_async_read_buffer,
                                        fuzz_bytes_read_async);

      stream_null_async_cb->post_data_stream = nullptr;
      stream_async->post_data_stream = nullptr;
      delete mock_post_data_4;
      delete mock_post_data_4b;
      break;
    }
    case 5: {
      // SetReadCallback and SetAsyncReadCallback
      CefRefPtr<ArkWeb_HttpBodyStream_> stream =
          new ArkWeb_HttpBodyStream_(nullptr);
      ArkWeb_HttpBodyStreamReadCallback read_cb =
          [](const ArkWeb_HttpBodyStream* httpBodyStream, uint8_t* buffer,
             int bytesRead) {};
      stream->SetReadCallback(read_cb);

      ArkWeb_HttpBodyStreamAsyncReadCallback async_read_cb =
          [](const ArkWeb_HttpBodyStream* httpBodyStream, uint8_t* buffer,
             int bytesRead) {};
      stream->SetAsyncReadCallback(async_read_cb);
      break;
    }
    case 6: {
      // SetUserData (null + valid) and GetUserData
      CefRefPtr<ArkWeb_HttpBodyStream_> stream =
          new ArkWeb_HttpBodyStream_(nullptr);

      // null user_data path
      stream->SetUserData(nullptr);

      // valid user_data path
      int user_data_value = 42;
      stream->SetUserData(&user_data_value);

      // GetUserData
      void* retrieved = stream->GetUserData();
      (void)retrieved;
      break;
    }
    case 7: {
      // GetSize and GetPosition: null post_data_stream + valid
      CefRefPtr<ArkWeb_HttpBodyStream_> stream_null =
          new ArkWeb_HttpBodyStream_(nullptr);
      stream_null->GetSize();
      stream_null->GetPosition();

      // valid post_data_stream path with fuzz-controlled values
      CefRefPtr<ArkWeb_HttpBodyStream_> stream =
          new ArkWeb_HttpBodyStream_(nullptr);
      auto* mock_post_data = new MockPostDataStream();
      stream->post_data_stream = mock_post_data;
      uint64_t fuzz_size = dataProvider.ConsumeIntegral<uint64_t>();
      uint64_t fuzz_position = dataProvider.ConsumeIntegral<uint64_t>();
      mock_post_data->SetSize(fuzz_size);
      mock_post_data->SetPosition(fuzz_position);
      stream->GetSize();
      stream->GetPosition();

      stream->post_data_stream = nullptr;
      delete mock_post_data;
      break;
    }
    case 8: {
      // IsChunked, IsEOF, IsInMemory: null + valid with fuzz-controlled
      CefRefPtr<ArkWeb_HttpBodyStream_> stream_null =
          new ArkWeb_HttpBodyStream_(nullptr);
      stream_null->IsChunked();
      stream_null->IsEOF();
      stream_null->IsInMemory();

      // valid post_data_stream path with fuzz-controlled flags
      CefRefPtr<ArkWeb_HttpBodyStream_> stream =
          new ArkWeb_HttpBodyStream_(nullptr);
      auto* mock_post_data = new MockPostDataStream();
      stream->post_data_stream = mock_post_data;
      bool fuzz_chunked = dataProvider.ConsumeBool();
      bool fuzz_eof = dataProvider.ConsumeBool();
      bool fuzz_in_memory = dataProvider.ConsumeBool();
      mock_post_data->SetChunked(fuzz_chunked);
      mock_post_data->SetEOF(fuzz_eof);
      mock_post_data->SetInMemory(fuzz_in_memory);
      stream->IsChunked();
      stream->IsEOF();
      stream->IsInMemory();

      stream->post_data_stream = nullptr;
      delete mock_post_data;
      break;
    }
    case 9: {
      // OnInitComplete paths (avoiding is_ets_=true which calls CEF_POST_TASK)
      // null post_data_stream path
      CefRefPtr<ArkWeb_HttpBodyStream_> stream_null_pds =
          new ArkWeb_HttpBodyStream_(nullptr);
      int fuzz_rv_1 = dataProvider.ConsumeIntegral<int>();
      stream_null_pds->OnInitComplete(fuzz_rv_1);

      // null stream_init_callback path (with valid post_data_stream)
      CefRefPtr<ArkWeb_HttpBodyStream_> stream_null_cb =
          new ArkWeb_HttpBodyStream_(nullptr);
      auto* mock_post_data_9a = new MockPostDataStream();
      stream_null_cb->post_data_stream = mock_post_data_9a;
      stream_null_cb->stream_init_callback = nullptr;
      stream_null_cb->is_ets_ = false;
      int fuzz_rv_2 = dataProvider.ConsumeIntegral<int>();
      stream_null_cb->OnInitComplete(fuzz_rv_2);

      // valid callback path with is_ets_ = false (direct invocation)
      CefRefPtr<ArkWeb_HttpBodyStream_> stream_valid_cb =
          new ArkWeb_HttpBodyStream_(nullptr);
      auto* mock_post_data_9b = new MockPostDataStream();
      stream_valid_cb->post_data_stream = mock_post_data_9b;
      ArkWeb_HttpBodyStreamInitCallback test_init_callback =
          [](const ArkWeb_HttpBodyStream* httpBodyStream,
             ArkWeb_NetError result) {};
      stream_valid_cb->stream_init_callback = test_init_callback;
      stream_valid_cb->is_ets_ = false;
      int fuzz_rv_3 = dataProvider.ConsumeIntegral<int>();
      stream_valid_cb->OnInitComplete(fuzz_rv_3);

      stream_null_cb->post_data_stream = nullptr;
      stream_valid_cb->post_data_stream = nullptr;
      delete mock_post_data_9a;
      delete mock_post_data_9b;
      break;
    }
    case 10: {
      // ResetOnIOThread: null post_data_stream path (returns early)
      CefRefPtr<ArkWeb_HttpBodyStream_> stream_null_rst =
          new ArkWeb_HttpBodyStream_(nullptr);
      stream_null_rst->ResetOnIOThread();

      // Valid path: post_data_stream is non-null.
      // ResetOnIOThread calls post_data_stream->Reset() then Release().
      // With CefRefPtr holding the object (refcount=1), we must AddRef
      // before calling to prevent premature deletion.
      CefRefPtr<ArkWeb_HttpBodyStream_> stream_rst =
          new ArkWeb_HttpBodyStream_(nullptr);
      auto* mock_post_data_10 = new MockPostDataStream();
      stream_rst->post_data_stream = mock_post_data_10;
      stream_rst->AddRef();  // Refcount: 1 -> 2
      stream_rst->ResetOnIOThread();  // Release() -> Refcount: 2 -> 1
      // CefRefPtr destructor will Release() -> 0 -> delete

      stream_rst->post_data_stream = nullptr;
      delete mock_post_data_10;
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