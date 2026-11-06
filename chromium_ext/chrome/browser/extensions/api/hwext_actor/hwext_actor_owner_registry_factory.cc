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

#include "arkweb/chromium_ext/chrome/browser/extensions/api/hwext_actor/hwext_actor_owner_registry_factory.h"

#include "arkweb/chromium_ext/chrome/browser/extensions/api/hwext_actor/hwext_actor_owner_registry.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace extensions {

// static
HwextActorOwnerRegistryFactory* HwextActorOwnerRegistryFactory::GetInstance() {
  static base::NoDestructor<HwextActorOwnerRegistryFactory> factory;
  return factory.get();
}

// static
HwextActorOwnerRegistry* HwextActorOwnerRegistryFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<HwextActorOwnerRegistry*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

HwextActorOwnerRegistryFactory::HwextActorOwnerRegistryFactory()
    : ProfileKeyedServiceFactory(
          "HwextActorOwnerRegistry",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

std::unique_ptr<KeyedService>
HwextActorOwnerRegistryFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<HwextActorOwnerRegistry>(context);
}

}  // namespace extensions
