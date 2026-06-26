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
namespace blink {
TEST_F(FrameSelectionTest, MoveRangeSelection) {
  SetBodyContent("<div id=sample>0123456789</div>abc");
  Element* anchor = GetDocument().getElementById(AtomicString("anchor"));
  gfx::Point point;
  Selection().AsFrameSelectionExt()->MoveRangeSelection(point, true, TextGranularity::kCharacter);
  Selection().AsFrameSelectionExt()->MoveRangeSelection(gfx::Point(-5, -5), true, TextGranularity::kCharacter);
  Selection().AsFrameSelectionExt()->MoveRangeSelection(gfx::Point(5, 5), true, TextGranularity::kCharacter);
  Selection().AsFrameSelectionExt()->MoveRangeSelection(gfx::Point(1000, 1000), true, TextGranularity::kCharacter);

  Selection().AsFrameSelectionExt()->MoveRangeSelection(point, false, TextGranularity::kCharacter);  
  Selection().AsFrameSelectionExt()->MoveRangeSelection(gfx::Point(-5, -5), false, TextGranularity::kCharacter);
  Selection().AsFrameSelectionExt()->MoveRangeSelection(gfx::Point(5, 5), false, TextGranularity::kCharacter);
  Selection().AsFrameSelectionExt()->MoveRangeSelection(gfx::Point(1000, 1000), false, TextGranularity::kCharacter);
}

TEST_F(FrameSelectionTest, MoveRangeSelectionWithDifferentGranularity) {
  SetBodyContent("<div id=sample>0123456789</div>abc");
  Element* anchor = GetDocument().getElementById(AtomicString("anchor"));
  gfx::Point point(5, 5);
  Selection().AsFrameSelectionExt()->MoveRangeSelection(point, true, TextGranularity::kWord);
  Selection().AsFrameSelectionExt()->MoveRangeSelection(point, true, TextGranularity::kLine);
  Selection().AsFrameSelectionExt()->MoveRangeSelection(point, true, TextGranularity::kParagraph);
  Selection().AsFrameSelectionExt()->MoveRangeSelection(point, false, TextGranularity::kWord);
  Selection().AsFrameSelectionExt()->MoveRangeSelection(point, false, TextGranularity::kLine);
  Selection().AsFrameSelectionExt()->MoveRangeSelection(point, false, TextGranularity::kParagraph);
}

TEST_F(FrameSelectionTest, MoveRangeSelectionNullPosition) {
  SetBodyContent("<div id=sample>0123456789</div>abc");
  Selection().AsFrameSelectionExt()->MoveRangeSelection(gfx::Point(1000, 1000), true, TextGranularity::kCharacter);
  Selection().AsFrameSelectionExt()->MoveRangeSelection(gfx::Point(1000, 1000), false, TextGranularity::kCharacter);
}

TEST_F(FrameSelectionTest, GetSelectionMarkMaxLengthOverflow) {
  SetBodyContent("<div id=sample>0123456789</div>abc");
  EXPECT_TRUE(Selection().AsFrameSelectionExt()->GetSelectionMarkMaxLengthOverflow());
  EXPECT_FALSE(Selection().AsFrameSelectionExt()->GetSelectionMarkMaxLengthOverflow());
}

TEST_F(FrameSelectionTest, ClippedSelectionBoundsInRootFrame) {
  SetBodyContent("<div id=sample>0123456789</div>abc");
  auto bounds = Selection().AsFrameSelectionExt()->ClippedSelectionBoundsInRootFrame();
  EXPECT_TRUE(bounds.IsEmpty());
  
  Selection().SelectAll();
  bounds = Selection().AsFrameSelectionExt()->ClippedSelectionBoundsInRootFrame();
}

TEST_F(FrameSelectionTest, ClippedSelectionBoundsInRootFrameWithSelection) {
  SetBodyContent("<div id=sample>0123456789</div>abc");
  Selection().SelectAll();
  auto bounds = Selection().AsFrameSelectionExt()->ClippedSelectionBoundsInRootFrame();
}

TEST_F(FrameSelectionTest, ScrollRectToVisualIfClosestEdgeTest) {
  SetBodyContent("<div id=sample>0123456789</div>abc");
  Selection().SelectAll();
  Selection().AsFrameSelectionExt()->ScrollRectToVisualIfClosestEdge(gfx::Point(5, 5));
  Selection().AsFrameSelectionExt()->ScrollRectToVisualIfClosestEdge(gfx::Point(1000, 1000));
}

TEST_F(FrameSelectionTest, InvalidateSelectionForDragTest) {
  SetBodyContent("<div id=sample>0123456789</div>abc");
  Selection().SelectAll();
  Selection().AsFrameSelectionExt()->InvalidateSelectionForDrag();
}

}  // namespace blink
