// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/pointer.h"

#include "ash/common/display/display_info.h"
#include "ash/common/shell_window_ids.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "components/exo/pointer_delegate.h"
#include "components/exo/pointer_stylus_delegate.h"
#include "components/exo/surface.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/views/widget/widget.h"

namespace exo {
namespace {

static constexpr float kLargeCursorScale = 2.8;

// Synthesized events typically lack floating point precision so to avoid
// generating mouse event jitter we consider the location of these events
// to be the same as |location| if floored values match.
bool SameLocation(const ui::LocatedEvent* event, const gfx::PointF& location) {
  if (event->flags() & ui::EF_IS_SYNTHESIZED)
    return event->location() == gfx::ToFlooredPoint(location);

  return event->location_f() == location;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Pointer, public:

Pointer::Pointer(PointerDelegate* delegate)
    : delegate_(delegate),
      surface_(nullptr),
      focus_(nullptr),
      cursor_scale_(1.0f) {
  ash::Shell* ash_shell = ash::Shell::GetInstance();
  ash_shell->AddPreTargetHandler(this);

  wm::CursorManager* cursor_manager = ash_shell->cursor_manager();
  DCHECK(cursor_manager);
  cursor_manager->AddObserver(this);
}

Pointer::~Pointer() {
  delegate_->OnPointerDestroying(this);
  if (stylus_delegate_)
    stylus_delegate_->OnPointerDestroying(this);
  if (surface_)
    surface_->RemoveSurfaceObserver(this);
  if (focus_) {
    focus_->RemoveSurfaceObserver(this);
    focus_->UnregisterCursorProvider(this);
  }
  if (widget_)
    widget_->CloseNow();

  ash::Shell* ash_shell = ash::Shell::GetInstance();
  DCHECK(ash_shell->cursor_manager());
  ash_shell->cursor_manager()->RemoveObserver(this);
  ash_shell->RemovePreTargetHandler(this);
}

void Pointer::SetCursor(Surface* surface, const gfx::Point& hotspot) {
  // Early out if the pointer doesn't have a surface in focus.
  if (!focus_)
    return;

  // If surface is different than the current pointer surface then remove the
  // current surface and add the new surface.
  if (surface != surface_) {
    if (surface && surface->HasSurfaceDelegate()) {
      DLOG(ERROR) << "Surface has already been assigned a role";
      return;
    }
    if (surface_) {
      widget_->GetNativeWindow()->RemoveChild(surface_->window());
      surface_->window()->Hide();
      surface_->SetSurfaceDelegate(nullptr);
      surface_->RemoveSurfaceObserver(this);
    }
    surface_ = surface;
    if (surface_) {
      surface_->SetSurfaceDelegate(this);
      surface_->AddSurfaceObserver(this);
      widget_->GetNativeWindow()->AddChild(surface_->window());
    }
  }

  // Update hotspot and show cursor surface if not already visible.
  hotspot_ = hotspot;
  if (surface_) {
    surface_->window()->SetBounds(
        gfx::Rect(gfx::Point() - hotspot_.OffsetFromOrigin(),
                  surface_->window()->layer()->size()));
    if (!surface_->window()->IsVisible())
      surface_->window()->Show();

    // Show widget now that cursor has been defined.
    if (!widget_->IsVisible())
      widget_->Show();
  }

  // Register pointer as cursor provider now that the cursor for |focus_| has
  // been defined.
  focus_->RegisterCursorProvider(this);

  // Update cursor in case the registration of pointer as cursor provider
  // caused the cursor to change.
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(focus_->window()->GetRootWindow());
  if (cursor_client)
    cursor_client->SetCursor(
        focus_->window()->GetCursor(gfx::ToFlooredPoint(location_)));
}

void Pointer::SetStylusDelegate(PointerStylusDelegate* delegate) {
  stylus_delegate_ = delegate;
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void Pointer::OnMouseEvent(ui::MouseEvent* event) {
  Surface* target = GetEffectiveTargetForEvent(event);

  // If target is different than the current pointer focus then we need to
  // generate enter and leave events.
  if (target != focus_) {
    // First generate a leave event if we currently have a target in focus.
    if (focus_) {
      delegate_->OnPointerLeave(focus_);
      focus_->RemoveSurfaceObserver(this);
      // Require SetCursor() to be called and cursor to be re-defined in
      // response to each OnPointerEnter() call.
      focus_->UnregisterCursorProvider(this);
      focus_ = nullptr;
    }
    // Second generate an enter event if focus moved to a new target.
    if (target) {
      delegate_->OnPointerEnter(target, event->location_f(),
                                event->button_flags());
      location_ = event->location_f();
      // Defaulting pointer_type to POINTER_TYPE_MOUSE prevents the tool change
      // event from being fired when using a mouse.
      pointer_type_ = ui::EventPointerType::POINTER_TYPE_MOUSE;

      focus_ = target;
      focus_->AddSurfaceObserver(this);
    }
    delegate_->OnPointerFrame();
  }

  // Report changes in pointer type. We treat unknown devices as a mouse.
  auto new_pointer_type = event->pointer_details().pointer_type;
  if (new_pointer_type == ui::EventPointerType::POINTER_TYPE_UNKNOWN)
    new_pointer_type = ui::EventPointerType::POINTER_TYPE_MOUSE;
  if (focus_ && stylus_delegate_ && new_pointer_type != pointer_type_) {
    stylus_delegate_->OnPointerToolChange(new_pointer_type);
    pointer_type_ = new_pointer_type;
  }

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_RELEASED:
      if (focus_) {
        delegate_->OnPointerButton(event->time_stamp(),
                                   event->changed_button_flags(),
                                   event->type() == ui::ET_MOUSE_PRESSED);
        delegate_->OnPointerFrame();
      }
      break;
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED:
      if (focus_) {
        bool send_frame = false;
        // Generate motion event if location changed. We need to check location
        // here as mouse movement can generate both "moved" and "entered" events
        // but OnPointerMotion should only be called if location changed since
        // OnPointerEnter was called.
        if (!SameLocation(event, location_)) {
          location_ = event->location_f();
          delegate_->OnPointerMotion(event->time_stamp(), location_);
          send_frame = true;
        }
        if (stylus_delegate_ &&
            pointer_type_ != ui::EventPointerType::POINTER_TYPE_MOUSE) {
          constexpr float kEpsilon = std::numeric_limits<float>::epsilon();
          gfx::Vector2dF new_tilt = gfx::Vector2dF(
              event->pointer_details().tilt_x, event->pointer_details().tilt_y);
          if (std::abs(new_tilt.x() - tilt_.x()) > kEpsilon ||
              std::abs(new_tilt.y() - tilt_.y()) > kEpsilon) {
            tilt_ = new_tilt;
            stylus_delegate_->OnPointerTilt(event->time_stamp(), new_tilt);
            send_frame = true;
          }

          float new_force = event->pointer_details().force;
          if (std::abs(new_force - force_) > kEpsilon) {
            force_ = new_force;
            stylus_delegate_->OnPointerForce(event->time_stamp(), new_force);
            send_frame = true;
          }
        }
        if (send_frame)
          delegate_->OnPointerFrame();
      }
      break;
    case ui::ET_SCROLL:
      if (focus_) {
        ui::ScrollEvent* scroll_event = static_cast<ui::ScrollEvent*>(event);
        delegate_->OnPointerScroll(
            event->time_stamp(),
            gfx::Vector2dF(scroll_event->x_offset(), scroll_event->y_offset()),
            false);
        delegate_->OnPointerFrame();
      }
      break;
    case ui::ET_MOUSEWHEEL:
      if (focus_) {
        delegate_->OnPointerScroll(
            event->time_stamp(),
            static_cast<ui::MouseWheelEvent*>(event)->offset(), true);
        delegate_->OnPointerFrame();
      }
      break;
    case ui::ET_SCROLL_FLING_START:
      if (focus_) {
        delegate_->OnPointerScrollStop(event->time_stamp());
        delegate_->OnPointerFrame();
      }
      break;
    case ui::ET_SCROLL_FLING_CANCEL:
      if (focus_) {
        delegate_->OnPointerScrollCancel(event->time_stamp());
        delegate_->OnPointerFrame();
      }
      break;
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSE_CAPTURE_CHANGED:
      break;
    default:
      NOTREACHED();
      break;
  }

  // Update cursor widget to reflect current focus and pointer location.
  if (focus_) {
    if (!widget_)
      CreatePointerWidget();

    // Update cursor location if mouse event caused it to change.
    gfx::Point mouse_location = aura::Env::GetInstance()->last_mouse_location();
    if (mouse_location != widget_->GetNativeWindow()->bounds().origin()) {
      gfx::Rect bounds = widget_->GetNativeWindow()->bounds();
      bounds.set_origin(mouse_location);
      widget_->GetNativeWindow()->SetBounds(bounds);
    }

    UpdateCursorScale();
  } else {
    if (widget_ && widget_->IsVisible())
      widget_->Hide();
  }
}

void Pointer::OnScrollEvent(ui::ScrollEvent* event) {
  OnMouseEvent(event);
}

void Pointer::OnCursorSetChanged(ui::CursorSetType cursor_set) {
  UpdateCursorScale();
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

void Pointer::OnSurfaceCommit() {
  surface_->CheckIfSurfaceHierarchyNeedsCommitToNewSurfaces();
  surface_->CommitSurfaceHierarchy();
  surface_->window()->SetBounds(
      gfx::Rect(gfx::Point() - hotspot_.OffsetFromOrigin(),
                surface_->window()->layer()->size()));
}

bool Pointer::IsSurfaceSynchronized() const {
  // A pointer surface is always desynchronized.
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void Pointer::OnSurfaceDestroying(Surface* surface) {
  DCHECK(surface == surface_ || surface == focus_);
  if (surface == surface_)
    surface_ = nullptr;
  if (surface == focus_)
    focus_ = nullptr;
  surface->RemoveSurfaceObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// Pointer, private:

void Pointer::CreatePointerWidget() {
  DCHECK(!widget_);

  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_TOOLTIP;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.shadow_type = views::Widget::InitParams::SHADOW_TYPE_NONE;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = false;
  params.parent =
      ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                               ash::kShellWindowId_MouseCursorContainer);
  widget_.reset(new views::Widget);
  widget_->Init(params);
  widget_->GetNativeWindow()->set_owned_by_parent(false);
  widget_->GetNativeWindow()->SetName("ExoPointer");
}

Surface* Pointer::GetEffectiveTargetForEvent(ui::Event* event) const {
  Surface* target =
      Surface::AsSurface(static_cast<aura::Window*>(event->target()));
  if (!target)
    return nullptr;

  return delegate_->CanAcceptPointerEventsForSurface(target) ? target : nullptr;
}

void Pointer::UpdateCursorScale() {
  if (!focus_)
    return;

  // Update cursor scale if the effective UI scale has changed.
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget_->GetNativeWindow());
  float ui_scale = ash::Shell::GetInstance()
                       ->display_manager()
                       ->GetDisplayInfo(display.id())
                       .GetEffectiveUIScale();

  ash::Shell* ash_shell = ash::Shell::GetInstance();
  if (ash_shell->cursor_manager()->GetCursorSet() == ui::CURSOR_SET_LARGE)
    ui_scale *= kLargeCursorScale;

  if (ui_scale != cursor_scale_) {
    gfx::Transform transform;
    transform.Scale(ui_scale, ui_scale);
    widget_->GetNativeWindow()->SetTransform(transform);
    cursor_scale_ = ui_scale;
  }
}

}  // namespace exo
