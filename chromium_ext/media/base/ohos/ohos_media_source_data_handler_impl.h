// Copyright 2026 The Huawei Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_OHOS_MEDIA_SOURCE_DATA_HANDLER_IMPL_H_
#define MEDIA_BASE_OHOS_MEDIA_SOURCE_DATA_HANDLER_IMPL_H_

#include <atomic>
#include <cstdint>
#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/media_adapter.h"

namespace media {

class MediaSourceDownloadManager;

// MediaSourceDataHandlerImpl is the boundary callback that the OHOS player
// (HiStreamer) reaches through webview's LoaderCallbackImpl. It implements
// OHOS::NWeb::MediaSourceDataHandler and is handed to the player as a
// shared_ptr, so it outlives OHOSMediaPlayerBridge — exactly like
// OHOSMediaPlayerCallback (see ohos_media_player_callback.{h,cc}).
//
// Because it may outlive the bridge (and thus MediaSourceDownloadManager), it
// holds only a WeakPtr to the manager and never derefs a raw pointer. Open/
// Read/Close merely PostTask into the manager on task_runner_; if the manager
// is gone the WeakPtr is invalid and the posted call is dropped — no UAF.
//
// `uuid` is generated here (atomic) so Open can return it synchronously
// (HiStreamer uses the Open return value for subsequent Read/Close); the real
// downloads_ work is deferred to the manager on task_runner_.
class MediaSourceDataHandlerImpl : public OHOS::NWeb::MediaSourceDataHandler {
 public:
  MediaSourceDataHandlerImpl(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::WeakPtr<MediaSourceDownloadManager> manager);
  ~MediaSourceDataHandlerImpl() override;

  int64_t HandleDataOpen(const std::string& url,
                         const std::map<std::string, std::string>& header) override;
  void HandleDataRead(int64_t uuid, int64_t offset, int64_t length) override;
  void HandleDataClose(int64_t uuid) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtr<MediaSourceDownloadManager> manager_;
  std::atomic<int64_t> next_uuid_{1};
};

}  // namespace media

#endif  // MEDIA_BASE_OHOS_MEDIA_SOURCE_DATA_HANDLER_IMPL_H_