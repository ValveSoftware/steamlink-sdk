// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_AURA_DEMO_WINDOW_TREE_HOST_VIEW_MANAGER_H_
#define MOJO_EXAMPLES_AURA_DEMO_WINDOW_TREE_HOST_VIEW_MANAGER_H_

#include "mojo/services/public/cpp/view_manager/node_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_source.h"
#include "ui/gfx/geometry/rect.h"

class SkBitmap;

namespace ui {
class Compositor;
}

namespace mojo {

class WindowTreeHostMojoDelegate;

class WindowTreeHostMojo : public aura::WindowTreeHost,
                           public ui::EventSource,
                           public view_manager::NodeObserver {
 public:
  WindowTreeHostMojo(view_manager::Node* node,
                     WindowTreeHostMojoDelegate* delegate);
  virtual ~WindowTreeHostMojo();

  // Returns the WindowTreeHostMojo for the specified compositor.
  static WindowTreeHostMojo* ForCompositor(ui::Compositor* compositor);

  const gfx::Rect& bounds() const { return bounds_; }

  // Sets the contents to show in this WindowTreeHost. This forwards to the
  // delegate.
  void SetContents(const SkBitmap& contents);

  ui::EventDispatchDetails SendEventToProcessor(ui::Event* event) {
    return ui::EventSource::SendEventToProcessor(event);
  }

 private:
  // WindowTreeHost:
  virtual ui::EventSource* GetEventSource() OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual gfx::Point GetLocationOnNativeScreen() const OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;
  virtual void PostNativeEvent(const base::NativeEvent& native_event) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual void SetCursorNative(gfx::NativeCursor cursor) OVERRIDE;
  virtual void MoveCursorToNative(const gfx::Point& location) OVERRIDE;
  virtual void OnCursorVisibilityChangedNative(bool show) OVERRIDE;

  // ui::EventSource:
  virtual ui::EventProcessor* GetEventProcessor() OVERRIDE;

  // view_manager::NodeObserver:
  virtual void OnNodeBoundsChange(
      view_manager::Node* node,
      const gfx::Rect& old_bounds,
      const gfx::Rect& new_bounds,
      view_manager::NodeObserver::DispositionChangePhase phase) OVERRIDE;

  view_manager::Node* node_;

  gfx::Rect bounds_;

  WindowTreeHostMojoDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostMojo);
};

}  // namespace mojo

#endif  // MOJO_EXAMPLES_AURA_DEMO_WINDOW_TREE_HOST_VIEW_MANAGER_H_
