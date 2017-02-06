// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/touch.h"

#include "ash/shell.h"
#include "components/exo/surface.h"
#include "components/exo/touch_delegate.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"

namespace exo {
namespace {

// Helper function that returns an iterator to the first item in |vector|
// with |value|.
template <typename T, typename U>
typename T::iterator FindVectorItem(T& vector, U value) {
  return std::find(vector.begin(), vector.end(), value);
}

// Helper function that returns true if |vector| contains an item with |value|.
template <typename T, typename U>
bool VectorContainsItem(T& vector, U value) {
  return FindVectorItem(vector, value) != vector.end();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Touch, public:

Touch::Touch(TouchDelegate* delegate) : delegate_(delegate) {
  ash::Shell::GetInstance()->AddPreTargetHandler(this);
}

Touch::~Touch() {
  delegate_->OnTouchDestroying(this);
  if (focus_)
    focus_->RemoveSurfaceObserver(this);
  ash::Shell::GetInstance()->RemovePreTargetHandler(this);
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void Touch::OnTouchEvent(ui::TouchEvent* event) {
  switch (event->type()) {
    case ui::ET_TOUCH_PRESSED: {
      // Early out if event doesn't contain a valid target for touch device.
      Surface* target = GetEffectiveTargetForEvent(event);
      if (!target)
        return;

      // If this is the first touch point then target becomes the focus surface
      // until all touch points have been released.
      if (touch_points_.empty()) {
        DCHECK(!focus_);
        focus_ = target;
        focus_->AddSurfaceObserver(this);
      }

      DCHECK(!VectorContainsItem(touch_points_, event->touch_id()));
      touch_points_.push_back(event->touch_id());

      // Convert location to focus surface coordinate space.
      DCHECK(focus_);
      gfx::Point location = event->location();
      aura::Window::ConvertPointToTarget(target->window(), focus_->window(),
                                         &location);

      // Generate a touch down event for the focus surface. Note that this can
      // be different from the target surface.
      delegate_->OnTouchDown(focus_, event->time_stamp(), event->touch_id(),
                             location);
    } break;
    case ui::ET_TOUCH_RELEASED: {
      auto it = FindVectorItem(touch_points_, event->touch_id());
      if (it != touch_points_.end()) {
        touch_points_.erase(it);

        // Reset focus surface if this is the last touch point.
        if (touch_points_.empty()) {
          DCHECK(focus_);
          focus_->RemoveSurfaceObserver(this);
          focus_ = nullptr;
        }

        delegate_->OnTouchUp(event->time_stamp(), event->touch_id());
      }
    } break;
    case ui::ET_TOUCH_MOVED: {
      auto it = FindVectorItem(touch_points_, event->touch_id());
      if (it != touch_points_.end()) {
        DCHECK(focus_);
        // Convert location to focus surface coordinate space.
        gfx::Point location = event->location();
        aura::Window::ConvertPointToTarget(
            static_cast<aura::Window*>(event->target()), focus_->window(),
            &location);

        delegate_->OnTouchMotion(event->time_stamp(), event->touch_id(),
                                 location);
      }
    } break;
    case ui::ET_TOUCH_CANCELLED: {
      auto it = FindVectorItem(touch_points_, event->touch_id());
      if (it != touch_points_.end()) {
        DCHECK(focus_);
        focus_->RemoveSurfaceObserver(this);
        focus_ = nullptr;

        // Cancel the full set of touch sequences as soon as one is canceled.
        touch_points_.clear();
        delegate_->OnTouchCancel();
      }
    } break;
    default:
      NOTREACHED();
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void Touch::OnSurfaceDestroying(Surface* surface) {
  DCHECK(surface == focus_);
  focus_ = nullptr;
  surface->RemoveSurfaceObserver(this);

  // Cancel touch sequences.
  DCHECK_NE(touch_points_.size(), 0u);
  touch_points_.clear();
  delegate_->OnTouchCancel();
}

////////////////////////////////////////////////////////////////////////////////
// Touch, private:

Surface* Touch::GetEffectiveTargetForEvent(ui::Event* event) const {
  Surface* target =
      Surface::AsSurface(static_cast<aura::Window*>(event->target()));
  if (!target)
    return nullptr;

  return delegate_->CanAcceptTouchEventsForSurface(target) ? target : nullptr;
}

}  // namespace exo
