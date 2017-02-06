// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintArtifactToSkCanvas.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/testing/TestPaintArtifact.h"
#include "platform/transforms/TransformationMatrix.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRect.h"

using testing::_;
using testing::Eq;
using testing::Pointee;
using testing::Property;
using testing::ResultOf;

namespace blink {
namespace {

static const int kCanvasWidth = 800;
static const int kCanvasHeight = 600;

class MockCanvas : public SkCanvas {
public:
    MockCanvas(int width, int height) : SkCanvas(width, height) {}

    MOCK_METHOD3(onDrawRect, void(const SkRect&, const SkPaint&, MockCanvas*));
    MOCK_METHOD2(willSaveLayer, void(unsigned alpha, MockCanvas*));

private:
    void onDrawRect(const SkRect& rect, const SkPaint& paint) override
    {
        onDrawRect(rect, paint, this);
    }

    SaveLayerStrategy getSaveLayerStrategy(const SaveLayerRec& rec) override
    {
        willSaveLayer(rec.fPaint->getAlpha(), this);
        return SaveLayerStrategy::kFullLayer_SaveLayerStrategy;
    }
};

class PaintArtifactToSkCanvasTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);
    }

    void TearDown() override
    {
        m_featuresBackup.restore();
    }

private:
    RuntimeEnabledFeatures::Backup m_featuresBackup;
};

TEST_F(PaintArtifactToSkCanvasTest, Empty)
{
    MockCanvas canvas(kCanvasWidth, kCanvasHeight);
    EXPECT_CALL(canvas, onDrawRect(_, _, _)).Times(0);

    PaintArtifact artifact;
    paintArtifactToSkCanvas(artifact, &canvas);
}

TEST_F(PaintArtifactToSkCanvasTest, OneChunkWithDrawingsInOrder)
{
    MockCanvas canvas(kCanvasWidth, kCanvasHeight);
    {
        testing::InSequence sequence;
        EXPECT_CALL(canvas, onDrawRect(
            SkRect::MakeXYWH(100, 100, 100, 100),
            Property(&SkPaint::getColor, SK_ColorRED), _));
        EXPECT_CALL(canvas, onDrawRect(
            SkRect::MakeXYWH(100, 150, 300, 200),
            Property(&SkPaint::getColor, SK_ColorBLUE), _));
    }

    TestPaintArtifact artifact;
    artifact.chunk(PaintChunkProperties())
        .rectDrawing(FloatRect(100, 100, 100, 100), SK_ColorRED)
        .rectDrawing(FloatRect(100, 150, 300, 200), SK_ColorBLUE);
    paintArtifactToSkCanvas(artifact.build(), &canvas);
}

TEST_F(PaintArtifactToSkCanvasTest, TransformCombining)
{
    // We expect a matrix which applies the inner translation to the points
    // first, followed by the origin-adjusted scale.
    SkMatrix adjustedScale;
    adjustedScale.setTranslate(-10, -10);
    adjustedScale.postScale(2, 2);
    adjustedScale.postTranslate(10, 10);
    SkMatrix combinedMatrix(adjustedScale);
    combinedMatrix.preTranslate(5, 5);
    MockCanvas canvas(kCanvasWidth, kCanvasHeight);
    {
        testing::InSequence sequence;
        EXPECT_CALL(canvas, onDrawRect(SkRect::MakeWH(300, 200), Property(&SkPaint::getColor, SK_ColorRED),
            Pointee(Property(&SkCanvas::getTotalMatrix, adjustedScale))));
        EXPECT_CALL(canvas, onDrawRect(SkRect::MakeWH(300, 200), Property(&SkPaint::getColor, SK_ColorBLUE),
            Pointee(Property(&SkCanvas::getTotalMatrix, combinedMatrix))));
    }

    // Build the transform tree.
    TransformationMatrix matrix1;
    matrix1.scale(2);
    FloatPoint3D origin1(10, 10, 0);
    RefPtr<TransformPaintPropertyNode> transform1 = TransformPaintPropertyNode::create(matrix1, origin1);
    TransformationMatrix matrix2;
    matrix2.translate(5, 5);
    RefPtr<TransformPaintPropertyNode> transform2 = TransformPaintPropertyNode::create(matrix2, FloatPoint3D(), transform1.get());

    TestPaintArtifact artifact;
    artifact.chunk(transform1.get(), nullptr, nullptr)
        .rectDrawing(FloatRect(0, 0, 300, 200), SK_ColorRED);
    artifact.chunk(transform2.get(), nullptr, nullptr)
        .rectDrawing(FloatRect(0, 0, 300, 200), SK_ColorBLUE);
    paintArtifactToSkCanvas(artifact.build(), &canvas);
}

TEST_F(PaintArtifactToSkCanvasTest, OpacityEffectsCombining)
{
    unsigned expectedFirstOpacity = 127; // floor(0.5 * 255)
    unsigned expectedSecondOpacity = 63; // floor(0.25 * 255)
    MockCanvas canvas(kCanvasWidth, kCanvasHeight);
    {
        testing::InSequence sequence;
        EXPECT_CALL(canvas, willSaveLayer(expectedFirstOpacity, _));
        EXPECT_CALL(canvas, onDrawRect(SkRect::MakeWH(300, 200), Property(&SkPaint::getColor, SK_ColorRED), _));
        EXPECT_CALL(canvas, willSaveLayer(expectedSecondOpacity, _));
        EXPECT_CALL(canvas, onDrawRect(SkRect::MakeWH(300, 200), Property(&SkPaint::getColor, SK_ColorBLUE), _));
    }

    // Build an opacity effect tree.
    RefPtr<EffectPaintPropertyNode> opacityEffect1 = EffectPaintPropertyNode::create(0.5);
    RefPtr<EffectPaintPropertyNode> opacityEffect2 = EffectPaintPropertyNode::create(0.25, opacityEffect1);

    TestPaintArtifact artifact;
    artifact.chunk(nullptr, nullptr, opacityEffect1.get())
        .rectDrawing(FloatRect(0, 0, 300, 200), SK_ColorRED);
    artifact.chunk(nullptr, nullptr, opacityEffect2.get())
        .rectDrawing(FloatRect(0, 0, 300, 200), SK_ColorBLUE);
    paintArtifactToSkCanvas(artifact.build(), &canvas);
}

TEST_F(PaintArtifactToSkCanvasTest, ChangingOpacityEffects)
{
    unsigned expectedAOpacity = 25; // floor(0.1 * 255)
    unsigned expectedBOpacity = 51; // floor(0.2 * 255)
    unsigned expectedCOpacity = 76; // floor(0.3 * 255)
    unsigned expectedDOpacity = 102; // floor(0.4 * 255)
    MockCanvas canvas(kCanvasWidth, kCanvasHeight);
    {
        testing::InSequence sequence;
        EXPECT_CALL(canvas, willSaveLayer(expectedAOpacity, _));
        EXPECT_CALL(canvas, willSaveLayer(expectedBOpacity, _));
        EXPECT_CALL(canvas, onDrawRect(SkRect::MakeWH(300, 200), Property(&SkPaint::getColor, SK_ColorRED), _));
        EXPECT_CALL(canvas, willSaveLayer(expectedCOpacity, _));
        EXPECT_CALL(canvas, willSaveLayer(expectedDOpacity, _));
        EXPECT_CALL(canvas, onDrawRect(SkRect::MakeWH(300, 200), Property(&SkPaint::getColor, SK_ColorBLUE), _));
    }

    // Build an opacity effect tree with the following structure:
    //       _root_
    //      |      |
    // 0.1  a      c  0.3
    //      |      |
    // 0.2  b      d  0.4
    RefPtr<EffectPaintPropertyNode> opacityEffectA = EffectPaintPropertyNode::create(0.1);
    RefPtr<EffectPaintPropertyNode> opacityEffectB = EffectPaintPropertyNode::create(0.2, opacityEffectA);
    RefPtr<EffectPaintPropertyNode> opacityEffectC = EffectPaintPropertyNode::create(0.3);
    RefPtr<EffectPaintPropertyNode> opacityEffectD = EffectPaintPropertyNode::create(0.4, opacityEffectC);

    // Build a two-chunk artifact directly.
    // chunk1 references opacity node b, chunk2 references opacity node d.
    TestPaintArtifact artifact;
    artifact.chunk(nullptr, nullptr, opacityEffectB.get())
        .rectDrawing(FloatRect(0, 0, 300, 200), SK_ColorRED);
    artifact.chunk(nullptr, nullptr, opacityEffectD.get())
        .rectDrawing(FloatRect(0, 0, 300, 200), SK_ColorBLUE);
    paintArtifactToSkCanvas(artifact.build(), &canvas);
}

static SkRegion getCanvasClipAsRegion(SkCanvas* canvas)
{
    SkIRect clipRect;
    canvas->getClipDeviceBounds(&clipRect);

    SkRegion clipRegion;
    clipRegion.setRect(clipRect);
    return clipRegion;
}

TEST_F(PaintArtifactToSkCanvasTest, ClipWithScrollEscaping)
{
    // The setup is to simulate scenario similar to this html:
    // <div style="position:absolute; left:0; top:0; clip:rect(200px,200px,300px,100px);">
    //     <div style="position:fixed; left:150px; top:150px; width:100px; height:100px; overflow:hidden;">
    //         client1
    //     </div>
    // </div>
    // <script>scrollTo(0, 100)</script>
    // The content itself will not be scrolled due to fixed positioning,
    // but will be affected by some scrolled clip.

    // Setup transform tree.
    RefPtr<TransformPaintPropertyNode> transform1 = TransformPaintPropertyNode::create(
        TransformationMatrix().translate(0, -100), FloatPoint3D());

    // Setup clip tree.
    RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::create(
        transform1.get(), FloatRoundedRect(100, 200, 100, 100));
    RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::create(
        nullptr, FloatRoundedRect(150, 150, 100, 100), clip1.get());

    MockCanvas canvas(kCanvasWidth, kCanvasHeight);

    SkRegion totalClip;
    totalClip.setRect(SkIRect::MakeXYWH(100, 100, 100, 100));
    totalClip.op(SkIRect::MakeXYWH(150, 150, 100, 100), SkRegion::kIntersect_Op);
    EXPECT_CALL(canvas, onDrawRect(SkRect::MakeWH(300, 200), Property(&SkPaint::getColor, SK_ColorRED),
        ResultOf(&getCanvasClipAsRegion, Eq(totalClip))));

    TestPaintArtifact artifact;
    artifact.chunk(nullptr, clip2.get(), nullptr)
        .rectDrawing(FloatRect(0, 0, 300, 200), SK_ColorRED);
    paintArtifactToSkCanvas(artifact.build(), &canvas);
}

} // namespace
} // namespace blink
