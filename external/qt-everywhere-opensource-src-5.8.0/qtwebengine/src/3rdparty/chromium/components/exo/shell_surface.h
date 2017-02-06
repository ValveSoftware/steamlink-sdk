// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SHELL_SURFACE_H_
#define COMPONENTS_EXO_SHELL_SURFACE_H_

#include <deque>
#include <memory>
#include <string>

#include "ash/common/wm/window_state_observer.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/surface_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {
class WindowResizer;
}

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace exo {
class Surface;

// This class provides functions for treating a surfaces like toplevel,
// fullscreen or popup widgets, move, resize or maximize them, associate
// metadata like title and class, etc.
class ShellSurface : public SurfaceDelegate,
                     public SurfaceObserver,
                     public views::WidgetDelegate,
                     public views::View,
                     public ash::wm::WindowStateObserver,
                     public aura::WindowObserver,
                     public aura::client::ActivationChangeObserver {
 public:
  ShellSurface(Surface* surface,
               ShellSurface* parent,
               const gfx::Rect& initial_bounds,
               bool activatable,
               int container);
  explicit ShellSurface(Surface* surface);
  ~ShellSurface() override;

  // Set the callback to run when the user wants the shell surface to be closed.
  // The receiver can chose to not close the window on this signal.
  void set_close_callback(const base::Closure& close_callback) {
    close_callback_ = close_callback;
  }

  // Set the callback to run when the surface is destroyed.
  void set_surface_destroyed_callback(
      const base::Closure& surface_destroyed_callback) {
    surface_destroyed_callback_ = surface_destroyed_callback;
  }

  // Set the callback to run when the surface state changed.
  using StateChangedCallback =
      base::Callback<void(ash::wm::WindowStateType old_state_type,
                          ash::wm::WindowStateType new_state_type)>;
  void set_state_changed_callback(
      const StateChangedCallback& state_changed_callback) {
    state_changed_callback_ = state_changed_callback;
  }

  // Set the callback to run when the client is asked to configure the surface.
  // The size is a hint, in the sense that the client is free to ignore it if
  // it doesn't resize, pick a smaller size (to satisfy aspect ratio or resize
  // in steps of NxM pixels).
  using ConfigureCallback =
      base::Callback<uint32_t(const gfx::Size& size,
                              ash::wm::WindowStateType state_type,
                              bool resizing,
                              bool activated)>;
  void set_configure_callback(const ConfigureCallback& configure_callback) {
    configure_callback_ = configure_callback;
  }

  // When the client is asked to configure the surface, it should acknowledge
  // the configure request sometime before the commit. |serial| is the serial
  // from the configure callback.
  void AcknowledgeConfigure(uint32_t serial);

  // Set the "parent" of this surface. This window should be stacked above a
  // parent.
  void SetParent(ShellSurface* parent);

  // Activates the shell surface.
  void Activate();

  // Maximizes the shell surface.
  void Maximize();

  // Minimize the shell surface.
  void Minimize();

  // Restore the shell surface.
  void Restore();

  // Set fullscreen state for shell surface.
  void SetFullscreen(bool fullscreen);

  // Pins the shell surface.
  void SetPinned(bool pinned);

  // Set title for surface.
  void SetTitle(const base::string16& title);

  // Sets the system modality.
  void SetSystemModal(bool system_modal);

  // Sets the application ID for the window. The application ID identifies the
  // general class of applications to which the window belongs.
  static void SetApplicationId(aura::Window* window,
                               std::string* application_id);
  static const std::string GetApplicationId(aura::Window* window);

  // Set application id for surface.
  void SetApplicationId(const std::string& application_id);

  // Start an interactive move of surface.
  void Move();

  // Start an interactive resize of surface. |component| is one of the windows
  // HT constants (see ui/base/hit_test.h) and describes in what direction the
  // surface should be resized.
  void Resize(int component);

  // Signal a request to close the window. It is up to the implementation to
  // actually decide to do so though.
  void Close();

  // Set geometry for surface. The geometry represents the "visible bounds"
  // for the surface from the user's perspective.
  void SetGeometry(const gfx::Rect& geometry);

  // Set the content bounds for the shadow. Empty bounds will delete
  // the shadow.
  void SetRectangularShadow(const gfx::Rect& content_bounds);

  // Set the pacity of the background for the window that has a shadow.
  void SetRectangularShadowBackgroundOpacity(float opacity);

  // Set scale factor for surface. The scale factor will be applied to surface
  // and all descendants.
  void SetScale(double scale);

  // Set top inset for surface.
  void SetTopInset(int height);

  // Sets the main surface for the window.
  static void SetMainSurface(aura::Window* window, Surface* surface);

  // Returns the main Surface instance or nullptr if it is not set.
  // |window| must not be nullptr.
  static Surface* GetMainSurface(const aura::Window* window);

  // Returns a trace value representing the state of the surface.
  std::unique_ptr<base::trace_event::TracedValue> AsTracedValue() const;

  // Overridden from SurfaceDelegate:
  void OnSurfaceCommit() override;
  bool IsSurfaceSynchronized() const override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

  // Overridden from views::WidgetDelegate:
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  base::string16 GetWindowTitle() const override;
  void WindowClosing() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  views::View* GetContentsView() override;
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  bool WidgetHasHitTestMask() const override;
  void GetWidgetHitTestMask(gfx::Path* mask) const override;

  // Overridden from views::View:
  gfx::Size GetPreferredSize() const override;

  // Overridden from ash::wm::WindowStateObserver:
  void OnPreWindowStateTypeChange(ash::wm::WindowState* window_state,
                                  ash::wm::WindowStateType old_type) override;
  void OnPostWindowStateTypeChange(ash::wm::WindowState* window_state,
                                   ash::wm::WindowStateType old_type) override;

  // Overridden from aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowDestroying(aura::Window* window) override;

  // Overridden from aura::client::ActivationChangeObserver:
  void OnWindowActivated(
      aura::client::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // Overridden from ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  const aura::Window* shadow_underlay_for_test() const {
    return shadow_underlay_;
  }

 private:
  class ScopedConfigure;
  class ScopedAnimationsDisabled;

  // Surface state associated with each configure request.
  struct Config {
    uint32_t serial;
    gfx::Vector2d origin_offset;
    int resize_component;
  };

  // Creates the |widget_| for |surface_|. |show_state| is the initial state
  // of the widget (e.g. maximized).
  void CreateShellSurfaceWidget(ui::WindowShowState show_state);

  // Asks the client to configure its surface.
  void Configure();

  // Attempt to start a drag operation. The type of drag operation to start is
  // determined by |component|.
  void AttemptToStartDrag(int component);

  // End current drag operation.
  void EndDrag(bool revert);

  // Returns true if surface is currently being resized.
  bool IsResizing() const;

  // Returns the "visible bounds" for the surface from the user's perspective.
  gfx::Rect GetVisibleBounds() const;

  // Returns the origin for the surface taking visible bounds and current
  // resize direction into account.
  gfx::Point GetSurfaceOrigin() const;

  // Updates the bounds of widget to match the current surface bounds.
  void UpdateWidgetBounds();

  // Creates, deletes and update the shadow bounds based on
  // |pending_shadow_content_bounds_|.
  void UpdateShadow();

  views::Widget* widget_ = nullptr;
  Surface* surface_;
  aura::Window* parent_;
  const gfx::Rect initial_bounds_;
  const bool activatable_;
  // Container Window Id (see ash/common/shell_window_ids.h)
  const int container_;
  bool pending_show_widget_ = false;
  base::string16 title_;
  std::string application_id_;
  gfx::Rect geometry_;
  gfx::Rect pending_geometry_;
  double scale_ = 1.0;
  double pending_scale_ = 1.0;
  base::Closure close_callback_;
  base::Closure surface_destroyed_callback_;
  StateChangedCallback state_changed_callback_;
  ConfigureCallback configure_callback_;
  ScopedConfigure* scoped_configure_ = nullptr;
  bool ignore_window_bounds_changes_ = false;
  gfx::Point origin_;
  gfx::Vector2d pending_origin_offset_;
  gfx::Vector2d pending_origin_config_offset_;
  int resize_component_ = HTCAPTION;  // HT constant (see ui/base/hit_test.h)
  int pending_resize_component_ = HTCAPTION;
  aura::Window* shadow_overlay_ = nullptr;
  aura::Window* shadow_underlay_ = nullptr;
  gfx::Rect shadow_content_bounds_;
  std::deque<Config> pending_configs_;
  std::unique_ptr<ash::WindowResizer> resizer_;
  std::unique_ptr<ScopedAnimationsDisabled> scoped_animations_disabled_;
  int top_inset_height_ = 0;
  int pending_top_inset_height_ = 0;
  float rectangular_shadow_background_opacity_ = 1.0;

  DISALLOW_COPY_AND_ASSIGN(ShellSurface);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SHELL_SURFACE_H_
