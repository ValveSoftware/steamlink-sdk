// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_CUSTOM_NOTIFICATION_VIEW_H_
#define UI_ARC_NOTIFICATION_ARC_CUSTOM_NOTIFICATION_VIEW_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "ui/arc/notification/arc_custom_notification_item.h"
#include "ui/arc/notification/arc_notification_surface_manager.h"
#include "ui/aura/window_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/native/native_view_host.h"

namespace exo {
class NotificationSurface;
}

namespace views {
class ImageButton;
class Widget;
}

namespace arc {

class ArcCustomNotificationView
    : public views::NativeViewHost,
      public views::ButtonListener,
      public aura::WindowObserver,
      public ArcCustomNotificationItem::Observer,
      public ArcNotificationSurfaceManager::Observer {
 public:
  explicit ArcCustomNotificationView(ArcCustomNotificationItem* item);
  ~ArcCustomNotificationView() override;

 private:
  class EventForwarder;
  class SlideHelper;

  void CreateFloatingCloseButton();
  void SetSurface(exo::NotificationSurface* surface);
  void UpdatePreferredSize();
  void UpdateCloseButtonVisiblity();
  void UpdatePinnedState();
  void UpdateSnapshot();
  void AttachSurface();

  // views::NativeViewHost
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // views::ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // aura::WindowObserver
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowDestroying(aura::Window* window) override;

  // ArcCustomNotificationItem::Observer
  void OnItemDestroying() override;
  void OnItemUpdated() override;

  // ArcNotificationSurfaceManager::Observer:
  void OnNotificationSurfaceAdded(exo::NotificationSurface* surface) override;
  void OnNotificationSurfaceRemoved(exo::NotificationSurface* surface) override;

  ArcCustomNotificationItem* item_ = nullptr;
  exo::NotificationSurface* surface_ = nullptr;

  const std::string notification_key_;

  // A pre-target event handler to forward events on the surface to this view.
  // Using a pre-target event handler instead of a target handler on the surface
  // window because it has descendant aura::Window and the events on them need
  // to be handled as well.
  // TODO(xiyuan): Revisit after exo::Surface no longer has an aura::Window.
  std::unique_ptr<EventForwarder> event_forwarder_;

  // A helper to observe slide transform/animation and use surface layer copy
  // when a slide is in progress and restore the surface when it finishes.
  std::unique_ptr<SlideHelper> slide_helper_;

  // A close button on top of NotificationSurface. Needed because the
  // aura::Window of NotificationSurface is added after hosting widget's
  // RootView thus standard notification close button is always below
  // it.
  std::unique_ptr<views::Widget> floating_close_button_widget_;

  views::ImageButton* floating_close_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ArcCustomNotificationView);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_CUSTOM_NOTIFICATION_VIEW_H_
