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

#include "arkweb/chromium_ext/chrome/browser/extensions/api/hwext_actor/hwext_actor_worker_observer_factory.h"

#include "arkweb/build/features/features.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "arkweb/chromium_ext/chrome/browser/extensions/api/hwext_actor/hwext_actor_owner_registry_factory.h"
#include "arkweb/chromium_ext/chrome/browser/extensions/api/hwext_actor/hwext_actor_worker_observer.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/process_manager_factory.h"

namespace extensions {

// static
HwextActorWorkerObserverFactory* HwextActorWorkerObserverFactory::GetInstance() {
  static base::NoDestructor<HwextActorWorkerObserverFactory> factory;
  return factory.get();
}

// static
HwextActorWorkerObserver* HwextActorWorkerObserverFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<HwextActorWorkerObserver*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

HwextActorWorkerObserverFactory::HwextActorWorkerObserverFactory()
    : ProfileKeyedServiceFactory(
          "HwextActorWorkerObserver",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ProcessManagerFactory::GetInstance());
  DependsOn(EventRouterFactory::GetInstance());
  DependsOn(HwextActorOwnerRegistryFactory::GetInstance());
  DependsOn(actor::ActorKeyedServiceFactory::GetInstance());
}

bool HwextActorWorkerObserverFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

std::unique_ptr<KeyedService>
HwextActorWorkerObserverFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<HwextActorWorkerObserver>(context);
}

}  // namespace extensions
