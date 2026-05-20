/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

#include <fuzzer/FuzzedDataProvider.h>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>
#include <cstring>

#include "arkweb/chromium_ext/services/device/business_risk_intelligent_detection/business_risk_intelligent_detection_host_impl.h"
#include "arkweb/ohos_nweb_ex/third_party/securec/include/securec.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"

namespace {

constexpr size_t MAX_NONCE_SIZE = 256;

// Mock FFI function for testing - avoids calling external library
int32_t MockDetectSimulatedClickRiskEnhancedFFI(
    const char *&result,
    const int8_t *nonceArr,
    size_t nonceLength,
    int32_t alg,
    int32_t version) {
  if (nonceLength < 24 || nonceLength > 80) {
    return 1012500007;
  }
  char* cStr = new char[8];
  strcpy_s(cStr, 8, "success");
  result = cStr;
  return 0;
}

// Mock class that inherits from BusinessRiskIntelligentDetectionHostImpl
// and overrides GetDetectFunc to return mock function
class MockBusinessRiskIntelligentDetectionHostImpl
    : public device::BusinessRiskIntelligentDetectionHostImpl {
 public:
  MockBusinessRiskIntelligentDetectionHostImpl() = default;

  device::DetectSimulatedClickRiskEnhancedFFI GetDetectFunc() override {
    return MockDetectSimulatedClickRiskEnhancedFFI;
  }
};

// Global host instance with mock function to avoid destructor issues
MockBusinessRiskIntelligentDetectionHostImpl* GetMockHost() {
  static MockBusinessRiskIntelligentDetectionHostImpl* host = nullptr;
  if (!host) {
    host = new MockBusinessRiskIntelligentDetectionHostImpl();
  }
  return host;
}

void FuzzDetectSimulatedClickRiskEnhanced(FuzzedDataProvider* fdp) {
  auto* host = GetMockHost();
  if (!host) return;

  int32_t request_id = fdp->ConsumeIntegral<int32_t>();
  int32_t algorithm = fdp->ConsumeIntegral<int32_t>();
  int32_t version = fdp->ConsumeIntegral<int32_t>();

  size_t nonce_size = fdp->ConsumeIntegralInRange<size_t>(0, MAX_NONCE_SIZE);
  std::vector<int8_t> nonce(nonce_size);
  for (size_t i = 0; i < nonce_size; ++i) {
    nonce[i] = static_cast<int8_t>(fdp->ConsumeIntegral<int8_t>());
  }

  host->DetectSimulatedClickRiskEnhanced(
      request_id, algorithm, nonce, version,
      base::BindOnce([](int32_t req_id, int32_t result, const std::string& ret_str) {
        (void)req_id;
        (void)result;
        (void)ret_str;
      }));
}

void FuzzWithEmptyNonce(FuzzedDataProvider* fdp) {
  auto* host = GetMockHost();
  if (!host) return;

  std::vector<int8_t> empty_nonce;
  host->DetectSimulatedClickRiskEnhanced(
      0, 0, empty_nonce, 0,
      base::BindOnce([](int32_t req_id, int32_t result, const std::string& ret_str) {
        (void)req_id;
        (void)result;
        (void)ret_str;
      }));
}

void FuzzWithLargeNonce(FuzzedDataProvider* fdp) {
  auto* host = GetMockHost();
  if (!host) return;

  std::vector<int8_t> large_nonce(MAX_NONCE_SIZE);
  for (size_t i = 0; i < MAX_NONCE_SIZE; ++i) {
    large_nonce[i] = static_cast<int8_t>(fdp->ConsumeIntegral<int8_t>());
  }

  host->DetectSimulatedClickRiskEnhanced(
      fdp->ConsumeIntegral<int32_t>(),
      fdp->ConsumeIntegral<int32_t>(),
      large_nonce,
      fdp->ConsumeIntegral<int32_t>(),
      base::BindOnce([](int32_t req_id, int32_t result, const std::string& ret_str) {
        (void)req_id;
        (void)result;
        (void)ret_str;
      }));
}

void FuzzBoundaryValues(FuzzedDataProvider* fdp) {
  auto* host = GetMockHost();
  if (!host) return;

  std::vector<int8_t> nonce = {0, -1, 1, 127, -128};

  int32_t extreme_value = fdp->ConsumeBool() ? INT32_MIN : INT32_MAX;

  host->DetectSimulatedClickRiskEnhanced(
      extreme_value, extreme_value, nonce, extreme_value,
      base::BindOnce([](int32_t req_id, int32_t result, const std::string& ret_str) {
        (void)req_id;
        (void)result;
        (void)ret_str;
      }));
}

void FuzzMultipleCalls(FuzzedDataProvider* fdp) {
  auto* host = GetMockHost();
  if (!host) return;

  uint8_t num_calls = fdp->ConsumeIntegralInRange<uint8_t>(1, 5);
  for (uint8_t i = 0; i < num_calls; ++i) {
    size_t nonce_size = fdp->ConsumeIntegralInRange<size_t>(0, 32);
    std::vector<int8_t> nonce(nonce_size);
    for (size_t j = 0; j < nonce_size; ++j) {
      nonce[j] = static_cast<int8_t>(fdp->ConsumeIntegral<int8_t>());
    }

    host->DetectSimulatedClickRiskEnhanced(
        fdp->ConsumeIntegral<int32_t>(),
        fdp->ConsumeIntegral<int32_t>(),
        nonce,
        fdp->ConsumeIntegral<int32_t>(),
        base::BindOnce([](int32_t req_id, int32_t result, const std::string& ret_str) {
          (void)req_id;
          (void)result;
          (void)ret_str;
        }));
  }
}

void FuzzGetDetectFunc(FuzzedDataProvider* fdp) {
  auto host = std::make_unique<device::BusinessRiskIntelligentDetectionHostImpl>();

  device::DetectSimulatedClickRiskEnhancedFFI func = host->GetDetectFunc();
  (void)func;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!data || size < sizeof(int32_t)) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);
  uint32_t selector = fdp.ConsumeIntegralInRange<uint32_t>(0, 5);

  switch (selector) {
    case 0:
      FuzzDetectSimulatedClickRiskEnhanced(&fdp);
      break;
    case 1:
      FuzzWithEmptyNonce(&fdp);
      break;
    case 2:
      FuzzWithLargeNonce(&fdp);
      break;
    case 3:
      FuzzBoundaryValues(&fdp);
      break;
    case 4:
      FuzzMultipleCalls(&fdp);
      break;
    case 5:
      FuzzGetDetectFunc(&fdp);
      break;
    default:
      FuzzDetectSimulatedClickRiskEnhanced(&fdp);
      break;
  }

  return 0;
}