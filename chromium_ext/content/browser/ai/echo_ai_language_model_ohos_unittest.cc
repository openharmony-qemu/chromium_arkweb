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

#include "echo_ai_language_model_ohos.h"

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
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace content {

namespace {

using ::blink::mojom::AILanguageCode;
using ::blink::mojom::AILanguageCodePtr;
using ::blink::mojom::AILanguageModelPrompt;
using ::blink::mojom::AILanguageModelPromptContent;
using ::blink::mojom::AILanguageModelPromptRole;
using ::blink::mojom::AILanguageModelPromptType;
using ::blink::mojom::AILanguageModelSamplingParams;
using ::blink::mojom::AILanguageModelExpected;
using ::blink::mojom::AILanguageModelPromptPtr;
using ::blink::mojom::AILanguageModelPromptContentPtr;
using ::blink::mojom::AILanguageModelExpectedPtr;

constexpr char kInput[] = "test input";
constexpr char kContext[] = "test context";

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
      std::move(callback).Run(AISessionResultType::SUCCESS, "");
    }
  }

  void OnDestroyAISession(AISessionType type, const std::string& uuid) override {
    on_destroy_ai_session_called_ = true;
    destroy_type_ = type;
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
  }

  void OnCompletion(
      blink::mojom::ModelExecutionContextInfoPtr context_info) override {
    on_completion_called_ = true;
    if (context_info) {
      context_info_ = context_info.Clone();
    }
  }

  void OnError(blink::mojom::ModelStreamingResponseStatus status,
               blink::mojom::QuotaErrorInfoPtr quota_error_info) override {
    on_error_called_ = true;
    error_status_ = status;
  }

  void OnQuotaOverflow() override {}

  bool on_completion_called_ = false;
  bool on_error_called_ = false;
  std::string streaming_content_;
  blink::mojom::ModelStreamingResponseStatus error_status_;
  blink::mojom::ModelExecutionContextInfoPtr context_info_;

 private:
  mojo::Receiver<blink::mojom::ModelStreamingResponder> receiver_{this};
};

class MockAIManagerCreateLanguageModelClient
    : public blink::mojom::AIManagerCreateLanguageModelClient {
 public:
  MockAIManagerCreateLanguageModelClient() = default;
  ~MockAIManagerCreateLanguageModelClient() override = default;

  mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
  BindRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnResult(
      mojo::PendingRemote<blink::mojom::AILanguageModel> language_model_remote,
      blink::mojom::AILanguageModelInstanceInfoPtr info) override {
    on_result_called_ = true;
    if (info) {
      info_ = info.Clone();
    }
  }

  void OnError(blink::mojom::AIManagerCreateClientError error,
               blink::mojom::QuotaErrorInfoPtr quota_error_info) override {
    on_error_called_ = true;
    error_ = error;
  }

  bool on_result_called_ = false;
  bool on_error_called_ = false;
  blink::mojom::AIManagerCreateClientError error_;
  blink::mojom::AILanguageModelInstanceInfoPtr info_;

 private:
  mojo::Receiver<blink::mojom::AIManagerCreateLanguageModelClient> receiver_{
      this};
};

AILanguageModelPromptPtr CreateTextPrompt(AILanguageModelPromptRole role,
                                          const std::string& text) {
  std::vector<AILanguageModelPromptContentPtr> contents;
  contents.push_back(AILanguageModelPromptContent::NewText(text));
  return AILanguageModelPrompt::New(role, std::move(contents), false);
}

}  // namespace

class EchoAILanguageModelOHOSTest : public RenderViewHostTestHarness {
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

  void DestroyModel(EchoAILanguageModelOHOS& model) {
    model.Destroy();
  }

  void CallPrompt(
      EchoAILanguageModelOHOS& model,
      std::vector<AILanguageModelPromptPtr> prompts,
      on_device_model::mojom::ResponseConstraintPtr constraint,
      mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
          pending_responder) {
    model.Prompt(std::move(prompts), std::move(constraint),
                 std::move(pending_responder));
  }

  void CallAppend(
      EchoAILanguageModelOHOS& model,
      std::vector<AILanguageModelPromptPtr> prompts,
      mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
          pending_responder) {
    model.Append(std::move(prompts), std::move(pending_responder));
  }

  void CallFork(
      EchoAILanguageModelOHOS& model,
      mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
          client) {
    model.Fork(std::move(client));
  }

  void CallMeasureInputUsage(
      EchoAILanguageModelOHOS& model,
      std::vector<AILanguageModelPromptPtr> input,
      base::OnceCallback<void(std::optional<uint32_t>)> callback) {
    model.MeasureInputUsage(std::move(input), std::move(callback));
  }

  std::unique_ptr<MockWebContentsDelegate> delegate_;
};

TEST_F(EchoAILanguageModelOHOSTest, ConstructorWithSamplingParams) {
  bool callback_called = false;
  bool callback_result = false;
  auto sampling_params = AILanguageModelSamplingParams::New(1, 0.5f);

  EchoAILanguageModelOHOS model(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      std::move(sampling_params), base::flat_set<AILanguageModelPromptType>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelPromptPtr>(), 0);

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAILanguageModelOHOSTest, ConstructorWithoutSamplingParams) {
  bool callback_called = false;
  bool callback_result = false;

  EchoAILanguageModelOHOS model(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      nullptr, base::flat_set<AILanguageModelPromptType>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelPromptPtr>(), 0);

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAILanguageModelOHOSTest, ConstructorWithInitialPrompts) {
  bool callback_called = false;
  bool callback_result = false;

  std::vector<AILanguageModelPromptPtr> initial_prompts;
  initial_prompts.push_back(CreateTextPrompt(AILanguageModelPromptRole::kSystem,
                                             "You are a helpful assistant"));
  initial_prompts.push_back(
      CreateTextPrompt(AILanguageModelPromptRole::kUser, "Hello"));

  EchoAILanguageModelOHOS model(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      nullptr, base::flat_set<AILanguageModelPromptType>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelExpectedPtr>(), std::move(initial_prompts), 0);

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAILanguageModelOHOSTest, ConstructorWithoutDelegate) {
  web_contents()->SetDelegate(nullptr);
  bool callback_called = false;
  bool callback_result = false;

  EchoAILanguageModelOHOS model(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      nullptr, base::flat_set<AILanguageModelPromptType>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelPromptPtr>(), 0);

  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(callback_result);
}

TEST_F(EchoAILanguageModelOHOSTest, Prompt) {
  bool callback_called = false;
  bool callback_result = false;

  EchoAILanguageModelOHOS model(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      nullptr, base::flat_set<AILanguageModelPromptType>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelPromptPtr>(), 0);

  std::vector<AILanguageModelPromptPtr> prompts;
  prompts.push_back(
      CreateTextPrompt(AILanguageModelPromptRole::kUser, "Hello"));

  MockModelStreamingResponder responder;
  CallPrompt(model, std::move(prompts), nullptr, responder.BindRemote());

  EXPECT_TRUE(delegate_->on_execute_ai_action_called_);
  EXPECT_EQ(delegate_->execute_type_, AISessionType::PROMPT);
}

TEST_F(EchoAILanguageModelOHOSTest, PromptWithHistory) {
  bool callback_called = false;
  bool callback_result = false;

  std::vector<AILanguageModelPromptPtr> initial_prompts;
  initial_prompts.push_back(CreateTextPrompt(AILanguageModelPromptRole::kSystem,
                                             "You are a helpful assistant"));

  EchoAILanguageModelOHOS model(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      nullptr, base::flat_set<AILanguageModelPromptType>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelExpectedPtr>(), std::move(initial_prompts), 0);

  std::vector<AILanguageModelPromptPtr> prompts;
  prompts.push_back(
      CreateTextPrompt(AILanguageModelPromptRole::kUser, "Hello"));

  MockModelStreamingResponder responder;
  CallPrompt(model, std::move(prompts), nullptr, responder.BindRemote());

  EXPECT_TRUE(delegate_->on_execute_ai_action_called_);
}

TEST_F(EchoAILanguageModelOHOSTest, PromptAfterDestroy) {
  bool callback_called = false;
  bool callback_result = false;

  EchoAILanguageModelOHOS model(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      nullptr, base::flat_set<AILanguageModelPromptType>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelPromptPtr>(), 0);

  DestroyModel(model);

  std::vector<AILanguageModelPromptPtr> prompts;
  prompts.push_back(
      CreateTextPrompt(AILanguageModelPromptRole::kUser, "Hello"));

  MockModelStreamingResponder responder;
  CallPrompt(model, std::move(prompts), nullptr, responder.BindRemote());

  EXPECT_TRUE(responder.on_error_called_);
  EXPECT_EQ(responder.error_status_,
            blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
}

TEST_F(EchoAILanguageModelOHOSTest, Append) {
  bool callback_called = false;
  bool callback_result = false;

  EchoAILanguageModelOHOS model(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      nullptr, base::flat_set<AILanguageModelPromptType>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelPromptPtr>(), 100);

  std::vector<AILanguageModelPromptPtr> prompts;
  prompts.push_back(
      CreateTextPrompt(AILanguageModelPromptRole::kUser, "Hello"));

  MockModelStreamingResponder responder;
  CallAppend(model, std::move(prompts), responder.BindRemote());

  EXPECT_FALSE(responder.on_completion_called_);
  EXPECT_FALSE(responder.context_info_);
}

TEST_F(EchoAILanguageModelOHOSTest, Fork) {
  bool callback_called = false;
  bool callback_result = false;

  EchoAILanguageModelOHOS model(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      nullptr, base::flat_set<AILanguageModelPromptType>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelPromptPtr>(), 50);

  MockAIManagerCreateLanguageModelClient client;
  CallFork(model, client.BindRemote());

  EXPECT_TRUE(client.on_result_called_);
  EXPECT_TRUE(client.info_);
}

TEST_F(EchoAILanguageModelOHOSTest, Destroy) {
  bool callback_called = false;
  bool callback_result = false;

  EchoAILanguageModelOHOS model(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      nullptr, base::flat_set<AILanguageModelPromptType>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelPromptPtr>(), 0);

  MockModelStreamingResponder responder1;
  std::vector<AILanguageModelPromptPtr> test;
  test.push_back(
      CreateTextPrompt(AILanguageModelPromptRole::kUser, "test"));
  CallPrompt(model, std::move(test),
      nullptr, responder1.BindRemote());

  MockModelStreamingResponder responder2;
  std::vector<AILanguageModelPromptPtr> test2;
  test2.push_back(
      CreateTextPrompt(AILanguageModelPromptRole::kUser, "test2"));
  CallPrompt(model, std::move(test2),
      nullptr, responder2.BindRemote());

  DestroyModel(model);

  EXPECT_TRUE(responder1.on_error_called_);
  EXPECT_TRUE(responder2.on_error_called_);
  EXPECT_EQ(responder1.error_status_,
            blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
  EXPECT_EQ(responder2.error_status_,
            blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
}

TEST_F(EchoAILanguageModelOHOSTest, MeasureInputUsage) {
  bool callback_called = false;
  bool callback_result = false;

  EchoAILanguageModelOHOS model(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      nullptr, base::flat_set<AILanguageModelPromptType>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelPromptPtr>(), 0);

  std::vector<AILanguageModelPromptPtr> input;
  input.push_back(
      CreateTextPrompt(AILanguageModelPromptRole::kUser, "Hello World"));

  uint32_t measured_usage = 0;
  CallMeasureInputUsage(
      model, std::move(input),
      base::BindOnce(
          [](uint32_t* output, std::optional<uint32_t> usage) {
            *output = usage.value_or(0);
          },
          &measured_usage));

  EXPECT_EQ(measured_usage, 11u);
}

}  // namespace content