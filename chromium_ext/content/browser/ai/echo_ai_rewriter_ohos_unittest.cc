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

#include "echo_ai_rewriter_ohos.h"

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
#include "third_party/blink/public/mojom/ai/ai_rewriter.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace content {

namespace {

using ::blink::mojom::AILanguageCode;
using ::blink::mojom::AILanguageCodePtr;
using ::blink::mojom::AIRewriterCreateOptions;
using ::blink::mojom::AIRewriterFormat;
using ::blink::mojom::AIRewriterLength;
using ::blink::mojom::AIRewriterTone;

constexpr char kSharedContext[] = "test shared context";
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
  }

  void OnError(blink::mojom::ModelStreamingResponseStatus status,
               blink::mojom::QuotaErrorInfoPtr quota_error_info) override {
    on_error_called_ = true;
    error_status_ = status;
  }

  bool on_completion_called_ = false;
  bool on_error_called_ = false;
  std::string streaming_content_;
  blink::mojom::ModelStreamingResponseStatus error_status_;

 private:
  mojo::Receiver<blink::mojom::ModelStreamingResponder> receiver_{this};
};

blink::mojom::AIRewriterCreateOptionsPtr CreateOptionsWithTone(AIRewriterTone tone) {
  return AIRewriterCreateOptions::New(
      kSharedContext, tone, AIRewriterFormat::kPlainText,
      AIRewriterLength::kShorter, std::vector<AILanguageCodePtr>(),
      std::vector<AILanguageCodePtr>(), AILanguageCode::New("en"));
}

blink::mojom::AIRewriterCreateOptionsPtr CreateOptionsWithFormat(AIRewriterFormat format) {
  return AIRewriterCreateOptions::New(
      kSharedContext, AIRewriterTone::kAsIs, format, AIRewriterLength::kShorter,
      std::vector<AILanguageCodePtr>(), std::vector<AILanguageCodePtr>(),
      AILanguageCode::New("en"));
}

blink::mojom::AIRewriterCreateOptionsPtr CreateOptionsWithLength(AIRewriterLength length) {
  return AIRewriterCreateOptions::New(
      kSharedContext, AIRewriterTone::kAsIs, AIRewriterFormat::kPlainText,
      length, std::vector<AILanguageCodePtr>(),
      std::vector<AILanguageCodePtr>(), AILanguageCode::New("en"));
}

blink::mojom::AIRewriterCreateOptionsPtr CreateOptionsWithLanguages() {
  std::vector<AILanguageCodePtr> input_langs;
  input_langs.push_back(AILanguageCode::New("en"));
  input_langs.push_back(AILanguageCode::New("zh"));

  std::vector<AILanguageCodePtr> context_langs;
  context_langs.push_back(AILanguageCode::New("fr"));
  context_langs.push_back(AILanguageCode::New("de"));

  return AIRewriterCreateOptions::New(
      kSharedContext, AIRewriterTone::kAsIs, AIRewriterFormat::kPlainText,
      AIRewriterLength::kShorter, std::move(input_langs),
      std::move(context_langs), AILanguageCode::New("es"));
}

blink::mojom::AIRewriterCreateOptionsPtr CreateOptionsWithoutSharedContext() {
  return AIRewriterCreateOptions::New(
      std::nullopt, AIRewriterTone::kAsIs, AIRewriterFormat::kPlainText,
      AIRewriterLength::kShorter, std::vector<AILanguageCodePtr>(),
      std::vector<AILanguageCodePtr>(), AILanguageCode::New("en"));
}

class EchoAIRewriterOHOSTest : public RenderViewHostTestHarness {
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

TEST_F(EchoAIRewriterOHOSTest, ConstructorWithAsIsTone) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithTone(AIRewriterTone::kAsIs);

  EchoAIRewriterOHOS rewriter(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAIRewriterOHOSTest, ConstructorWithMoreFormalTone) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithTone(AIRewriterTone::kMoreFormal);

  EchoAIRewriterOHOS rewriter(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAIRewriterOHOSTest, ConstructorWithMoreCasualTone) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithTone(AIRewriterTone::kMoreCasual);

  EchoAIRewriterOHOS rewriter(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAIRewriterOHOSTest, ConstructorWithAsIsFormat) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithFormat(AIRewriterFormat::kAsIs);

  EchoAIRewriterOHOS rewriter(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAIRewriterOHOSTest, ConstructorWithPlainTextFormat) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithFormat(AIRewriterFormat::kPlainText);

  EchoAIRewriterOHOS rewriter(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAIRewriterOHOSTest, ConstructorWithMarkdownFormat) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithFormat(AIRewriterFormat::kMarkdown);

  EchoAIRewriterOHOS rewriter(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAIRewriterOHOSTest, ConstructorWithAsIsLength) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithLength(AIRewriterLength::kAsIs);

  EchoAIRewriterOHOS rewriter(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAIRewriterOHOSTest, ConstructorWithShorterLength) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithLength(AIRewriterLength::kShorter);

  EchoAIRewriterOHOS rewriter(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAIRewriterOHOSTest, ConstructorWithLongerLength) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithLength(AIRewriterLength::kLonger);

  EchoAIRewriterOHOS rewriter(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAIRewriterOHOSTest, ConstructorWithLanguages) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithLanguages();

  EchoAIRewriterOHOS rewriter(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAIRewriterOHOSTest, ConstructorWithSharedContext) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithTone(AIRewriterTone::kAsIs);

  EchoAIRewriterOHOS rewriter(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAIRewriterOHOSTest, ConstructorWithoutSharedContext) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithoutSharedContext();

  EchoAIRewriterOHOS rewriter(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAIRewriterOHOSTest, ConstructorWithoutDelegate) {
  web_contents()->SetDelegate(nullptr);
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithTone(AIRewriterTone::kAsIs);

  EchoAIRewriterOHOS rewriter(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      std::move(options));

  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(callback_result);
}

TEST_F(EchoAIRewriterOHOSTest, ConstructorWithEmptyLanguages) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = AIRewriterCreateOptions::New(
      kSharedContext, AIRewriterTone::kAsIs, AIRewriterFormat::kPlainText,
      AIRewriterLength::kShorter, std::vector<AILanguageCodePtr>(),
      std::vector<AILanguageCodePtr>(), AILanguageCode::New("en"));

  EchoAIRewriterOHOS rewriter(
      web_contents(),
      base::BindOnce([](bool* called, bool* result, bool success) {
        *called = true;
        *result = success;
      }, &callback_called, &callback_result),
      std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

}  // namespace

}  // namespace content