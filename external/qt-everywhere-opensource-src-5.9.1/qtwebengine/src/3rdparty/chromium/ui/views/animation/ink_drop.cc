// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop.h"

namespace views {

InkDropContainerView::InkDropContainerView() {}

void InkDropContainerView::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  SetPaintToLayer(true);
  SetVisible(true);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->Add(ink_drop_layer);
}

void InkDropContainerView::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  layer()->Remove(ink_drop_layer);
  SetVisible(false);
  SetPaintToLayer(false);
}

bool InkDropContainerView::CanProcessEventsWithinSubtree() const {
  // Ensure the container View is found as the EventTarget instead of this.
  return false;
}

}  // namespace views
