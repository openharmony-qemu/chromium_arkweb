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

#include "echo_ai_base_ohos.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace content {

namespace {

constexpr char kInput[] = "test input";
constexpr char kContent[] = "test content";

class MockWebContentsDelegate : public WebContentsDelegate {
 public:
  MockWebContentsDelegate() = default;
  ~MockWebContentsDelegate() override = default;

  void OnCreateAISession(AISessionType type,
                         const std::string& uuid,
                         const std::string& input,
                         AISessionCallback callback) override {
    on_create_ai_session_called_ = true;
    type_ = type;
    uuid_ = uuid;
    input_ = input;
    if (callback) {
      std::move(callback).Run(AISessionResultType::SUCCESS, "");
    }
  }

  void OnExecuteAIAction(AISessionType type,
                         const std::string& uuid,
                         const std::string& input,
                         AISessionCallback callback) override {
    on_execute_ai_action_called_ = true;
    execute_type_ = type;
    execute_uuid_ = uuid;
    execute_input_ = input;
    if (callback) {
      std::move(callback).Run(AISessionResultType::SUCCESS, kContent);
    }
  }

  void OnDestroyAISession(AISessionType type, const std::string& uuid) override {
    on_destroy_ai_session_called_ = true;
    destroy_type_ = type;
    destroy_uuid_ = uuid;
  }

  bool on_create_ai_session_called_ = false;
  bool on_execute_ai_action_called_ = false;
  bool on_destroy_ai_session_called_ = false;
  AISessionType type_;
  AISessionType execute_type_;
  AISessionType destroy_type_;
  std::string uuid_;
  std::string input_;
  std::string execute_uuid_;
  std::string execute_input_;
  std::string destroy_uuid_;
};

class MockModelStreamingResponder : public blink::mojom::ModelStreamingResponder {
 public:
  MockModelStreamingResponder() = default;
  ~MockModelStreamingResponder() override = default;

  mojo::PendingRemote<blink::mojom::ModelStreamingResponder> BindRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnStreaming(const std::string& content) override {
    streaming_content_ += content;
    on_streaming_called_ = true;
  }

  void OnCompletion(
      blink::mojom::ModelExecutionContextInfoPtr context_info) override {
    on_completion_called_ = true;
  }

  void OnError(blink::mojom::ModelStreamingResponseStatus status,
               blink::mojom::QuotaErrorInfoPtr quota_error_info) override {
    on_error_called_ = true;
    error_status_ = status;
  }

  void OnQuotaOverflow() override {}

  bool on_streaming_called_ = false;
  bool on_completion_called_ = false;
  bool on_error_called_ = false;
  std::string streaming_content_;
  blink::mojom::ModelStreamingResponseStatus error_status_;

 private:
  mojo::Receiver<blink::mojom::ModelStreamingResponder> receiver_{this};
};

class TestEchoAIBaseOHOS : public EchoAIBaseOHOS {
 public:
  using EchoAIBaseOHOS::HandleCreateSessionCallback;
  using EchoAIBaseOHOS::HandleExecuteActionCallback;
  using EchoAIBaseOHOS::OnCreateAISession;
  using EchoAIBaseOHOS::OnExecuteAIAction;
  using EchoAIBaseOHOS::GetDelegate;
  using EchoAIBaseOHOS::responder_set_;
  using EchoAIBaseOHOS::uuid_;
  using EchoAIBaseOHOS::type_;
  using EchoAIBaseOHOS::create_callback_called_;
  using EchoAIBaseOHOS::callback_;
  using EchoAIBaseOHOS::weak_ptr_factory_;

  TestEchoAIBaseOHOS(WebContents* web_contents,
                     base::OnceCallback<void(bool)> callback,
                     AISessionType type)
      : EchoAIBaseOHOS(web_contents, std::move(callback), type) {}

  ~TestEchoAIBaseOHOS() override = default;
};

class EchoAIBaseOHOSTest : public RenderViewHostTestHarness {
 protected:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    auto web_contents = TestWebContents::Create(browser_context(), nullptr);
    delegate_ = std::make_unique<MockWebContentsDelegate>();
    web_contents->SetDelegate(delegate_.get());
    SetContents(std::move(web_contents));
  }

  void TearDown() override {
    delegate_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<MockWebContentsDelegate> delegate_;
};

TEST_F(EchoAIBaseOHOSTest, Constructor) {
  bool callback_called = false;
  bool callback_result = false;

  TestEchoAIBaseOHOS base(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      AISessionType::SUMMARIZER);

  EXPECT_FALSE(callback_called);
  EXPECT_FALSE(callback_result);
}

TEST_F(EchoAIBaseOHOSTest, OnCreateAISessionWithDelegate) {
  bool callback_called = false;
  bool callback_result = false;

  TestEchoAIBaseOHOS base(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      AISessionType::WRITER);

  base.OnCreateAISession(kContent);

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_EQ(delegate_->type_, AISessionType::WRITER);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAIBaseOHOSTest, OnCreateAISessionWithoutDelegate) {
  web_contents()->SetDelegate(nullptr);
  bool callback_called = false;
  bool callback_result = false;

  TestEchoAIBaseOHOS base(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      AISessionType::REWRITER);

  base.OnCreateAISession(kContent);

  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(callback_result);
}

TEST_F(EchoAIBaseOHOSTest, HandleCreateSessionCallbackWithSuccess) {
  bool callback_called = false;
  bool callback_result = false;

  TestEchoAIBaseOHOS base(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      AISessionType::SUMMARIZER);

  base.HandleCreateSessionCallback(AISessionResultType::SUCCESS, kContent);

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAIBaseOHOSTest, HandleCreateSessionCallbackWithFailure) {
  bool callback_called = false;
  bool callback_result = false;

  TestEchoAIBaseOHOS base(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      AISessionType::SUMMARIZER);

  base.HandleCreateSessionCallback(AISessionResultType::FAILURE, kContent);

  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(callback_result);
}

TEST_F(EchoAIBaseOHOSTest, HandleCreateSessionCallbackOnlyOnce) {
  bool callback_called = false;
  bool callback_result = false;

  TestEchoAIBaseOHOS base(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      AISessionType::SUMMARIZER);

  base.HandleCreateSessionCallback(AISessionResultType::SUCCESS, kContent);
  base.HandleCreateSessionCallback(AISessionResultType::SUCCESS, kContent);

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAIBaseOHOSTest, OnExecuteAIActionWithDelegate) {
  MockModelStreamingResponder responder;
  bool callback_called = false;
  bool callback_result = false;

  TestEchoAIBaseOHOS base(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      AISessionType::TRANSLATOR);

  base.OnExecuteAIAction(kInput, responder.BindRemote());

  EXPECT_TRUE(delegate_->on_execute_ai_action_called_);
  EXPECT_EQ(delegate_->execute_type_, AISessionType::TRANSLATOR);
}

TEST_F(EchoAIBaseOHOSTest, OnExecuteAIActionWithoutDelegate) {
  web_contents()->SetDelegate(nullptr);
  MockModelStreamingResponder responder;
  bool callback_called = false;
  bool callback_result = false;

  TestEchoAIBaseOHOS base(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      AISessionType::TRANSLATOR);

  base.OnExecuteAIAction(kInput, responder.BindRemote());

  EXPECT_TRUE(responder.on_error_called_);
  EXPECT_EQ(responder.error_status_,
            blink::mojom::ModelStreamingResponseStatus::kErrorUnknown);
}

TEST_F(EchoAIBaseOHOSTest, HandleExecuteActionCallbackWithRunning) {
  MockModelStreamingResponder responder;
  bool callback_called = false;
  bool callback_result = false;

  TestEchoAIBaseOHOS base(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      AISessionType::SUMMARIZER);

  mojo::RemoteSetElementId responder_id =
      base.responder_set_.Add(responder.BindRemote());
  base.HandleExecuteActionCallback(responder_id, AISessionResultType::RUNNING,
                                   kContent);

  EXPECT_TRUE(responder.on_streaming_called_);
  EXPECT_EQ(responder.streaming_content_, kContent);
  EXPECT_FALSE(responder.on_completion_called_);
  EXPECT_FALSE(responder.on_error_called_);
}

TEST_F(EchoAIBaseOHOSTest, HandleExecuteActionCallbackWithSuccess) {
  MockModelStreamingResponder responder;
  bool callback_called = false;
  bool callback_result = false;

  TestEchoAIBaseOHOS base(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      AISessionType::SUMMARIZER);

  mojo::RemoteSetElementId responder_id =
      base.responder_set_.Add(responder.BindRemote());
  base.HandleExecuteActionCallback(responder_id, AISessionResultType::SUCCESS,
                                   kContent);

  EXPECT_TRUE(responder.on_streaming_called_);
  EXPECT_EQ(responder.streaming_content_, kContent);
  EXPECT_TRUE(responder.on_completion_called_);
  EXPECT_FALSE(responder.on_error_called_);
}

TEST_F(EchoAIBaseOHOSTest, HandleExecuteActionCallbackWithFailure) {
  MockModelStreamingResponder responder;
  bool callback_called = false;
  bool callback_result = false;

  TestEchoAIBaseOHOS base(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      AISessionType::SUMMARIZER);

  mojo::RemoteSetElementId responder_id =
      base.responder_set_.Add(responder.BindRemote());
  base.HandleExecuteActionCallback(responder_id, AISessionResultType::FAILURE,
                                   kContent);

  EXPECT_FALSE(responder.on_streaming_called_);
  EXPECT_FALSE(responder.on_completion_called_);
  EXPECT_TRUE(responder.on_error_called_);
  EXPECT_EQ(responder.error_status_,
            blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure);
}

TEST_F(EchoAIBaseOHOSTest, HandleExecuteActionCallbackWithInvalidResponder) {
  bool callback_called = false;
  bool callback_result = false;

  TestEchoAIBaseOHOS base(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      AISessionType::SUMMARIZER);

  base.HandleExecuteActionCallback(mojo::RemoteSetElementId(999),
                                   AISessionResultType::SUCCESS, kContent);
}

TEST_F(EchoAIBaseOHOSTest, DestructorCallsOnDestroyAISession) {
  bool callback_called = false;
  bool callback_result = false;
  std::string uuid;

  {
    TestEchoAIBaseOHOS base(
        web_contents(),
        base::BindOnce([](bool* called, bool* result, bool success) {
          *called = true;
          *result = success;
        }, &callback_called, &callback_result),
        AISessionType::PROOFREADER);
    uuid = base.uuid_;
  }

  EXPECT_TRUE(delegate_->on_destroy_ai_session_called_);
  EXPECT_EQ(delegate_->destroy_type_, AISessionType::PROOFREADER);
  EXPECT_EQ(delegate_->destroy_uuid_, uuid);
}

TEST_F(EchoAIBaseOHOSTest, DestructorWithoutDelegate) {
  bool callback_called = false;
  bool callback_result = false;

  {
    web_contents()->SetDelegate(nullptr);
    TestEchoAIBaseOHOS base(
        web_contents(),
        base::BindOnce([](bool* called, bool* result, bool success) {
          *called = true;
          *result = success;
        }, &callback_called, &callback_result),
        AISessionType::PROOFREADER);
  }

  EXPECT_FALSE(delegate_->on_destroy_ai_session_called_);
}

TEST_F(EchoAIBaseOHOSTest, GetDelegateWithWebContents) {
  bool callback_called = false;
  bool callback_result = false;

  TestEchoAIBaseOHOS base(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      AISessionType::SUMMARIZER);

  EXPECT_EQ(base.GetDelegate(), delegate_.get());
}

TEST_F(EchoAIBaseOHOSTest, GetDelegateWithoutWebContents) {
  bool callback_called = false;
  bool callback_result = false;

  TestEchoAIBaseOHOS base(
      nullptr,
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      AISessionType::SUMMARIZER);

  EXPECT_EQ(base.GetDelegate(), nullptr);
}

}  // namespace

}  // namespace content