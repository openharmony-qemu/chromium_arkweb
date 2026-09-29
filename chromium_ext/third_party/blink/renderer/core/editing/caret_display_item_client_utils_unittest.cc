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

#include "arkweb/build/features/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/caret_display_item_client.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

namespace blink {

#if BUILDFLAG(ARKWEB_MENU)

class CaretDisplayItemClientUtilsTest : public RenderingTest {};

TEST_F(CaretDisplayItemClientUtilsTest,
       GetBlinkCaretRectRejectsNullLayoutBlock) {
  PaintController paint_controller;
  GraphicsContext context(paint_controller);
  context.BeginRecording();
  auto* utils = MakeGarbageCollected<CaretDisplayItemClientUtils>(nullptr);

  EXPECT_FALSE(utils->GetBlinkCaretRect(context, gfx::Rect(0, 0, 1, 10),
                                        nullptr, Color::kBlack));
  context.EndRecording();
}

TEST_F(CaretDisplayItemClientUtilsTest, RejectsLayoutBlockWithoutFrameView) {
  Document* detached_document =
      Document::CreateForTest(*GetDocument().GetExecutionContext());
  // Constructing a LayoutObject with a non-null Node requires its document to
  // have a FrameView. Use the anonymous factory so this detached-document case
  // reaches the utility's null-FrameView guard instead of crashing here.
  auto* layout_block =
      LayoutBlockFlow::CreateAnonymous(detached_document, nullptr);
  // A frame-less test document remains kInactive, but destroying its layout
  // objects requires the document to have entered teardown.
  detached_document->Lifecycle().AdvanceTo(DocumentLifecycle::kStopping);
  ASSERT_EQ(layout_block->GetFrameView(), nullptr);
  auto* utils = MakeGarbageCollected<CaretDisplayItemClientUtils>(nullptr);

  PaintController paint_controller;
  GraphicsContext context(paint_controller);
  context.BeginRecording();
  EXPECT_FALSE(utils->GetBlinkCaretRect(context, gfx::Rect(0, 0, 1, 10),
                                        layout_block, Color::kBlack));
  context.EndRecording();
  EXPECT_FALSE(utils->IsViewportScale(layout_block));

  layout_block->Destroy();
}

TEST_F(CaretDisplayItemClientUtilsTest, GetBlinkCaretRectPaintsActiveViewport) {
  SetBodyContent("<div id=editable contenteditable>text</div>");
  UpdateAllLifecyclePhasesForTest();
  auto* element = GetDocument().getElementById(AtomicString("editable"));
  ASSERT_NE(element, nullptr);
  auto* layout_block = DynamicTo<LayoutBlock>(element->GetLayoutObject());
  ASSERT_NE(layout_block, nullptr);
  PaintController paint_controller;
  GraphicsContext context(paint_controller);
  context.BeginRecording();
  auto* utils = MakeGarbageCollected<CaretDisplayItemClientUtils>(nullptr);

  EXPECT_TRUE(utils->GetBlinkCaretRect(context, gfx::Rect(0, 0, 1, 10),
                                       layout_block, Color::kBlack));
  context.EndRecording();
}

TEST_F(CaretDisplayItemClientUtilsTest, IsViewportScaleRejectsNullLayoutBlock) {
  auto* utils = MakeGarbageCollected<CaretDisplayItemClientUtils>(nullptr);

  EXPECT_FALSE(utils->IsViewportScale(nullptr));
}

TEST_F(CaretDisplayItemClientUtilsTest, IsViewportScaleReportsScaleEnd) {
  SetBodyContent("<div id=editable contenteditable>text</div>");
  UpdateAllLifecyclePhasesForTest();
  auto* element = GetDocument().getElementById(AtomicString("editable"));
  ASSERT_NE(element, nullptr);
  auto* layout_block = DynamicTo<LayoutBlock>(element->GetLayoutObject());
  ASSERT_NE(layout_block, nullptr);
  LocalFrameView* frame_view = layout_block->GetFrameView();
  ASSERT_NE(frame_view, nullptr);
  ASSERT_NE(frame_view->GetChromeClient(), nullptr);
  WebView* web_view = frame_view->GetChromeClient()->GetWebView();
  ASSERT_NE(web_view, nullptr);
  WebSettings* settings = web_view->GetSettings();
  ASSERT_NE(settings, nullptr);
  auto* utils = MakeGarbageCollected<CaretDisplayItemClientUtils>(nullptr);

  settings->SetViewportScaleState(true);
  EXPECT_FALSE(utils->IsViewportScale(layout_block));
  EXPECT_FALSE(settings->IsViewportScale());
  EXPECT_TRUE(utils->IsViewportScale(layout_block));
  EXPECT_FALSE(utils->IsViewportScale(layout_block));
}

#endif  // BUILDFLAG(ARKWEB_MENU)

}  // namespace blink
