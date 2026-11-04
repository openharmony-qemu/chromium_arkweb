// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MESSAGING_BROWSER_MESSAGE_PORT_H_
#define EXTENSIONS_BROWSER_API_MESSAGING_BROWSER_MESSAGE_PORT_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "extensions/browser/api/messaging/message_port.h"
#include "extensions/common/extension_id.h"

namespace extensions {

class Message;
struct PortId;

// A port that manages one-time messages from extensions to the host browser
// application.
class BrowserMessagePort : public MessagePort {
 public:
  using BrowserMessageResponseCallback = base::OnceCallback<void(std::string response)>;
  using BrowserMessageHandler =
    base::RepeatingCallback<void(const ExtensionId& extension_id, const std::string& message_json,
                   BrowserMessageResponseCallback response_callback)>;

  BrowserMessagePort(base::WeakPtr<ChannelDelegate> channel_delegate, const PortId &port_id,
                     ExtensionId extension_id, BrowserMessageHandler browser_message_handler);
  ~BrowserMessagePort() override;

  // MessagePort implementation.
  bool IsValidPort() override;
  void DispatchOnMessage(const Message &message) override;

 private:
  void OnResponse(std::string response);
  void OnTimeout();
  void Complete(std::string response);

  ExtensionId extension_id_;
  BrowserMessageHandler browser_message_handler_;
  base::OneShotTimer response_timer_;
  bool response_pending_ = false;
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<BrowserMessagePort> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MESSAGING_BROWSER_MESSAGE_PORT_H_
