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


#include "arkweb/chromium_ext/components/services/font_data/font_data_service_impl.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/containers/heap_array.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "skia/ext/font_utils.h"
#include "third_party/skia/include/core/SkFontStyle.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"


namespace font_data_service {

namespace {
base::SequencedTaskRunner* GetFontDataServiceTaskRunner() {
  static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>>
      task_runner{base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING})};
  return task_runner->get();
}

void BindToFontService(
    mojo::PendingReceiver<font_data_service::mojom::FontDataService> receiver) {
  static base::NoDestructor<font_data_service::FontDataServiceImpl> service;
  service->BindReceiver(std::move(receiver));
}
}

FontDataServiceImpl::FontDataServiceImpl(){}

FontDataServiceImpl::~FontDataServiceImpl() = default;


void FontDataServiceImpl::ConnectToFontService(
    mojo::PendingReceiver<font_data_service::mojom::FontDataService> receiver) {
  GetFontDataServiceTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&BindToFontService, std::move(receiver)));
}

void FontDataServiceImpl::BindReceiver(
    mojo::PendingReceiver<mojom::FontDataService> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(this, std::move(receiver));
}


void FontDataServiceImpl::GetFontConfigInfo(GetFontConfigInfoCallback callback){
   TRACE_EVENT0("fonts", "FontDataServiceImpl::GetFontConfigInfo");

  OHOS::NWeb::ArkWeb_Drawing_FontConfigInfo* fontconfig = nullptr;
  OHOS::NWeb::OhosAdapterHelper::GetInstance()
      .GetOhosDrawingTextTypographyAdapter()
      .GetSystemFontConfigInfo(nullptr, reinterpret_cast<void**>(&fontconfig));
  if (fontconfig == nullptr) {
    std::move(callback).Run(nullptr);
    LOG(ERROR) << "the font configuration class was empty.";
    return;
  }
  auto result = mojom::FontConfigInfo::New();
  result->fontDirSize = fontconfig->fontDirSize;
  result->fontGenericInfoSize = fontconfig->fontGenericInfoSize;
  result->fallbackGroupSize = fontconfig->fallbackGroupSize;
  
  for (size_t i = 0; i < fontconfig->fontDirSize; i++) {
    result->fontDirSet.push_back(fontconfig->fontDirSet[i]);
  }
  for (size_t i = 0; i < fontconfig->fontGenericInfoSize; i++) {
    auto familyName = fontconfig->fontGenericInfoSet[i].familyName;
    auto aliasInfoSize = fontconfig->fontGenericInfoSet[i].aliasInfoSize;
    auto adjustInfoSize = fontconfig->fontGenericInfoSet[i].adjustInfoSize;
    
    auto genericInfo = mojom::FontGenericInfo::New();
    genericInfo->familyName = familyName;
    genericInfo->aliasInfoSize = aliasInfoSize;
    genericInfo->adjustInfoSize = adjustInfoSize;

    for (size_t j = 0; j < aliasInfoSize; j++) {
      auto info = mojom::FontAliasInfo::New();
      info->weight = 
          fontconfig->fontGenericInfoSet[i].aliasInfoSet[j].weight;
      info->familyName =
          fontconfig->fontGenericInfoSet[i].aliasInfoSet[j].familyName;
      genericInfo->aliasInfoSet.push_back(std::move(info));
    }

    for (size_t j = 0; j < adjustInfoSize; j++) {
      auto info = mojom::FontAdjustInfo::New();
      info->weight = 
          fontconfig->fontGenericInfoSet[i].adjustInfoSet[j].weight;
      info->to =
          fontconfig->fontGenericInfoSet[i].adjustInfoSet[j].to; 
      genericInfo->adjustInfoSet.push_back(std::move(info));
    }
    result->fontGenericInfoSet.push_back(std::move(genericInfo));
  }
  for (size_t i = 0; i < fontconfig->fallbackGroupSize; i++) {
    auto fallbackInfoSize = fontconfig->fallbackGroupSet[i].fallbackInfoSize;
    auto groupName = fontconfig->fallbackGroupSet[i].groupName;
    auto group = mojom::FontFallbackGroup::New();
    group->fallbackInfoSize = fallbackInfoSize;
    for (size_t j = 0; j < fallbackInfoSize; j++) {
      auto info = mojom::FontFallbackInfo::New();
      auto language = fontconfig->fallbackGroupSet[i].fallbackInfoSet[j].language;
      if (language) {
        info->language = language;
      }
      info->familyName = fontconfig->fallbackGroupSet[i].fallbackInfoSet[j].familyName;
      group->fallbackInfoSet.push_back(std::move(info));
    }
    result->fallbackGroupSet.push_back(std::move(group));
  }
  std::move(callback).Run(std::move(result));
    OHOS::NWeb::OhosAdapterHelper::GetInstance()
      .GetOhosDrawingTextTypographyAdapter()
      .DestroySystemFontConfigInfo(fontconfig);
  fontconfig = nullptr;
}
}  // namespace font_data_service
