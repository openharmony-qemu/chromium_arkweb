// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arkweb/chromium_ext/extensions/browser/api/messaging/browser_message_port.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"

namespace extensions {

namespace {
constexpr char kTimeoutResponse[] = R"({"code":504,"msg":"browser message response timeout"})";
constexpr base::TimeDelta kBrowserMessageTimeout = base::Seconds(10);
}  // namespace

BrowserMessagePort::BrowserMessagePort(base::WeakPtr<ChannelDelegate> channel_delegate, const PortId& port_id,
                     ExtensionId extension_id, BrowserMessageHandler browser_message_handler)
  : MessagePort(std::move(channel_delegate), port_id),
    extension_id_(std::move(extension_id)),
    browser_message_handler_(std::move(browser_message_handler)) {
}

BrowserMessagePort::~BrowserMessagePort() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool BrowserMessagePort::IsValidPort() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

void BrowserMessagePort::DispatchOnMessage(const Message& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!weak_channel_delegate_ || response_pending_) {
    if (response_pending_) {
      LOG(WARNING) << "BrowserMessagePort::DispatchOnMessage: dropping message "
             << "while response is pending for port " << port_id_.port_number;
    }
    return;
  }

  response_pending_ = true;
  response_timer_.Start(FROM_HERE, kBrowserMessageTimeout,
              base::BindOnce(&BrowserMessagePort::OnTimeout, weak_factory_.GetWeakPtr()));
  browser_message_handler_.Run(extension_id_, message.data(),
                 base::BindOnce(&BrowserMessagePort::OnResponse, weak_factory_.GetWeakPtr()));
}

void BrowserMessagePort::OnResponse(std::string response) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!response_pending_) {
    return;
  }
  response_timer_.Stop();
  response_pending_ = false;
  Complete(std::move(response));
}

void BrowserMessagePort::OnTimeout() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!response_pending_) {
    return;
  }
  response_pending_ = false;
  Complete(kTimeoutResponse);
}

void BrowserMessagePort::Complete(std::string response) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!weak_channel_delegate_) {
    return;
  }
  weak_channel_delegate_->PostMessage(
    port_id_, Message(std::move(response), mojom::SerializationFormat::kJson, false /* user_gesture */));
  weak_channel_delegate_->CloseChannel(port_id_, std::string());
}

}  // namespace extensions
