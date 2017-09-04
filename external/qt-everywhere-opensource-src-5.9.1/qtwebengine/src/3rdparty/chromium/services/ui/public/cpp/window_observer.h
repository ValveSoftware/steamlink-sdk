// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_WINDOW_OBSERVER_H_
#define SERVICES_UI_PUBLIC_CPP_WINDOW_OBSERVER_H_

#include <stdint.h>

#include <vector>

#include "services/ui/public/cpp/window.h"

namespace ui {

class Window;

// A note on -ing and -ed suffixes:
//
// -ing methods are called before changes are applied to the local window model.
// -ed methods are called after changes are applied to the local window model.
//
// If the change originated from another connection to the window manager, it's
// possible that the change has already been applied to the service-side model
// prior to being called, so for example in the case of OnWindowDestroying(),
// it's possible the window has already been destroyed on the service side.

class WindowObserver {
 public:
  struct TreeChangeParams {
    TreeChangeParams();
    // The window whose parent changed or is changing.
    Window* target;
    Window* old_parent;
    Window* new_parent;

    // TreeChangeParams is supplied as an argument to various WindowObserver
    // functions. |receiver| is the window being observed by the receipient of
    // this notification, which may equal any of the struct members above.
    // TODO(sky): move this outside of TreeChangeParams.
    Window* receiver;
  };

  virtual void OnTreeChanging(const TreeChangeParams& params) {}
  virtual void OnTreeChanged(const TreeChangeParams& params) {}

  virtual void OnWindowReordering(Window* window,
                                  Window* relative_window,
                                  mojom::OrderDirection direction) {}
  virtual void OnWindowReordered(Window* window,
                                 Window* relative_window,
                                 mojom::OrderDirection direction) {}

  virtual void OnWindowDestroying(Window* window) {}
  virtual void OnWindowDestroyed(Window* window) {}

  virtual void OnWindowBoundsChanging(Window* window,
                                      const gfx::Rect& old_bounds,
                                      const gfx::Rect& new_bounds) {}
  virtual void OnWindowBoundsChanged(Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) {}
  virtual void OnWindowLostCapture(Window* window) {}
  virtual void OnWindowClientAreaChanged(
      Window* window,
      const gfx::Insets& old_client_area,
      const std::vector<gfx::Rect>& old_additional_client_areas) {}

  virtual void OnWindowFocusChanged(Window* gained_focus, Window* lost_focus) {}

  virtual void OnWindowPredefinedCursorChanged(Window* window,
                                               mojom::Cursor cursor) {}

  // Changing the visibility of a window results in the following sequence of
  // functions being called:
  // . OnWindowVisibilityChanging(): called on observers added to the window
  //    whose visibility is changing. This is called before the visibility has
  //    changed internally.
  // The following are called after the visibility changes:
  // . OnChildWindowVisibilityChanged(): called on observers added to the
  //   parent of the window whose visibility changed. This function is generally
  //   intended for layout managers that need to do processing before
  //   OnWindowVisibilityChanged() is called on observers of the window.
  // . OnWindowVisibilityChanged(): called on observers added to the window
  //   whose visibility changed, as well as observers added to all ancestors and
  //   all descendants of the window.
  virtual void OnWindowVisibilityChanging(Window* window, bool visible) {}
  virtual void OnChildWindowVisibilityChanged(Window* window, bool visible) {}
  virtual void OnWindowVisibilityChanged(Window* window, bool visible) {}

  virtual void OnWindowOpacityChanged(Window* window,
                                      float old_opacity,
                                      float new_opacity) {}

  // Invoked when this Window's shared properties have changed. This can either
  // be caused by SetSharedProperty() being called locally, or by us receiving
  // a mojo message that this property has changed. If this property has been
  // added, |old_data| is null. If this property was removed, |new_data| is
  // null.
  virtual void OnWindowSharedPropertyChanged(
      Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) {}

  // Invoked when SetProperty() or ClearProperty() is called on the window.
  // |key| is either a WindowProperty<T>* (SetProperty, ClearProperty). Either
  // way, it can simply be compared for equality with the property
  // constant. |old| is the old property value, which must be cast to the
  // appropriate type before use.
  virtual void OnWindowLocalPropertyChanged(Window* window,
                                            const void* key,
                                            intptr_t old) {}

  virtual void OnWindowEmbeddedAppDisconnected(Window* window) {}

  // Sent when the drawn state changes. This is only sent for the root nodes
  // when embedded.
  virtual void OnWindowDrawnChanging(Window* window) {}
  virtual void OnWindowDrawnChanged(Window* window) {}

  virtual void OnTransientChildAdded(Window* window, Window* transient) {}
  virtual void OnTransientChildRemoved(Window* window, Window* transient) {}

  // The WindowManager has requested the window to close. If the observer
  // allows the close it should destroy the window as appropriate.
  virtual void OnRequestClose(Window* window) {}

 protected:
  virtual ~WindowObserver() {}
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_WINDOW_OBSERVER_H_
