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

#include <fuzzer/FuzzedDataProvider.h>
#include <memory>
#include <string>

#include "arkweb/chromium_ext/ui/events/devices/mmi_device_info_adapter_impl.h"

using namespace OHOS::NWeb;

namespace {
constexpr size_t MAX_STRING_LEN = 64;

// Global shared resource for performance
static std::unique_ptr<MMIDeviceInfoAdapterImpl> g_adapter = nullptr;

MMIDeviceInfoAdapterImpl* GetAdapter() {
  if (!g_adapter) {
    g_adapter = std::make_unique<MMIDeviceInfoAdapterImpl>();
  }
  return g_adapter.get();
}

// Test all setters with fuzzed values
void FuzzSetters(FuzzedDataProvider* fdp, MMIDeviceInfoAdapterImpl* adapter) {
  int32_t id = fdp->ConsumeIntegral<int32_t>();
  int32_t type = fdp->ConsumeIntegral<int32_t>();
  int32_t bus = fdp->ConsumeIntegral<int32_t>();
  int32_t version = fdp->ConsumeIntegral<int32_t>();
  int32_t product = fdp->ConsumeIntegral<int32_t>();
  int32_t vendor = fdp->ConsumeIntegral<int32_t>();

  std::string name = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);
  std::string phys = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);
  std::string uniq = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);

  adapter->SetId(id);
  adapter->SetType(type);
  adapter->SetBus(bus);
  adapter->SetVersion(version);
  adapter->SetProduct(product);
  adapter->SetVendor(vendor);
  adapter->SetName(name);
  adapter->SetPhys(phys);
  adapter->SetUniq(uniq);
}

// Test all getters
void FuzzGetters(FuzzedDataProvider* fdp, MMIDeviceInfoAdapterImpl* adapter) {
  // First set some values
  FuzzSetters(fdp, adapter);

  // Then retrieve them
  adapter->GetId();
  adapter->GetType();
  adapter->GetBus();
  adapter->GetVersion();
  adapter->GetProduct();
  adapter->GetVendor();
  adapter->GetName();
  adapter->GetPhys();
  adapter->GetUniq();
}

// Test boundary values
void FuzzBoundaryValues(FuzzedDataProvider* fdp, MMIDeviceInfoAdapterImpl* adapter) {
  adapter->SetId(INT32_MAX);
  adapter->SetId(INT32_MIN);
  adapter->SetId(0);

  adapter->SetType(INT32_MAX);
  adapter->SetType(0);

  adapter->SetName(std::string(MAX_STRING_LEN, 'a'));
  adapter->SetName("");

  adapter->GetId();
  adapter->GetName();
}

// Test empty strings
void FuzzEmptyStrings(FuzzedDataProvider* fdp, MMIDeviceInfoAdapterImpl* adapter) {
  adapter->SetName("");
  adapter->SetPhys("");
  adapter->SetUniq("");

  adapter->GetName();
  adapter->GetPhys();
  adapter->GetUniq();
}

// Test TransformToMMIDeviceInfo with valid adapter
void FuzzTransformValidAdapter(FuzzedDataProvider* fdp) {
  auto adapter = std::make_shared<MMIDeviceInfoAdapterImpl>();

  // Set all fields
  FuzzSetters(fdp, adapter.get());

  // Test transformation
  struct MMIDeviceInfo info = TransformToMMIDeviceInfo(adapter);

  // Access fields to ensure no crashes
  (void)info.id;
  (void)info.type;
  (void)info.bus;
  (void)info.version;
  (void)info.product;
  (void)info.vendor;
  (void)info.name;
  (void)info.phys;
  (void)info.uniq;
}

// Test TransformToMMIDeviceInfo with null adapter
void FuzzTransformNullAdapter(FuzzedDataProvider* fdp) {
  std::shared_ptr<MMIDeviceInfoAdapter> null_adapter = nullptr;

  // Test transformation with null (should handle gracefully)
  struct MMIDeviceInfo info = TransformToMMIDeviceInfo(null_adapter);

  // Access fields to ensure no crashes
  (void)info.id;
  (void)info.type;
  (void)info.bus;
  (void)info.version;
  (void)info.product;
  (void)info.vendor;
  (void)info.name;
  (void)info.phys;
  (void)info.uniq;
}

// Test sequential set and get operations
void FuzzSequentialOperations(FuzzedDataProvider* fdp) {
  MMIDeviceInfoAdapterImpl* adapter = GetAdapter();
  if (!adapter) return;

  size_t iterations = fdp->ConsumeIntegralInRange<size_t>(1, 50);

  for (size_t i = 0; i < iterations; i++) {
    int32_t field = fdp->ConsumeIntegralInRange<int32_t>(0, 8);
    int32_t value = fdp->ConsumeIntegral<int32_t>();

    switch (field) {
      case 0:
        adapter->SetId(value);
        (void)adapter->GetId();
        break;
      case 1:
        adapter->SetType(value);
        (void)adapter->GetType();
        break;
      case 2:
        adapter->SetBus(value);
        (void)adapter->GetBus();
        break;
      case 3:
        adapter->SetVersion(value);
        (void)adapter->GetVersion();
        break;
      case 4:
        adapter->SetProduct(value);
        (void)adapter->GetProduct();
        break;
      case 5:
        adapter->SetVendor(value);
        (void)adapter->GetVendor();
        break;
      case 6: {
        std::string str = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);
        adapter->SetName(str);
        (void)adapter->GetName();
        break;
      }
      case 7: {
        std::string str = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);
        adapter->SetPhys(str);
        (void)adapter->GetPhys();
        break;
      }
      case 8: {
        std::string str = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);
        adapter->SetUniq(str);
        (void)adapter->GetUniq();
        break;
      }
    }
  }
}

// Test special string values
void FuzzSpecialStrings(FuzzedDataProvider* fdp) {
  MMIDeviceInfoAdapterImpl* adapter = GetAdapter();
  if (!adapter) return;

  // Test with various special string patterns
  std::string special_strings[] = {
    "",
    " ",
    "test/device",
    "test\0device",
    " very long string with spaces ",
    std::string(MAX_STRING_LEN, 'x'),
    "device_with_numbers_123",
    "device-with-special-chars-!@#$%"
  };

  size_t idx = fdp->ConsumeIntegralInRange<size_t>(0, 7);
  adapter->SetName(special_strings[idx]);
  adapter->SetPhys(special_strings[(idx + 1) % 8]);
  adapter->SetUniq(special_strings[(idx + 2) % 8]);

  (void)adapter->GetName();
  (void)adapter->GetPhys();
  (void)adapter->GetUniq();
}

// Test same value for all integer fields
void FuzzSameIntegerValues(FuzzedDataProvider* fdp) {
  MMIDeviceInfoAdapterImpl* adapter = GetAdapter();
  if (!adapter) return;

  int32_t value = fdp->ConsumeIntegral<int32_t>();

  adapter->SetId(value);
  adapter->SetType(value);
  adapter->SetBus(value);
  adapter->SetVersion(value);
  adapter->SetProduct(value);
  adapter->SetVendor(value);

  (void)adapter->GetId();
  (void)adapter->GetType();
  (void)adapter->GetBus();
  (void)adapter->GetVersion();
  (void)adapter->GetProduct();
  (void)adapter->GetVendor();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!data || size < sizeof(uint32_t)) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);

  MMIDeviceInfoAdapterImpl* adapter = GetAdapter();
  if (!adapter) {
    return 0;
  }

  // Select test scenario to maximize coverage
  uint32_t scenario = fdp.ConsumeIntegralInRange<uint32_t>(0, 9);

  switch (scenario) {
    case 0:
      FuzzSetters(&fdp, adapter);
      break;
    case 1:
      FuzzGetters(&fdp, adapter);
      break;
    case 2:
      FuzzBoundaryValues(&fdp, adapter);
      break;
    case 3:
      FuzzEmptyStrings(&fdp, adapter);
      break;
    case 4:
      FuzzTransformValidAdapter(&fdp);
      break;
    case 5:
      FuzzTransformNullAdapter(&fdp);
      break;
    case 6:
      FuzzSequentialOperations(&fdp);
      break;
    case 7:
      FuzzSpecialStrings(&fdp);
      break;
    case 8:
      FuzzSameIntegerValues(&fdp);
      break;
    case 9:
      // Test transform and then access fields
      auto shared_adapter = std::make_shared<MMIDeviceInfoAdapterImpl>();
      FuzzSetters(&fdp, shared_adapter.get());
      struct MMIDeviceInfo info = TransformToMMIDeviceInfo(shared_adapter);
      (void)info.id;
      (void)info.name;
      break;
  }

  return 0;
}
