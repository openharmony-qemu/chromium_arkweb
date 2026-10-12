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

#ifndef ARKWEB_CHROMIUM_EXT_CONTENT_RENDERER_DFX_EVENT_SENDER_H_
#define ARKWEB_CHROMIUM_EXT_CONTENT_RENDERER_DFX_EVENT_SENDER_H_

#include <string>

// NOTE: The mojom interface header lives under content/common/dfx/mojom/.
// .mojom files are inherently cross-process: the generated C++ bindings are
// consumed by both the browser (server) and the renderer (client) sides, so
// the mojom source is placed under content/common/ to reflect that it is
// shared across processes rather than owned by the browser side.
#include "arkweb/chromium_ext/content/common/dfx/mojom/dfx_reporting.mojom.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

// Singleton that provides a generic renderer->browser hisysevent reporting
// channel via the DfxReporter Mojo interface.
//
// Renderer code calls ReportHiSysEvent(event_name, dict) which serializes
// the dict to JSON and forwards it to the browser process. The browser-side
// DfxReporterImpl validates the event name against a whitelist, parses the
// JSON and dispatches to the corresponding event_reporter::Report* function.
//
// This replaces direct HiSysEventAdapter::Write() calls in the renderer
// process, which are no longer permitted for sandboxed renderers.
//
// Thread safety: The Mojo |remote_| is bound on the sequence that calls
// GetPendingReceiver().  ReportHiSysEvent() may be called from any sequence;
// if called from a different sequence, the call is posted to the binding
// sequence to avoid cross-sequence Mojo usage.  |binding_task_runner_| is
// protected by |lock_| to establish a happens-before relationship between
// the write in GetPendingReceiver() and the read in ReportHiSysEvent().
//
// Binding: The Mojo connection is established from
// RenderFrameImpl::DidCommitNavigation (alongside the existing
// MemoryMonitorImpl binding).  Use GetPendingReceiver() to obtain the
// pending receiver and pass it to GetBrowserInterfaceBroker().GetInterface().
class DfxEventSender {
 public:
  static DfxEventSender* GetInstance();

  // Returns a PendingReceiver to be passed to
  // GetBrowserInterfaceBroker().GetInterface().  This also creates the
  // message pipe and binds |remote_| to the client end.  Must be called on
  // the sequence that will own the Mojo connection.
  mojo::PendingReceiver<dfx::mojom::DfxReporter> GetPendingReceiver();

  // Generic event reporting. Serializes |info| to JSON and sends
  // (event_name, json) to the browser via DfxReporter::ReportHiSysEvent.
  // Safe to call from any sequence; cross-sequence calls are forwarded to
  // the binding sequence via PostTask.
  void ReportHiSysEvent(const std::string& event_name, base::Value::Dict info);

  bool IsBound() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return remote_.is_bound();
  }

 private:
  friend class base::NoDestructor<DfxEventSender>;

  DfxEventSender();
  ~DfxEventSender();

  void ReportHiSysEventOnSequence(const std::string& event_name,
                                  const std::string& json_string);

  mojo::Remote<dfx::mojom::DfxReporter> remote_;

  // The sequence on which |remote_| was bound.  Used to forward
  // cross-sequence ReportHiSysEvent calls.  Guarded by |lock_| because
  // GetPendingReceiver() writes on the binding sequence while
  // ReportHiSysEvent() may read from any sequence.
  scoped_refptr<base::SequencedTaskRunner> binding_task_runner_;

  // Protects |binding_task_runner_| against the cross-sequence race between
  // GetPendingReceiver() (writer) and ReportHiSysEvent() (reader).
  base::Lock lock_;

  SEQUENCE_CHECKER(sequence_checker_);

  DfxEventSender(const DfxEventSender&) = delete;
  DfxEventSender& operator=(const DfxEventSender&) = delete;
};

}  // namespace content

#endif  // ARKWEB_CHROMIUM_EXT_CONTENT_RENDERER_DFX_EVENT_SENDER_H_
