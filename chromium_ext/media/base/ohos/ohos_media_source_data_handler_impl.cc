// Copyright 2026 The Huawei Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/ohos/ohos_media_source_data_handler_impl.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "media/base/ohos/ohos_media_source_download_manager.h"

namespace media {

MediaSourceDataHandlerImpl::MediaSourceDataHandlerImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<MediaSourceDownloadManager> manager)
    : task_runner_(std::move(task_runner)), manager_(std::move(manager)) {
  DCHECK(task_runner_);
}

MediaSourceDataHandlerImpl::~MediaSourceDataHandlerImpl() = default;

int64_t MediaSourceDataHandlerImpl::HandleDataOpen(
    const std::string& url,
    const std::map<std::string, std::string>& header) {
  // Runs on the OHOS Binder/IPC thread (HiStreamer's LoaderCallback). We must
  // not touch the manager or its downloads_ here. Generate uuid atomically for
  // the synchronous return, then defer all work onto task_runner_. The WeakPtr
  // is only copied into the BindState here (thread-safe); it is deref'd on
  // task_runner_, so if the manager is already gone the call is dropped.
  int64_t uuid = next_uuid_.fetch_add(1);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaSourceDownloadManager::Open, manager_, uuid, url,
                     header));
  return uuid;
}

void MediaSourceDataHandlerImpl::HandleDataRead(int64_t uuid,
                                                int64_t offset,
                                                int64_t length) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaSourceDownloadManager::Read, manager_, uuid, offset,
                     length));
}

void MediaSourceDataHandlerImpl::HandleDataClose(int64_t uuid) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaSourceDownloadManager::Close, manager_, uuid));
}

}  // namespace media