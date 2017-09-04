// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "compositor_state_deserializer.h"

#include "base/run_loop.h"
#include "cc/animation/animation_host.h"
#include "cc/blimp/client_picture_cache.h"
#include "cc/blimp/compositor_proto_state.h"
#include "cc/blimp/layer_tree_host_remote.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/solid_color_scrollbar_layer.h"
#include "cc/playback/display_item_list.h"
#include "cc/playback/drawing_display_item.h"
#include "cc/proto/compositor_message.pb.h"
#include "cc/test/fake_image_serialization_processor.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/fake_remote_compositor_bridge.h"
#include "cc/test/remote_client_layer_factory.h"
#include "cc/test/remote_compositor_test.h"
#include "cc/test/serialization_test_utils.h"
#include "cc/test/skia_common.h"
#include "cc/test/stub_layer_tree_host_client.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"

namespace cc {
namespace {

class FakeContentLayerClient : public ContentLayerClient {
 public:
  FakeContentLayerClient(scoped_refptr<DisplayItemList> display_list,
                         gfx::Rect recorded_viewport)
      : display_list_(std::move(display_list)),
        recorded_viewport_(recorded_viewport) {}
  ~FakeContentLayerClient() override {}

  // ContentLayerClient implementation.
  gfx::Rect PaintableRegion() override { return recorded_viewport_; }
  scoped_refptr<DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting painting_status) override {
    return display_list_;
  }
  bool FillsBoundsCompletely() const override { return false; }
  size_t GetApproximateUnsharedMemoryUsage() const override { return 0; }

 private:
  scoped_refptr<DisplayItemList> display_list_;
  gfx::Rect recorded_viewport_;
};

class CompositorStateDeserializerTest : public RemoteCompositorTest {
 public:
  void VerifyTreesAreIdentical() {
    VerifySerializedTreesAreIdentical(
        layer_tree_host_remote_->GetLayerTree(),
        layer_tree_host_in_process_->GetLayerTree(),
        compositor_state_deserializer_.get());
  }
};

TEST_F(CompositorStateDeserializerTest, BasicSync) {
  // Set up a tree with a single node.
  scoped_refptr<Layer> root_layer = Layer::Create();
  layer_tree_host_remote_->GetLayerTree()->SetRootLayer(root_layer);

  // Synchronize State and verify.
  base::RunLoop().RunUntilIdle();
  VerifyTreesAreIdentical();

  // Swap the root layer.
  scoped_refptr<Layer> new_root_layer = Layer::Create();
  new_root_layer->AddChild(Layer::Create());
  new_root_layer->AddChild(Layer::Create());
  layer_tree_host_remote_->GetLayerTree()->SetRootLayer(new_root_layer);

  // Synchronize State and verify.
  base::RunLoop().RunUntilIdle();
  VerifyTreesAreIdentical();
  // Verify that we are no longer tracking the destroyed layer on the client.
  EXPECT_EQ(
      compositor_state_deserializer_->GetLayerForEngineId(root_layer->id()),
      nullptr);

  // Remove the root layer to change to a null tree.
  layer_tree_host_remote_->GetLayerTree()->SetRootLayer(nullptr);

  // Synchronize State and verify.
  base::RunLoop().RunUntilIdle();
  VerifyTreesAreIdentical();
}

TEST_F(CompositorStateDeserializerTest, ViewportLayers) {
  scoped_refptr<Layer> root_layer = Layer::Create();
  layer_tree_host_remote_->GetLayerTree()->SetRootLayer(root_layer);

  scoped_refptr<Layer> overscroll_elasticity_layer = Layer::Create();
  scoped_refptr<Layer> inner_viewport_scroll_layer = Layer::Create();
  scoped_refptr<Layer> outer_viewport_scroll_layer = Layer::Create();
  scoped_refptr<Layer> page_scale_layer = Layer::Create();
  layer_tree_host_remote_->GetLayerTree()->RegisterViewportLayers(
      overscroll_elasticity_layer, page_scale_layer,
      inner_viewport_scroll_layer, outer_viewport_scroll_layer);

  root_layer->AddChild(overscroll_elasticity_layer);
  overscroll_elasticity_layer->AddChild(page_scale_layer);
  page_scale_layer->AddChild(inner_viewport_scroll_layer);
  inner_viewport_scroll_layer->AddChild(outer_viewport_scroll_layer);

  // Synchronize State and verify.
  base::RunLoop().RunUntilIdle();
  VerifyTreesAreIdentical();
}

TEST_F(CompositorStateDeserializerTest, ScrollClipAndMaskLayers) {
  /*   root -- A---C---D
  /      |      \
  /      |       E(MaskLayer)
  /      ------B */
  scoped_refptr<Layer> root_layer = Layer::Create();
  layer_tree_host_remote_->GetLayerTree()->SetRootLayer(root_layer);

  scoped_refptr<Layer> layer_a = Layer::Create();
  scoped_refptr<Layer> layer_b = Layer::Create();
  scoped_refptr<Layer> layer_c = Layer::Create();
  scoped_refptr<Layer> layer_d = Layer::Create();
  scoped_refptr<Layer> layer_e = Layer::Create();

  root_layer->AddChild(layer_a);
  root_layer->AddChild(layer_b);
  layer_a->AddChild(layer_c);
  layer_c->AddChild(layer_d);

  layer_a->SetMaskLayer(layer_e.get());
  layer_c->SetScrollParent(layer_b.get());
  layer_c->SetScrollClipLayerId(root_layer->id());
  layer_d->SetClipParent(layer_a.get());

  // Synchronize State and verify.
  base::RunLoop().RunUntilIdle();
  VerifyTreesAreIdentical();
}

TEST_F(CompositorStateDeserializerTest, ReconcileScrollAndScale) {
  scoped_refptr<Layer> root_layer = Layer::Create();
  layer_tree_host_remote_->GetLayerTree()->SetRootLayer(root_layer);

  // Set scroll offset.
  scoped_refptr<Layer> scroll_layer = Layer::Create();
  root_layer->AddChild(scroll_layer);
  gfx::ScrollOffset engine_offset(4, 3);
  scroll_layer->SetScrollOffset(engine_offset);

  // Set page scale.
  float engine_page_scale = 0.5f;
  layer_tree_host_remote_->GetLayerTree()->SetPageScaleFactorAndLimits(
      engine_page_scale, 1.0, 1.0);

  // Synchronize State and verify that the engine values are used.
  base::RunLoop().RunUntilIdle();
  VerifyTreesAreIdentical();
  Layer* client_scroll_layer =
      compositor_state_deserializer_->GetLayerForEngineId(scroll_layer->id());
  EXPECT_EQ(engine_page_scale,
            layer_tree_host_in_process_->GetLayerTree()->page_scale_factor());
  EXPECT_EQ(engine_offset, client_scroll_layer->scroll_offset());

  // Now send some updates from the impl thread.
  ScrollAndScaleSet scroll_and_scale_set;

  gfx::ScrollOffset offset_from_impl_thread(10, 3);
  gfx::ScrollOffset scroll_delta =
      offset_from_impl_thread - client_scroll_layer->scroll_offset();
  LayerTreeHostCommon::ScrollUpdateInfo scroll_update;
  scroll_update.layer_id = client_scroll_layer->id();
  scroll_update.scroll_delta = gfx::ScrollOffsetToFlooredVector2d(scroll_delta);
  scroll_and_scale_set.scrolls.push_back(scroll_update);

  float page_scale_from_impl_side = 3.2f;
  float page_scale_delta =
      page_scale_from_impl_side /
      layer_tree_host_in_process_->GetLayerTree()->page_scale_factor();
  scroll_and_scale_set.page_scale_delta = page_scale_delta;

  layer_tree_host_in_process_->ApplyScrollAndScale(&scroll_and_scale_set);

  // The values on the client should have been forced to retain the original
  // engine value.
  EXPECT_EQ(engine_page_scale,
            layer_tree_host_in_process_->GetLayerTree()->page_scale_factor());
  EXPECT_EQ(engine_offset, client_scroll_layer->scroll_offset());

  // Now pull the deltas from the client onto the engine, this should result
  // in an aborted commit.
  proto::ClientStateUpdate client_state_update;
  compositor_state_deserializer_->PullClientStateUpdate(&client_state_update);

  // Send the reflected main frame state to the client layer tree host.
  compositor_state_deserializer_->SendUnappliedDeltasToLayerTreeHost();
  const ReflectedMainFrameState* reflected_state =
      layer_tree_host_in_process_->reflected_main_frame_state_for_testing();
  EXPECT_EQ(reflected_state->scrolls.size(), 1u);
  EXPECT_EQ(reflected_state->scrolls[0].layer_id, client_scroll_layer->id());
  EXPECT_EQ(reflected_state->scrolls[0].scroll_delta,
            gfx::ScrollOffsetToVector2dF(scroll_delta));
  EXPECT_EQ(reflected_state->page_scale_delta, page_scale_delta);

  layer_tree_host_remote_->ApplyStateUpdateFromClient(client_state_update);

  // Inform the deserializer that the updates were applied on the engine.
  // This should pre-emptively apply the deltas on the client.
  compositor_state_deserializer_->DidApplyStateUpdatesOnEngine();
  EXPECT_EQ(page_scale_from_impl_side,
            layer_tree_host_in_process_->GetLayerTree()->page_scale_factor());
  EXPECT_EQ(offset_from_impl_thread, client_scroll_layer->scroll_offset());

  // Now update the scroll offset on the engine, and ensure that the value is
  // used on the client.
  gfx::ScrollOffset new_engine_offset(10, 20);
  scroll_layer->SetScrollOffset(new_engine_offset);

  base::RunLoop().RunUntilIdle();
  VerifyTreesAreIdentical();
  EXPECT_EQ(page_scale_from_impl_side,
            layer_tree_host_in_process_->GetLayerTree()->page_scale_factor());
  EXPECT_EQ(new_engine_offset, client_scroll_layer->scroll_offset());
}

TEST_F(CompositorStateDeserializerTest, PropertyTreesAreIdentical) {
  // Override the LayerFactory. This is necessary to ensure the layer ids
  // tracked in PropertyTrees on the engine and client are identical.
  compositor_state_deserializer_->SetLayerFactoryForTesting(
      base::MakeUnique<RemoteClientLayerFactory>());

  scoped_refptr<Layer> root_layer = Layer::Create();
  root_layer->SetBounds(gfx::Size(10, 10));
  layer_tree_host_remote_->GetLayerTree()->SetRootLayer(root_layer);

  scoped_refptr<Layer> child1 = Layer::Create();
  root_layer->AddChild(child1);
  gfx::Transform transform;
  transform.Translate(gfx::Vector2dF(5, 4));
  child1->SetTransform(transform);
  child1->SetMasksToBounds(true);

  scoped_refptr<Layer> child2 = Layer::Create();
  root_layer->AddChild(child2);
  child2->SetBounds(gfx::Size(5, 5));
  child2->SetScrollOffset(gfx::ScrollOffset(3, 4));
  child2->SetScrollParent(child1.get());
  child2->SetUserScrollable(true, true);

  scoped_refptr<Layer> grandchild11 = Layer::Create();
  child1->AddChild(grandchild11);
  grandchild11->SetClipParent(root_layer.get());

  scoped_refptr<Layer> grandchild21 = Layer::Create();
  child2->AddChild(grandchild21);
  grandchild21->SetScrollClipLayerId(child1->id());
  grandchild21->SetOpacity(0.5);

  // Synchronize State and verify.
  base::RunLoop().RunUntilIdle();
  VerifyTreesAreIdentical();
  EXPECT_EQ(root_layer->id(), layer_tree_host_in_process_->root_layer()->id());

  // Sanity test to ensure that the PropertyTrees generated from the Layers on
  // the client and engine are identical.
  layer_tree_host_remote_->GetLayerTree()->BuildPropertyTreesForTesting();
  PropertyTrees* engine_property_trees =
      layer_tree_host_remote_->GetLayerTree()->property_trees();

  layer_tree_host_in_process_->BuildPropertyTreesForTesting();
  PropertyTrees* client_property_trees =
      layer_tree_host_in_process_->property_trees();

  EXPECT_EQ(*engine_property_trees, *client_property_trees);
}

TEST_F(CompositorStateDeserializerTest, SolidColorScrollbarLayer) {
  scoped_refptr<Layer> root_layer = Layer::Create();
  layer_tree_host_remote_->GetLayerTree()->SetRootLayer(root_layer);

  scoped_refptr<Layer> child_layer1 = Layer::Create();
  root_layer->AddChild(child_layer1);
  scoped_refptr<SolidColorScrollbarLayer> scroll_layer1 =
      SolidColorScrollbarLayer::Create(ScrollbarOrientation::HORIZONTAL, 20, 5,
                                       true, 3);
  scroll_layer1->SetScrollLayer(child_layer1->id());
  child_layer1->AddChild(scroll_layer1);

  scoped_refptr<SolidColorScrollbarLayer> scroll_layer2 =
      SolidColorScrollbarLayer::Create(ScrollbarOrientation::VERTICAL, 2, 9,
                                       false, 3);
  root_layer->AddChild(scroll_layer2);
  scoped_refptr<Layer> child_layer2 = Layer::Create();
  scroll_layer2->AddChild(child_layer2);
  scroll_layer2->SetScrollLayer(child_layer2->id());

  // Synchronize State and verify.
  base::RunLoop().RunUntilIdle();
  VerifyTreesAreIdentical();

  // Verify Scrollbar layers.
  SolidColorScrollbarLayer* client_scroll_layer1 =
      static_cast<SolidColorScrollbarLayer*>(
          compositor_state_deserializer_->GetLayerForEngineId(
              scroll_layer1->id()));
  EXPECT_EQ(client_scroll_layer1->ScrollLayerId(),
            compositor_state_deserializer_
                ->GetLayerForEngineId(scroll_layer1->ScrollLayerId())
                ->id());
  EXPECT_EQ(client_scroll_layer1->orientation(), scroll_layer1->orientation());

  SolidColorScrollbarLayer* client_scroll_layer2 =
      static_cast<SolidColorScrollbarLayer*>(
          compositor_state_deserializer_->GetLayerForEngineId(
              scroll_layer2->id()));
  EXPECT_EQ(client_scroll_layer2->ScrollLayerId(),
            compositor_state_deserializer_
                ->GetLayerForEngineId(scroll_layer2->ScrollLayerId())
                ->id());
  EXPECT_EQ(client_scroll_layer2->orientation(), scroll_layer2->orientation());
}

TEST_F(CompositorStateDeserializerTest, PictureLayer) {
  scoped_refptr<Layer> root_layer = Layer::Create();
  root_layer->SetBounds(gfx::Size(10, 10));
  root_layer->SetIsDrawable(true);
  layer_tree_host_remote_->GetLayerTree()->SetRootLayer(root_layer);

  gfx::Size layer_size = gfx::Size(5, 5);

  gfx::PointF offset(2.f, 3.f);
  SkPictureRecorder recorder;
  sk_sp<SkCanvas> canvas;
  SkPaint red_paint;
  red_paint.setColor(SK_ColorRED);
  canvas = sk_ref_sp(recorder.beginRecording(SkRect::MakeXYWH(
      offset.x(), offset.y(), layer_size.width(), layer_size.height())));
  canvas->translate(offset.x(), offset.y());
  canvas->drawRectCoords(0.f, 0.f, 4.f, 4.f, red_paint);
  sk_sp<SkPicture> test_picture = recorder.finishRecordingAsPicture();

  DisplayItemListSettings settings;
  settings.use_cached_picture = false;
  scoped_refptr<DisplayItemList> display_list =
      DisplayItemList::Create(settings);
  const gfx::Rect visual_rect(0, 0, 42, 42);
  display_list->CreateAndAppendDrawingItem<DrawingDisplayItem>(visual_rect,
                                                               test_picture);
  display_list->Finalize();
  FakeContentLayerClient content_client(display_list, gfx::Rect(layer_size));

  scoped_refptr<PictureLayer> picture_layer =
      PictureLayer::Create(&content_client);
  picture_layer->SetBounds(layer_size);
  picture_layer->SetIsDrawable(true);
  root_layer->AddChild(picture_layer);

  // Synchronize State and verify.
  base::RunLoop().RunUntilIdle();
  VerifyTreesAreIdentical();

  // Verify PictureLayer.
  PictureLayer* client_picture_layer = static_cast<PictureLayer*>(
      compositor_state_deserializer_->GetLayerForEngineId(picture_layer->id()));
  scoped_refptr<DisplayItemList> client_display_list =
      client_picture_layer->client()->PaintContentsToDisplayList(
          ContentLayerClient::PaintingControlSetting::PAINTING_BEHAVIOR_NORMAL);
  EXPECT_TRUE(AreDisplayListDrawingResultsSame(
      gfx::Rect(layer_size), display_list.get(), client_display_list.get()));

  // Now attach new layer with the same DisplayList.
  scoped_refptr<PictureLayer> picture_layer2 =
      PictureLayer::Create(&content_client);
  picture_layer2->SetBounds(layer_size);
  picture_layer2->SetIsDrawable(true);
  root_layer->AddChild(picture_layer2);

  // Synchronize State and verify.
  base::RunLoop().RunUntilIdle();
  VerifyTreesAreIdentical();

  // Verify PictureLayer2.
  PictureLayer* client_picture_layer2 = static_cast<PictureLayer*>(
      compositor_state_deserializer_->GetLayerForEngineId(
          picture_layer2->id()));
  scoped_refptr<DisplayItemList> client_display_list2 =
      client_picture_layer2->client()->PaintContentsToDisplayList(
          ContentLayerClient::PaintingControlSetting::PAINTING_BEHAVIOR_NORMAL);
  EXPECT_TRUE(AreDisplayListDrawingResultsSame(
      gfx::Rect(layer_size), display_list.get(), client_display_list2.get()));
}

}  // namespace
}  // namespace cc
