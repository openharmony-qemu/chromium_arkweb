// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARKWEB_CHROMIUM_EXT_COMPONENTS_WHITE_SCREEN_FRAME_STATS_BRIDGE_H_
#define ARKWEB_CHROMIUM_EXT_COMPONENTS_WHITE_SCREEN_FRAME_STATS_BRIDGE_H_

#include <map>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "arkweb/chromium_ext/components/blank_screen/frame_analysis.h"

namespace blank_screen {

// Bridge for frame analysis and JS exception delivery from GPU/renderer
// processes to the browser UI thread. Routes FrameStats and JS exceptions
// to the registered BlankScreenMonitor callback per FrameSinkId.
class FrameStatsBridge {
 public:
  using FrameCallback =
      base::RepeatingCallback<void(const FrameStats&)>;
  using JsExceptionCallback =
      base::RepeatingCallback<void(int source,
                                   const std::string& message,
                                   const std::string& url,
                                   int line)>;

  static FrameStatsBridge& GetInstance();

  void Register(const viz::FrameSinkId& id, FrameCallback callback);
  void Unregister(const viz::FrameSinkId& id);

  // Called by HostFrameSinkManager when FrameStats arrive from GPU process.
  void OnFrameAnalyzedFromGpuProcess(const viz::FrameSinkId& id,
                                      const FrameStats& stats);

  void RegisterJsExceptionCallback(const viz::FrameSinkId& id,
                                   JsExceptionCallback callback);
  void UnregisterJsExceptionCallback(const viz::FrameSinkId& id);

  // Called when JS exception arrives from renderer process via Mojo.
  void OnJsExceptionFromRenderer(const viz::FrameSinkId& id,
                                 int source,
                                 const std::string& message,
                                 const std::string& url,
                                 int line);

 private:
  FrameStatsBridge();
  ~FrameStatsBridge();

  friend class base::NoDestructor<FrameStatsBridge>;

  struct CallbackEntry {
    FrameCallback callback;
    scoped_refptr<base::SequencedTaskRunner> task_runner;
  };

  struct JsExceptionCallbackEntry {
    JsExceptionCallback callback;
    scoped_refptr<base::SequencedTaskRunner> task_runner;
  };

  base::Lock lock_;
  std::map<viz::FrameSinkId, CallbackEntry> callbacks_
      GUARDED_BY(lock_);
  std::map<viz::FrameSinkId, JsExceptionCallbackEntry> js_callbacks_
      GUARDED_BY(lock_);
};

}  // namespace blank_screen

#endif  // ARKWEB_CHROMIUM_EXT_COMPONENTS_WHITE_SCREEN_FRAME_STATS_BRIDGE_H_
