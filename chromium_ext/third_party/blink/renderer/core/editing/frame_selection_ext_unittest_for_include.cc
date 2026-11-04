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
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"

namespace blink {
namespace {
gfx::Point ContentsPointForPosition(const Position& position) {
  return AbsoluteCaretBoundsOf(PositionWithAffinity(position)).CenterPoint();
}
}  // namespace

TEST_F(FrameSelectionTest, MoveRangeSelectionMovesBaseFromOldFocus) {
  SetBodyContent("<div id=sample>0123456789</div>");
  Node* const text =
      GetDocument().getElementById(AtomicString("sample"))->firstChild();
  UpdateAllLifecyclePhasesForTest();
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 2), Position(text, 7))
          .Build(),
      SetSelectionOptions());

  Selection().AsFrameSelectionExt()->MoveRangeSelection(
      ContentsPointForPosition(Position(text, 5)), true,
      TextGranularity::kCharacter);

  const VisibleSelection& selection =
      Selection().ComputeVisibleSelectionInDOMTree();
  EXPECT_EQ(Position(text, 7), selection.Anchor());
  EXPECT_EQ(Position(text, 5), selection.Focus());
}

TEST_F(FrameSelectionTest, MoveRangeSelectionMovesExtentFromOldAnchor) {
  SetBodyContent("<div id=sample>0123456789</div>");
  Node* const text =
      GetDocument().getElementById(AtomicString("sample"))->firstChild();
  UpdateAllLifecyclePhasesForTest();
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 2), Position(text, 7))
          .Build(),
      SetSelectionOptions());

  Selection().AsFrameSelectionExt()->MoveRangeSelection(
      ContentsPointForPosition(Position(text, 5)), false,
      TextGranularity::kCharacter);

  const VisibleSelection& selection =
      Selection().ComputeVisibleSelectionInDOMTree();
  EXPECT_EQ(Position(text, 2), selection.Anchor());
  EXPECT_EQ(Position(text, 5), selection.Focus());
}

TEST_F(FrameSelectionTest, MoveRangeSelectionAtExistingHandles) {
  SetBodyContent("<div id=sample>0123456789</div>");
  Node* const text =
      GetDocument().getElementById(AtomicString("sample"))->firstChild();
  UpdateAllLifecyclePhasesForTest();
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 2), Position(text, 7))
          .Build(),
      SetSelectionOptions());

  Selection().AsFrameSelectionExt()->MoveRangeSelection(
      ContentsPointForPosition(Position(text, 7)), true,
      TextGranularity::kCharacter);
  EXPECT_EQ(Position(text, 2),
            Selection().ComputeVisibleSelectionInDOMTree().Anchor());
  EXPECT_EQ(Position(text, 7),
            Selection().ComputeVisibleSelectionInDOMTree().Focus());

  Selection().AsFrameSelectionExt()->MoveRangeSelection(
      ContentsPointForPosition(Position(text, 2)), false,
      TextGranularity::kCharacter);
  EXPECT_EQ(Position(text, 7),
            Selection().ComputeVisibleSelectionInDOMTree().Anchor());
  EXPECT_EQ(Position(text, 2),
            Selection().ComputeVisibleSelectionInDOMTree().Focus());
}

TEST_F(FrameSelectionTest, MoveRangeSelectionWithDifferentGranularity) {
  SetBodyContent("<div id=sample>0123456789</div>abc");
  gfx::Point point(5, 5);
  Selection().AsFrameSelectionExt()->MoveRangeSelection(point, true, TextGranularity::kWord);
  Selection().AsFrameSelectionExt()->MoveRangeSelection(point, true, TextGranularity::kLine);
  Selection().AsFrameSelectionExt()->MoveRangeSelection(point, true, TextGranularity::kParagraph);
  Selection().AsFrameSelectionExt()->MoveRangeSelection(point, false, TextGranularity::kWord);
  Selection().AsFrameSelectionExt()->MoveRangeSelection(point, false, TextGranularity::kLine);
  Selection().AsFrameSelectionExt()->MoveRangeSelection(point, false, TextGranularity::kParagraph);
  EXPECT_FALSE(Selection().ComputeVisibleSelectionInDOMTree().IsNone());
}

TEST_F(FrameSelectionTest, MoveRangeSelectionNullPosition) {
  SetBodyContent("<div id=sample>0123456789</div>abc");
  Selection().AsFrameSelectionExt()->MoveRangeSelection(gfx::Point(1000, 1000), true, TextGranularity::kCharacter);
  Selection().AsFrameSelectionExt()->MoveRangeSelection(gfx::Point(1000, 1000), false, TextGranularity::kCharacter);
  EXPECT_TRUE(Selection().ComputeVisibleSelectionInDOMTree().IsNone());
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

  Node* const text =
      GetDocument().getElementById(AtomicString("sample"))->firstChild();
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(Position(text, 3)).Build(),
      SetSelectionOptions());
  UpdateAllLifecyclePhasesForTest();
  bounds = Selection().AsFrameSelectionExt()->ClippedSelectionBoundsInRootFrame();
  EXPECT_TRUE(bounds.IsEmpty());

  Selection().SelectAll();
  UpdateAllLifecyclePhasesForTest();
  bounds = Selection().AsFrameSelectionExt()->ClippedSelectionBoundsInRootFrame();
  EXPECT_FALSE(bounds.IsEmpty());
}

TEST_F(FrameSelectionTest, ClippedSelectionBoundsInRootFrameWithSelection) {
  SetBodyContent("<div id=sample>0123456789</div>abc");
  Selection().SelectAll();
  UpdateAllLifecyclePhasesForTest();
  auto bounds = Selection().AsFrameSelectionExt()->ClippedSelectionBoundsInRootFrame();
  EXPECT_FALSE(bounds.IsEmpty());
}

TEST_F(FrameSelectionTest, ScrollRectToVisualIfClosestEdgeTest) {
  SetBodyContent("<div id=sample>0123456789</div>abc");
  UpdateAllLifecyclePhasesForTest();
  auto* const selection_ext = Selection().AsFrameSelectionExt();
  const base::TimeTicks no_selection_time = base::TimeTicks::Now();
  selection_ext->last_autoscroll_time_ = no_selection_time;
  selection_ext->ScrollRectToVisualIfClosestEdge(gfx::Point(5, 5));
  EXPECT_EQ(no_selection_time, selection_ext->last_autoscroll_time_);

  Selection().SelectAll();
  UpdateAllLifecyclePhasesForTest();

  const base::TimeTicks throttled_time = base::TimeTicks::Now();
  selection_ext->last_autoscroll_time_ = throttled_time;
  selection_ext->ScrollRectToVisualIfClosestEdge(gfx::Point(5, 5));
  EXPECT_EQ(throttled_time, selection_ext->last_autoscroll_time_);

  const base::TimeTicks middle_time =
      base::TimeTicks::Now() - base::Milliseconds(100);
  selection_ext->last_autoscroll_time_ = middle_time;
  const int viewport_middle =
      GetDocument().GetFrame()->GetPage()->GetVisualViewport().Size().height() /
      2;
  selection_ext->ScrollRectToVisualIfClosestEdge(
      gfx::Point(5, viewport_middle));
  EXPECT_EQ(middle_time, selection_ext->last_autoscroll_time_);

  const base::TimeTicks top_edge_time =
      base::TimeTicks::Now() - base::Milliseconds(100);
  selection_ext->last_autoscroll_time_ = top_edge_time;
  selection_ext->ScrollRectToVisualIfClosestEdge(gfx::Point(5, 5));
  EXPECT_GT(selection_ext->last_autoscroll_time_, top_edge_time);

  const base::TimeTicks bottom_edge_time =
      base::TimeTicks::Now() - base::Milliseconds(100);
  selection_ext->last_autoscroll_time_ = bottom_edge_time;
  selection_ext->ScrollRectToVisualIfClosestEdge(gfx::Point(5, 1000));
  EXPECT_GT(selection_ext->last_autoscroll_time_, bottom_edge_time);
}

TEST_F(FrameSelectionTest, InvalidateSelectionForDragTest) {
  SetBodyContent("<div id=sample>0123456789</div>abc");
  Selection().SelectAll();
  UpdateAllLifecyclePhasesForTest();
  Selection().AsFrameSelectionExt()->InvalidateSelectionForDrag();
  EXPECT_TRUE(Selection().ComputeVisibleSelectionInDOMTree().IsRange());
}

TEST_F(FrameSelectionTest, IsPointInSelectionNoSelection) {
  SetBodyContent("<div id=sample>0123456789</div>abc");
  // No selection active — AbsoluteUnclippedBounds() returns empty rect,
  // so Contains() returns false and IsPointInSelection returns false.
  PhysicalOffset point(5, 5);
  EXPECT_FALSE(Selection().AsFrameSelectionExt()->IsPointInSelection(point));
}

TEST_F(FrameSelectionTest, IsPointInSelectionPointInsideBounds) {
  SetBodyContent("<div id=sample>01234567890123456789</div>");
  // Select all to create a visible selection range.
  Selection().SelectAll();
  UpdateAllLifecyclePhasesForTest();
  const PhysicalRect bounds = Selection().AbsoluteUnclippedBounds();
  ASSERT_FALSE(bounds.IsEmpty());
  EXPECT_TRUE(
      Selection().AsFrameSelectionExt()->IsPointInSelection(bounds.Center()));
}

TEST_F(FrameSelectionTest, IsPointInSelectionPointOutsideBounds) {
  SetBodyContent("<div id=sample>01234567890123456789</div>");
  Selection().SelectAll();
  // Use a very large offset that is definitely outside the selection bounds.
  PhysicalOffset point(99999, 99999);
  EXPECT_FALSE(Selection().AsFrameSelectionExt()->IsPointInSelection(point));
}

TEST_F(FrameSelectionTest, IsPointInSelectionPointAboveBounds) {
  SetBodyContent("<div id=sample>01234567890123456789</div>");
  Selection().SelectAll();
  // Use a negative offset that is above/to the left of any selection.
  PhysicalOffset point(-100, -100);
  EXPECT_FALSE(Selection().AsFrameSelectionExt()->IsPointInSelection(point));
}

}  // namespace blink
