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

#include <fuzzer/FuzzedDataProvider.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "arkweb/chromium_ext/third_party/blink/renderer/core/paint/timing/blank_screen_detector.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

static std::unique_ptr<DummyPageHolder> g_page_holder;

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();
    g_page_holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  }
  ~Environment() { g_page_holder.reset(); }
  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

static String FuzzString(FuzzedDataProvider* fdp, size_t max_len) {
  std::string s = fdp->ConsumeRandomLengthString(max_len);
  return String(s.c_str());
}

static String DocumentUrl() {
  if (!g_page_holder) {
    return String();
  }
  return g_page_holder->GetDocument().Url().GetString();
}

static std::vector<double> FuzzTimingVector(FuzzedDataProvider* fdp) {
  size_t count = fdp->ConsumeIntegralInRange<size_t>(0, 8);
  std::vector<double> timing;
  timing.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    timing.push_back(fdp->ConsumeFloatingPointInRange<double>(-10.0, 30.0));
  }
  return timing;
}

static std::vector<int32_t> FuzzMethodsVector(FuzzedDataProvider* fdp) {
  size_t count = fdp->ConsumeIntegralInRange<size_t>(0, 6);
  std::vector<int32_t> methods;
  methods.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    methods.push_back(fdp->ConsumeIntegralInRange<int32_t>(-2, 5));
  }
  return methods;
}

static void DrainPendingTasks() {
  base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
  loop.RunUntilIdle();
}

void FuzzDetectAndStop(BlankScreenDetector* detector, FuzzedDataProvider* fdp) {
  String url = FuzzString(fdp, 128);
  std::vector<double> timing = FuzzTimingVector(fdp);
  std::vector<int32_t> methods = FuzzMethodsVector(fdp);
  int32_t threshold = fdp->ConsumeIntegral<int32_t>();

  detector->DetectBlankScreen(url, timing, methods, threshold);
  detector->OnInputOrScroll();
}

void FuzzStateSequence(BlankScreenDetector* detector, FuzzedDataProvider* fdp) {
  int num_ops = fdp->ConsumeIntegralInRange<int>(2, 10);
  for (int i = 0; i < num_ops && fdp->remaining_bytes() > 4; ++i) {
    int op = fdp->ConsumeIntegralInRange<int>(0, 2);
    switch (op) {
      case 0:
      case 1: {
        String url = FuzzString(fdp, 64);
        std::vector<double> timing = FuzzTimingVector(fdp);
        std::vector<int32_t> methods = FuzzMethodsVector(fdp);
        int32_t threshold = fdp->ConsumeIntegralInRange<int32_t>(0, 20);
        detector->DetectBlankScreen(url, timing, methods, threshold);
        break;
      }
      case 2:
        detector->OnInputOrScroll();
        break;
    }
  }
  detector->OnInputOrScroll();
}

void FuzzEdgeCases(BlankScreenDetector* detector, FuzzedDataProvider* fdp) {
  detector->DetectBlankScreen(String(), {}, {}, 0);
  detector->OnInputOrScroll();

  detector->DetectBlankScreen(String(), {-1.0, -2.0, -3.0}, {}, 0);
  detector->OnInputOrScroll();

  detector->DetectBlankScreen(String(), {1.0}, {-1, 5, 99}, 0);
  detector->OnInputOrScroll();

  detector->DetectBlankScreen(String(), {0.001}, {0},
                              fdp->ConsumeIntegralInRange<int32_t>(0, 10000));
  detector->OnInputOrScroll();

  detector->DetectBlankScreen(String(), {1.0, 2.0, 3.0, 4.0, 5.0}, {0, 0, 0},
                              fdp->ConsumeIntegralInRange<int32_t>(-1, 100));
  detector->OnInputOrScroll();
}

void FuzzMultipleDetections(BlankScreenDetector* detector,
                            FuzzedDataProvider* fdp) {
  int num_detects = fdp->ConsumeIntegralInRange<int>(2, 5);
  for (int i = 0; i < num_detects && fdp->remaining_bytes() > 8; ++i) {
    FuzzDetectAndStop(detector, fdp);
  }
}

// Forces the recursive ScheduleNextTask <= 0 branch by feeding only tiny
// (sub-millisecond) positive delays which become 0 after seconds->ms truncation
// or are dwarfed by the elapsed wall-clock time before scheduling.
void FuzzZeroDelayRecursion(BlankScreenDetector* detector,
                            FuzzedDataProvider* fdp) {
  std::vector<double> tiny_delays;
  size_t count = fdp->ConsumeIntegralInRange<size_t>(1, 8);
  for (size_t i = 0; i < count; ++i) {
    tiny_delays.push_back(
        fdp->ConsumeFloatingPointInRange<double>(0.0001, 0.0009));
  }
  detector->DetectBlankScreen(DocumentUrl(), tiny_delays, {0},
                              fdp->ConsumeIntegralInRange<int32_t>(0, 4));
  // After all are skipped, current_task_index_ exceeds size(); a final
  // OnInputOrScroll resets state.
  detector->OnInputOrScroll();
}

// Triggers the URL-match path so CalculateContentfulCount + GenerateTestPoints
// + GenerateTestPointsByMethodDetection17 + GetPaintRects can run when the
// detection task fires.
void FuzzMatchingUrlAndDrain(BlankScreenDetector* detector,
                             FuzzedDataProvider* fdp) {
  String url = DocumentUrl();
  std::vector<double> timing = {
      fdp->ConsumeFloatingPointInRange<double>(0.001, 0.05),
      fdp->ConsumeFloatingPointInRange<double>(0.001, 0.05),
  };
  std::vector<int32_t> methods = {
      static_cast<int32_t>(
          BlankScreenDetectionMethod::DETECTION_CONTENTFUL_NODES_SEVENTEEN)};
  int32_t threshold = fdp->ConsumeIntegralInRange<int32_t>(0, 32);

  detector->DetectBlankScreen(url, timing, methods, threshold);
  // Pump any timers that became due immediately.
  DrainPendingTasks();
  detector->OnInputOrScroll();
}

// Mismatched URL path: CalculateContentfulCount short-circuits, exercising
// the NO_CONTENTFUL_NODES branch in RunDetectionTask without paint setup.
void FuzzMismatchedUrlAndDrain(BlankScreenDetector* detector,
                               FuzzedDataProvider* fdp) {
  String url = FuzzString(fdp, 32);
  if (url == DocumentUrl()) {
    url = String("about:fuzzer-mismatch");
  }
  std::vector<double> timing = {0.001};
  std::vector<int32_t> methods = {
      static_cast<int32_t>(
          BlankScreenDetectionMethod::DETECTION_CONTENTFUL_NODES_SEVENTEEN)};
  detector->DetectBlankScreen(url, timing, methods,
                              fdp->ConsumeIntegralInRange<int32_t>(-1, 8));
  DrainPendingTasks();
  detector->OnInputOrScroll();
}

// Defaults branch: empty methods + empty/all-non-positive timing so both
// detection_methods_ and task_delays_ms_ fall back to defaults.
void FuzzDefaultsFallback(BlankScreenDetector* detector,
                          FuzzedDataProvider* fdp) {
  detector->DetectBlankScreen(DocumentUrl(), {-1.0, 0.0, -2.0}, {},
                              fdp->ConsumeIntegralInRange<int32_t>(0, 4));
  // Don't drain — defaults are 1s/3s/5s, not realistic to fire here. Just
  // make sure StopTask resets cleanly.
  detector->OnInputOrScroll();
}

// Stress: re-issue detection mid-flight to exercise the StopTask-then-restart
// branch inside DetectBlankScreen.
void FuzzReentrantDetect(BlankScreenDetector* detector,
                         FuzzedDataProvider* fdp) {
  detector->DetectBlankScreen(DocumentUrl(), {0.5, 1.0}, {0}, 1);
  detector->DetectBlankScreen(DocumentUrl(), {0.001}, {0, 0, 0},
                              fdp->ConsumeIntegralInRange<int32_t>(0, 10));
  detector->DetectBlankScreen(FuzzString(fdp, 16), {}, {99}, 0);
  DrainPendingTasks();
  detector->OnInputOrScroll();
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::Environment env;

  if (!blink::g_page_holder) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);

  // Use Persistent<> to firmly root the GC object across any task pumping
  // (RunUntilIdle) or implicit GC that may run inside scenarios. A bare
  // raw pointer on the stack can be dropped by the optimizer, leaving the
  // detector eligible for collection — accessing it afterwards crashed
  // inside StopTask() because `this` was dangling.
  blink::Persistent<blink::BlankScreenDetector> detector =
      blink::MakeGarbageCollected<blink::BlankScreenDetector>(
          &blink::g_page_holder->GetFrame());

  int32_t scenario = fdp.ConsumeIntegralInRange<int32_t>(0, 8);
  switch (scenario) {
    case 0:
      blink::FuzzDetectAndStop(detector.Get(), &fdp);
      break;
    case 1:
      blink::FuzzStateSequence(detector.Get(), &fdp);
      break;
    case 2:
      blink::FuzzEdgeCases(detector.Get(), &fdp);
      break;
    case 3:
      blink::FuzzMultipleDetections(detector.Get(), &fdp);
      break;
    case 4:
      blink::FuzzZeroDelayRecursion(detector.Get(), &fdp);
      break;
    case 5:
      blink::FuzzMatchingUrlAndDrain(detector.Get(), &fdp);
      break;
    case 6:
      blink::FuzzMismatchedUrlAndDrain(detector.Get(), &fdp);
      break;
    case 7:
      blink::FuzzDefaultsFallback(detector.Get(), &fdp);
      break;
    case 8:
      blink::FuzzReentrantDetect(detector.Get(), &fdp);
      break;
    default:
      blink::FuzzDetectAndStop(detector.Get(), &fdp);
      break;
  }

  // Detector still rooted here; safe to stop the timer one final time.
  detector->OnInputOrScroll();
  // Drain any tasks that the scenario left scheduled, while detector is
  // still rooted, so RunDetectionTask can execute (raising coverage of
  // GenerateTestPoints / CalculateContentfulCount / GetPaintRects).
  blink::DrainPendingTasks();
  // Final stop after drain in case a callback re-armed the timer.
  detector->OnInputOrScroll();

  // Release the strong root so the next iteration's GC can collect it.
  detector.Clear();
  return 0;
}
