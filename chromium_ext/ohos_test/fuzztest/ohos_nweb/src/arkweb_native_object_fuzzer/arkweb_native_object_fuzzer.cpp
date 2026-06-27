// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include <cstring>
#include <memory>
#include <string>

#include "arkweb_native_object.h"

namespace OHOS::NWeb {

void FuzzBindWebTagToWebInstance(const uint8_t* data, size_t size) {
  FuzzedDataProvider dataProvider(data, size);

  uint8_t op = dataProvider.ConsumeIntegralInRange<uint8_t>(0, 8);

  switch (op) {
    case 0: {
      // BindWebTagToWebInstance with fuzz-derived webTag, no NWebImpl
      // → GetWebInstanceByWebTag returns nullptr → else branch creates entry
      std::string tag = dataProvider.ConsumeRandomLengthString(256);
      int32_t id = dataProvider.ConsumeIntegral<int32_t>();
      ArkWebNativeObject::BindWebTagToWebInstance(id, tag.c_str());

      // Clean up entry
      auto obj = ArkWebNativeObject::GetWebInstanceByWebTag(tag.c_str());
      if (obj) {
        obj->SetDestroyCallback([]() {});
        obj->FireDestroyCallback();
      }
      break;
    }
    case 1: {
      // BindWebTagToWebInstance with empty string webTag
      std::string tag = "";
      int32_t id = dataProvider.ConsumeIntegral<int32_t>();
      ArkWebNativeObject::BindWebTagToWebInstance(id, tag.c_str());

      auto obj = ArkWebNativeObject::GetWebInstanceByWebTag(tag.c_str());
      if (obj) {
        obj->SetDestroyCallback([]() {});
        obj->FireDestroyCallback();
      }
      break;
    }
    case 2: {
      // BindWebTagToWebInstance: first bind (else branch), then re-bind
      // (if branch: webObject exists, NWebImpl not registered →
      //  GetNWebSharedPtr returns nullptr → SetWebWeakPtr logs error,
      //  FireValidCallback logs error)
      std::string tag = dataProvider.ConsumeRandomLengthString(64);
      int32_t id1 = dataProvider.ConsumeIntegral<int32_t>();
      int32_t id2 = dataProvider.ConsumeIntegral<int32_t>();

      // First bind: creates entry in g_NativeWebMap
      ArkWebNativeObject::BindWebTagToWebInstance(id1, tag.c_str());

      // Second bind: finds existing entry, GetNWebSharedPtr(id2) returns
      // nullptr since no NWebImpl registered with that id.
      // SetWebWeakPtr(nullptr) logs error, FireValidCallback logs error.
      ArkWebNativeObject::BindWebTagToWebInstance(id2, tag.c_str());

      auto obj = ArkWebNativeObject::GetWebInstanceByWebTag(tag.c_str());
      if (obj) {
        obj->SetDestroyCallback([]() {});
        obj->FireDestroyCallback();
      }
      break;
    }
    case 3: {
      // Repeated binds with same tag and various IDs
      // Tests map lookup + SetWebWeakPtr with null nwebSharedPtr path
      std::string tag = dataProvider.ConsumeRandomLengthString(32);
      int32_t count = dataProvider.ConsumeIntegralInRange<int32_t>(1, 8);

      // First bind creates the entry
      ArkWebNativeObject::BindWebTagToWebInstance(count, tag.c_str());

      // Subsequent binds find the entry and try to set weak ptr
      for (int32_t i = 0; i < count; ++i) {
        int32_t fuzz_id = dataProvider.ConsumeIntegral<int32_t>();
        ArkWebNativeObject::BindWebTagToWebInstance(fuzz_id, tag.c_str());
      }

      auto obj = ArkWebNativeObject::GetWebInstanceByWebTag(tag.c_str());
      if (obj) {
        obj->SetDestroyCallback([]() {});
        obj->FireDestroyCallback();
      }
      break;
    }
    case 4: {
      // GetWebInstanceByWebTag direct test + combined bind/lookup/destroy
      // null webTag → GetWebInstanceByWebTag returns nullptr
      auto null_result = ArkWebNativeObject::GetWebInstanceByWebTag(nullptr);
      (void)null_result;

      // non-existent tag → returns nullptr
      std::string tag = dataProvider.ConsumeRandomLengthString(64);
      auto not_found = ArkWebNativeObject::GetWebInstanceByWebTag(tag.c_str());
      (void)not_found;

      // Bind then lookup — should find it
      int32_t id = dataProvider.ConsumeIntegral<int32_t>();
      ArkWebNativeObject::BindWebTagToWebInstance(id, tag.c_str());
      auto found = ArkWebNativeObject::GetWebInstanceByWebTag(tag.c_str());

      // Destroy then lookup — should not find it
      if (found) {
        found->SetDestroyCallback([]() {});
        found->FireDestroyCallback();
      }
      auto after_destroy =
          ArkWebNativeObject::GetWebInstanceByWebTag(tag.c_str());
      (void)after_destroy;
      break;
    }
    case 5: {
      // FireLoadStartCallback: null callback + valid callback
      std::string tag = dataProvider.ConsumeRandomLengthString(64);
      int32_t id = dataProvider.ConsumeIntegral<int32_t>();
      ArkWebNativeObject::BindWebTagToWebInstance(id, tag.c_str());
      auto obj = ArkWebNativeObject::GetWebInstanceByWebTag(tag.c_str());

      if (obj) {
        // null callback path (loadStartCallback_ is nullptr)
        obj->FireLoadStartCallback();

        // valid callback path
        obj->SetLoadStartCallback([]() {});
        obj->FireLoadStartCallback();

        obj->SetDestroyCallback([]() {});
        obj->FireDestroyCallback();
      }
      break;
    }
    case 6: {
      // FireLoadEndCallback: null callback + valid callback
      std::string tag = dataProvider.ConsumeRandomLengthString(64);
      int32_t id = dataProvider.ConsumeIntegral<int32_t>();
      ArkWebNativeObject::BindWebTagToWebInstance(id, tag.c_str());
      auto obj = ArkWebNativeObject::GetWebInstanceByWebTag(tag.c_str());

      if (obj) {
        // null callback path (loadEndCallback_ is nullptr)
        obj->FireLoadEndCallback();

        // valid callback path
        obj->SetLoadEndCallback([]() {});
        obj->FireLoadEndCallback();

        obj->SetDestroyCallback([]() {});
        obj->FireDestroyCallback();
      }
      break;
    }
    case 7: {
      // FireScrollCallback: null callback + valid callback with fuzz args
      std::string tag = dataProvider.ConsumeRandomLengthString(64);
      int32_t id = dataProvider.ConsumeIntegral<int32_t>();
      ArkWebNativeObject::BindWebTagToWebInstance(id, tag.c_str());
      auto obj = ArkWebNativeObject::GetWebInstanceByWebTag(tag.c_str());

      if (obj) {
        // null callback path (scrollCallback_ is nullptr)
        obj->FireScrollCallback(0.0, 0.0);

        // valid callback path with fuzz scroll coordinates
        double fuzz_x = dataProvider.ConsumeFloatingPoint<double>();
        double fuzz_y = dataProvider.ConsumeFloatingPoint<double>();
        obj->SetScrollCallback([](double x, double y) {});
        obj->FireScrollCallback(fuzz_x, fuzz_y);

        obj->SetDestroyCallback([]() {});
        obj->FireDestroyCallback();
      }
      break;
    }
    case 8: {
      // FireValidCallback with valid validCallback_ set
      // (nwebSharedPtr is null since no NWebImpl registered)
      std::string tag = dataProvider.ConsumeRandomLengthString(64);
      int32_t id = dataProvider.ConsumeIntegral<int32_t>();
      ArkWebNativeObject::BindWebTagToWebInstance(id, tag.c_str());
      auto obj = ArkWebNativeObject::GetWebInstanceByWebTag(tag.c_str());

      if (obj) {
        // null validCallback_ path (already covered via BindWebTagToWebInstance
        // re-bind, but call directly for completeness)
        obj->FireValidCallback();

        // valid validCallback_ path: nwebSharedPtr is null → else branch,
        // then validCallback_() is invoked and returns true
        obj->SetValidCallback([]() {});
        obj->FireValidCallback();

        obj->SetDestroyCallback([]() {});
        obj->FireDestroyCallback();
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace OHOS::NWeb

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if ((data == nullptr) || (size < sizeof(uint8_t))) {
    return 0;
  }
  OHOS::NWeb::FuzzBindWebTagToWebInstance(data, size);
  return 0;
}