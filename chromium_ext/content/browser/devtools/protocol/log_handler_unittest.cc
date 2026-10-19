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

#include "content/browser/devtools/protocol/log_handler.h"

#include <memory>

#include "content/browser/devtools/protocol/log.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/inspector_protocol/crdtp/chromium/protocol_traits.h"

namespace content::protocol {

class LogHandlerTest : public testing::Test {
 public:
  void SetUp() override { log_handler_ = std::make_unique<LogHandler>(); }

  void TearDown() override { log_handler_.reset(); }

 protected:
  std::unique_ptr<LogHandler> log_handler_;
};

TEST_F(LogHandlerTest, EnableReturnsFallThrough) {
  DispatchResponse response = log_handler_->Enable();
  EXPECT_EQ(crdtp::DispatchCode::FALL_THROUGH, response.Code());
}

TEST_F(LogHandlerTest, DisableReturnsFallThrough) {
  log_handler_->Enable();
  DispatchResponse response = log_handler_->Disable();
  EXPECT_EQ(crdtp::DispatchCode::FALL_THROUGH, response.Code());
}

TEST_F(LogHandlerTest, DisableWithoutEnable) {
  DispatchResponse response = log_handler_->Disable();
  EXPECT_EQ(crdtp::DispatchCode::FALL_THROUGH, response.Code());
}

TEST_F(LogHandlerTest, EntryAddedNoOpWhenDisabled) {
  auto entry = Log::LogEntry::Create()
                   .SetSource(Log::LogEntry::SourceEnum::Other)
                   .SetLevel(Log::LogEntry::LevelEnum::Info)
                   .SetText("test message")
                   .SetTimestamp(0.0)
                   .Build();
  log_handler_->EntryAdded(entry.get());
}

TEST_F(LogHandlerTest, EntryAddedNoOpAfterDisable) {
  log_handler_->Enable();
  log_handler_->Disable();

  auto entry = Log::LogEntry::Create()
                   .SetSource(Log::LogEntry::SourceEnum::Network)
                   .SetLevel(Log::LogEntry::LevelEnum::Error)
                   .SetText("error message")
                   .SetTimestamp(1.0)
                   .Build();
  log_handler_->EntryAdded(entry.get());
}

TEST_F(LogHandlerTest, FreshHandlerDoesNotForwardEntries) {
  auto entry = Log::LogEntry::Create()
                   .SetSource(Log::LogEntry::SourceEnum::Javascript)
                   .SetLevel(Log::LogEntry::LevelEnum::Verbose)
                   .SetText("verbose message")
                   .SetTimestamp(2.0)
                   .Build();
  log_handler_->EntryAdded(entry.get());
}

}  // namespace content::protocol
