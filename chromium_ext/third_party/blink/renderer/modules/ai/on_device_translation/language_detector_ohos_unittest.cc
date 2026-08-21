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

#include "arkweb/chromium_ext/third_party/blink/renderer/modules/ai/on_device_translation/language_detector_ohos.h"

#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ai/on_device_translation/resolver_with_abort_signal.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

class TestLanguageDetectorOHOS final
    : public GarbageCollected<TestLanguageDetectorOHOS>,
      public LanguageDetectorOHOS {
 public:
  explicit TestLanguageDetectorOHOS(ScriptState* script_state)
      : LanguageDetectorOHOS(script_state) {}

  using LanguageDetectorOHOS::Detect;
  using LanguageDetectorOHOS::HandleDetectionAbortSignal;
  using LanguageDetectorOHOS::HandleDetectionError;
  using LanguageDetectorOHOS::HandleDetectionSuccess;
  using LanguageDetectorOHOS::ParseDetectionResults;

  bool IsRemoteBound() const { return remote_.is_bound(); }

  void Trace(Visitor* visitor) const override {
    LanguageDetectorOHOS::Trace(visitor);
  }
};

class MockAILanguageDetector final : public mojom::blink::AILanguageDetector {
 public:
  mojo::PendingRemote<mojom::blink::AILanguageDetector> BindRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  void Detect(const String& input,
              mojo::PendingRemote<mojom::blink::ModelStreamingResponder>
                  responder) override {
    last_input_ = input;
    responder_ = std::move(responder);
  }

  String last_input_;
  mojo::PendingRemote<mojom::blink::ModelStreamingResponder> responder_;

 private:
  mojo::Receiver<mojom::blink::AILanguageDetector> receiver_{this};
};

TEST(LanguageDetectorOHOSTest, InvalidJsonReturnsUndeterminedLanguage) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* detector =
      MakeGarbageCollected<TestLanguageDetectorOHOS>(scope.GetScriptState());

  auto predictions = detector->ParseDetectionResults("not-json");

  ASSERT_EQ(predictions.size(), 1u);
  EXPECT_EQ(predictions[0].language, "und");
  EXPECT_FLOAT_EQ(predictions[0].score, 1.0f);
}

TEST(LanguageDetectorOHOSTest,
     EmptyOrMalformedListReturnsUndeterminedLanguage) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* detector =
      MakeGarbageCollected<TestLanguageDetectorOHOS>(scope.GetScriptState());

  auto predictions = detector->ParseDetectionResults(
      R"([{"language":"en"},{"score":0.5},17])");

  ASSERT_EQ(predictions.size(), 1u);
  EXPECT_EQ(predictions[0].language, "und");
  EXPECT_FLOAT_EQ(predictions[0].score, 1.0f);
}

TEST(LanguageDetectorOHOSTest, ValidPredictionsAppendUnknownConfidence) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* detector =
      MakeGarbageCollected<TestLanguageDetectorOHOS>(scope.GetScriptState());

  auto predictions = detector->ParseDetectionResults(
      R"([{"language":"en","score":0.6},{"language":"fr","score":0.25},])");

  ASSERT_EQ(predictions.size(), 3u);
  EXPECT_EQ(predictions[0].language, "en");
  EXPECT_FLOAT_EQ(predictions[0].score, 0.6f);
  EXPECT_EQ(predictions[1].language, "fr");
  EXPECT_FLOAT_EQ(predictions[1].score, 0.25f);
  EXPECT_EQ(predictions[2].language, "unknown");
  EXPECT_FLOAT_EQ(predictions[2].score, 0.15f);
}

TEST(LanguageDetectorOHOSTest, BindRemoteBindsDetectorInterface) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* detector =
      MakeGarbageCollected<TestLanguageDetectorOHOS>(scope.GetScriptState());
  MockAILanguageDetector mock_detector;
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      scope.GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault);

  detector->BindRemote(mock_detector.BindRemote(), task_runner);

  EXPECT_TRUE(detector->IsRemoteBound());
}

TEST(LanguageDetectorOHOSTest, DetectForwardsInputToRemote) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* detector =
      MakeGarbageCollected<TestLanguageDetectorOHOS>(scope.GetScriptState());
  MockAILanguageDetector mock_detector;
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      scope.GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault);
  detector->BindRemote(mock_detector.BindRemote(), task_runner);
  auto* resolver = MakeGarbageCollected<
      ResolverWithAbortSignal<IDLSequence<LanguageDetectionResult>>>(
      scope.GetScriptState(), nullptr);

  detector->Detect(scope.GetScriptState(), "hello", nullptr, task_runner,
                   resolver);
  mock_detector.FlushForTesting();

  EXPECT_EQ(mock_detector.last_input_, "hello");
  EXPECT_TRUE(mock_detector.responder_.is_valid());
}

TEST(LanguageDetectorOHOSTest, HandleDetectionSuccessResolvesPromise) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* detector =
      MakeGarbageCollected<TestLanguageDetectorOHOS>(scope.GetScriptState());
  auto* resolver = MakeGarbageCollected<
      ResolverWithAbortSignal<IDLSequence<LanguageDetectionResult>>>(
      scope.GetScriptState(), nullptr);
  ScriptPromiseTester promise_tester(scope.GetScriptState(),
                                     resolver->Promise());

  detector->HandleDetectionSuccess(
      resolver, R"([{"language":"en","score":0.8}])", nullptr);
  promise_tester.WaitUntilSettled();

  EXPECT_TRUE(promise_tester.IsFulfilled());
}

TEST(LanguageDetectorOHOSTest, HandleDetectionErrorRejectsPromise) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* detector =
      MakeGarbageCollected<TestLanguageDetectorOHOS>(scope.GetScriptState());
  auto* resolver = MakeGarbageCollected<
      ResolverWithAbortSignal<IDLSequence<LanguageDetectionResult>>>(
      scope.GetScriptState(), nullptr);
  ScriptPromiseTester promise_tester(scope.GetScriptState(),
                                     resolver->Promise());

  detector->HandleDetectionError(
      resolver, MakeGarbageCollected<DOMException>(
                    DOMExceptionCode::kUnknownError, "detection failed"));
  promise_tester.WaitUntilSettled();

  EXPECT_TRUE(promise_tester.IsRejected());
}

TEST(LanguageDetectorOHOSTest, HandleDetectionAbortSignalRejectsPromise) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* detector =
      MakeGarbageCollected<TestLanguageDetectorOHOS>(scope.GetScriptState());
  auto* resolver = MakeGarbageCollected<
      ResolverWithAbortSignal<IDLSequence<LanguageDetectionResult>>>(
      scope.GetScriptState(), nullptr);
  ScriptPromiseTester promise_tester(scope.GetScriptState(),
                                     resolver->Promise());
  AbortController* controller = AbortController::Create(scope.GetScriptState());
  controller->abort(scope.GetScriptState());

  detector->HandleDetectionAbortSignal(resolver, controller->signal(),
                                       scope.GetScriptState());
  promise_tester.WaitUntilSettled();

  EXPECT_TRUE(promise_tester.IsRejected());
}

TEST(LanguageDetectorOHOSTest, HandleDetectionAbortSignalIgnoresNullSignal) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* detector =
      MakeGarbageCollected<TestLanguageDetectorOHOS>(scope.GetScriptState());
  auto* resolver = MakeGarbageCollected<
      ResolverWithAbortSignal<IDLSequence<LanguageDetectionResult>>>(
      scope.GetScriptState(), nullptr);
  ScriptPromiseTester promise_tester(scope.GetScriptState(),
                                     resolver->Promise());

  detector->HandleDetectionAbortSignal(resolver, nullptr,
                                       scope.GetScriptState());
  scope.GetIsolate()->PerformMicrotaskCheckpoint();

  EXPECT_FALSE(promise_tester.IsFulfilled());
  EXPECT_FALSE(promise_tester.IsRejected());
}

}  // namespace
}  // namespace blink
