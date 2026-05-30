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

#include "echo_ai_manager_ohos.h"

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
#include "third_party/blink/public/mojom/ai/ai_summarizer.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_writer.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_rewriter.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_proofreader.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace content {

namespace {

using ::blink::mojom::AILanguageCode;
using ::blink::mojom::AILanguageCodePtr;
using ::blink::mojom::AILanguageModelCreateOptions;
using ::blink::mojom::AILanguageModelExpected;
using ::blink::mojom::AILanguageModelPrompt;
using ::blink::mojom::AILanguageModelPromptContent;
using ::blink::mojom::AILanguageModelPromptRole;
using ::blink::mojom::AILanguageModelPromptType;
using ::blink::mojom::AILanguageModelSamplingParams;
using ::blink::mojom::AILanguageModelPromptPtr;
using ::blink::mojom::AILanguageModelPromptContentPtr;
using ::blink::mojom::AILanguageModelExpectedPtr;

constexpr char kInput[] = "test input";

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
  AISessionType destroy_type_;
  std::string uuid_;
  std::string input_;
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

class MockModelDownloadProgressObserver
    : public blink::mojom::ModelDownloadProgressObserver {
 public:
  MockModelDownloadProgressObserver() = default;
  ~MockModelDownloadProgressObserver() override = default;

  mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver> BindRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnDownloadProgressUpdate(uint64_t bytes_downloaded,
                                uint64_t total_bytes_to_download) override {
    download_progress_update_count_++;
    last_bytes_downloaded_ = bytes_downloaded;
    last_total_bytes_to_download_ = total_bytes_to_download;
  }

  int download_progress_update_count_ = 0;
  uint64_t last_bytes_downloaded_ = 0;
  uint64_t last_total_bytes_to_download_ = 0;

 private:
  mojo::Receiver<blink::mojom::ModelDownloadProgressObserver> receiver_{this};
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
    if (quota_error_info) {
      quota_error_info_ = quota_error_info.Clone();
    }
  }

  bool on_result_called_ = false;
  bool on_error_called_ = false;
  blink::mojom::AIManagerCreateClientError error_;
  blink::mojom::QuotaErrorInfoPtr quota_error_info_;
  blink::mojom::AILanguageModelInstanceInfoPtr info_;

 private:
  mojo::Receiver<blink::mojom::AIManagerCreateLanguageModelClient> receiver_{
      this};
};

class MockAIManagerCreateSummarizerClient
    : public blink::mojom::AIManagerCreateSummarizerClient {
 public:
  MockAIManagerCreateSummarizerClient() = default;
  ~MockAIManagerCreateSummarizerClient() override = default;

  mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient>
  BindRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnResult(
      mojo::PendingRemote<blink::mojom::AISummarizer> summarizer_remote)
      override {
    on_result_called_ = true;
  }

  void OnError(blink::mojom::AIManagerCreateClientError error,
               blink::mojom::QuotaErrorInfoPtr quota_error_info) override {
    on_error_called_ = true;
    error_ = error;
  }

  bool on_result_called_ = false;
  bool on_error_called_ = false;
  blink::mojom::AIManagerCreateClientError error_;

 private:
  mojo::Receiver<blink::mojom::AIManagerCreateSummarizerClient> receiver_{this};
};

class MockAIManagerCreateWriterClient
    : public blink::mojom::AIManagerCreateWriterClient {
 public:
  MockAIManagerCreateWriterClient() = default;
  ~MockAIManagerCreateWriterClient() override = default;

  mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient> BindRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnResult(mojo::PendingRemote<blink::mojom::AIWriter> writer_remote)
      override {
    on_result_called_ = true;
  }

  void OnError(blink::mojom::AIManagerCreateClientError error,
               blink::mojom::QuotaErrorInfoPtr quota_error_info) override {
    on_error_called_ = true;
    error_ = error;
  }

  bool on_result_called_ = false;
  bool on_error_called_ = false;
  blink::mojom::AIManagerCreateClientError error_;

 private:
  mojo::Receiver<blink::mojom::AIManagerCreateWriterClient> receiver_{this};
};

class MockAIManagerCreateRewriterClient
    : public blink::mojom::AIManagerCreateRewriterClient {
 public:
  MockAIManagerCreateRewriterClient() = default;
  ~MockAIManagerCreateRewriterClient() override = default;

  mojo::PendingRemote<blink::mojom::AIManagerCreateRewriterClient> BindRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnResult(mojo::PendingRemote<blink::mojom::AIRewriter> rewriter_remote)
      override {
    on_result_called_ = true;
  }

  void OnError(blink::mojom::AIManagerCreateClientError error,
               blink::mojom::QuotaErrorInfoPtr quota_error_info) override {
    on_error_called_ = true;
    error_ = error;
  }

  bool on_result_called_ = false;
  bool on_error_called_ = false;
  blink::mojom::AIManagerCreateClientError error_;

 private:
  mojo::Receiver<blink::mojom::AIManagerCreateRewriterClient> receiver_{this};
};

class MockAIManagerCreateProofreaderClient
    : public blink::mojom::AIManagerCreateProofreaderClient {
 public:
  MockAIManagerCreateProofreaderClient() = default;
  ~MockAIManagerCreateProofreaderClient() override = default;

  mojo::PendingRemote<blink::mojom::AIManagerCreateProofreaderClient>
  BindRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnResult(
      mojo::PendingRemote<blink::mojom::AIProofreader> proofreader_remote)
      override {
    on_result_called_ = true;
  }

  void OnError(blink::mojom::AIManagerCreateClientError error,
               blink::mojom::QuotaErrorInfoPtr quota_error_info) override {
    on_error_called_ = true;
    error_ = error;
  }

  bool on_result_called_ = false;
  bool on_error_called_ = false;
  blink::mojom::AIManagerCreateClientError error_;

 private:
  mojo::Receiver<blink::mojom::AIManagerCreateProofreaderClient> receiver_{
      this};
};

class MockTranslationManagerCreateTranslatorClient
    : public blink::mojom::TranslationManagerCreateTranslatorClient {
 public:
  MockTranslationManagerCreateTranslatorClient() = default;
  ~MockTranslationManagerCreateTranslatorClient() override = default;

  mojo::PendingRemote<blink::mojom::TranslationManagerCreateTranslatorClient>
  BindRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnResult(blink::mojom::CreateTranslatorResultPtr result,
                blink::mojom::TranslatorLanguageCodePtr source_lang,
                blink::mojom::TranslatorLanguageCodePtr target_lang) override {
    on_result_called_ = true;
    result_ = std::move(result);
  }

  bool on_result_called_ = false;
  blink::mojom::CreateTranslatorResultPtr result_;

 private:
  mojo::Receiver<blink::mojom::TranslationManagerCreateTranslatorClient>
      receiver_{this};
};

AILanguageModelPromptPtr CreateTextPrompt(AILanguageModelPromptRole role,
                                          const std::string& text) {
  std::vector<AILanguageModelPromptContentPtr> contents;
  contents.push_back(AILanguageModelPromptContent::NewText(text));
  return AILanguageModelPrompt::New(role, std::move(contents), false);
}

}  // namespace

class EchoAIManagerOHOSTest : public RenderViewHostTestHarness {
 protected:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    auto web_contents = TestWebContents::Create(browser_context(), nullptr);
    delegate_ = std::make_unique<MockWebContentsDelegate>();
    web_contents->SetDelegate(delegate_.get());
    EchoAIManagerOHOS::CreateForWebContents(web_contents.get());
    manager_ = EchoAIManagerOHOS::FromWebContents(web_contents.get());
    SetContents(std::move(web_contents));
  }

  void TearDown() override {
    task_environment()->RunUntilIdle();
    delegate_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<MockWebContentsDelegate> delegate_;
  raw_ptr<EchoAIManagerOHOS> manager_ = nullptr;

  void AddReceiver(mojo::PendingReceiver<blink::mojom::AIManager> receiver) {
    manager_->AddReceiver(std::move(receiver));
  }

  void AddReceiver(mojo::PendingReceiver<blink::mojom::TranslationManager> receiver) {
    manager_->AddReceiver(std::move(receiver));
  }

  void CanCreateLanguageModel(
      blink::mojom::AILanguageModelCreateOptionsPtr options,
      base::OnceCallback<void(blink::mojom::ModelAvailabilityCheckResult)> callback) {
    manager_->CanCreateLanguageModel(std::move(options), std::move(callback));
  }

  void CreateLanguageModel(
      mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient> client,
      blink::mojom::AILanguageModelCreateOptionsPtr options) {
    manager_->CreateLanguageModel(std::move(client), std::move(options));
    task_environment()->RunUntilIdle();
  }

  void CanCreateSummarizer(
      blink::mojom::AISummarizerCreateOptionsPtr options,
      base::OnceCallback<void(blink::mojom::ModelAvailabilityCheckResult)> callback) {
    manager_->CanCreateSummarizer(std::move(options), std::move(callback));
  }

  void CreateSummarizer(
      mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient> client,
      blink::mojom::AISummarizerCreateOptionsPtr options) {
    manager_->CreateSummarizer(std::move(client), std::move(options));
  }

  void GetLanguageModelParams(
      base::OnceCallback<void(blink::mojom::AILanguageModelParamsPtr)> callback) {
    manager_->GetLanguageModelParams(std::move(callback));
  }

  void CanCreateWriter(
      blink::mojom::AIWriterCreateOptionsPtr options,
      base::OnceCallback<void(blink::mojom::ModelAvailabilityCheckResult)> callback) {
    manager_->CanCreateWriter(std::move(options), std::move(callback));
  }

  void CreateWriter(
      mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient> client,
      blink::mojom::AIWriterCreateOptionsPtr options) {
    manager_->CreateWriter(std::move(client), std::move(options));
  }

  void CanCreateRewriter(
      blink::mojom::AIRewriterCreateOptionsPtr options,
      base::OnceCallback<void(blink::mojom::ModelAvailabilityCheckResult)> callback) {
    manager_->CanCreateRewriter(std::move(options), std::move(callback));
  }

  void CreateRewriter(
      mojo::PendingRemote<blink::mojom::AIManagerCreateRewriterClient> client,
      blink::mojom::AIRewriterCreateOptionsPtr options) {
    manager_->CreateRewriter(std::move(client), std::move(options));
  }

  void CanCreateProofreader(
      blink::mojom::AIProofreaderCreateOptionsPtr options,
      base::OnceCallback<void(blink::mojom::ModelAvailabilityCheckResult)> callback) {
    manager_->CanCreateProofreader(std::move(options), std::move(callback));
  }

  void CreateProofreader(
      mojo::PendingRemote<blink::mojom::AIManagerCreateProofreaderClient> client,
      blink::mojom::AIProofreaderCreateOptionsPtr options) {
    manager_->CreateProofreader(std::move(client), std::move(options));
  }

  void AddModelDownloadProgressObserver(
      mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver> observer_remote) {
    manager_->AddModelDownloadProgressObserver(std::move(observer_remote));
  }

  void CreateTranslator(
      mojo::PendingRemote<blink::mojom::TranslationManagerCreateTranslatorClient> client,
      blink::mojom::TranslatorCreateOptionsPtr options,
      bool streaming) {
    manager_->CreateTranslator(std::move(client), std::move(options), streaming);
  }

  void TranslationAvailable(
      blink::mojom::TranslatorLanguageCodePtr source_lang,
      blink::mojom::TranslatorLanguageCodePtr target_lang,
      base::OnceCallback<void(blink::mojom::CanCreateTranslatorResult)> callback) {
    manager_->TranslationAvailable(std::move(source_lang), std::move(target_lang), std::move(callback));
  }

  void CreateLanguageDetector(
      base::OnceCallback<void(mojo::PendingRemote<blink::mojom::AILanguageDetector>)> callback) {
    manager_->CreateLanguageDetector(std::move(callback));
  }

  bool IsModelDownloadedForCurrentReciever() {
    return manager_->IsModelDownloadedForCurrentReciever();
  }

  void DoMockDownloadingAndReturn(base::OnceClosure callback) {
    manager_->DoMockDownloadingAndReturn(std::move(callback));
  }
};

TEST_F(EchoAIManagerOHOSTest, AddReceiver) {
  mojo::PendingRemote<blink::mojom::AIManager> remote;
  AddReceiver(remote.InitWithNewPipeAndPassReceiver());
}

TEST_F(EchoAIManagerOHOSTest, AddTranslationReceiver) {
  mojo::PendingRemote<blink::mojom::TranslationManager> remote;
  AddReceiver(remote.InitWithNewPipeAndPassReceiver());
}

TEST_F(EchoAIManagerOHOSTest, CanCreateLanguageModelWithNullOptions) {
  bool callback_called = false;
  blink::mojom::ModelAvailabilityCheckResult result;

  CanCreateLanguageModel(
      nullptr, base::BindOnce(
                   [](bool* called, blink::mojom::ModelAvailabilityCheckResult* out,
                      blink::mojom::ModelAvailabilityCheckResult result) {
                     *called = true;
                     *out = result;
                   },
                   &callback_called, &result));

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(result, blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
}

TEST_F(EchoAIManagerOHOSTest, CanCreateLanguageModelWithDownloadedModel) {
  bool callback_called = false;
  blink::mojom::ModelAvailabilityCheckResult result;

  mojo::PendingRemote<blink::mojom::AIManager> remote;
  AddReceiver(remote.InitWithNewPipeAndPassReceiver());

  auto options = AILanguageModelCreateOptions::New(
      nullptr, std::vector<AILanguageModelPromptPtr>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelExpectedPtr>());

  MockAIManagerCreateLanguageModelClient client;
  CreateLanguageModel(client.BindRemote(), std::move(options));

  CanCreateLanguageModel(
      nullptr, base::BindOnce(
                   [](bool* called, blink::mojom::ModelAvailabilityCheckResult* out,
                      blink::mojom::ModelAvailabilityCheckResult result) {
                     *called = true;
                     *out = result;
                   },
                   &callback_called, &result));

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(result, blink::mojom::ModelAvailabilityCheckResult::kAvailable);
}

TEST_F(EchoAIManagerOHOSTest, CanCreateLanguageModelWithTextExpectedInput) {
  bool callback_called = false;
  blink::mojom::ModelAvailabilityCheckResult result;

  std::vector<AILanguageCodePtr> codes;
  codes.push_back(AILanguageCode::New("en"));
  std::vector<AILanguageModelExpectedPtr> expected_inputs;
  expected_inputs.push_back(AILanguageModelExpected::New(
      AILanguageModelPromptType::kText,
      std::move(codes)));
  auto options = AILanguageModelCreateOptions::New(
      nullptr, std::vector<AILanguageModelPromptPtr>(),
      std::move(expected_inputs),
      std::vector<AILanguageModelExpectedPtr>());

  CanCreateLanguageModel(
      std::move(options),
      base::BindOnce(
          [](bool* called, blink::mojom::ModelAvailabilityCheckResult* out,
             blink::mojom::ModelAvailabilityCheckResult result) {
            *called = true;
            *out = result;
          },
          &callback_called, &result));

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(result, blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
}

TEST_F(EchoAIManagerOHOSTest, CreateLanguageModelSuccess) {
  auto options = AILanguageModelCreateOptions::New(
      nullptr, std::vector<AILanguageModelPromptPtr>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelExpectedPtr>());

  MockAIManagerCreateLanguageModelClient client;
  CreateLanguageModel(client.BindRemote(), std::move(options));

  EXPECT_TRUE(client.on_result_called_ || client.on_error_called_);
}

TEST_F(EchoAIManagerOHOSTest, CreateLanguageModelWithInitialPrompts) {
  std::vector<AILanguageModelPromptPtr> initial_prompts;
  initial_prompts.push_back(
      CreateTextPrompt(AILanguageModelPromptRole::kUser, "Hello"));

  auto options = AILanguageModelCreateOptions::New(
      nullptr, std::move(initial_prompts),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelExpectedPtr>());

  MockAIManagerCreateLanguageModelClient client;
  CreateLanguageModel(client.BindRemote(), std::move(options));

  EXPECT_TRUE(client.on_result_called_ || client.on_error_called_);
  if (client.on_result_called_) {
    EXPECT_TRUE(client.info_);
  }
}

TEST_F(EchoAIManagerOHOSTest, CreateLanguageModelWithInitialPromptsExceedingContextSize) {
  std::string large_text(EchoAIManagerOHOS::kMaxContextSizeInTokens + 100, 'x');
  std::vector<AILanguageModelPromptPtr> initial_prompts;
  initial_prompts.push_back(CreateTextPrompt(AILanguageModelPromptRole::kUser, large_text));

  auto options = AILanguageModelCreateOptions::New(
      nullptr, std::move(initial_prompts),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelExpectedPtr>());

  MockAIManagerCreateLanguageModelClient client;
  CreateLanguageModel(client.BindRemote(), std::move(options));

  EXPECT_TRUE(client.on_error_called_);
  EXPECT_EQ(client.error_,
            blink::mojom::AIManagerCreateClientError::kInitialInputTooLarge);
  EXPECT_TRUE(client.quota_error_info_);
}

TEST_F(EchoAIManagerOHOSTest, CanCreateSummarizerWithSupportedLanguages) {
  bool callback_called = false;
  blink::mojom::ModelAvailabilityCheckResult result;

  std::vector<AILanguageCodePtr> codes1;
  codes1.push_back(AILanguageCode::New("en"));
  std::vector<AILanguageCodePtr> codes2;
  codes2.push_back(AILanguageCode::New("en"));
  auto options = blink::mojom::AISummarizerCreateOptions::New(
      std::nullopt, blink::mojom::AISummarizerType::kTLDR,
      blink::mojom::AISummarizerFormat::kPlainText,
      blink::mojom::AISummarizerLength::kShort,
      std::move(codes1), std::move(codes2),
      AILanguageCode::New("en"));

  CanCreateSummarizer(
      std::move(options),
      base::BindOnce(
          [](bool* called, blink::mojom::ModelAvailabilityCheckResult* out,
             blink::mojom::ModelAvailabilityCheckResult result) {
            *called = true;
            *out = result;
          },
          &callback_called, &result));

  EXPECT_TRUE(callback_called);
}

TEST_F(EchoAIManagerOHOSTest, CanCreateSummarizerWithNullOptions) {
  bool callback_called = false;
  blink::mojom::ModelAvailabilityCheckResult result;

  CanCreateSummarizer(
      nullptr,
      base::BindOnce(
          [](bool* called, blink::mojom::ModelAvailabilityCheckResult* out,
             blink::mojom::ModelAvailabilityCheckResult result) {
            *called = true;
            *out = result;
          },
          &callback_called, &result));

  EXPECT_TRUE(callback_called);
}

TEST_F(EchoAIManagerOHOSTest, CreateSummarizerSuccess) {
  auto options = blink::mojom::AISummarizerCreateOptions::New(
      std::nullopt, blink::mojom::AISummarizerType::kTLDR,
      blink::mojom::AISummarizerFormat::kPlainText,
      blink::mojom::AISummarizerLength::kShort,
      std::vector<AILanguageCodePtr>(), std::vector<AILanguageCodePtr>(),
      AILanguageCode::New("en"));

  MockAIManagerCreateSummarizerClient client;
  CreateSummarizer(client.BindRemote(), std::move(options));

  EXPECT_TRUE(client.on_result_called_ || client.on_error_called_);
}

TEST_F(EchoAIManagerOHOSTest, CanCreateWriterWithSupportedLanguages) {
  bool callback_called = false;
  blink::mojom::ModelAvailabilityCheckResult result;

  auto options = blink::mojom::AIWriterCreateOptions::New(
      std::nullopt, blink::mojom::AIWriterTone::kNeutral,
      blink::mojom::AIWriterFormat::kPlainText, blink::mojom::AIWriterLength::kMedium,
      std::vector<AILanguageCodePtr>(), std::vector<AILanguageCodePtr>(),
      AILanguageCode::New("en"));

  CanCreateWriter(
      std::move(options),
      base::BindOnce(
          [](bool* called, blink::mojom::ModelAvailabilityCheckResult* out,
             blink::mojom::ModelAvailabilityCheckResult result) {
            *called = true;
            *out = result;
          },
          &callback_called, &result));

  EXPECT_TRUE(callback_called);
}

TEST_F(EchoAIManagerOHOSTest, CreateWriterSuccess) {
  auto options = blink::mojom::AIWriterCreateOptions::New(
      std::nullopt, blink::mojom::AIWriterTone::kNeutral,
      blink::mojom::AIWriterFormat::kPlainText, blink::mojom::AIWriterLength::kMedium,
      std::vector<AILanguageCodePtr>(), std::vector<AILanguageCodePtr>(),
      AILanguageCode::New("en"));

  MockAIManagerCreateWriterClient client;
  CreateWriter(client.BindRemote(), std::move(options));

  EXPECT_TRUE(client.on_result_called_ || client.on_error_called_);
}

TEST_F(EchoAIManagerOHOSTest, CanCreateRewriterWithSupportedLanguages) {
  bool callback_called = false;
  blink::mojom::ModelAvailabilityCheckResult result;

  auto options = blink::mojom::AIRewriterCreateOptions::New(
      std::nullopt, blink::mojom::AIRewriterTone::kAsIs,
      blink::mojom::AIRewriterFormat::kPlainText, blink::mojom::AIRewriterLength::kAsIs,
      std::vector<AILanguageCodePtr>(), std::vector<AILanguageCodePtr>(),
      AILanguageCode::New("en"));

  CanCreateRewriter(
      std::move(options),
      base::BindOnce(
          [](bool* called, blink::mojom::ModelAvailabilityCheckResult* out,
             blink::mojom::ModelAvailabilityCheckResult result) {
            *called = true;
            *out = result;
          },
          &callback_called, &result));

  EXPECT_TRUE(callback_called);
}

TEST_F(EchoAIManagerOHOSTest, CreateRewriterSuccess) {
  auto options = blink::mojom::AIRewriterCreateOptions::New(
      std::nullopt, blink::mojom::AIRewriterTone::kAsIs,
      blink::mojom::AIRewriterFormat::kPlainText, blink::mojom::AIRewriterLength::kAsIs,
      std::vector<AILanguageCodePtr>(), std::vector<AILanguageCodePtr>(),
      AILanguageCode::New("en"));

  MockAIManagerCreateRewriterClient client;
  CreateRewriter(client.BindRemote(), std::move(options));

  EXPECT_TRUE(client.on_result_called_ || client.on_error_called_);
}

TEST_F(EchoAIManagerOHOSTest, CanCreateProofreaderWithSupportedLanguages) {
  bool callback_called = false;
  blink::mojom::ModelAvailabilityCheckResult result;

  std::vector<AILanguageCodePtr> codes;
  codes.push_back(AILanguageCode::New("en"));
  auto options = blink::mojom::AIProofreaderCreateOptions::New(
      true, true, AILanguageCode::New("en"), std::move(codes));

  CanCreateProofreader(
      std::move(options),
      base::BindOnce(
          [](bool* called, blink::mojom::ModelAvailabilityCheckResult* out,
             blink::mojom::ModelAvailabilityCheckResult result) {
            *called = true;
            *out = result;
          },
          &callback_called, &result));

  EXPECT_TRUE(callback_called);
}

TEST_F(EchoAIManagerOHOSTest, CreateProofreaderSuccess) {
  std::vector<AILanguageCodePtr> codes;
  codes.push_back(AILanguageCode::New("en"));
  auto options = blink::mojom::AIProofreaderCreateOptions::New(
      true, true, AILanguageCode::New("en"),
      std::move(codes));

  MockAIManagerCreateProofreaderClient client;
  CreateProofreader(client.BindRemote(), std::move(options));

  EXPECT_TRUE(client.on_result_called_ || client.on_error_called_);
}

TEST_F(EchoAIManagerOHOSTest, GetLanguageModelParams) {
  bool callback_called = false;
  blink::mojom::AILanguageModelParamsPtr params;

  GetLanguageModelParams(
      base::BindOnce(
          [](bool* called, blink::mojom::AILanguageModelParamsPtr* out,
             blink::mojom::AILanguageModelParamsPtr result) {
            *called = true;
            *out = std::move(result);
          },
          &callback_called, &params));

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(params);
  EXPECT_TRUE(params->default_sampling_params);
  EXPECT_TRUE(params->max_sampling_params);
}

TEST_F(EchoAIManagerOHOSTest, AddModelDownloadProgressObserver) {
  MockModelDownloadProgressObserver observer;
  AddModelDownloadProgressObserver(observer.BindRemote());
}

TEST_F(EchoAIManagerOHOSTest, CreateTranslatorSuccess) {
  auto options = blink::mojom::TranslatorCreateOptions::New(
      blink::mojom::TranslatorLanguageCode::New("en"),
      blink::mojom::TranslatorLanguageCode::New("zh"),
      mojo::NullRemote());

  MockTranslationManagerCreateTranslatorClient client;
  CreateTranslator(client.BindRemote(), std::move(options), false);

  EXPECT_TRUE(client.on_result_called_);
}

TEST_F(EchoAIManagerOHOSTest, TranslationAvailable) {
  bool callback_called = false;
  blink::mojom::CanCreateTranslatorResult result;

  TranslationAvailable(
      blink::mojom::TranslatorLanguageCode::New("en"),
      blink::mojom::TranslatorLanguageCode::New("zh"),
      base::BindOnce(
          [](bool* called, blink::mojom::CanCreateTranslatorResult* out,
             blink::mojom::CanCreateTranslatorResult result) {
            *called = true;
            *out = result;
          },
          &callback_called, &result));

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(result, blink::mojom::CanCreateTranslatorResult::kReadily);
}

TEST_F(EchoAIManagerOHOSTest, CreateLanguageDetector) {
  bool callback_called = false;
  mojo::PendingRemote<blink::mojom::AILanguageDetector> detector_remote;

  CreateLanguageDetector(
      base::BindOnce(
          [](bool* called,
             mojo::PendingRemote<blink::mojom::AILanguageDetector>* out,
             mojo::PendingRemote<blink::mojom::AILanguageDetector> result) {
            *called = true;
            *out = std::move(result);
          },
          &callback_called, &detector_remote));

  EXPECT_TRUE(callback_called);
}

TEST_F(EchoAIManagerOHOSTest, IsModelDownloadedForCurrentReciever) {
  bool result = IsModelDownloadedForCurrentReciever();
  EXPECT_FALSE(result);
}

TEST_F(EchoAIManagerOHOSTest, DoMockDownloadingAndReturn) {
  MockModelDownloadProgressObserver observer;
  AddModelDownloadProgressObserver(observer.BindRemote());

  base::RunLoop loop;
  DoMockDownloadingAndReturn(loop.QuitClosure());
  loop.Run();

  EXPECT_GT(observer.download_progress_update_count_, 0);
}

TEST_F(EchoAIManagerOHOSTest, CanCreateLanguageModelWithExpectedInputsNull) {
  bool callback_called = false;
  blink::mojom::ModelAvailabilityCheckResult result;

  auto options = AILanguageModelCreateOptions::New(
      nullptr, std::vector<AILanguageModelPromptPtr>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelExpectedPtr>());

  CanCreateLanguageModel(
      std::move(options),
      base::BindOnce(
          [](bool* called, blink::mojom::ModelAvailabilityCheckResult* out,
             blink::mojom::ModelAvailabilityCheckResult result) {
            *called = true;
            *out = result;
          },
          &callback_called, &result));

  EXPECT_TRUE(callback_called);
}

TEST_F(EchoAIManagerOHOSTest, CanCreateLanguageModelWithExpectedOutputsNull) {
  bool callback_called = false;
  blink::mojom::ModelAvailabilityCheckResult result;

  std::vector<AILanguageCodePtr> codes;
  codes.push_back(AILanguageCode::New("en"));
  std::vector<AILanguageModelExpectedPtr> expected_inputs;
  expected_inputs.push_back(AILanguageModelExpected::New(
      AILanguageModelPromptType::kText,
      std::move(codes)));
  auto options = AILanguageModelCreateOptions::New(
      nullptr, std::vector<AILanguageModelPromptPtr>(),
      std::move(expected_inputs),
      std::vector<AILanguageModelExpectedPtr>());

  CanCreateLanguageModel(
      std::move(options),
      base::BindOnce(
          [](bool* called, blink::mojom::ModelAvailabilityCheckResult* out,
             blink::mojom::ModelAvailabilityCheckResult result) {
            *called = true;
            *out = result;
          },
          &callback_called, &result));

  EXPECT_TRUE(callback_called);
}

TEST_F(EchoAIManagerOHOSTest, CreateLanguageModelWithNullSamplingParams) {
  auto options = AILanguageModelCreateOptions::New(
      nullptr, std::vector<AILanguageModelPromptPtr>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelExpectedPtr>());

  MockAIManagerCreateLanguageModelClient client;
  CreateLanguageModel(client.BindRemote(), std::move(options));

  EXPECT_TRUE(client.on_result_called_ || client.on_error_called_);
  if (client.on_result_called_ && client.info_) {
  }
}

TEST_F(EchoAIManagerOHOSTest, CreateLanguageModelWithNonTextExpectedInput) {
  std::vector<AILanguageCodePtr> codes;
  codes.push_back(AILanguageCode::New("en"));
  std::vector<AILanguageModelExpectedPtr> expected_inputs;
  expected_inputs.push_back(AILanguageModelExpected::New(
      AILanguageModelPromptType::kImage,
      std::move(codes)));

  auto options = AILanguageModelCreateOptions::New(
      nullptr, std::vector<AILanguageModelPromptPtr>(),
      std::move(expected_inputs),
      std::vector<AILanguageModelExpectedPtr>());

  MockAIManagerCreateLanguageModelClient client;
  CreateLanguageModel(client.BindRemote(), std::move(options));

  EXPECT_TRUE(client.on_error_called_);
  EXPECT_EQ(client.error_,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(EchoAIManagerOHOSTest, CreateLanguageModelWithNonTextExpectedOutput) {
  std::vector<AILanguageCodePtr> codes;
  codes.push_back(AILanguageCode::New("en"));
  std::vector<AILanguageModelExpectedPtr> expected_outputs;
  expected_outputs.push_back(AILanguageModelExpected::New(
      AILanguageModelPromptType::kImage,
      std::move(codes)));

  auto options = AILanguageModelCreateOptions::New(
      nullptr, std::vector<AILanguageModelPromptPtr>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::move(expected_outputs));

  MockAIManagerCreateLanguageModelClient client;
  CreateLanguageModel(client.BindRemote(), std::move(options));

  EXPECT_TRUE(client.on_error_called_);
  EXPECT_EQ(client.error_,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(EchoAIManagerOHOSTest, CreateLanguageModelWithSamplingParams) {
  auto sampling_params = AILanguageModelSamplingParams::New(1, 0.5f);

  auto options = AILanguageModelCreateOptions::New(
      std::move(sampling_params), std::vector<AILanguageModelPromptPtr>(),
      std::vector<AILanguageModelExpectedPtr>(),
      std::vector<AILanguageModelExpectedPtr>());

  MockAIManagerCreateLanguageModelClient client;
  CreateLanguageModel(client.BindRemote(), std::move(options));

  EXPECT_TRUE(client.on_result_called_ || client.on_error_called_);
}

TEST_F(EchoAIManagerOHOSTest, CreateLanguageModelWithMultipleExpectedInputs) {
  std::vector<AILanguageCodePtr> codes;
  codes.push_back(AILanguageCode::New("en"));
  std::vector<AILanguageModelExpectedPtr> expected_inputs;
  expected_inputs.push_back(AILanguageModelExpected::New(
      AILanguageModelPromptType::kText,
      std::move(codes)));
  expected_inputs.push_back(AILanguageModelExpected::New(
      AILanguageModelPromptType::kText,
      std::move(codes)));

  auto options = AILanguageModelCreateOptions::New(
      nullptr, std::vector<AILanguageModelPromptPtr>(),
      std::move(expected_inputs),
      std::vector<AILanguageModelExpectedPtr>());

  MockAIManagerCreateLanguageModelClient client;
  CreateLanguageModel(client.BindRemote(), std::move(options));

  EXPECT_TRUE(client.on_result_called_ || client.on_error_called_);
}

}  // namespace content