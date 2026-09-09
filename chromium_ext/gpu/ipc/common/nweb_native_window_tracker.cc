// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nweb_native_window_tracker.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/common/content_switches.h"
#if BUILDFLAG(IS_OHOS)
#include "arkweb/ohos_adapter_ndk/interfaces/ohos_adapter_helper.h"
#endif
#include "arkweb/ohos_adapter_ndk/ohos_adapter_helper_ext.h"

NWebNativeWindowTracker* g_instance = nullptr;

extern void* QueryRenderWindowFromBrowserProcess(int32_t window_id);
extern void DestroyRenderWindowFromBrowserProcess(int32_t window_id);
extern void PassWindowToBrowserProcess(int64_t window_id);

namespace {
constexpr char kGpuProcess[] = "gpu-process";

class BrowserClientAdapterImpl : public OHOS::NWeb::AafwkBrowserClientAdapter {
 public:
  BrowserClientAdapterImpl() = default;
  ~BrowserClientAdapterImpl() override = default;

  void* QueryRenderSurface(int32_t surface_id) override {
    LOG(INFO) << "QueryRenderSurface, surface_id=" << surface_id;
    return QueryRenderWindowFromBrowserProcess(surface_id);
  }

  void ReportThread(OHOS::NWeb::ResSchedStatusAdapter status,
                    int32_t process_id,
                    int32_t thread_id,
                    OHOS::NWeb::ResSchedRoleAdapter role) override {
    LOG(INFO) << "ReportThread is not used, process_id=" << process_id
              << ", thread_id=" << thread_id;
  }

  void PassSurface(int64_t surface_id) override {
    LOG(INFO) << "PassSurface, surface_id=" << surface_id;
    PassWindowToBrowserProcess(surface_id);
  }

  void DestroyRenderSurface(int32_t surface_id) override {
    LOG(INFO) << "DestroyRenderSurface, surface_id=" << surface_id;
    DestroyRenderWindowFromBrowserProcess(surface_id);
  }
};
}  // namespace

//LCOV_EXCL_START
NWebNativeWindowTracker::NWebNativeWindowTracker()
    : next_native_window_id_(1) {
  auto process_type = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kProcessType);
  if (process_type == kGpuProcess) {
    g_browser_client_ = std::make_shared<BrowserClientAdapterImpl>();
  }
}

NWebNativeWindowTracker::~NWebNativeWindowTracker() {}

NWebNativeWindowTracker* NWebNativeWindowTracker::GetInstance() {
  return base::Singleton<NWebNativeWindowTracker>::get();
}
//LCOV_EXCL_STOP

int32_t NWebNativeWindowTracker::AddNativeWindow(void* native_window) {
  base::AutoLock lock(window_map_lock_);
  OHOS::NWeb::OhosAdapterHelper::GetInstance()
      .GetWindowAdapterInstance()
      .NativeWindowSetUsage(native_window);
  int32_t native_window_id = next_native_window_id_++;
  native_window_map_.emplace(native_window_id, std::move((void*)native_window));
  LOG(DEBUG) << "Add native window id = " << native_window_id;
  return native_window_id;
}

void* NWebNativeWindowTracker::GetNativeWindow(int32_t native_window_id, bool is_ref) {
  base::AutoLock lock(window_map_lock_);
  auto it = native_window_map_.find(native_window_id);
  if (it == native_window_map_.end()) {
#ifndef OHOS_FUZZ_COMPILE_ERROR_FIX
    LOG(DEBUG) << __FUNCTION__
               << "Cann't get for native_window id = " << native_window_id;
    if (g_browser_client_) {
      // call binder to get window from browser proc (sync call)
      void *window = g_browser_client_->QueryRenderSurface(native_window_id);
      if (window) {
        OHOS::NWeb::OhosAdapterHelper::GetInstance()
            .GetWindowAdapterInstance()
            .NativeWindowSetUsage(window);
        native_window_map_.emplace(native_window_id, std::move((void *)window));
        LOG(DEBUG) << "Add native window id = " << native_window_id;
        if (is_ref) {
          OHOS::NWeb::OhosAdapterHelper::GetInstance().GetWindowAdapterInstance().AddNativeWindowRef(window);
        }
        return window;
      }
    }
#endif
    return nullptr;
  }

  LOG(DEBUG) << __FUNCTION__
             << "Get for native_window id = " << native_window_id;
  if (is_ref) {
    OHOS::NWeb::OhosAdapterHelper::GetInstance().GetWindowAdapterInstance().AddNativeWindowRef(it->second);
  }
  return it->second;
}

void NWebNativeWindowTracker::DestroyNativeWindow(int32_t native_window_id) {
  base::AutoLock lock(window_map_lock_);
  auto it = native_window_map_.find(native_window_id);
  if (it != native_window_map_.end()) {
    if (g_browser_client_) {
      OHOS::NWeb::OhosAdapterHelper::GetInstance()
          .GetWindowAdapterInstance()
          .NativeWindowUnRef(it->second);
    }
    native_window_map_.erase(native_window_id);
    LOG(DEBUG) << __FUNCTION__
               << "Destroy native_window id = " << native_window_id;
  }

  if (g_browser_client_) {
    g_browser_client_->DestroyRenderSurface(native_window_id);
    LOG(DEBUG) << __FUNCTION__
               << "Destroy browser native_window id = " << native_window_id;
  }
}

bool NWebNativeWindowTracker::CheckNativeWindow(void* native_window)
{
    base::AutoLock lock(window_map_lock_);
    for (auto it : native_window_map_) {
        if (it.second == native_window) {
            return true;
        }
    }
    return false;
}
