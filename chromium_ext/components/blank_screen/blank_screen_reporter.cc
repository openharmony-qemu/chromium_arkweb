// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arkweb/chromium_ext/components/blank_screen/blank_screen_reporter.h"

#if BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)

#include "base/logging.h"
#include "base/no_destructor.h"

namespace blank_screen {

BlankScreenReporter& BlankScreenReporter::GetInstance() {
  static base::NoDestructor<BlankScreenReporter> instance;
  return *instance;
}

BlankScreenReporter::BlankScreenReporter() = default;
BlankScreenReporter::~BlankScreenReporter() = default;

void BlankScreenReporter::SetHost(
    const viz::FrameSinkId& frame_sink_id,
    mojo::SharedRemote<blank_screen::mojom::BlankScreenMonitorHost> host) {
  base::AutoLock lock(lock_);
  auto& state = states_[frame_sink_id];
  state.host = std::move(host);
  LOG(DEBUG) << "[BlankScreenMonitor] BlankScreenReporter: host set for "
            << frame_sink_id.ToString()
            << " (total FrameSinkIds: " << states_.size() << ")";
}

void BlankScreenReporter::ResetForNewNavigation(
    const viz::FrameSinkId& frame_sink_id) {
  base::AutoLock lock(lock_);
  auto it = states_.find(frame_sink_id);
  if (it != states_.end()) {
    it->second.content_confirmed = false;
  LOG(DEBUG) << "[BlankScreenMonitor] BlankScreenReporter: reset for new navigation, "
              << frame_sink_id.ToString();
  }
}

void BlankScreenReporter::RemoveFrameSink(
    const viz::FrameSinkId& frame_sink_id) {
  base::AutoLock lock(lock_);
  auto count = states_.erase(frame_sink_id);
  if (count > 0) {
    LOG(DEBUG) << "[BlankScreenMonitor] BlankScreenReporter: removed "
              << frame_sink_id.ToString()
              << " (remaining FrameSinkIds: " << states_.size() << ")";
  }
}

mojo::SharedRemote<blank_screen::mojom::BlankScreenMonitorHost>
BlankScreenReporter::GetHost(const viz::FrameSinkId& frame_sink_id) const {
  base::AutoLock lock(lock_);
  auto it = states_.find(frame_sink_id);
  if (it != states_.end()) {
    return it->second.host;
  }
  return mojo::SharedRemote<blank_screen::mojom::BlankScreenMonitorHost>();
}

void BlankScreenReporter::SetContentConfirmed(
    const viz::FrameSinkId& frame_sink_id) {
  base::AutoLock lock(lock_);
  auto it = states_.find(frame_sink_id);
  if (it != states_.end() && !it->second.content_confirmed) {
    it->second.content_confirmed = true;
    LOG(DEBUG) << "[BlankScreenMonitor] BlankScreenReporter: content confirmed for "
              << frame_sink_id.ToString() << ", stopping frame analysis";
  }
}

bool BlankScreenReporter::IsContentConfirmed(
    const viz::FrameSinkId& frame_sink_id) const {
  base::AutoLock lock(lock_);
  auto it = states_.find(frame_sink_id);
  if (it != states_.end()) {
    return it->second.content_confirmed;
  }
  return false;
}

}  // namespace blank_screen

#endif  // BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)
