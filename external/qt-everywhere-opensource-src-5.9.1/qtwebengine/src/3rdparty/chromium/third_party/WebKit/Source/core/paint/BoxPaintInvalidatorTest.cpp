// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/HTMLNames.h"
#include "core/frame/FrameView.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/RasterInvalidationTracking.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"

namespace blink {

class BoxPaintInvalidatorTest : public RenderingTest {
 protected:
  const RasterInvalidationTracking* getRasterInvalidationTracking() const {
    // TODO(wangxianzhu): Test SPv2.
    return layoutView()
        .layer()
        ->graphicsLayerBacking()
        ->getRasterInvalidationTracking();
  }

 private:
  void SetUp() override {
    RenderingTest::SetUp();
    enableCompositing();
    setBodyInnerHTML(
        "<style>"
        "  body { margin: 0 }"
        "  #target {"
        "    width: 50px;"
        "    height: 100px;"
        "    border-width: 20px 10px;"
        "    border-style: solid;"
        "    border-color: red;"
        "    transform-origin: 0 0"
        "  }"
        "</style>"
        "<div id='target'></div>");
  }
};

TEST_F(BoxPaintInvalidatorTest, IncrementalInvalidationExpand) {
  document().view()->setTracksPaintInvalidations(true);
  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr, "width: 100px; height: 200px");
  document().view()->updateAllLifecyclePhases();
  const auto& rasterInvalidations =
      getRasterInvalidationTracking()->trackedRasterInvalidations;
  EXPECT_EQ(2u, rasterInvalidations.size());
  EXPECT_EQ(IntRect(60, 0, 60, 240), rasterInvalidations[0].rect);
  EXPECT_EQ(PaintInvalidationIncremental, rasterInvalidations[0].reason);
  EXPECT_EQ(IntRect(0, 120, 120, 120), rasterInvalidations[1].rect);
  EXPECT_EQ(PaintInvalidationIncremental, rasterInvalidations[1].reason);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_F(BoxPaintInvalidatorTest, IncrementalInvalidationShrink) {
  document().view()->setTracksPaintInvalidations(true);
  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr, "width: 20px; height: 80px");
  document().view()->updateAllLifecyclePhases();
  const auto& rasterInvalidations =
      getRasterInvalidationTracking()->trackedRasterInvalidations;
  EXPECT_EQ(2u, rasterInvalidations.size());
  EXPECT_EQ(IntRect(30, 0, 40, 140), rasterInvalidations[0].rect);
  EXPECT_EQ(PaintInvalidationIncremental, rasterInvalidations[0].reason);
  EXPECT_EQ(IntRect(0, 100, 70, 40), rasterInvalidations[1].rect);
  EXPECT_EQ(PaintInvalidationIncremental, rasterInvalidations[1].reason);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_F(BoxPaintInvalidatorTest, IncrementalInvalidationMixed) {
  document().view()->setTracksPaintInvalidations(true);
  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr, "width: 100px; height: 80px");
  document().view()->updateAllLifecyclePhases();
  const auto& rasterInvalidations =
      getRasterInvalidationTracking()->trackedRasterInvalidations;
  EXPECT_EQ(2u, rasterInvalidations.size());
  EXPECT_EQ(IntRect(60, 0, 60, 120), rasterInvalidations[0].rect);
  EXPECT_EQ(PaintInvalidationIncremental, rasterInvalidations[0].reason);
  EXPECT_EQ(IntRect(0, 100, 70, 40), rasterInvalidations[1].rect);
  EXPECT_EQ(PaintInvalidationIncremental, rasterInvalidations[1].reason);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_F(BoxPaintInvalidatorTest, SubpixelVisualRectChagne) {
  ScopedSlimmingPaintInvalidationForTest scopedSlimmingPaintInvalidation(true);

  Element* target = document().getElementById("target");

  // Should do full invalidation if new geometry has subpixels.
  document().view()->setTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr, "width: 100.6px; height: 70.3px");
  document().view()->updateAllLifecyclePhases();
  const auto* rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  EXPECT_EQ(2u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(0, 0, 70, 140), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationBorderBoxChange, (*rasterInvalidations)[0].reason);
  EXPECT_EQ(IntRect(0, 0, 121, 111), (*rasterInvalidations)[1].rect);
  EXPECT_EQ(PaintInvalidationBorderBoxChange, (*rasterInvalidations)[1].reason);
  document().view()->setTracksPaintInvalidations(false);

  // Should do full invalidation if old geometry has subpixels.
  document().view()->setTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr, "width: 50px; height: 100px");
  document().view()->updateAllLifecyclePhases();
  rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  EXPECT_EQ(2u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(0, 0, 121, 111), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationBorderBoxChange, (*rasterInvalidations)[0].reason);
  EXPECT_EQ(IntRect(0, 0, 70, 140), (*rasterInvalidations)[1].rect);
  EXPECT_EQ(PaintInvalidationBorderBoxChange, (*rasterInvalidations)[1].reason);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_F(BoxPaintInvalidatorTest, SubpixelChangeWithoutVisualRectChange) {
  ScopedSlimmingPaintInvalidationForTest scopedSlimmingPaintInvalidation(true);

  Element* target = document().getElementById("target");
  LayoutObject* targetObject = target->layoutObject();
  EXPECT_EQ(LayoutRect(0, 0, 70, 140), targetObject->previousVisualRect());

  // Should do full invalidation if new geometry has subpixels even if the paint
  // invalidation rect doesn't change.
  document().view()->setTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr,
                       "margin-top: 0.6px; width: 50px; height: 99.3px");
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(LayoutRect(0, 0, 70, 140), targetObject->previousVisualRect());
  const auto* rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  EXPECT_EQ(1u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(0, 0, 70, 140), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationLocationChange, (*rasterInvalidations)[0].reason);
  document().view()->setTracksPaintInvalidations(false);

  document().view()->setTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr,
                       "margin-top: 0.6px; width: 49.3px; height: 98.5px");
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(LayoutRect(0, 0, 70, 140), targetObject->previousVisualRect());
  rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  EXPECT_EQ(1u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(0, 0, 70, 140), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationBorderBoxChange, (*rasterInvalidations)[0].reason);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_F(BoxPaintInvalidatorTest, ResizeRotated) {
  ScopedSlimmingPaintInvalidationForTest scopedSlimmingPaintInvalidation(true);

  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr, "transform: rotate(45deg)");
  document().view()->updateAllLifecyclePhases();

  // Should do full invalidation a rotated object is resized.
  document().view()->setTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr,
                       "transform: rotate(45deg); width: 200px");
  document().view()->updateAllLifecyclePhases();
  const auto* rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  EXPECT_EQ(1u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(-99, 0, 255, 255), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationBorderBoxChange, (*rasterInvalidations)[0].reason);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_F(BoxPaintInvalidatorTest, ResizeRotatedChild) {
  ScopedSlimmingPaintInvalidationForTest scopedSlimmingPaintInvalidation(true);

  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr,
                       "transform: rotate(45deg); width: 200px");
  target->setInnerHTML(
      "<div id=child style='width: 50px; height: 50px; background: "
      "red'></div>");
  document().view()->updateAllLifecyclePhases();
  Element* child = document().getElementById("child");

  // Should do full invalidation a rotated object is resized.
  document().view()->setTracksPaintInvalidations(true);
  child->setAttribute(HTMLNames::styleAttr,
                      "width: 100px; height: 50px; background: red");
  document().view()->updateAllLifecyclePhases();
  const auto* rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  EXPECT_EQ(1u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(-43, 21, 107, 107), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationBorderBoxChange, (*rasterInvalidations)[0].reason);
  document().view()->setTracksPaintInvalidations(false);
}

}  // namespace blink
