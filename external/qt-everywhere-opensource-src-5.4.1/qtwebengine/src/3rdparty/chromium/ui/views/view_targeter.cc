// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_targeter.h"

#include "ui/events/event_target.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace views {

ViewTargeter::ViewTargeter() {}
ViewTargeter::~ViewTargeter() {}

gfx::RectF ViewTargeter::BoundsForEvent(const ui::LocatedEvent& event) const {
  gfx::RectF event_bounds(event.location_f(), gfx::SizeF(1, 1));
  if (event.IsGestureEvent()) {
    const ui::GestureEvent& gesture =
        static_cast<const ui::GestureEvent&>(event);
    event_bounds = gesture.details().bounding_box_f();
  }

  return event_bounds;
}

ui::EventTarget* ViewTargeter::FindTargetForEvent(ui::EventTarget* root,
                                                  ui::Event* event) {
  View* view = static_cast<View*>(root);
  if (event->IsKeyEvent())
    return FindTargetForKeyEvent(view, *static_cast<ui::KeyEvent*>(event));
  else if (event->IsScrollEvent())
    return EventTargeter::FindTargetForEvent(root, event);

  NOTREACHED() << "ViewTargeter does not yet support this event type.";
  return NULL;
}

ui::EventTarget* ViewTargeter::FindNextBestTarget(
    ui::EventTarget* previous_target,
    ui::Event* event) {
  return previous_target->GetParentTarget();
}

bool ViewTargeter::SubtreeCanAcceptEvent(
    ui::EventTarget* target,
    const ui::LocatedEvent& event) const {
  views::View* view = static_cast<views::View*>(target);
  if (!view->visible())
    return false;

  if (!view->CanProcessEventsWithinSubtree())
    return false;

  return true;
}

bool ViewTargeter::EventLocationInsideBounds(
    ui::EventTarget* target,
    const ui::LocatedEvent& event) const {
  views::View* view = static_cast<views::View*>(target);
  gfx::Rect rect(event.location(), gfx::Size(1, 1));
  gfx::RectF rect_in_view_coords_f(rect);
  if (view->parent())
    View::ConvertRectToTarget(view->parent(), view, &rect_in_view_coords_f);
  gfx::Rect rect_in_view_coords = gfx::ToEnclosingRect(rect_in_view_coords_f);

  // TODO(tdanderson): Don't call into HitTestRect() directly here.
  //                   See crbug.com/370579.
  return view->HitTestRect(rect_in_view_coords);
}

View* ViewTargeter::FindTargetForKeyEvent(View* view, const ui::KeyEvent& key) {
  if (view->GetFocusManager())
    return view->GetFocusManager()->GetFocusedView();
  return NULL;
}

}  // namespace aura
