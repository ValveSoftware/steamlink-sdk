// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view.h"

#include "mojo/services/view_manager/node.h"

namespace mojo {
namespace view_manager {
namespace service {

View::View(const ViewId& id) : id_(id), node_(NULL) {}

View::~View() {
}

void View::SetBitmap(const SkBitmap& bitmap) {
  bitmap_ = bitmap;
  if (node_) {
    node_->window()->SchedulePaintInRect(
        gfx::Rect(node_->window()->bounds().size()));
  }
}

}  // namespace service
}  // namespace view_manager
}  // namespace mojo
