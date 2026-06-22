// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_OHOS_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_OHOS_H_

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "cef/ohos_cef_ext/libcef/browser/policy/browser_policy_handler.h"
#include "components/policy/core/common/async_policy_loader.h"
#include "components/policy/policy_export.h"

namespace base {
class Value;
}

namespace policy {

class PolicyChangedEventCallback;

void RegisterBrowserPolicyProfilePrefs(PrefRegistrySimple* registry);

class POLICY_EXPORT PolicyLoaderOhos : public AsyncPolicyLoader {
 public:
  PolicyLoaderOhos(scoped_refptr<base::SequencedTaskRunner> task_runner);
  PolicyLoaderOhos(const PolicyLoaderOhos&) = delete;
  PolicyLoaderOhos& operator=(const PolicyLoaderOhos&) = delete;
  ~PolicyLoaderOhos() override;

  // AsyncPolicyLoader implementation.
  void InitOnBackgroundThread() override;
  PolicyBundle Load() override;

  static bool ParsePolicy(const std::string& json, PolicyBundle* bundle);
  static void ParseExtensionsPolicy(const base::Value* policies,
                                    PolicyBundle* bundle);

 private:
  std::shared_ptr<PolicyChangedEventCallback> event_callback_;
};

class PolicyChangedEventCallback
    : public policy::BrowserPolicyHandler::Observer {
 public:
  PolicyChangedEventCallback(PolicyLoaderOhos* loader);

  // policy::BrowserPolicyHandler::Observer overrides
  void OnPolicyChanged() override;

 private:
  friend class PolicyChangedEventCallbackTest;
  raw_ptr<PolicyLoaderOhos> loader_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_OHOS_H_
