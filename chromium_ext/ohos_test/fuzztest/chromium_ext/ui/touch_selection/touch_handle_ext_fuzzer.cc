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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <cmath>

#include "fuzzer/FuzzedDataProvider.h"
#include "ui/touch_selection/touch_handle.h"
#include "ui/touch_selection/touch_handle_orientation.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/point_f.h"
#define private public
#include "arkweb/chromium_ext/ui/touch_selection/touch_handle_ext.h"
#undef private

namespace ui {

static const TouchHandleOrientation kOrientations[] = {
    TouchHandleOrientation::LEFT,
    TouchHandleOrientation::CENTER,
    TouchHandleOrientation::RIGHT,
    TouchHandleOrientation::UNDEFINED,
};

float ConsumeFloat(FuzzedDataProvider* fdp, float min, float max) {
    int32_t scaled = fdp->ConsumeIntegralInRange<int32_t>(
        static_cast<int32_t>(min), static_cast<int32_t>(max));
    return static_cast<float>(scaled);
}

class MockTouchHandleDrawable : public TouchHandleDrawable {
public:
    MockTouchHandleDrawable() = default;
    ~MockTouchHandleDrawable() override = default;

    void SetEnabled(bool enabled) override {}
    void SetOrientation(TouchHandleOrientation orientation,
                        bool mirror_vertical,
                        bool mirror_horizontal) override {}
    void SetOrigin(const gfx::PointF& origin) override {}
    void SetAlpha(float alpha) override {}
    gfx::RectF GetVisibleBounds() const override {
        return gfx::RectF(0, 0, 10, 50);
    }
    float GetDrawableHorizontalPaddingRatio() const override {
        return 0.5f;
    }
    void SetEdge(const gfx::PointF& top, const gfx::PointF& bottom) override {}
};

class MockTouchHandleClient : public TouchHandleClient {
public:
    MockTouchHandleClient() = default;
    ~MockTouchHandleClient() override = default;

    void OnDragBegin(const TouchSelectionDraggable& handler,
                     const gfx::PointF& drag_position) override {}

    void OnDragUpdate(const TouchSelectionDraggable& handler,
                      const gfx::PointF& drag_position) override {}

    void OnDragEnd(const TouchSelectionDraggable& handler) override {}

    void UpdateSelectionChanged(
        const TouchSelectionDraggable& draggable) override {}

    bool IsWithinTapSlop(const gfx::Vector2dF& delta) const override {
        return delta.LengthSquared() < 100;
    }

    void OnHandleTapped(const TouchHandle& handle) override {}

    void SetNeedsAnimate() override {}

    std::unique_ptr<TouchHandleDrawable> CreateDrawable() override {
        return std::make_unique<MockTouchHandleDrawable>();
    }

    base::TimeDelta GetMaxTapDuration() const override {
        return base::Milliseconds(200);
    }

    bool IsAdaptiveHandleOrientationEnabled() const override {
        return true;
    }
};

void FuzzBasicOperations(FuzzedDataProvider* fdp) {
    MockTouchHandleClient client;
    
    TouchHandleOrientation orientation = kOrientations[fdp->ConsumeIntegralInRange<int>(0, 3)];
    gfx::RectF viewport_rect(
        ConsumeFloat(fdp, -1000.0f, 1000.0f),
        ConsumeFloat(fdp, -1000.0f, 1000.0f),
        ConsumeFloat(fdp, 100.0f, 2000.0f),
        ConsumeFloat(fdp, 100.0f, 2000.0f)
    );

    TouchHandleExt handle(&client, orientation, viewport_rect);
    
    TouchHandleExt* ext = handle.AsTouchHandleExt();
    if (ext) {
        (void)ext->GetEnabled();
        (void)ext->GetVisible();
    }
}

void FuzzSetEdge(FuzzedDataProvider* fdp) {
    MockTouchHandleClient client;
    
    TouchHandleOrientation orientation = TouchHandleOrientation::LEFT;
    gfx::RectF viewport_rect(0, 0, 560, 1200);

    TouchHandleExt handle(&client, orientation, viewport_rect);
    
    handle.SetVisible(true, TouchHandle::ANIMATION_NONE);
    handle.UpdateHandleLayout();
    
    gfx::PointF top(
        ConsumeFloat(fdp, -100.0f, 100.0f),
        ConsumeFloat(fdp, -100.0f, 100.0f)
    );
    gfx::PointF bottom(
        ConsumeFloat(fdp, -100.0f, 100.0f),
        ConsumeFloat(fdp, -100.0f, 100.0f) + 10.0f
    );

    handle.SetEdge(top, bottom);
}

void FuzzResetPositionAfterDragEnd(FuzzedDataProvider* fdp) {
    MockTouchHandleClient client;
    
    TouchHandleOrientation orientation = TouchHandleOrientation::CENTER;
    gfx::RectF viewport_rect(0, 0, 560, 1200);

    TouchHandleExt handle(&client, orientation, viewport_rect);
    
    handle.ResetPositionAfterDragEnd();
    
    handle.SetVisible(true, TouchHandle::ANIMATION_NONE);
    handle.UpdateHandleLayout();
    handle.ResetPositionAfterDragEnd();
    
    handle.SetVisible(false, TouchHandle::ANIMATION_NONE);
    handle.ResetPositionAfterDragEnd();
    
    (void)fdp;
}

void FuzzSetFocus(FuzzedDataProvider* fdp) {
    MockTouchHandleClient client;
    
    TouchHandleOrientation orientation = TouchHandleOrientation::LEFT;
    gfx::RectF viewport_rect(0, 0, 560, 1200);

    TouchHandleExt handle(&client, orientation, viewport_rect);
    
    gfx::PointF top1(10.0f, 100.0f);
    gfx::PointF bottom1(10.0f, 110.0f);
    
    bool result1 = handle.SetFocus(top1, bottom1);
    (void)result1;
    
    gfx::PointF top2(20.0f, 200.0f);
    gfx::PointF bottom2(20.0f, 190.0f);
    
    bool result2 = handle.SetFocus(top2, bottom2);
    (void)result2;
    
    bool result3 = handle.SetFocus(bottom2, top2);
    (void)result3;
    
    gfx::PointF top3(30.0f, 300.0f);
    gfx::PointF bottom3(30.0f, 300.0f);
    
    bool result4 = handle.SetFocus(top3, bottom3);
    (void)result4;
    
    (void)fdp;
}

void FuzzSetInsertHandleAlpha(FuzzedDataProvider* fdp) {
    MockTouchHandleClient client;
    
    TouchHandleOrientation orientation = TouchHandleOrientation::CENTER;
    gfx::RectF viewport_rect(0, 0, 560, 1200);

    TouchHandleExt handle(&client, orientation, viewport_rect);
    
    handle.SetVisible(true, TouchHandle::ANIMATION_NONE);
    handle.UpdateHandleLayout();
    
    std::array<float, 7> alpha_values = {
        0.0f, 0.5f, 1.0f, -0.5f, 1.5f, 0.3f, 0.7f
    };
    
    for (float alpha : alpha_values) {
        handle.SetInsertHandleAlpha(alpha);
    }
    
    handle.SetInsertHandleAlpha(0.5f);
    handle.SetInsertHandleAlpha(0.5f);
    
    (void)fdp;
}

void FuzzBoundaryCoordinates(FuzzedDataProvider* fdp) {
    MockTouchHandleClient client;
    
    TouchHandleOrientation orientation = TouchHandleOrientation::LEFT;
    gfx::RectF viewport_rect(-1000.0f, -1000.0f, 2000.0f, 3000.0f);

    TouchHandleExt handle(&client, orientation, viewport_rect);
    
    gfx::PointF extreme_top(1e6f, 1e6f);
    gfx::PointF extreme_bottom(1e6f, 1e6f + 10.0f);
    handle.SetFocus(extreme_top, extreme_bottom);
    
    gfx::PointF neg_top(-1e6f, -1e6f);
    gfx::PointF neg_bottom(-1e6f, -1e6f + 10.0f);
    handle.SetFocus(neg_top, neg_bottom);
    
    gfx::PointF nan_top(std::nanf(""), 0.0f);
    gfx::PointF nan_bottom(0.0f, std::nanf(""));
    handle.SetFocus(nan_top, nan_bottom);
    
    handle.SetEdge(extreme_top, extreme_bottom);
    handle.SetEdge(neg_top, neg_bottom);
    
    (void)fdp;
}

}  // namespace ui

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (data == nullptr || size < sizeof(int32_t)) {
        return 0;
    }

    FuzzedDataProvider fdp(data, size);

    int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 5);

    switch (selector) {
        case 0:
            ui::FuzzBasicOperations(&fdp);
            break;
        case 1:
            ui::FuzzSetEdge(&fdp);
            break;
        case 2:
            ui::FuzzResetPositionAfterDragEnd(&fdp);
            break;
        case 3:
            ui::FuzzSetFocus(&fdp);
            break;
        case 4:
            ui::FuzzSetInsertHandleAlpha(&fdp);
            break;
        case 5:
            ui::FuzzBoundaryCoordinates(&fdp);
            break;
        default:
            ui::FuzzBasicOperations(&fdp);
            break;
    }

    return 0;
}

