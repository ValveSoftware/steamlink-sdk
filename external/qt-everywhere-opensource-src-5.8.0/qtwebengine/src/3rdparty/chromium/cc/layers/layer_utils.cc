// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_utils.h"

#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/geometry/box_f.h"

namespace cc {

namespace {

bool HasTransformAnimationThatInflatesBounds(const LayerImpl& layer) {
  return layer.HasTransformAnimationThatInflatesBounds();
}

inline bool HasAncestorTransformAnimation(const TransformNode* transform_node) {
  return transform_node->data.to_screen_is_potentially_animated;
}

inline bool HasAncestorFilterAnimation(const LayerImpl& layer) {
  // This function returns false, it should use the effect tree once filters
  // and filter animations are known by the tree.
  return false;
}

}  // namespace

bool LayerUtils::GetAnimationBounds(const LayerImpl& layer_in, gfx::BoxF* out) {
  // We don't care about animated bounds for invisible layers.
  if (!layer_in.DrawsContent())
    return false;

  TransformTree& transform_tree =
      layer_in.layer_tree_impl()->property_trees()->transform_tree;

  // We also don't care for layers that are not animated or a child of an
  // animated layer.
  if (!HasAncestorTransformAnimation(
          transform_tree.Node(layer_in.transform_tree_index())) &&
      !HasAncestorFilterAnimation(layer_in))
    return false;

  // To compute the inflated bounds for a layer, we start by taking its bounds
  // and converting it to a 3d box, and then we transform or inflate it
  // repeatedly as we walk up the transform tree to the root.
  //
  // At each transform_node without transform animation that inflates bounds:
  //
  //   1) We concat transform_node->data.to_parent to the coalesced_transform.
  //
  // At each transform_node with a transform animation that inflates bounds:
  //
  //   1) We concat transform_node->data.pre_local to the coalesced_transform.
  //   This is to translate the box so that the anchor point is the origin.
  //   2) We apply coalesced_transform to the 3d box and make the transform
  //   identity. This is apply the accumulated transform to the box to inflate
  //   it.
  //   3) We inflate the 3d box for animation as the node has a animated
  //   transform.
  //   4) We concat transform_node->data.post_local to the coalesced_transform.
  //   This step undoes the translation in step (1), accounts for the layer's
  //   position and the 2d translation to the space of the parent transform
  //   node.

  gfx::BoxF box(layer_in.bounds().width(), layer_in.bounds().height(), 0.f);

  // We want to inflate/transform the box as few times as possible. Each time
  // we do this, we have to make the box axis aligned again, so if we make many
  // small adjustments to the box by transforming it repeatedly rather than
  // once by the product of all these matrices, we will accumulate a bunch of
  // unnecessary inflation because of the the many axis-alignment fixes. This
  // matrix stores said product.
  gfx::Transform coalesced_transform;

  const TransformNode* transform_node =
      transform_tree.Node(layer_in.transform_tree_index());

  // If layer_in has a transform node, the offset_to_transform_parent() is 0
  coalesced_transform.Translate(layer_in.offset_to_transform_parent().x(),
                                layer_in.offset_to_transform_parent().y());

  for (; transform_tree.parent(transform_node);
       transform_node = transform_tree.parent(transform_node)) {
    LayerImpl* layer =
        layer_in.layer_tree_impl()->LayerById(transform_node->owner_id);

    // Filter animation bounds are unimplemented, see function
    // HasAncestorFilterAnimation() for reference.

    if (HasTransformAnimationThatInflatesBounds(*layer)) {
      coalesced_transform.ConcatTransform(transform_node->data.pre_local);
      coalesced_transform.TransformBox(&box);
      coalesced_transform.MakeIdentity();

      gfx::BoxF inflated;
      if (!layer->TransformAnimationBoundsForBox(box, &inflated))
        return false;
      box = inflated;

      coalesced_transform.ConcatTransform(transform_node->data.post_local);
    } else {
      coalesced_transform.ConcatTransform(transform_node->data.to_parent);
    }
  }

  // If we've got an unapplied coalesced transform at this point, it must still
  // be applied.
  if (!coalesced_transform.IsIdentity())
    coalesced_transform.TransformBox(&box);

  *out = box;

  return true;
}

}  // namespace cc
