// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_USER_DISPLAY_MANAGER_H_
#define COMPONENTS_MUS_WS_USER_DISPLAY_MANAGER_H_

#include <set>

#include "base/atomicops.h"
#include "base/macros.h"
#include "components/mus/public/interfaces/display.mojom.h"
#include "components/mus/ws/user_id.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace gfx {
class Point;
}

namespace mus {
namespace ws {

class Display;
class DisplayManager;
class DisplayManagerDelegate;

namespace test {
class UserDisplayManagerTestApi;
}

// Provides per user display state.
class UserDisplayManager : public mojom::DisplayManager {
 public:
  UserDisplayManager(ws::DisplayManager* display_manager,
                     DisplayManagerDelegate* delegate,
                     const UserId& user_id);
  ~UserDisplayManager() override;

  // Called when the frame decorations for this user change.
  void OnFrameDecorationValuesChanged();

  void AddDisplayManagerBinding(
      mojo::InterfaceRequest<mojom::DisplayManager> request);

  // Called by Display prior to |display| being removed and destroyed.
  void OnWillDestroyDisplay(Display* display);

  // Called from WindowManagerState when its EventDispatcher receives a mouse
  // event.
  void OnMouseCursorLocationChanged(const gfx::Point& point);

  // Called when something about the display (e.g. pixel-ratio, size) changes.
  void OnDisplayUpdate(Display* display);

  // Returns a read-only handle to the shared memory which contains the global
  // mouse cursor position. Each call returns a new handle.
  mojo::ScopedSharedBufferHandle GetCursorLocationMemory();

 private:
  friend class test::UserDisplayManagerTestApi;

  // Called when a new observer is added. If frame decorations are available
  // notifies the observer immediately.
  void OnObserverAdded(mojom::DisplayManagerObserver* observer);

  mojo::Array<mojom::DisplayPtr> GetAllDisplays();

  // Calls OnDisplays() on |observer|.
  void CallOnDisplays(mojom::DisplayManagerObserver* observer);

  // Overriden from mojom::DisplayManager:
  void AddObserver(mojom::DisplayManagerObserverPtr observer) override;

  base::subtle::Atomic32* cursor_location_memory() {
    return reinterpret_cast<base::subtle::Atomic32*>(
        cursor_location_mapping_.get());
  }

  ws::DisplayManager* display_manager_;

  DisplayManagerDelegate* delegate_;

  const UserId user_id_;

  // Set to true the first time at least one Display has valid frame values.
  bool got_valid_frame_decorations_;

  mojo::BindingSet<mojom::DisplayManager> display_manager_bindings_;

  // WARNING: only use these once |got_valid_frame_decorations_| is true.
  mojo::InterfacePtrSet<mojom::DisplayManagerObserver>
      display_manager_observers_;

  // Observer used for tests.
  mojom::DisplayManagerObserver* test_observer_ = nullptr;

  // The current location of the cursor. This is always kept up to date so we
  // can atomically write this to |cursor_location_memory()| once it is created.
  base::subtle::Atomic32 current_cursor_location_;

  // A handle to a shared memory buffer that is one 64 bit integer long. We
  // share this with any client as the same user. This buffer is lazily
  // created on the first access.
  mojo::ScopedSharedBufferHandle cursor_location_handle_;

  // The one int32 in |cursor_location_handle_|. When we write to this
  // location, we must always write to it atomically. (On the other side of the
  // mojo connection, this data must be read atomically.)
  mojo::ScopedSharedBufferMapping cursor_location_mapping_;

  DISALLOW_COPY_AND_ASSIGN(UserDisplayManager);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_USER_DISPLAY_MANAGER_H_
