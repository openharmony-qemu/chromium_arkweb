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

#include "arkweb/chromium_ext/content/browser/font_unique_name_lookup/arkweb_custom_font_lookup_service.h"

#include "arkweb/chromium_ext/content/browser/font_unique_name_lookup/arkweb_custom_font_manager.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

// static
void ArkwebCustomFontLookupService::Create(
    mojo::PendingReceiver<blink::mojom::ArkwebCustomFontLookup> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<ArkwebCustomFontLookupService>(),
                              std::move(receiver));
}

// static
scoped_refptr<base::SequencedTaskRunner>
ArkwebCustomFontLookupService::GetTaskRunner() {
  static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>> runner(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
           base::TaskPriority::USER_BLOCKING}));
  return *runner;
}

void ArkwebCustomFontLookupService::GetUniqueNameLookupTable(
    blink::mojom::ArkwebCustomFontLookup::GetUniqueNameLookupTableCallback
        callback) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  std::move(callback).Run(
      ArkwebCustomFontManager::GetInstance()->GetAllCustomFontsUniqueNames());
}

void ArkwebCustomFontLookupService::MatchLocalFontByUniqueName(
    const std::string &font_unique_name,
    blink::mojom::ArkwebCustomFontLookup::MatchLocalFontByUniqueNameCallback
        callback) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                         ::base::File());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ArkwebCustomFontManager::GetFontByUniqueName,
                     base::Unretained(ArkwebCustomFontManager::GetInstance()),
                     font_unique_name),
      base::BindOnce(
          [](blink::mojom::ArkwebCustomFontLookup::
                 MatchLocalFontByUniqueNameCallback cb,
             ::base::File result) { std::move(cb).Run(std::move(result)); },
          std::move(callback)));
}

void ArkwebCustomFontLookupService::FetchAllFontFiles(
    blink::mojom::ArkwebCustomFontLookup::FetchAllFontFilesCallback callback) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), base::flat_map<std::string, ::base::File>{});
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ArkwebCustomFontManager::GetAllCustomFonts,
                     base::Unretained(ArkwebCustomFontManager::GetInstance())),
      base::BindOnce(
          [](blink::mojom::ArkwebCustomFontLookup::FetchAllFontFilesCallback cb,
             base::flat_map<std::string, ::base::File> results) {
            std::move(cb).Run(std::move(results));
          },
          std::move(callback)));
}
}  // namespace content
