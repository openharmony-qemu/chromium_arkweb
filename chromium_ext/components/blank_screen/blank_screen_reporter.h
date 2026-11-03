// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARKWEB_CHROMIUM_EXT_COMPONENTS_WHITE_SCREEN_BLANK_SCREEN_REPORTER_H_
#define ARKWEB_CHROMIUM_EXT_COMPONENTS_WHITE_SCREEN_BLANK_SCREEN_REPORTER_H_

#include "arkweb/build/features/features.h"

#if BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)

#include <map>

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "arkweb/chromium_ext/components/blank_screen/blank_screen_monitor.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace blank_screen {

// Renderer-side singleton holding the Mojo remote to the browser-side
// BlankScreenMonitorHost, one per FrameSinkId. Used by compositor thread
// to send frame analysis results and by JsExceptionCollector to send
// JS exceptions.
struct PerFrameSinkState {
  mojo::SharedRemote<blank_screen::mojom::BlankScreenMonitorHost> host;
  bool content_confirmed = false;
};

class BlankScreenReporter {
 public:
  static BlankScreenReporter& GetInstance();

  BlankScreenReporter(const BlankScreenReporter&) = delete;
  BlankScreenReporter& operator=(const BlankScreenReporter&) = delete;

  void SetHost(const viz::FrameSinkId& frame_sink_id,
               mojo::SharedRemote<blank_screen::mojom::BlankScreenMonitorHost> host);
  void ResetForNewNavigation(const viz::FrameSinkId& frame_sink_id);
  void RemoveFrameSink(const viz::FrameSinkId& frame_sink_id);

  mojo::SharedRemote<blank_screen::mojom::BlankScreenMonitorHost> GetHost(
      const viz::FrameSinkId& frame_sink_id) const;

  void SetContentConfirmed(const viz::FrameSinkId& frame_sink_id);
  bool IsContentConfirmed(const viz::FrameSinkId& frame_sink_id) const;

 private:
  friend class base::NoDestructor<BlankScreenReporter>;
  BlankScreenReporter();
  ~BlankScreenReporter();

  mutable base::Lock lock_;
  std::map<viz::FrameSinkId, PerFrameSinkState> states_;
};

}  // namespace blank_screen

#endif  // BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)

#endif  // ARKWEB_CHROMIUM_EXT_COMPONENTS_WHITE_SCREEN_BLANK_SCREEN_REPORTER_H_
