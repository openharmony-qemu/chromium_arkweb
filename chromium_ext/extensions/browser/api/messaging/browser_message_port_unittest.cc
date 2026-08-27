// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arkweb/chromium_ext/extensions/browser/api/messaging/browser_message_port.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "extensions/browser/api/messaging/message_port.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_context.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class TestChannelDelegate : public MessagePort::ChannelDelegate {
public:
  TestChannelDelegate() = default;
  ~TestChannelDelegate() = default;

  base::WeakPtr<TestChannelDelegate> GetWeakPtr()
  {
    return weak_factory_.GetWeakPtr();
  }

  void CloseChannel(const PortId &port_id, const std::string &error_message) override
  {
    closed_port_id_ = port_id;
    close_error_message_ = error_message;
    ++close_channel_count_;
  }

  void ClosePort(const PortId &port_id, int process_id, const PortContext &port_context, bool close_channel,
           const std::string &error_message) override
  {
  }

  void PostMessage(const PortId &port_id, const Message &message) override
  {
    posted_port_id_ = port_id;
    posted_messages_.push_back(message);
  }

  void NotifyResponsePending(const PortId &port_id) override {}

  const std::vector<Message> &posted_messages() const
  {
    return posted_messages_;
  }
  const PortId &posted_port_id() const
  {
    return posted_port_id_;
  }
  const PortId &closed_port_id() const
  {
    return closed_port_id_;
  }
  const std::string &close_error_message() const
  {
    return close_error_message_;
  }
  int close_channel_count() const
  {
    return close_channel_count_;
  }

private:
  std::vector<Message> posted_messages_;
  PortId posted_port_id_;
  PortId closed_port_id_;
  std::string close_error_message_;
  int close_channel_count_ = 0;
  base::WeakPtrFactory<TestChannelDelegate> weak_factory_{this};
};

PortId CreateTestPortId()
{
  return PortId(base::UnguessableToken::Create(), 1, true, mojom::SerializationFormat::kJson);
}

}  // namespace

TEST(BrowserMessagePortTest, WaitsForAsynchronousResponse)
{
  base::test::TaskEnvironment task_environment;
  TestChannelDelegate delegate;
  const PortId port_id = CreateTestPortId();
  BrowserMessagePort::BrowserMessageResponseCallback response_callback;
  BrowserMessagePort port(
    delegate.GetWeakPtr(), port_id, "test_extension",
    base::BindRepeating(
      [](BrowserMessagePort::BrowserMessageResponseCallback *saved_callback, const ExtensionId &,
         const std::string &,
         BrowserMessagePort::BrowserMessageResponseCallback callback) { *saved_callback = std::move(callback); },
      &response_callback));

  port.DispatchOnMessage(Message(R"({"id":"getAppInfo"})", mojom::SerializationFormat::kJson, false));

  EXPECT_TRUE(delegate.posted_messages().empty());
  EXPECT_EQ(0, delegate.close_channel_count());

  std::move(response_callback).Run(R"({"appId":"test.browser"})");

  ASSERT_EQ(1u, delegate.posted_messages().size());
  EXPECT_EQ(R"({"appId":"test.browser"})", delegate.posted_messages()[0].data());
  EXPECT_EQ(1, delegate.close_channel_count());
}

TEST(BrowserMessagePortTest, TimesOutAndIgnoresLateResponse)
{
  base::test::TaskEnvironment task_environment(base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestChannelDelegate delegate;
  const PortId port_id = CreateTestPortId();
  BrowserMessagePort::BrowserMessageResponseCallback response_callback;
  BrowserMessagePort port(
    delegate.GetWeakPtr(), port_id, "test_extension",
    base::BindRepeating(
      [](BrowserMessagePort::BrowserMessageResponseCallback *saved_callback, const ExtensionId &,
         const std::string &,
         BrowserMessagePort::BrowserMessageResponseCallback callback) { *saved_callback = std::move(callback); },
      &response_callback));

  port.DispatchOnMessage(Message(R"({"id":"getAppInfo"})", mojom::SerializationFormat::kJson, false));
  task_environment.FastForwardBy(base::Seconds(10));

  ASSERT_EQ(1u, delegate.posted_messages().size());
  EXPECT_EQ(R"({"code":504,"msg":"browser message response timeout"})", delegate.posted_messages()[0].data());
  EXPECT_EQ(1, delegate.close_channel_count());

  std::move(response_callback).Run(R"({"appId":"late.browser"})");
  EXPECT_EQ(1u, delegate.posted_messages().size());
  EXPECT_EQ(1, delegate.close_channel_count());
}

}  // namespace extensions
