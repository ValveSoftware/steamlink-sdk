// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintController.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/CachedDisplayItem.h"
#include "platform/graphics/paint/ClipPathDisplayItem.h"
#include "platform/graphics/paint/ClipPathRecorder.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/SubsequenceRecorder.h"
#include "platform/testing/FakeDisplayItemClient.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class PaintControllerTest : public ::testing::Test {
public:
    PaintControllerTest()
        : m_paintController(PaintController::create())
        , m_originalSlimmingPaintV2Enabled(RuntimeEnabledFeatures::slimmingPaintV2Enabled()) { }

    IntRect visualRect(const PaintArtifact& paintArtifact, unsigned index)
    {
        return paintArtifact.getDisplayItemList().visualRect(index);
    }

protected:
    PaintController& getPaintController() { return *m_paintController; }

private:
    void TearDown() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(m_originalSlimmingPaintV2Enabled);
    }

    std::unique_ptr<PaintController> m_paintController;
    bool m_originalSlimmingPaintV2Enabled;
};

const DisplayItem::Type foregroundDrawingType = static_cast<DisplayItem::Type>(DisplayItem::DrawingPaintPhaseFirst + 4);
const DisplayItem::Type backgroundDrawingType = DisplayItem::DrawingPaintPhaseFirst;
const DisplayItem::Type clipType = DisplayItem::ClipFirst;

class TestDisplayItem final : public DisplayItem {
public:
    TestDisplayItem(const FakeDisplayItemClient& client, Type type) : DisplayItem(client, type, sizeof(*this)) { }

    void replay(GraphicsContext&) const final { ASSERT_NOT_REACHED(); }
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const final { ASSERT_NOT_REACHED(); }
};

#ifndef NDEBUG
#define TRACE_DISPLAY_ITEMS(i, expected, actual) \
    String trace = String::format("%d: ", (int)i) + "Expected: " + (expected).asDebugString() + " Actual: " + (actual).asDebugString(); \
    SCOPED_TRACE(trace.utf8().data());
#else
#define TRACE_DISPLAY_ITEMS(i, expected, actual)
#endif

#define EXPECT_DISPLAY_LIST(actual, expectedSize, ...) \
    do { \
        EXPECT_EQ((size_t)expectedSize, actual.size()); \
        if (expectedSize != actual.size()) \
            break; \
        const TestDisplayItem expected[] = { __VA_ARGS__ }; \
        for (size_t index = 0; index < std::min<size_t>(actual.size(), expectedSize); index++) { \
            TRACE_DISPLAY_ITEMS(index, expected[index], actual[index]); \
            EXPECT_EQ(expected[index].client(), actual[index].client()); \
            EXPECT_EQ(expected[index].getType(), actual[index].getType()); \
        } \
    } while (false);

void drawRect(GraphicsContext& context, const FakeDisplayItemClient& client, DisplayItem::Type type, const FloatRect& bounds)
{
    if (DrawingRecorder::useCachedDrawingIfPossible(context, client, type))
        return;
    DrawingRecorder drawingRecorder(context, client, type, bounds);
    IntRect rect(0, 0, 10, 10);
    context.drawRect(rect);
}

void drawClippedRect(GraphicsContext& context, const FakeDisplayItemClient& client, DisplayItem::Type clipType, DisplayItem::Type drawingType, const FloatRect& bound)
{
    ClipRecorder clipRecorder(context, client, clipType, IntRect(1, 1, 9, 9));
    drawRect(context, client, drawingType, bound);
}

TEST_F(PaintControllerTest, NestedRecorders)
{
    GraphicsContext context(getPaintController());

    FakeDisplayItemClient client("client");

    drawClippedRect(context, client, clipType, backgroundDrawingType, FloatRect(100, 100, 200, 200));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
        TestDisplayItem(client, clipType),
        TestDisplayItem(client, backgroundDrawingType),
        TestDisplayItem(client, DisplayItem::clipTypeToEndClipType(clipType)));
}

TEST_F(PaintControllerTest, UpdateBasic)
{
    FakeDisplayItemClient first("first");
    FakeDisplayItemClient second("second");
    GraphicsContext context(getPaintController());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 300, 300));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 200, 200));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 300, 300));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType));

    second.setDisplayItemsUncached();
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 300, 300));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 300, 300));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType));
}

TEST_F(PaintControllerTest, UpdateSwapOrder)
{
    FakeDisplayItemClient first("first");
    FakeDisplayItemClient second("second");
    FakeDisplayItemClient unaffected("unaffected");
    GraphicsContext context(getPaintController());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, unaffected, backgroundDrawingType, FloatRect(300, 300, 10, 10));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(unaffected, backgroundDrawingType));

    second.setDisplayItemsUncached();
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, unaffected, backgroundDrawingType, FloatRect(300, 300, 10, 10));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(unaffected, backgroundDrawingType));
}

TEST_F(PaintControllerTest, UpdateNewItemInMiddle)
{
    FakeDisplayItemClient first("first");
    FakeDisplayItemClient second("second");
    FakeDisplayItemClient third("third");
    GraphicsContext context(getPaintController());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, third, backgroundDrawingType, FloatRect(125, 100, 200, 50));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));
}

TEST_F(PaintControllerTest, UpdateInvalidationWithPhases)
{
    FakeDisplayItemClient first("first");
    FakeDisplayItemClient second("second");
    FakeDisplayItemClient third("third");
    GraphicsContext context(getPaintController());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, third, backgroundDrawingType, FloatRect(300, 100, 50, 50));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, third, foregroundDrawingType, FloatRect(300, 100, 50, 50));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 6,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType),
        TestDisplayItem(third, foregroundDrawingType));

    second.setDisplayItemsUncached();
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, third, backgroundDrawingType, FloatRect(300, 100, 50, 50));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, second, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, third, foregroundDrawingType, FloatRect(300, 100, 50, 50));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 6,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType),
        TestDisplayItem(third, foregroundDrawingType));
}

TEST_F(PaintControllerTest, UpdateAddFirstOverlap)
{
    FakeDisplayItemClient first("first");
    FakeDisplayItemClient second("second");
    GraphicsContext context(getPaintController());

    drawRect(context, second, backgroundDrawingType, FloatRect(200, 200, 50, 50));
    drawRect(context, second, foregroundDrawingType, FloatRect(200, 200, 50, 50));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType));

    first.setDisplayItemsUncached();
    second.setDisplayItemsUncached();
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(200, 200, 50, 50));
    drawRect(context, second, foregroundDrawingType, FloatRect(200, 200, 50, 50));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 4,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType));

    first.setDisplayItemsUncached();
    drawRect(context, second, backgroundDrawingType, FloatRect(200, 200, 50, 50));
    drawRect(context, second, foregroundDrawingType, FloatRect(200, 200, 50, 50));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType));
}

TEST_F(PaintControllerTest, UpdateAddLastOverlap)
{
    FakeDisplayItemClient first("first");
    FakeDisplayItemClient second("second");
    GraphicsContext context(getPaintController());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 150, 150));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType));

    first.setDisplayItemsUncached();
    second.setDisplayItemsUncached();
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(200, 200, 50, 50));
    drawRect(context, second, foregroundDrawingType, FloatRect(200, 200, 50, 50));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 4,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, foregroundDrawingType));

    first.setDisplayItemsUncached();
    second.setDisplayItemsUncached();
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, first, foregroundDrawingType, FloatRect(100, 100, 150, 150));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(first, foregroundDrawingType));
}

TEST_F(PaintControllerTest, UpdateClip)
{
    FakeDisplayItemClient first("first");
    FakeDisplayItemClient second("second");
    GraphicsContext context(getPaintController());

    {
        ClipRecorder clipRecorder(context, first, clipType, IntRect(1, 1, 2, 2));
        drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
        drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    }
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 4,
        TestDisplayItem(first, clipType),
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(first, DisplayItem::clipTypeToEndClipType(clipType)));

    first.setDisplayItemsUncached();
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));

    second.setDisplayItemsUncached();
    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    {
        ClipRecorder clipRecorder(context, second, clipType, IntRect(1, 1, 2, 2));
        drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    }
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 4,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, clipType),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, DisplayItem::clipTypeToEndClipType(clipType)));
}

TEST_F(PaintControllerTest, CachedDisplayItems)
{
    FakeDisplayItemClient first("first");
    FakeDisplayItemClient second("second");
    GraphicsContext context(getPaintController());

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));
    EXPECT_TRUE(getPaintController().clientCacheIsValid(first));
    EXPECT_TRUE(getPaintController().clientCacheIsValid(second));
    const SkPicture* firstPicture = static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[0]).picture();
    const SkPicture* secondPicture = static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[1]).picture();

    first.setDisplayItemsUncached();
    EXPECT_FALSE(getPaintController().clientCacheIsValid(first));
    EXPECT_TRUE(getPaintController().clientCacheIsValid(second));

    drawRect(context, first, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType, FloatRect(100, 100, 150, 150));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, backgroundDrawingType));
    // The first display item should be updated.
    EXPECT_NE(firstPicture, static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[0]).picture());
    // The second display item should be cached.
    EXPECT_EQ(secondPicture, static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[1]).picture());
    EXPECT_TRUE(getPaintController().clientCacheIsValid(first));
    EXPECT_TRUE(getPaintController().clientCacheIsValid(second));

    getPaintController().invalidateAll();
    EXPECT_FALSE(getPaintController().clientCacheIsValid(first));
    EXPECT_FALSE(getPaintController().clientCacheIsValid(second));
}

TEST_F(PaintControllerTest, ComplexUpdateSwapOrder)
{
    FakeDisplayItemClient container1("container1");
    FakeDisplayItemClient content1("content1");
    FakeDisplayItemClient container2("container2");
    FakeDisplayItemClient content2("content2");
    GraphicsContext context(getPaintController());

    drawRect(context, container1, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, content1, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, content1, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, container1, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, container2, backgroundDrawingType, FloatRect(100, 200, 100, 100));
    drawRect(context, content2, backgroundDrawingType, FloatRect(100, 200, 50, 200));
    drawRect(context, content2, foregroundDrawingType, FloatRect(100, 200, 50, 200));
    drawRect(context, container2, foregroundDrawingType, FloatRect(100, 200, 100, 100));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 8,
        TestDisplayItem(container1, backgroundDrawingType),
        TestDisplayItem(content1, backgroundDrawingType),
        TestDisplayItem(content1, foregroundDrawingType),
        TestDisplayItem(container1, foregroundDrawingType),
        TestDisplayItem(container2, backgroundDrawingType),
        TestDisplayItem(content2, backgroundDrawingType),
        TestDisplayItem(content2, foregroundDrawingType),
        TestDisplayItem(container2, foregroundDrawingType));

    // Simulate the situation when container1 e.g. gets a z-index that is now greater than container2.
    container1.setDisplayItemsUncached();
    drawRect(context, container2, backgroundDrawingType, FloatRect(100, 200, 100, 100));
    drawRect(context, content2, backgroundDrawingType, FloatRect(100, 200, 50, 200));
    drawRect(context, content2, foregroundDrawingType, FloatRect(100, 200, 50, 200));
    drawRect(context, container2, foregroundDrawingType, FloatRect(100, 200, 100, 100));
    drawRect(context, container1, backgroundDrawingType, FloatRect(100, 100, 100, 100));
    drawRect(context, content1, backgroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, content1, foregroundDrawingType, FloatRect(100, 100, 50, 200));
    drawRect(context, container1, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 8,
        TestDisplayItem(container2, backgroundDrawingType),
        TestDisplayItem(content2, backgroundDrawingType),
        TestDisplayItem(content2, foregroundDrawingType),
        TestDisplayItem(container2, foregroundDrawingType),
        TestDisplayItem(container1, backgroundDrawingType),
        TestDisplayItem(content1, backgroundDrawingType),
        TestDisplayItem(content1, foregroundDrawingType),
        TestDisplayItem(container1, foregroundDrawingType));
}

TEST_F(PaintControllerTest, CachedSubsequenceSwapOrder)
{
    FakeDisplayItemClient container1("container1");
    FakeDisplayItemClient content1("content1");
    FakeDisplayItemClient container2("container2");
    FakeDisplayItemClient content2("content2");
    GraphicsContext context(getPaintController());

    {
        SubsequenceRecorder r(context, container1);
        drawRect(context, container1, backgroundDrawingType, FloatRect(100, 100, 100, 100));
        drawRect(context, content1, backgroundDrawingType, FloatRect(100, 100, 50, 200));
        drawRect(context, content1, foregroundDrawingType, FloatRect(100, 100, 50, 200));
        drawRect(context, container1, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    }
    {
        SubsequenceRecorder r(context, container2);
        drawRect(context, container2, backgroundDrawingType, FloatRect(100, 200, 100, 100));
        drawRect(context, content2, backgroundDrawingType, FloatRect(100, 200, 50, 200));
        drawRect(context, content2, foregroundDrawingType, FloatRect(100, 200, 50, 200));
        drawRect(context, container2, foregroundDrawingType, FloatRect(100, 200, 100, 100));
    }
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 12,
        TestDisplayItem(container1, DisplayItem::Subsequence),
        TestDisplayItem(container1, backgroundDrawingType),
        TestDisplayItem(content1, backgroundDrawingType),
        TestDisplayItem(content1, foregroundDrawingType),
        TestDisplayItem(container1, foregroundDrawingType),
        TestDisplayItem(container1, DisplayItem::EndSubsequence),

        TestDisplayItem(container2, DisplayItem::Subsequence),
        TestDisplayItem(container2, backgroundDrawingType),
        TestDisplayItem(content2, backgroundDrawingType),
        TestDisplayItem(content2, foregroundDrawingType),
        TestDisplayItem(container2, foregroundDrawingType),
        TestDisplayItem(container2, DisplayItem::EndSubsequence));

    // Simulate the situation when container1 e.g. gets a z-index that is now greater than container2.
    EXPECT_TRUE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container2));
    EXPECT_TRUE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container1));

    EXPECT_DISPLAY_LIST(getPaintController().newDisplayItemList(), 2,
        TestDisplayItem(container2, DisplayItem::CachedSubsequence),
        TestDisplayItem(container1, DisplayItem::CachedSubsequence));

    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 12,
        TestDisplayItem(container2, DisplayItem::Subsequence),
        TestDisplayItem(container2, backgroundDrawingType),
        TestDisplayItem(content2, backgroundDrawingType),
        TestDisplayItem(content2, foregroundDrawingType),
        TestDisplayItem(container2, foregroundDrawingType),
        TestDisplayItem(container2, DisplayItem::EndSubsequence),

        TestDisplayItem(container1, DisplayItem::Subsequence),
        TestDisplayItem(container1, backgroundDrawingType),
        TestDisplayItem(content1, backgroundDrawingType),
        TestDisplayItem(content1, foregroundDrawingType),
        TestDisplayItem(container1, foregroundDrawingType),
        TestDisplayItem(container1, DisplayItem::EndSubsequence));

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    DisplayItemClient::endShouldKeepAliveAllClients();
#endif
}

TEST_F(PaintControllerTest, OutOfOrderNoCrash)
{
    FakeDisplayItemClient client("client");
    GraphicsContext context(getPaintController());

    const DisplayItem::Type type1 = DisplayItem::DrawingFirst;
    const DisplayItem::Type type2 = static_cast<DisplayItem::Type>(DisplayItem::DrawingFirst + 1);
    const DisplayItem::Type type3 = static_cast<DisplayItem::Type>(DisplayItem::DrawingFirst + 2);
    const DisplayItem::Type type4 = static_cast<DisplayItem::Type>(DisplayItem::DrawingFirst + 3);

    drawRect(context, client, type1, FloatRect(100, 100, 100, 100));
    drawRect(context, client, type2, FloatRect(100, 100, 50, 200));
    drawRect(context, client, type3, FloatRect(100, 100, 50, 200));
    drawRect(context, client, type4, FloatRect(100, 100, 100, 100));

    getPaintController().commitNewDisplayItems();

    drawRect(context, client, type2, FloatRect(100, 100, 50, 200));
    drawRect(context, client, type3, FloatRect(100, 100, 50, 200));
    drawRect(context, client, type1, FloatRect(100, 100, 100, 100));
    drawRect(context, client, type4, FloatRect(100, 100, 100, 100));

    getPaintController().commitNewDisplayItems();
}

TEST_F(PaintControllerTest, CachedNestedSubsequenceUpdate)
{
    FakeDisplayItemClient container1("container1");
    FakeDisplayItemClient content1("content1");
    FakeDisplayItemClient container2("container2");
    FakeDisplayItemClient content2("content2");
    GraphicsContext context(getPaintController());

    {
        SubsequenceRecorder r(context, container1);
        drawRect(context, container1, backgroundDrawingType, FloatRect(100, 100, 100, 100));
        {
            SubsequenceRecorder r(context, content1);
            drawRect(context, content1, backgroundDrawingType, FloatRect(100, 100, 50, 200));
            drawRect(context, content1, foregroundDrawingType, FloatRect(100, 100, 50, 200));
        }
        drawRect(context, container1, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    }
    {
        SubsequenceRecorder r(context, container2);
        drawRect(context, container2, backgroundDrawingType, FloatRect(100, 200, 100, 100));
        {
            SubsequenceRecorder r(context, content2);
            drawRect(context, content2, backgroundDrawingType, FloatRect(100, 200, 50, 200));
        }
    }
    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 14,
        TestDisplayItem(container1, DisplayItem::Subsequence),
        TestDisplayItem(container1, backgroundDrawingType),
        TestDisplayItem(content1, DisplayItem::Subsequence),
        TestDisplayItem(content1, backgroundDrawingType),
        TestDisplayItem(content1, foregroundDrawingType),
        TestDisplayItem(content1, DisplayItem::EndSubsequence),
        TestDisplayItem(container1, foregroundDrawingType),
        TestDisplayItem(container1, DisplayItem::EndSubsequence),

        TestDisplayItem(container2, DisplayItem::Subsequence),
        TestDisplayItem(container2, backgroundDrawingType),
        TestDisplayItem(content2, DisplayItem::Subsequence),
        TestDisplayItem(content2, backgroundDrawingType),
        TestDisplayItem(content2, DisplayItem::EndSubsequence),
        TestDisplayItem(container2, DisplayItem::EndSubsequence));

    // Invalidate container1 but not content1.
    container1.setDisplayItemsUncached();

    // Container2 itself now becomes empty (but still has the 'content2' child),
    // and chooses not to output subsequence info.

    container2.setDisplayItemsUncached();
    content2.setDisplayItemsUncached();
    EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container2));
    EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, content2));
    // Content2 now outputs foreground only.
    {
        SubsequenceRecorder r(context, content2);
        drawRect(context, content2, foregroundDrawingType, FloatRect(100, 200, 50, 200));
    }
    // Repaint container1 with foreground only.
    {
        EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container1));
        SubsequenceRecorder r(context, container1);
        // Use cached subsequence of content1.
        EXPECT_TRUE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, content1));
        drawRect(context, container1, foregroundDrawingType, FloatRect(100, 100, 100, 100));
    }
    EXPECT_DISPLAY_LIST(getPaintController().newDisplayItemList(), 7,
        TestDisplayItem(content2, DisplayItem::Subsequence),
        TestDisplayItem(content2, foregroundDrawingType),
        TestDisplayItem(content2, DisplayItem::EndSubsequence),
        TestDisplayItem(container1, DisplayItem::Subsequence),
        TestDisplayItem(content1, DisplayItem::CachedSubsequence),
        TestDisplayItem(container1, foregroundDrawingType),
        TestDisplayItem(container1, DisplayItem::EndSubsequence));

    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 10,
        TestDisplayItem(content2, DisplayItem::Subsequence),
        TestDisplayItem(content2, foregroundDrawingType),
        TestDisplayItem(content2, DisplayItem::EndSubsequence),

        TestDisplayItem(container1, DisplayItem::Subsequence),
        TestDisplayItem(content1, DisplayItem::Subsequence),
        TestDisplayItem(content1, backgroundDrawingType),
        TestDisplayItem(content1, foregroundDrawingType),
        TestDisplayItem(content1, DisplayItem::EndSubsequence),
        TestDisplayItem(container1, foregroundDrawingType),
        TestDisplayItem(container1, DisplayItem::EndSubsequence));

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    DisplayItemClient::endShouldKeepAliveAllClients();
#endif
}

TEST_F(PaintControllerTest, SkipCache)
{
    FakeDisplayItemClient multicol("multicol");
    FakeDisplayItemClient content("content");
    GraphicsContext context(getPaintController());

    FloatRect rect1(100, 100, 50, 50);
    FloatRect rect2(150, 100, 50, 50);
    FloatRect rect3(200, 100, 50, 50);

    drawRect(context, multicol, backgroundDrawingType, FloatRect(100, 200, 100, 100));

    getPaintController().beginSkippingCache();
    drawRect(context, content, foregroundDrawingType, rect1);
    drawRect(context, content, foregroundDrawingType, rect2);
    getPaintController().endSkippingCache();

    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
        TestDisplayItem(multicol, backgroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType));
    RefPtr<const SkPicture> picture1 = static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[1]).picture();
    RefPtr<const SkPicture> picture2 = static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[2]).picture();
    EXPECT_NE(picture1, picture2);

    // Draw again with nothing invalidated.
    EXPECT_TRUE(getPaintController().clientCacheIsValid(multicol));
    drawRect(context, multicol, backgroundDrawingType, FloatRect(100, 200, 100, 100));

    getPaintController().beginSkippingCache();
    drawRect(context, content, foregroundDrawingType, rect1);
    drawRect(context, content, foregroundDrawingType, rect2);
    getPaintController().endSkippingCache();

    EXPECT_DISPLAY_LIST(getPaintController().newDisplayItemList(), 3,
        TestDisplayItem(multicol, DisplayItem::drawingTypeToCachedDrawingType(backgroundDrawingType)),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType));

    getPaintController().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
        TestDisplayItem(multicol, backgroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType));
    EXPECT_NE(picture1, static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[1]).picture());
    EXPECT_NE(picture2, static_cast<const DrawingDisplayItem&>(getPaintController().getDisplayItemList()[2]).picture());

    // Now the multicol becomes 3 columns and repaints.
    multicol.setDisplayItemsUncached();
    drawRect(context, multicol, backgroundDrawingType, FloatRect(100, 100, 100, 100));

    getPaintController().beginSkippingCache();
    drawRect(context, content, foregroundDrawingType, rect1);
    drawRect(context, content, foregroundDrawingType, rect2);
    drawRect(context, content, foregroundDrawingType, rect3);
    getPaintController().endSkippingCache();

    // We should repaint everything on invalidation of the scope container.
    EXPECT_DISPLAY_LIST(getPaintController().newDisplayItemList(), 4,
        TestDisplayItem(multicol, backgroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType),
        TestDisplayItem(content, foregroundDrawingType));
    EXPECT_NE(picture1, static_cast<const DrawingDisplayItem&>(getPaintController().newDisplayItemList()[1]).picture());
    EXPECT_NE(picture2, static_cast<const DrawingDisplayItem&>(getPaintController().newDisplayItemList()[2]).picture());

    getPaintController().commitNewDisplayItems();
}

TEST_F(PaintControllerTest, OptimizeNoopPairs)
{
    FakeDisplayItemClient first("first");
    FakeDisplayItemClient second("second");
    FakeDisplayItemClient third("third");

    GraphicsContext context(getPaintController());
    drawRect(context, first, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    {
        ClipPathRecorder clipRecorder(context, second, Path());
        drawRect(context, second, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    }
    drawRect(context, third, backgroundDrawingType, FloatRect(0, 0, 100, 100));

    getPaintController().commitNewDisplayItems();
    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 5,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(second, DisplayItem::BeginClipPath),
        TestDisplayItem(second, backgroundDrawingType),
        TestDisplayItem(second, DisplayItem::EndClipPath),
        TestDisplayItem(third, backgroundDrawingType));

    second.setDisplayItemsUncached();
    drawRect(context, first, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    {
        ClipRecorder clipRecorder(context, second, clipType, IntRect(1, 1, 2, 2));
        // Do not draw anything for second.
    }
    drawRect(context, third, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    getPaintController().commitNewDisplayItems();

    // Empty clips should have been optimized out.
    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType));

    second.setDisplayItemsUncached();
    drawRect(context, first, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    {
        ClipRecorder clipRecorder(context, second, clipType, IntRect(1, 1, 2, 2));
        {
            ClipPathRecorder clipPathRecorder(context, second, Path());
            // Do not draw anything for second.
        }
    }
    drawRect(context, third, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    getPaintController().commitNewDisplayItems();

    // Empty clips should have been optimized out.
    EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
        TestDisplayItem(first, backgroundDrawingType),
        TestDisplayItem(third, backgroundDrawingType));
}

TEST_F(PaintControllerTest, SmallPaintControllerHasOnePaintChunk)
{
    RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);
    FakeDisplayItemClient client("test client");

    GraphicsContext context(getPaintController());
    drawRect(context, client, backgroundDrawingType, FloatRect(0, 0, 100, 100));

    getPaintController().commitNewDisplayItems();
    const auto& paintChunks = getPaintController().paintChunks();
    ASSERT_EQ(1u, paintChunks.size());
    EXPECT_EQ(0u, paintChunks[0].beginIndex);
    EXPECT_EQ(1u, paintChunks[0].endIndex);
}

#define EXPECT_RECT_EQ(expected, actual) \
    do { \
        const IntRect& actualRect = actual; \
        EXPECT_EQ(expected.x(), actualRect.x()); \
        EXPECT_EQ(expected.y(), actualRect.y()); \
        EXPECT_EQ(expected.width(), actualRect.width()); \
        EXPECT_EQ(expected.height(), actualRect.height()); \
    } while (false)

TEST_F(PaintControllerTest, PaintArtifactWithVisualRects)
{
    FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));

    GraphicsContext context(getPaintController());
    drawRect(context, client, backgroundDrawingType, FloatRect(0, 0, 100, 100));

    getPaintController().commitNewDisplayItems(LayoutSize(20, 30));
    const auto& paintArtifact = getPaintController().paintArtifact();
    ASSERT_EQ(1u, paintArtifact.getDisplayItemList().size());
    EXPECT_RECT_EQ(IntRect(-20, -30, 200, 100), visualRect(paintArtifact, 0));
}

void drawPath(GraphicsContext& context, DisplayItemClient& client, DisplayItem::Type type, unsigned count)
{
    if (DrawingRecorder::useCachedDrawingIfPossible(context, client, type))
        return;

    DrawingRecorder drawingRecorder(context, client, type, FloatRect(0, 0, 100, 100));
    SkPath path;
    path.moveTo(0, 0);
    path.lineTo(0, 100);
    path.lineTo(50, 50);
    path.lineTo(100, 100);
    path.lineTo(100, 0);
    path.close();
    SkPaint paint;
    paint.setAntiAlias(true);
    for (unsigned i = 0; i < count; i++)
        context.drawPath(path, paint);
}

TEST_F(PaintControllerTest, IsSuitableForGpuRasterizationSinglePath)
{
    FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));
    GraphicsContext context(getPaintController());
    drawPath(context, client, backgroundDrawingType, 1);
    getPaintController().commitNewDisplayItems(LayoutSize());
    EXPECT_TRUE(getPaintController().paintArtifact().isSuitableForGpuRasterization());
}

TEST_F(PaintControllerTest, IsNotSuitableForGpuRasterizationSinglePictureManyPaths)
{
    FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));
    GraphicsContext context(getPaintController());

    drawPath(context, client, backgroundDrawingType, 50);
    getPaintController().commitNewDisplayItems(LayoutSize());
    EXPECT_FALSE(getPaintController().paintArtifact().isSuitableForGpuRasterization());
}

TEST_F(PaintControllerTest, IsNotSuitableForGpuRasterizationMultiplePicturesSinglePathEach)
{
    FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));
    GraphicsContext context(getPaintController());
    getPaintController().beginSkippingCache();

    for (int i = 0; i < 50; ++i)
        drawPath(context, client, backgroundDrawingType, 50);

    getPaintController().endSkippingCache();
    getPaintController().commitNewDisplayItems(LayoutSize());
    EXPECT_FALSE(getPaintController().paintArtifact().isSuitableForGpuRasterization());
}

TEST_F(PaintControllerTest, IsNotSuitableForGpuRasterizationSinglePictureManyPathsTwoPaints)
{
    FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));

    {
        GraphicsContext context(getPaintController());
        drawPath(context, client, backgroundDrawingType, 50);
        getPaintController().commitNewDisplayItems(LayoutSize());
        EXPECT_FALSE(getPaintController().paintArtifact().isSuitableForGpuRasterization());
    }

    client.setDisplayItemsUncached();

    {
        GraphicsContext context(getPaintController());
        drawPath(context, client, backgroundDrawingType, 50);
        getPaintController().commitNewDisplayItems(LayoutSize());
        EXPECT_FALSE(getPaintController().paintArtifact().isSuitableForGpuRasterization());
    }
}

TEST_F(PaintControllerTest, IsNotSuitableForGpuRasterizationSinglePictureManyPathsCached)
{
    FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));

    {
        GraphicsContext context(getPaintController());
        drawPath(context, client, backgroundDrawingType, 50);
        getPaintController().commitNewDisplayItems(LayoutSize());
        EXPECT_FALSE(getPaintController().paintArtifact().isSuitableForGpuRasterization());
    }

    {
        GraphicsContext context(getPaintController());
        drawPath(context, client, backgroundDrawingType, 50);
        getPaintController().commitNewDisplayItems(LayoutSize());
        EXPECT_FALSE(getPaintController().paintArtifact().isSuitableForGpuRasterization());
    }
}

TEST_F(PaintControllerTest, IsNotSuitableForGpuRasterizationSinglePictureManyPathsCachedSubsequence)
{
    FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));
    FakeDisplayItemClient container("container", LayoutRect(0, 0, 200, 100));

    GraphicsContext context(getPaintController());
    {
        SubsequenceRecorder subsequenceRecorder(context, container);
        drawPath(context, client, backgroundDrawingType, 50);
    }
    getPaintController().commitNewDisplayItems(LayoutSize());
    EXPECT_FALSE(getPaintController().paintArtifact().isSuitableForGpuRasterization());

    EXPECT_TRUE(SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container));
    getPaintController().commitNewDisplayItems(LayoutSize());
    EXPECT_FALSE(getPaintController().paintArtifact().isSuitableForGpuRasterization());

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    DisplayItemClient::endShouldKeepAliveAllClients();
#endif
}

// Temporarily disabled (pref regressions due to GPU veto stickiness: http://crbug.com/603969).
TEST_F(PaintControllerTest, DISABLED_IsNotSuitableForGpuRasterizationConcaveClipPath)
{
    Path path;
    path.addLineTo(FloatPoint(50, 50));
    path.addLineTo(FloatPoint(100, 0));
    path.addLineTo(FloatPoint(50, 100));
    path.closeSubpath();

    FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));
    GraphicsContext context(getPaintController());

    // Run twice for empty/non-empty m_currentPaintArtifact coverage.
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 50; ++j)
            getPaintController().createAndAppend<BeginClipPathDisplayItem>(client, path);
        drawRect(context, client, backgroundDrawingType, FloatRect(0, 0, 100, 100));
        for (int j = 0; j < 50; ++j)
            getPaintController().createAndAppend<EndClipPathDisplayItem>(client);
        getPaintController().commitNewDisplayItems(LayoutSize());
        EXPECT_FALSE(getPaintController().paintArtifact().isSuitableForGpuRasterization());
    }
}

} // namespace blink
