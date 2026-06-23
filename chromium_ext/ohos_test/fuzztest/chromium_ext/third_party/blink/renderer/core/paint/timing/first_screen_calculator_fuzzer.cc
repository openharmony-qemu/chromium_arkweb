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

#include <memory>
#include <optional>
#include <vector>

#include "arkweb/chromium_ext/third_party/blink/renderer/core/paint/timing/first_screen_calculator.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/paint/timing/lcp_objects.h"

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

static gfx::Rect FuzzRect(FuzzedDataProvider* fdp) {
  int x = fdp->ConsumeIntegralInRange<int>(-1200, 1200);
  int y = fdp->ConsumeIntegralInRange<int>(-1200, 1200);
  int w = fdp->ConsumeIntegralInRange<int>(0, 1200);
  int h = fdp->ConsumeIntegralInRange<int>(0, 1200);
  return gfx::Rect(x, y, w, h);
}

static gfx::RectF FuzzRectF(FuzzedDataProvider* fdp) {
  float x = fdp->ConsumeFloatingPointInRange<float>(-1200.f, 1200.f);
  float y = fdp->ConsumeFloatingPointInRange<float>(-1200.f, 1200.f);
  float w = fdp->ConsumeFloatingPointInRange<float>(0.f, 1200.f);
  float h = fdp->ConsumeFloatingPointInRange<float>(0.f, 1200.f);
  return gfx::RectF(x, y, w, h);
}

static base::TimeTicks FuzzTimeTicks(FuzzedDataProvider* fdp) {
  int64_t ms = fdp->ConsumeIntegralInRange<int64_t>(0, 100000000LL);
  return base::TimeTicks() + base::Milliseconds(ms);
}

static Node* GetBodyNode() {
  if (!g_page_holder) {
    return nullptr;
  }
  return g_page_holder->GetDocument().body();
}

void FuzzSingleImagePaint(FirstScreenCalculator* calc,
                          FuzzedDataProvider* fdp) {
  Node* node = GetBodyNode();
  if (!node) {
    return;
  }
  gfx::Rect frame_rect = FuzzRect(fdp);
  gfx::RectF root_rectF = FuzzRectF(fdp);
  MediaRecordIdHash hash = fdp->ConsumeIntegral<uint64_t>();
  bool is_video = fdp->ConsumeBool();

  auto* record = MakeGarbageCollected<ImageRecord>(
      node, nullptr, fdp->ConsumeIntegral<uint64_t>(), frame_rect, root_rectF,
      hash, 0.0, nullptr);

  std::optional<uint64_t> viewport_size;
  if (fdp->ConsumeBool()) {
    viewport_size = fdp->ConsumeIntegral<uint64_t>();
  }
  calc->NotifyImagePaint(hash, record, viewport_size, is_video);

  if (fdp->ConsumeBool()) {
    calc->AssignImagePaintTime(hash, FuzzRect(fdp), FuzzTimeTicks(fdp));
  }
  if (fdp->ConsumeBool()) {
    calc->RemoveImageRecord(hash);
  }
}

void FuzzSingleTextPaint(FirstScreenCalculator* calc, FuzzedDataProvider* fdp) {
  Node* node = GetBodyNode();
  if (!node) {
    return;
  }
  gfx::Rect frame_rect = FuzzRect(fdp);
  gfx::RectF root_rectF = FuzzRectF(fdp);

  auto* record = MakeGarbageCollected<TextRecord>(
      node, fdp->ConsumeIntegral<uint64_t>(), root_rectF, frame_rect,
      root_rectF, fdp->ConsumeBool(), nullptr);

  calc->NotifyTextPaint(record, FuzzTimeTicks(fdp));
}

void FuzzBackgroundRatioPath(FirstScreenCalculator* calc,
                             FuzzedDataProvider* fdp) {
  Node* node = GetBodyNode();
  if (!node) {
    return;
  }
  // Large rect to trigger background_image_id_ path (ratio > 0.75)
  gfx::Rect big_rect(0, 0, 760, 570);
  gfx::RectF big_rectF(0.f, 0.f, 760.f, 570.f);
  MediaRecordIdHash hash_bg = fdp->ConsumeIntegral<uint64_t>();
  auto* bg_record = MakeGarbageCollected<ImageRecord>(
      node, nullptr, 1000u, big_rect, big_rectF, hash_bg, 0.0, nullptr);
  calc->NotifyImagePaint(hash_bg, bg_record, std::nullopt, false);

  // A smaller intersecting image to test DoesRectIntersectExistingRects
  gfx::Rect small_rect = FuzzRect(fdp);
  gfx::RectF small_rectF(small_rect.x(), small_rect.y(), small_rect.width(),
                         small_rect.height());
  MediaRecordIdHash hash_sm = fdp->ConsumeIntegral<uint64_t>();
  auto* sm_record = MakeGarbageCollected<ImageRecord>(
      node, nullptr, 200u, small_rect, small_rectF, hash_sm, 0.0, nullptr);
  calc->NotifyImagePaint(hash_sm, sm_record, std::nullopt, false);

  base::TimeTicks ts = FuzzTimeTicks(fdp);
  calc->AssignImagePaintTime(hash_bg, big_rect, ts);
}

void FuzzNearlyFinishedThreshold(FirstScreenCalculator* calc,
                                 FuzzedDataProvider* fdp) {
  Node* node = GetBodyNode();
  if (!node) {
    return;
  }
  int num_images = fdp->ConsumeIntegralInRange<int>(1, 6);
  for (int i = 0; i < num_images && fdp->remaining_bytes() > 8; ++i) {
    gfx::Rect rect = FuzzRect(fdp);
    gfx::RectF rectF(rect.x(), rect.y(), rect.width(), rect.height());
    MediaRecordIdHash hash = fdp->ConsumeIntegral<uint64_t>();
    auto* record = MakeGarbageCollected<ImageRecord>(node, nullptr, 100u, rect,
                                                     rectF, hash, 0.0, nullptr);
    calc->NotifyImagePaint(hash, record, std::nullopt, false);
    if (fdp->ConsumeBool()) {
      calc->AssignImagePaintTime(hash, rect, FuzzTimeTicks(fdp));
    }
  }

  int num_text = fdp->ConsumeIntegralInRange<int>(1, 4);
  for (int i = 0; i < num_text && fdp->remaining_bytes() > 8; ++i) {
    gfx::Rect rect = FuzzRect(fdp);
    gfx::RectF rectF(rect.x(), rect.y(), rect.width(), rect.height());
    auto* record = MakeGarbageCollected<TextRecord>(node, 50u, rectF, rect,
                                                    rectF, false, nullptr);
    calc->NotifyTextPaint(record, FuzzTimeTicks(fdp));
  }
}

void FuzzStateSequence(FirstScreenCalculator* calc, FuzzedDataProvider* fdp) {
  int num_ops = fdp->ConsumeIntegralInRange<int>(2, 12);
  for (int i = 0; i < num_ops && fdp->remaining_bytes() > 4; ++i) {
    int op = fdp->ConsumeIntegralInRange<int>(0, 6);
    switch (op) {
      case 0:
        FuzzSingleImagePaint(calc, fdp);
        break;
      case 1:
        FuzzSingleTextPaint(calc, fdp);
        break;
      case 2:
        calc->OnUserScroll();
        break;
      case 3:
        calc->HasUserScrolled();
        break;
      case 4: {
        std::vector<gfx::Rect> rects;
        calc->GetPaintRects(rects);
        break;
      }
      case 5:
        calc->RestartRecordingFirstScreenPaint();
        break;
      case 6: {
        MediaRecordIdHash hash = fdp->ConsumeIntegral<uint64_t>();
        calc->RemoveImageRecord(hash);
        break;
      }
    }
  }
}

void FuzzVideoPath(FirstScreenCalculator* calc, FuzzedDataProvider* fdp) {
  Node* node = GetBodyNode();
  if (!node) {
    return;
  }
  gfx::Rect frame_rect = FuzzRect(fdp);
  gfx::RectF root_rectF(frame_rect.x(), frame_rect.y(), frame_rect.width(),
                        frame_rect.height());
  MediaRecordIdHash hash = fdp->ConsumeIntegral<uint64_t>();

  auto* record = MakeGarbageCollected<ImageRecord>(node, nullptr, 500u,
                                                   frame_rect, root_rectF,
                                                   hash, 0.0, nullptr);
  // is_video=true triggers the rect halving path
  calc->NotifyImagePaint(hash, record, std::nullopt, true);
  calc->AssignImagePaintTime(hash, FuzzRect(fdp), FuzzTimeTicks(fdp));
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::Environment env;

  if (!blink::g_page_holder || !blink::GetBodyNode()) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);

  auto* calc = blink::MakeGarbageCollected<blink::FirstScreenCalculator>(
      &blink::g_page_holder->GetFrame());

  int32_t scenario = fdp.ConsumeIntegralInRange<int32_t>(0, 5);
  switch (scenario) {
    case 0:
      blink::FuzzSingleImagePaint(calc, &fdp);
      break;
    case 1:
      blink::FuzzSingleTextPaint(calc, &fdp);
      break;
    case 2:
      blink::FuzzBackgroundRatioPath(calc, &fdp);
      break;
    case 3:
      blink::FuzzNearlyFinishedThreshold(calc, &fdp);
      break;
    case 4:
      blink::FuzzStateSequence(calc, &fdp);
      break;
    case 5:
      blink::FuzzVideoPath(calc, &fdp);
      break;
    default:
      blink::FuzzStateSequence(calc, &fdp);
      break;
  }

  // Stop any pending timer before this iteration ends.
  calc->RestartRecordingFirstScreenPaint();

  return 0;
}
