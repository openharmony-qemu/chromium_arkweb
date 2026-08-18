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

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#define private public
#include "arkweb/chromium_ext/third_party/blink/renderer/core/paint/timing/first_screen_calculator.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/paint/timing/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"

namespace blink {
namespace {

class FirstScreenCalculatorTest : public RenderingTest {
 protected:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
    web_view_helper_.Initialize();
    dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
    local_frame_ = &dummy_page_holder_->GetFrame();
    calculator_ = MakeGarbageCollected<FirstScreenCalculator>(local_frame_.get());
  }
  void TearDown() override {}

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  raw_ptr<LocalFrame> local_frame_;
  Member<FirstScreenCalculator> calculator_;
  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_F(FirstScreenCalculatorTest, NotifyImagePaint) {
  ASSERT_NE(calculator_, nullptr);
  calculator_->user_scrolled_ = false;
  calculator_->NotifyImagePaint(0, nullptr, 10, false);
  EXPECT_EQ(calculator_->viewport_rect_.size().GetArea(), 0);
  calculator_->user_scrolled_ = true;
  gfx::Rect rect(0, 0, 10, 10);
  gfx::RectF root_rectF(0, 0, 10, 10);
  gfx::Rect root_rect(0, 0, 10, 10);
  ImageResourceContent* content = ImageResourceContent::CreateNotStarted();
  ASSERT_NE(content, nullptr);
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  ImageRecord record(one, content, 20, rect, root_rectF, 1, 0, nullptr);
  calculator_->NotifyImagePaint(0, &record, 10, false);
  EXPECT_EQ(calculator_->viewport_rect_.size().GetArea(), 0);
  calculator_->user_scrolled_ = false;
  record.lcp_rect_info_ = nullptr;
  calculator_->NotifyImagePaint(0, &record, 10, false);
  EXPECT_EQ(calculator_->viewport_rect_.size().GetArea(), 0);

  record.lcp_rect_info_ = std::make_unique<LCPRectInfo>(rect, root_rect);
  calculator_->user_scrolled_ = false;
  calculator_->NotifyImagePaint(0, &record, 10, false);
  EXPECT_EQ(calculator_->viewport_rect_.size().GetArea(), 480000);
}

TEST_F(FirstScreenCalculatorTest, NotifyTextPaint) {
  ASSERT_NE(calculator_, nullptr);
  base::TimeTicks timestamp = base::TimeTicks::Now();
  calculator_->user_scrolled_ = false;
  calculator_->NotifyTextPaint(nullptr, timestamp);
  EXPECT_EQ(calculator_->viewport_rect_.size().GetArea(), 0);

  calculator_->user_scrolled_ = true;
  gfx::Rect rect(0, 0, 10, 10);
  gfx::RectF root_rect(0, 0, 10, 10);
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  TextRecord record(one, 10, root_rect, rect, root_rect, 0, nullptr);
  calculator_->NotifyTextPaint(&record, timestamp);
  EXPECT_EQ(calculator_->viewport_rect_.size().GetArea(), 0);

  calculator_->user_scrolled_ = false;
  record.lcp_rect_info_ = nullptr;
  calculator_->NotifyTextPaint(&record, timestamp);
  EXPECT_EQ(calculator_->viewport_rect_.size().GetArea(), 0);
}

TEST_F(FirstScreenCalculatorTest, AssignImagePaintTime) {
  ASSERT_NE(calculator_, nullptr);
  calculator_->first_screen_paint_time_ = base::TimeTicks();
  calculator_->background_image_id_ = 10;
  
  gfx::Rect rect(0, 0, 10, 10);
  base::TimeTicks timestamp = base::TimeTicks::Now();
  calculator_->AssignImagePaintTime(10, rect, timestamp);
  EXPECT_EQ(calculator_->first_screen_paint_time_.ToInternalValue(), timestamp.ToInternalValue());

  calculator_->first_screen_paint_time_ = base::TimeTicks();
  calculator_->user_scrolled_ = true;
  calculator_->AssignImagePaintTime(9, rect, timestamp);
  EXPECT_EQ(calculator_->first_screen_paint_time_.ToInternalValue(), base::TimeTicks().ToInternalValue());

  calculator_->first_screen_paint_time_ = base::TimeTicks();
  calculator_->user_scrolled_ = false;
  calculator_->AssignImagePaintTime(9, rect, timestamp);
  EXPECT_EQ(calculator_->first_screen_paint_time_.ToInternalValue(), base::TimeTicks().ToInternalValue());

  calculator_->first_screen_paint_time_ = base::TimeTicks();
  calculator_->user_scrolled_ = true;
  calculator_->AssignImagePaintTime(9, rect, timestamp);
  EXPECT_EQ(calculator_->first_screen_paint_time_.ToInternalValue(), base::TimeTicks().ToInternalValue());
}

TEST_F(FirstScreenCalculatorTest, RemoveImageRecord) {

  ASSERT_NE(calculator_, nullptr);
  calculator_->user_scrolled_ = true;
  EXPECT_EQ(calculator_->RemoveImageRecord(10), false);

  ASSERT_NE(calculator_, nullptr);
  calculator_->user_scrolled_ = false;
  FirstScreenCalculator::PaintRectInfo info;
  calculator_->image_rects_map_[10] = info;
  calculator_->image_rects_map_[10].paint_time_ = base::TimeTicks::Now();
  EXPECT_EQ(calculator_->RemoveImageRecord(10), false);

  calculator_->image_rects_map_[10].paint_time_ = base::TimeTicks();
  EXPECT_EQ(calculator_->RemoveImageRecord(10), true);
}

TEST_F(FirstScreenCalculatorTest, OnUserScroll) {
  ASSERT_NE(calculator_, nullptr);
  calculator_->user_scrolled_ = false;
  calculator_->OnUserScroll();
  EXPECT_EQ(calculator_->HasUserScrolled(), true);
}

TEST_F(FirstScreenCalculatorTest, RestartRecordingFirstScreenPaint) {
  ASSERT_NE(calculator_, nullptr);
  calculator_->RestartRecordingFirstScreenPaint();
  EXPECT_EQ(calculator_->user_scrolled_, false);
  EXPECT_EQ(calculator_->nearly_finished_, false);
  EXPECT_EQ(calculator_->first_screen_paint_time_, base::TimeTicks());
  EXPECT_EQ(calculator_->background_image_id_, 0ul);
}

TEST_F(FirstScreenCalculatorTest, OnFirstScreenInvoked) {
  ASSERT_NE(calculator_, nullptr);
  calculator_->first_screen_paint_time_ = base::TimeTicks();
  calculator_->navigation_start_time_ = base::TimeTicks();
  calculator_->OnFirstScreenInvoked();
  EXPECT_EQ(calculator_->navigation_start_time_, base::TimeTicks());

  calculator_->first_screen_paint_time_ = base::TimeTicks::Now();
  calculator_->OnFirstScreenInvoked();
  EXPECT_NE(calculator_->navigation_start_time_, base::TimeTicks());
}

TEST_F(FirstScreenCalculatorTest, RestartTimerForFirstScreenDetection) {
  ASSERT_NE(calculator_, nullptr);
  calculator_->user_scrolled_ = true;
  calculator_->RestartTimerForFirstScreenDetection();
  EXPECT_EQ(calculator_->timer_.IsRunning(), false);

  calculator_->user_scrolled_ = false;
  calculator_->RestartTimerForFirstScreenDetection();
  EXPECT_EQ(calculator_->timer_.IsRunning(), true);
}

TEST_F(FirstScreenCalculatorTest, IsRectContainedByExistingRects) {
  ASSERT_NE(calculator_, nullptr);
  gfx::Rect root_rect(0, 0, 10, 10);
  EXPECT_EQ(calculator_->IsRectContainedByExistingRects(root_rect), false);

  gfx::Rect rect1(0, 0, 10, 10);
  FirstScreenCalculator::PaintRectInfo info1(rect1, base::TimeTicks());
  calculator_->image_rects_map_[10] = info1;
  EXPECT_EQ(calculator_->IsRectContainedByExistingRects(root_rect), true);

  gfx::Rect rect2(0, 0, 20, 20);
  FirstScreenCalculator::PaintRectInfo info2(rect2, base::TimeTicks());
  calculator_->image_rects_map_[10] = info2;
  EXPECT_EQ(calculator_->IsRectContainedByExistingRects(root_rect), true);

  calculator_->image_rects_map_.clear();
  calculator_->text_paint_rects_.emplace_back(info1);
  EXPECT_EQ(calculator_->IsRectContainedByExistingRects(root_rect), true);

  calculator_->text_paint_rects_.clear();
  calculator_->text_paint_rects_.emplace_back(info2);
  EXPECT_EQ(calculator_->IsRectContainedByExistingRects(root_rect), true);
}

TEST_F(FirstScreenCalculatorTest, DoesRectIntersectExistingRects) {
  ASSERT_NE(calculator_, nullptr);
  gfx::Rect root_rect(0, 0, 10, 10);
  EXPECT_EQ(calculator_->DoesRectIntersectExistingRects(root_rect), false);

  gfx::Rect rect1(0, 0, 10, 10);
  FirstScreenCalculator::PaintRectInfo info1(rect1, base::TimeTicks());
  calculator_->image_rects_map_[10] = info1;
  EXPECT_EQ(calculator_->DoesRectIntersectExistingRects(root_rect), false);

  gfx::Rect rect2(0, 0, 20, 20);
  FirstScreenCalculator::PaintRectInfo info2(rect2, base::TimeTicks());
  calculator_->image_rects_map_[10] = info2;
  EXPECT_EQ(calculator_->DoesRectIntersectExistingRects(root_rect), true);

  calculator_->image_rects_map_.clear();
  calculator_->text_paint_rects_.emplace_back(info1);
  EXPECT_EQ(calculator_->DoesRectIntersectExistingRects(root_rect), false);

  calculator_->text_paint_rects_.clear();
  calculator_->text_paint_rects_.emplace_back(info2);
  EXPECT_EQ(calculator_->DoesRectIntersectExistingRects(root_rect), true);
}

TEST_F(FirstScreenCalculatorTest, RemoveExistingRectsContainedByRect) {
  ASSERT_NE(calculator_, nullptr);
  gfx::Rect root_rect(0, 0, 10, 10);
  calculator_->RemoveExistingRectsContainedByRect(root_rect);
  EXPECT_EQ(calculator_->image_rects_map_.size(), 0ul);

  gfx::Rect rect1(0, 0, 5, 5);
  FirstScreenCalculator::PaintRectInfo info1(rect1, base::TimeTicks());
  calculator_->image_rects_map_[10] = info1;
  calculator_->RemoveExistingRectsContainedByRect(root_rect);
  EXPECT_EQ(calculator_->image_rects_map_.size(), 0ul);

  gfx::Rect rect2(0, 0, 20, 20);
  FirstScreenCalculator::PaintRectInfo info2(rect2, base::TimeTicks());
  calculator_->image_rects_map_[10] = info2;
  calculator_->RemoveExistingRectsContainedByRect(root_rect);
  EXPECT_EQ(calculator_->image_rects_map_.size(), 1ul);

  calculator_->image_rects_map_.clear();
  calculator_->text_paint_rects_.clear();
  calculator_->text_paint_rects_.emplace_back(info1);
  calculator_->RemoveExistingRectsContainedByRect(root_rect);
  EXPECT_EQ(calculator_->text_paint_rects_.size(), 0ul);

  calculator_->text_paint_rects_.emplace_back(info2);
  calculator_->RemoveExistingRectsContainedByRect(root_rect);
  EXPECT_EQ(calculator_->text_paint_rects_.size(), 1ul);
}

TEST_F(FirstScreenCalculatorTest, IsRectTooSmallWhenNearlyFinished) {
  ASSERT_NE(calculator_, nullptr);
  calculator_->viewport_rect_ = gfx::Rect(0,0,0,0);
  gfx::Rect root_rect(0, 0, 10, 10);
  EXPECT_EQ(calculator_->IsRectTooSmallWhenNearlyFinished(root_rect), false);

  gfx::Rect rect1(0, 0, 10, 10);
  calculator_->viewport_rect_ = rect1;
  calculator_->nearly_finished_ = false;
  calculator_->occupied_rect_ = rect1;
  EXPECT_EQ(calculator_->IsRectTooSmallWhenNearlyFinished(root_rect), false);
  EXPECT_EQ(calculator_->nearly_finished_, true);
}

TEST_F(FirstScreenCalculatorTest, GetViewportAreaAndTrimRect) {
  ASSERT_NE(calculator_, nullptr);
  calculator_->viewport_rect_ = gfx::Rect(0,0,800,0);
  gfx::Rect root_rect(0, 0, 10, 10);
  EXPECT_EQ(calculator_->GetViewportAreaAndTrimRect(root_rect), true);
  EXPECT_EQ(calculator_->viewport_rect_, gfx::Rect(0,0,800,600));

  gfx::Rect rect1(1000, 1000, 10, 10);
  EXPECT_EQ(calculator_->GetViewportAreaAndTrimRect(rect1), false);
}

TEST_F(FirstScreenCalculatorTest, NotifyImagePaintBackgroundImage) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  gfx::Rect rect(0, 0, 700, 700);
  gfx::RectF root_rect_f(0, 0, 700, 700);
  ImageResourceContent* content = ImageResourceContent::CreateNotStarted();
  ASSERT_NE(content, nullptr);
  ImageRecord record(one, content, 20, rect, root_rect_f, 1, 0, nullptr);
  record.lcp_rect_info_ = std::make_unique<LCPRectInfo>(rect, rect);
  calculator_->user_scrolled_ = false;
  calculator_->NotifyImagePaint(1, &record, 10, false);
  EXPECT_EQ(calculator_->background_image_id_, 1u);
  EXPECT_EQ(calculator_->image_rects_map_.size(), 0u);
}

TEST_F(FirstScreenCalculatorTest, NotifyImagePaintBackgroundImageIntersected) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  gfx::Rect existing_rect(0, 0, 700, 700);
  FirstScreenCalculator::PaintRectInfo info(existing_rect, base::TimeTicks::Now());
  calculator_->image_rects_map_[2] = info;
  gfx::Rect rect(0, 0, 700, 700);
  gfx::RectF root_rect_f(0, 0, 700, 700);
  ImageResourceContent* content = ImageResourceContent::CreateNotStarted();
  ASSERT_NE(content, nullptr);
  ImageRecord record(one, content, 20, rect, root_rect_f, 1, 0, nullptr);
  record.lcp_rect_info_ = std::make_unique<LCPRectInfo>(rect, rect);
  calculator_->user_scrolled_ = false;
  calculator_->NotifyImagePaint(1, &record, 10, false);
  EXPECT_NE(calculator_->background_image_id_, 1u);
}

TEST_F(FirstScreenCalculatorTest, NotifyImagePaintVideo) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  gfx::Rect rect(100, 100, 100, 100);
  gfx::RectF root_rect_f(100, 100, 100, 100);
  ImageResourceContent* content = ImageResourceContent::CreateNotStarted();
  ASSERT_NE(content, nullptr);
  ImageRecord record(one, content, 20, rect, root_rect_f, 1, 0, nullptr);
  record.lcp_rect_info_ = std::make_unique<LCPRectInfo>(rect, rect);
  calculator_->user_scrolled_ = false;
  calculator_->NotifyImagePaint(1, &record, 10, true);
  EXPECT_EQ(calculator_->image_rects_map_.size(), 1u);
  auto it = calculator_->image_rects_map_.find(1);
  ASSERT_NE(it, calculator_->image_rects_map_.end());
  EXPECT_EQ(it->second.rect_.x(), 50);
  EXPECT_EQ(it->second.rect_.y(), 50);
}

TEST_F(FirstScreenCalculatorTest, NotifyImagePaintExistingHash) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  gfx::Rect existing_rect(0, 0, 100, 100);
  FirstScreenCalculator::PaintRectInfo info(existing_rect, base::TimeTicks());
  calculator_->image_rects_map_[1] = info;
  gfx::Rect rect(0, 0, 100, 100);
  gfx::RectF root_rect_f(0, 0, 100, 100);
  ImageResourceContent* content = ImageResourceContent::CreateNotStarted();
  ASSERT_NE(content, nullptr);
  ImageRecord record(one, content, 20, rect, root_rect_f, 1, 0, nullptr);
  record.lcp_rect_info_ = std::make_unique<LCPRectInfo>(rect, rect);
  calculator_->user_scrolled_ = false;
  calculator_->NotifyImagePaint(1, &record, 10, false);
  EXPECT_EQ(calculator_->image_rects_map_.size(), 1u);
}

TEST_F(FirstScreenCalculatorTest, NotifyImagePaintContainedRect) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  gfx::Rect existing_rect(0, 0, 200, 200);
  FirstScreenCalculator::PaintRectInfo info(existing_rect, base::TimeTicks());
  calculator_->image_rects_map_[1] = info;
  gfx::Rect rect(0, 0, 10, 10);
  gfx::RectF root_rect_f(0, 0, 10, 10);
  ImageResourceContent* content = ImageResourceContent::CreateNotStarted();
  ASSERT_NE(content, nullptr);
  ImageRecord record(one, content, 20, rect, root_rect_f, 2, 0, nullptr);
  record.lcp_rect_info_ = std::make_unique<LCPRectInfo>(rect, rect);
  calculator_->user_scrolled_ = false;
  calculator_->NotifyImagePaint(2, &record, 10, false);
  EXPECT_EQ(calculator_->image_rects_map_.size(), 1u);
}

TEST_F(FirstScreenCalculatorTest, NotifyImagePaintTooSmall) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  gfx::Rect existing_rect(300, 300, 100, 100);
  FirstScreenCalculator::PaintRectInfo info(existing_rect, base::TimeTicks());
  calculator_->image_rects_map_[1] = info;
  calculator_->nearly_finished_ = true;
  calculator_->viewport_rect_ = gfx::Rect(0, 0, 800, 600);
  gfx::Rect rect(0, 0, 2, 2);
  gfx::RectF root_rect_f(0, 0, 2, 2);
  ImageResourceContent* content = ImageResourceContent::CreateNotStarted();
  ASSERT_NE(content, nullptr);
  ImageRecord record(one, content, 20, rect, root_rect_f, 2, 0, nullptr);
  record.lcp_rect_info_ = std::make_unique<LCPRectInfo>(rect, rect);
  calculator_->user_scrolled_ = false;
  calculator_->NotifyImagePaint(2, &record, 10, false);
  EXPECT_EQ(calculator_->image_rects_map_.size(), 1u);
}

TEST_F(FirstScreenCalculatorTest, NotifyImagePaintIntersected) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  gfx::Rect existing_rect(0, 0, 200, 200);
  FirstScreenCalculator::PaintRectInfo info(existing_rect, base::TimeTicks());
  calculator_->image_rects_map_[1] = info;
  gfx::Rect rect(100, 100, 200, 200);
  gfx::RectF root_rect_f(100, 100, 200, 200);
  ImageResourceContent* content = ImageResourceContent::CreateNotStarted();
  ASSERT_NE(content, nullptr);
  ImageRecord record(one, content, 20, rect, root_rect_f, 2, 0, nullptr);
  record.lcp_rect_info_ = std::make_unique<LCPRectInfo>(rect, rect);
  calculator_->user_scrolled_ = false;
  calculator_->NotifyImagePaint(2, &record, 10, false);
  EXPECT_EQ(calculator_->image_rects_map_.size(), 2u);
  EXPECT_EQ(calculator_->intersected_image_ids_.size(), 1u);
  EXPECT_EQ(calculator_->intersected_image_ids_[0], 2u);
}

TEST_F(FirstScreenCalculatorTest, NotifyImagePaintSecondRecordNoIntersect) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  gfx::Rect existing_rect(0, 0, 100, 100);
  FirstScreenCalculator::PaintRectInfo info(existing_rect, base::TimeTicks());
  calculator_->image_rects_map_[1] = info;
  gfx::Rect rect(400, 400, 100, 100);
  gfx::RectF root_rect_f(400, 400, 100, 100);
  ImageResourceContent* content = ImageResourceContent::CreateNotStarted();
  ASSERT_NE(content, nullptr);
  ImageRecord record(one, content, 20, rect, root_rect_f, 2, 0, nullptr);
  record.lcp_rect_info_ = std::make_unique<LCPRectInfo>(rect, rect);
  calculator_->user_scrolled_ = false;
  calculator_->NotifyImagePaint(2, &record, 10, false);
  EXPECT_EQ(calculator_->image_rects_map_.size(), 2u);
  EXPECT_EQ(calculator_->intersected_image_ids_.size(), 0u);
  EXPECT_EQ(calculator_->timer_.IsRunning(), true);
}

TEST_F(FirstScreenCalculatorTest, NotifyImagePaintGetViewportFails) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  gfx::Rect rect(1000, 1000, 10, 10);
  gfx::RectF root_rect_f(1000, 1000, 10, 10);
  ImageResourceContent* content = ImageResourceContent::CreateNotStarted();
  ASSERT_NE(content, nullptr);
  ImageRecord record(one, content, 20, rect, root_rect_f, 1, 0, nullptr);
  record.lcp_rect_info_ = std::make_unique<LCPRectInfo>(rect, rect);
  calculator_->user_scrolled_ = false;
  calculator_->NotifyImagePaint(1, &record, 10, false);
  EXPECT_EQ(calculator_->image_rects_map_.size(), 0u);
}

TEST_F(FirstScreenCalculatorTest, NotifyTextPaintWithValidRecord) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  base::TimeTicks timestamp = base::TimeTicks::Now();
  gfx::Rect rect(0, 0, 100, 100);
  gfx::RectF rect_f(0, 0, 100, 100);
  TextRecord record(one, 10, rect_f, rect, rect_f, 0, nullptr);
  record.lcp_rect_info_ = std::make_unique<LCPRectInfo>(rect, rect);
  calculator_->user_scrolled_ = false;
  calculator_->NotifyTextPaint(&record, timestamp);
  EXPECT_EQ(calculator_->text_paint_rects_.size(), 1u);
  EXPECT_EQ(calculator_->first_screen_paint_time_, timestamp);
  EXPECT_EQ(calculator_->timer_.IsRunning(), true);
}

TEST_F(FirstScreenCalculatorTest, NotifyTextPaintWithExistingPaintTime) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  base::TimeTicks old_time = base::TimeTicks::Now() - base::Seconds(1);
  base::TimeTicks new_time = base::TimeTicks::Now();
  calculator_->first_screen_paint_time_ = old_time;
  gfx::Rect rect(0, 0, 100, 100);
  gfx::RectF rect_f(0, 0, 100, 100);
  TextRecord record(one, 10, rect_f, rect, rect_f, 0, nullptr);
  record.lcp_rect_info_ = std::make_unique<LCPRectInfo>(rect, rect);
  calculator_->user_scrolled_ = false;
  calculator_->NotifyTextPaint(&record, new_time);
  EXPECT_EQ(calculator_->first_screen_paint_time_, new_time);
}

TEST_F(FirstScreenCalculatorTest, NotifyTextPaintSecondRecord) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  gfx::Rect existing_rect(300, 300, 100, 100);
  FirstScreenCalculator::PaintRectInfo info(existing_rect, base::TimeTicks());
  calculator_->text_paint_rects_.emplace_back(info);
  base::TimeTicks timestamp = base::TimeTicks::Now();
  gfx::Rect rect(0, 0, 100, 100);
  gfx::RectF rect_f(0, 0, 100, 100);
  TextRecord record(one, 10, rect_f, rect, rect_f, 0, nullptr);
  record.lcp_rect_info_ = std::make_unique<LCPRectInfo>(rect, rect);
  calculator_->user_scrolled_ = false;
  calculator_->first_screen_paint_time_ = base::TimeTicks();
  calculator_->NotifyTextPaint(&record, timestamp);
  EXPECT_EQ(calculator_->text_paint_rects_.size(), 2u);
  EXPECT_EQ(calculator_->first_screen_paint_time_, timestamp);
}

TEST_F(FirstScreenCalculatorTest, NotifyTextPaintContainedRect) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  gfx::Rect existing_rect(0, 0, 200, 200);
  FirstScreenCalculator::PaintRectInfo info(existing_rect, base::TimeTicks());
  calculator_->text_paint_rects_.emplace_back(info);
  base::TimeTicks timestamp = base::TimeTicks::Now();
  gfx::Rect rect(0, 0, 10, 10);
  gfx::RectF rect_f(0, 0, 10, 10);
  TextRecord record(one, 10, rect_f, rect, rect_f, 0, nullptr);
  record.lcp_rect_info_ = std::make_unique<LCPRectInfo>(rect, rect);
  calculator_->user_scrolled_ = false;
  calculator_->NotifyTextPaint(&record, timestamp);
  EXPECT_EQ(calculator_->text_paint_rects_.size(), 1u);
}

TEST_F(FirstScreenCalculatorTest, NotifyTextPaintTooSmall) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  gfx::Rect existing_rect(300, 300, 100, 100);
  FirstScreenCalculator::PaintRectInfo info(existing_rect, base::TimeTicks());
  calculator_->text_paint_rects_.emplace_back(info);
  calculator_->nearly_finished_ = true;
  calculator_->viewport_rect_ = gfx::Rect(0, 0, 800, 600);
  base::TimeTicks timestamp = base::TimeTicks::Now();
  gfx::Rect rect(0, 0, 2, 2);
  gfx::RectF rect_f(0, 0, 2, 2);
  TextRecord record(one, 10, rect_f, rect, rect_f, 0, nullptr);
  record.lcp_rect_info_ = std::make_unique<LCPRectInfo>(rect, rect);
  calculator_->user_scrolled_ = false;
  calculator_->NotifyTextPaint(&record, timestamp);
  EXPECT_EQ(calculator_->text_paint_rects_.size(), 1u);
}

TEST_F(FirstScreenCalculatorTest, NotifyTextPaintIntersects) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  gfx::Rect existing_rect(0, 0, 200, 200);
  FirstScreenCalculator::PaintRectInfo info(existing_rect, base::TimeTicks());
  calculator_->text_paint_rects_.emplace_back(info);
  calculator_->viewport_rect_ = gfx::Rect(0, 0, 800, 600);
  base::TimeTicks timestamp = base::TimeTicks::Now();
  gfx::Rect rect(100, 100, 200, 200);
  gfx::RectF rect_f(100, 100, 200, 200);
  TextRecord record(one, 10, rect_f, rect, rect_f, 0, nullptr);
  record.lcp_rect_info_ = std::make_unique<LCPRectInfo>(rect, rect);
  calculator_->user_scrolled_ = false;
  calculator_->NotifyTextPaint(&record, timestamp);
  EXPECT_EQ(calculator_->text_paint_rects_.size(), 2u);
  EXPECT_EQ(calculator_->timer_.IsRunning(), false);
}

TEST_F(FirstScreenCalculatorTest, NotifyTextPaintOldTimestamp) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  base::TimeTicks new_time = base::TimeTicks::Now();
  base::TimeTicks old_time = new_time - base::Seconds(1);
  calculator_->first_screen_paint_time_ = new_time;
  gfx::Rect existing_rect(300, 300, 100, 100);
  FirstScreenCalculator::PaintRectInfo info(existing_rect, base::TimeTicks());
  calculator_->text_paint_rects_.emplace_back(info);
  gfx::Rect rect(0, 0, 100, 100);
  gfx::RectF rect_f(0, 0, 100, 100);
  TextRecord record(one, 10, rect_f, rect, rect_f, 0, nullptr);
  record.lcp_rect_info_ = std::make_unique<LCPRectInfo>(rect, rect);
  calculator_->user_scrolled_ = false;
  calculator_->NotifyTextPaint(&record, old_time);
  EXPECT_EQ(calculator_->first_screen_paint_time_, new_time);
}

TEST_F(FirstScreenCalculatorTest, NotifyTextPaintGetViewportFails) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  base::TimeTicks timestamp = base::TimeTicks::Now();
  gfx::Rect rect(1000, 1000, 10, 10);
  gfx::RectF rect_f(1000, 1000, 10, 10);
  TextRecord record(one, 10, rect_f, rect, rect_f, 0, nullptr);
  record.lcp_rect_info_ = std::make_unique<LCPRectInfo>(rect, rect);
  calculator_->user_scrolled_ = false;
  calculator_->NotifyTextPaint(&record, timestamp);
  EXPECT_EQ(calculator_->text_paint_rects_.size(), 0u);
}

TEST_F(FirstScreenCalculatorTest, AssignImagePaintTimeNullPaintTimeInMap) {
  gfx::Rect rect(0, 0, 100, 100);
  calculator_->background_image_id_ = 99;
  FirstScreenCalculator::PaintRectInfo info(rect, base::TimeTicks());
  calculator_->image_rects_map_[1] = info;
  base::TimeTicks timestamp = base::TimeTicks::Now();
  calculator_->user_scrolled_ = true;
  calculator_->first_screen_paint_time_ = base::TimeTicks();
  calculator_->AssignImagePaintTime(1, rect, timestamp);
  EXPECT_EQ(calculator_->image_rects_map_[1].paint_time_, timestamp);
}

TEST_F(FirstScreenCalculatorTest, AssignImagePaintTimeNonNullPaintTimeInMap) {
  gfx::Rect rect(0, 0, 100, 100);
  calculator_->background_image_id_ = 99;
  base::TimeTicks existing_time = base::TimeTicks::Now() - base::Seconds(1);
  FirstScreenCalculator::PaintRectInfo info(rect, existing_time);
  calculator_->image_rects_map_[1] = info;
  base::TimeTicks timestamp = base::TimeTicks::Now();
  calculator_->user_scrolled_ = true;
  calculator_->first_screen_paint_time_ = base::TimeTicks();
  calculator_->AssignImagePaintTime(1, rect, timestamp);
  EXPECT_EQ(calculator_->image_rects_map_[1].paint_time_, existing_time);
}

TEST_F(FirstScreenCalculatorTest, AssignImagePaintTimeIntersectedId) {
  gfx::Rect rect(0, 0, 100, 100);
  calculator_->background_image_id_ = 99;
  FirstScreenCalculator::PaintRectInfo info(rect, base::TimeTicks());
  calculator_->image_rects_map_[1] = info;
  calculator_->intersected_image_ids_.emplace_back(1);
  base::TimeTicks old_time = base::TimeTicks::Now() - base::Seconds(1);
  base::TimeTicks timestamp = base::TimeTicks::Now();
  calculator_->first_screen_paint_time_ = old_time;
  calculator_->user_scrolled_ = false;
  calculator_->AssignImagePaintTime(1, rect, timestamp);
  EXPECT_EQ(calculator_->image_rects_map_[1].paint_time_, timestamp);
  EXPECT_EQ(calculator_->first_screen_paint_time_, old_time);
}

TEST_F(FirstScreenCalculatorTest, AssignImagePaintTimeUpdateTimestamp) {
  gfx::Rect rect(0, 0, 100, 100);
  calculator_->background_image_id_ = 99;
  base::TimeTicks old_time = base::TimeTicks::Now() - base::Seconds(1);
  base::TimeTicks new_time = base::TimeTicks::Now();
  calculator_->first_screen_paint_time_ = old_time;
  calculator_->user_scrolled_ = false;
  FirstScreenCalculator::PaintRectInfo info(rect, base::TimeTicks());
  calculator_->image_rects_map_[1] = info;
  calculator_->AssignImagePaintTime(1, rect, new_time);
  EXPECT_EQ(calculator_->first_screen_paint_time_, new_time);
}

TEST_F(FirstScreenCalculatorTest, AssignImagePaintTimeOlderTimestamp) {
  gfx::Rect rect(0, 0, 100, 100);
  calculator_->background_image_id_ = 99;
  base::TimeTicks new_time = base::TimeTicks::Now();
  base::TimeTicks old_time = new_time - base::Seconds(1);
  calculator_->first_screen_paint_time_ = new_time;
  calculator_->user_scrolled_ = false;
  FirstScreenCalculator::PaintRectInfo info(rect, base::TimeTicks());
  calculator_->image_rects_map_[1] = info;
  calculator_->AssignImagePaintTime(1, rect, old_time);
  EXPECT_EQ(calculator_->first_screen_paint_time_, new_time);
  EXPECT_EQ(calculator_->timer_.IsRunning(), true);
}

TEST_F(FirstScreenCalculatorTest, IsRectTooSmallWhenNearlyFinishedNotExceeded) {
  calculator_->viewport_rect_ = gfx::Rect(0, 0, 800, 600);
  calculator_->nearly_finished_ = false;
  calculator_->occupied_rect_ = gfx::Rect();
  gfx::Rect rect(0, 0, 100, 100);
  EXPECT_EQ(calculator_->IsRectTooSmallWhenNearlyFinished(rect), false);
  EXPECT_EQ(calculator_->nearly_finished_, false);
}

TEST_F(FirstScreenCalculatorTest, IsRectTooSmallWhenNearlyFinishedSmallRect) {
  calculator_->viewport_rect_ = gfx::Rect(0, 0, 800, 600);
  calculator_->nearly_finished_ = true;
  gfx::Rect rect(0, 0, 1, 1);
  EXPECT_EQ(calculator_->IsRectTooSmallWhenNearlyFinished(rect), true);
}

TEST_F(FirstScreenCalculatorTest, IsRectTooSmallWhenNearlyFinishedLargeRect) {
  calculator_->viewport_rect_ = gfx::Rect(0, 0, 800, 600);
  calculator_->nearly_finished_ = true;
  gfx::Rect rect(0, 0, 100, 100);
  EXPECT_EQ(calculator_->IsRectTooSmallWhenNearlyFinished(rect), false);
}

TEST_F(FirstScreenCalculatorTest, GetPaintRects) {
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(200, 200, 100, 100);
  gfx::Rect rect3(400, 400, 100, 100);
  FirstScreenCalculator::PaintRectInfo img_info1(rect1, base::TimeTicks::Now());
  calculator_->image_rects_map_[1] = img_info1;
  FirstScreenCalculator::PaintRectInfo img_info2(rect2, base::TimeTicks());
  calculator_->image_rects_map_[2] = img_info2;
  FirstScreenCalculator::PaintRectInfo txt_info1(rect3, base::TimeTicks::Now());
  calculator_->text_paint_rects_.emplace_back(txt_info1);
  FirstScreenCalculator::PaintRectInfo txt_info2(rect2, base::TimeTicks());
  calculator_->text_paint_rects_.emplace_back(txt_info2);
  std::vector<gfx::Rect> paint_rects;
  calculator_->GetPaintRects(paint_rects);
  EXPECT_EQ(paint_rects.size(), 2u);
  EXPECT_EQ(paint_rects[0], rect1);
  EXPECT_EQ(paint_rects[1], rect3);
}

TEST_F(FirstScreenCalculatorTest, NotifyTextPaintNewTimestampNonEmpty) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  ASSERT_NE(one, nullptr);
  base::TimeTicks old_time = base::TimeTicks::Now() - base::Seconds(1);
  base::TimeTicks new_time = base::TimeTicks::Now();
  calculator_->first_screen_paint_time_ = old_time;
  gfx::Rect existing_rect(300, 300, 100, 100);
  FirstScreenCalculator::PaintRectInfo info(existing_rect, base::TimeTicks());
  calculator_->text_paint_rects_.emplace_back(info);
  calculator_->viewport_rect_ = gfx::Rect(0, 0, 800, 600);
  calculator_->nearly_finished_ = false;
  calculator_->occupied_rect_ = gfx::Rect(0, 0, 10, 10);
  gfx::Rect rect(0, 0, 100, 100);
  gfx::RectF rect_f(0, 0, 100, 100);
  TextRecord record(one, 10, rect_f, rect, rect_f, 0, nullptr);
  record.lcp_rect_info_ = std::make_unique<LCPRectInfo>(rect, rect);
  calculator_->user_scrolled_ = false;
  calculator_->NotifyTextPaint(&record, new_time);
  EXPECT_EQ(calculator_->text_paint_rects_.size(), 2u);
  EXPECT_EQ(calculator_->first_screen_paint_time_, new_time);
  EXPECT_EQ(calculator_->timer_.IsRunning(), true);
}

// GetPaintRects (line 417): background_image_id_ != 0 true arm. The existing
// GetPaintRects test leaves background_image_id_ at 0 (false arm); this sets a
// background id+rect so the final emplace_back branch is taken.
TEST_F(FirstScreenCalculatorTest, GetPaintRectsWithBackgroundImage) {
  gfx::Rect bg_rect(0, 0, 700, 700);
  calculator_->background_image_id_ = 5;
  calculator_->background_rect_ = bg_rect;
  std::vector<gfx::Rect> paint_rects;
  calculator_->GetPaintRects(paint_rects);
  ASSERT_EQ(paint_rects.size(), 1u);
  EXPECT_EQ(paint_rects[0], bg_rect);
}

// OnFirstScreenInvoked (line 53): navigation_start_time_ == navigation_start_time
// false arm of the first condition. A first call (non-null paint time) records
// navigation_start_time_; the second call hits the equal branch and skips the
// OnFirstScreenPaint block. The existing OnFirstScreenInvoked test always has
// navigation_start_time_ != navigation_start_time (true arm only).
TEST_F(FirstScreenCalculatorTest, OnFirstScreenInvokedSameNavigationStart) {
  calculator_->first_screen_paint_time_ = base::TimeTicks::Now();
  calculator_->navigation_start_time_ = base::TimeTicks();
  calculator_->OnFirstScreenInvoked();
  ASSERT_NE(calculator_->navigation_start_time_, base::TimeTicks());
  base::TimeTicks first_nav = calculator_->navigation_start_time_;
  calculator_->OnFirstScreenInvoked();
  EXPECT_EQ(calculator_->navigation_start_time_, first_nav);
}

// Null-local-frame guards: a calculator constructed without a LocalFrame reaches
// the early-return arms that the main fixture (always with a frame) cannot:
// OnFirstScreenInvoked `if (local_frame_)` false (line 48),
// RestartTimerForFirstScreenDetection `if (!local_frame_)` true (line 115),
// GetViewportAreaAndTrimRect local_frame_ short-circuit/else (line 360), and
// OnUserScroll `if (local_frame_)` false (line 378). The constructor only stores
// the pointer, so a nullptr frame is safe; every guard returns early.
TEST_F(FirstScreenCalculatorTest, NullLocalFrameGuards) {
  FirstScreenCalculator* calc =
      MakeGarbageCollected<FirstScreenCalculator>(nullptr);
  ASSERT_NE(calc, nullptr);

  // line 48 false arm.
  calc->OnFirstScreenInvoked();

  // line 115 true arm (!local_frame_); timer stays stopped.
  calc->user_scrolled_ = false;
  calc->RestartTimerForFirstScreenDetection();
  EXPECT_FALSE(calc->timer_.IsRunning());

  // line 360 local_frame_ short-circuit -> else -> return false.
  gfx::Rect rect(0, 0, 10, 10);
  EXPECT_FALSE(calc->GetViewportAreaAndTrimRect(rect));

  // line 378 false arm (blank-screen block skipped on null frame).
  calc->OnUserScroll();
  EXPECT_TRUE(calc->HasUserScrolled());
}

}  // namespace
}  // namespace blink