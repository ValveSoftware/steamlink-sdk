// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_proto_converter.h"

#include "cc/layers/empty_content_layer_client.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/proto/layer.pb.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {
class LayerProtoConverterTest : public testing::Test {
 public:
  LayerProtoConverterTest()
      : fake_client_(FakeLayerTreeHostClient::DIRECT_3D) {}

 protected:
  void SetUp() override {
    layer_tree_host_ =
        FakeLayerTreeHost::Create(&fake_client_, &task_graph_runner_);
  }

  void TearDown() override {
    layer_tree_host_->SetRootLayer(nullptr);
    layer_tree_host_ = nullptr;
  }

  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostClient fake_client_;
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_;
};

TEST_F(LayerProtoConverterTest, TestKeepingRoot) {
  /* Test deserialization of a tree that looks like:
         root
        /   \
       a     b
              \
               c
     The old root node will be reused during deserialization.
  */
  scoped_refptr<Layer> old_root = Layer::Create();
  proto::LayerNode root_node;
  root_node.set_id(old_root->id());
  root_node.set_type(proto::LayerNode::LAYER);

  proto::LayerNode* child_a_node = root_node.add_children();
  child_a_node->set_id(442);
  child_a_node->set_type(proto::LayerNode::LAYER);
  child_a_node->set_parent_id(old_root->id());  // root_node

  proto::LayerNode* child_b_node = root_node.add_children();
  child_b_node->set_id(443);
  child_b_node->set_type(proto::LayerNode::LAYER);
  child_b_node->set_parent_id(old_root->id());  // root_node

  proto::LayerNode* child_c_node = child_b_node->add_children();
  child_c_node->set_id(444);
  child_c_node->set_type(proto::LayerNode::LAYER);
  child_c_node->set_parent_id(child_b_node->id());

  scoped_refptr<Layer> new_root =
      LayerProtoConverter::DeserializeLayerHierarchy(old_root, root_node,
                                                     layer_tree_host_.get());

  // The new root should not be the same as the old root.
  EXPECT_EQ(old_root->id(), new_root->id());
  ASSERT_EQ(2u, new_root->children().size());
  scoped_refptr<Layer> child_a = new_root->children()[0];
  scoped_refptr<Layer> child_b = new_root->children()[1];

  EXPECT_EQ(child_a_node->id(), child_a->id());
  EXPECT_EQ(child_b_node->id(), child_b->id());

  EXPECT_EQ(0u, child_a->children().size());
  ASSERT_EQ(1u, child_b->children().size());

  scoped_refptr<Layer> child_c = child_b->children()[0];
  EXPECT_EQ(child_c_node->id(), child_c->id());

  new_root->SetLayerTreeHost(nullptr);
}

TEST_F(LayerProtoConverterTest, TestNoExistingRoot) {
  /* Test deserialization of a tree that looks like:
         root
        /
       a
     There is no existing root node before serialization.
  */
  int new_root_id = 244;
  proto::LayerNode root_node;
  root_node.set_id(new_root_id);
  root_node.set_type(proto::LayerNode::LAYER);

  proto::LayerNode* child_a_node = root_node.add_children();
  child_a_node->set_id(442);
  child_a_node->set_type(proto::LayerNode::LAYER);
  child_a_node->set_parent_id(new_root_id);  // root_node

  scoped_refptr<Layer> new_root =
      LayerProtoConverter::DeserializeLayerHierarchy(nullptr, root_node,
                                                     layer_tree_host_.get());

  // The new root should not be the same as the old root.
  EXPECT_EQ(new_root_id, new_root->id());
  ASSERT_EQ(1u, new_root->children().size());
  scoped_refptr<Layer> child_a = new_root->children()[0];

  EXPECT_EQ(child_a_node->id(), child_a->id());
  EXPECT_EQ(0u, child_a->children().size());

  new_root->SetLayerTreeHost(nullptr);
}

TEST_F(LayerProtoConverterTest, TestSwappingRoot) {
  /* Test deserialization of a tree that looks like:
         root
        /   \
       a     b
              \
               c
     The old root node will be swapped out during deserialization.
  */
  proto::LayerNode root_node;
  root_node.set_id(441);
  root_node.set_type(proto::LayerNode::LAYER);

  proto::LayerNode* child_a_node = root_node.add_children();
  child_a_node->set_id(442);
  child_a_node->set_type(proto::LayerNode::LAYER);
  child_a_node->set_parent_id(root_node.id());

  proto::LayerNode* child_b_node = root_node.add_children();
  child_b_node->set_id(443);
  child_b_node->set_type(proto::LayerNode::LAYER);
  child_b_node->set_parent_id(root_node.id());

  proto::LayerNode* child_c_node = child_b_node->add_children();
  child_c_node->set_id(444);
  child_c_node->set_type(proto::LayerNode::LAYER);
  child_c_node->set_parent_id(child_b_node->id());

  scoped_refptr<Layer> old_root = Layer::Create();
  scoped_refptr<Layer> new_root =
      LayerProtoConverter::DeserializeLayerHierarchy(old_root, root_node,
                                                     layer_tree_host_.get());

  // The new root should not be the same as the old root.
  EXPECT_EQ(root_node.id(), new_root->id());
  ASSERT_EQ(2u, new_root->children().size());
  scoped_refptr<Layer> child_a = new_root->children()[0];
  scoped_refptr<Layer> child_b = new_root->children()[1];

  EXPECT_EQ(child_a_node->id(), child_a->id());
  EXPECT_EQ(child_b_node->id(), child_b->id());

  EXPECT_EQ(0u, child_a->children().size());
  ASSERT_EQ(1u, child_b->children().size());

  scoped_refptr<Layer> child_c = child_b->children()[0];
  EXPECT_EQ(child_c_node->id(), child_c->id());

  new_root->SetLayerTreeHost(nullptr);
}

TEST_F(LayerProtoConverterTest, RecursivePropertiesSerialization) {
  /* Testing serialization of properties for a tree that looks like this:
          root+
          /  \
         a*   b*+[mask:*,replica]
        /      \
       c        d*
     Layers marked with * have changed properties.
     Layers marked with + have descendants with changed properties.
     Layer b also has a mask layer and a replica layer.
  */
  scoped_refptr<Layer> layer_src_root = Layer::Create();
  scoped_refptr<Layer> layer_src_a = Layer::Create();
  scoped_refptr<Layer> layer_src_b = Layer::Create();
  scoped_refptr<Layer> layer_src_b_mask = Layer::Create();
  scoped_refptr<Layer> layer_src_b_replica = Layer::Create();
  scoped_refptr<Layer> layer_src_c = Layer::Create();
  scoped_refptr<Layer> layer_src_d = Layer::Create();
  layer_src_root->SetLayerTreeHost(layer_tree_host_.get());
  layer_src_root->AddChild(layer_src_a);
  layer_src_root->AddChild(layer_src_b);
  layer_src_a->AddChild(layer_src_c);
  layer_src_b->AddChild(layer_src_d);
  layer_src_b->SetMaskLayer(layer_src_b_mask.get());
  layer_src_b->SetReplicaLayer(layer_src_b_replica.get());

  proto::LayerUpdate layer_update;
  LayerProtoConverter::SerializeLayerProperties(
      layer_src_root->layer_tree_host(), &layer_update);

  // All flags for pushing properties should have been cleared.
  EXPECT_FALSE(
      layer_src_root->layer_tree_host()->LayerNeedsPushPropertiesForTesting(
          layer_src_root.get()));
  EXPECT_FALSE(
      layer_src_a->layer_tree_host()->LayerNeedsPushPropertiesForTesting(
          layer_src_a.get()));
  EXPECT_FALSE(
      layer_src_b->layer_tree_host()->LayerNeedsPushPropertiesForTesting(
          layer_src_b.get()));
  EXPECT_FALSE(
      layer_src_b_mask->layer_tree_host()->LayerNeedsPushPropertiesForTesting(
          layer_src_b_mask.get()));
  EXPECT_FALSE(
      layer_src_b_replica->layer_tree_host()
          ->LayerNeedsPushPropertiesForTesting(layer_src_b_replica.get()));
  EXPECT_FALSE(
      layer_src_c->layer_tree_host()->LayerNeedsPushPropertiesForTesting(
          layer_src_c.get()));
  EXPECT_FALSE(
      layer_src_d->layer_tree_host()->LayerNeedsPushPropertiesForTesting(
          layer_src_d.get()));

  // All layers needs to push properties as their layer tree host changed.
  ASSERT_EQ(7, layer_update.layers_size());
  layer_update.Clear();

  std::unordered_set<int> dirty_layer_ids;
  layer_src_a->SetNeedsPushProperties();
  dirty_layer_ids.insert(layer_src_a->id());
  layer_src_b->SetNeedsPushProperties();
  dirty_layer_ids.insert(layer_src_b->id());
  layer_src_b_mask->SetNeedsPushProperties();
  dirty_layer_ids.insert(layer_src_b_mask->id());
  layer_src_d->SetNeedsPushProperties();
  dirty_layer_ids.insert(layer_src_d->id());

  LayerProtoConverter::SerializeLayerProperties(
      layer_src_root->layer_tree_host(), &layer_update);

  // All flags for pushing properties should have been cleared.
  EXPECT_FALSE(
      layer_src_root->layer_tree_host()->LayerNeedsPushPropertiesForTesting(
          layer_src_root.get()));
  EXPECT_FALSE(
      layer_src_a->layer_tree_host()->LayerNeedsPushPropertiesForTesting(
          layer_src_a.get()));
  EXPECT_FALSE(
      layer_src_b->layer_tree_host()->LayerNeedsPushPropertiesForTesting(
          layer_src_b.get()));
  EXPECT_FALSE(
      layer_src_b_mask->layer_tree_host()->LayerNeedsPushPropertiesForTesting(
          layer_src_b_mask.get()));
  EXPECT_FALSE(
      layer_src_b_replica->layer_tree_host()
          ->LayerNeedsPushPropertiesForTesting(layer_src_b_replica.get()));
  EXPECT_FALSE(
      layer_src_c->layer_tree_host()->LayerNeedsPushPropertiesForTesting(
          layer_src_c.get()));
  EXPECT_FALSE(
      layer_src_d->layer_tree_host()->LayerNeedsPushPropertiesForTesting(
          layer_src_d.get()));

  // Only 4 of the layers should have been serialized.
  ASSERT_EQ(4, layer_update.layers_size());
  for (int index = 0; index < layer_update.layers_size(); index++)
    EXPECT_NE(dirty_layer_ids.find(layer_update.layers(index).id()),
              dirty_layer_ids.end());
  layer_src_root->SetLayerTreeHost(nullptr);
}

TEST_F(LayerProtoConverterTest, RecursivePropertiesSerializationSingleChild) {
  /* Testing serialization of properties for a tree that looks like this:
          root+
             \
              b*+[mask:*]
               \
                c
     Layers marked with * have changed properties.
     Layers marked with + have descendants with changed properties.
     Layer b also has a mask layer.
  */
  scoped_refptr<Layer> layer_src_root = Layer::Create();
  scoped_refptr<Layer> layer_src_b = Layer::Create();
  scoped_refptr<Layer> layer_src_b_mask = Layer::Create();
  scoped_refptr<Layer> layer_src_c = Layer::Create();
  layer_src_root->AddChild(layer_src_b);
  layer_src_b->AddChild(layer_src_c);
  layer_src_b->SetMaskLayer(layer_src_b_mask.get());
  layer_src_root->SetLayerTreeHost(layer_tree_host_.get());

  proto::LayerUpdate layer_update;
  LayerProtoConverter::SerializeLayerProperties(
      layer_src_root->layer_tree_host(), &layer_update);
  // All layers need to push properties as their layer tree host changed.
  ASSERT_EQ(4, layer_update.layers_size());
  layer_update.Clear();

  std::unordered_set<int> dirty_layer_ids;
  layer_src_b->SetNeedsPushProperties();
  dirty_layer_ids.insert(layer_src_b->id());
  layer_src_b_mask->SetNeedsPushProperties();
  dirty_layer_ids.insert(layer_src_b_mask->id());

  LayerProtoConverter::SerializeLayerProperties(
      layer_src_root->layer_tree_host(), &layer_update);

  // All flags for pushing properties should have been cleared.
  EXPECT_FALSE(
      layer_src_root->layer_tree_host()->LayerNeedsPushPropertiesForTesting(
          layer_src_root.get()));
  EXPECT_FALSE(
      layer_src_b->layer_tree_host()->LayerNeedsPushPropertiesForTesting(
          layer_src_b.get()));
  EXPECT_FALSE(
      layer_src_b_mask->layer_tree_host()->LayerNeedsPushPropertiesForTesting(
          layer_src_b_mask.get()));
  EXPECT_FALSE(
      layer_src_c->layer_tree_host()->LayerNeedsPushPropertiesForTesting(
          layer_src_c.get()));

  // Only 2 of the layers should have been serialized.
  ASSERT_EQ(2, layer_update.layers_size());
  for (int index = 0; index < layer_update.layers_size(); index++)
    EXPECT_NE(dirty_layer_ids.find(layer_update.layers(index).id()),
              dirty_layer_ids.end());
  layer_src_root->SetLayerTreeHost(nullptr);
}

TEST_F(LayerProtoConverterTest, PictureLayerTypeSerialization) {
  // Make sure that PictureLayers serialize to the
  // proto::LayerType::PICTURE_LAYER type.
  scoped_refptr<PictureLayer> layer =
      PictureLayer::Create(EmptyContentLayerClient::GetInstance());

  proto::LayerNode layer_hierarchy;
  LayerProtoConverter::SerializeLayerHierarchy(layer.get(), &layer_hierarchy);
  EXPECT_EQ(proto::LayerNode::PICTURE_LAYER, layer_hierarchy.type());
}

TEST_F(LayerProtoConverterTest, PictureLayerTypeDeserialization) {
  // Make sure that proto::LayerType::PICTURE_LAYER ends up building a
  // PictureLayer.
  scoped_refptr<Layer> old_root =
      PictureLayer::Create(EmptyContentLayerClient::GetInstance());
  proto::LayerNode root_node;
  root_node.set_id(old_root->id());
  root_node.set_type(proto::LayerNode::PICTURE_LAYER);

  scoped_refptr<Layer> new_root =
      LayerProtoConverter::DeserializeLayerHierarchy(old_root, root_node,
                                                     layer_tree_host_.get());

  // Validate that the ids are equal.
  EXPECT_EQ(old_root->id(), new_root->id());

  // Check that the layer type is equal by using the type this layer would
  // serialize to.
  proto::LayerNode layer_node;
  new_root->SetTypeForProtoSerialization(&layer_node);
  EXPECT_EQ(proto::LayerNode::PICTURE_LAYER, layer_node.type());

  new_root->SetLayerTreeHost(nullptr);
}

TEST_F(LayerProtoConverterTest, HudLayerTypeSerialization) {
  // Make sure that PictureLayers serialize to the
  // proto::LayerType::HEADS_UP_DISPLAY_LAYER type.
  scoped_refptr<HeadsUpDisplayLayer> layer = HeadsUpDisplayLayer::Create();

  proto::LayerNode layer_hierarchy;
  LayerProtoConverter::SerializeLayerHierarchy(layer.get(), &layer_hierarchy);
  EXPECT_EQ(proto::LayerNode::HEADS_UP_DISPLAY_LAYER, layer_hierarchy.type());
}

TEST_F(LayerProtoConverterTest, HudLayerTypeDeserialization) {
  // Make sure that proto::LayerType::HEADS_UP_DISPLAY_LAYER ends up building a
  // HeadsUpDisplayLayer.
  scoped_refptr<Layer> old_root = HeadsUpDisplayLayer::Create();
  proto::LayerNode root_node;
  root_node.set_id(old_root->id());
  root_node.set_type(proto::LayerNode::HEADS_UP_DISPLAY_LAYER);

  scoped_refptr<Layer> new_root =
      LayerProtoConverter::DeserializeLayerHierarchy(old_root, root_node,
                                                     layer_tree_host_.get());

  // Validate that the ids are equal.
  EXPECT_EQ(old_root->id(), new_root->id());

  // Check that the layer type is equal by using the type this layer would
  // serialize to.
  proto::LayerNode layer_node;
  new_root->SetTypeForProtoSerialization(&layer_node);
  EXPECT_EQ(proto::LayerNode::HEADS_UP_DISPLAY_LAYER, layer_node.type());

  new_root->SetLayerTreeHost(nullptr);
}

}  // namespace
}  // namespace cc
