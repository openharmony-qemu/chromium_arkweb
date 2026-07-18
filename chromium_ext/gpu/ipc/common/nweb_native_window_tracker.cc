// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nweb_native_window_tracker.h"

#include "base/check.h"
#include "base/logging.h"
#if BUILDFLAG(IS_OHOS)
#include "content/renderer/render_remote_proxy_ohos.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"
#endif
#include "arkweb/ohos_adapter_ndk/ohos_adapter_helper_ext.h"

NWebNativeWindowTracker* g_instance = nullptr;

//LCOV_EXCL_START
NWebNativeWindowTracker::NWebNativeWindowTracker()
    : next_native_window_id_(1) {}

NWebNativeWindowTracker::~NWebNativeWindowTracker() {}

NWebNativeWindowTracker* NWebNativeWindowTracker::GetInstance() {
  return base::Singleton<NWebNativeWindowTracker>::get();
}
//LCOV_EXCL_STOP

int32_t NWebNativeWindowTracker::AddNativeWindow(void* native_window) {
  base::AutoLock lock(window_map_lock_);
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
    std::shared_ptr<content::RenderRemoteProxy> g_render_remote_proxy =
        std::make_shared<content::RenderRemoteProxy>();
    std::shared_ptr<OHOS::NWeb::AafwkBrowserClientAdapter> client;
    {
      base::AutoLock bc_lock(browser_client_lock_);
      client = g_browser_client_;
    }
    if (client) {
      // call binder to get window from browser proc (sync call)
      void *window = client->QueryRenderSurface(native_window_id);
      if (window) {
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
    native_window_map_.erase(native_window_id);
    LOG(DEBUG) << __FUNCTION__
               << "Destroy native_window id = " << native_window_id;
  }

  {
    std::shared_ptr<OHOS::NWeb::AafwkBrowserClientAdapter> client;
    {
      base::AutoLock bc_lock(browser_client_lock_);
      client = g_browser_client_;
    }
    if (client) {
      client->DestroyRenderSurface(native_window_id);
      LOG(DEBUG) << __FUNCTION__
                 << "Destroy browser native_window id = " << native_window_id;
    }
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

void NWebNativeWindowTracker::SetBrowserClient(
    std::shared_ptr<OHOS::NWeb::AafwkBrowserClientAdapter> client) {
  base::AutoLock lock(browser_client_lock_);
  g_browser_client_ = std::move(client);
}

std::shared_ptr<OHOS::NWeb::AafwkBrowserClientAdapter>
NWebNativeWindowTracker::GetBrowserClient() const {
  base::AutoLock lock(browser_client_lock_);
  return g_browser_client_;
}

void NWebNativeWindowTracker::ReportThread(
    OHOS::NWeb::ResSchedStatusAdapter status,
    int32_t process_id,
    int32_t thread_id,
    OHOS::NWeb::ResSchedRoleAdapter role) {
  std::shared_ptr<OHOS::NWeb::AafwkBrowserClientAdapter> client;
  {
    base::AutoLock lock(browser_client_lock_);
    client = g_browser_client_;
  }
  if (client) {
    client->ReportThread(status, process_id, thread_id, role);
  } else {
    LOG(DEBUG) << __FUNCTION__ << " g_browser_client_ is null, skip ReportThread";
  }
}

void NWebNativeWindowTracker::PassSurface(int64_t surface_id) {
  std::shared_ptr<OHOS::NWeb::AafwkBrowserClientAdapter> client;
  {
    base::AutoLock lock(browser_client_lock_);
    client = g_browser_client_;
  }
  if (client) {
    client->PassSurface(surface_id);
  } else {
    LOG(DEBUG) << __FUNCTION__ << " g_browser_client_ is null, skip PassSurface";
  }
}
