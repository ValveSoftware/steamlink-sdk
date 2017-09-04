// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintChunker.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace blink {
namespace {

// TODO(crbug.com/629946): The tests fail mysteriously on some Windows debug
// bots.
#if defined(NDEBUG) || !OS(WIN)

PaintChunkProperties rootPaintChunkProperties() {
  PaintChunkProperties rootProperties;
  rootProperties.transform = TransformPaintPropertyNode::root();
  rootProperties.clip = ClipPaintPropertyNode::root();
  rootProperties.effect = EffectPaintPropertyNode::root();
  rootProperties.scroll = ScrollPaintPropertyNode::root();
  return rootProperties;
}

class PaintChunkerTest : public testing::Test {
 protected:
  void SetUp() override {
    RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);
  }

  void TearDown() override { m_featuresBackup.restore(); }

 private:
  RuntimeEnabledFeatures::Backup m_featuresBackup;
};

class TestDisplayItem : public DisplayItem, public DisplayItemClient {
 public:
  TestDisplayItem(DisplayItem::Type type)
      : DisplayItem(*this, type, sizeof(*this)) {}

  void replay(GraphicsContext&) const final { NOTREACHED(); }
  void appendToWebDisplayItemList(const IntRect&,
                                  WebDisplayItemList*) const final {
    NOTREACHED();
  }
  String debugName() const final { return "Test"; }
  LayoutRect visualRect() const final { return LayoutRect(); }
};

class NormalTestDisplayItem : public TestDisplayItem {
 public:
  NormalTestDisplayItem() : TestDisplayItem(DisplayItem::kDrawingFirst) {}
};

class TestDisplayItemRequiringSeparateChunk : public TestDisplayItem {
 public:
  TestDisplayItemRequiringSeparateChunk()
      : TestDisplayItem(DisplayItem::kForeignLayerPlugin) {}
};

TEST_F(PaintChunkerTest, Empty) {
  Vector<PaintChunk> chunks = PaintChunker().releasePaintChunks();
  ASSERT_TRUE(chunks.isEmpty());
}

TEST_F(PaintChunkerTest, SingleNonEmptyRange) {
  PaintChunker chunker;
  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            rootPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  Vector<PaintChunk> chunks = chunker.releasePaintChunks();

  EXPECT_THAT(chunks, ElementsAre(PaintChunk(0, 2, nullptr,
                                             rootPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, SamePropertiesTwiceCombineIntoOneChunk) {
  PaintChunker chunker;
  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            rootPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            rootPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  Vector<PaintChunk> chunks = chunker.releasePaintChunks();

  EXPECT_THAT(chunks, ElementsAre(PaintChunk(0, 3, nullptr,
                                             rootPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, CanRewindDisplayItemIndex) {
  PaintChunker chunker;
  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            rootPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  chunker.decrementDisplayItemIndex();
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  Vector<PaintChunk> chunks = chunker.releasePaintChunks();

  EXPECT_THAT(chunks, ElementsAre(PaintChunk(0, 2, nullptr,
                                             rootPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, BuildMultipleChunksWithSinglePropertyChanging) {
  PaintChunker chunker;
  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            rootPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());

  PaintChunkProperties simpleTransform = rootPaintChunkProperties();
  simpleTransform.transform = TransformPaintPropertyNode::create(
      nullptr, TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));

  chunker.updateCurrentPaintChunkProperties(nullptr, simpleTransform);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());

  PaintChunkProperties anotherTransform = rootPaintChunkProperties();
  anotherTransform.transform = TransformPaintPropertyNode::create(
      nullptr, TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));
  chunker.updateCurrentPaintChunkProperties(nullptr, anotherTransform);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());

  Vector<PaintChunk> chunks = chunker.releasePaintChunks();

  EXPECT_THAT(chunks,
              ElementsAre(PaintChunk(0, 2, nullptr, rootPaintChunkProperties()),
                          PaintChunk(2, 3, nullptr, simpleTransform),
                          PaintChunk(3, 4, nullptr, anotherTransform)));
}

TEST_F(PaintChunkerTest, BuildMultipleChunksWithDifferentPropertyChanges) {
  PaintChunker chunker;
  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            rootPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());

  PaintChunkProperties simpleTransform = rootPaintChunkProperties();
  simpleTransform.transform = TransformPaintPropertyNode::create(
      nullptr, TransformationMatrix(0, 0, 0, 0, 0, 0), FloatPoint3D(9, 8, 7));
  chunker.updateCurrentPaintChunkProperties(nullptr, simpleTransform);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());

  PaintChunkProperties simpleTransformAndEffect = rootPaintChunkProperties();
  simpleTransformAndEffect.transform = simpleTransform.transform;
  simpleTransformAndEffect.effect = EffectPaintPropertyNode::create(
      EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      ClipPaintPropertyNode::root(), CompositorFilterOperations(), 0.5f);
  chunker.updateCurrentPaintChunkProperties(nullptr, simpleTransformAndEffect);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());

  PaintChunkProperties simpleTransformAndEffectWithUpdatedTransform =
      rootPaintChunkProperties();
  simpleTransformAndEffectWithUpdatedTransform.transform =
      TransformPaintPropertyNode::create(nullptr,
                                         TransformationMatrix(1, 1, 0, 0, 0, 0),
                                         FloatPoint3D(9, 8, 7));
  simpleTransformAndEffectWithUpdatedTransform.effect =
      EffectPaintPropertyNode::create(
          EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
          ClipPaintPropertyNode::root(), CompositorFilterOperations(),
          simpleTransformAndEffect.effect->opacity());
  chunker.updateCurrentPaintChunkProperties(
      nullptr, simpleTransformAndEffectWithUpdatedTransform);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());

  // Test that going back to a previous chunk property still creates a new
  // chunk.
  chunker.updateCurrentPaintChunkProperties(nullptr, simpleTransformAndEffect);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());

  Vector<PaintChunk> chunks = chunker.releasePaintChunks();

  EXPECT_THAT(
      chunks,
      ElementsAre(PaintChunk(0, 1, nullptr, rootPaintChunkProperties()),
                  PaintChunk(1, 3, nullptr, simpleTransform),
                  PaintChunk(3, 5, nullptr, simpleTransformAndEffect),
                  PaintChunk(5, 7, nullptr,
                             simpleTransformAndEffectWithUpdatedTransform),
                  PaintChunk(7, 9, nullptr, simpleTransformAndEffect)));
}

TEST_F(PaintChunkerTest, BuildChunksFromNestedTransforms) {
  // Test that "nested" transforms linearize using the following
  // sequence of transforms and display items:
  // <root xform>, <paint>, <a xform>, <paint>, <paint>, </a xform>, <paint>,
  // </root xform>
  PaintChunker chunker;
  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            rootPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());

  PaintChunkProperties simpleTransform = rootPaintChunkProperties();
  simpleTransform.transform = TransformPaintPropertyNode::create(
      nullptr, TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));
  chunker.updateCurrentPaintChunkProperties(nullptr, simpleTransform);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());

  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            rootPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());

  Vector<PaintChunk> chunks = chunker.releasePaintChunks();

  EXPECT_THAT(
      chunks,
      ElementsAre(PaintChunk(0, 1, nullptr, rootPaintChunkProperties()),
                  PaintChunk(1, 3, nullptr, simpleTransform),
                  PaintChunk(3, 4, nullptr, rootPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ChangingPropertiesWithoutItems) {
  // Test that properties can change without display items being generated.
  PaintChunker chunker;
  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            rootPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());

  PaintChunkProperties firstTransform = rootPaintChunkProperties();
  firstTransform.transform = TransformPaintPropertyNode::create(
      nullptr, TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));
  chunker.updateCurrentPaintChunkProperties(nullptr, firstTransform);

  PaintChunkProperties secondTransform = rootPaintChunkProperties();
  secondTransform.transform = TransformPaintPropertyNode::create(
      nullptr, TransformationMatrix(9, 8, 7, 6, 5, 4), FloatPoint3D(3, 2, 1));
  chunker.updateCurrentPaintChunkProperties(nullptr, secondTransform);

  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  Vector<PaintChunk> chunks = chunker.releasePaintChunks();

  EXPECT_THAT(chunks,
              ElementsAre(PaintChunk(0, 1, nullptr, rootPaintChunkProperties()),
                          PaintChunk(1, 2, nullptr, secondTransform)));
}

TEST_F(PaintChunkerTest, CreatesSeparateChunksWhenRequested) {
  // Tests that the chunker creates a separate chunks for display items which
  // require it.
  PaintChunker chunker;
  TestDisplayItemRequiringSeparateChunk i1, i2, i3, i4, i5, i6;
  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            rootPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  chunker.incrementDisplayItemIndex(i1);
  chunker.incrementDisplayItemIndex(i2);
  chunker.incrementDisplayItemIndex(i3);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  chunker.incrementDisplayItemIndex(i4);
  chunker.incrementDisplayItemIndex(i5);
  chunker.decrementDisplayItemIndex();
  chunker.decrementDisplayItemIndex();
  chunker.decrementDisplayItemIndex();
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  chunker.incrementDisplayItemIndex(i6);

  DisplayItem::Id id1 = i1.getId();
  DisplayItem::Id id2 = i2.getId();
  DisplayItem::Id id3 = i3.getId();
  DisplayItem::Id id6 = i6.getId();
  Vector<PaintChunk> chunks = chunker.releasePaintChunks();
  EXPECT_THAT(chunks,
              ElementsAre(PaintChunk(0, 1, nullptr, rootPaintChunkProperties()),
                          PaintChunk(1, 2, &id1, rootPaintChunkProperties()),
                          PaintChunk(2, 3, &id2, rootPaintChunkProperties()),
                          PaintChunk(3, 4, &id3, rootPaintChunkProperties()),
                          PaintChunk(4, 6, nullptr, rootPaintChunkProperties()),
                          PaintChunk(6, 7, &id6, rootPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ChunkIds) {
  PaintChunker chunker;
  TestDisplayItem i1(DisplayItem::kDrawingFirst);
  DisplayItem::Id id1 = i1.getId();
  TestDisplayItemRequiringSeparateChunk i2;
  DisplayItem::Id id2 = i2.getId();

  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            rootPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());

  PaintChunkProperties simpleTransform = rootPaintChunkProperties();
  simpleTransform.transform = TransformPaintPropertyNode::create(
      nullptr, TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));
  chunker.updateCurrentPaintChunkProperties(&id1, simpleTransform);

  chunker.incrementDisplayItemIndex(i1);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  chunker.incrementDisplayItemIndex(i2);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());

  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            rootPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());

  Vector<PaintChunk> chunks = chunker.releasePaintChunks();
  EXPECT_THAT(
      chunks,
      ElementsAre(PaintChunk(0, 2, nullptr, rootPaintChunkProperties()),
                  PaintChunk(2, 4, &id1, simpleTransform),
                  PaintChunk(4, 5, &id2, simpleTransform),
                  PaintChunk(5, 6, nullptr, simpleTransform),
                  PaintChunk(6, 7, nullptr, rootPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ChunkIdsSkippingCache) {
  PaintChunker chunker;
  TestDisplayItem i1(DisplayItem::kDrawingFirst);
  i1.setSkippedCache();
  DisplayItem::Id id1 = i1.getId();
  TestDisplayItemRequiringSeparateChunk i2;
  i2.setSkippedCache();

  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            rootPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());

  PaintChunkProperties simpleTransform = rootPaintChunkProperties();
  simpleTransform.transform = TransformPaintPropertyNode::create(
      nullptr, TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));
  chunker.updateCurrentPaintChunkProperties(&id1, simpleTransform);

  chunker.incrementDisplayItemIndex(i1);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());
  chunker.incrementDisplayItemIndex(i2);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());

  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            rootPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem());

  Vector<PaintChunk> chunks = chunker.releasePaintChunks();
  EXPECT_THAT(
      chunks,
      ElementsAre(PaintChunk(0, 2, nullptr, rootPaintChunkProperties()),
                  PaintChunk(2, 4, nullptr, simpleTransform),
                  PaintChunk(4, 5, nullptr, simpleTransform),
                  PaintChunk(5, 6, nullptr, simpleTransform),
                  PaintChunk(6, 7, nullptr, rootPaintChunkProperties())));
}

#endif

}  // namespace
}  // namespace blink
