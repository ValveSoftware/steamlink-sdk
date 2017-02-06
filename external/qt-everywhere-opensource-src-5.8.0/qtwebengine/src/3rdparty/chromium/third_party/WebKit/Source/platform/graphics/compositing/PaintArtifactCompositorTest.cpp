// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/PaintArtifactCompositor.h"

#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/layers/layer.h"
#include "cc/test/fake_output_surface.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/testing/PictureMatchers.h"
#include "platform/testing/TestPaintArtifact.h"
#include "platform/testing/WebLayerTreeViewImplForTesting.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {
namespace {

using ::testing::Pointee;

gfx::Transform translation(SkMScalar x, SkMScalar y)
{
    gfx::Transform transform;
    transform.Translate(x, y);
    return transform;
}

class PaintArtifactCompositorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);

        // Delay constructing the compositor until after the feature is set.
        m_paintArtifactCompositor = wrapUnique(new PaintArtifactCompositor);
        m_paintArtifactCompositor->enableExtraDataForTesting();
    }

    void TearDown() override
    {
        m_featuresBackup.restore();
    }

    PaintArtifactCompositor& getPaintArtifactCompositor() { return *m_paintArtifactCompositor; }
    cc::Layer* rootLayer() { return m_paintArtifactCompositor->rootLayer(); }
    void update(const PaintArtifact& artifact) { m_paintArtifactCompositor->update(artifact); }

    size_t contentLayerCount()
    {
        return m_paintArtifactCompositor->getExtraDataForTesting()->contentLayers.size();
    }

    cc::Layer* contentLayerAt(unsigned index)
    {
        return m_paintArtifactCompositor->getExtraDataForTesting()->contentLayers[index].get();
    }

private:
    RuntimeEnabledFeatures::Backup m_featuresBackup;
    std::unique_ptr<PaintArtifactCompositor> m_paintArtifactCompositor;
};

TEST_F(PaintArtifactCompositorTest, EmptyPaintArtifact)
{
    PaintArtifact emptyArtifact;
    update(emptyArtifact);
    EXPECT_TRUE(rootLayer()->children().empty());
}

TEST_F(PaintArtifactCompositorTest, OneChunkWithAnOffset)
{
    TestPaintArtifact artifact;
    artifact.chunk(PaintChunkProperties())
        .rectDrawing(FloatRect(50, -50, 100, 100), Color::white);
    update(artifact.build());

    ASSERT_EQ(1u, rootLayer()->children().size());
    const cc::Layer* child = rootLayer()->child_at(0);
    EXPECT_THAT(child->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::white)));
    EXPECT_EQ(translation(50, -50), child->transform());
    EXPECT_EQ(gfx::Size(100, 100), child->bounds());
}

TEST_F(PaintArtifactCompositorTest, OneTransform)
{
    // A 90 degree clockwise rotation about (100, 100).
    RefPtr<TransformPaintPropertyNode> transform = TransformPaintPropertyNode::create(
        TransformationMatrix().rotate(90), FloatPoint3D(100, 100, 0));

    TestPaintArtifact artifact;
    artifact.chunk(transform, nullptr, nullptr)
        .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
    artifact.chunk(nullptr, nullptr, nullptr)
        .rectDrawing(FloatRect(0, 0, 100, 100), Color::gray);
    artifact.chunk(transform, nullptr, nullptr)
        .rectDrawing(FloatRect(100, 100, 200, 100), Color::black);
    update(artifact.build());

    ASSERT_EQ(3u, rootLayer()->children().size());
    {
        const cc::Layer* layer = rootLayer()->child_at(0);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::white)));
        gfx::RectF mappedRect(0, 0, 100, 100);
        layer->transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(100, 0, 100, 100), mappedRect);
    }
    {
        const cc::Layer* layer = rootLayer()->child_at(1);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::gray)));
        EXPECT_EQ(gfx::Transform(), layer->transform());
    }
    {
        const cc::Layer* layer = rootLayer()->child_at(2);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 200, 100), Color::black)));
        gfx::RectF mappedRect(0, 0, 200, 100);
        layer->transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(0, 100, 100, 200), mappedRect);
    }
}

TEST_F(PaintArtifactCompositorTest, TransformCombining)
{
    // A translation by (5, 5) within a 2x scale about (10, 10).
    RefPtr<TransformPaintPropertyNode> transform1 = TransformPaintPropertyNode::create(
        TransformationMatrix().scale(2), FloatPoint3D(10, 10, 0));
    RefPtr<TransformPaintPropertyNode> transform2 = TransformPaintPropertyNode::create(
        TransformationMatrix().translate(5, 5), FloatPoint3D(), transform1);

    TestPaintArtifact artifact;
    artifact.chunk(transform1, nullptr, nullptr)
        .rectDrawing(FloatRect(0, 0, 300, 200), Color::white);
    artifact.chunk(transform2, nullptr, nullptr)
        .rectDrawing(FloatRect(0, 0, 300, 200), Color::black);
    update(artifact.build());

    ASSERT_EQ(2u, rootLayer()->children().size());
    {
        const cc::Layer* layer = rootLayer()->child_at(0);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::white)));
        gfx::RectF mappedRect(0, 0, 300, 200);
        layer->transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(-10, -10, 600, 400), mappedRect);
    }
    {
        const cc::Layer* layer = rootLayer()->child_at(1);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::black)));
        gfx::RectF mappedRect(0, 0, 300, 200);
        layer->transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(0, 0, 600, 400), mappedRect);
    }
}

TEST_F(PaintArtifactCompositorTest, LayerOriginCancellation)
{
    RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
        nullptr, FloatRoundedRect(100, 100, 100, 100));
    RefPtr<TransformPaintPropertyNode> transform = TransformPaintPropertyNode::create(
        TransformationMatrix().scale(2), FloatPoint3D());

    TestPaintArtifact artifact;
    artifact.chunk(transform, clip, nullptr)
        .rectDrawing(FloatRect(12, 34, 56, 78), Color::white);
    update(artifact.build());

    ASSERT_EQ(1u, rootLayer()->children().size());
    cc::Layer* clipLayer = rootLayer()->child_at(0);
    EXPECT_EQ(gfx::Size(100, 100), clipLayer->bounds());
    EXPECT_EQ(translation(100, 100), clipLayer->transform());
    EXPECT_TRUE(clipLayer->masks_to_bounds());

    ASSERT_EQ(1u, clipLayer->children().size());
    cc::Layer* layer = clipLayer->child_at(0);
    EXPECT_EQ(gfx::Size(56, 78), layer->bounds());
    gfx::Transform expectedTransform;
    expectedTransform.Translate(-100, -100);
    expectedTransform.Scale(2, 2);
    expectedTransform.Translate(12, 34);
    EXPECT_EQ(expectedTransform, layer->transform());
}

TEST_F(PaintArtifactCompositorTest, OneClip)
{
    RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
        nullptr, FloatRoundedRect(100, 100, 300, 200));

    TestPaintArtifact artifact;
    artifact.chunk(nullptr, clip, nullptr)
        .rectDrawing(FloatRect(220, 80, 300, 200), Color::black);
    update(artifact.build());

    ASSERT_EQ(1u, rootLayer()->children().size());
    cc::Layer* clipLayer = rootLayer()->child_at(0);
    EXPECT_TRUE(clipLayer->masks_to_bounds());
    EXPECT_EQ(gfx::Size(300, 200), clipLayer->bounds());
    EXPECT_EQ(translation(100, 100), clipLayer->transform());

    ASSERT_EQ(1u, clipLayer->children().size());
    const cc::Layer* layer = clipLayer->child_at(0);
    EXPECT_THAT(layer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::black)));
    EXPECT_EQ(translation(120, -20), layer->transform());
}

TEST_F(PaintArtifactCompositorTest, NestedClips)
{
    RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::create(
        nullptr, FloatRoundedRect(100, 100, 700, 700));
    RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::create(
        nullptr, FloatRoundedRect(200, 200, 700, 100), clip1);

    TestPaintArtifact artifact;
    artifact.chunk(nullptr, clip1, nullptr)
        .rectDrawing(FloatRect(300, 350, 100, 100), Color::white);
    artifact.chunk(nullptr, clip2, nullptr)
        .rectDrawing(FloatRect(300, 350, 100, 100), Color::lightGray);
    artifact.chunk(nullptr, clip1, nullptr)
        .rectDrawing(FloatRect(300, 350, 100, 100), Color::darkGray);
    artifact.chunk(nullptr, clip2, nullptr)
        .rectDrawing(FloatRect(300, 350, 100, 100), Color::black);
    update(artifact.build());

    ASSERT_EQ(1u, rootLayer()->children().size());
    cc::Layer* clipLayer1 = rootLayer()->child_at(0);
    EXPECT_TRUE(clipLayer1->masks_to_bounds());
    EXPECT_EQ(gfx::Size(700, 700), clipLayer1->bounds());
    EXPECT_EQ(translation(100, 100), clipLayer1->transform());

    ASSERT_EQ(4u, clipLayer1->children().size());
    {
        const cc::Layer* whiteLayer = clipLayer1->child_at(0);
        EXPECT_THAT(whiteLayer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::white)));
        EXPECT_EQ(translation(200, 250), whiteLayer->transform());
    }
    {
        cc::Layer* lightGrayClip = clipLayer1->child_at(1);
        EXPECT_TRUE(lightGrayClip->masks_to_bounds());
        EXPECT_EQ(gfx::Size(700, 100), lightGrayClip->bounds());
        EXPECT_EQ(translation(100, 100), lightGrayClip->transform());
        ASSERT_EQ(1u, lightGrayClip->children().size());
        const cc::Layer* lightGrayLayer = lightGrayClip->child_at(0);
        EXPECT_THAT(lightGrayLayer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::lightGray)));
        EXPECT_EQ(translation(100, 150), lightGrayLayer->transform());
    }
    {
        const cc::Layer* darkGrayLayer = clipLayer1->child_at(2);
        EXPECT_THAT(darkGrayLayer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::darkGray)));
        EXPECT_EQ(translation(200, 250), darkGrayLayer->transform());
    }
    {
        cc::Layer* blackClip = clipLayer1->child_at(3);
        EXPECT_TRUE(blackClip->masks_to_bounds());
        EXPECT_EQ(gfx::Size(700, 100), blackClip->bounds());
        EXPECT_EQ(translation(100, 100), blackClip->transform());
        ASSERT_EQ(1u, blackClip->children().size());
        const cc::Layer* blackLayer = blackClip->child_at(0);
        EXPECT_THAT(blackLayer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::black)));
        EXPECT_EQ(translation(100, 150), blackLayer->transform());
    }
}

TEST_F(PaintArtifactCompositorTest, DeeplyNestedClips)
{
    Vector<RefPtr<ClipPaintPropertyNode>> clips;
    for (unsigned i = 1; i <= 10; i++) {
        clips.append(ClipPaintPropertyNode::create(
            nullptr, FloatRoundedRect(5 * i, 0, 100, 200 - 10 * i),
            clips.isEmpty() ? nullptr : clips.last()));
    }

    TestPaintArtifact artifact;
    artifact.chunk(nullptr, clips.last(), nullptr)
        .rectDrawing(FloatRect(0, 0, 200, 200), Color::white);
    update(artifact.build());

    // Check the clip layers.
    cc::Layer* layer = rootLayer();
    for (const auto& clipNode : clips) {
        ASSERT_EQ(1u, layer->children().size());
        layer = layer->child_at(0);
        EXPECT_EQ(clipNode->clipRect().rect().width(), layer->bounds().width());
        EXPECT_EQ(clipNode->clipRect().rect().height(), layer->bounds().height());
        EXPECT_EQ(translation(5, 0), layer->transform());
    }

    // Check the drawing layer.
    ASSERT_EQ(1u, layer->children().size());
    cc::Layer* drawingLayer = layer->child_at(0);
    EXPECT_THAT(drawingLayer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 200, 200), Color::white)));
    EXPECT_EQ(translation(-50, 0), drawingLayer->transform());
}

TEST_F(PaintArtifactCompositorTest, SiblingClips)
{
    RefPtr<ClipPaintPropertyNode> commonClip = ClipPaintPropertyNode::create(
        nullptr, FloatRoundedRect(0, 0, 800, 600));
    RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::create(
        nullptr, FloatRoundedRect(0, 0, 400, 600), commonClip);
    RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::create(
        nullptr, FloatRoundedRect(400, 0, 400, 600), commonClip);

    TestPaintArtifact artifact;
    artifact.chunk(nullptr, clip1, nullptr)
        .rectDrawing(FloatRect(0, 0, 640, 480), Color::white);
    artifact.chunk(nullptr, clip2, nullptr)
        .rectDrawing(FloatRect(0, 0, 640, 480), Color::black);
    update(artifact.build());

    ASSERT_EQ(1u, rootLayer()->children().size());
    cc::Layer* commonClipLayer = rootLayer()->child_at(0);
    EXPECT_TRUE(commonClipLayer->masks_to_bounds());
    EXPECT_EQ(gfx::Size(800, 600), commonClipLayer->bounds());
    EXPECT_EQ(gfx::Transform(), commonClipLayer->transform());
    ASSERT_EQ(2u, commonClipLayer->children().size());
    {
        cc::Layer* clipLayer1 = commonClipLayer->child_at(0);
        EXPECT_TRUE(clipLayer1->masks_to_bounds());
        EXPECT_EQ(gfx::Size(400, 600), clipLayer1->bounds());
        EXPECT_EQ(gfx::Transform(), clipLayer1->transform());
        ASSERT_EQ(1u, clipLayer1->children().size());
        cc::Layer* whiteLayer = clipLayer1->child_at(0);
        EXPECT_THAT(whiteLayer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 640, 480), Color::white)));
        EXPECT_EQ(gfx::Transform(), whiteLayer->transform());
    }
    {
        cc::Layer* clipLayer2 = commonClipLayer->child_at(1);
        EXPECT_TRUE(clipLayer2->masks_to_bounds());
        EXPECT_EQ(gfx::Size(400, 600), clipLayer2->bounds());
        EXPECT_EQ(translation(400, 0), clipLayer2->transform());
        ASSERT_EQ(1u, clipLayer2->children().size());
        cc::Layer* blackLayer = clipLayer2->child_at(0);
        EXPECT_THAT(blackLayer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 640, 480), Color::black)));
        EXPECT_EQ(translation(-400, 0), blackLayer->transform());
    }
}

TEST_F(PaintArtifactCompositorTest, ForeignLayerPassesThrough)
{
    scoped_refptr<cc::Layer> layer = cc::Layer::Create();

    TestPaintArtifact artifact;
    artifact.chunk(PaintChunkProperties())
        .foreignLayer(FloatPoint(50, 100), IntSize(400, 300), layer);
    update(artifact.build());

    ASSERT_EQ(1u, rootLayer()->children().size());
    EXPECT_EQ(layer, rootLayer()->child_at(0));
    EXPECT_EQ(gfx::Size(400, 300), layer->bounds());
    EXPECT_EQ(translation(50, 100), layer->transform());
}

// Similar to the above, but for the path where we build cc property trees
// directly. This will eventually supersede the above.

class WebLayerTreeViewWithOutputSurface : public WebLayerTreeViewImplForTesting {
public:
    WebLayerTreeViewWithOutputSurface(const cc::LayerTreeSettings& settings)
        : WebLayerTreeViewImplForTesting(settings) {}

    // cc::LayerTreeHostClient
    void RequestNewOutputSurface() override
    {
        layerTreeHost()->SetOutputSurface(cc::FakeOutputSurface::CreateDelegating3d());
    }
};

class PaintArtifactCompositorTestWithPropertyTrees : public PaintArtifactCompositorTest {
protected:
    PaintArtifactCompositorTestWithPropertyTrees()
        : m_taskRunner(new base::TestSimpleTaskRunner)
        , m_taskRunnerHandle(m_taskRunner)
    {
    }

    void SetUp() override
    {
        PaintArtifactCompositorTest::SetUp();

        cc::LayerTreeSettings settings = WebLayerTreeViewImplForTesting::defaultLayerTreeSettings();
        settings.single_thread_proxy_scheduler = false;
        settings.use_layer_lists = true;
        m_webLayerTreeView = wrapUnique(new WebLayerTreeViewWithOutputSurface(settings));
        m_webLayerTreeView->setRootLayer(*getPaintArtifactCompositor().getWebLayer());
    }

    const cc::PropertyTrees& propertyTrees()
    {
        return *m_webLayerTreeView->layerTreeHost()->property_trees();
    }

    void update(const PaintArtifact& artifact)
    {
        PaintArtifactCompositorTest::update(artifact);
        m_webLayerTreeView->layerTreeHost()->LayoutAndUpdateLayers();
    }

private:
    scoped_refptr<base::TestSimpleTaskRunner> m_taskRunner;
    base::ThreadTaskRunnerHandle m_taskRunnerHandle;
    std::unique_ptr<WebLayerTreeViewWithOutputSurface> m_webLayerTreeView;
};

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, EmptyPaintArtifact)
{
    PaintArtifact emptyArtifact;
    update(emptyArtifact);
    EXPECT_TRUE(rootLayer()->children().empty());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, OneChunkWithAnOffset)
{
    TestPaintArtifact artifact;
    artifact.chunk(PaintChunkProperties())
        .rectDrawing(FloatRect(50, -50, 100, 100), Color::white);
    update(artifact.build());

    ASSERT_EQ(1u, contentLayerCount());
    const cc::Layer* child = contentLayerAt(0);
    EXPECT_THAT(child->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::white)));
    EXPECT_EQ(translation(50, -50), child->screen_space_transform());
    EXPECT_EQ(gfx::Size(100, 100), child->bounds());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, OneTransform)
{
    // A 90 degree clockwise rotation about (100, 100).
    RefPtr<TransformPaintPropertyNode> transform = TransformPaintPropertyNode::create(
        TransformationMatrix().rotate(90), FloatPoint3D(100, 100, 0));

    TestPaintArtifact artifact;
    artifact.chunk(transform, nullptr, nullptr)
        .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
    artifact.chunk(nullptr, nullptr, nullptr)
        .rectDrawing(FloatRect(0, 0, 100, 100), Color::gray);
    artifact.chunk(transform, nullptr, nullptr)
        .rectDrawing(FloatRect(100, 100, 200, 100), Color::black);
    update(artifact.build());

    ASSERT_EQ(3u, contentLayerCount());
    {
        const cc::Layer* layer = contentLayerAt(0);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::white)));
        gfx::RectF mappedRect(0, 0, 100, 100);
        layer->screen_space_transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(100, 0, 100, 100), mappedRect);
    }
    {
        const cc::Layer* layer = contentLayerAt(1);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::gray)));
        EXPECT_EQ(gfx::Transform(), layer->screen_space_transform());
    }
    {
        const cc::Layer* layer = contentLayerAt(2);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 200, 100), Color::black)));
        gfx::RectF mappedRect(0, 0, 200, 100);
        layer->screen_space_transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(0, 100, 100, 200), mappedRect);
    }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, TransformCombining)
{
    // A translation by (5, 5) within a 2x scale about (10, 10).
    RefPtr<TransformPaintPropertyNode> transform1 = TransformPaintPropertyNode::create(
        TransformationMatrix().scale(2), FloatPoint3D(10, 10, 0));
    RefPtr<TransformPaintPropertyNode> transform2 = TransformPaintPropertyNode::create(
        TransformationMatrix().translate(5, 5), FloatPoint3D(), transform1);

    TestPaintArtifact artifact;
    artifact.chunk(transform1, nullptr, nullptr)
        .rectDrawing(FloatRect(0, 0, 300, 200), Color::white);
    artifact.chunk(transform2, nullptr, nullptr)
        .rectDrawing(FloatRect(0, 0, 300, 200), Color::black);
    update(artifact.build());

    ASSERT_EQ(2u, contentLayerCount());
    {
        const cc::Layer* layer = contentLayerAt(0);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::white)));
        gfx::RectF mappedRect(0, 0, 300, 200);
        layer->screen_space_transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(-10, -10, 600, 400), mappedRect);
    }
    {
        const cc::Layer* layer = contentLayerAt(1);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::black)));
        gfx::RectF mappedRect(0, 0, 300, 200);
        layer->screen_space_transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(0, 0, 600, 400), mappedRect);
    }
    EXPECT_NE(
        contentLayerAt(0)->transform_tree_index(),
        contentLayerAt(1)->transform_tree_index());
}


} // namespace
} // namespace blink
