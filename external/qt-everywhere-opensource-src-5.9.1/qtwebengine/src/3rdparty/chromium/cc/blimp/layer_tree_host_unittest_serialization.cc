// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "cc/animation/animation_host.h"
#include "cc/blimp/layer_tree_host_remote.h"
#include "cc/layers/empty_content_layer_client.h"
#include "cc/layers/layer.h"
#include "cc/layers/solid_color_scrollbar_layer.h"
#include "cc/proto/layer.pb.h"
#include "cc/proto/layer_tree_host.pb.h"
#include "cc/test/fake_image_serialization_processor.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/remote_client_layer_factory.h"
#include "cc/test/remote_compositor_test.h"
#include "cc/test/serialization_test_utils.h"
#include "cc/trees/layer_tree.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_settings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

#define EXPECT_CLIENT_LAYER_DIRTY(engine_layer)                                \
  do {                                                                         \
    Layer* client_layer = compositor_state_deserializer_->GetLayerForEngineId( \
        engine_layer->id());                                                   \
    LayerTree* client_tree = client_layer->GetLayerTree();                     \
    EXPECT_TRUE(                                                               \
        client_tree->LayerNeedsPushPropertiesForTesting(client_layer));        \
  } while (false)

class LayerTreeHostSerializationTest : public RemoteCompositorTest {
 protected:
  void VerifySerializationAndDeserialization() {
    // Synchronize state.
    CHECK(HasPendingUpdate());
    base::RunLoop().RunUntilIdle();
    VerifySerializedTreesAreIdentical(
        layer_tree_host_remote_->GetLayerTree(),
        layer_tree_host_in_process_->GetLayerTree(),
        compositor_state_deserializer_.get());
  }

  void SetUpViewportLayers(LayerTree* engine_layer_tree) {
    scoped_refptr<Layer> overscroll_elasticity_layer = Layer::Create();
    scoped_refptr<Layer> page_scale_layer = Layer::Create();
    scoped_refptr<Layer> inner_viewport_scroll_layer = Layer::Create();
    scoped_refptr<Layer> outer_viewport_scroll_layer = Layer::Create();

    engine_layer_tree->root_layer()->AddChild(overscroll_elasticity_layer);
    engine_layer_tree->root_layer()->AddChild(page_scale_layer);
    engine_layer_tree->root_layer()->AddChild(inner_viewport_scroll_layer);
    engine_layer_tree->root_layer()->AddChild(outer_viewport_scroll_layer);

    engine_layer_tree->RegisterViewportLayers(
        overscroll_elasticity_layer, page_scale_layer,
        inner_viewport_scroll_layer, outer_viewport_scroll_layer);
  }
};

TEST_F(LayerTreeHostSerializationTest, AllMembersChanged) {
  LayerTree* engine_layer_tree = layer_tree_host_remote_->GetLayerTree();

  engine_layer_tree->SetRootLayer(Layer::Create());
  scoped_refptr<Layer> mask_layer = Layer::Create();
  engine_layer_tree->root_layer()->SetMaskLayer(mask_layer.get());
  SetUpViewportLayers(engine_layer_tree);

  engine_layer_tree->SetViewportSize(gfx::Size(3, 14));
  engine_layer_tree->SetDeviceScaleFactor(4.f);
  engine_layer_tree->SetPaintedDeviceScaleFactor(2.f);
  engine_layer_tree->SetPageScaleFactorAndLimits(2.f, 0.5f, 3.f);
  engine_layer_tree->set_background_color(SK_ColorMAGENTA);
  engine_layer_tree->set_has_transparent_background(true);
  LayerSelectionBound sel_bound;
  sel_bound.edge_top = gfx::Point(14, 3);
  LayerSelection selection;
  selection.start = sel_bound;
  engine_layer_tree->RegisterSelection(selection);
  VerifySerializationAndDeserialization();
}

TEST_F(LayerTreeHostSerializationTest, LayersChangedMultipleSerializations) {
  LayerTree* engine_layer_tree = layer_tree_host_remote_->GetLayerTree();
  engine_layer_tree->SetRootLayer(Layer::Create());
  SetUpViewportLayers(engine_layer_tree);

  VerifySerializationAndDeserialization();

  scoped_refptr<Layer> new_child = Layer::Create();
  engine_layer_tree->root_layer()->AddChild(new_child);
  engine_layer_tree->RegisterViewportLayers(nullptr, nullptr, nullptr, nullptr);
  VerifySerializationAndDeserialization();

  engine_layer_tree->SetRootLayer(nullptr);
  VerifySerializationAndDeserialization();
}

TEST_F(LayerTreeHostSerializationTest, AddAndRemoveNodeFromLayerTree) {
  /* Testing serialization when the tree hierarchy changes like this:
           root                  root
          /    \                /    \
         a      b        =>    a      c
                 \                     \
                  c                     d
      */
  LayerTree* engine_layer_tree = layer_tree_host_remote_->GetLayerTree();
  scoped_refptr<Layer> layer_src_root = Layer::Create();
  engine_layer_tree->SetRootLayer(layer_src_root);

  scoped_refptr<Layer> layer_src_a = Layer::Create();
  scoped_refptr<Layer> layer_src_b = Layer::Create();
  scoped_refptr<Layer> layer_src_c = Layer::Create();
  scoped_refptr<Layer> layer_src_d = Layer::Create();

  layer_src_root->AddChild(layer_src_a);
  layer_src_root->AddChild(layer_src_b);
  layer_src_b->AddChild(layer_src_c);
  VerifySerializationAndDeserialization();

  // Now change the Layer Hierarchy
  layer_src_c->RemoveFromParent();
  layer_src_b->RemoveFromParent();
  layer_src_root->AddChild(layer_src_c);
  layer_src_c->AddChild(layer_src_d);
  VerifySerializationAndDeserialization();
}

TEST_F(LayerTreeHostSerializationTest, TestNoExistingRoot) {
  /* Test deserialization of a tree that looks like:
         root
        /   \
       a     b
              \
               c
     There is no existing root node before serialization.
  */
  scoped_refptr<Layer> old_root_layer = Layer::Create();
  scoped_refptr<Layer> layer_a = Layer::Create();
  scoped_refptr<Layer> layer_b = Layer::Create();
  scoped_refptr<Layer> layer_c = Layer::Create();
  old_root_layer->AddChild(layer_a);
  old_root_layer->AddChild(layer_b);
  layer_b->AddChild(layer_c);
  layer_tree_host_remote_->GetLayerTree()->SetRootLayer(old_root_layer);
  VerifySerializationAndDeserialization();

  // Swap the root node.
  scoped_refptr<Layer> new_root_layer = Layer::Create();
  new_root_layer->AddChild(layer_a);
  new_root_layer->AddChild(layer_b);
  layer_tree_host_remote_->GetLayerTree()->SetRootLayer(old_root_layer);
  VerifySerializationAndDeserialization();
}

TEST_F(LayerTreeHostSerializationTest, RecursivePropertiesSerialization) {
  /* Testing serialization of properties for a tree that looks like this:
          root+
          /  \
         a*   b+[mask:*]
        /      \
       c        d
     Layers marked with * have changed properties.
     Layers marked with + have descendants with changed properties.
     Layer b also has a mask layer.
  */
  scoped_refptr<Layer> layer_src_root = Layer::Create();
  scoped_refptr<Layer> layer_src_a = Layer::Create();
  scoped_refptr<Layer> layer_src_b = Layer::Create();
  scoped_refptr<Layer> layer_src_b_mask = Layer::Create();
  scoped_refptr<Layer> layer_src_c = Layer::Create();
  scoped_refptr<Layer> layer_src_d = Layer::Create();

  layer_src_root->AddChild(layer_src_a);
  layer_src_root->AddChild(layer_src_b);
  layer_src_a->AddChild(layer_src_c);
  layer_src_b->AddChild(layer_src_d);
  layer_src_b->SetMaskLayer(layer_src_b_mask.get());

  layer_tree_host_remote_->GetLayerTree()->SetRootLayer(layer_src_root);
  VerifySerializationAndDeserialization();
  EXPECT_EQ(layer_tree_host_remote_->GetLayerTree()
                ->LayersThatShouldPushProperties()
                .size(),
            0u);
  EXPECT_EQ(layer_tree_host_in_process_->GetLayerTree()
                ->LayersThatShouldPushProperties()
                .size(),
            6u);
}

TEST_F(LayerTreeHostSerializationTest, ChildrenOrderChange) {
  /* Testing serialization and deserialization of a tree that initially looks
     like this:
          root
          /  \
         a    b
     The children are then re-ordered and changed to:
         root
         /  \
        b    a
              \
               c
  */
  scoped_refptr<Layer> layer_src_root = Layer::Create();
  layer_tree_host_remote_->GetLayerTree()->SetRootLayer(layer_src_root);
  scoped_refptr<Layer> layer_src_a = Layer::Create();
  scoped_refptr<Layer> layer_src_b = Layer::Create();

  layer_src_root->AddChild(layer_src_b);
  layer_src_root->AddChild(layer_src_a);
  VerifySerializationAndDeserialization();

  Layer* client_root =
      compositor_state_deserializer_->GetLayerForEngineId(layer_src_root->id());
  Layer* client_a =
      compositor_state_deserializer_->GetLayerForEngineId(layer_src_a->id());
  Layer* client_b =
      compositor_state_deserializer_->GetLayerForEngineId(layer_src_b->id());

  // Swap the children.
  layer_src_root->RemoveAllChildren();
  layer_src_root->AddChild(layer_src_a);
  layer_src_root->AddChild(layer_src_b);
  layer_src_a->AddChild(Layer::Create());
  VerifySerializationAndDeserialization();

  // Verify all old layers are re-used.
  Layer* new_client_root =
      compositor_state_deserializer_->GetLayerForEngineId(layer_src_root->id());
  Layer* new_client_a =
      compositor_state_deserializer_->GetLayerForEngineId(layer_src_a->id());
  Layer* new_client_b =
      compositor_state_deserializer_->GetLayerForEngineId(layer_src_b->id());
  EXPECT_EQ(client_root, new_client_root);
  EXPECT_EQ(client_a, new_client_a);
  EXPECT_EQ(client_b, new_client_b);
}

TEST_F(LayerTreeHostSerializationTest, DeletingLayers) {
  /* Testing serialization and deserialization of a tree that initially
     looks like this:
          root
          /  \
         a    b
               \
                c+[mask:*]
     First the mask layer is deleted.
     Then the subtree from node |b| is deleted in the next update.
  */
  scoped_refptr<Layer> layer_src_root = Layer::Create();
  layer_tree_host_remote_->GetLayerTree()->SetRootLayer(layer_src_root);

  scoped_refptr<Layer> layer_src_a = Layer::Create();
  scoped_refptr<Layer> layer_src_b = Layer::Create();
  scoped_refptr<Layer> layer_src_c = Layer::Create();
  scoped_refptr<Layer> layer_src_c_mask = Layer::Create();
  layer_src_root->AddChild(layer_src_a);
  layer_src_root->AddChild(layer_src_b);
  layer_src_b->AddChild(layer_src_c);
  layer_src_c->SetMaskLayer(layer_src_c_mask.get());
  VerifySerializationAndDeserialization();

  // Delete the mask layer.
  layer_src_c->SetMaskLayer(nullptr);
  VerifySerializationAndDeserialization();

  // Remove child b.
  layer_src_b->RemoveFromParent();
  VerifySerializationAndDeserialization();
}

TEST_F(LayerTreeHostSerializationTest, LayerDataSerialization) {
  scoped_refptr<Layer> layer = Layer::Create();
  layer_tree_host_remote_->GetLayerTree()->SetRootLayer(layer);
  VerifySerializationAndDeserialization();

  // Change all the fields.
  layer->SetTransformOrigin(gfx::Point3F(3.0f, 1.0f, 4.0f));
  layer->SetBackgroundColor(SK_ColorRED);
  layer->SetBounds(gfx::Size(3, 14));
  layer->SetDoubleSided(!layer->double_sided());
  layer->SetHideLayerAndSubtree(!layer->hide_layer_and_subtree());
  layer->SetMasksToBounds(!layer->masks_to_bounds());
  layer->AddMainThreadScrollingReasons(
      MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);
  layer->SetNonFastScrollableRegion(Region(gfx::Rect(5, 1, 14, 3)));
  layer->SetNonFastScrollableRegion(Region(gfx::Rect(3, 14, 1, 5)));
  layer->SetContentsOpaque(!layer->contents_opaque());
  layer->SetOpacity(0.4f);
  layer->SetBlendMode(SkXfermode::kOverlay_Mode);
  layer->SetIsRootForIsolatedGroup(!layer->is_root_for_isolated_group());
  layer->SetPosition(gfx::PointF(3.14f, 6.28f));
  layer->SetIsContainerForFixedPositionLayers(
      !layer->IsContainerForFixedPositionLayers());
  LayerPositionConstraint pos_con;
  pos_con.set_is_fixed_to_bottom_edge(true);
  layer->SetPositionConstraint(pos_con);
  layer->SetShouldFlattenTransform(!layer->should_flatten_transform());
  layer->SetUseParentBackfaceVisibility(
      !layer->use_parent_backface_visibility());
  gfx::Transform transform;
  transform.Rotate(90);
  layer->SetTransform(transform);
  layer->Set3dSortingContextId(42);
  layer->SetUserScrollable(layer->user_scrollable_horizontal(),
                           layer->user_scrollable_vertical());
  layer->SetScrollOffset(gfx::ScrollOffset(3, 14));
  gfx::Rect update_rect(14, 15);
  layer->SetNeedsDisplayRect(update_rect);

  VerifySerializationAndDeserialization();
  Layer* client_layer =
      compositor_state_deserializer_->GetLayerForEngineId(layer->id());
  EXPECT_EQ(update_rect, client_layer->update_rect());
}

TEST_F(LayerTreeHostSerializationTest, SolidColorScrollbarLayer) {
  scoped_refptr<Layer> root_layer = Layer::Create();
  layer_tree_host_remote_->GetLayerTree()->SetRootLayer(root_layer);
  scoped_refptr<Layer> child_layer = Layer::Create();
  root_layer->AddChild(child_layer);

  std::vector<scoped_refptr<SolidColorScrollbarLayer>> scrollbar_layers;
  scrollbar_layers.push_back(SolidColorScrollbarLayer::Create(
      ScrollbarOrientation::HORIZONTAL, 20, 5, true, root_layer->id()));
  scrollbar_layers.push_back(SolidColorScrollbarLayer::Create(
      ScrollbarOrientation::VERTICAL, 20, 5, false, child_layer->id()));
  scrollbar_layers.push_back(SolidColorScrollbarLayer::Create(
      ScrollbarOrientation::HORIZONTAL, 0, 0, true, root_layer->id()));
  scrollbar_layers.push_back(SolidColorScrollbarLayer::Create(
      ScrollbarOrientation::VERTICAL, 10, 35, true, child_layer->id()));
  for (const auto& layer : scrollbar_layers) {
    root_layer->AddChild(layer);
  }

  VerifySerializationAndDeserialization();
  for (const auto& engine_layer : scrollbar_layers) {
    VerifySerializedScrollbarLayersAreIdentical(
        engine_layer.get(), compositor_state_deserializer_.get());
  }
}

TEST_F(LayerTreeHostSerializationTest, PictureLayerSerialization) {
  // Override the layer factor to create FakePictureLayers in the deserializer.
  compositor_state_deserializer_->SetLayerFactoryForTesting(
      base::MakeUnique<RemoteClientLayerFactory>());

  LayerTree* engine_layer_tree = layer_tree_host_remote_->GetLayerTree();
  scoped_refptr<Layer> root_layer_src = Layer::Create();
  engine_layer_tree->SetRootLayer(root_layer_src);

  // Ensure that a PictureLayer work correctly for multiple rounds of
  // serialization and deserialization.
  FakeContentLayerClient content_client;
  gfx::Size bounds(256, 256);
  content_client.set_bounds(bounds);
  SkPaint simple_paint;
  simple_paint.setColor(SkColorSetARGB(255, 12, 23, 34));
  content_client.add_draw_rect(gfx::Rect(bounds), simple_paint);
  scoped_refptr<FakePictureLayer> picture_layer_src =
      FakePictureLayer::Create(&content_client);

  root_layer_src->AddChild(picture_layer_src);
  picture_layer_src->SetNearestNeighbor(!picture_layer_src->nearest_neighbor());
  picture_layer_src->SetNeedsDisplay();
  VerifySerializationAndDeserialization();
  layer_tree_host_in_process_->UpdateLayers();
  VerifySerializedPictureLayersAreIdentical(
      picture_layer_src.get(), compositor_state_deserializer_.get());

  // Another round.
  picture_layer_src->SetNeedsDisplay();
  SkPaint new_paint;
  new_paint.setColor(SkColorSetARGB(255, 12, 32, 44));
  content_client.add_draw_rect(gfx::Rect(bounds), new_paint);
  VerifySerializationAndDeserialization();
  layer_tree_host_in_process_->UpdateLayers();
  VerifySerializedPictureLayersAreIdentical(
      picture_layer_src.get(), compositor_state_deserializer_.get());
}

TEST_F(LayerTreeHostSerializationTest, EmptyPictureLayerSerialization) {
  // Override the layer factor to create FakePictureLayers in the deserializer.
  compositor_state_deserializer_->SetLayerFactoryForTesting(
      base::MakeUnique<RemoteClientLayerFactory>());

  LayerTree* engine_layer_tree = layer_tree_host_remote_->GetLayerTree();
  scoped_refptr<Layer> root_layer_src = Layer::Create();
  engine_layer_tree->SetRootLayer(root_layer_src);
  scoped_refptr<FakePictureLayer> picture_layer_src =
      FakePictureLayer::Create(EmptyContentLayerClient::GetInstance());
  root_layer_src->AddChild(picture_layer_src);
  picture_layer_src->SetNeedsDisplay();
  VerifySerializationAndDeserialization();
  layer_tree_host_in_process_->UpdateLayers();
  VerifySerializedPictureLayersAreIdentical(
      picture_layer_src.get(), compositor_state_deserializer_.get());
}

}  // namespace cc
