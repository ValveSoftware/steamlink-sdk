// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_owner.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "ui/compositor/layer_owner_delegate.h"

namespace ui {

LayerOwner::LayerOwner() : layer_(NULL), layer_owner_delegate_(NULL) {
}

LayerOwner::~LayerOwner() {
}

void LayerOwner::SetLayer(Layer* layer) {
  DCHECK(!OwnsLayer());
  layer_owner_.reset(layer);
  layer_ = layer;
  layer_->owner_ = this;
}

std::unique_ptr<Layer> LayerOwner::AcquireLayer() {
  if (layer_owner_)
    layer_owner_->owner_ = NULL;
  return std::move(layer_owner_);
}

std::unique_ptr<Layer> LayerOwner::RecreateLayer() {
  std::unique_ptr<ui::Layer> old_layer(AcquireLayer());
  if (!old_layer)
    return old_layer;

  LayerDelegate* old_delegate = old_layer->delegate();
  old_layer->set_delegate(NULL);

  const gfx::Rect layer_bounds(old_layer->bounds());
  Layer* new_layer = new ui::Layer(old_layer->type());
  SetLayer(new_layer);
  new_layer->SetVisible(old_layer->GetTargetVisibility());
  new_layer->SetOpacity(old_layer->GetTargetOpacity());
  new_layer->SetBounds(layer_bounds);
  new_layer->SetMasksToBounds(old_layer->GetMasksToBounds());
  new_layer->set_name(old_layer->name());
  new_layer->SetFillsBoundsOpaquely(old_layer->fills_bounds_opaquely());
  new_layer->SetFillsBoundsCompletely(old_layer->FillsBoundsCompletely());
  new_layer->SetSubpixelPositionOffset(old_layer->subpixel_position_offset());
  new_layer->SetLayerInverted(old_layer->layer_inverted());
  new_layer->SetTransform(old_layer->GetTargetTransform());
  if (old_layer->type() == LAYER_SOLID_COLOR)
    new_layer->SetColor(old_layer->GetTargetColor());
  SkRegion* alpha_shape = old_layer->alpha_shape();
  if (alpha_shape)
    new_layer->SetAlphaShape(base::WrapUnique(new SkRegion(*alpha_shape)));

  if (old_layer->parent()) {
    // Install new layer as a sibling of the old layer, stacked below it.
    old_layer->parent()->Add(new_layer);
    old_layer->parent()->StackBelow(new_layer, old_layer.get());
  } else if (old_layer->GetCompositor()) {
    // If old_layer was the layer tree root then we need to move the Compositor
    // over to the new root.
    old_layer->GetCompositor()->SetRootLayer(new_layer);
  }

  // Migrate all the child layers over to the new layer. Copy the list because
  // the items are removed during iteration.
  std::vector<ui::Layer*> children_copy = old_layer->children();
  for (std::vector<ui::Layer*>::const_iterator it = children_copy.begin();
       it != children_copy.end();
       ++it) {
    ui::Layer* child = *it;
    new_layer->Add(child);
  }

  // Install the delegate last so that the delegate isn't notified as we copy
  // state to the new layer.
  new_layer->set_delegate(old_delegate);

  if (layer_owner_delegate_)
    layer_owner_delegate_->OnLayerRecreated(old_layer.get(), new_layer);

  return old_layer;
}

void LayerOwner::DestroyLayer() {
  layer_ = NULL;
  layer_owner_.reset();
}

bool LayerOwner::OwnsLayer() const {
  return !!layer_owner_;
}

}  // namespace ui
