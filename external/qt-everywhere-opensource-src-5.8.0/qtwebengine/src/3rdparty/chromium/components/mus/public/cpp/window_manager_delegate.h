// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_WINDOW_MANAGER_DELEGATE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_WINDOW_MANAGER_DELEGATE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "components/mus/public/interfaces/cursor.mojom.h"
#include "components/mus/public/interfaces/event_matcher.mojom.h"
#include "components/mus/public/interfaces/window_manager_constants.mojom.h"
#include "ui/events/mojo/event.mojom.h"

namespace display {
class Display;
}

namespace gfx {
class Insets;
class Rect;
class Vector2d;
}

namespace ui {
class Event;
}

namespace mus {

class Window;

// See the mojom with the same name for details on the functions in this
// interface.
class WindowManagerClient {
 public:
  virtual void SetFrameDecorationValues(
      mojom::FrameDecorationValuesPtr values) = 0;
  virtual void SetNonClientCursor(Window* window,
                                  mojom::Cursor non_client_cursor) = 0;

  virtual void AddAccelerator(uint32_t id,
                              mojom::EventMatcherPtr event_matcher,
                              const base::Callback<void(bool)>& callback) = 0;
  virtual void RemoveAccelerator(uint32_t id) = 0;
  virtual void AddActivationParent(Window* window) = 0;
  virtual void RemoveActivationParent(Window* window) = 0;
  virtual void ActivateNextWindow() = 0;
  virtual void SetUnderlaySurfaceOffsetAndExtendedHitArea(
      Window* window,
      const gfx::Vector2d& offset,
      const gfx::Insets& hit_area) = 0;

 protected:
  virtual ~WindowManagerClient() {}
};

// Used by clients implementing a window manager.
// TODO(sky): this should be called WindowManager, but that's rather confusing
// currently.
class WindowManagerDelegate {
 public:
  // Called once to give the delegate access to functions only exposed to
  // the WindowManager.
  virtual void SetWindowManagerClient(WindowManagerClient* client) = 0;

  // A client requested the bounds of |window| to change to |bounds|. Return
  // true if the bounds are allowed to change. A return value of false
  // indicates the change is not allowed.
  // NOTE: This should not change the bounds of |window|. Instead return the
  // bounds the window should be in |bounds|.
  virtual bool OnWmSetBounds(Window* window, gfx::Rect* bounds) = 0;

  // A client requested the shared property named |name| to change to
  // |new_data|. Return true to allow the change to |new_data|, false
  // otherwise.
  virtual bool OnWmSetProperty(
      Window* window,
      const std::string& name,
      std::unique_ptr<std::vector<uint8_t>>* new_data) = 0;

  // A client has requested a new top level window. The delegate should create
  // and parent the window appropriately and return it. |properties| is the
  // supplied properties from the client requesting the new window. The
  // delegate may modify |properties| before calling NewWindow(), but the
  // delegate does *not* own |properties|, they are valid only for the life
  // of OnWmCreateTopLevelWindow().
  virtual Window* OnWmCreateTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties) = 0;

  // Called when a Mus client's jankiness changes. |windows| is the set of
  // windows owned by the window manager in which the client is embedded.
  virtual void OnWmClientJankinessChanged(
      const std::set<Window*>& client_windows,
      bool janky) = 0;

  // Called when a display is added. |window| is the root of the window tree for
  // the specified display.
  virtual void OnWmNewDisplay(Window* window,
                              const display::Display& display) = 0;

  virtual void OnAccelerator(uint32_t id, const ui::Event& event) = 0;

 protected:
  virtual ~WindowManagerDelegate() {}
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_WINDOW_MANAGER_DELEGATE_H_
