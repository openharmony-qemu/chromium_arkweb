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

#include "language_detector_ohos.h"

#include "base/containers/fixed_flat_set.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_create_monitor_callback.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/ai/ai_context_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/availability.h"
#include "third_party/blink/renderer/modules/ai/create_monitor.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "v8/include/v8-isolate.h"

#include "base/json/json_reader.h"
#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"
#include "third_party/blink/renderer/modules/ai/on_device_translation/language_detector.h"

namespace blink {

namespace {

// TODO(crbug.com/410949688): Figure out how to retrieve these from the model.
static constexpr auto kSupportedLanguages =
    base::MakeFixedFlatSet<std::string_view>({
        "af",      "am", "ar",      "ar-Latn", "az",      "be",  "bg",
        "bg-Latn", "bn", "bs",      "ca",      "ceb",     "co",  "cs",
        "cy",      "da", "de",      "el",      "el-Latn", "en",  "eo",
        "es",      "et", "eu",      "fa",      "fi",      "fil", "fr",
        "fy",      "ga", "gd",      "gl",      "gu",      "ha",  "haw",
        "he",      "hi", "hi-Latn", "hmn",     "hr",      "ht",  "hu",
        "hy",      "id", "ig",      "is",      "it",      "ja",  "ja-Latn",
        "jv",      "ka", "kk",      "km",      "kn",      "ko",  "ku",
        "ky",      "la", "lb",      "lo",      "lt",      "lv",  "mg",
        "mi",      "mk", "ml",      "mn",      "mr",      "ms",  "mt",
        "my",      "ne", "nl",      "no",      "ny",      "pa",  "pl",
        "ps",      "pt", "ro",      "ru",      "ru-Latn", "sd",  "si",
        "sk",      "sl", "sm",      "sn",      "so",      "sq",  "sr",
        "st",      "su", "sv",      "sw",      "ta",      "te",  "tg",
        "th",      "tr", "uk",      "ur",      "uz",      "vi",  "xh",
        "yi",      "yo", "zh",      "zh-Latn", "zu",
    });

class LanguageDetectorCreateTaskOHOS
    : public GarbageCollected<LanguageDetectorCreateTaskOHOS>,
      public ExecutionContextClient,
      public AIContextObserver<LanguageDetector> {
 public:
  LanguageDetectorCreateTaskOHOS(
      ScriptState* script_state,
      ScriptPromiseResolver<LanguageDetector>* resolver,
      LanguageDetectorCreateOptions* options)
      : ExecutionContextClient(ExecutionContext::From(script_state)),
        AIContextObserver(script_state,
                          this,
                          resolver,
                          options->getSignalOr(nullptr)),
        task_runner_(AIInterfaceProxy::GetTaskRunner(GetExecutionContext())),
        options_(options) {
    if (options->hasMonitor()) {
      monitor_ = MakeGarbageCollected<CreateMonitor>(
          GetExecutionContext(), options->getSignalOr(nullptr), task_runner_);

      // If an exception is thrown, don't initiate language detection model
      // download. `CreateMonitorCallback`'s `Invoke` will automatically
      // reject the promise with the thrown exception.
      if (options->monitor()->Invoke(nullptr, monitor_).IsNothing()) {
        return;
      }
    }

    AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext())
        ->CreateLanguageDetector(
            blink::BindOnce(&LanguageDetectorCreateTaskOHOS::OnResult,
                            WrapPersistent(this))
                .Then(RejectOnDestruction(GetResolver())));
  }

  void Trace(Visitor* visitor) const override {
    ExecutionContextClient::Trace(visitor);
    AIContextObserver::Trace(visitor);
    visitor->Trace(monitor_);
    visitor->Trace(options_);
  }

  void OnResult(
      mojo::PendingRemote<mojom::blink::AILanguageDetector> pending_remote) {
    // Call `Cleanup` when this function returns.
    RunOnDestruction run_on_destruction(BindOnce(
        &LanguageDetectorCreateTaskOHOS::Cleanup, WrapWeakPersistent(this)));

    if (!GetResolver()) {
      return;
    }

    if (!pending_remote) {
      GetResolver()->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError, "Model not available"));
      return;
    }

    std::optional<Vector<String>> expected_input_languages;
    if (options_->hasExpectedInputLanguages() &&
        !options_->expectedInputLanguages().empty()) {
      expected_input_languages = GetBestFitLanguages(
          kSupportedLanguages, options_->expectedInputLanguages());
      if (!expected_input_languages.has_value()) {
        GetResolver()->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kUnknownError, "Language not available"));
        return;
      }
    }

    if (monitor_) {
      // Zero must be sent.
      monitor_->OnDownloadProgressUpdate(0, kNormalizedDownloadProgressMax);

      // Abort may have been triggered by `OnDownloadProgressUpdate`.
      if (!GetResolver()) {
        return;
      }

      // Ensure that a download completion event is sent.
      monitor_->OnDownloadProgressUpdate(kNormalizedDownloadProgressMax,
                                         kNormalizedDownloadProgressMax);

      // Abort may have been triggered by `OnDownloadProgressUpdate`.
      if (!GetResolver()) {
        return;
      }
    }
    LanguageDetector* language_detector =
        MakeGarbageCollected<LanguageDetector>(
            GetScriptState(), nullptr, GetAbortSignal(),
            std::move(expected_input_languages), task_runner_);
    language_detector->BindRemote(std::move(pending_remote), task_runner_);
    GetResolver()->Resolve(language_detector);
  }

 private:
  void ResetReceiver() override {}

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  Member<CreateMonitor> monitor_;
  Member<LanguageDetectorCreateOptions> options_;
};

}  // namespace

LanguageDetectorOHOS::LanguageDetectorOHOS(ScriptState* script_state)
    : remote_(ExecutionContext::From(script_state)) {}

LanguageDetectorOHOS::~LanguageDetectorOHOS() {
  remote_.reset();
}

void LanguageDetectorOHOS::Trace(Visitor* visitor) const {
  visitor->Trace(remote_);
}

void LanguageDetectorOHOS::BindRemote(
    mojo::PendingRemote<mojom::blink::AILanguageDetector> pending_remote,
    scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  remote_.Bind(std::move(pending_remote), task_runner);
}

// static
void LanguageDetectorOHOS::CreateTask(
    ScriptState* script_state,
    ScriptPromiseResolver<LanguageDetector>* resolver,
    LanguageDetectorCreateOptions* options) {
  MakeGarbageCollected<LanguageDetectorCreateTaskOHOS>(script_state, resolver,
                                                       options);
}

Vector<LanguageDetectionModel::LanguagePrediction> 
LanguageDetectorOHOS::ParseDetectionResults(const String& response) {
  std::optional<base::ListValue> result = base::JSONReader::ReadList(
      response.Utf8(),
      base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  Vector<LanguageDetectionModel::LanguagePrediction> predictions;
  float cumulative_confidence = 0;

  if (!result.has_value()) {
    predictions.push_back(LanguageDetectionModel::LanguagePrediction("und", 1));
    return predictions;
  }

  for (const auto& value : result.value()) {
    if (value.is_dict()) {
      const std::string* language = value.GetDict().FindString("language");
      const std::optional<double> score = value.GetDict().FindDouble("score");

      if (language && score.has_value()) {
        LanguageDetectionModel::LanguagePrediction prediction(
            *language, score.value());
        predictions.push_back(prediction);
        cumulative_confidence += score.value();
      }
    }
  }
  if (predictions.empty()) {
    predictions.push_back(LanguageDetectionModel::LanguagePrediction("und", 1));
  } else {
    predictions.push_back(
        LanguageDetectionModel::LanguagePrediction("unknown", 1 - cumulative_confidence));
  }

  return predictions;
}

void LanguageDetectorOHOS::HandleDetectionSuccess(
    ResolverWithAbortSignal<IDLSequence<LanguageDetectionResult>>* resolver,
    const String& response,
    mojom::blink::ModelExecutionContextInfoPtr context_info) {
  LOG(INFO) << "LanguageDetectorOHOS::Detect result: " << response;

  Vector<LanguageDetectionModel::LanguagePrediction> predictions = 
      ParseDetectionResults(response);
  LanguageDetector::OnDetectComplete(resolver, predictions);
}

void LanguageDetectorOHOS::HandleDetectionError(
    ResolverWithAbortSignal<IDLSequence<LanguageDetectionResult>>* resolver,
    DOMException* exception) {
  resolver->Reject(exception);
}

void LanguageDetectorOHOS::HandleDetectionAbortSignal(
    ResolverWithAbortSignal<IDLSequence<LanguageDetectionResult>>* resolver,
    AbortSignal* signal, 
    ScriptState* script_state) {
  if (signal) {
    resolver->Reject(signal->reason(script_state));
  }
}

void LanguageDetectorOHOS::Detect(
    ScriptState* script_state,
    const String& input,
    AbortSignal* composite_signal,
    scoped_refptr<base::SequencedTaskRunner>& task_runner,
    ResolverWithAbortSignal<IDLSequence<LanguageDetectionResult>>* resolver) {
  auto pending_remote = CreateModelExecutionResponder(
      script_state, 
      composite_signal, 
      task_runner,
      AIMetrics::AISessionType::kLanguageDetector,
      BindOnce(&LanguageDetectorOHOS::HandleDetectionSuccess, 
          WrapPersistent(this), 
          WrapPersistent(resolver)),
      base::DoNothingWithBoundArgs(WrapPersistent(this)),
      BindOnce(&LanguageDetectorOHOS::HandleDetectionError, 
          WrapPersistent(this), 
          WrapPersistent(resolver)),
      BindOnce(&LanguageDetectorOHOS::HandleDetectionAbortSignal, 
          WrapPersistent(this), 
          WrapPersistent(resolver), 
          WrapPersistent(composite_signal),
          WrapPersistent(script_state)));

  remote_->Detect(input, std::move(pending_remote));
}

}  // namespace blink
