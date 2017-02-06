// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DisplayItemList.h"

#include "SkPictureRecorder.h"
#include "SkTypes.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/SubsequenceDisplayItem.h"
#include "platform/graphics/paint/TransformDisplayItem.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/testing/FakeDisplayItemClient.h"
#include "platform/transforms/AffineTransform.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

#define EXPECT_RECT_EQ(expected, actual) \
    do { \
        const IntRect& actualRect = actual; \
        EXPECT_EQ(expected.x(), actualRect.x()); \
        EXPECT_EQ(expected.y(), actualRect.y()); \
        EXPECT_EQ(expected.width(), actualRect.width()); \
        EXPECT_EQ(expected.height(), actualRect.height()); \
    } while (false)

static const size_t kDefaultListBytes = 10 * 1024;

class DisplayItemListTest : public ::testing::Test {
public:
    DisplayItemListTest()
        : m_list(kDefaultListBytes) {}

    DisplayItemList m_list;
    FakeDisplayItemClient m_client;
};

static PassRefPtr<SkPicture> createRectPicture(const IntRect& bounds)
{
    SkPictureRecorder recorder;
    SkCanvas* canvas = recorder.beginRecording(bounds.width(), bounds.height());
    canvas->drawRect(SkRect::MakeXYWH(bounds.x(), bounds.y(), bounds.width(), bounds.height()),
        SkPaint());
    return fromSkSp(recorder.finishRecordingAsPicture());
}

TEST_F(DisplayItemListTest, AppendVisualRect_Simple)
{
    // One drawing: D.

    IntRect drawingBounds(5, 6, 7, 8);
    m_list.allocateAndConstruct<DrawingDisplayItem>(
        m_client, DisplayItem::Type::DocumentBackground, createRectPicture(drawingBounds), true);
    m_list.appendVisualRect(drawingBounds);

    EXPECT_EQ(static_cast<size_t>(1), m_list.size());
    EXPECT_RECT_EQ(drawingBounds, m_list.visualRect(0));
}

TEST_F(DisplayItemListTest, AppendVisualRect_EmptyBlock)
{
    // One block: B1, E1.

    IntRect subsequenceBounds(5, 6, 7, 8);
    m_list.allocateAndConstruct<BeginSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    m_list.allocateAndConstruct<EndSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    EXPECT_EQ(static_cast<size_t>(2), m_list.size());
    EXPECT_RECT_EQ(subsequenceBounds, m_list.visualRect(0));
    EXPECT_RECT_EQ(subsequenceBounds, m_list.visualRect(1));
}

TEST_F(DisplayItemListTest, AppendVisualRect_EmptyBlockContainingEmptyBlock)
{
    // Two nested blocks: B1, B2, E2, E1.

    IntRect subsequenceBounds(5, 6, 7, 8);
    m_list.allocateAndConstruct<BeginSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    IntRect transformBounds(5, 6, 1, 1);
    AffineTransform transform;
    m_list.allocateAndConstruct<BeginTransformDisplayItem>(m_client, transform);
    m_list.appendVisualRect(transformBounds);

    m_list.allocateAndConstruct<EndTransformDisplayItem>(m_client);
    m_list.appendVisualRect(transformBounds);

    m_list.allocateAndConstruct<EndSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    EXPECT_EQ(static_cast<size_t>(4), m_list.size());
    EXPECT_RECT_EQ(subsequenceBounds, m_list.visualRect(0));
    EXPECT_RECT_EQ(transformBounds, m_list.visualRect(1));
    EXPECT_RECT_EQ(transformBounds, m_list.visualRect(2));
    EXPECT_RECT_EQ(subsequenceBounds, m_list.visualRect(3));
}

TEST_F(DisplayItemListTest, AppendVisualRect_EmptyBlockContainingEscapedEmptyBlock)
{
    // Two nested blocks with the inner block escaping:
    // B1, B2 (escapes), E2, E1.

    IntRect subsequenceBounds(5, 6, 7, 8);
    m_list.allocateAndConstruct<BeginSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    IntRect transformBounds(1, 2, 3, 4);
    AffineTransform transform;
    m_list.allocateAndConstruct<BeginTransformDisplayItem>(m_client, transform);
    m_list.appendVisualRect(transformBounds);

    m_list.allocateAndConstruct<EndTransformDisplayItem>(m_client);
    m_list.appendVisualRect(transformBounds);

    m_list.allocateAndConstruct<EndSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    EXPECT_EQ(static_cast<size_t>(4), m_list.size());
    IntRect mergedSubsequenceBounds(1, 2, 11, 12);
    EXPECT_RECT_EQ(mergedSubsequenceBounds, m_list.visualRect(0));
    EXPECT_RECT_EQ(transformBounds, m_list.visualRect(1));
    EXPECT_RECT_EQ(transformBounds, m_list.visualRect(2));
    EXPECT_RECT_EQ(mergedSubsequenceBounds, m_list.visualRect(3));
}

TEST_F(DisplayItemListTest, AppendVisualRect_BlockContainingDrawing)
{
    // One block with one drawing: B1, Da, E1.

    IntRect subsequenceBounds(5, 6, 7, 8);
    m_list.allocateAndConstruct<BeginSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    IntRect drawingBounds(5, 6, 1, 1);
    m_list.allocateAndConstruct<DrawingDisplayItem>(
        m_client, DisplayItem::Type::DocumentBackground, createRectPicture(drawingBounds), true);
    m_list.appendVisualRect(drawingBounds);

    m_list.allocateAndConstruct<EndSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    EXPECT_EQ(static_cast<size_t>(3), m_list.size());
    EXPECT_RECT_EQ(subsequenceBounds, m_list.visualRect(0));
    EXPECT_RECT_EQ(drawingBounds, m_list.visualRect(1));
    EXPECT_RECT_EQ(subsequenceBounds, m_list.visualRect(2));
}

TEST_F(DisplayItemListTest, AppendVisualRect_BlockContainingEscapedDrawing)
{
    // One block with one drawing: B1, Da (escapes), E1.

    IntRect subsequenceBounds(5, 6, 7, 8);
    m_list.allocateAndConstruct<BeginSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    IntRect drawingBounds(1, 2, 3, 4);
    m_list.allocateAndConstruct<DrawingDisplayItem>(
        m_client, DisplayItem::Type::DocumentBackground, createRectPicture(drawingBounds), true);
    m_list.appendVisualRect(drawingBounds);

    m_list.allocateAndConstruct<EndSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    EXPECT_EQ(static_cast<size_t>(3), m_list.size());
    IntRect mergedSubsequenceBounds(1, 2, 11, 12);
    EXPECT_RECT_EQ(mergedSubsequenceBounds, m_list.visualRect(0));
    EXPECT_RECT_EQ(drawingBounds, m_list.visualRect(1));
    EXPECT_RECT_EQ(mergedSubsequenceBounds, m_list.visualRect(2));
}

TEST_F(DisplayItemListTest, AppendVisualRect_DrawingFollowedByBlockContainingEscapedDrawing)
{
    // One drawing followed by one block with one drawing: Da, B1, Db (escapes), E1.

    IntRect drawingABounds(1, 2, 3, 4);
    m_list.allocateAndConstruct<DrawingDisplayItem>(
        m_client, DisplayItem::Type::DocumentBackground, createRectPicture(drawingABounds), true);
    m_list.appendVisualRect(drawingABounds);

    IntRect subsequenceBounds(5, 6, 7, 8);
    m_list.allocateAndConstruct<BeginSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    IntRect drawingBBounds(13, 14, 1, 1);
    m_list.allocateAndConstruct<DrawingDisplayItem>(
        m_client, DisplayItem::Type::DocumentBackground, createRectPicture(drawingBBounds), true);
    m_list.appendVisualRect(drawingBBounds);

    m_list.allocateAndConstruct<EndSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    EXPECT_EQ(static_cast<size_t>(4), m_list.size());
    EXPECT_RECT_EQ(drawingABounds, m_list.visualRect(0));
    IntRect mergedSubsequenceBounds(5, 6, 9, 9);
    EXPECT_RECT_EQ(mergedSubsequenceBounds, m_list.visualRect(1));
    EXPECT_RECT_EQ(drawingBBounds, m_list.visualRect(2));
    EXPECT_RECT_EQ(mergedSubsequenceBounds, m_list.visualRect(3));
}

TEST_F(DisplayItemListTest, AppendVisualRect_TwoBlocksTwoDrawings)
{
    // Multiple nested blocks with drawings amidst: B1, Da, B2, Db, E2, E1.

    IntRect subsequenceBounds(5, 6, 7, 8);
    m_list.allocateAndConstruct<BeginSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    IntRect drawingABounds(5, 6, 1, 1);
    m_list.allocateAndConstruct<DrawingDisplayItem>(
        m_client, DisplayItem::Type::DocumentBackground, createRectPicture(drawingABounds), true);
    m_list.appendVisualRect(drawingABounds);

    IntRect transformBounds(7, 8, 2, 2);
    AffineTransform transform;
    m_list.allocateAndConstruct<BeginTransformDisplayItem>(m_client, transform);
    m_list.appendVisualRect(transformBounds);

    IntRect drawingBBounds(7, 8, 1, 1);
    m_list.allocateAndConstruct<DrawingDisplayItem>(
        m_client, DisplayItem::Type::DocumentBackground, createRectPicture(drawingBBounds), true);
    m_list.appendVisualRect(drawingBBounds);

    m_list.allocateAndConstruct<EndTransformDisplayItem>(m_client);
    m_list.appendVisualRect(transformBounds);

    m_list.allocateAndConstruct<EndSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    EXPECT_EQ(static_cast<size_t>(6), m_list.size());
    EXPECT_RECT_EQ(subsequenceBounds, m_list.visualRect(0));
    EXPECT_RECT_EQ(drawingABounds, m_list.visualRect(1));
    EXPECT_RECT_EQ(transformBounds, m_list.visualRect(2));
    EXPECT_RECT_EQ(drawingBBounds, m_list.visualRect(3));
    EXPECT_RECT_EQ(transformBounds, m_list.visualRect(4));
    EXPECT_RECT_EQ(subsequenceBounds, m_list.visualRect(5));
}

TEST_F(DisplayItemListTest, AppendVisualRect_TwoBlocksTwoDrawingsInnerDrawingEscaped)
{
    // Multiple nested blocks with drawings amidst: B1, Da, B2, Db (escapes), E2, E1.

    IntRect subsequenceBounds(5, 6, 7, 8);
    m_list.allocateAndConstruct<BeginSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    IntRect drawingABounds(5, 6, 1, 1);
    m_list.allocateAndConstruct<DrawingDisplayItem>(
        m_client, DisplayItem::Type::DocumentBackground, createRectPicture(drawingABounds), true);
    m_list.appendVisualRect(drawingABounds);

    IntRect transformBounds(7, 8, 2, 2);
    AffineTransform transform;
    m_list.allocateAndConstruct<BeginTransformDisplayItem>(m_client, transform);
    m_list.appendVisualRect(transformBounds);

    IntRect drawingBBounds(1, 2, 3, 4);
    m_list.allocateAndConstruct<DrawingDisplayItem>(
        m_client, DisplayItem::Type::DocumentBackground, createRectPicture(drawingBBounds), true);
    m_list.appendVisualRect(drawingBBounds);

    m_list.allocateAndConstruct<EndTransformDisplayItem>(m_client);
    m_list.appendVisualRect(transformBounds);

    m_list.allocateAndConstruct<EndSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    EXPECT_EQ(static_cast<size_t>(6), m_list.size());
    IntRect mergedSubsequenceBounds(1, 2, 11, 12);
    EXPECT_RECT_EQ(mergedSubsequenceBounds, m_list.visualRect(0));
    EXPECT_RECT_EQ(drawingABounds, m_list.visualRect(1));
    IntRect mergedTransformBounds(1, 2, 8, 8);
    EXPECT_RECT_EQ(mergedTransformBounds, m_list.visualRect(2));
    EXPECT_RECT_EQ(drawingBBounds, m_list.visualRect(3));
    EXPECT_RECT_EQ(mergedTransformBounds, m_list.visualRect(4));
    EXPECT_RECT_EQ(mergedSubsequenceBounds, m_list.visualRect(5));
}

TEST_F(DisplayItemListTest, AppendVisualRect_TwoBlocksTwoDrawingsOuterDrawingEscaped)
{
    // Multiple nested blocks with drawings amidst: B1, Da (escapes), B2, Db, E2, E1.

    IntRect subsequenceBounds(5, 6, 7, 8);
    m_list.allocateAndConstruct<BeginSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    IntRect drawingABounds(1, 2, 3, 4);
    m_list.allocateAndConstruct<DrawingDisplayItem>(
        m_client, DisplayItem::Type::DocumentBackground, createRectPicture(drawingABounds), true);
    m_list.appendVisualRect(drawingABounds);

    IntRect transformBounds(7, 8, 2, 2);
    AffineTransform transform;
    m_list.allocateAndConstruct<BeginTransformDisplayItem>(m_client, transform);
    m_list.appendVisualRect(transformBounds);

    IntRect drawingBBounds(7, 8, 1, 1);
    m_list.allocateAndConstruct<DrawingDisplayItem>(
        m_client, DisplayItem::Type::DocumentBackground, createRectPicture(drawingBBounds), true);
    m_list.appendVisualRect(drawingBBounds);

    m_list.allocateAndConstruct<EndTransformDisplayItem>(m_client);
    m_list.appendVisualRect(transformBounds);

    m_list.allocateAndConstruct<EndSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    EXPECT_EQ(static_cast<size_t>(6), m_list.size());
    IntRect mergedSubsequenceBounds(1, 2, 11, 12);
    EXPECT_RECT_EQ(mergedSubsequenceBounds, m_list.visualRect(0));
    EXPECT_RECT_EQ(drawingABounds, m_list.visualRect(1));
    EXPECT_RECT_EQ(transformBounds, m_list.visualRect(2));
    EXPECT_RECT_EQ(drawingBBounds, m_list.visualRect(3));
    EXPECT_RECT_EQ(transformBounds, m_list.visualRect(4));
    EXPECT_RECT_EQ(mergedSubsequenceBounds, m_list.visualRect(5));
}

TEST_F(DisplayItemListTest, AppendVisualRect_TwoBlocksTwoDrawingsBothDrawingsEscaped)
{
    // Multiple nested blocks with drawings amidst:
    // B1, Da (escapes to the right), B2, Db (escapes to the left), E2, E1.

    IntRect subsequenceBounds(5, 6, 7, 8);
    m_list.allocateAndConstruct<BeginSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    IntRect drawingABounds(13, 14, 1, 1);
    m_list.allocateAndConstruct<DrawingDisplayItem>(
        m_client, DisplayItem::Type::DocumentBackground, createRectPicture(drawingABounds), true);
    m_list.appendVisualRect(drawingABounds);

    IntRect transformBounds(7, 8, 2, 2);
    AffineTransform transform;
    m_list.allocateAndConstruct<BeginTransformDisplayItem>(m_client, transform);
    m_list.appendVisualRect(transformBounds);

    IntRect drawingBBounds(1, 2, 3, 4);
    m_list.allocateAndConstruct<DrawingDisplayItem>(
        m_client, DisplayItem::Type::DocumentBackground, createRectPicture(drawingBBounds), true);
    m_list.appendVisualRect(drawingBBounds);

    m_list.allocateAndConstruct<EndTransformDisplayItem>(m_client);
    m_list.appendVisualRect(transformBounds);

    m_list.allocateAndConstruct<EndSubsequenceDisplayItem>(m_client);
    m_list.appendVisualRect(subsequenceBounds);

    EXPECT_EQ(static_cast<size_t>(6), m_list.size());
    IntRect mergedSubsequenceBounds(1, 2, 13, 13);
    EXPECT_RECT_EQ(mergedSubsequenceBounds, m_list.visualRect(0));
    EXPECT_RECT_EQ(drawingABounds, m_list.visualRect(1));
    IntRect mergedTransformBounds(1, 2, 8, 8);
    EXPECT_RECT_EQ(mergedTransformBounds, m_list.visualRect(2));
    EXPECT_RECT_EQ(drawingBBounds, m_list.visualRect(3));
    EXPECT_RECT_EQ(mergedTransformBounds, m_list.visualRect(4));
    EXPECT_RECT_EQ(mergedSubsequenceBounds, m_list.visualRect(5));
}

} // namespace
} // namespace blink
