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

#ifndef V8_OOM_REPORTER_BROWSER_IMPL_H_
#define V8_OOM_REPORTER_BROWSER_IMPL_H_

#include "arkweb/chromium_ext/v8/dfx/oom_mojom/mojom/v8_oom_reporting.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class V8OOMReporterImpl : public v8_oom::mojom::V8OOMReporter {
public:
  static constexpr uint16_t BYTES_TO_KB = 1024;
#if BUILDFLAG(IS_ARKWEB_EXT)
   static constexpr std::string V8_JS_MEM_LEAK = "V8_JS_MEM_LEAK";
#else
  static constexpr char V8_JS_MEM_LEAK[] = "V8_JS_MEM_LEAK";
#endif

  V8OOMReporterImpl() {}
  ~V8OOMReporterImpl() {}

  static void ProcessPendingReceiver(mojo::PendingReceiver<v8_oom::mojom::V8OOMReporter>& receiver);

  void ReportV8OOM(const std::string& eventName, const std::string& oomInfoJson) override;
};

#endif // V8_OOM_REPORTER_BROWSER_IMPL_H_
