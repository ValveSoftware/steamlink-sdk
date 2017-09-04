// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/pointer.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "components/exo/pointer_delegate.h"
#include "components/exo/pointer_stylus_delegate.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
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
  auto* helper = WMHelper::GetInstance();
  helper->AddPreTargetHandler(this);
  helper->AddCursorObserver(this);
}

Pointer::~Pointer() {
  delegate_->OnPointerDestroying(this);
  if (surface_)
    surface_->RemoveSurfaceObserver(this);
  if (focus_) {
    focus_->RemoveSurfaceObserver(this);
    focus_->UnregisterCursorProvider(this);
  }
  if (widget_)
    widget_->CloseNow();

  auto* helper = WMHelper::GetInstance();
  helper->RemoveCursorObserver(this);
  helper->RemovePreTargetHandler(this);
}

void Pointer::SetCursor(Surface* surface, const gfx::Point& hotspot) {
  // Early out if the pointer doesn't have a surface in focus.
  if (!focus_)
    return;

  if (!widget_)
    CreatePointerWidget();

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

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void Pointer::OnMouseEvent(ui::MouseEvent* event) {
  Surface* target = GetEffectiveTargetForEvent(event);

  if (event->flags() & ui::EF_TOUCH_ACCESSIBILITY)
    return;

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
      focus_ = target;
      focus_->AddSurfaceObserver(this);
    }
    delegate_->OnPointerFrame();
  }

  if (focus_ && event->IsMouseEvent() && event->type() != ui::ET_MOUSE_EXITED) {
    // Generate motion event if location changed. We need to check location
    // here as mouse movement can generate both "moved" and "entered" events
    // but OnPointerMotion should only be called if location changed since
    // OnPointerEnter was called.
    if (!SameLocation(event, location_)) {
      location_ = event->location_f();
      delegate_->OnPointerMotion(event->time_stamp(), location_);
      delegate_->OnPointerFrame();
    }
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
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED:
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
    gfx::Rect bounds = widget_->GetWindowBoundsInScreen();
    if (mouse_location != bounds.origin()) {
      bounds.set_origin(mouse_location);
      widget_->SetBounds(bounds);
    }

    UpdateCursorScale();
    if (!widget_->IsVisible())
      widget_->Show();
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
  params.parent = WMHelper::GetInstance()->GetContainer(
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
  float ui_scale = WMHelper::GetInstance()
                       ->GetDisplayInfo(display.id())
                       .GetEffectiveUIScale();
  if (WMHelper::GetInstance()->GetCursorSet() == ui::CURSOR_SET_LARGE)
    ui_scale *= kLargeCursorScale;

  if (ui_scale != cursor_scale_) {
    gfx::Transform transform;
    transform.Scale(ui_scale, ui_scale);
    widget_->GetNativeWindow()->SetTransform(transform);
    cursor_scale_ = ui_scale;
  }
}

}  // namespace exo
