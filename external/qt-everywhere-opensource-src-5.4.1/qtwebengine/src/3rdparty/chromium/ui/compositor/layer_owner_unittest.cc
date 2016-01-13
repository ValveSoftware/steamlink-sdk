// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_owner.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ui {

TEST(LayerOwnerTest, RecreateLayerHonorsTargetVisibilityAndOpacity) {
  LayerOwner owner;
  Layer* layer = new Layer;
  layer->SetVisible(true);
  layer->SetOpacity(1.0f);

  owner.SetLayer(layer);

  ScopedLayerAnimationSettings settings(layer->GetAnimator());
  layer->SetVisible(false);
  layer->SetOpacity(0.0f);
  EXPECT_TRUE(layer->visible());
  EXPECT_EQ(1.0f, layer->opacity());

  scoped_ptr<Layer> old_layer(owner.RecreateLayer());
  EXPECT_FALSE(owner.layer()->visible());
  EXPECT_EQ(0.0f, owner.layer()->opacity());
}

}  // namespace ui
