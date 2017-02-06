// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintInfo.h"

#include "platform/graphics/paint/PaintController.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class PaintInfoTest : public testing::Test {
protected:
    PaintInfoTest()
        : m_paintController(PaintController::create())
        , m_context(*m_paintController)
    { }

    std::unique_ptr<PaintController> m_paintController;
    GraphicsContext m_context;
};

TEST_F(PaintInfoTest, intersectsCullRect)
{
    PaintInfo paintInfo(m_context, IntRect(0, 0, 50, 50), PaintPhaseSelfBlockBackgroundOnly, GlobalPaintNormalPhase, PaintLayerNoFlag);

    EXPECT_TRUE(paintInfo.cullRect().intersectsCullRect(IntRect(0, 0, 1, 1)));
    EXPECT_FALSE(paintInfo.cullRect().intersectsCullRect(IntRect(51, 51, 1, 1)));
}

TEST_F(PaintInfoTest, intersectsCullRectWithLayoutRect)
{
    PaintInfo paintInfo(m_context, IntRect(0, 0, 50, 50), PaintPhaseSelfBlockBackgroundOnly, GlobalPaintNormalPhase, PaintLayerNoFlag);

    EXPECT_TRUE(paintInfo.cullRect().intersectsCullRect(LayoutRect(0, 0, 1, 1)));
    EXPECT_TRUE(paintInfo.cullRect().intersectsCullRect(LayoutRect(LayoutUnit(0.1), LayoutUnit(0.1), LayoutUnit(0.1), LayoutUnit(0.1))));
}

TEST_F(PaintInfoTest, intersectsCullRectWithTransform)
{
    PaintInfo paintInfo(m_context, IntRect(0, 0, 50, 50), PaintPhaseSelfBlockBackgroundOnly, GlobalPaintNormalPhase, PaintLayerNoFlag);
    AffineTransform transform;
    transform.translate(-2, -2);

    EXPECT_TRUE(paintInfo.cullRect().intersectsCullRect(transform, IntRect(51, 51, 1, 1)));
    EXPECT_FALSE(paintInfo.cullRect().intersectsCullRect(IntRect(52, 52, 1, 1)));
}

TEST_F(PaintInfoTest, updateCullRect)
{
    PaintInfo paintInfo(m_context, IntRect(1, 1, 50, 50), PaintPhaseSelfBlockBackgroundOnly, GlobalPaintNormalPhase, PaintLayerNoFlag);
    AffineTransform transform;
    transform.translate(1, 1);
    paintInfo.updateCullRect(transform);

    EXPECT_TRUE(paintInfo.cullRect().intersectsCullRect(IntRect(0, 0, 1, 1)));
    EXPECT_FALSE(paintInfo.cullRect().intersectsCullRect(IntRect(51, 51, 1, 1)));
}

TEST_F(PaintInfoTest, intersectsVerticalRange)
{
    PaintInfo paintInfo(m_context, IntRect(0, 0, 50, 100), PaintPhaseSelfBlockBackgroundOnly, GlobalPaintNormalPhase, PaintLayerNoFlag);

    EXPECT_TRUE(paintInfo.cullRect().intersectsVerticalRange(LayoutUnit(), LayoutUnit(1)));
    EXPECT_FALSE(paintInfo.cullRect().intersectsVerticalRange(LayoutUnit(100), LayoutUnit(101)));
}

TEST_F(PaintInfoTest, intersectsHorizontalRange)
{
    PaintInfo paintInfo(m_context, IntRect(0, 0, 50, 100), PaintPhaseSelfBlockBackgroundOnly, GlobalPaintNormalPhase, PaintLayerNoFlag);

    EXPECT_TRUE(paintInfo.cullRect().intersectsHorizontalRange(LayoutUnit(), LayoutUnit(1)));
    EXPECT_FALSE(paintInfo.cullRect().intersectsHorizontalRange(LayoutUnit(50), LayoutUnit(51)));
}

} // namespace blink
