// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/NinePieceImageGrid.h"

#include "core/css/CSSGradientValue.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/style/ComputedStyle.h"
#include "core/style/NinePieceImage.h"
#include "core/style/StyleGeneratedImage.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class NinePieceImageGridTest : public RenderingTest {
public:
    NinePieceImageGridTest() { }

    StyleImage* generatedImage()
    {
        CSSLinearGradientValue* gradient = CSSLinearGradientValue::create(Repeating);
        return StyleGeneratedImage::create(*gradient);
    }

private:
    void SetUp() override
    {
        RenderingTest::SetUp();
    }
};

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_NoDrawables)
{
    NinePieceImage ninePiece;
    ninePiece.setImage(generatedImage());

    IntSize imageSize(100, 100);
    IntRect borderImageArea(0, 0, 100, 100);
    IntRectOutsets borderWidths(0, 0, 0, 0);

    NinePieceImageGrid grid = NinePieceImageGrid(ninePiece, imageSize, borderImageArea, borderWidths);
    for (NinePiece piece = MinPiece; piece < MaxPiece; ++piece) {
        NinePieceImageGrid::NinePieceDrawInfo drawInfo = grid.getNinePieceDrawInfo(piece, 1);
        EXPECT_FALSE(drawInfo.isDrawable);
    }
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_AllDrawable)
{
    NinePieceImage ninePiece;
    ninePiece.setImage(generatedImage());
    ninePiece.setImageSlices(LengthBox(10, 10, 10, 10));
    ninePiece.setFill(true);

    IntSize imageSize(100, 100);
    IntRect borderImageArea(0, 0, 100, 100);
    IntRectOutsets borderWidths(10, 10, 10, 10);

    NinePieceImageGrid grid = NinePieceImageGrid(ninePiece, imageSize, borderImageArea, borderWidths);
    for (NinePiece piece = MinPiece; piece < MaxPiece; ++piece) {
        NinePieceImageGrid::NinePieceDrawInfo drawInfo = grid.getNinePieceDrawInfo(piece, 1);
        EXPECT_TRUE(drawInfo.isDrawable);
    }
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_NoFillMiddleNotDrawable)
{
    NinePieceImage ninePiece;
    ninePiece.setImage(generatedImage());
    ninePiece.setImageSlices(LengthBox(10, 10, 10, 10));
    ninePiece.setFill(false); // default

    IntSize imageSize(100, 100);
    IntRect borderImageArea(0, 0, 100, 100);
    IntRectOutsets borderWidths(10, 10, 10, 10);

    NinePieceImageGrid grid = NinePieceImageGrid(ninePiece, imageSize, borderImageArea, borderWidths);
    for (NinePiece piece = MinPiece; piece < MaxPiece; ++piece) {
        NinePieceImageGrid::NinePieceDrawInfo drawInfo = grid.getNinePieceDrawInfo(piece, 1);
        if (piece != MiddlePiece)
            EXPECT_TRUE(drawInfo.isDrawable);
        else
            EXPECT_FALSE(drawInfo.isDrawable);
    }
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_TopLeftDrawable)
{
    NinePieceImage ninePiece;
    ninePiece.setImage(generatedImage());
    ninePiece.setImageSlices(LengthBox(10, 10, 10, 10));

    IntSize imageSize(100, 100);
    IntRect borderImageArea(0, 0, 100, 100);
    IntRectOutsets borderWidths(10, 10, 10, 10);

    const struct {
        IntRectOutsets borderWidths;
        bool expectedIsDrawable;
    } testCases[] = {
        { IntRectOutsets(0, 0, 0, 0), false },
        { IntRectOutsets(10, 0, 0, 0), false },
        { IntRectOutsets(0, 0, 0, 10), false },
        { IntRectOutsets(10, 0, 0, 10), true },
    };

    for (const auto& testCase : testCases) {
        NinePieceImageGrid grid = NinePieceImageGrid(ninePiece, imageSize, borderImageArea, testCase.borderWidths);
        for (NinePiece piece = MinPiece; piece < MaxPiece; ++piece) {
            NinePieceImageGrid::NinePieceDrawInfo drawInfo = grid.getNinePieceDrawInfo(piece, 1);
            if (piece == TopLeftPiece)
                EXPECT_EQ(drawInfo.isDrawable, testCase.expectedIsDrawable);
        }
    }
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_ScaleDownBorder)
{
    NinePieceImage ninePiece;
    ninePiece.setImage(generatedImage());
    ninePiece.setImageSlices(LengthBox(10, 10, 10, 10));

    IntSize imageSize(100, 100);
    IntRect borderImageArea(0, 0, 100, 100);
    IntRectOutsets borderWidths(10, 10, 10, 10);

    // Set border slices wide enough so that the widths are scaled
    // down and corner pieces cover the entire border image area.
    ninePiece.setBorderSlices(BorderImageLengthBox(6));

    NinePieceImageGrid grid = NinePieceImageGrid(ninePiece, imageSize, borderImageArea, borderWidths);
    for (NinePiece piece = MinPiece; piece < MaxPiece; ++piece) {
        NinePieceImageGrid::NinePieceDrawInfo drawInfo = grid.getNinePieceDrawInfo(piece, 1);
        if (drawInfo.isCornerPiece)
            EXPECT_EQ(drawInfo.destination.size(), FloatSize(50, 50));
        else
            EXPECT_TRUE(drawInfo.destination.size().isEmpty());
    }
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting)
{
    const struct {
        IntSize imageSize;
        IntRect borderImageArea;
        IntRectOutsets borderWidths;
        bool fill;
        LengthBox imageSlices;
        Image::TileRule horizontalRule;
        Image::TileRule verticalRule;
        struct {
            bool isDrawable;
            bool isCornerPiece;
            FloatRect destination;
            FloatRect source;
            float tileScaleHorizontal;
            float tileScaleVertical;
            Image::TileRule horizontalRule;
            Image::TileRule verticalRule;
        } pieces[9];
    } testCases[] = {
        {
            // Empty border and slices but with fill
            IntSize(100, 100), IntRect(0, 0, 100, 100), IntRectOutsets(0, 0, 0, 0), true,
            LengthBox(Length(0, Fixed), Length(0, Fixed), Length(0, Fixed), Length(0, Fixed)), Image::StretchTile, Image::StretchTile, {
                { false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1, Image::StretchTile, Image::StretchTile },
                { false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1, Image::StretchTile, Image::StretchTile },
                { false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0, Image::StretchTile, Image::StretchTile },
                { false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1, Image::StretchTile, Image::StretchTile },
                { false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1, Image::StretchTile, Image::StretchTile },
                { false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0, Image::StretchTile, Image::StretchTile },
                { false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0, Image::StretchTile, Image::StretchTile },
                { false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0, Image::StretchTile, Image::StretchTile },
                { true, false, FloatRect(0, 0, 100, 100), FloatRect(0, 0, 100, 100), 1, 1, Image::StretchTile, Image::StretchTile },
        }}, {
            // Single border and fill
            IntSize(100, 100), IntRect(0, 0, 100, 100), IntRectOutsets(0, 0, 10, 0), true,
            LengthBox(Length(20, Percent), Length(20, Percent), Length(20, Percent), Length(20, Percent)), Image::StretchTile, Image::StretchTile, {
                { false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1, Image::StretchTile, Image::StretchTile },
                { false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1, Image::StretchTile, Image::StretchTile },
                { false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0, Image::StretchTile, Image::StretchTile },
                { false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1, Image::StretchTile, Image::StretchTile },
                { false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1, Image::StretchTile, Image::StretchTile },
                { false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0, Image::StretchTile, Image::StretchTile },
                { false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0, Image::StretchTile, Image::StretchTile },
                { true, false, FloatRect(0, 90, 100, 10), FloatRect(20, 80, 60, 20), 0.5, 0.5, Image::StretchTile, Image::StretchTile },
                { true, false, FloatRect(0, 0, 100, 90), FloatRect(20, 20, 60, 60), 1.666667, 1.5, Image::StretchTile, Image::StretchTile },
        }}, {
            // All borders, no fill
            IntSize(100, 100), IntRect(0, 0, 100, 100), IntRectOutsets(10, 10, 10, 10), false,
            LengthBox(Length(20, Percent), Length(20, Percent), Length(20, Percent), Length(20, Percent)), Image::StretchTile, Image::StretchTile, {
                { true, true, FloatRect(0, 0, 10, 10), FloatRect(0, 0, 20, 20), 1, 1, Image::StretchTile, Image::StretchTile },
                { true, true, FloatRect(0, 90, 10, 10), FloatRect(0, 80, 20, 20), 1, 1, Image::StretchTile, Image::StretchTile },
                { true, false, FloatRect(0, 10, 10, 80), FloatRect(0, 20, 20, 60), 0.5, 0.5, Image::StretchTile, Image::StretchTile },
                { true, true, FloatRect(90, 0, 10, 10), FloatRect(80, 0, 20, 20), 1, 1, Image::StretchTile, Image::StretchTile },
                { true, true, FloatRect(90, 90, 10, 10), FloatRect(80, 80, 20, 20), 1, 1, Image::StretchTile, Image::StretchTile },
                { true, false, FloatRect(90, 10, 10, 80), FloatRect(80, 20, 20, 60), 0.5, 0.5, Image::StretchTile, Image::StretchTile },
                { true, false, FloatRect(10, 0, 80, 10), FloatRect(20, 0, 60, 20), 0.5, 0.5, Image::StretchTile, Image::StretchTile },
                { true, false, FloatRect(10, 90, 80, 10), FloatRect(20, 80, 60, 20), 0.5, 0.5, Image::StretchTile, Image::StretchTile },
                { false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0, Image::StretchTile, Image::StretchTile },
        }}, {
            // Single border, no fill
            IntSize(100, 100), IntRect(0, 0, 100, 100), IntRectOutsets(0, 0, 0, 10), false,
            LengthBox(Length(20, Percent), Length(20, Percent), Length(20, Percent), Length(20, Percent)), Image::StretchTile, Image::RoundTile, {
                { false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1, Image::StretchTile, Image::StretchTile },
                { false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1, Image::StretchTile, Image::StretchTile },
                { true, false, FloatRect(0, 0, 10, 100), FloatRect(0, 20, 20, 60), 0.5, 0.5, Image::StretchTile, Image::RoundTile },
                { false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1, Image::StretchTile, Image::StretchTile },
                { false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1, Image::StretchTile, Image::StretchTile },
                { false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0, Image::StretchTile, Image::RoundTile },
                { false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0, Image::StretchTile, Image::RoundTile },
                { false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0, Image::StretchTile, Image::RoundTile },
                { false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0, Image::StretchTile, Image::RoundTile },
        }}, {
            // All borders but no slices, with fill (stretch horizontally, space vertically)
            IntSize(100, 100), IntRect(0, 0, 100, 100), IntRectOutsets(10, 10, 10, 10), true,
            LengthBox(Length(0, Fixed), Length(0, Fixed), Length(0, Fixed), Length(0, Fixed)), Image::StretchTile, Image::SpaceTile, {
                { false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1, Image::StretchTile, Image::StretchTile },
                { false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1, Image::StretchTile, Image::StretchTile },
                { false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0, Image::StretchTile, Image::SpaceTile },
                { false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1, Image::StretchTile, Image::StretchTile },
                { false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1, Image::StretchTile, Image::StretchTile },
                { false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0, Image::StretchTile, Image::SpaceTile },
                { false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0, Image::StretchTile, Image::SpaceTile },
                { false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0, Image::StretchTile, Image::SpaceTile },
                { true, false, FloatRect(10, 10, 80, 80), FloatRect(0, 0, 100, 100), 0.800000, 1, Image::StretchTile, Image::SpaceTile },
        }},
    };

    for (auto& testCase : testCases) {
        NinePieceImage ninePiece;
        ninePiece.setImage(generatedImage());
        ninePiece.setFill(testCase.fill);
        ninePiece.setImageSlices(testCase.imageSlices);
        ninePiece.setHorizontalRule((ENinePieceImageRule)testCase.horizontalRule);
        ninePiece.setVerticalRule((ENinePieceImageRule)testCase.verticalRule);

        NinePieceImageGrid grid = NinePieceImageGrid(ninePiece, testCase.imageSize, testCase.borderImageArea, testCase.borderWidths);
        for (NinePiece piece = MinPiece; piece < MaxPiece; ++piece) {
            NinePieceImageGrid::NinePieceDrawInfo drawInfo = grid.getNinePieceDrawInfo(piece, 1);
            EXPECT_EQ(testCase.pieces[piece].isDrawable, drawInfo.isDrawable);
            if (!testCase.pieces[piece].isDrawable)
                continue;

            EXPECT_EQ(testCase.pieces[piece].destination.x(), drawInfo.destination.x());
            EXPECT_EQ(testCase.pieces[piece].destination.y(), drawInfo.destination.y());
            EXPECT_EQ(testCase.pieces[piece].destination.width(), drawInfo.destination.width());
            EXPECT_EQ(testCase.pieces[piece].destination.height(), drawInfo.destination.height());
            EXPECT_EQ(testCase.pieces[piece].source.x(), drawInfo.source.x());
            EXPECT_EQ(testCase.pieces[piece].source.y(), drawInfo.source.y());
            EXPECT_EQ(testCase.pieces[piece].source.width(), drawInfo.source.width());
            EXPECT_EQ(testCase.pieces[piece].source.height(), drawInfo.source.height());

            if (testCase.pieces[piece].isCornerPiece)
                continue;

            EXPECT_FLOAT_EQ(testCase.pieces[piece].tileScaleHorizontal, drawInfo.tileScale.width());
            EXPECT_FLOAT_EQ(testCase.pieces[piece].tileScaleVertical, drawInfo.tileScale.height());
            EXPECT_EQ(testCase.pieces[piece].horizontalRule, drawInfo.tileRule.horizontal);
            EXPECT_EQ(testCase.pieces[piece].verticalRule, drawInfo.tileRule.vertical);
        }
    }
}

} // namespace
} // namespace blink
