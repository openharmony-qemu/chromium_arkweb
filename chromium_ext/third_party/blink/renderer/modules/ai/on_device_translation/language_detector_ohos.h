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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_LANGUAGE_DETECTOR_OHOS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_LANGUAGE_DETECTOR_OHOS_H_

#include "arkweb/chromium_ext/third_party/blink/public/mojom/ai/ai_language_detector.mojom-blink.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_availability.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_detection_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_detector_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_detector_detect_options.h"
#include "third_party/blink/renderer/modules/ai/on_device_translation/resolver_with_abort_signal.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

#include "third_party/blink/renderer/platform/language_detection/language_detection_model.h"
namespace blink {
class LanguageDetector;

class LanguageDetectorOHOS : public GarbageCollectedMixin {
 public:
  explicit LanguageDetectorOHOS(ScriptState* script_state);
  ~LanguageDetectorOHOS();

  void Trace(Visitor* visitor) const override;

  void BindRemote(
      mojo::PendingRemote<mojom::blink::AILanguageDetector> pending_remote,
      scoped_refptr<base::SequencedTaskRunner>& task_runner);

  static void CreateTask(ScriptState* script_state,
                         ScriptPromiseResolver<LanguageDetector>* resolver,
                         LanguageDetectorCreateOptions* options);

 protected:
  Vector<LanguageDetectionModel::LanguagePrediction> ParseDetectionResults(
      const String& response);
    
  void HandleDetectionSuccess(
      ResolverWithAbortSignal<IDLSequence<LanguageDetectionResult>>* resolver,
      const String& response,
      mojom::blink::ModelExecutionContextInfoPtr context_info);
    
  void HandleDetectionError(
      ResolverWithAbortSignal<IDLSequence<LanguageDetectionResult>>* resolver,
      DOMException* exception);
    
  void HandleDetectionAbortSignal(
      ResolverWithAbortSignal<IDLSequence<LanguageDetectionResult>>* resolver,
      AbortSignal* signal, 
      ScriptState* script_state);

  void Detect(
      ScriptState* script_state,
      const String& input,
      AbortSignal* composite_signal,
      scoped_refptr<base::SequencedTaskRunner>& task_runner,
      ResolverWithAbortSignal<IDLSequence<LanguageDetectionResult>>* resolver);

  HeapMojoRemote<mojom::blink::AILanguageDetector> remote_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_LANGUAGE_DETECTOR_OHOS_H_
