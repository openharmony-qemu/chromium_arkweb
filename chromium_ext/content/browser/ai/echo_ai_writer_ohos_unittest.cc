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

#include "echo_ai_writer_ohos.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_writer.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace content {

namespace {

using ::blink::mojom::AILanguageCode;
using ::blink::mojom::AILanguageCodePtr;
using ::blink::mojom::AIWriterCreateOptions;
using ::blink::mojom::AIWriterFormat;
using ::blink::mojom::AIWriterLength;
using ::blink::mojom::AIWriterTone;

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

  void OnDestroyAISession(AISessionType type,
                          const std::string& uuid) override {
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

class MockModelStreamingResponder
    : public blink::mojom::ModelStreamingResponder {
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

  void OnQuotaOverflow() override {}

  bool on_completion_called_ = false;
  bool on_error_called_ = false;
  std::string streaming_content_;
  blink::mojom::ModelStreamingResponseStatus error_status_;

 private:
  mojo::Receiver<blink::mojom::ModelStreamingResponder> receiver_{this};
};

void ExpectCreateStringField(const MockWebContentsDelegate& delegate,
                             std::string_view field,
                             std::string_view expected) {
  std::optional<base::Value> value =
      base::JSONReader::Read(delegate.input_, base::JSON_PARSE_RFC);
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_dict());
  const std::string* actual = value->GetDict().FindString(field);
  ASSERT_NE(actual, nullptr);
  EXPECT_EQ(*actual, expected);
}

blink::mojom::AIWriterCreateOptionsPtr CreateOptionsWithTone(
    AIWriterTone tone) {
  return AIWriterCreateOptions::New(
      kSharedContext, tone, AIWriterFormat::kPlainText, AIWriterLength::kMedium,
      std::vector<AILanguageCodePtr>(), std::vector<AILanguageCodePtr>(),
      AILanguageCode::New("en"));
}

blink::mojom::AIWriterCreateOptionsPtr CreateOptionsWithFormat(
    AIWriterFormat format) {
  return AIWriterCreateOptions::New(
      kSharedContext, AIWriterTone::kNeutral, format, AIWriterLength::kMedium,
      std::vector<AILanguageCodePtr>(), std::vector<AILanguageCodePtr>(),
      AILanguageCode::New("en"));
}

blink::mojom::AIWriterCreateOptionsPtr CreateOptionsWithLength(
    AIWriterLength length) {
  return AIWriterCreateOptions::New(
      kSharedContext, AIWriterTone::kNeutral, AIWriterFormat::kPlainText,
      length, std::vector<AILanguageCodePtr>(),
      std::vector<AILanguageCodePtr>(), AILanguageCode::New("en"));
}

blink::mojom::AIWriterCreateOptionsPtr CreateOptionsWithLanguages() {
  std::vector<AILanguageCodePtr> input_langs;
  input_langs.push_back(AILanguageCode::New("en"));
  input_langs.push_back(AILanguageCode::New("zh"));

  std::vector<AILanguageCodePtr> context_langs;
  context_langs.push_back(AILanguageCode::New("fr"));
  context_langs.push_back(AILanguageCode::New("de"));

  return AIWriterCreateOptions::New(
      kSharedContext, AIWriterTone::kNeutral, AIWriterFormat::kPlainText,
      AIWriterLength::kMedium, std::move(input_langs), std::move(context_langs),
      AILanguageCode::New("es"));
}

blink::mojom::AIWriterCreateOptionsPtr CreateOptionsWithoutSharedContext() {
  return AIWriterCreateOptions::New(
      std::nullopt, AIWriterTone::kNeutral, AIWriterFormat::kPlainText,
      AIWriterLength::kMedium, std::vector<AILanguageCodePtr>(),
      std::vector<AILanguageCodePtr>(), AILanguageCode::New("en"));
}

class EchoAIWriterOHOSTest : public RenderViewHostTestHarness {
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

TEST_F(EchoAIWriterOHOSTest, ConstructorWithFormalTone) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithTone(AIWriterTone::kFormal);

  EchoAIWriterOHOS writer(web_contents(),
                          base::BindOnce(
                              [](bool* called, bool* result, bool success) {
                                *called = true;
                                *result = success;
                              },
                              &callback_called, &callback_result),
                          std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
  ExpectCreateStringField(*delegate_, "tone", "formal");
}

TEST_F(EchoAIWriterOHOSTest, ConstructorWithNeutralTone) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithTone(AIWriterTone::kNeutral);

  EchoAIWriterOHOS writer(web_contents(),
                          base::BindOnce(
                              [](bool* called, bool* result, bool success) {
                                *called = true;
                                *result = success;
                              },
                              &callback_called, &callback_result),
                          std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAIWriterOHOSTest, ConstructorWithCasualTone) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithTone(AIWriterTone::kCasual);

  EchoAIWriterOHOS writer(web_contents(),
                          base::BindOnce(
                              [](bool* called, bool* result, bool success) {
                                *called = true;
                                *result = success;
                              },
                              &callback_called, &callback_result),
                          std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
  ExpectCreateStringField(*delegate_, "tone", "casual");
}

TEST_F(EchoAIWriterOHOSTest, ConstructorWithPlainTextFormat) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithFormat(AIWriterFormat::kPlainText);

  EchoAIWriterOHOS writer(web_contents(),
                          base::BindOnce(
                              [](bool* called, bool* result, bool success) {
                                *called = true;
                                *result = success;
                              },
                              &callback_called, &callback_result),
                          std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAIWriterOHOSTest, ConstructorWithMarkdownFormat) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithFormat(AIWriterFormat::kMarkdown);

  EchoAIWriterOHOS writer(web_contents(),
                          base::BindOnce(
                              [](bool* called, bool* result, bool success) {
                                *called = true;
                                *result = success;
                              },
                              &callback_called, &callback_result),
                          std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
  ExpectCreateStringField(*delegate_, "format", "markdown");
}

TEST_F(EchoAIWriterOHOSTest, ConstructorWithShortLength) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithLength(AIWriterLength::kShort);

  EchoAIWriterOHOS writer(web_contents(),
                          base::BindOnce(
                              [](bool* called, bool* result, bool success) {
                                *called = true;
                                *result = success;
                              },
                              &callback_called, &callback_result),
                          std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
  ExpectCreateStringField(*delegate_, "length", "short");
}

TEST_F(EchoAIWriterOHOSTest, ConstructorWithMediumLength) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithLength(AIWriterLength::kMedium);

  EchoAIWriterOHOS writer(web_contents(),
                          base::BindOnce(
                              [](bool* called, bool* result, bool success) {
                                *called = true;
                                *result = success;
                              },
                              &callback_called, &callback_result),
                          std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAIWriterOHOSTest, ConstructorWithLongLength) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithLength(AIWriterLength::kLong);

  EchoAIWriterOHOS writer(web_contents(),
                          base::BindOnce(
                              [](bool* called, bool* result, bool success) {
                                *called = true;
                                *result = success;
                              },
                              &callback_called, &callback_result),
                          std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
  ExpectCreateStringField(*delegate_, "length", "long");
}

TEST_F(EchoAIWriterOHOSTest, ConstructorWithLanguages) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithLanguages();

  EchoAIWriterOHOS writer(web_contents(),
                          base::BindOnce(
                              [](bool* called, bool* result, bool success) {
                                *called = true;
                                *result = success;
                              },
                              &callback_called, &callback_result),
                          std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);

  ExpectCreateStringField(*delegate_, "sharedContext", kSharedContext);
  ExpectCreateStringField(*delegate_, "tone", "neutral");
  ExpectCreateStringField(*delegate_, "format", "plain-text");
  ExpectCreateStringField(*delegate_, "length", "medium");
  ExpectCreateStringField(*delegate_, "outputLanguage", "es");

  std::optional<base::Value> value =
      base::JSONReader::Read(delegate_->input_, base::JSON_PARSE_RFC);
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_dict());
  const base::Value::Dict& dict = value->GetDict();
  const base::Value::List* input_languages =
      dict.FindList("expectedInputLanguages");
  ASSERT_NE(input_languages, nullptr);
  ASSERT_EQ(input_languages->size(), 2u);
  EXPECT_EQ((*input_languages)[0].GetString(), "en");
  EXPECT_EQ((*input_languages)[1].GetString(), "zh");
  const base::Value::List* context_languages =
      dict.FindList("expectedContextLanguages");
  ASSERT_NE(context_languages, nullptr);
  ASSERT_EQ(context_languages->size(), 2u);
  EXPECT_EQ((*context_languages)[0].GetString(), "fr");
  EXPECT_EQ((*context_languages)[1].GetString(), "de");
}

TEST_F(EchoAIWriterOHOSTest, ConstructorWithSharedContext) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithTone(AIWriterTone::kNeutral);

  EchoAIWriterOHOS writer(web_contents(),
                          base::BindOnce(
                              [](bool* called, bool* result, bool success) {
                                *called = true;
                                *result = success;
                              },
                              &callback_called, &callback_result),
                          std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(EchoAIWriterOHOSTest, ConstructorWithoutSharedContext) {
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithoutSharedContext();

  EchoAIWriterOHOS writer(web_contents(),
                          base::BindOnce(
                              [](bool* called, bool* result, bool success) {
                                *called = true;
                                *result = success;
                              },
                              &callback_called, &callback_result),
                          std::move(options));

  EXPECT_TRUE(delegate_->on_create_ai_session_called_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);

  std::optional<base::Value> value =
      base::JSONReader::Read(delegate_->input_, base::JSON_PARSE_RFC);
  ASSERT_TRUE(value.has_value());
  const base::Value* shared_context = value->GetDict().Find("sharedContext");
  ASSERT_NE(shared_context, nullptr);
  EXPECT_TRUE(shared_context->is_none());
}

TEST_F(EchoAIWriterOHOSTest, ConstructorWithoutDelegate) {
  web_contents()->SetDelegate(nullptr);
  bool callback_called = false;
  bool callback_result = false;
  auto options = CreateOptionsWithTone(AIWriterTone::kNeutral);

  EchoAIWriterOHOS writer(web_contents(),
                          base::BindOnce(
                              [](bool* called, bool* result, bool success) {
                                *called = true;
                                *result = success;
                              },
                              &callback_called, &callback_result),
                          std::move(options));

  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(callback_result);
}

TEST_F(EchoAIWriterOHOSTest, WriteSerializesOptionalContext) {
  EchoAIWriterOHOS writer(web_contents(), base::BindOnce([](bool) {}),
                          CreateOptionsWithTone(AIWriterTone::kNeutral));
  MockModelStreamingResponder responder;

  static_cast<blink::mojom::AIWriter&>(writer).Write(kInput, kContext,
                                                     responder.BindRemote());

  ASSERT_TRUE(delegate_->on_execute_ai_action_called_);
  std::optional<base::Value> value =
      base::JSONReader::Read(delegate_->execute_input_, base::JSON_PARSE_RFC);
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_dict());
  const std::string* input = value->GetDict().FindString("input");
  ASSERT_NE(input, nullptr);
  EXPECT_EQ(*input, kInput);
  const base::Value::Dict* options = value->GetDict().FindDict("options");
  ASSERT_NE(options, nullptr);
  EXPECT_EQ(*options->FindString("context"), kContext);
}

TEST_F(EchoAIWriterOHOSTest, WriteWithoutContextAndMeasureUsage) {
  EchoAIWriterOHOS writer(web_contents(), base::BindOnce([](bool) {}),
                          CreateOptionsWithTone(AIWriterTone::kNeutral));
  MockModelStreamingResponder responder;

  static_cast<blink::mojom::AIWriter&>(writer).Write(kInput, std::nullopt,
                                                     responder.BindRemote());

  ASSERT_TRUE(delegate_->on_execute_ai_action_called_);
  std::optional<base::Value> value =
      base::JSONReader::Read(delegate_->execute_input_, base::JSON_PARSE_RFC);
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_dict());
  const std::string* input = value->GetDict().FindString("input");
  ASSERT_NE(input, nullptr);
  EXPECT_EQ(*input, kInput);
  EXPECT_EQ(value->GetDict().Find("options"), nullptr);

  std::optional<uint32_t> measured_usage;
  static_cast<blink::mojom::AIWriter&>(writer).MeasureUsage(
      kInput, kContext,
      base::BindOnce(
          [](std::optional<uint32_t>* result,
             std::optional<uint32_t> number_of_tokens) {
            *result = number_of_tokens;
          },
          &measured_usage));
  ASSERT_TRUE(measured_usage.has_value());
  EXPECT_EQ(*measured_usage, std::string_view(kInput).size() +
                                 std::string_view(kContext).size());
}

}  // namespace

}  // namespace content
