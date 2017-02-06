// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_PROTO_CONVERTER_H_
#define CC_LAYERS_LAYER_PROTO_CONVERTER_H_

#include <unordered_map>

#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer.h"

namespace cc {

namespace proto {
class LayerNode;
class LayerUpdate;
}

// A class to faciliate (de)serialization of a Layer tree to protocol buffers.
class CC_EXPORT LayerProtoConverter {
 public:
  // Starting at |root_layer|, serializes the layer hierarchy into the
  // proto::LayerNode.
  static void SerializeLayerHierarchy(const scoped_refptr<Layer> root_layer,
                                      proto::LayerNode* root_node);

  // Recursively iterate over the given LayerNode proto and read the structure
  // into a local Layer structure, re-using existing Layers. returns the new
  // root Layer after updating the hierarchy (may be the same as
  // |existing_root|). |existing_root| may be null, which might happen during
  // the first deserialize.
  static scoped_refptr<Layer> DeserializeLayerHierarchy(
      const scoped_refptr<Layer> existing_root,
      const proto::LayerNode& root_node,
      LayerTreeHost* layer_tree_host);

  // Serializes the properties of all the dirty nodes in the Layer hierarchy.
  // The proto::LayerUpdate will contain all nodes that are dirty. These nodes
  // will contain the list of dirty properties. This function also resets the
  // layers that need push properties set.
  static void SerializeLayerProperties(LayerTreeHost* host,
                                       proto::LayerUpdate* layer_update);

  // Iterate over all updated layers from the LayerUpdate, and update the
  // local Layers. |existing_root| must not be null, as that will make it
  // impossible to find the layer to apply the properties to.
  static void DeserializeLayerProperties(
      Layer* existing_root,
      const proto::LayerUpdate& layer_update);

  // Returns the Layer with proto.id() as the Layer id, if it exists in
  // |layer_id_map|. Otherwise, a new Layer is constructed of the type given
  // from proto.type().
  static scoped_refptr<Layer> FindOrAllocateAndConstruct(
      const proto::LayerNode& proto,
      const Layer::LayerIdMap& layer_id_map);

 private:
  LayerProtoConverter();
  ~LayerProtoConverter();

  // This method is the inner recursive function for SerializeLayerProperties
  // declared above.
  static void RecursivelySerializeLayerProperties(
      Layer* root_layer,
      proto::LayerUpdate* layer_update);

  using LayerIdMap = std::unordered_map<int, scoped_refptr<Layer>>;
  // Start at |root_layer| and recursively add the layer and all its children
  // and special layers to |layer_id_map|.
  // TODO(vmpstr): LayerIdMap ref counts layers, so this function needs to deal
  // with ref counted objects instead of iterating over raw pointers.
  static void RecursivelyFindAllLayers(Layer* root_layer,
                                       LayerIdMap* layer_id_map);
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_PROTO_CONVERTER_H_
