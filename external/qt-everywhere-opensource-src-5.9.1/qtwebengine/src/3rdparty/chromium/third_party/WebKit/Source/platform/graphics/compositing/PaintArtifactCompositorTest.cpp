// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/PaintArtifactCompositor.h"

#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/layers/layer.h"
#include "cc/test/fake_compositor_frame_sink.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/testing/PictureMatchers.h"
#include "platform/testing/TestPaintArtifact.h"
#include "platform/testing/WebLayerTreeViewImplForTesting.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {
namespace {

using ::testing::Pointee;

PaintChunkProperties defaultPaintChunkProperties() {
  PaintChunkProperties properties;
  properties.transform = TransformPaintPropertyNode::root();
  properties.clip = ClipPaintPropertyNode::root();
  properties.effect = EffectPaintPropertyNode::root();
  properties.scroll = ScrollPaintPropertyNode::root();
  return properties;
}

gfx::Transform translation(SkMScalar x, SkMScalar y) {
  gfx::Transform transform;
  transform.Translate(x, y);
  return transform;
}

class WebLayerTreeViewWithCompositorFrameSink
    : public WebLayerTreeViewImplForTesting {
 public:
  WebLayerTreeViewWithCompositorFrameSink(const cc::LayerTreeSettings& settings)
      : WebLayerTreeViewImplForTesting(settings) {}

  // cc::LayerTreeHostClient
  void RequestNewCompositorFrameSink() override {
    layerTreeHost()->SetCompositorFrameSink(
        cc::FakeCompositorFrameSink::Create3d());
  }
};

class PaintArtifactCompositorTestWithPropertyTrees : public ::testing::Test {
 protected:
  PaintArtifactCompositorTestWithPropertyTrees()
      : m_taskRunner(new base::TestSimpleTaskRunner),
        m_taskRunnerHandle(m_taskRunner) {}

  void SetUp() override {
    RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);

    // Delay constructing the compositor until after the feature is set.
    m_paintArtifactCompositor = PaintArtifactCompositor::create();
    m_paintArtifactCompositor->enableExtraDataForTesting();

    cc::LayerTreeSettings settings =
        WebLayerTreeViewImplForTesting::defaultLayerTreeSettings();
    settings.single_thread_proxy_scheduler = false;
    settings.use_layer_lists = true;
    m_webLayerTreeView =
        makeUnique<WebLayerTreeViewWithCompositorFrameSink>(settings);
    m_webLayerTreeView->setRootLayer(*m_paintArtifactCompositor->getWebLayer());
  }

  void TearDown() override { m_featuresBackup.restore(); }

  const cc::PropertyTrees& propertyTrees() {
    return *m_webLayerTreeView->layerTreeHost()
                ->GetLayerTree()
                ->property_trees();
  }

  const cc::TransformNode& transformNode(const cc::Layer* layer) {
    return *propertyTrees().transform_tree.Node(layer->transform_tree_index());
  }

  void update(const PaintArtifact& artifact) {
    m_paintArtifactCompositor->update(artifact, nullptr);
    m_webLayerTreeView->layerTreeHost()->LayoutAndUpdateLayers();
  }

  cc::Layer* rootLayer() { return m_paintArtifactCompositor->rootLayer(); }

  size_t contentLayerCount() {
    return m_paintArtifactCompositor->getExtraDataForTesting()
        ->contentLayers.size();
  }

  cc::Layer* contentLayerAt(unsigned index) {
    return m_paintArtifactCompositor->getExtraDataForTesting()
        ->contentLayers[index]
        .get();
  }

 private:
  RuntimeEnabledFeatures::Backup m_featuresBackup;
  std::unique_ptr<PaintArtifactCompositor> m_paintArtifactCompositor;
  scoped_refptr<base::TestSimpleTaskRunner> m_taskRunner;
  base::ThreadTaskRunnerHandle m_taskRunnerHandle;
  std::unique_ptr<WebLayerTreeViewWithCompositorFrameSink> m_webLayerTreeView;
};

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, EmptyPaintArtifact) {
  PaintArtifact emptyArtifact;
  update(emptyArtifact);
  EXPECT_TRUE(rootLayer()->children().empty());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, OneChunkWithAnOffset) {
  TestPaintArtifact artifact;
  artifact.chunk(defaultPaintChunkProperties())
      .rectDrawing(FloatRect(50, -50, 100, 100), Color::white);
  update(artifact.build());

  ASSERT_EQ(1u, contentLayerCount());
  const cc::Layer* child = contentLayerAt(0);
  EXPECT_THAT(child->GetPicture(),
              Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::white)));
  EXPECT_EQ(translation(50, -50), child->screen_space_transform());
  EXPECT_EQ(gfx::Size(100, 100), child->bounds());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, OneTransform) {
  // A 90 degree clockwise rotation about (100, 100).
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(TransformPaintPropertyNode::root(),
                                         TransformationMatrix().rotate(90),
                                         FloatPoint3D(100, 100, 0));

  TestPaintArtifact artifact;
  artifact
      .chunk(transform, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  artifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::gray);
  artifact
      .chunk(transform, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(100, 100, 200, 100), Color::black);
  update(artifact.build());

  ASSERT_EQ(3u, contentLayerCount());
  {
    const cc::Layer* layer = contentLayerAt(0);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::white)));
    gfx::RectF mappedRect(0, 0, 100, 100);
    layer->screen_space_transform().TransformRect(&mappedRect);
    EXPECT_EQ(gfx::RectF(100, 0, 100, 100), mappedRect);
  }
  {
    const cc::Layer* layer = contentLayerAt(1);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::gray)));
    EXPECT_EQ(gfx::Transform(), layer->screen_space_transform());
  }
  {
    const cc::Layer* layer = contentLayerAt(2);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 200, 100), Color::black)));
    gfx::RectF mappedRect(0, 0, 200, 100);
    layer->screen_space_transform().TransformRect(&mappedRect);
    EXPECT_EQ(gfx::RectF(0, 100, 100, 200), mappedRect);
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, TransformCombining) {
  // A translation by (5, 5) within a 2x scale about (10, 10).
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::create(TransformPaintPropertyNode::root(),
                                         TransformationMatrix().scale(2),
                                         FloatPoint3D(10, 10, 0));
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(
          transform1, TransformationMatrix().translate(5, 5), FloatPoint3D());

  TestPaintArtifact artifact;
  artifact
      .chunk(transform1, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 300, 200), Color::white);
  artifact
      .chunk(transform2, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 300, 200), Color::black);
  update(artifact.build());

  ASSERT_EQ(2u, contentLayerCount());
  {
    const cc::Layer* layer = contentLayerAt(0);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::white)));
    gfx::RectF mappedRect(0, 0, 300, 200);
    layer->screen_space_transform().TransformRect(&mappedRect);
    EXPECT_EQ(gfx::RectF(-10, -10, 600, 400), mappedRect);
  }
  {
    const cc::Layer* layer = contentLayerAt(1);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::black)));
    gfx::RectF mappedRect(0, 0, 300, 200);
    layer->screen_space_transform().TransformRect(&mappedRect);
    EXPECT_EQ(gfx::RectF(0, 0, 600, 400), mappedRect);
  }
  EXPECT_NE(contentLayerAt(0)->transform_tree_index(),
            contentLayerAt(1)->transform_tree_index());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       FlattensInheritedTransform) {
  for (bool transformIsFlattened : {true, false}) {
    SCOPED_TRACE(transformIsFlattened);

    // The flattens_inherited_transform bit corresponds to whether the _parent_
    // transform node flattens the transform. This is because Blink's notion of
    // flattening determines whether content within the node's local transform
    // is flattened, while cc's notion applies in the parent's coordinate space.
    RefPtr<TransformPaintPropertyNode> transform1 =
        TransformPaintPropertyNode::create(TransformPaintPropertyNode::root(),
                                           TransformationMatrix(),
                                           FloatPoint3D());
    RefPtr<TransformPaintPropertyNode> transform2 =
        TransformPaintPropertyNode::create(
            transform1, TransformationMatrix().rotate3d(0, 45, 0),
            FloatPoint3D());
    RefPtr<TransformPaintPropertyNode> transform3 =
        TransformPaintPropertyNode::create(
            transform2, TransformationMatrix().rotate3d(0, 45, 0),
            FloatPoint3D(), transformIsFlattened);

    TestPaintArtifact artifact;
    artifact
        .chunk(transform3, ClipPaintPropertyNode::root(),
               EffectPaintPropertyNode::root())
        .rectDrawing(FloatRect(0, 0, 300, 200), Color::white);
    update(artifact.build());

    ASSERT_EQ(1u, contentLayerCount());
    const cc::Layer* layer = contentLayerAt(0);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::white)));

    // The leaf transform node should flatten its inherited transform node
    // if and only if the intermediate rotation transform in the Blink tree
    // flattens.
    const cc::TransformNode* transformNode3 =
        propertyTrees().transform_tree.Node(layer->transform_tree_index());
    EXPECT_EQ(transformIsFlattened,
              transformNode3->flattens_inherited_transform);

    // Given this, we should expect the correct screen space transform for
    // each case. If the transform was flattened, we should see it getting
    // an effective horizontal scale of 1/sqrt(2) each time, thus it gets
    // half as wide. If the transform was not flattened, we should see an
    // empty rectangle (as the total 90 degree rotation makes it
    // perpendicular to the viewport).
    gfx::RectF rect(0, 0, 100, 100);
    layer->screen_space_transform().TransformRect(&rect);
    if (transformIsFlattened)
      EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 50, 100), rect);
    else
      EXPECT_TRUE(rect.IsEmpty());
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, SortingContextID) {
  // Has no 3D rendering context.
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::create(TransformPaintPropertyNode::root(),
                                         TransformationMatrix(),
                                         FloatPoint3D());
  // Establishes a 3D rendering context.
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(transform1, TransformationMatrix(),
                                         FloatPoint3D(), false, 1);
  // Extends the 3D rendering context of transform2.
  RefPtr<TransformPaintPropertyNode> transform3 =
      TransformPaintPropertyNode::create(transform2, TransformationMatrix(),
                                         FloatPoint3D(), false, 1);
  // Establishes a 3D rendering context distinct from transform2.
  RefPtr<TransformPaintPropertyNode> transform4 =
      TransformPaintPropertyNode::create(transform2, TransformationMatrix(),
                                         FloatPoint3D(), false, 2);

  TestPaintArtifact artifact;
  artifact
      .chunk(transform1, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 300, 200), Color::white);
  artifact
      .chunk(transform2, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 300, 200), Color::lightGray);
  artifact
      .chunk(transform3, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 300, 200), Color::darkGray);
  artifact
      .chunk(transform4, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 300, 200), Color::black);
  update(artifact.build());

  ASSERT_EQ(4u, contentLayerCount());

  // The white layer is not 3D sorted.
  const cc::Layer* whiteLayer = contentLayerAt(0);
  EXPECT_THAT(whiteLayer->GetPicture(),
              Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::white)));
  int whiteSortingContextId = transformNode(whiteLayer).sorting_context_id;
  EXPECT_EQ(whiteLayer->sorting_context_id(), whiteSortingContextId);
  EXPECT_EQ(0, whiteSortingContextId);

  // The light gray layer is 3D sorted.
  const cc::Layer* lightGrayLayer = contentLayerAt(1);
  EXPECT_THAT(
      lightGrayLayer->GetPicture(),
      Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::lightGray)));
  int lightGraySortingContextId =
      transformNode(lightGrayLayer).sorting_context_id;
  EXPECT_EQ(lightGrayLayer->sorting_context_id(), lightGraySortingContextId);
  EXPECT_NE(0, lightGraySortingContextId);

  // The dark gray layer is 3D sorted with the light gray layer, but has a
  // separate transform node.
  const cc::Layer* darkGrayLayer = contentLayerAt(2);
  EXPECT_THAT(
      darkGrayLayer->GetPicture(),
      Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::darkGray)));
  int darkGraySortingContextId =
      transformNode(darkGrayLayer).sorting_context_id;
  EXPECT_EQ(darkGrayLayer->sorting_context_id(), darkGraySortingContextId);
  EXPECT_EQ(lightGraySortingContextId, darkGraySortingContextId);
  EXPECT_NE(lightGrayLayer->transform_tree_index(),
            darkGrayLayer->transform_tree_index());

  // The black layer is 3D sorted, but in a separate context from the previous
  // layers.
  const cc::Layer* blackLayer = contentLayerAt(3);
  EXPECT_THAT(blackLayer->GetPicture(),
              Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::black)));
  int blackSortingContextId = transformNode(blackLayer).sorting_context_id;
  EXPECT_EQ(blackLayer->sorting_context_id(), blackSortingContextId);
  EXPECT_NE(0, blackSortingContextId);
  EXPECT_NE(lightGraySortingContextId, blackSortingContextId);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, OneClip) {
  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect(100, 100, 300, 200));

  TestPaintArtifact artifact;
  artifact
      .chunk(TransformPaintPropertyNode::root(), clip,
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(220, 80, 300, 200), Color::black);
  update(artifact.build());

  ASSERT_EQ(1u, contentLayerCount());
  const cc::Layer* layer = contentLayerAt(0);
  EXPECT_THAT(layer->GetPicture(),
              Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::black)));
  EXPECT_EQ(translation(220, 80), layer->screen_space_transform());

  const cc::ClipNode* clipNode =
      propertyTrees().clip_tree.Node(layer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, clipNode->clip_type);
  EXPECT_TRUE(clipNode->layers_are_clipped);
  EXPECT_EQ(gfx::RectF(100, 100, 300, 200), clipNode->clip);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, NestedClips) {
  RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect(100, 100, 700, 700));
  RefPtr<ClipPaintPropertyNode> clip2 =
      ClipPaintPropertyNode::create(clip1, TransformPaintPropertyNode::root(),
                                    FloatRoundedRect(200, 200, 700, 100));

  TestPaintArtifact artifact;
  artifact
      .chunk(TransformPaintPropertyNode::root(), clip1,
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(300, 350, 100, 100), Color::white);
  artifact
      .chunk(TransformPaintPropertyNode::root(), clip2,
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(300, 350, 100, 100), Color::lightGray);
  artifact
      .chunk(TransformPaintPropertyNode::root(), clip1,
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(300, 350, 100, 100), Color::darkGray);
  artifact
      .chunk(TransformPaintPropertyNode::root(), clip2,
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(300, 350, 100, 100), Color::black);
  update(artifact.build());

  ASSERT_EQ(4u, contentLayerCount());

  const cc::Layer* whiteLayer = contentLayerAt(0);
  EXPECT_THAT(whiteLayer->GetPicture(),
              Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::white)));
  EXPECT_EQ(translation(300, 350), whiteLayer->screen_space_transform());

  const cc::Layer* lightGrayLayer = contentLayerAt(1);
  EXPECT_THAT(
      lightGrayLayer->GetPicture(),
      Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::lightGray)));
  EXPECT_EQ(translation(300, 350), lightGrayLayer->screen_space_transform());

  const cc::Layer* darkGrayLayer = contentLayerAt(2);
  EXPECT_THAT(
      darkGrayLayer->GetPicture(),
      Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::darkGray)));
  EXPECT_EQ(translation(300, 350), darkGrayLayer->screen_space_transform());

  const cc::Layer* blackLayer = contentLayerAt(3);
  EXPECT_THAT(blackLayer->GetPicture(),
              Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::black)));
  EXPECT_EQ(translation(300, 350), blackLayer->screen_space_transform());

  EXPECT_EQ(whiteLayer->clip_tree_index(), darkGrayLayer->clip_tree_index());
  const cc::ClipNode* outerClip =
      propertyTrees().clip_tree.Node(whiteLayer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, outerClip->clip_type);
  EXPECT_TRUE(outerClip->layers_are_clipped);
  EXPECT_EQ(gfx::RectF(100, 100, 700, 700), outerClip->clip);

  EXPECT_EQ(lightGrayLayer->clip_tree_index(), blackLayer->clip_tree_index());
  const cc::ClipNode* innerClip =
      propertyTrees().clip_tree.Node(blackLayer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, innerClip->clip_type);
  EXPECT_TRUE(innerClip->layers_are_clipped);
  EXPECT_EQ(gfx::RectF(200, 200, 700, 100), innerClip->clip);
  EXPECT_EQ(outerClip->id, innerClip->parent_id);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, DeeplyNestedClips) {
  Vector<RefPtr<ClipPaintPropertyNode>> clips;
  for (unsigned i = 1; i <= 10; i++) {
    clips.append(ClipPaintPropertyNode::create(
        clips.isEmpty() ? ClipPaintPropertyNode::root() : clips.last(),
        TransformPaintPropertyNode::root(),
        FloatRoundedRect(5 * i, 0, 100, 200 - 10 * i)));
  }

  TestPaintArtifact artifact;
  artifact
      .chunk(TransformPaintPropertyNode::root(), clips.last(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 200, 200), Color::white);
  update(artifact.build());

  // Check the drawing layer.
  ASSERT_EQ(1u, contentLayerCount());
  const cc::Layer* drawingLayer = contentLayerAt(0);
  EXPECT_THAT(drawingLayer->GetPicture(),
              Pointee(drawsRectangle(FloatRect(0, 0, 200, 200), Color::white)));
  EXPECT_EQ(gfx::Transform(), drawingLayer->screen_space_transform());

  // Check the clip nodes.
  const cc::ClipNode* clipNode =
      propertyTrees().clip_tree.Node(drawingLayer->clip_tree_index());
  for (auto it = clips.rbegin(); it != clips.rend(); ++it) {
    const ClipPaintPropertyNode* paintClipNode = it->get();
    EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, clipNode->clip_type);
    EXPECT_TRUE(clipNode->layers_are_clipped);
    EXPECT_EQ(paintClipNode->clipRect().rect(), clipNode->clip);
    clipNode = propertyTrees().clip_tree.Node(clipNode->parent_id);
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, SiblingClips) {
  RefPtr<ClipPaintPropertyNode> commonClip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect(0, 0, 800, 600));
  RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::create(
      commonClip, TransformPaintPropertyNode::root(),
      FloatRoundedRect(0, 0, 400, 600));
  RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::create(
      commonClip, TransformPaintPropertyNode::root(),
      FloatRoundedRect(400, 0, 400, 600));

  TestPaintArtifact artifact;
  artifact
      .chunk(TransformPaintPropertyNode::root(), clip1,
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 640, 480), Color::white);
  artifact
      .chunk(TransformPaintPropertyNode::root(), clip2,
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 640, 480), Color::black);
  update(artifact.build());

  ASSERT_EQ(2u, contentLayerCount());

  const cc::Layer* whiteLayer = contentLayerAt(0);
  EXPECT_THAT(whiteLayer->GetPicture(),
              Pointee(drawsRectangle(FloatRect(0, 0, 640, 480), Color::white)));
  EXPECT_EQ(gfx::Transform(), whiteLayer->screen_space_transform());
  const cc::ClipNode* whiteClip =
      propertyTrees().clip_tree.Node(whiteLayer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, whiteClip->clip_type);
  EXPECT_TRUE(whiteClip->layers_are_clipped);
  ASSERT_EQ(gfx::RectF(0, 0, 400, 600), whiteClip->clip);

  const cc::Layer* blackLayer = contentLayerAt(1);
  EXPECT_THAT(blackLayer->GetPicture(),
              Pointee(drawsRectangle(FloatRect(0, 0, 640, 480), Color::black)));
  EXPECT_EQ(gfx::Transform(), blackLayer->screen_space_transform());
  const cc::ClipNode* blackClip =
      propertyTrees().clip_tree.Node(blackLayer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, blackClip->clip_type);
  EXPECT_TRUE(blackClip->layers_are_clipped);
  ASSERT_EQ(gfx::RectF(400, 0, 400, 600), blackClip->clip);

  EXPECT_EQ(whiteClip->parent_id, blackClip->parent_id);
  const cc::ClipNode* commonClipNode =
      propertyTrees().clip_tree.Node(whiteClip->parent_id);
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP,
            commonClipNode->clip_type);
  EXPECT_TRUE(commonClipNode->layers_are_clipped);
  ASSERT_EQ(gfx::RectF(0, 0, 800, 600), commonClipNode->clip);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       ForeignLayerPassesThrough) {
  scoped_refptr<cc::Layer> layer = cc::Layer::Create();

  TestPaintArtifact artifact;
  artifact.chunk(defaultPaintChunkProperties())
      .foreignLayer(FloatPoint(50, 100), IntSize(400, 300), layer);
  update(artifact.build());

  ASSERT_EQ(1u, contentLayerCount());
  EXPECT_EQ(layer, contentLayerAt(0));
  EXPECT_EQ(gfx::Size(400, 300), layer->bounds());
  EXPECT_EQ(translation(50, 100), layer->screen_space_transform());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, EffectTreeConversion) {
  RefPtr<EffectPaintPropertyNode> effect1 = EffectPaintPropertyNode::create(
      EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      ClipPaintPropertyNode::root(), CompositorFilterOperations(), 0.5);
  RefPtr<EffectPaintPropertyNode> effect2 = EffectPaintPropertyNode::create(
      effect1, TransformPaintPropertyNode::root(),
      ClipPaintPropertyNode::root(), CompositorFilterOperations(), 0.3);
  RefPtr<EffectPaintPropertyNode> effect3 = EffectPaintPropertyNode::create(
      EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      ClipPaintPropertyNode::root(), CompositorFilterOperations(), 0.2);

  TestPaintArtifact artifact;
  artifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             effect2.get())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  artifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             effect1.get())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  artifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             effect3.get())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  update(artifact.build());

  ASSERT_EQ(3u, contentLayerCount());

  const cc::EffectTree& effectTree = propertyTrees().effect_tree;
  // Node #0 reserved for null; #1 for root render surface; #2 for
  // EffectPaintPropertyNode::root(), plus 3 nodes for those created by
  // this test.
  ASSERT_EQ(5u, effectTree.size());

  const cc::EffectNode& convertedRootEffect = *effectTree.Node(1);
  EXPECT_EQ(-1, convertedRootEffect.parent_id);

  const cc::EffectNode& convertedEffect1 = *effectTree.Node(2);
  EXPECT_EQ(convertedRootEffect.id, convertedEffect1.parent_id);
  EXPECT_FLOAT_EQ(0.5, convertedEffect1.opacity);

  const cc::EffectNode& convertedEffect2 = *effectTree.Node(3);
  EXPECT_EQ(convertedEffect1.id, convertedEffect2.parent_id);
  EXPECT_FLOAT_EQ(0.3, convertedEffect2.opacity);

  const cc::EffectNode& convertedEffect3 = *effectTree.Node(4);
  EXPECT_EQ(convertedRootEffect.id, convertedEffect3.parent_id);
  EXPECT_FLOAT_EQ(0.2, convertedEffect3.opacity);

  EXPECT_EQ(convertedEffect2.id, contentLayerAt(0)->effect_tree_index());
  EXPECT_EQ(convertedEffect1.id, contentLayerAt(1)->effect_tree_index());
  EXPECT_EQ(convertedEffect3.id, contentLayerAt(2)->effect_tree_index());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, OneScrollNode) {
  RefPtr<TransformPaintPropertyNode> scrollTranslation =
      TransformPaintPropertyNode::create(TransformPaintPropertyNode::root(),
                                         TransformationMatrix().translate(7, 9),
                                         FloatPoint3D());
  RefPtr<ScrollPaintPropertyNode> scroll = ScrollPaintPropertyNode::create(
      ScrollPaintPropertyNode::root(), scrollTranslation, IntSize(11, 13),
      IntSize(27, 31), true, false);

  TestPaintArtifact artifact;
  artifact
      .chunk(scrollTranslation, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root(), scroll)
      .rectDrawing(FloatRect(11, 13, 17, 19), Color::white);
  update(artifact.build());

  const cc::ScrollTree& scrollTree = propertyTrees().scroll_tree;
  // Node #0 reserved for null; #1 for root render surface.
  ASSERT_EQ(3u, scrollTree.size());
  const cc::ScrollNode& scrollNode = *scrollTree.Node(2);
  EXPECT_EQ(gfx::Size(11, 13), scrollNode.scroll_clip_layer_bounds);
  EXPECT_EQ(gfx::Size(27, 31), scrollNode.bounds);
  EXPECT_TRUE(scrollNode.user_scrollable_horizontal);
  EXPECT_FALSE(scrollNode.user_scrollable_vertical);
  EXPECT_EQ(1, scrollNode.parent_id);

  const cc::TransformTree& transformTree = propertyTrees().transform_tree;
  const cc::TransformNode& transformNode =
      *transformTree.Node(scrollNode.transform_id);
  EXPECT_TRUE(transformNode.local.IsIdentity());

  EXPECT_EQ(gfx::ScrollOffset(-7, -9),
            scrollTree.current_scroll_offset(contentLayerAt(0)->id()));

  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            scrollNode.main_thread_scrolling_reasons);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, NestedScrollNodes) {
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::create(
      EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      ClipPaintPropertyNode::root(), CompositorFilterOperations(), 0.5);

  RefPtr<TransformPaintPropertyNode> scrollTranslationA =
      TransformPaintPropertyNode::create(
          TransformPaintPropertyNode::root(),
          TransformationMatrix().translate(11, 13), FloatPoint3D());
  RefPtr<ScrollPaintPropertyNode> scrollA = ScrollPaintPropertyNode::create(
      ScrollPaintPropertyNode::root(), scrollTranslationA, IntSize(2, 3),
      IntSize(5, 7), false, true);
  scrollA->addMainThreadScrollingReasons(
      MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);
  RefPtr<TransformPaintPropertyNode> scrollTranslationB =
      TransformPaintPropertyNode::create(
          scrollTranslationA, TransformationMatrix().translate(37, 41),
          FloatPoint3D());
  RefPtr<ScrollPaintPropertyNode> scrollB = ScrollPaintPropertyNode::create(
      scrollA, scrollTranslationB, IntSize(19, 23), IntSize(29, 31), true,
      false);
  TestPaintArtifact artifact;
  artifact
      .chunk(scrollTranslationA, ClipPaintPropertyNode::root(), effect, scrollA)
      .rectDrawing(FloatRect(7, 11, 13, 17), Color::white);
  artifact
      .chunk(scrollTranslationB, ClipPaintPropertyNode::root(), effect, scrollB)
      .rectDrawing(FloatRect(1, 2, 3, 5), Color::white);
  update(artifact.build());

  const cc::ScrollTree& scrollTree = propertyTrees().scroll_tree;
  // Node #0 reserved for null; #1 for root render surface.
  ASSERT_EQ(4u, scrollTree.size());
  const cc::ScrollNode& scrollNodeA = *scrollTree.Node(2);
  EXPECT_EQ(gfx::Size(2, 3), scrollNodeA.scroll_clip_layer_bounds);
  EXPECT_EQ(gfx::Size(5, 7), scrollNodeA.bounds);
  EXPECT_FALSE(scrollNodeA.user_scrollable_horizontal);
  EXPECT_TRUE(scrollNodeA.user_scrollable_vertical);
  EXPECT_EQ(1, scrollNodeA.parent_id);
  const cc::ScrollNode& scrollNodeB = *scrollTree.Node(3);
  EXPECT_EQ(gfx::Size(19, 23), scrollNodeB.scroll_clip_layer_bounds);
  EXPECT_EQ(gfx::Size(29, 31), scrollNodeB.bounds);
  EXPECT_TRUE(scrollNodeB.user_scrollable_horizontal);
  EXPECT_FALSE(scrollNodeB.user_scrollable_vertical);
  EXPECT_EQ(scrollNodeA.id, scrollNodeB.parent_id);

  const cc::TransformTree& transformTree = propertyTrees().transform_tree;
  const cc::TransformNode& transformNodeA =
      *transformTree.Node(scrollNodeA.transform_id);
  EXPECT_TRUE(transformNodeA.local.IsIdentity());
  const cc::TransformNode& transformNodeB =
      *transformTree.Node(scrollNodeB.transform_id);
  EXPECT_TRUE(transformNodeB.local.IsIdentity());

  EXPECT_EQ(gfx::ScrollOffset(-11, -13),
            scrollTree.current_scroll_offset(contentLayerAt(0)->id()));
  EXPECT_EQ(gfx::ScrollOffset(-37, -41),
            scrollTree.current_scroll_offset(contentLayerAt(1)->id()));

  EXPECT_TRUE(scrollNodeA.main_thread_scrolling_reasons &
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);
  EXPECT_FALSE(scrollNodeB.main_thread_scrolling_reasons &
               MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);
}

}  // namespace
}  // namespace blink
