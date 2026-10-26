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

#ifndef CHROME_BROWSER_EXTENSIONS_API_HWEXT_ACTOR_HWEXT_ACTOR_OWNER_REGISTRY_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_HWEXT_ACTOR_HWEXT_ACTOR_OWNER_REGISTRY_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class BrowserContext;
}

namespace extensions {

class HwextActorOwnerRegistry;

// The factory responsible for creating the HwextActorOwnerRegistry.
class HwextActorOwnerRegistryFactory : public ProfileKeyedServiceFactory {
 public:
  static HwextActorOwnerRegistryFactory* GetInstance();

  HwextActorOwnerRegistryFactory(const HwextActorOwnerRegistryFactory&) = delete;
  HwextActorOwnerRegistryFactory& operator=(
      const HwextActorOwnerRegistryFactory&) = delete;

  static HwextActorOwnerRegistry* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend base::NoDestructor<HwextActorOwnerRegistryFactory>;

  HwextActorOwnerRegistryFactory();
  ~HwextActorOwnerRegistryFactory() override = default;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_HWEXT_ACTOR_HWEXT_ACTOR_OWNER_REGISTRY_FACTORY_H_
