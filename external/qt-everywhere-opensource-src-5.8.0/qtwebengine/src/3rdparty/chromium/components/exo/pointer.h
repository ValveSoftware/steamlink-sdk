// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_POINTER_H_
#define COMPONENTS_EXO_POINTER_H_

#include <memory>

#include "base/macros.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/surface_observer.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {
class Event;
class MouseEvent;
class MouseWheelEvent;
}

namespace views {
class Widget;
}

namespace exo {
class PointerDelegate;
class PointerStylusDelegate;
class Surface;

// This class implements a client pointer that represents one or more input
// devices, such as mice, which control the pointer location and pointer focus.
class Pointer : public ui::EventHandler,
                public aura::client::CursorClientObserver,
                public SurfaceDelegate,
                public SurfaceObserver {
 public:
  explicit Pointer(PointerDelegate* delegate);
  ~Pointer() override;

  // Set the pointer surface, i.e., the surface that contains the pointer image
  // (cursor). The |hotspot| argument defines the position of the pointer
  // surface relative to the pointer location. Its top-left corner is always at
  // (x, y) - (hotspot.x, hotspot.y), where (x, y) are the coordinates of the
  // pointer location, in surface local coordinates.
  void SetCursor(Surface* surface, const gfx::Point& hotspot);

  // Set delegate for stylus events.
  void SetStylusDelegate(PointerStylusDelegate* delegate);

  // Overridden from ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;

  // Overridden from aura::client::CursorClientObserver:
  void OnCursorSetChanged(ui::CursorSetType cursor_set) override;

  // Overridden from SurfaceDelegate:
  void OnSurfaceCommit() override;
  bool IsSurfaceSynchronized() const override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

 private:
  // Creates the |widget_| for pointer.
  void CreatePointerWidget();

  // Updates the scale of the cursor with the latest state.
  void UpdateCursorScale();

  // Returns the effective target for |event|.
  Surface* GetEffectiveTargetForEvent(ui::Event* event) const;

  // The delegate instance that all events are dispatched to.
  PointerDelegate* const delegate_;

  // The delegate instance that all stylus related events are dispatched to.
  PointerStylusDelegate* stylus_delegate_ = nullptr;

  // The widget for the pointer cursor.
  std::unique_ptr<views::Widget> widget_;

  // The current pointer surface.
  Surface* surface_ = nullptr;

  // The current focus surface for the pointer.
  Surface* focus_ = nullptr;

  // The location of the pointer in the current focus surface.
  gfx::PointF location_;

  // The scale applied to the cursor to compensate for the UI scale.
  float cursor_scale_ = 1.0f;

  // The position of the pointer surface relative to the pointer location.
  gfx::Point hotspot_;

  // The current pointer type.
  ui::EventPointerType pointer_type_;

  // The current pointer tilt.
  gfx::Vector2dF tilt_;

  // The current pointer force.
  float force_;

  DISALLOW_COPY_AND_ASSIGN(Pointer);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_POINTER_H_
