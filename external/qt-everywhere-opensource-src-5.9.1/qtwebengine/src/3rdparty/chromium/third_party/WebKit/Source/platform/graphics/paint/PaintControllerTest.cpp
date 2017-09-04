// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintController.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/ClipPathDisplayItem.h"
#include "platform/graphics/paint/ClipPathRecorder.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/CompositingRecorder.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/SubsequenceRecorder.h"
#include "platform/testing/FakeDisplayItemClient.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

using testing::UnorderedElementsAre;

namespace blink {

class PaintControllerTestBase : public testing::Test {
 public:
  PaintControllerTestBase() : m_paintController(PaintController::create()) {}

  IntRect visualRect(const PaintArtifact& paintArtifact, size_t index) {
    return paintArtifact.getDisplayItemList().visualRect(index);
  }

 protected:
  PaintController& getPaintController() { return *m_paintController; }

  int numCachedNewItems() const {
    return m_paintController->m_numCachedNewItems;
  }

#ifndef NDEBUG
  int numSequentialMatches() const {
    return m_paintController->m_numSequentialMatches;
  }
  int numOutOfOrderMatches() const {
    return m_paintController->m_numOutOfOrderMatches;
  }
  int numIndexedItems() const { return m_paintController->m_numIndexedItems; }
#endif

  void TearDown() override { m_featuresBackup.restore(); }

 private:
  std::unique_ptr<PaintController> m_paintController;
  RuntimeEnabledFeatures::Backup m_featuresBackup;
};

PaintChunkProperties rootPaintChunkProperties() {
  PaintChunkProperties rootProperties;
  rootProperties.transform = TransformPaintPropertyNode::root();
  rootProperties.clip = ClipPaintPropertyNode::root();
  rootProperties.effect = EffectPaintPropertyNode::root();
  rootProperties.scroll = ScrollPaintPropertyNode::root();
  return rootProperties;
}

const DisplayItem::Type foregroundDrawingType =
    static_cast<DisplayItem::Type>(DisplayItem::kDrawingPaintPhaseFirst + 4);
const DisplayItem::Type backgroundDrawingType =
    DisplayItem::kDrawingPaintPhaseFirst;
const DisplayItem::Type clipType = DisplayItem::kClipFirst;

class TestDisplayItem final : public DisplayItem {
 public:
  TestDisplayItem(const FakeDisplayItemClient& client, Type type)
      : DisplayItem(client, type, sizeof(*this)) {}

  void replay(GraphicsContext&) const final { ASSERT_NOT_REACHED(); }
  void appendToWebDisplayItemList(const IntRect&,
                                  WebDisplayItemList*) const final {
    ASSERT_NOT_REACHED();
  }
};

#ifndef NDEBUG
#define TRACE_DISPLAY_ITEMS(i, expected, actual)                 \
  String trace = String::format("%d: ", (int)i) + "Expected: " + \
                 (expected).asDebugString() + " Actual: " +      \
                 (actual).asDebugString();                       \
  SCOPED_TRACE(trace.utf8().data());
#else
#define TRACE_DISPLAY_ITEMS(i, expected, actual)
#endif

#define EXPECT_DISPLAY_LIST(actual, expectedSize, ...)                     \
  do {                                                                     \
    EXPECT_EQ((size_t)expectedSize, actual.size());                        \
    if (expectedSize != actual.size())                                     \
      break;                                                               \
    const TestDisplayItem expected[] = {__VA_ARGS__};                      \
    for (size_t index = 0;                                                 \
         index < std::min<size_t>(actual.size(), expectedSize); index++) { \
      TRACE_DISPLAY_ITEMS(index, expected[index], actual[index]);          \
      EXPECT_EQ(expected[index].client(), actual[index].client());         \
      EXPECT_EQ(expected[index].getType(), actual[index].getType());       \
    }                                                                      \
  } while (false);

void drawRect(GraphicsContext& context,
              const FakeDisplayItemClient& client,
              DisplayItem::Type type,
              const FloatRect& bounds) {
  if (DrawingRecorder::useCachedDrawingIfPossible(context, client, type))
    return;
  DrawingRecorder drawingRecorder(context, client, type, bounds);
  IntRect rect(0, 0, 10, 10);
  context.drawRect(rect);
}

void drawClippedRect(GraphicsContext& context,
                     const FakeDisplayItemClient& client,
                     DisplayItem::Type clipType,
                     DisplayItem::Type drawingType,
                     const FloatRect& bound) {
  ClipRecorder clipRecorder(context, client, clipType, IntRect(1, 1, 9, 9));
  drawRect(context, client, drawingType, bound);
}

enum TestConfigurations {
  SPv1,
  SPv2,
  UnderInvalidationCheckingSPv1,
  UnderInvalidationCheckingSPv2,
};

// Tests using this class will be tested with under-invalidation-checking
// enabled and disabled.
class PaintControllerTest
    : public PaintControllerTestBase,
      public testing::WithParamInterface<TestConfigurations> {
 public:
  PaintControllerTest()
      : m_rootPaintPropertyClient("root"),
        m_rootPaintChunkId(m_rootPaintPropertyClient,
                           DisplayItem::kUninitializedType) {}

 protected:
  void SetUp() override {
    switch (GetParam()) {
      case SPv1:
        break;
      case SPv2:
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);
        break;
      case UnderInvalidationCheckingSPv1:
        RuntimeEnabledFeatures::setPaintUnderInvalidationCheckingEnabled(true);
        break;
      case UnderInvalidationCheckingSPv2:
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);
        RuntimeEnabledFeatures::setPaintUnderInvalidationCheckingEnabled(true);
        break;
    }
  }

  FakeDisplayItemClient m_rootPaintPropertyClient;
  PaintChunk::Id m_rootPaintChunkId;
};

INSTANTIATE_TEST_CASE_P(All,
                        PaintControllerTest,
                        ::testing::Values(SPv1,
                                          SPv2,
                                          UnderInvalidationCheckingSPv1,
                                          UnderInvalidationCheckingSPv2));

TEST_P(PaintControllerTest, NestedRecorders) {
  GraphicsContext context(getPaintController());
  FakeDisplayItemClient client("client", LayoutRect(100, 100, 200, 200));
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  drawClippedRect(context, client, clipType, backgroundDrawingType,
                  FloatRect(100, 100, 200, 200));
  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(
      getPaintController().getDisplayItemList(), 3,
      TestDisplayItem(client, clipType),
      TestDisplayItem(client, backgroundDrawingType),
      TestDisplayItem(client, DisplayItem::clipTypeToEndClipType(clipType)));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, getPaintController().paintChunks().size());
    EXPECT_THAT(getPaintController().paintChunks()[0].rasterInvalidationRects,
                UnorderedElementsAre(FloatRect(LayoutRect::infiniteIntRect())));
  }
}

TEST_P(PaintControllerTest, UpdateBasic) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 300, 300));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 200, 200));
  GraphicsContext context(getPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  drawRect(context, first, backgroundDrawingType,
           FloatRect(100, 100, 300, 300));
  drawRect(context, second, backgroundDrawingType,
           FloatRect(100, 100, 200, 200));
  drawRect(context, first, foregroundDrawingType,
           FloatRect(100, 100, 300, 300));

  EXPECT_EQ(0, numCachedNewItems());

  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
                      TestDisplayItem(first, backgroundDrawingType),
                      TestDisplayItem(second, backgroundDrawingType),
                      TestDisplayItem(first, foregroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, getPaintController().paintChunks().size());
    EXPECT_THAT(getPaintController().paintChunks()[0].rasterInvalidationRects,
                UnorderedElementsAre(FloatRect(LayoutRect::infiniteIntRect())));

    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  drawRect(context, first, backgroundDrawingType,
           FloatRect(100, 100, 300, 300));
  drawRect(context, first, foregroundDrawingType,
           FloatRect(100, 100, 300, 300));

  EXPECT_EQ(2, numCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(2, numSequentialMatches());
  EXPECT_EQ(0, numOutOfOrderMatches());
  EXPECT_EQ(1, numIndexedItems());
#endif

  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
                      TestDisplayItem(first, backgroundDrawingType),
                      TestDisplayItem(first, foregroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, getPaintController().paintChunks().size());
    EXPECT_THAT(
        getPaintController().paintChunks()[0].rasterInvalidationRects,
        UnorderedElementsAre(FloatRect(
            100, 100, 200, 200)));  // |second| disappeared from the chunk.
  }
}

TEST_P(PaintControllerTest, UpdateSwapOrder) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient unaffected("unaffected", LayoutRect(300, 300, 10, 10));
  GraphicsContext context(getPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  drawRect(context, first, backgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, first, foregroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, second, backgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, second, foregroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, unaffected, backgroundDrawingType,
           FloatRect(300, 300, 10, 10));
  drawRect(context, unaffected, foregroundDrawingType,
           FloatRect(300, 300, 10, 10));
  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 6,
                      TestDisplayItem(first, backgroundDrawingType),
                      TestDisplayItem(first, foregroundDrawingType),
                      TestDisplayItem(second, backgroundDrawingType),
                      TestDisplayItem(second, foregroundDrawingType),
                      TestDisplayItem(unaffected, backgroundDrawingType),
                      TestDisplayItem(unaffected, foregroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }
  drawRect(context, second, backgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, second, foregroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, first, backgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, first, foregroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, unaffected, backgroundDrawingType,
           FloatRect(300, 300, 10, 10));
  drawRect(context, unaffected, foregroundDrawingType,
           FloatRect(300, 300, 10, 10));

  EXPECT_EQ(6, numCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(5, numSequentialMatches());  // second, first foreground, unaffected
  EXPECT_EQ(1, numOutOfOrderMatches());  // first
  EXPECT_EQ(2, numIndexedItems());       // first
#endif

  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 6,
                      TestDisplayItem(second, backgroundDrawingType),
                      TestDisplayItem(second, foregroundDrawingType),
                      TestDisplayItem(first, backgroundDrawingType),
                      TestDisplayItem(first, foregroundDrawingType),
                      TestDisplayItem(unaffected, backgroundDrawingType),
                      TestDisplayItem(unaffected, foregroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, getPaintController().paintChunks().size());
    EXPECT_THAT(getPaintController().paintChunks()[0].rasterInvalidationRects,
                UnorderedElementsAre(
                    FloatRect(100, 100, 50, 200)));  // Bounds of |second|.
  }
}

TEST_P(PaintControllerTest, UpdateSwapOrderWithInvalidation) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient unaffected("unaffected", LayoutRect(300, 300, 10, 10));
  GraphicsContext context(getPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  drawRect(context, first, backgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, first, foregroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, second, backgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, second, foregroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, unaffected, backgroundDrawingType,
           FloatRect(300, 300, 10, 10));
  drawRect(context, unaffected, foregroundDrawingType,
           FloatRect(300, 300, 10, 10));
  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 6,
                      TestDisplayItem(first, backgroundDrawingType),
                      TestDisplayItem(first, foregroundDrawingType),
                      TestDisplayItem(second, backgroundDrawingType),
                      TestDisplayItem(second, foregroundDrawingType),
                      TestDisplayItem(unaffected, backgroundDrawingType),
                      TestDisplayItem(unaffected, foregroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  first.setDisplayItemsUncached();
  drawRect(context, second, backgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, second, foregroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, first, backgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, first, foregroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, unaffected, backgroundDrawingType,
           FloatRect(300, 300, 10, 10));
  drawRect(context, unaffected, foregroundDrawingType,
           FloatRect(300, 300, 10, 10));

  EXPECT_EQ(4, numCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(4, numSequentialMatches());  // second, unaffected
  EXPECT_EQ(0, numOutOfOrderMatches());
  EXPECT_EQ(2, numIndexedItems());
#endif

  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 6,
                      TestDisplayItem(second, backgroundDrawingType),
                      TestDisplayItem(second, foregroundDrawingType),
                      TestDisplayItem(first, backgroundDrawingType),
                      TestDisplayItem(first, foregroundDrawingType),
                      TestDisplayItem(unaffected, backgroundDrawingType),
                      TestDisplayItem(unaffected, foregroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, getPaintController().paintChunks().size());
    EXPECT_THAT(getPaintController().paintChunks()[0].rasterInvalidationRects,
                UnorderedElementsAre(
                    FloatRect(100, 100, 100, 100),    // Old bounds of |first|.
                    FloatRect(100, 100, 100, 100)));  // New bounds of |first|.
    // No need to invalidate raster of |second|, because the client (|first|)
    // which swapped order with it has been invalidated.
  }
}

TEST_P(PaintControllerTest, UpdateNewItemInMiddle) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient third("third", LayoutRect(125, 100, 200, 50));
  GraphicsContext context(getPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  drawRect(context, first, backgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, second, backgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
                      TestDisplayItem(first, backgroundDrawingType),
                      TestDisplayItem(second, backgroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  drawRect(context, first, backgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, third, backgroundDrawingType, FloatRect(125, 100, 200, 50));
  drawRect(context, second, backgroundDrawingType,
           FloatRect(100, 100, 50, 200));

  EXPECT_EQ(2, numCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(2, numSequentialMatches());  // first, second
  EXPECT_EQ(0, numOutOfOrderMatches());
  EXPECT_EQ(0, numIndexedItems());
#endif

  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
                      TestDisplayItem(first, backgroundDrawingType),
                      TestDisplayItem(third, backgroundDrawingType),
                      TestDisplayItem(second, backgroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, getPaintController().paintChunks().size());
    EXPECT_THAT(
        getPaintController().paintChunks()[0].rasterInvalidationRects,
        UnorderedElementsAre(FloatRect(
            125, 100, 200, 50)));  // |third| newly appeared in the chunk.
  }
}

TEST_P(PaintControllerTest, UpdateInvalidationWithPhases) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient third("third", LayoutRect(300, 100, 50, 50));
  GraphicsContext context(getPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  drawRect(context, first, backgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, second, backgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, third, backgroundDrawingType, FloatRect(300, 100, 50, 50));
  drawRect(context, first, foregroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, second, foregroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, third, foregroundDrawingType, FloatRect(300, 100, 50, 50));
  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 6,
                      TestDisplayItem(first, backgroundDrawingType),
                      TestDisplayItem(second, backgroundDrawingType),
                      TestDisplayItem(third, backgroundDrawingType),
                      TestDisplayItem(first, foregroundDrawingType),
                      TestDisplayItem(second, foregroundDrawingType),
                      TestDisplayItem(third, foregroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  second.setDisplayItemsUncached();
  drawRect(context, first, backgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, second, backgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, third, backgroundDrawingType, FloatRect(300, 100, 50, 50));
  drawRect(context, first, foregroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, second, foregroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, third, foregroundDrawingType, FloatRect(300, 100, 50, 50));

  EXPECT_EQ(4, numCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(4, numSequentialMatches());
  EXPECT_EQ(0, numOutOfOrderMatches());
  EXPECT_EQ(2, numIndexedItems());
#endif

  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 6,
                      TestDisplayItem(first, backgroundDrawingType),
                      TestDisplayItem(second, backgroundDrawingType),
                      TestDisplayItem(third, backgroundDrawingType),
                      TestDisplayItem(first, foregroundDrawingType),
                      TestDisplayItem(second, foregroundDrawingType),
                      TestDisplayItem(third, foregroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, getPaintController().paintChunks().size());
    EXPECT_THAT(getPaintController().paintChunks()[0].rasterInvalidationRects,
                UnorderedElementsAre(
                    FloatRect(100, 100, 50, 200),    // Old bounds of |second|.
                    FloatRect(100, 100, 50, 200)));  // New bounds of |second|.
  }
}

TEST_P(PaintControllerTest, UpdateAddFirstOverlap) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 150, 150));
  FakeDisplayItemClient second("second", LayoutRect(200, 200, 50, 50));
  GraphicsContext context(getPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  drawRect(context, second, backgroundDrawingType, FloatRect(200, 200, 50, 50));
  drawRect(context, second, foregroundDrawingType, FloatRect(200, 200, 50, 50));
  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
                      TestDisplayItem(second, backgroundDrawingType),
                      TestDisplayItem(second, foregroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  first.setDisplayItemsUncached();
  second.setDisplayItemsUncached();
  second.setVisualRect(LayoutRect(150, 150, 100, 100));
  drawRect(context, first, backgroundDrawingType,
           FloatRect(100, 100, 150, 150));
  drawRect(context, first, foregroundDrawingType,
           FloatRect(100, 100, 150, 150));
  drawRect(context, second, backgroundDrawingType,
           FloatRect(150, 150, 100, 100));
  drawRect(context, second, foregroundDrawingType,
           FloatRect(150, 150, 100, 100));
  EXPECT_EQ(0, numCachedNewItems());
  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 4,
                      TestDisplayItem(first, backgroundDrawingType),
                      TestDisplayItem(first, foregroundDrawingType),
                      TestDisplayItem(second, backgroundDrawingType),
                      TestDisplayItem(second, foregroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, getPaintController().paintChunks().size());
    EXPECT_THAT(getPaintController().paintChunks()[0].rasterInvalidationRects,
                UnorderedElementsAre(
                    FloatRect(100, 100, 150,
                              150),  // |first| newly appeared in the chunk.
                    FloatRect(200, 200, 50, 50),      // Old bounds of |second|.
                    FloatRect(150, 150, 100, 100)));  // New bounds of |second|.

    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  drawRect(context, second, backgroundDrawingType,
           FloatRect(150, 150, 100, 100));
  drawRect(context, second, foregroundDrawingType,
           FloatRect(150, 150, 100, 100));

  EXPECT_EQ(2, numCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(2, numSequentialMatches());
  EXPECT_EQ(0, numOutOfOrderMatches());
  EXPECT_EQ(2, numIndexedItems());
#endif

  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
                      TestDisplayItem(second, backgroundDrawingType),
                      TestDisplayItem(second, foregroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, getPaintController().paintChunks().size());
    EXPECT_THAT(
        getPaintController().paintChunks()[0].rasterInvalidationRects,
        UnorderedElementsAre(FloatRect(
            100, 100, 150, 150)));  // |first| disappeared from the chunk.
  }
}

TEST_P(PaintControllerTest, UpdateAddLastOverlap) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 150, 150));
  FakeDisplayItemClient second("second", LayoutRect(200, 200, 50, 50));
  GraphicsContext context(getPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  drawRect(context, first, backgroundDrawingType,
           FloatRect(100, 100, 150, 150));
  drawRect(context, first, foregroundDrawingType,
           FloatRect(100, 100, 150, 150));
  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
                      TestDisplayItem(first, backgroundDrawingType),
                      TestDisplayItem(first, foregroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  first.setDisplayItemsUncached();
  first.setVisualRect(LayoutRect(150, 150, 100, 100));
  second.setDisplayItemsUncached();
  drawRect(context, first, backgroundDrawingType,
           FloatRect(150, 150, 100, 100));
  drawRect(context, first, foregroundDrawingType,
           FloatRect(150, 150, 100, 100));
  drawRect(context, second, backgroundDrawingType, FloatRect(200, 200, 50, 50));
  drawRect(context, second, foregroundDrawingType, FloatRect(200, 200, 50, 50));
  EXPECT_EQ(0, numCachedNewItems());
  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 4,
                      TestDisplayItem(first, backgroundDrawingType),
                      TestDisplayItem(first, foregroundDrawingType),
                      TestDisplayItem(second, backgroundDrawingType),
                      TestDisplayItem(second, foregroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, getPaintController().paintChunks().size());
    EXPECT_THAT(getPaintController().paintChunks()[0].rasterInvalidationRects,
                UnorderedElementsAre(
                    FloatRect(100, 100, 150, 150),  // Old bounds of |first|.
                    FloatRect(150, 150, 100, 100),  // New bounds of |first|.
                    FloatRect(200, 200, 50,
                              50)));  // |second| newly appeared in the chunk.

    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  first.setDisplayItemsUncached();
  first.setVisualRect(LayoutRect(100, 100, 150, 150));
  second.setDisplayItemsUncached();
  drawRect(context, first, backgroundDrawingType,
           FloatRect(100, 100, 150, 150));
  drawRect(context, first, foregroundDrawingType,
           FloatRect(100, 100, 150, 150));
  EXPECT_EQ(0, numCachedNewItems());
  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
                      TestDisplayItem(first, backgroundDrawingType),
                      TestDisplayItem(first, foregroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, getPaintController().paintChunks().size());
    EXPECT_THAT(getPaintController().paintChunks()[0].rasterInvalidationRects,
                UnorderedElementsAre(
                    FloatRect(150, 150, 100, 100),  // Old bounds of |first|.
                    FloatRect(100, 100, 150, 150),  // New bounds of |first|.
                    FloatRect(200, 200, 50,
                              50)));  // |second| disappeared from the chunk.
  }
}

TEST_P(PaintControllerTest, UpdateClip) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 150, 150));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 200, 200));
  GraphicsContext context(getPaintController());

  {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(first, clipType);
      PaintChunkProperties properties = rootPaintChunkProperties();
      properties.clip = ClipPaintPropertyNode::create(
          nullptr, nullptr, FloatRoundedRect(1, 1, 2, 2));
      getPaintController().updateCurrentPaintChunkProperties(&id, properties);
    }
    ClipRecorder clipRecorder(context, first, clipType, IntRect(1, 1, 2, 2));
    drawRect(context, first, backgroundDrawingType,
             FloatRect(100, 100, 150, 150));
    drawRect(context, second, backgroundDrawingType,
             FloatRect(100, 100, 200, 200));
  }
  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(
      getPaintController().getDisplayItemList(), 4,
      TestDisplayItem(first, clipType),
      TestDisplayItem(first, backgroundDrawingType),
      TestDisplayItem(second, backgroundDrawingType),
      TestDisplayItem(first, DisplayItem::clipTypeToEndClipType(clipType)));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  first.setDisplayItemsUncached();
  drawRect(context, first, backgroundDrawingType,
           FloatRect(100, 100, 150, 150));
  drawRect(context, second, backgroundDrawingType,
           FloatRect(100, 100, 200, 200));

  EXPECT_EQ(1, numCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(1, numSequentialMatches());
  EXPECT_EQ(0, numOutOfOrderMatches());
  EXPECT_EQ(1, numIndexedItems());
#endif

  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
                      TestDisplayItem(first, backgroundDrawingType),
                      TestDisplayItem(second, backgroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, getPaintController().paintChunks().size());
    EXPECT_THAT(getPaintController().paintChunks()[0].rasterInvalidationRects,
                UnorderedElementsAre(FloatRect(
                    LayoutRect::infiniteIntRect())));  // This is a new chunk.

    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  second.setDisplayItemsUncached();
  drawRect(context, first, backgroundDrawingType,
           FloatRect(100, 100, 150, 150));
  {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(second, clipType);
      PaintChunkProperties properties = rootPaintChunkProperties();
      properties.clip = ClipPaintPropertyNode::create(
          nullptr, nullptr, FloatRoundedRect(1, 1, 2, 2));
      getPaintController().updateCurrentPaintChunkProperties(&id, properties);
    }
    ClipRecorder clipRecorder(context, second, clipType, IntRect(1, 1, 2, 2));
    drawRect(context, second, backgroundDrawingType,
             FloatRect(100, 100, 200, 200));
  }
  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(
      getPaintController().getDisplayItemList(), 4,
      TestDisplayItem(first, backgroundDrawingType),
      TestDisplayItem(second, clipType),
      TestDisplayItem(second, backgroundDrawingType),
      TestDisplayItem(second, DisplayItem::clipTypeToEndClipType(clipType)));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(2u, getPaintController().paintChunks().size());
    EXPECT_THAT(getPaintController().paintChunks()[0].rasterInvalidationRects,
                UnorderedElementsAre(FloatRect(
                    100, 100, 200,
                    200)));  // |second| disappeared from the first chunk.
    EXPECT_THAT(getPaintController().paintChunks()[1].rasterInvalidationRects,
                UnorderedElementsAre(FloatRect(
                    LayoutRect::infiniteIntRect())));  // This is a new chunk.
  }
}

TEST_P(PaintControllerTest, CachedDisplayItems) {
  FakeDisplayItemClient first("first");
  FakeDisplayItemClient second("second");
  GraphicsContext context(getPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  drawRect(context, first, backgroundDrawingType,
           FloatRect(100, 100, 150, 150));
  drawRect(context, second, backgroundDrawingType,
           FloatRect(100, 100, 150, 150));
  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
                      TestDisplayItem(first, backgroundDrawingType),
                      TestDisplayItem(second, backgroundDrawingType));
  EXPECT_TRUE(getPaintController().clientCacheIsValid(first));
  EXPECT_TRUE(getPaintController().clientCacheIsValid(second));
  const SkPicture* firstPicture =
      static_cast<const DrawingDisplayItem&>(
          getPaintController().getDisplayItemList()[0])
          .picture();
  const SkPicture* secondPicture =
      static_cast<const DrawingDisplayItem&>(
          getPaintController().getDisplayItemList()[1])
          .picture();

  first.setDisplayItemsUncached();
  EXPECT_FALSE(getPaintController().clientCacheIsValid(first));
  EXPECT_TRUE(getPaintController().clientCacheIsValid(second));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }
  drawRect(context, first, backgroundDrawingType,
           FloatRect(100, 100, 150, 150));
  drawRect(context, second, backgroundDrawingType,
           FloatRect(100, 100, 150, 150));
  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 2,
                      TestDisplayItem(first, backgroundDrawingType),
                      TestDisplayItem(second, backgroundDrawingType));
  // The first display item should be updated.
  EXPECT_NE(firstPicture, static_cast<const DrawingDisplayItem&>(
                              getPaintController().getDisplayItemList()[0])
                              .picture());
  // The second display item should be cached.
  if (!RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled())
    EXPECT_EQ(secondPicture, static_cast<const DrawingDisplayItem&>(
                                 getPaintController().getDisplayItemList()[1])
                                 .picture());
  EXPECT_TRUE(getPaintController().clientCacheIsValid(first));
  EXPECT_TRUE(getPaintController().clientCacheIsValid(second));

  getPaintController().invalidateAll();
  EXPECT_FALSE(getPaintController().clientCacheIsValid(first));
  EXPECT_FALSE(getPaintController().clientCacheIsValid(second));
}

TEST_P(PaintControllerTest, UpdateSwapOrderWithChildren) {
  FakeDisplayItemClient container1("container1",
                                   LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient content1("content1", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2",
                                   LayoutRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", LayoutRect(100, 200, 50, 200));
  GraphicsContext context(getPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  drawRect(context, container1, backgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, content1, backgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, content1, foregroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, container1, foregroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, container2, backgroundDrawingType,
           FloatRect(100, 200, 100, 100));
  drawRect(context, content2, backgroundDrawingType,
           FloatRect(100, 200, 50, 200));
  drawRect(context, content2, foregroundDrawingType,
           FloatRect(100, 200, 50, 200));
  drawRect(context, container2, foregroundDrawingType,
           FloatRect(100, 200, 100, 100));
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

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  // Simulate the situation when |container1| gets a z-index that is greater
  // than that of |container2|.
  drawRect(context, container2, backgroundDrawingType,
           FloatRect(100, 200, 100, 100));
  drawRect(context, content2, backgroundDrawingType,
           FloatRect(100, 200, 50, 200));
  drawRect(context, content2, foregroundDrawingType,
           FloatRect(100, 200, 50, 200));
  drawRect(context, container2, foregroundDrawingType,
           FloatRect(100, 200, 100, 100));
  drawRect(context, container1, backgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, content1, backgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, content1, foregroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, container1, foregroundDrawingType,
           FloatRect(100, 100, 100, 100));
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

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, getPaintController().paintChunks().size());
    EXPECT_THAT(
        getPaintController().paintChunks()[0].rasterInvalidationRects,
        UnorderedElementsAre(
            FloatRect(100, 200, 100, 100),   // Bounds of |container2| which was
                                             // moved behind |container1|.
            FloatRect(100, 200, 50, 200)));  // Bounds of |content2| which was
                                             // moved along with |container2|.
  }
}

TEST_P(PaintControllerTest, UpdateSwapOrderWithChildrenAndInvalidation) {
  FakeDisplayItemClient container1("container1",
                                   LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient content1("content1", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2",
                                   LayoutRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", LayoutRect(100, 200, 50, 200));
  GraphicsContext context(getPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  drawRect(context, container1, backgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, content1, backgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, content1, foregroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, container1, foregroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, container2, backgroundDrawingType,
           FloatRect(100, 200, 100, 100));
  drawRect(context, content2, backgroundDrawingType,
           FloatRect(100, 200, 50, 200));
  drawRect(context, content2, foregroundDrawingType,
           FloatRect(100, 200, 50, 200));
  drawRect(context, container2, foregroundDrawingType,
           FloatRect(100, 200, 100, 100));
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

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  // Simulate the situation when |container1| gets a z-index that is greater
  // than that of |container2|, and |container1| is invalidated.
  container1.setDisplayItemsUncached();
  drawRect(context, container2, backgroundDrawingType,
           FloatRect(100, 200, 100, 100));
  drawRect(context, content2, backgroundDrawingType,
           FloatRect(100, 200, 50, 200));
  drawRect(context, content2, foregroundDrawingType,
           FloatRect(100, 200, 50, 200));
  drawRect(context, container2, foregroundDrawingType,
           FloatRect(100, 200, 100, 100));
  drawRect(context, container1, backgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, content1, backgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, content1, foregroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, container1, foregroundDrawingType,
           FloatRect(100, 100, 100, 100));
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

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, getPaintController().paintChunks().size());
    EXPECT_THAT(
        getPaintController().paintChunks()[0].rasterInvalidationRects,
        UnorderedElementsAre(
            FloatRect(100, 100, 100, 100),   // Old bounds of |container1|.
            FloatRect(100, 100, 100, 100),   // New bounds of |container1|.
            FloatRect(100, 200, 100, 100),   // Bounds of |container2| which was
                                             // moved behind |container1|.
            FloatRect(100, 200, 50, 200)));  // Bounds of |content2| which was
                                             // moved along with |container2|.
  }
}

TEST_P(PaintControllerTest, CachedSubsequenceSwapOrder) {
  FakeDisplayItemClient container1("container1",
                                   LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient content1("content1", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2",
                                   LayoutRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", LayoutRect(100, 200, 50, 200));
  GraphicsContext context(getPaintController());

  PaintChunkProperties container1Properties = rootPaintChunkProperties();
  PaintChunkProperties container2Properties = rootPaintChunkProperties();

  {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(container1, backgroundDrawingType);
      container1Properties.effect = EffectPaintPropertyNode::create(
          EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
          ClipPaintPropertyNode::root(), CompositorFilterOperations(), 0.5);
      getPaintController().updateCurrentPaintChunkProperties(
          &id, container1Properties);
    }
    SubsequenceRecorder r(context, container1);
    drawRect(context, container1, backgroundDrawingType,
             FloatRect(100, 100, 100, 100));
    drawRect(context, content1, backgroundDrawingType,
             FloatRect(100, 100, 50, 200));
    drawRect(context, content1, foregroundDrawingType,
             FloatRect(100, 100, 50, 200));
    drawRect(context, container1, foregroundDrawingType,
             FloatRect(100, 100, 100, 100));
  }
  {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(container2, backgroundDrawingType);
      container2Properties.effect = EffectPaintPropertyNode::create(
          EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
          ClipPaintPropertyNode::root(), CompositorFilterOperations(), 0.5);
      getPaintController().updateCurrentPaintChunkProperties(
          &id, container2Properties);
    }
    SubsequenceRecorder r(context, container2);
    drawRect(context, container2, backgroundDrawingType,
             FloatRect(100, 200, 100, 100));
    drawRect(context, content2, backgroundDrawingType,
             FloatRect(100, 200, 50, 200));
    drawRect(context, content2, foregroundDrawingType,
             FloatRect(100, 200, 50, 200));
    drawRect(context, container2, foregroundDrawingType,
             FloatRect(100, 200, 100, 100));
  }
  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(
      getPaintController().getDisplayItemList(), 12,
      TestDisplayItem(container1, DisplayItem::kSubsequence),
      TestDisplayItem(container1, backgroundDrawingType),
      TestDisplayItem(content1, backgroundDrawingType),
      TestDisplayItem(content1, foregroundDrawingType),
      TestDisplayItem(container1, foregroundDrawingType),
      TestDisplayItem(container1, DisplayItem::kEndSubsequence),

      TestDisplayItem(container2, DisplayItem::kSubsequence),
      TestDisplayItem(container2, backgroundDrawingType),
      TestDisplayItem(content2, backgroundDrawingType),
      TestDisplayItem(content2, foregroundDrawingType),
      TestDisplayItem(container2, foregroundDrawingType),
      TestDisplayItem(container2, DisplayItem::kEndSubsequence));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(2u, getPaintController().paintChunks().size());
    EXPECT_EQ(PaintChunk::Id(container1, backgroundDrawingType),
              getPaintController().paintChunks()[0].id);
    EXPECT_EQ(PaintChunk::Id(container2, backgroundDrawingType),
              getPaintController().paintChunks()[1].id);
    EXPECT_THAT(getPaintController().paintChunks()[0].rasterInvalidationRects,
                UnorderedElementsAre(FloatRect(LayoutRect::infiniteIntRect())));
    EXPECT_THAT(getPaintController().paintChunks()[1].rasterInvalidationRects,
                UnorderedElementsAre(FloatRect(LayoutRect::infiniteIntRect())));
  }

  // Simulate the situation when |container1| gets a z-index that is greater
  // than that of |container2|.
  if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled()) {
    // When under-invalidation-checking is enabled,
    // useCachedSubsequenceIfPossible is forced off, and the client is expected
    // to create the same painting as in the previous paint.
    EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(
        context, container2));
    {
      if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
        PaintChunk::Id id(container2, backgroundDrawingType);
        getPaintController().updateCurrentPaintChunkProperties(
            &id, container2Properties);
      }
      SubsequenceRecorder r(context, container2);
      drawRect(context, container2, backgroundDrawingType,
               FloatRect(100, 200, 100, 100));
      drawRect(context, content2, backgroundDrawingType,
               FloatRect(100, 200, 50, 200));
      drawRect(context, content2, foregroundDrawingType,
               FloatRect(100, 200, 50, 200));
      drawRect(context, container2, foregroundDrawingType,
               FloatRect(100, 200, 100, 100));
    }
    EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(
        context, container1));
    {
      if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
        PaintChunk::Id id(container1, backgroundDrawingType);
        getPaintController().updateCurrentPaintChunkProperties(
            &id, container1Properties);
      }
      SubsequenceRecorder r(context, container1);
      drawRect(context, container1, backgroundDrawingType,
               FloatRect(100, 100, 100, 100));
      drawRect(context, content1, backgroundDrawingType,
               FloatRect(100, 100, 50, 200));
      drawRect(context, content1, foregroundDrawingType,
               FloatRect(100, 100, 50, 200));
      drawRect(context, container1, foregroundDrawingType,
               FloatRect(100, 100, 100, 100));
    }
  } else {
    EXPECT_TRUE(SubsequenceRecorder::useCachedSubsequenceIfPossible(
        context, container2));
    EXPECT_TRUE(SubsequenceRecorder::useCachedSubsequenceIfPossible(
        context, container1));
  }

  EXPECT_EQ(12, numCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(1, numSequentialMatches());
  EXPECT_EQ(1, numOutOfOrderMatches());
  EXPECT_EQ(5, numIndexedItems());
#endif

  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(
      getPaintController().getDisplayItemList(), 12,
      TestDisplayItem(container2, DisplayItem::kSubsequence),
      TestDisplayItem(container2, backgroundDrawingType),
      TestDisplayItem(content2, backgroundDrawingType),
      TestDisplayItem(content2, foregroundDrawingType),
      TestDisplayItem(container2, foregroundDrawingType),
      TestDisplayItem(container2, DisplayItem::kEndSubsequence),

      TestDisplayItem(container1, DisplayItem::kSubsequence),
      TestDisplayItem(container1, backgroundDrawingType),
      TestDisplayItem(content1, backgroundDrawingType),
      TestDisplayItem(content1, foregroundDrawingType),
      TestDisplayItem(container1, foregroundDrawingType),
      TestDisplayItem(container1, DisplayItem::kEndSubsequence));

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
  DisplayItemClient::endShouldKeepAliveAllClients();
#endif

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(2u, getPaintController().paintChunks().size());
    EXPECT_EQ(PaintChunk::Id(container2, backgroundDrawingType),
              getPaintController().paintChunks()[0].id);
    EXPECT_EQ(PaintChunk::Id(container1, backgroundDrawingType),
              getPaintController().paintChunks()[1].id);
    // Swapping order of chunks should not invalidate anything.
    EXPECT_THAT(getPaintController().paintChunks()[0].rasterInvalidationRects,
                UnorderedElementsAre());
    EXPECT_THAT(getPaintController().paintChunks()[1].rasterInvalidationRects,
                UnorderedElementsAre());
  }
}

TEST_P(PaintControllerTest, UpdateSwapOrderCrossingChunks) {
  FakeDisplayItemClient container1("container1",
                                   LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient content1("content1", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2",
                                   LayoutRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", LayoutRect(100, 200, 50, 200));
  GraphicsContext context(getPaintController());

  PaintChunkProperties container1Properties = rootPaintChunkProperties();
  PaintChunkProperties container2Properties = rootPaintChunkProperties();

  {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(container1, backgroundDrawingType);
      container1Properties.effect = EffectPaintPropertyNode::create(
          EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
          ClipPaintPropertyNode::root(), CompositorFilterOperations(), 0.5);
      getPaintController().updateCurrentPaintChunkProperties(
          &id, container1Properties);
    }
    drawRect(context, container1, backgroundDrawingType,
             FloatRect(100, 100, 100, 100));
    drawRect(context, content1, backgroundDrawingType,
             FloatRect(100, 100, 50, 200));
  }
  {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(container2, backgroundDrawingType);
      container2Properties.effect = EffectPaintPropertyNode::create(
          EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
          ClipPaintPropertyNode::root(), CompositorFilterOperations(), 0.5);
      getPaintController().updateCurrentPaintChunkProperties(
          &id, container2Properties);
    }
    drawRect(context, container2, backgroundDrawingType,
             FloatRect(100, 200, 100, 100));
    drawRect(context, content2, backgroundDrawingType,
             FloatRect(100, 200, 50, 200));
  }
  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 4,
                      TestDisplayItem(container1, backgroundDrawingType),
                      TestDisplayItem(content1, backgroundDrawingType),
                      TestDisplayItem(container2, backgroundDrawingType),
                      TestDisplayItem(content2, backgroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(2u, getPaintController().paintChunks().size());
    EXPECT_EQ(PaintChunk::Id(container1, backgroundDrawingType),
              getPaintController().paintChunks()[0].id);
    EXPECT_EQ(PaintChunk::Id(container2, backgroundDrawingType),
              getPaintController().paintChunks()[1].id);
    EXPECT_THAT(getPaintController().paintChunks()[0].rasterInvalidationRects,
                UnorderedElementsAre(FloatRect(LayoutRect::infiniteIntRect())));
    EXPECT_THAT(getPaintController().paintChunks()[1].rasterInvalidationRects,
                UnorderedElementsAre(FloatRect(LayoutRect::infiniteIntRect())));
  }

  // Move content2 into container1, without invalidation.
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    PaintChunk::Id id(container1, backgroundDrawingType);
    getPaintController().updateCurrentPaintChunkProperties(
        &id, container1Properties);
  }
  drawRect(context, container1, backgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  drawRect(context, content1, backgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  drawRect(context, content2, backgroundDrawingType,
           FloatRect(100, 200, 50, 200));
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    PaintChunk::Id id(container2, backgroundDrawingType);
    getPaintController().updateCurrentPaintChunkProperties(
        &id, container2Properties);
  }
  drawRect(context, container2, backgroundDrawingType,
           FloatRect(100, 200, 100, 100));

  EXPECT_EQ(4, numCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(3, numSequentialMatches());
  EXPECT_EQ(1, numOutOfOrderMatches());
  EXPECT_EQ(1, numIndexedItems());
#endif

  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 4,
                      TestDisplayItem(container1, backgroundDrawingType),
                      TestDisplayItem(content1, backgroundDrawingType),
                      TestDisplayItem(content2, backgroundDrawingType),
                      TestDisplayItem(container2, backgroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(2u, getPaintController().paintChunks().size());
    EXPECT_EQ(PaintChunk::Id(container1, backgroundDrawingType),
              getPaintController().paintChunks()[0].id);
    EXPECT_EQ(PaintChunk::Id(container2, backgroundDrawingType),
              getPaintController().paintChunks()[1].id);
    // |content2| is invalidated raster on both the old chunk and the new chunk.
    EXPECT_THAT(getPaintController().paintChunks()[0].rasterInvalidationRects,
                UnorderedElementsAre(FloatRect(100, 200, 50, 200)));
    EXPECT_THAT(getPaintController().paintChunks()[1].rasterInvalidationRects,
                UnorderedElementsAre(FloatRect(100, 200, 50, 200)));
  }
}

TEST_P(PaintControllerTest, OutOfOrderNoCrash) {
  FakeDisplayItemClient client("client");
  GraphicsContext context(getPaintController());

  const DisplayItem::Type type1 = DisplayItem::kDrawingFirst;
  const DisplayItem::Type type2 =
      static_cast<DisplayItem::Type>(DisplayItem::kDrawingFirst + 1);
  const DisplayItem::Type type3 =
      static_cast<DisplayItem::Type>(DisplayItem::kDrawingFirst + 2);
  const DisplayItem::Type type4 =
      static_cast<DisplayItem::Type>(DisplayItem::kDrawingFirst + 3);

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }
  drawRect(context, client, type1, FloatRect(100, 100, 100, 100));
  drawRect(context, client, type2, FloatRect(100, 100, 50, 200));
  drawRect(context, client, type3, FloatRect(100, 100, 50, 200));
  drawRect(context, client, type4, FloatRect(100, 100, 100, 100));

  getPaintController().commitNewDisplayItems();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }
  drawRect(context, client, type2, FloatRect(100, 100, 50, 200));
  drawRect(context, client, type3, FloatRect(100, 100, 50, 200));
  drawRect(context, client, type1, FloatRect(100, 100, 100, 100));
  drawRect(context, client, type4, FloatRect(100, 100, 100, 100));

  getPaintController().commitNewDisplayItems();
}

TEST_P(PaintControllerTest, CachedNestedSubsequenceUpdate) {
  FakeDisplayItemClient container1("container1",
                                   LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient content1("content1", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2",
                                   LayoutRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", LayoutRect(100, 200, 50, 200));
  GraphicsContext context(getPaintController());

  PaintChunkProperties container1BackgroundProperties =
      rootPaintChunkProperties();
  PaintChunkProperties content1Properties = rootPaintChunkProperties();
  PaintChunkProperties container1ForegroundProperties =
      rootPaintChunkProperties();
  PaintChunkProperties container2BackgroundProperties =
      rootPaintChunkProperties();
  PaintChunkProperties content2Properties = rootPaintChunkProperties();

  {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(container1, backgroundDrawingType);
      container1BackgroundProperties.effect = EffectPaintPropertyNode::create(
          EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
          ClipPaintPropertyNode::root(), CompositorFilterOperations(), 0.5);
      getPaintController().updateCurrentPaintChunkProperties(
          &id, container1BackgroundProperties);
    }
    SubsequenceRecorder r(context, container1);
    drawRect(context, container1, backgroundDrawingType,
             FloatRect(100, 100, 100, 100));
    {
      if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
        PaintChunk::Id id(content1, backgroundDrawingType);
        content1Properties.effect = EffectPaintPropertyNode::create(
            EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
            ClipPaintPropertyNode::root(), CompositorFilterOperations(), 0.6);
        getPaintController().updateCurrentPaintChunkProperties(
            &id, content1Properties);
      }
      SubsequenceRecorder r(context, content1);
      drawRect(context, content1, backgroundDrawingType,
               FloatRect(100, 100, 50, 200));
      drawRect(context, content1, foregroundDrawingType,
               FloatRect(100, 100, 50, 200));
    }
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(container1, foregroundDrawingType);
      container1ForegroundProperties.effect = EffectPaintPropertyNode::create(
          EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
          ClipPaintPropertyNode::root(), CompositorFilterOperations(), 0.5);
      getPaintController().updateCurrentPaintChunkProperties(
          &id, container1ForegroundProperties);
    }
    drawRect(context, container1, foregroundDrawingType,
             FloatRect(100, 100, 100, 100));
  }
  {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(container2, backgroundDrawingType);
      container2BackgroundProperties.effect = EffectPaintPropertyNode::create(
          EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
          ClipPaintPropertyNode::root(), CompositorFilterOperations(), 0.7);
      getPaintController().updateCurrentPaintChunkProperties(
          &id, container2BackgroundProperties);
    }
    SubsequenceRecorder r(context, container2);
    drawRect(context, container2, backgroundDrawingType,
             FloatRect(100, 200, 100, 100));
    {
      if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
        PaintChunk::Id id(content2, backgroundDrawingType);
        content2Properties.effect = EffectPaintPropertyNode::create(
            EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
            ClipPaintPropertyNode::root(), CompositorFilterOperations(), 0.8);
        getPaintController().updateCurrentPaintChunkProperties(
            &id, content2Properties);
      }
      SubsequenceRecorder r(context, content2);
      drawRect(context, content2, backgroundDrawingType,
               FloatRect(100, 200, 50, 200));
    }
  }
  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(
      getPaintController().getDisplayItemList(), 14,
      TestDisplayItem(container1, DisplayItem::kSubsequence),
      TestDisplayItem(container1, backgroundDrawingType),
      TestDisplayItem(content1, DisplayItem::kSubsequence),
      TestDisplayItem(content1, backgroundDrawingType),
      TestDisplayItem(content1, foregroundDrawingType),
      TestDisplayItem(content1, DisplayItem::kEndSubsequence),
      TestDisplayItem(container1, foregroundDrawingType),
      TestDisplayItem(container1, DisplayItem::kEndSubsequence),

      TestDisplayItem(container2, DisplayItem::kSubsequence),
      TestDisplayItem(container2, backgroundDrawingType),
      TestDisplayItem(content2, DisplayItem::kSubsequence),
      TestDisplayItem(content2, backgroundDrawingType),
      TestDisplayItem(content2, DisplayItem::kEndSubsequence),
      TestDisplayItem(container2, DisplayItem::kEndSubsequence));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(5u, getPaintController().paintChunks().size());
    EXPECT_EQ(PaintChunk::Id(container1, backgroundDrawingType),
              getPaintController().paintChunks()[0].id);
    EXPECT_EQ(PaintChunk::Id(content1, backgroundDrawingType),
              getPaintController().paintChunks()[1].id);
    EXPECT_EQ(PaintChunk::Id(container1, foregroundDrawingType),
              getPaintController().paintChunks()[2].id);
    EXPECT_EQ(PaintChunk::Id(container2, backgroundDrawingType),
              getPaintController().paintChunks()[3].id);
    EXPECT_EQ(PaintChunk::Id(content2, backgroundDrawingType),
              getPaintController().paintChunks()[4].id);
    EXPECT_THAT(getPaintController().paintChunks()[0].rasterInvalidationRects,
                UnorderedElementsAre(FloatRect(LayoutRect::infiniteIntRect())));
    EXPECT_THAT(getPaintController().paintChunks()[1].rasterInvalidationRects,
                UnorderedElementsAre(FloatRect(LayoutRect::infiniteIntRect())));
    EXPECT_THAT(getPaintController().paintChunks()[2].rasterInvalidationRects,
                UnorderedElementsAre(FloatRect(LayoutRect::infiniteIntRect())));
    EXPECT_THAT(getPaintController().paintChunks()[3].rasterInvalidationRects,
                UnorderedElementsAre(FloatRect(LayoutRect::infiniteIntRect())));
    EXPECT_THAT(getPaintController().paintChunks()[4].rasterInvalidationRects,
                UnorderedElementsAre(FloatRect(LayoutRect::infiniteIntRect())));
  }

  // Invalidate container1 but not content1.
  container1.setDisplayItemsUncached();

  // Container2 itself now becomes empty (but still has the 'content2' child),
  // and chooses not to output subsequence info.

  container2.setDisplayItemsUncached();
  content2.setDisplayItemsUncached();
  EXPECT_FALSE(
      SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container2));
  EXPECT_FALSE(
      SubsequenceRecorder::useCachedSubsequenceIfPossible(context, content2));
  // Content2 now outputs foreground only.
  {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(content2, foregroundDrawingType);
      getPaintController().updateCurrentPaintChunkProperties(
          &id, content2Properties);
    }
    SubsequenceRecorder r(context, content2);
    drawRect(context, content2, foregroundDrawingType,
             FloatRect(100, 200, 50, 200));
  }
  // Repaint container1 with foreground only.
  {
    EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(
        context, container1));
    SubsequenceRecorder r(context, container1);
    // Use cached subsequence of content1.
    if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled()) {
      // When under-invalidation-checking is enabled,
      // useCachedSubsequenceIfPossible is forced off, and the client is
      // expected to create the same painting as in the previous paint.
      EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(
          context, content1));
      if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
        PaintChunk::Id id(content1, backgroundDrawingType);
        getPaintController().updateCurrentPaintChunkProperties(
            &id, content1Properties);
      }
      SubsequenceRecorder r(context, content1);
      drawRect(context, content1, backgroundDrawingType,
               FloatRect(100, 100, 50, 200));
      drawRect(context, content1, foregroundDrawingType,
               FloatRect(100, 100, 50, 200));
    } else {
      EXPECT_TRUE(SubsequenceRecorder::useCachedSubsequenceIfPossible(
          context, content1));
    }
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(container1, foregroundDrawingType);
      getPaintController().updateCurrentPaintChunkProperties(
          &id, container1ForegroundProperties);
    }
    drawRect(context, container1, foregroundDrawingType,
             FloatRect(100, 100, 100, 100));
  }

  EXPECT_EQ(4, numCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(1, numSequentialMatches());
  EXPECT_EQ(0, numOutOfOrderMatches());
  EXPECT_EQ(2, numIndexedItems());
#endif

  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(
      getPaintController().getDisplayItemList(), 10,
      TestDisplayItem(content2, DisplayItem::kSubsequence),
      TestDisplayItem(content2, foregroundDrawingType),
      TestDisplayItem(content2, DisplayItem::kEndSubsequence),

      TestDisplayItem(container1, DisplayItem::kSubsequence),
      TestDisplayItem(content1, DisplayItem::kSubsequence),
      TestDisplayItem(content1, backgroundDrawingType),
      TestDisplayItem(content1, foregroundDrawingType),
      TestDisplayItem(content1, DisplayItem::kEndSubsequence),
      TestDisplayItem(container1, foregroundDrawingType),
      TestDisplayItem(container1, DisplayItem::kEndSubsequence));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(3u, getPaintController().paintChunks().size());
    EXPECT_EQ(PaintChunk::Id(content2, foregroundDrawingType),
              getPaintController().paintChunks()[0].id);
    EXPECT_EQ(PaintChunk::Id(content1, backgroundDrawingType),
              getPaintController().paintChunks()[1].id);
    EXPECT_EQ(PaintChunk::Id(container1, foregroundDrawingType),
              getPaintController().paintChunks()[2].id);
    // This is a new chunk.
    EXPECT_THAT(getPaintController().paintChunks()[0].rasterInvalidationRects,
                UnorderedElementsAre(FloatRect(LayoutRect::infiniteIntRect())));
    // This chunk didn't change.
    EXPECT_THAT(getPaintController().paintChunks()[1].rasterInvalidationRects,
                UnorderedElementsAre());
    // |container1| is invalidated.
    EXPECT_THAT(
        getPaintController().paintChunks()[2].rasterInvalidationRects,
        UnorderedElementsAre(
            FloatRect(100, 100, 100, 100),    // Old bounds of |container1|.
            FloatRect(100, 100, 100, 100)));  // New bounds of |container1|.
  }

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
  DisplayItemClient::endShouldKeepAliveAllClients();
#endif
}

TEST_P(PaintControllerTest, SkipCache) {
  FakeDisplayItemClient multicol("multicol", LayoutRect(100, 100, 200, 200));
  FakeDisplayItemClient content("content", LayoutRect(100, 100, 100, 100));
  GraphicsContext context(getPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  FloatRect rect1(100, 100, 50, 50);
  FloatRect rect2(150, 100, 50, 50);
  FloatRect rect3(200, 100, 50, 50);

  drawRect(context, multicol, backgroundDrawingType,
           FloatRect(100, 200, 100, 100));

  getPaintController().beginSkippingCache();
  drawRect(context, content, foregroundDrawingType, rect1);
  drawRect(context, content, foregroundDrawingType, rect2);
  getPaintController().endSkippingCache();

  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
                      TestDisplayItem(multicol, backgroundDrawingType),
                      TestDisplayItem(content, foregroundDrawingType),
                      TestDisplayItem(content, foregroundDrawingType));
  sk_sp<const SkPicture> picture1 =
      sk_ref_sp(static_cast<const DrawingDisplayItem&>(
                    getPaintController().getDisplayItemList()[1])
                    .picture());
  sk_sp<const SkPicture> picture2 =
      sk_ref_sp(static_cast<const DrawingDisplayItem&>(
                    getPaintController().getDisplayItemList()[2])
                    .picture());
  EXPECT_NE(picture1, picture2);

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, getPaintController().paintChunks().size());
    EXPECT_THAT(getPaintController().paintChunks()[0].rasterInvalidationRects,
                UnorderedElementsAre(FloatRect(LayoutRect::infiniteIntRect())));

    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  // Draw again with nothing invalidated.
  EXPECT_TRUE(getPaintController().clientCacheIsValid(multicol));
  drawRect(context, multicol, backgroundDrawingType,
           FloatRect(100, 200, 100, 100));

  getPaintController().beginSkippingCache();
  drawRect(context, content, foregroundDrawingType, rect1);
  drawRect(context, content, foregroundDrawingType, rect2);
  getPaintController().endSkippingCache();

  EXPECT_EQ(1, numCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(1, numSequentialMatches());
  EXPECT_EQ(0, numOutOfOrderMatches());
  EXPECT_EQ(0, numIndexedItems());
#endif

  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
                      TestDisplayItem(multicol, backgroundDrawingType),
                      TestDisplayItem(content, foregroundDrawingType),
                      TestDisplayItem(content, foregroundDrawingType));
  EXPECT_NE(picture1.get(), static_cast<const DrawingDisplayItem&>(
                                getPaintController().getDisplayItemList()[1])
                                .picture());
  EXPECT_NE(picture2.get(), static_cast<const DrawingDisplayItem&>(
                                getPaintController().getDisplayItemList()[2])
                                .picture());

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, getPaintController().paintChunks().size());
    EXPECT_THAT(
        getPaintController().paintChunks()[0].rasterInvalidationRects,
        UnorderedElementsAre(
            FloatRect(100, 100, 100, 100),    // Old bounds of |content|.
            FloatRect(100, 100, 100, 100)));  // New bounds of |content|.

    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }

  // Now the multicol becomes 3 columns and repaints.
  multicol.setDisplayItemsUncached();
  drawRect(context, multicol, backgroundDrawingType,
           FloatRect(100, 100, 100, 100));

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
  EXPECT_NE(picture1.get(), static_cast<const DrawingDisplayItem&>(
                                getPaintController().newDisplayItemList()[1])
                                .picture());
  EXPECT_NE(picture2.get(), static_cast<const DrawingDisplayItem&>(
                                getPaintController().newDisplayItemList()[2])
                                .picture());

  getPaintController().commitNewDisplayItems();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, getPaintController().paintChunks().size());
    EXPECT_THAT(
        getPaintController().paintChunks()[0].rasterInvalidationRects,
        UnorderedElementsAre(
            FloatRect(100, 100, 200, 200),    // Old bounds of |multicol|.
            FloatRect(100, 100, 200, 200),    // New bounds of |multicol|.
            FloatRect(100, 100, 100, 100),    // Old bounds of |content|.
            FloatRect(100, 100, 100, 100)));  // New bounds of |content|.
  }
}

TEST_P(PaintControllerTest, PartialSkipCache) {
  FakeDisplayItemClient content("content");
  GraphicsContext context(getPaintController());

  FloatRect rect1(100, 100, 50, 50);
  FloatRect rect2(150, 100, 50, 50);
  FloatRect rect3(200, 100, 50, 50);

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }
  drawRect(context, content, backgroundDrawingType, rect1);
  getPaintController().beginSkippingCache();
  drawRect(context, content, foregroundDrawingType, rect2);
  getPaintController().endSkippingCache();
  drawRect(context, content, foregroundDrawingType, rect3);

  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
                      TestDisplayItem(content, backgroundDrawingType),
                      TestDisplayItem(content, foregroundDrawingType),
                      TestDisplayItem(content, foregroundDrawingType));
  sk_sp<const SkPicture> picture0 =
      sk_ref_sp(static_cast<const DrawingDisplayItem&>(
                    getPaintController().getDisplayItemList()[0])
                    .picture());
  sk_sp<const SkPicture> picture1 =
      sk_ref_sp(static_cast<const DrawingDisplayItem&>(
                    getPaintController().getDisplayItemList()[1])
                    .picture());
  sk_sp<const SkPicture> picture2 =
      sk_ref_sp(static_cast<const DrawingDisplayItem&>(
                    getPaintController().getDisplayItemList()[2])
                    .picture());
  EXPECT_NE(picture1, picture2);

  // Content's cache is invalid because it has display items skipped cache.
  EXPECT_FALSE(getPaintController().clientCacheIsValid(content));
  EXPECT_EQ(PaintInvalidationFull, content.getPaintInvalidationReason());

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        &m_rootPaintChunkId, rootPaintChunkProperties());
  }
  // Draw again with nothing invalidated.
  drawRect(context, content, backgroundDrawingType, rect1);
  getPaintController().beginSkippingCache();
  drawRect(context, content, foregroundDrawingType, rect2);
  getPaintController().endSkippingCache();
  drawRect(context, content, foregroundDrawingType, rect3);

  EXPECT_EQ(0, numCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(0, numSequentialMatches());
  EXPECT_EQ(0, numOutOfOrderMatches());
  EXPECT_EQ(0, numIndexedItems());
#endif

  getPaintController().commitNewDisplayItems();

  EXPECT_DISPLAY_LIST(getPaintController().getDisplayItemList(), 3,
                      TestDisplayItem(content, backgroundDrawingType),
                      TestDisplayItem(content, foregroundDrawingType),
                      TestDisplayItem(content, foregroundDrawingType));
  EXPECT_NE(picture0.get(), static_cast<const DrawingDisplayItem&>(
                                getPaintController().getDisplayItemList()[0])
                                .picture());
  EXPECT_NE(picture1.get(), static_cast<const DrawingDisplayItem&>(
                                getPaintController().getDisplayItemList()[1])
                                .picture());
  EXPECT_NE(picture2.get(), static_cast<const DrawingDisplayItem&>(
                                getPaintController().getDisplayItemList()[2])
                                .picture());
}

TEST_F(PaintControllerTestBase, OptimizeNoopPairs) {
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
                      TestDisplayItem(second, DisplayItem::kBeginClipPath),
                      TestDisplayItem(second, backgroundDrawingType),
                      TestDisplayItem(second, DisplayItem::kEndClipPath),
                      TestDisplayItem(third, backgroundDrawingType));

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

TEST_F(PaintControllerTestBase, SmallPaintControllerHasOnePaintChunk) {
  RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);
  FakeDisplayItemClient client("test client");

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    getPaintController().updateCurrentPaintChunkProperties(
        nullptr, rootPaintChunkProperties());
  }
  GraphicsContext context(getPaintController());
  drawRect(context, client, backgroundDrawingType, FloatRect(0, 0, 100, 100));

  getPaintController().commitNewDisplayItems();
  const auto& paintChunks = getPaintController().paintChunks();
  ASSERT_EQ(1u, paintChunks.size());
  EXPECT_EQ(0u, paintChunks[0].beginIndex);
  EXPECT_EQ(1u, paintChunks[0].endIndex);
}

TEST_F(PaintControllerTestBase, PaintArtifactWithVisualRects) {
  FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));

  GraphicsContext context(getPaintController());
  drawRect(context, client, backgroundDrawingType, FloatRect(0, 0, 100, 100));

  getPaintController().commitNewDisplayItems(LayoutSize(20, 30));
  const auto& paintArtifact = getPaintController().paintArtifact();
  ASSERT_EQ(1u, paintArtifact.getDisplayItemList().size());
  EXPECT_EQ(IntRect(-20, -30, 200, 100), visualRect(paintArtifact, 0));
}

void drawPath(GraphicsContext& context,
              DisplayItemClient& client,
              DisplayItem::Type type,
              unsigned count) {
  if (DrawingRecorder::useCachedDrawingIfPossible(context, client, type))
    return;

  DrawingRecorder drawingRecorder(context, client, type,
                                  FloatRect(0, 0, 100, 100));
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

TEST_F(PaintControllerTestBase, IsSuitableForGpuRasterizationSinglePath) {
  FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));
  GraphicsContext context(getPaintController());
  drawPath(context, client, backgroundDrawingType, 1);
  getPaintController().commitNewDisplayItems(LayoutSize());
  EXPECT_TRUE(
      getPaintController().paintArtifact().isSuitableForGpuRasterization());
}

TEST_F(PaintControllerTestBase,
       IsNotSuitableForGpuRasterizationSinglePictureManyPaths) {
  FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));
  GraphicsContext context(getPaintController());

  drawPath(context, client, backgroundDrawingType, 50);
  getPaintController().commitNewDisplayItems(LayoutSize());
  EXPECT_FALSE(
      getPaintController().paintArtifact().isSuitableForGpuRasterization());
}

TEST_F(PaintControllerTestBase,
       IsNotSuitableForGpuRasterizationMultiplePicturesSinglePathEach) {
  FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));
  GraphicsContext context(getPaintController());
  getPaintController().beginSkippingCache();

  for (int i = 0; i < 50; ++i)
    drawPath(context, client, backgroundDrawingType, 50);

  getPaintController().endSkippingCache();
  getPaintController().commitNewDisplayItems(LayoutSize());
  EXPECT_FALSE(
      getPaintController().paintArtifact().isSuitableForGpuRasterization());
}

TEST_F(PaintControllerTestBase,
       IsNotSuitableForGpuRasterizationSinglePictureManyPathsTwoPaints) {
  FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));

  {
    GraphicsContext context(getPaintController());
    drawPath(context, client, backgroundDrawingType, 50);
    getPaintController().commitNewDisplayItems(LayoutSize());
    EXPECT_FALSE(
        getPaintController().paintArtifact().isSuitableForGpuRasterization());
  }

  client.setDisplayItemsUncached();

  {
    GraphicsContext context(getPaintController());
    drawPath(context, client, backgroundDrawingType, 50);
    getPaintController().commitNewDisplayItems(LayoutSize());
    EXPECT_FALSE(
        getPaintController().paintArtifact().isSuitableForGpuRasterization());
  }
}

TEST_F(PaintControllerTestBase,
       IsNotSuitableForGpuRasterizationSinglePictureManyPathsCached) {
  FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));

  {
    GraphicsContext context(getPaintController());
    drawPath(context, client, backgroundDrawingType, 50);
    getPaintController().commitNewDisplayItems(LayoutSize());
    EXPECT_FALSE(
        getPaintController().paintArtifact().isSuitableForGpuRasterization());
  }

  {
    GraphicsContext context(getPaintController());
    drawPath(context, client, backgroundDrawingType, 50);
    getPaintController().commitNewDisplayItems(LayoutSize());
    EXPECT_FALSE(
        getPaintController().paintArtifact().isSuitableForGpuRasterization());
  }
}

TEST_F(
    PaintControllerTestBase,
    IsNotSuitableForGpuRasterizationSinglePictureManyPathsCachedSubsequence) {
  FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));
  FakeDisplayItemClient container("container", LayoutRect(0, 0, 200, 100));

  GraphicsContext context(getPaintController());
  {
    SubsequenceRecorder subsequenceRecorder(context, container);
    drawPath(context, client, backgroundDrawingType, 50);
  }
  getPaintController().commitNewDisplayItems(LayoutSize());
  EXPECT_FALSE(
      getPaintController().paintArtifact().isSuitableForGpuRasterization());

  EXPECT_TRUE(
      SubsequenceRecorder::useCachedSubsequenceIfPossible(context, container));
  getPaintController().commitNewDisplayItems(LayoutSize());
  EXPECT_FALSE(
      getPaintController().paintArtifact().isSuitableForGpuRasterization());

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
  DisplayItemClient::endShouldKeepAliveAllClients();
#endif
}

// Temporarily disabled (pref regressions due to GPU veto stickiness:
// http://crbug.com/603969).
TEST_F(PaintControllerTestBase,
       DISABLED_IsNotSuitableForGpuRasterizationConcaveClipPath) {
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
      getPaintController().createAndAppend<BeginClipPathDisplayItem>(client,
                                                                     path);
    drawRect(context, client, backgroundDrawingType, FloatRect(0, 0, 100, 100));
    for (int j = 0; j < 50; ++j)
      getPaintController().createAndAppend<EndClipPathDisplayItem>(client);
    getPaintController().commitNewDisplayItems(LayoutSize());
    EXPECT_FALSE(
        getPaintController().paintArtifact().isSuitableForGpuRasterization());
  }
}

// Death tests don't work properly on Android.
#if defined(GTEST_HAS_DEATH_TEST) && !OS(ANDROID)

class PaintControllerUnderInvalidationTest : public PaintControllerTestBase {
 protected:
  void SetUp() override {
    PaintControllerTestBase::SetUp();
    RuntimeEnabledFeatures::setPaintUnderInvalidationCheckingEnabled(true);
  }

  void testChangeDrawing() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(getPaintController());

    drawRect(context, first, backgroundDrawingType,
             FloatRect(100, 100, 300, 300));
    drawRect(context, first, foregroundDrawingType,
             FloatRect(100, 100, 300, 300));
    getPaintController().commitNewDisplayItems();
    drawRect(context, first, backgroundDrawingType,
             FloatRect(200, 200, 300, 300));
    drawRect(context, first, foregroundDrawingType,
             FloatRect(100, 100, 300, 300));
    getPaintController().commitNewDisplayItems();
  }

  void testMoreDrawing() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(getPaintController());

    drawRect(context, first, backgroundDrawingType,
             FloatRect(100, 100, 300, 300));
    getPaintController().commitNewDisplayItems();
    drawRect(context, first, backgroundDrawingType,
             FloatRect(100, 100, 300, 300));
    drawRect(context, first, foregroundDrawingType,
             FloatRect(100, 100, 300, 300));
    getPaintController().commitNewDisplayItems();
  }

  void testLessDrawing() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(getPaintController());

    drawRect(context, first, backgroundDrawingType,
             FloatRect(100, 100, 300, 300));
    drawRect(context, first, foregroundDrawingType,
             FloatRect(100, 100, 300, 300));
    getPaintController().commitNewDisplayItems();
    drawRect(context, first, backgroundDrawingType,
             FloatRect(100, 100, 300, 300));
    getPaintController().commitNewDisplayItems();
  }

  void testNoopPairsInSubsequence() {
    FakeDisplayItemClient container("container");
    GraphicsContext context(getPaintController());

    {
      SubsequenceRecorder r(context, container);
      drawRect(context, container, backgroundDrawingType,
               FloatRect(100, 100, 100, 100));
    }
    getPaintController().commitNewDisplayItems();

    EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(
        context, container));
    {
      // Generate some no-op pairs which should not affect under-invalidation
      // checking.
      ClipRecorder r1(context, container, clipType, IntRect(1, 1, 9, 9));
      ClipRecorder r2(context, container, clipType, IntRect(1, 1, 2, 2));
      ClipRecorder r3(context, container, clipType, IntRect(1, 1, 3, 3));
      ClipPathRecorder r4(context, container, Path());
    }
    {
      EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(
          context, container));
      SubsequenceRecorder r(context, container);
      drawRect(context, container, backgroundDrawingType,
               FloatRect(100, 100, 100, 100));
    }
    getPaintController().commitNewDisplayItems();

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    DisplayItemClient::endShouldKeepAliveAllClients();
#endif
  }

  void testChangeDrawingInSubsequence() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(getPaintController());
    {
      SubsequenceRecorder r(context, first);
      drawRect(context, first, backgroundDrawingType,
               FloatRect(100, 100, 300, 300));
      drawRect(context, first, foregroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    getPaintController().commitNewDisplayItems();
    {
      EXPECT_FALSE(
          SubsequenceRecorder::useCachedSubsequenceIfPossible(context, first));
      SubsequenceRecorder r(context, first);
      drawRect(context, first, backgroundDrawingType,
               FloatRect(200, 200, 300, 300));
      drawRect(context, first, foregroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    getPaintController().commitNewDisplayItems();
  }

  void testMoreDrawingInSubsequence() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(getPaintController());

    {
      SubsequenceRecorder r(context, first);
      drawRect(context, first, backgroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    getPaintController().commitNewDisplayItems();

    {
      EXPECT_FALSE(
          SubsequenceRecorder::useCachedSubsequenceIfPossible(context, first));
      SubsequenceRecorder r(context, first);
      drawRect(context, first, backgroundDrawingType,
               FloatRect(100, 100, 300, 300));
      drawRect(context, first, foregroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    getPaintController().commitNewDisplayItems();
  }

  void testLessDrawingInSubsequence() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(getPaintController());

    {
      SubsequenceRecorder r(context, first);
      drawRect(context, first, backgroundDrawingType,
               FloatRect(100, 100, 300, 300));
      drawRect(context, first, foregroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    getPaintController().commitNewDisplayItems();

    {
      EXPECT_FALSE(
          SubsequenceRecorder::useCachedSubsequenceIfPossible(context, first));
      SubsequenceRecorder r(context, first);
      drawRect(context, first, backgroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    getPaintController().commitNewDisplayItems();
  }

  void testChangeNonCacheableInSubsequence() {
    FakeDisplayItemClient container("container");
    FakeDisplayItemClient content("content");
    GraphicsContext context(getPaintController());

    {
      SubsequenceRecorder r(context, container);
      { ClipPathRecorder clipPathRecorder(context, container, Path()); }
      ClipRecorder clip(context, container, clipType, IntRect(1, 1, 9, 9));
      drawRect(context, content, backgroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    getPaintController().commitNewDisplayItems();

    {
      EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(
          context, container));
      SubsequenceRecorder r(context, container);
      { ClipPathRecorder clipPathRecorder(context, container, Path()); }
      ClipRecorder clip(context, container, clipType, IntRect(1, 1, 30, 30));
      drawRect(context, content, backgroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    getPaintController().commitNewDisplayItems();
  }

  void testInvalidationInSubsequence() {
    FakeDisplayItemClient container("container");
    FakeDisplayItemClient content("content");
    GraphicsContext context(getPaintController());

    {
      SubsequenceRecorder r(context, container);
      drawRect(context, content, backgroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    getPaintController().commitNewDisplayItems();

    content.setDisplayItemsUncached();
    // Leave container not invalidated.
    {
      EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(
          context, container));
      SubsequenceRecorder r(context, container);
      drawRect(context, content, backgroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    getPaintController().commitNewDisplayItems();

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    DisplayItemClient::endShouldKeepAliveAllClients();
#endif
  }

  void testFoldCompositingDrawingInSubsequence() {
    FakeDisplayItemClient container("container");
    FakeDisplayItemClient content("content");
    GraphicsContext context(getPaintController());

    {
      SubsequenceRecorder subsequence(context, container);
      CompositingRecorder compositing(context, content, SkBlendMode::kSrc, 0.5);
      drawRect(context, content, backgroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    getPaintController().commitNewDisplayItems();
    EXPECT_EQ(3u,
              getPaintController().paintArtifact().getDisplayItemList().size());

    {
      EXPECT_FALSE(SubsequenceRecorder::useCachedSubsequenceIfPossible(
          context, container));
      SubsequenceRecorder subsequence(context, container);
      CompositingRecorder compositing(context, content, SkBlendMode::kSrc, 0.5);
      drawRect(context, content, backgroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    getPaintController().commitNewDisplayItems();
    EXPECT_EQ(3u,
              getPaintController().paintArtifact().getDisplayItemList().size());

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    DisplayItemClient::endShouldKeepAliveAllClients();
#endif
  }
};

TEST_F(PaintControllerUnderInvalidationTest, ChangeDrawing) {
  EXPECT_DEATH(testChangeDrawing(), "under-invalidation: display item changed");
}

TEST_F(PaintControllerUnderInvalidationTest, MoreDrawing) {
  EXPECT_DEATH(testMoreDrawing(), "");
}

TEST_F(PaintControllerUnderInvalidationTest, LessDrawing) {
  // We don't detect under-invalidation in this case, and PaintController can
  // also handle the case gracefully. However, less drawing at one time often
  // means more-drawing at another time, so eventually we'll detect such
  // under-invalidations.
  testLessDrawing();
}

TEST_F(PaintControllerUnderInvalidationTest, NoopPairsInSubsequence) {
  // This should not die.
  testNoopPairsInSubsequence();
}

TEST_F(PaintControllerUnderInvalidationTest, ChangeDrawingInSubsequence) {
  EXPECT_DEATH(testChangeDrawingInSubsequence(),
               "\"\\(In cached subsequence of first\\)\" under-invalidation: "
               "display item changed");
}

TEST_F(PaintControllerUnderInvalidationTest, MoreDrawingInSubsequence) {
  EXPECT_DEATH(testMoreDrawingInSubsequence(),
               "\"\\(In cached subsequence of first\\)\" under-invalidation: "
               "display item changed");
}

TEST_F(PaintControllerUnderInvalidationTest, LessDrawingInSubsequence) {
  EXPECT_DEATH(testLessDrawingInSubsequence(),
               "\"\\(In cached subsequence of first\\)\" under-invalidation: "
               "display item changed");
}

TEST_F(PaintControllerUnderInvalidationTest, ChangeNonCacheableInSubsequence) {
  EXPECT_DEATH(testChangeNonCacheableInSubsequence(),
               "\"\\(In cached subsequence of container\\)\" "
               "under-invalidation: display item changed");
}

TEST_F(PaintControllerUnderInvalidationTest, InvalidationInSubsequence) {
  // We allow invalidated display item clients as long as they would produce the
  // same display items. The cases of changed display items are tested by other
  // test cases.
  testInvalidationInSubsequence();
}

TEST_F(PaintControllerUnderInvalidationTest,
       FoldCompositingDrawingInSubsequence) {
  testFoldCompositingDrawingInSubsequence();
}

#endif  // defined(GTEST_HAS_DEATH_TEST) && !OS(ANDROID)

}  // namespace blink
