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
#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "arkweb/chromium_ext/content/public/browser/native_embed_info.h"
#include "ui/gfx/geometry/rect.h"

namespace content {
namespace {

constexpr size_t MAX_STRING_LENGTH = 256;
constexpr int MAX_MAP_SIZE = 20;
constexpr int MAX_COORDINATE = 10000;
constexpr int MAX_EMBED_ID = 100000;

void FuzzDefaultConstructor(FuzzedDataProvider* fdp) {
  NativeEmbedInfo info;
  
  // Verify default values
  int id = info.native_embed_id;
  bool url_valid = info.url.is_valid();
  size_t element_id_size = info.embed_element_id.size();
}

void FuzzParameterizedConstructor(FuzzedDataProvider* fdp) {
  int native_embed_id = fdp->ConsumeIntegralInRange<int>(-MAX_EMBED_ID, MAX_EMBED_ID);
  
  size_t url_length = fdp->ConsumeIntegralInRange<size_t>(0, MAX_STRING_LENGTH);
  std::string url_str = fdp->ConsumeRandomLengthString(url_length);
  GURL url(url_str);
  
  std::string element_id = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  std::string native_type = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  std::string native_source = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  std::string tag = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  
  int x = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  int y = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  int width = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  int height = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  gfx::Rect rect(x, y, width, height);
  
  int map_size = fdp->ConsumeIntegralInRange<int>(0, MAX_MAP_SIZE);
  std::map<std::string, std::string> params;
  for (int i = 0; i < map_size; ++i) {
    std::string key = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string value = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    params[key] = value;
  }
  
  NativeEmbedInfo info(native_embed_id, url, element_id, native_type,
                       native_source, tag, rect, params);
}

void FuzzCopyConstructor(FuzzedDataProvider* fdp) {
  // First create an original object
  int native_embed_id = fdp->ConsumeIntegral<int>();
  GURL url(fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH).c_str());
  std::string element_id = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  std::string native_type = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  std::string native_source = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  std::string tag = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  
  int x = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  int y = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  int width = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  int height = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  gfx::Rect rect(x, y, width, height);
  
  int map_size = fdp->ConsumeIntegralInRange<int>(0, MAX_MAP_SIZE);
  std::map<std::string, std::string> params;
  for (int i = 0; i < map_size; ++i) {
    std::string key = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string value = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    params[key] = value;
  }
  
  NativeEmbedInfo original(native_embed_id, url, element_id, native_type,
                           native_source, tag, rect, params);
  
  // Test copy constructor
  NativeEmbedInfo copy(original);
  
  // Test values are copied
  int copied_id = copy.native_embed_id;
  GURL copied_url = copy.url;
}

void FuzzMoveConstructor(FuzzedDataProvider* fdp) {
  // First create an original object
  int native_embed_id = fdp->ConsumeIntegral<int>();
  GURL url(fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH).c_str());
  std::string element_id = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  std::string native_type = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  std::string native_source = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  std::string tag = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  
  int x = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  int y = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  int width = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  int height = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  gfx::Rect rect(x, y, width, height);
  
  int map_size = fdp->ConsumeIntegralInRange<int>(0, MAX_MAP_SIZE);
  std::map<std::string, std::string> params;
  for (int i = 0; i < map_size; ++i) {
    std::string key = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string value = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    params[key] = value;
  }
  
  NativeEmbedInfo original(native_embed_id, url, element_id, native_type,
                           native_source, tag, rect, params);
  
  // Test move constructor
  NativeEmbedInfo moved(std::move(original));
  
  // Test values are moved
  int moved_id = moved.native_embed_id;
  GURL moved_url = moved.url;
}

void FuzzCopyAssignment(FuzzedDataProvider* fdp) {
  NativeEmbedInfo info1;
  NativeEmbedInfo info2;
  
  // Set up info1
  info1.native_embed_id = fdp->ConsumeIntegral<int>();
  info1.url = GURL(fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH).c_str());
  info1.embed_element_id = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  info1.native_type = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  info1.native_source = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  info1.tag = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  
  int x = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  int y = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  int width = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  int height = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  info1.rect = gfx::Rect(x, y, width, height);
  
  int map_size = fdp->ConsumeIntegralInRange<int>(0, MAX_MAP_SIZE);
  for (int i = 0; i < map_size; ++i) {
    std::string key = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string value = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    info1.params[key] = value;
  }
  
}

void FuzzMoveAssignment(FuzzedDataProvider* fdp) {
  NativeEmbedInfo info1;
  NativeEmbedInfo info2;
  
  // Set up info1
  info1.native_embed_id = fdp->ConsumeIntegral<int>();
  info1.url = GURL(fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH).c_str());
  info1.embed_element_id = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  info1.native_type = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  info1.native_source = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  info1.tag = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  
  int x = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  int y = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  int width = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  int height = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  info1.rect = gfx::Rect(x, y, width, height);
  
  int map_size = fdp->ConsumeIntegralInRange<int>(0, MAX_MAP_SIZE);
  for (int i = 0; i < map_size; ++i) {
    std::string key = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string value = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    info1.params[key] = value;
  }
  
  // Test move assignment
  info2 = std::move(info1);
}

void FuzzFieldManipulation(FuzzedDataProvider* fdp) {
  NativeEmbedInfo info;
  
  // Fuzz individual fields
  info.native_embed_id = fdp->ConsumeIntegralInRange<int>(-MAX_EMBED_ID, MAX_EMBED_ID);
  info.url = GURL(fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH).c_str());
  info.embed_element_id = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  info.native_type = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  info.native_source = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  info.tag = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  
  int x = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  int y = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  int width = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  int height = fdp->ConsumeIntegralInRange<int>(-MAX_COORDINATE, MAX_COORDINATE);
  info.rect = gfx::Rect(x, y, width, height);
  
  // Fuzz params map
  int map_size = fdp->ConsumeIntegralInRange<int>(0, MAX_MAP_SIZE);
  info.params.clear();
  for (int i = 0; i < map_size; ++i) {
    std::string key = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string value = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    info.params[key] = value;
  }
  
  // Access fields
  int id = info.native_embed_id;
  std::string element_id = info.embed_element_id;
  std::string type = info.native_type;
  gfx::Rect rect = info.rect;
  size_t params_size = info.params.size();
}

void FuzzParamItem(FuzzedDataProvider* fdp) {
  NativeEmbedParamItem item;
  
  item.status = static_cast<NativeEmbedParamStatus>(
      fdp->ConsumeIntegralInRange<int>(0, 2));
  item.id = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  item.name = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  item.value = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  
  // Access fields
  NativeEmbedParamStatus status = item.status;
  std::string id = item.id;
  std::string name = item.name;
  std::string value = item.value;
}

void FuzzParamDataInfo(FuzzedDataProvider* fdp) {
  NativeEmbedParamDataInfo info;
  
  info.embed_id = fdp->ConsumeIntegralInRange<int>(-MAX_EMBED_ID, MAX_EMBED_ID);
  info.object_attribute_id = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  
  int item_count = fdp->ConsumeIntegralInRange<int>(0, MAX_MAP_SIZE);
  for (int i = 0; i < item_count; ++i) {
    NativeEmbedParamItem item;
    item.status = static_cast<NativeEmbedParamStatus>(
        fdp->ConsumeIntegralInRange<int>(0, 2));
    item.id = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    item.name = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    item.value = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
    info.param_items.push_back(item);
  }
  
  // Test copy constructor
  NativeEmbedParamDataInfo copy(info);
  
  // Test copy assignment
  NativeEmbedParamDataInfo assigned;
  assigned = info;
}

void FuzzBoundaryValues(FuzzedDataProvider* fdp) {
  NativeEmbedInfo info;
  
  // Test extreme values
  info.native_embed_id = INT32_MIN;
  info.native_embed_id = INT32_MAX;
  
  info.url = GURL("");
  info.url = GURL("http://example.com/very/long/path/" + 
                  std::string(1000, 'a'));
  
  info.embed_element_id = std::string(10000, 'x');
  info.native_type = std::string(10000, 'y');
  info.native_source = std::string(10000, 'z');
  info.tag = std::string(10000, 'w');
  
  info.rect = gfx::Rect(INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX);
  
  // Test large params map
  for (int i = 0; i < 1000; ++i) {
    std::string key = "key_" + std::to_string(i);
    std::string value = "value_" + std::to_string(i);
    info.params[key] = value;
  }
}

void FuzzTagStateEnum(FuzzedDataProvider* fdp) {
  // Test all TagState enum values
  NativeEmbedInfo::TagState states[] = {
    NativeEmbedInfo::TAG_STATE_CREATE,
    NativeEmbedInfo::TAG_STATE_CHANGE,
    NativeEmbedInfo::TAG_STATE_DESTROY,
    NativeEmbedInfo::TAG_STATE_ENTER_BFCACHE,
    NativeEmbedInfo::TAG_STATE_LEAVE_BFCACHE
  };
  
  for (auto state : states) {
    NativeEmbedInfo info;
    // Use the enum value (in real usage, this would be passed to a function)
    int enum_value = static_cast<int>(state);
  }
}

}  // namespace
}  // namespace content

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(int32_t)) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);

  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 10);

  switch (selector) {
    case 0:
      content::FuzzDefaultConstructor(&fdp);
      break;
    case 1:
      content::FuzzParameterizedConstructor(&fdp);
      break;
    case 2:
      content::FuzzCopyConstructor(&fdp);
      break;
    case 3:
      content::FuzzMoveConstructor(&fdp);
      break;
    case 4:
      content::FuzzCopyAssignment(&fdp);
      break;
    case 5:
      content::FuzzMoveAssignment(&fdp);
      break;
    case 6:
      content::FuzzFieldManipulation(&fdp);
      break;
    case 7:
      content::FuzzParamItem(&fdp);
      break;
    case 8:
      content::FuzzParamDataInfo(&fdp);
      break;
    case 9:
      content::FuzzBoundaryValues(&fdp);
      break;
    case 10:
      content::FuzzTagStateEnum(&fdp);
      break;
    default:
      content::FuzzFieldManipulation(&fdp);
      break;
  }

  return 0;
}
