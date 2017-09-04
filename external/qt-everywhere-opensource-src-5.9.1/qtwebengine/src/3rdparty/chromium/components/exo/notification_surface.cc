// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/notification_surface.h"

#include "components/exo/notification_surface_manager.h"
#include "components/exo/surface.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace exo {

namespace {

class CustomWindowDelegate : public aura::WindowDelegate {
 public:
  explicit CustomWindowDelegate(Surface* surface) : surface_(surface) {}
  ~CustomWindowDelegate() override {}

  // Overridden from aura::WindowDelegate:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  gfx::Size GetMaximumSize() const override { return gfx::Size(); }
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {}
  gfx::NativeCursor GetCursor(const gfx::Point& point) override {
    // If surface has a cursor provider then return 'none' as cursor providers
    // are responsible for drawing cursors. Use default cursor if no cursor
    // provider is registered.
    return surface_->HasCursorProvider() ? ui::kCursorNone : ui::kCursorNull;
  }
  int GetNonClientComponent(const gfx::Point& point) const override {
    return HTNOWHERE;
  }
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override {
    return true;
  }
  bool CanFocus() override { return true; }
  void OnCaptureLost() override {}
  void OnPaint(const ui::PaintContext& context) override {}
  void OnDeviceScaleFactorChanged(float device_scale_factor) override {}
  void OnWindowDestroying(aura::Window* window) override {}
  void OnWindowDestroyed(aura::Window* window) override { delete this; }
  void OnWindowTargetVisibilityChanged(bool visible) override {}
  bool HasHitTestMask() const override { return surface_->HasHitTestMask(); }
  void GetHitTestMask(gfx::Path* mask) const override {
    surface_->GetHitTestMask(mask);
  }
  void OnKeyEvent(ui::KeyEvent* event) override {
    // Propagates the key event upto the top-level views Widget so that we can
    // trigger proper events in the views/ash level there. Event handling for
    // Surfaces is done in a post event handler in keyboard.cc.
    views::Widget* widget =
        views::Widget::GetTopLevelWidgetForNativeView(surface_->window());
    if (widget)
      widget->OnKeyEvent(event);
  }

 private:
  Surface* const surface_;

  DISALLOW_COPY_AND_ASSIGN(CustomWindowDelegate);
};

}  // namespace

NotificationSurface::NotificationSurface(NotificationSurfaceManager* manager,
                                         Surface* surface,
                                         const std::string& notification_id)
    : manager_(manager),
      surface_(surface),
      notification_id_(notification_id),
      window_(new aura::Window(new CustomWindowDelegate(surface))) {
  surface_->SetSurfaceDelegate(this);
  surface_->AddSurfaceObserver(this);

  window_->SetType(ui::wm::WINDOW_TYPE_CONTROL);
  window_->SetName("ExoNotificationSurface");
  window_->Init(ui::LAYER_NOT_DRAWN);
  window_->set_owned_by_parent(false);

  // TODO(xiyuan): Fix after Surface no longer has an aura::Window.
  window_->AddChild(surface_->window());
  surface_->window()->Show();
}

NotificationSurface::~NotificationSurface() {
  if (surface_) {
    surface_->SetSurfaceDelegate(nullptr);
    surface_->RemoveSurfaceObserver(this);
  }
  if (added_to_manager_)
    manager_->RemoveSurface(this);
}

gfx::Size NotificationSurface::GetSize() const {
  return surface_->content_size();
}

void NotificationSurface::OnSurfaceCommit() {
  surface_->CheckIfSurfaceHierarchyNeedsCommitToNewSurfaces();
  surface_->CommitSurfaceHierarchy();

  gfx::Rect bounds = window_->bounds();
  if (bounds.size() != surface_->content_size()) {
    bounds.set_size(surface_->content_size());
    window_->SetBounds(bounds);
  }

  // Defer AddSurface until there are contents to show.
  if (!added_to_manager_ && !surface_->content_size().IsEmpty()) {
    added_to_manager_ = true;
    manager_->AddSurface(this);
  }
}

bool NotificationSurface::IsSurfaceSynchronized() const {
  return false;
}

void NotificationSurface::OnSurfaceDestroying(Surface* surface) {
  window_.reset();
  surface->RemoveSurfaceObserver(this);
  surface_ = nullptr;
}

}  // namespace exo
