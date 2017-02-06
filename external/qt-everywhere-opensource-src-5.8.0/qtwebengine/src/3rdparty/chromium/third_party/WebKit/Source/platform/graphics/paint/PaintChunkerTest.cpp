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

static PaintChunkProperties rootPaintChunkProperties() { return PaintChunkProperties(); }

class PaintChunkerTest : public testing::Test {
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

TEST_F(PaintChunkerTest, Empty)
{
    Vector<PaintChunk> chunks = PaintChunker().releasePaintChunks();
    ASSERT_TRUE(chunks.isEmpty());
}

TEST_F(PaintChunkerTest, SingleNonEmptyRange)
{
    PaintChunker chunker;
    chunker.updateCurrentPaintChunkProperties(rootPaintChunkProperties());
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);
    Vector<PaintChunk> chunks = chunker.releasePaintChunks();

    EXPECT_THAT(chunks, ElementsAre(
        PaintChunk(0, 2, rootPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, SamePropertiesTwiceCombineIntoOneChunk)
{
    PaintChunker chunker;
    chunker.updateCurrentPaintChunkProperties(rootPaintChunkProperties());
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);
    chunker.updateCurrentPaintChunkProperties(rootPaintChunkProperties());
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);
    Vector<PaintChunk> chunks = chunker.releasePaintChunks();

    EXPECT_THAT(chunks, ElementsAre(
        PaintChunk(0, 3, rootPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, CanRewindDisplayItemIndex)
{
    PaintChunker chunker;
    chunker.updateCurrentPaintChunkProperties(rootPaintChunkProperties());
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);
    chunker.decrementDisplayItemIndex();
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);
    Vector<PaintChunk> chunks = chunker.releasePaintChunks();

    EXPECT_THAT(chunks, ElementsAre(
        PaintChunk(0, 2, rootPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, BuildMultipleChunksWithSinglePropertyChanging)
{
    PaintChunker chunker;
    chunker.updateCurrentPaintChunkProperties(rootPaintChunkProperties());
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);

    PaintChunkProperties simpleTransform;
    simpleTransform.transform = TransformPaintPropertyNode::create(TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));

    chunker.updateCurrentPaintChunkProperties(simpleTransform);
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);

    PaintChunkProperties anotherTransform;
    anotherTransform.transform = TransformPaintPropertyNode::create(TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));
    chunker.updateCurrentPaintChunkProperties(anotherTransform);
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);

    Vector<PaintChunk> chunks = chunker.releasePaintChunks();

    EXPECT_THAT(chunks, ElementsAre(
        PaintChunk(0, 2, rootPaintChunkProperties()),
        PaintChunk(2, 3, simpleTransform),
        PaintChunk(3, 4, anotherTransform)));
}

TEST_F(PaintChunkerTest, BuildMultipleChunksWithDifferentPropertyChanges)
{
    PaintChunker chunker;
    chunker.updateCurrentPaintChunkProperties(rootPaintChunkProperties());
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);

    PaintChunkProperties simpleTransform;
    simpleTransform.transform = TransformPaintPropertyNode::create(TransformationMatrix(0, 0, 0, 0, 0, 0), FloatPoint3D(9, 8, 7));
    chunker.updateCurrentPaintChunkProperties(simpleTransform);
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);

    PaintChunkProperties simpleTransformAndEffect;
    simpleTransformAndEffect.transform = simpleTransform.transform;
    simpleTransformAndEffect.effect = EffectPaintPropertyNode::create(0.5f);
    chunker.updateCurrentPaintChunkProperties(simpleTransformAndEffect);
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);

    PaintChunkProperties simpleTransformAndEffectWithUpdatedTransform;
    simpleTransformAndEffectWithUpdatedTransform.transform = TransformPaintPropertyNode::create(TransformationMatrix(1, 1, 0, 0, 0, 0), FloatPoint3D(9, 8, 7));
    simpleTransformAndEffectWithUpdatedTransform.effect = EffectPaintPropertyNode::create(simpleTransformAndEffect.effect->opacity());
    chunker.updateCurrentPaintChunkProperties(simpleTransformAndEffectWithUpdatedTransform);
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);

    // Test that going back to a previous chunk property still creates a new chunk.
    chunker.updateCurrentPaintChunkProperties(simpleTransformAndEffect);
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);

    Vector<PaintChunk> chunks = chunker.releasePaintChunks();

    EXPECT_THAT(chunks, ElementsAre(
        PaintChunk(0, 1, rootPaintChunkProperties()),
        PaintChunk(1, 3, simpleTransform),
        PaintChunk(3, 5, simpleTransformAndEffect),
        PaintChunk(5, 7, simpleTransformAndEffectWithUpdatedTransform),
        PaintChunk(7, 9, simpleTransformAndEffect)));
}

TEST_F(PaintChunkerTest, BuildChunksFromNestedTransforms)
{
    // Test that "nested" transforms linearize using the following
    // sequence of transforms and display items:
    // <root xform>, <paint>, <a xform>, <paint>, <paint>, </a xform>, <paint>, </root xform>
    PaintChunker chunker;
    chunker.updateCurrentPaintChunkProperties(rootPaintChunkProperties());
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);

    PaintChunkProperties simpleTransform;
    simpleTransform.transform = TransformPaintPropertyNode::create(TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));
    chunker.updateCurrentPaintChunkProperties(simpleTransform);
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);

    chunker.updateCurrentPaintChunkProperties(rootPaintChunkProperties());
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);

    Vector<PaintChunk> chunks = chunker.releasePaintChunks();

    EXPECT_THAT(chunks, ElementsAre(
        PaintChunk(0, 1, rootPaintChunkProperties()),
        PaintChunk(1, 3, simpleTransform),
        PaintChunk(3, 4, rootPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ChangingPropertiesWithoutItems)
{
    // Test that properties can change without display items being generated.
    PaintChunker chunker;
    chunker.updateCurrentPaintChunkProperties(rootPaintChunkProperties());
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);

    PaintChunkProperties firstTransform;
    firstTransform.transform = TransformPaintPropertyNode::create(TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));
    chunker.updateCurrentPaintChunkProperties(firstTransform);

    PaintChunkProperties secondTransform;
    secondTransform.transform = TransformPaintPropertyNode::create(TransformationMatrix(9, 8, 7, 6, 5, 4), FloatPoint3D(3, 2, 1));
    chunker.updateCurrentPaintChunkProperties(secondTransform);

    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);
    Vector<PaintChunk> chunks = chunker.releasePaintChunks();

    EXPECT_THAT(chunks, ElementsAre(
        PaintChunk(0, 1, rootPaintChunkProperties()),
        PaintChunk(1, 2, secondTransform)));
}

TEST_F(PaintChunkerTest, CreatesSeparateChunksWhenRequested)
{
    // Tests that the chunker creates a separate chunks for display items which
    // require it.
    PaintChunker chunker;
    chunker.updateCurrentPaintChunkProperties(rootPaintChunkProperties());
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);
    chunker.incrementDisplayItemIndex(PaintChunker::RequiresSeparateChunk);
    chunker.incrementDisplayItemIndex(PaintChunker::RequiresSeparateChunk);
    chunker.incrementDisplayItemIndex(PaintChunker::RequiresSeparateChunk);
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);
    chunker.incrementDisplayItemIndex(PaintChunker::RequiresSeparateChunk);
    chunker.incrementDisplayItemIndex(PaintChunker::RequiresSeparateChunk);
    chunker.decrementDisplayItemIndex();
    chunker.decrementDisplayItemIndex();
    chunker.decrementDisplayItemIndex();
    chunker.incrementDisplayItemIndex(PaintChunker::DefaultBehavior);
    chunker.incrementDisplayItemIndex(PaintChunker::RequiresSeparateChunk);

    Vector<PaintChunk> chunks = chunker.releasePaintChunks();
    EXPECT_THAT(chunks, ElementsAre(
        PaintChunk(0, 1, rootPaintChunkProperties()),
        PaintChunk(1, 2, rootPaintChunkProperties()),
        PaintChunk(2, 3, rootPaintChunkProperties()),
        PaintChunk(3, 4, rootPaintChunkProperties()),
        PaintChunk(4, 6, rootPaintChunkProperties()),
        PaintChunk(6, 7, rootPaintChunkProperties())));
}

} // namespace
} // namespace blink
