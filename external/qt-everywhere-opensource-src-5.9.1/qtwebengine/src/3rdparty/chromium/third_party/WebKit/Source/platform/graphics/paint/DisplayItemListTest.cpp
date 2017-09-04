// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DisplayItemList.h"

#include "SkPictureRecorder.h"
#include "SkTypes.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/SubsequenceDisplayItem.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/testing/FakeDisplayItemClient.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

#define EXPECT_RECT_EQ(expected, actual)               \
  do {                                                 \
    const IntRect& actualRect = actual;                \
    EXPECT_EQ(expected.x(), actualRect.x());           \
    EXPECT_EQ(expected.y(), actualRect.y());           \
    EXPECT_EQ(expected.width(), actualRect.width());   \
    EXPECT_EQ(expected.height(), actualRect.height()); \
  } while (false)

static const size_t kDefaultListBytes = 10 * 1024;

class DisplayItemListTest : public ::testing::Test {
 public:
  DisplayItemListTest() : m_list(kDefaultListBytes) {}

  DisplayItemList m_list;
  FakeDisplayItemClient m_client;
};

static sk_sp<SkPicture> createRectPicture(const IntRect& bounds) {
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(bounds.width(), bounds.height());
  canvas->drawRect(
      SkRect::MakeXYWH(bounds.x(), bounds.y(), bounds.width(), bounds.height()),
      SkPaint());
  return recorder.finishRecordingAsPicture();
}

TEST_F(DisplayItemListTest, AppendVisualRect_Simple) {
  IntRect drawingBounds(5, 6, 7, 8);
  m_list.allocateAndConstruct<DrawingDisplayItem>(
      m_client, DisplayItem::Type::kDocumentBackground,
      createRectPicture(drawingBounds), true);
  m_list.appendVisualRect(drawingBounds);

  EXPECT_EQ(static_cast<size_t>(1), m_list.size());
  EXPECT_RECT_EQ(drawingBounds, m_list.visualRect(0));
}

TEST_F(DisplayItemListTest, AppendVisualRect_BlockContainingDrawing) {
  // TODO(wkorman): Note the visual rects for the paired begin/end are now
  // irrelevant as they're overwritten in cc::DisplayItemList when rebuilt to
  // represent the union of all drawing display item visual rects between the
  // pair. We should consider revising Blink's display item list in some form
  // so as to only store visual rects for drawing display items.

  IntRect subsequenceBounds(5, 6, 7, 8);
  m_list.allocateAndConstruct<BeginSubsequenceDisplayItem>(m_client);
  m_list.appendVisualRect(subsequenceBounds);

  IntRect drawingBounds(5, 6, 1, 1);
  m_list.allocateAndConstruct<DrawingDisplayItem>(
      m_client, DisplayItem::Type::kDocumentBackground,
      createRectPicture(drawingBounds), true);
  m_list.appendVisualRect(drawingBounds);

  m_list.allocateAndConstruct<EndSubsequenceDisplayItem>(m_client);
  m_list.appendVisualRect(subsequenceBounds);

  EXPECT_EQ(static_cast<size_t>(3), m_list.size());
  EXPECT_RECT_EQ(subsequenceBounds, m_list.visualRect(0));
  EXPECT_RECT_EQ(drawingBounds, m_list.visualRect(1));
  EXPECT_RECT_EQ(subsequenceBounds, m_list.visualRect(2));
}
}  // namespace
}  // namespace blink
