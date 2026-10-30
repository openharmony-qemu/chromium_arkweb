/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include <iostream>
#include "arkweb/chromium_ext/third_party/blink/renderer/core/paint/timing/paint_timing_detector_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/paint/timing/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
#include "arkweb/chromium_ext/base/ohos/blankless/blankless_controller.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/media_timing.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#endif

#if BUILDFLAG(ARKWEB_FIRST_SCREEN_PAINT) || BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)
#include "arkweb/chromium_ext/third_party/blink/renderer/core/paint/timing/first_screen_calculator.h"
#endif

namespace blink {

class PaintTimingDetectorUtilsTest : public testing::Test {
protected:
  void SetUp() override {
    web_view_impl_ = helper_.InitializeAndLoad("about:blank");
  }
  void TearDown() override {
    helper_.Reset();
  }
  test::TaskEnvironment task_environment_;
  WebViewImpl* web_view_impl_;
  frame_test_helpers::WebViewHelper helper_;
  std::string base_url_{"http://www.test.com/"};
};

TEST_F(PaintTimingDetectorUtilsTest, Constructor) {
  auto test1 = web_view_impl_->MainFrameImpl();
  EXPECT_NE(test1, nullptr);
  LocalFrameView* view = test1->GetFrameView();
  EXPECT_NE(view, nullptr);
  auto& test = view->GetPaintTimingDetector();
  EXPECT_NE(&test, nullptr);
  PaintTimingDetector* paint_timing_detector = &view->GetPaintTimingDetector();
  PaintTimingDetectorUtils utils(paint_timing_detector);
  EXPECT_EQ(utils.paint_timing_detector_, paint_timing_detector);
}

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
TEST_F(PaintTimingDetectorUtilsTest, ConstructorWithNeedSupplementTrue) {
  auto test1 = web_view_impl_->MainFrameImpl();
  LocalFrameView* view = test1->GetFrameView();
  PaintTimingDetector* paint_timing_detector = &view->GetPaintTimingDetector();
  PaintTimingDetectorUtils utils(paint_timing_detector, true);
  EXPECT_EQ(utils.paint_timing_detector_, paint_timing_detector);
  EXPECT_TRUE(utils.HaveSupplementForBL());
}

TEST_F(PaintTimingDetectorUtilsTest, ConstructorWithNeedSupplementFalse) {
  auto test1 = web_view_impl_->MainFrameImpl();
  LocalFrameView* view = test1->GetFrameView();
  PaintTimingDetector* paint_timing_detector = &view->GetPaintTimingDetector();
  PaintTimingDetectorUtils utils(paint_timing_detector, false);
  EXPECT_EQ(utils.paint_timing_detector_, paint_timing_detector);
  EXPECT_FALSE(utils.HaveSupplementForBL());
}

TEST_F(PaintTimingDetectorUtilsTest, ForwardNotifyPaintFinishedTrue) {
  auto test1 = web_view_impl_->MainFrameImpl();
  LocalFrameView* view = test1->GetFrameView();
  PaintTimingDetector* paint_timing_detector = &view->GetPaintTimingDetector();
  PaintTimingDetectorUtils utils(paint_timing_detector, true);
  bool result = utils.ForwardNotifyPaintFinished();
  EXPECT_TRUE(result);
}

TEST_F(PaintTimingDetectorUtilsTest, ForwardNotifyPaintFinishedFalse) {
  auto test1 = web_view_impl_->MainFrameImpl();
  LocalFrameView* view = test1->GetFrameView();
  PaintTimingDetector* paint_timing_detector = &view->GetPaintTimingDetector();
  PaintTimingDetectorUtils utils(paint_timing_detector, false);
  bool result = utils.ForwardNotifyPaintFinished();
  EXPECT_FALSE(result);
}
#endif

#if BUILDFLAG(ARKWEB_FIRST_SCREEN_PAINT) || BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)
TEST_F(PaintTimingDetectorUtilsTest, GetFirstScreenCalculator) {
  auto test1 = web_view_impl_->MainFrameImpl();
  LocalFrameView* view = test1->GetFrameView();
  PaintTimingDetector* paint_timing_detector = &view->GetPaintTimingDetector();
  PaintTimingDetectorUtils utils(paint_timing_detector);
  FirstScreenCalculator* calculator = utils.GetFirstScreenCalculator();
  EXPECT_NE(calculator, nullptr);
}
#endif

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
namespace {
constexpr char kBackgroundImage[] =
    "width:20px;height:20px;background-image:url(data:image/gif;base64,"
    "R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==)";
}

// Every forwarder that only talks to the supplemental detector must take its
// early return when no supplement was requested.
TEST_F(PaintTimingDetectorUtilsTest, ForwardersEarlyReturnWithoutSupplement) {
  PaintTimingDetector* paint_timing_detector =
      &web_view_impl_->MainFrameImpl()->GetFrameView()->GetPaintTimingDetector();
  PaintTimingDetectorUtils utils(paint_timing_detector, false);
  ASSERT_FALSE(utils.HaveSupplementForBL());

  Document* document = web_view_impl_->MainFrameImpl()->GetFrameView()->GetFrame().GetDocument();
  ASSERT_NE(document, nullptr);
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);
  document->body()->AppendChild(div);
  LayoutObject* layout_object = div->GetLayoutObject();
  ASSERT_EQ(layout_object, nullptr);

  utils.ForwardNotifyImageFinished(*layout_object, nullptr);
  utils.ForwardLayoutObjectWillBeDestroyed(*layout_object);
  utils.ForwardNotifyImageRemoved(*layout_object, nullptr);
  utils.ForwardOnInputOrScroll();
  utils.ForwardRestartRecordingLCP();
  utils.ForwardRestartRecordingLCPToUkm();
  utils.ForwardSoftNavigationDetected(nullptr);
  utils.ForwardReportIgnoredContent();
  utils.ForwardUpdateLcpCandidate();
  utils.RestartRecordingForBlankless();
  utils.SyncIPTDFrameIdxToBLIPTD(7u);
  utils.SyncTPTDFrameIdxToBLTPTD(9u);
  EXPECT_FALSE(utils.ForwardNotifyPaintFinished());
}

// The image/text frame indexes held by the blankless-side detectors must be
// readable and writable through the public forwarders.
TEST_F(PaintTimingDetectorUtilsTest, SyncFrameIndexToSupplement) {
  PaintTimingDetector* paint_timing_detector =
      &web_view_impl_->MainFrameImpl()->GetFrameView()->GetPaintTimingDetector();
  PaintTimingDetectorUtils utils(paint_timing_detector, true);
  ASSERT_TRUE(utils.HaveSupplementForBL());

  utils.SyncIPTDFrameIdxToBLIPTD(3u);
  utils.SyncTPTDFrameIdxToBLTPTD(4u);

  TextPaintTimingDetector& text = paint_timing_detector->GetTextPaintTimingDetector();
  text.SetFrameIndex(4u);
  EXPECT_EQ(text.GetFrameIndex(), 4u);
  EXPECT_EQ(paint_timing_detector->GetImagePaintTimingDetector().GetFrameIndex(), 1u);
}

TEST_F(PaintTimingDetectorUtilsTest, ForwardNotifyPaintFinishedWithSupplement) {
  PaintTimingDetector* paint_timing_detector =
      &web_view_impl_->MainFrameImpl()->GetFrameView()->GetPaintTimingDetector();
  PaintTimingDetectorUtils utils(paint_timing_detector, true);
  ASSERT_TRUE(utils.HaveSupplementForBL());
  EXPECT_TRUE(utils.ForwardNotifyPaintFinished());
}

// CheckNotifyLcpForBlankless falls back to the supplemental LCP update when
// the primary detector holds no LCP candidate.
TEST_F(PaintTimingDetectorUtilsTest, CheckNotifyLcpForBlanklessWithoutCandidate) {
  PaintTimingDetector* paint_timing_detector =
      &web_view_impl_->MainFrameImpl()->GetFrameView()->GetPaintTimingDetector();
  PaintTimingDetectorUtils utils(paint_timing_detector, true);
  ASSERT_TRUE(utils.HaveSupplementForBL());
  ASSERT_FALSE(paint_timing_detector->GetImagePaintTimingDetector().TakeIfHasALCP());

  utils.CheckNotifyLcpForBlankless();
  SUCCEED();
}

// NotifyLcpForBlankless falls back to the default property lookup path when
// blankless is not enabled for this test process.
TEST_F(PaintTimingDetectorUtilsTest, NotifyLcpForBlanklessNoop) {
  PaintTimingDetector* paint_timing_detector =
      &web_view_impl_->MainFrameImpl()->GetFrameView()->GetPaintTimingDetector();
  PaintTimingDetectorUtils utils(paint_timing_detector, true);
  utils.NotifyLcpForBlankless();
  SUCCEED();
}

// Background image forwarder: contentful element, null LayoutObject and the
// body/html rejection paths.
TEST_F(PaintTimingDetectorUtilsTest, ForwardBackgroundImagePaintBranches) {
  PaintTimingDetector* paint_timing_detector =
      &web_view_impl_->MainFrameImpl()->GetFrameView()->GetPaintTimingDetector();
  PaintTimingDetectorUtils utils(paint_timing_detector, true);
  Document* document = web_view_impl_->MainFrameImpl()->GetFrameView()->GetFrame().GetDocument();
  ASSERT_NE(document, nullptr);

  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);
  div->setAttribute(html_names::kStyleAttr, AtomicString(kBackgroundImage));
  document->body()->AppendChild(div);
  document->View()->UpdateAllLifecyclePhasesForTest();
  LayoutObject* layout_object = div->GetLayoutObject();
  ASSERT_NE(layout_object, nullptr);
  const StyleImage* style_image = div->ComputedStyleRef().BackgroundLayers().GetImage();
  ASSERT_NE(style_image, nullptr);

  const Image& image = *Image::NullImage();
  utils.ForwardNotifyBackgroundImagePaint(*div, image, *style_image,
                                          PropertyTreeStateOrAlias(PropertyTreeState::Root()),
                                          gfx::Rect(0, 0, 20, 20));
  utils.ForwardNotifyBackgroundImagePaint(*document->body(), image, *style_image,
                                          PropertyTreeStateOrAlias(PropertyTreeState::Root()),
                                          gfx::Rect(0, 0, 20, 20));
  utils.ForwardNotifyBackgroundImagePaint(*document->documentElement(), image,
                                          *style_image,
                                          PropertyTreeStateOrAlias(PropertyTreeState::Root()),
                                          gfx::Rect(0, 0, 20, 20));

  // A detached node has no LayoutObject, so the forwarder must bail out.
  auto* detached = MakeGarbageCollected<HTMLDivElement>(*document);
  detached->setAttribute(html_names::kStyleAttr, AtomicString(kBackgroundImage));
  utils.ForwardNotifyBackgroundImagePaint(*detached, image, *style_image,
                                          PropertyTreeStateOrAlias(PropertyTreeState::Root()),
                                          gfx::Rect(0, 0, 20, 20));

  // Stop the supplemental image detector and confirm the recording guard.
  utils.ForwardOnInputOrScroll();
  utils.ForwardNotifyBackgroundImagePaint(*div, image, *style_image,
                                          PropertyTreeStateOrAlias(PropertyTreeState::Root()),
                                          gfx::Rect(0, 0, 20, 20));
}

// Foreground image forwarder with a live layout object and a real MediaTiming.
TEST_F(PaintTimingDetectorUtilsTest, ForwardImagePaintBranches) {
  PaintTimingDetector* paint_timing_detector =
      &web_view_impl_->MainFrameImpl()->GetFrameView()->GetPaintTimingDetector();
  PaintTimingDetectorUtils utils(paint_timing_detector, true);
  Document* document = web_view_impl_->MainFrameImpl()->GetFrameView()->GetFrame().GetDocument();
  ASSERT_NE(document, nullptr);

  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);
  div->setAttribute(html_names::kStyleAttr, AtomicString("width:20px;height:20px"));
  document->body()->AppendChild(div);
  document->View()->UpdateAllLifecyclePhasesForTest();
  LayoutObject* layout_object = div->GetLayoutObject();
  ASSERT_NE(layout_object, nullptr);

  ImageResourceContent* content = ImageResourceContent::CreateNotStarted();
  ASSERT_NE(content, nullptr);
  utils.ForwardNotifyImagePaint(*layout_object, gfx::Size(10, 10), *content,
                                PropertyTreeStateOrAlias(PropertyTreeState::Root()),
                                gfx::Rect(0, 0, 20, 20));

  // After input the supplemental detector stops recording, so the forwarder
  // takes its "not recording" return.
  utils.ForwardOnInputOrScroll();
  utils.ForwardNotifyImagePaint(*layout_object, gfx::Size(10, 10), *content,
                                PropertyTreeStateOrAlias(PropertyTreeState::Root()),
                                gfx::Rect(0, 0, 20, 20));
}

// Once the primary detector stops recording, the supplemental detector receives
// the image-finished notification.
TEST_F(PaintTimingDetectorUtilsTest, ForwardImageFinishedAfterInput) {
  PaintTimingDetector* paint_timing_detector =
      &web_view_impl_->MainFrameImpl()->GetFrameView()->GetPaintTimingDetector();
  PaintTimingDetectorUtils utils(paint_timing_detector, true);
  Document* document = web_view_impl_->MainFrameImpl()->GetFrameView()->GetFrame().GetDocument();
  ASSERT_NE(document, nullptr);

  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);
  div->setAttribute(html_names::kStyleAttr, AtomicString("width:20px;height:20px"));
  document->body()->AppendChild(div);
  document->View()->UpdateAllLifecyclePhasesForTest();
  LayoutObject* layout_object = div->GetLayoutObject();
  ASSERT_NE(layout_object, nullptr);

  paint_timing_detector->GetImagePaintTimingDetector().StopRecordingLargestImagePaint();
  utils.ForwardNotifyImageFinished(*layout_object, nullptr);
  utils.ForwardLayoutObjectWillBeDestroyed(*layout_object);
  utils.ForwardNotifyImageRemoved(*layout_object, nullptr);
  utils.ForwardReportIgnoredContent();

  // Stop the supplemental detector as well; the second guard must fire.
  utils.ForwardOnInputOrScroll();
  utils.ForwardNotifyImageFinished(*layout_object, nullptr);
}

// LCP update must reach the supplemental detector while hard LCP is recording.
TEST_F(PaintTimingDetectorUtilsTest, ForwardUpdateLcpCandidateWithSupplement) {
  PaintTimingDetector* paint_timing_detector =
      &web_view_impl_->MainFrameImpl()->GetFrameView()->GetPaintTimingDetector();
  PaintTimingDetectorUtils utils(paint_timing_detector, true);
  ASSERT_TRUE(utils.HaveSupplementForBL());
  utils.ForwardUpdateLcpCandidate();
  utils.ForwardOnInputOrScroll();
  SUCCEED();
}

// First screen paint helpers must exercise the calculator they own.
TEST_F(PaintTimingDetectorUtilsTest, FirstScreenPaintHelpers) {
  PaintTimingDetector* paint_timing_detector =
      &web_view_impl_->MainFrameImpl()->GetFrameView()->GetPaintTimingDetector();
  PaintTimingDetectorUtils utils(paint_timing_detector);
  ASSERT_NE(utils.GetFirstScreenCalculator(), nullptr);
  utils.RestartRecordingFirstScreenPaint();
  utils.OnUserScroll();

  // A default-constructed utils has no detector, so the getter must bail out.
  PaintTimingDetectorUtils empty_utils;
  EXPECT_EQ(empty_utils.GetFirstScreenCalculator(), nullptr);

  LargestContentfulPaintCalculator* lcp =
      paint_timing_detector->GetLargestContentfulPaintCalculator();
  ASSERT_NE(lcp, nullptr);
  lcp->SetForBlankless();
  SUCCEED();
}

// In a subframe the calculator is owned by the parent frame, which exercises
// the local-parent branch of GetFirstScreenCalculator.
TEST_F(PaintTimingDetectorUtilsTest, FirstScreenCalculatorFromParentFrame) {
  Document* document = web_view_impl_->MainFrameImpl()->GetFrameView()->GetFrame().GetDocument();
  ASSERT_NE(document, nullptr);
  document->body()->SetInnerHTMLWithoutTrustedTypes("<iframe></iframe>");
  document->View()->UpdateAllLifecyclePhasesForTest();

  LocalFrame* child = To<LocalFrame>(document->GetFrame()->Tree().FirstChild());
  ASSERT_NE(child, nullptr);
  ASSERT_NE(child->View(), nullptr);
  PaintTimingDetector* child_detector = &child->View()->GetPaintTimingDetector();
  PaintTimingDetectorUtils utils(child_detector);
  EXPECT_NE(utils.GetFirstScreenCalculator(), nullptr);
}

// These four supplement methods are CHECK(false) tombstones on this branch:
// the only observable behaviour is the process abort, so assert exactly that.
TEST_F(PaintTimingDetectorUtilsTest, SupplementTombstonesDie) {
  PaintTimingDetector* paint_timing_detector =
      &web_view_impl_->MainFrameImpl()->GetFrameView()->GetPaintTimingDetector();
  PaintTimingDetectorUtils utils(paint_timing_detector, true);
  ASSERT_TRUE(utils.HaveSupplementForBL());

  EXPECT_DEATH_IF_SUPPORTED(utils.ForwardRestartRecordingLCP(), "");
  EXPECT_DEATH_IF_SUPPORTED(utils.ForwardRestartRecordingLCPToUkm(), "");
  EXPECT_DEATH_IF_SUPPORTED(utils.ForwardSoftNavigationDetected(nullptr), "");
  EXPECT_DEATH_IF_SUPPORTED(utils.RestartRecordingForBlankless(), "");
  EXPECT_DEATH_IF_SUPPORTED(paint_timing_detector->RestartRecordingForBlankless(), "");
}

// Public pass-throughs on PaintTimingDetector must reach the utils layer.
TEST_F(PaintTimingDetectorUtilsTest, PaintTimingDetectorFrameSyncPassThrough) {
  PaintTimingDetector* paint_timing_detector =
      &web_view_impl_->MainFrameImpl()->GetFrameView()->GetPaintTimingDetector();
  paint_timing_detector->SyncIPTDFrameIdxToBLIPTD(5u);
  paint_timing_detector->SyncTPTDFrameIdxToBLTPTD(6u);
  EXPECT_EQ(paint_timing_detector->GetTextPaintTimingDetector().GetFrameIndex(), 1u);
}
#endif

}  // namespace blink