// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_proto_converter.h"

#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/empty_content_layer_client.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/solid_color_scrollbar_layer.h"
#include "cc/proto/layer.pb.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_settings.h"

namespace cc {

LayerProtoConverter::LayerProtoConverter() {}

LayerProtoConverter::~LayerProtoConverter() {}

// static
void LayerProtoConverter::SerializeLayerHierarchy(
    const scoped_refptr<Layer> root_layer,
    proto::LayerNode* root_node) {
  TRACE_EVENT0("cc.remote", "LayerProtoConverter::SerializeLayerHierarchy");
  root_layer->ToLayerNodeProto(root_node);
}

// static
scoped_refptr<Layer> LayerProtoConverter::DeserializeLayerHierarchy(
    scoped_refptr<Layer> existing_root,
    const proto::LayerNode& root_node,
    LayerTreeHost* layer_tree_host) {
  LayerIdMap layer_id_map;
  if (existing_root) {
    existing_root->ClearLayerTreePropertiesForDeserializationAndAddToMap(
        &layer_id_map);
  }

  scoped_refptr<Layer> new_root = existing_root;
  if (!existing_root ||
      (root_node.has_id() && root_node.id() != existing_root->id())) {
    // The root node has changed or there was no root node,
    // so find or create the new root.
    new_root = FindOrAllocateAndConstruct(root_node, layer_id_map);
  }
  new_root->FromLayerNodeProto(root_node, layer_id_map, layer_tree_host);
  return new_root;
}

// static
void LayerProtoConverter::SerializeLayerProperties(
    LayerTreeHost* host,
    proto::LayerUpdate* layer_update) {
  TRACE_EVENT0("cc.remote", "LayerProtoConverter::SerializeLayerProperties");
  for (auto layer : host->LayersThatShouldPushProperties())
    layer->ToLayerPropertiesProto(layer_update);
  host->LayersThatShouldPushProperties().clear();
}

// static
void LayerProtoConverter::DeserializeLayerProperties(
    Layer* existing_root,
    const proto::LayerUpdate& layer_update) {
  DCHECK(existing_root);
  LayerIdMap layer_id_map;
  RecursivelyFindAllLayers(existing_root, &layer_id_map);

  for (int i = 0; i < layer_update.layers_size(); ++i) {
    const proto::LayerProperties& layer_properties = layer_update.layers(i);

    Layer::LayerIdMap::const_iterator iter =
        layer_id_map.find(layer_properties.id());
    DCHECK(iter != layer_id_map.end());

    iter->second->FromLayerPropertiesProto(layer_properties);
  }
}

// static
void LayerProtoConverter::RecursivelyFindAllLayers(Layer* root_layer,
                                                   LayerIdMap* layer_id_map) {
  LayerTreeHostCommon::CallFunctionForEveryLayer(
      root_layer->layer_tree_host(),
      [layer_id_map](Layer* layer) { (*layer_id_map)[layer->id()] = layer; });
}

// static
scoped_refptr<Layer> LayerProtoConverter::FindOrAllocateAndConstruct(
    const proto::LayerNode& proto,
    const Layer::LayerIdMap& layer_id_map) {
  DCHECK(proto.has_id());
  Layer::LayerIdMap::const_iterator iter = layer_id_map.find(proto.id());
  if (iter != layer_id_map.end())
    return iter->second;
  DCHECK(proto.has_type());
  switch (proto.type()) {
    // Fall through and build a base layer.  This won't have any special layer
    // properties but still maintains the layer hierarchy if we run into a
    // layer type we don't support.
    case proto::LayerNode::UNKNOWN:
    case proto::LayerNode::LAYER:
      return Layer::Create().get();
    case proto::LayerNode::PICTURE_LAYER:
      return PictureLayer::Create(EmptyContentLayerClient::GetInstance());
    case proto::LayerNode::HEADS_UP_DISPLAY_LAYER:
      return HeadsUpDisplayLayer::Create();
    case proto::LayerNode::SOLID_COLOR_SCROLLBAR_LAYER:
      // Create and return a SolidColorScrollbarLayer with invalid properties
      // (orientation, thumb thickness, starting track, left_side_scroll, layer
      // id etc.).
      // These properties will be set correctly in the later step when we run
      // through LayerTreeHost and deserialize them for each layer.
      return SolidColorScrollbarLayer::Create(ScrollbarOrientation::HORIZONTAL,
                                              -1, -1, false, Layer::INVALID_ID);
  }
  // TODO(nyquist): Add the rest of the necessary LayerTypes. This function
  // should not return null.
  NOTREACHED();
  return nullptr;
}

}  // namespace cc
