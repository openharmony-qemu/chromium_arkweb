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

#include "arkweb/chromium_ext/skia/ext/skia_memory_tracer.h"

#include <climits>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr size_t MAX_STRING_LENGTH = 256;
constexpr size_t MAX_VECTOR_SIZE = 64;
constexpr uint64_t MAX_UINT64_VALUE = UINT64_MAX;

void FuzzWithCategoryKeyConstructor(FuzzedDataProvider* fdp) {
  std::string categoryKey = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  bool itemizeType = fdp->ConsumeBool();

  auto tracer = std::make_unique<SkiaMemoryTracer>(
      categoryKey.empty() ? nullptr : categoryKey.c_str(),
      itemizeType);

  size_t numDumps = fdp->ConsumeIntegralInRange<size_t>(0, 20);
  for (size_t i = 0; i < numDumps; ++i) {
    std::string dumpName = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string valueName = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string units = fdp->ConsumeBool() ? "bytes" : "KB";
    uint64_t value = fdp->ConsumeIntegralInRange<uint64_t>(0, MAX_UINT64_VALUE);

    tracer->dumpNumericValue(
        dumpName.empty() ? "" : dumpName.c_str(),
        valueName.empty() ? "" : valueName.c_str(),
        units.c_str(),
        value);
  }

  float memoryMB = tracer->GetGpuMemorySizeInMB();
  (void)memoryMB;
}

void FuzzWithResourceMapConstructor(FuzzedDataProvider* fdp) {
  std::vector<ResourcePair> resourceMap;
  size_t mapSize = fdp->ConsumeIntegralInRange<size_t>(0, MAX_VECTOR_SIZE);

  for (size_t i = 0; i < mapSize; ++i) {
    std::string first = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string second = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);

    if (!first.empty() && !second.empty()) {
      resourceMap.push_back({first.c_str(), second.c_str()});
    }
  }

  bool itemizeType = fdp->ConsumeBool();
  auto tracer = std::make_unique<SkiaMemoryTracer>(resourceMap, itemizeType);

  size_t numDumps = fdp->ConsumeIntegralInRange<size_t>(0, 20);
  for (size_t i = 0; i < numDumps; ++i) {
    std::string dumpName = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string valueName = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string units = fdp->ConsumeBool() ? "bytes" : "KB";
    uint64_t value = fdp->ConsumeIntegralInRange<uint64_t>(0, MAX_UINT64_VALUE);

    tracer->dumpNumericValue(
        dumpName.empty() ? "" : dumpName.c_str(),
        valueName.empty() ? "" : valueName.c_str(),
        units.c_str(),
        value);
  }

  float memoryMB = tracer->GetGpuMemorySizeInMB();
  (void)memoryMB;
}

void FuzzWithDumpStringValue(FuzzedDataProvider* fdp) {
  std::string categoryKey = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  bool itemizeType = fdp->ConsumeBool();

  auto tracer = std::make_unique<SkiaMemoryTracer>(
      categoryKey.empty() ? nullptr : categoryKey.c_str(),
      itemizeType);

  size_t numDumps = fdp->ConsumeIntegralInRange<size_t>(0, 20);
  for (size_t i = 0; i < numDumps; ++i) {
    std::string dumpName = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string valueName = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string value = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);

    tracer->dumpStringValue(
        dumpName.empty() ? "" : dumpName.c_str(),
        valueName.empty() ? "" : valueName.c_str(),
        value.empty() ? "" : value.c_str());
  }

  float memoryMB = tracer->GetGpuMemorySizeInMB();
  (void)memoryMB;
}

void FuzzWithBoundaryValues(FuzzedDataProvider* fdp) {
  auto tracer = std::make_unique<SkiaMemoryTracer>(nullptr, true);

  tracer->dumpNumericValue("", "", "bytes", 0);
  tracer->dumpNumericValue("", "", "bytes", MAX_UINT64_VALUE);
  tracer->dumpNumericValue("", "", "KB", 0);
  tracer->dumpNumericValue("", "", "KB", MAX_UINT64_VALUE);

  tracer->dumpNumericValue("test_dump", "size", "bytes", 0);
  tracer->dumpNumericValue("test_dump", "size", "bytes", MAX_UINT64_VALUE);
  tracer->dumpNumericValue("test_dump", "purgeable_size", "bytes", 1024);
  tracer->dumpNumericValue("test_dump", "type", "texture", 0);

  std::string longName(MAX_STRING_LENGTH, 'a');
  tracer->dumpNumericValue(longName.c_str(), "size", "bytes", 1024);

  float memoryMB = tracer->GetGpuMemorySizeInMB();
  (void)memoryMB;
}

void FuzzWithMultipleElements(FuzzedDataProvider* fdp) {
  std::string categoryKey = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  bool itemizeType = fdp->ConsumeBool();

  auto tracer = std::make_unique<SkiaMemoryTracer>(
      categoryKey.empty() ? nullptr : categoryKey.c_str(),
      itemizeType);

  tracer->dumpNumericValue("element1", "size", "bytes", 1024);
  tracer->dumpNumericValue("element1", "type", "texture", 0);
  tracer->dumpNumericValue("element1", "purgeable_size", "bytes", 512);

  tracer->dumpNumericValue("element2", "size", "bytes", 2048);
  tracer->dumpNumericValue("element2", "type", "buffer", 0);

  tracer->dumpNumericValue("element3", "size", "KB", 1024);

  float memoryMB = tracer->GetGpuMemorySizeInMB();
  (void)memoryMB;
}

void FuzzWithSameElementMultipleTimes(FuzzedDataProvider* fdp) {
  std::vector<ResourcePair> resourceMap;
  resourceMap.push_back({"texture", "TextureResource"});
  resourceMap.push_back({"buffer", "BufferResource"});

  auto tracer = std::make_unique<SkiaMemoryTracer>(resourceMap, true);

  size_t iterations = fdp->ConsumeIntegralInRange<size_t>(1, 50);
  for (size_t i = 0; i < iterations; ++i) {
    tracer->dumpNumericValue("texture_element", "size", "bytes", 
                            fdp->ConsumeIntegral<uint64_t>());
    tracer->dumpNumericValue("buffer_element", "size", "bytes",
                            fdp->ConsumeIntegral<uint64_t>());
  }

  float memoryMB = tracer->GetGpuMemorySizeInMB();
  (void)memoryMB;
}

void FuzzWithSpecialValues(FuzzedDataProvider* fdp) {
  auto tracer = std::make_unique<SkiaMemoryTracer>(nullptr, fdp->ConsumeBool());

  tracer->dumpNumericValue("test", "size", "bytes", 1);
  tracer->dumpNumericValue("test", "size", "bytes", 1023);
  tracer->dumpNumericValue("test", "size", "bytes", 1024);
  tracer->dumpNumericValue("test", "size", "bytes", 1025);
  tracer->dumpNumericValue("test", "size", "bytes", 1048575);
  tracer->dumpNumericValue("test", "size", "bytes", 1048576);
  tracer->dumpNumericValue("test", "size", "bytes", 1048577);

  tracer->dumpNumericValue("test_kb", "size", "KB", 1);
  tracer->dumpNumericValue("test_kb", "size", "KB", 1023);
  tracer->dumpNumericValue("test_kb", "size", "KB", 1024);
  tracer->dumpNumericValue("test_kb", "size", "KB", 1025);

  tracer->dumpNumericValue("test_other", "size", "MB", 100);
  tracer->dumpNumericValue("test_other", "size", "GB", 10);

  float memoryMB = tracer->GetGpuMemorySizeInMB();
  (void)memoryMB;
}

void FuzzWithEmptyAndNullInputs(FuzzedDataProvider* fdp) {
  auto tracer1 = std::make_unique<SkiaMemoryTracer>(nullptr, true);
  auto tracer2 = std::make_unique<SkiaMemoryTracer>(nullptr, false);

  tracer1->dumpNumericValue(nullptr, nullptr, "bytes", 0);
  tracer1->dumpNumericValue("", "", "", 0);
  tracer1->dumpStringValue(nullptr, nullptr, nullptr);
  tracer1->dumpStringValue("", "", "");

  tracer2->dumpNumericValue("test", "size", "bytes", fdp->ConsumeIntegral<uint64_t>());
  tracer2->dumpStringValue("test", "desc", fdp->ConsumeRandomLengthString(64).c_str());

  float memoryMB1 = tracer1->GetGpuMemorySizeInMB();
  float memoryMB2 = tracer2->GetGpuMemorySizeInMB();
  (void)memoryMB1;
  (void)memoryMB2;
}

void FuzzStressTest(FuzzedDataProvider* fdp) {
  std::vector<ResourcePair> resourceMap;
  size_t mapSize = fdp->ConsumeIntegralInRange<size_t>(0, 10);
  for (size_t i = 0; i < mapSize; ++i) {
    std::string key = fdp->ConsumeRandomLengthString(32);
    std::string value = fdp->ConsumeRandomLengthString(32);
    if (!key.empty() && !value.empty()) {
      resourceMap.push_back({key.c_str(), value.c_str()});
    }
  }

  bool useCategoryKey = fdp->ConsumeBool();
  std::unique_ptr<SkiaMemoryTracer> tracer;

  if (useCategoryKey) {
    std::string categoryKey = fdp->ConsumeRandomLengthString(64);
    tracer = std::make_unique<SkiaMemoryTracer>(
        categoryKey.empty() ? nullptr : categoryKey.c_str(),
        fdp->ConsumeBool());
  } else {
    tracer = std::make_unique<SkiaMemoryTracer>(resourceMap, fdp->ConsumeBool());
  }

  size_t numOperations = fdp->ConsumeIntegralInRange<size_t>(1, 100);
  for (size_t i = 0; i < numOperations; ++i) {
    uint8_t operation = fdp->ConsumeIntegralInRange<uint8_t>(0, 2);

    switch (operation) {
      case 0: {
        std::string dumpName = fdp->ConsumeRandomLengthString(128);
        std::string valueName = fdp->ConsumeRandomLengthString(128);
        std::string units = fdp->ConsumeBool() ? "bytes" : "KB";
        uint64_t value = fdp->ConsumeIntegral<uint64_t>();
        tracer->dumpNumericValue(
            dumpName.empty() ? "" : dumpName.c_str(),
            valueName.empty() ? "" : valueName.c_str(),
            units.c_str(),
            value);
        break;
      }
      case 1: {
        std::string dumpName = fdp->ConsumeRandomLengthString(128);
        std::string valueName = fdp->ConsumeRandomLengthString(128);
        std::string value = fdp->ConsumeRandomLengthString(128);
        tracer->dumpStringValue(
            dumpName.empty() ? "" : dumpName.c_str(),
            valueName.empty() ? "" : valueName.c_str(),
            value.empty() ? "" : value.c_str());
        break;
      }
      case 2: {
        float memoryMB = tracer->GetGpuMemorySizeInMB();
        (void)memoryMB;
        break;
      }
    }
  }

  float finalMemoryMB = tracer->GetGpuMemorySizeInMB();
  (void)finalMemoryMB;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(int32_t)) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);

  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 7);

  switch (selector) {
    case 0:
      FuzzWithCategoryKeyConstructor(&fdp);
      break;
    case 1:
      FuzzWithResourceMapConstructor(&fdp);
      break;
    case 2:
      FuzzWithDumpStringValue(&fdp);
      break;
    case 3:
      FuzzWithBoundaryValues(&fdp);
      break;
    case 4:
      FuzzWithMultipleElements(&fdp);
      break;
    case 5:
      FuzzWithSameElementMultipleTimes(&fdp);
      break;
    case 6:
      FuzzWithSpecialValues(&fdp);
      break;
    case 7:
      FuzzStressTest(&fdp);
      break;
    default:
      FuzzWithCategoryKeyConstructor(&fdp);
      break;
  }

  return 0;
}
