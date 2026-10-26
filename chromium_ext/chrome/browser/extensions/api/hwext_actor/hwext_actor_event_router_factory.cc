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

#include "arkweb/chromium_ext/chrome/browser/extensions/api/hwext_actor/hwext_actor_event_router_factory.h"

#include "arkweb/build/features/features.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/extensions/api/hwext_actor/hwext_actor_event_router.h"
#include "chrome/browser/extensions/api/hwext_actor/hwext_actor_owner_registry_factory.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "extensions/browser/event_router_factory.h"

namespace extensions {

// static
HwextActorEventRouterFactory* HwextActorEventRouterFactory::GetInstance() {
  static base::NoDestructor<HwextActorEventRouterFactory> factory;
  return factory.get();
}

HwextActorEventRouterFactory::HwextActorEventRouterFactory()
    : ProfileKeyedServiceFactory(
          "HwextActorEventRouter",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(EventRouterFactory::GetInstance());
  DependsOn(actor::ActorKeyedServiceFactory::GetInstance());
  DependsOn(HwextActorOwnerRegistryFactory::GetInstance());
}

bool HwextActorEventRouterFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService>
HwextActorEventRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<HwextActorEventRouter>(context);
}

}  // namespace extensions
