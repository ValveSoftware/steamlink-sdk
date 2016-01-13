// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/masked_view_targeter.h"

#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/view.h"

namespace views {

MaskedViewTargeter::MaskedViewTargeter(View* masked_view)
    : masked_view_(masked_view) {
}

MaskedViewTargeter::~MaskedViewTargeter() {
}

bool MaskedViewTargeter::EventLocationInsideBounds(
    ui::EventTarget* target,
    const ui::LocatedEvent& event) const {
  View* view = static_cast<View*>(target);
  if (view == masked_view_) {
    gfx::Path mask;
    if (!GetHitTestMask(view, &mask))
      return ViewTargeter::EventLocationInsideBounds(view, event);

    gfx::Size size = view->bounds().size();
    SkRegion clip_region;
    clip_region.setRect(0, 0, size.width(), size.height());

    gfx::RectF bounds_f = ViewTargeter::BoundsForEvent(event);
    if (view->parent())
      View::ConvertRectToTarget(view->parent(), view, &bounds_f);
    gfx::Rect bounds = gfx::ToEnclosingRect(bounds_f);

    SkRegion mask_region;
    return mask_region.setPath(mask, clip_region) &&
           mask_region.intersects(RectToSkIRect(bounds));
  }

  return ViewTargeter::EventLocationInsideBounds(view, event);
}

}  // namespace views
