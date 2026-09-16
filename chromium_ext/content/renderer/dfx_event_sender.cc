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

#include "arkweb/chromium_ext/content/renderer/dfx_event_sender.h"

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/no_destructor.h"

namespace content {

DfxEventSender::DfxEventSender() = default;
DfxEventSender::~DfxEventSender() = default;

// static
DfxEventSender* DfxEventSender::GetInstance() {
  static base::NoDestructor<DfxEventSender> instance;
  return instance.get();
}

mojo::PendingReceiver<dfx::mojom::DfxReporter>
DfxEventSender::GetPendingReceiver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  {
    base::AutoLock lock(lock_);
    binding_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  }
  return remote_.BindNewPipeAndPassReceiver();
}

void DfxEventSender::ReportHiSysEvent(const std::string& event_name,
                                      base::Value::Dict info) {
  std::string json_string;
  if (!base::JSONWriter::Write(info, &json_string)) {
    LOG(ERROR) << "DfxEventSender: failed to serialize JSON for event "
               << event_name;
    return;
  }

  // Snapshot |binding_task_runner_| under the lock to avoid a data race
  // with GetPendingReceiver() which writes it on the binding sequence.
  scoped_refptr<base::SequencedTaskRunner> task_runner;
  {
    base::AutoLock lock(lock_);
    task_runner = binding_task_runner_;
  }

  // If called from the binding sequence, send directly.  Otherwise, post
  // the task to the binding sequence to avoid cross-sequence Mojo usage.
  if (task_runner && task_runner->RunsTasksInCurrentSequence()) {
    ReportHiSysEventOnSequence(event_name, json_string);
  } else if (task_runner) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&DfxEventSender::ReportHiSysEventOnSequence,
                       // Safe: NoDestructor singleton, this never destructs.
                       base::Unretained(this), event_name, json_string));
  } else {
    LOG(WARNING) << "DfxEventSender: no binding task runner, dropping event "
                 << event_name;
  }
}

void DfxEventSender::ReportHiSysEventOnSequence(
    const std::string& event_name,
    const std::string& json_string) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!remote_.is_bound()) {
    LOG(WARNING) << "DfxEventSender: remote not bound, dropping event "
                 << event_name;
    return;
  }
  remote_->ReportHiSysEvent(event_name, json_string);
}

}  // namespace content
