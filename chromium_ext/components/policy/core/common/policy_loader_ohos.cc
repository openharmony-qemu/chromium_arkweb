// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_ohos.h"

#include <string>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_registry_simple.h"

namespace policy {

namespace {
bool IsPolicyKeySupported(std::string_view key) {
  static constexpr auto kSupportedPolicyKeys =
      base::MakeFixedFlatSet<std::string_view>(
          {"ExtensionInstallForcelist", "ExtensionInstallAllowlist",
           "OriginAgentClusterDefaultEnabled",
           "InsecurePrivateNetworkRequestsAllowed",
           "LegacySameSiteCookieBehaviorEnabledForDomainList"});
  return kSupportedPolicyKeys.contains(key);
}
}  // namespace

void RegisterBrowserPolicyProfilePrefs(PrefRegistrySimple* registry) {
  if (registry->defaults()->GetValue(
          policy::policy_prefs::kBrowserPolicyVersion, nullptr)) {
    return;
  }

  registry->RegisterIntegerPref(policy::policy_prefs::kBrowserPolicyVersion, 0);
}

PolicyChangedEventCallback::PolicyChangedEventCallback(PolicyLoaderOhos* loader)
    : loader_(loader) {}

void PolicyChangedEventCallback::OnPolicyChanged() {
  LOG(INFO) << "Recv browser policy change event and reload policy.";
  if (loader_) {
    loader_->Reload(true);
  }
}

PolicyLoaderOhos::PolicyLoaderOhos(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : AsyncPolicyLoader(task_runner, /*periodic_updates*/ false) {}

PolicyLoaderOhos::~PolicyLoaderOhos() {
  policy::BrowserPolicyHandler::GetInstance()->RemoveObserver(
      event_callback_.get());
}

void PolicyLoaderOhos::InitOnBackgroundThread() {
  event_callback_ = std::make_shared<PolicyChangedEventCallback>(this);

  BrowserPolicyHandler::GetInstance()->AddObserver(event_callback_.get());
}

PolicyBundle PolicyLoaderOhos::Load() {
  return policy::BrowserPolicyHandler::GetInstance()->GetPolicyBundle();
}

// static
bool PolicyLoaderOhos::ParsePolicy(const std::string& json,
                                   PolicyBundle* bundle) {
  /* policy json demo
  "InsecurePrivateNetworkRequestsAllowed": {
    "level": "mandatory",
    "scope": "machine",
    "source": "platform",
    "value": true
  }*/
  if (bundle == nullptr) {
    LOG(WARNING) << "Null bundle given, parse failed";
    return false;
  }

  base::Value::Dict dictionary_value;
  JSONStringValueDeserializer deserializer(json);
  std::string error_msg;
  std::unique_ptr<base::Value> json_value =
      deserializer.Deserialize(/*error_code=*/nullptr, &error_msg);
  if (!json_value) {
    LOG(WARNING) << "Unable to deserialize json data. error_msg: " << error_msg;
    return false;
  }

  if (json_value->type() == base::Value::Type::DICT) {
    auto third_party = json_value->GetDict().Extract("3rdparty");
    if (third_party.has_value()) {
      ParseExtensionsPolicy(&*third_party, bundle);
    }
    for (auto kv : json_value->GetDict()) {
      std::string key = kv.first;

      if (!IsPolicyKeySupported(key)) {
        LOG(WARNING) << "Policy key '" << key << "' is not supported, skipping";
        continue;
      }

      if (kv.second.type() != base::Value::Type::DICT) {
        LOG(WARNING) << "key: " << key << " type is not  DICTIONARY";
        continue;
      }
      base::Value::Dict& kv_dict = kv.second.GetDict();
      base::Value* policy_value = kv_dict.Find("value");
      if (!policy_value) {
        LOG(WARNING) << "key: " << key << " has no value";
        continue;
      }

      switch (policy_value->type()) {
        case base::Value::Type::BOOLEAN: {
          dictionary_value.Set(key, policy_value->GetBool());
          break;
        }
        case base::Value::Type::INTEGER: {
          dictionary_value.Set(key, policy_value->GetInt());
          break;
        }
        case base::Value::Type::DOUBLE: {
          dictionary_value.Set(key, policy_value->GetDouble());
          break;
        }
        case base::Value::Type::STRING: {
          dictionary_value.Set(key, policy_value->GetString());
          break;
        }
        case base::Value::Type::LIST: {
          dictionary_value.Set(key, std::move(policy_value->GetList()));
          break;
        }
        default:
          LOG(WARNING) << "unknown type";
      }
    }
  }

  PolicyMap policy_map;
  policy_map.LoadFrom(dictionary_value, POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM);
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .MergeFrom(policy_map);
  return true;
}

// static
void PolicyLoaderOhos::ParseExtensionsPolicy(const base::Value* policies,
                                             PolicyBundle* bundle) {
  if (!policies || !bundle) {
    LOG(ERROR) << __FUNCTION__ << " policies or bundle is null!";
    return;
  }
  const base::Value::Dict* extensions_dict = policies->GetIfDict();
  if (!extensions_dict) {
    LOG(WARNING) << __FUNCTION__ << " source value is not a dictionary!";
    return;
  }
  for (auto extensions_it : *extensions_dict) {
    std::string key = extensions_it.first;
    if (key != "extensions") {
      LOG(WARNING) << __FUNCTION__ << " unsupported key: " << key;
      continue;
    }
    const base::Value::Dict* items_dict = extensions_it.second.GetIfDict();
    if (!items_dict) {
      LOG(WARNING) << __FUNCTION__ << " extensions value is not a dictionary!";
      continue;
    }
    for (auto items_it : *items_dict) {
      std::string extension_id = items_it.first;
      const base::Value::Dict* policy_dict = items_it.second.GetIfDict();
      if (!policy_dict) {
        LOG(WARNING) << __FUNCTION__ << " extension " << extension_id
                     << " value is not a dictionary!";
        continue;
      }
      PolicyMap policy;
      policy.LoadFrom(*policy_dict, POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM);
      bundle->Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, extension_id))
          .MergeFrom(policy);
      LOG(INFO) << __FUNCTION__ << " extension " << extension_id
                << " policy merged";
    }
  }
}

}  // namespace policy
