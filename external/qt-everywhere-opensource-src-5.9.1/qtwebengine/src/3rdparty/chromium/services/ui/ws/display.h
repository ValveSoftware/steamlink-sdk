// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_DISPLAY_H_
#define SERVICES_UI_WS_DISPLAY_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <queue>
#include <set>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "services/ui/common/types.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "services/ui/public/interfaces/window_tree_host.mojom.h"
#include "services/ui/ws/focus_controller_delegate.h"
#include "services/ui/ws/focus_controller_observer.h"
#include "services/ui/ws/platform_display.h"
#include "services/ui/ws/platform_display_delegate.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_observer.h"
#include "services/ui/ws/server_window_tracker.h"
#include "services/ui/ws/user_id_tracker_observer.h"
#include "services/ui/ws/window_manager_window_tree_factory_set_observer.h"
#include "ui/display/display.h"

namespace ui {
namespace ws {

class DisplayBinding;
class DisplayManager;
class FocusController;
struct PlatformDisplayInitParams;
class WindowManagerDisplayRoot;
class WindowServer;
class WindowTree;

namespace test {
class DisplayTestApi;
}

// Displays manages the state associated with a single display. Display has a
// single root window whose children are the roots for a per-user
// WindowManager. Display is configured in two distinct
// ways:
// . with a DisplayBinding. In this mode there is only ever one WindowManager
//   for the display, which comes from the client that created the
//   Display.
// . without a DisplayBinding. In this mode a WindowManager is automatically
//   created per user.
class Display : public PlatformDisplayDelegate,
                public mojom::WindowTreeHost,
                public FocusControllerObserver,
                public FocusControllerDelegate,
                public UserIdTrackerObserver,
                public WindowManagerWindowTreeFactorySetObserver {
 public:
  explicit Display(WindowServer* window_server);
  ~Display() override;

  // Initializes the display root ServerWindow and PlatformDisplay. Adds this to
  // DisplayManager as a pending display, until accelerated widget is available.
  void Init(const PlatformDisplayInitParams& init_params,
            std::unique_ptr<DisplayBinding> binding);

  int64_t GetId() const;

  DisplayManager* display_manager();
  const DisplayManager* display_manager() const;

  PlatformDisplay* platform_display() { return platform_display_.get(); }

  // Returns a display::Display corresponding to this ws::Display.
  display::Display ToDisplay() const;

  // Returns the size of the display in physical pixels.
  gfx::Size GetSize() const;

  WindowServer* window_server() { return window_server_; }

  // Returns the root of the Display. The root's children are the roots
  // of the corresponding WindowManagers.
  ServerWindow* root_window() { return root_.get(); }
  const ServerWindow* root_window() const { return root_.get(); }

  // Returns the ServerWindow whose id is |id|. This does not do a search over
  // all windows, rather just the display and window manager root windows.
  //
  // In general you shouldn't use this, rather use WindowServer::GetWindow(),
  // which calls this as necessary.
  ServerWindow* GetRootWithId(const WindowId& id);

  WindowManagerDisplayRoot* GetWindowManagerDisplayRootWithRoot(
      const ServerWindow* window);
  WindowManagerDisplayRoot* GetWindowManagerDisplayRootForUser(
      const UserId& user_id) {
    return const_cast<WindowManagerDisplayRoot*>(
        const_cast<const Display*>(this)->GetWindowManagerDisplayRootForUser(
            user_id));
  }
  const WindowManagerDisplayRoot* GetWindowManagerDisplayRootForUser(
      const UserId& user_id) const;
  WindowManagerDisplayRoot* GetActiveWindowManagerDisplayRoot() {
    return const_cast<WindowManagerDisplayRoot*>(
        const_cast<const Display*>(this)->GetActiveWindowManagerDisplayRoot());
  }
  const WindowManagerDisplayRoot* GetActiveWindowManagerDisplayRoot() const;
  size_t num_window_manger_states() const {
    return window_manager_display_root_map_.size();
  }

  // TODO(sky): this should only be called by WindowServer, move to interface
  // used by WindowServer.
  // See description of WindowServer::SetFocusedWindow() for details on return
  // value.
  bool SetFocusedWindow(ServerWindow* window);
  // NOTE: this returns the focused window only if the focused window is in this
  // display. If this returns null focus may be in another display.
  ServerWindow* GetFocusedWindow();

  void ActivateNextWindow();

  void AddActivationParent(ServerWindow* window);
  void RemoveActivationParent(ServerWindow* window);

  void UpdateTextInputState(ServerWindow* window,
                            const ui::TextInputState& state);
  void SetImeVisibility(ServerWindow* window, bool visible);

  // Called just before |tree| is destroyed.
  void OnWillDestroyTree(WindowTree* tree);

  void UpdateNativeCursor(mojom::Cursor cursor_id);

  // mojom::WindowTreeHost:
  void SetSize(const gfx::Size& size) override;
  void SetTitle(const mojo::String& title) override;

  // Updates the size of display root ServerWindow and WM root ServerWindow(s).
  void OnViewportMetricsChanged(const display::ViewportMetrics& metrics);

 private:
  friend class test::DisplayTestApi;

  using WindowManagerDisplayRootMap =
      std::map<UserId, WindowManagerDisplayRoot*>;

  // Inits the necessary state once the display is ready.
  void InitWindowManagerDisplayRoots();

  // Creates the set of WindowManagerDisplayRoots from the
  // WindowManagerWindowTreeFactorySet.
  void CreateWindowManagerDisplayRootsFromFactories();

  void CreateWindowManagerDisplayRootFromFactory(
      WindowManagerWindowTreeFactory* factory);

  // Creates the root ServerWindow for this display, where |size| is in physical
  // pixels.
  void CreateRootWindow(const gfx::Size& size);

  // PlatformDisplayDelegate:
  ServerWindow* GetRootWindow() override;
  void OnAcceleratedWidgetAvailable() override;
  bool IsInHighContrastMode() override;
  void OnEvent(const ui::Event& event) override;
  void OnNativeCaptureLost() override;

  // FocusControllerDelegate:
  bool CanHaveActiveChildren(ServerWindow* window) const override;

  // FocusControllerObserver:
  void OnActivationChanged(ServerWindow* old_active_window,
                           ServerWindow* new_active_window) override;
  void OnFocusChanged(FocusControllerChangeSource change_source,
                      ServerWindow* old_focused_window,
                      ServerWindow* new_focused_window) override;

  // UserIdTrackerObserver:
  void OnUserIdRemoved(const UserId& id) override;

  // WindowManagerWindowTreeFactorySetObserver:
  void OnWindowManagerWindowTreeFactoryReady(
      WindowManagerWindowTreeFactory* factory) override;

  std::unique_ptr<DisplayBinding> binding_;
  WindowServer* const window_server_;
  std::unique_ptr<ServerWindow> root_;
  std::unique_ptr<PlatformDisplay> platform_display_;
  std::unique_ptr<FocusController> focus_controller_;

  // The last cursor set. Used to track whether we need to change the cursor.
  mojom::Cursor last_cursor_;

  ServerWindowTracker activation_parents_;

  WindowManagerDisplayRootMap window_manager_display_root_map_;

  DISALLOW_COPY_AND_ASSIGN(Display);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_DISPLAY_H_
