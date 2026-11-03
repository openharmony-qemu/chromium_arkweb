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

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_MIME_SNIFFING_URL_LOADER_H_
#error "must be in include form THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_MIME_SNIFFING_URL_LOADER_H_"
#endif

#include "base/time/time.h"

#if BUILDFLAG(ARKWEB_RESOURCE_INTERCEPTION)
void MimeSniffingURLLoader::OnTransferDataWithSharedMemory(
    base::ReadOnlySharedMemoryRegion region,
    uint64_t buffer_size) {
  // In sync mode with intercepted ArrayBuffer responses, the shared memory
  // message is the sole data and completion channel (ReadMore is skipped in
  // StreamReaderURLLoader::Continue). The data pipe body is empty, so the
  // normal sniffing flow (reading from data pipe) cannot work. Instead, sniff
  // the mime type from the shared memory data, resume the throttle, then
  // forward the shm data and completion status directly to the destination.
  switch (state_) {
    case State::kAborted:
      return;
    case State::kSniffing:
    case State::kWaitForBody: {
      // Sniff mime type from shared memory data.
      if (region.IsValid()) {
        base::ReadOnlySharedMemoryMapping mapping = region.Map();
        if (mapping.IsValid() && buffer_size > 0 &&
            buffer_size <= mapping.size()) {
          std::string new_type;
          net::SniffMimeType(
              std::string_view(mapping.GetMemoryAs<char>(),
                               static_cast<size_t>(buffer_size)),
              response_url_, response_head_->mime_type,
              net::ForceSniffFileUrlsForHtml::kDisabled, &new_type);
          response_head_->mime_type = new_type;
          response_head_->did_mime_sniff = true;
        } else {
          response_head_->mime_type = kDefaultMimeType;
        }
      } else {
        response_head_->mime_type = kDefaultMimeType;
      }

      state_ = State::kCompleted;

      // Stop watching the data pipe — the body arrives via shared memory.
      body_consumer_watcher_.Cancel();
      body_producer_watcher_.Cancel();
      body_consumer_handle_.reset();
      body_producer_handle_.reset();

      // Resume the throttle with the updated response head and no data pipe
      // body. ThrottlingURLLoader::Resume will call ForwardResponseToClient
      // which sends OnReceiveResponse to the destination synchronously.
      if (!throttle_) {
        Abort();
        return;
      }
      throttle_->ResumeWithNewResponseHead(
          std::move(response_head_),
          mojo::ScopedDataPipeConsumerHandle());

      // Forward the shared memory data to the destination client.
      destination_url_loader_client_->OnTransferDataWithSharedMemory(
          std::move(region), buffer_size);

      // Send OnComplete — the shm path does not go through ReadMore, so no
      // OnComplete would otherwise be sent.
      network::URLLoaderCompletionStatus status(net::OK);
      status.completion_time = base::TimeTicks::Now();
      status.encoded_data_length = buffer_size;
      status.encoded_body_length = buffer_size;
      status.decoded_body_length = buffer_size;
      destination_url_loader_client_->OnComplete(status);
      return;
    }
    case State::kSending:
    case State::kCompleted:
      // These states should not be reached for the shm path.
      return;
  }
}
#endif  // BUILDFLAG(ARKWEB_RESOURCE_INTERCEPTION)
