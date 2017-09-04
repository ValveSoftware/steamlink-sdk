// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_WINDOW_SERVER_H_
#define SERVICES_UI_WS_WINDOW_SERVER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "cc/ipc/display_compositor.mojom.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/interfaces/window_manager_window_tree_factory.mojom.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "services/ui/surfaces/display_compositor.h"
#include "services/ui/ws/display.h"
#include "services/ui/ws/gpu_service_proxy_delegate.h"
#include "services/ui/ws/ids.h"
#include "services/ui/ws/operation.h"
#include "services/ui/ws/server_window_delegate.h"
#include "services/ui/ws/server_window_observer.h"
#include "services/ui/ws/user_display_manager_delegate.h"
#include "services/ui/ws/user_id_tracker.h"
#include "services/ui/ws/user_id_tracker_observer.h"
#include "services/ui/ws/window_manager_window_tree_factory_set.h"

namespace ui {
namespace ws {

class AccessPolicy;
class DisplayManager;
class GpuServiceProxy;
class ServerWindow;
class UserActivityMonitor;
class WindowManagerState;
class WindowServerDelegate;
class WindowTree;
class WindowTreeBinding;

// WindowServer manages the set of clients of the window server (all the
// WindowTrees) as well as providing the root of the hierarchy.
class WindowServer : public ServerWindowDelegate,
                     public ServerWindowObserver,
                     public GpuServiceProxyDelegate,
                     public UserDisplayManagerDelegate,
                     public UserIdTrackerObserver,
                     public cc::mojom::DisplayCompositorClient {
 public:
  explicit WindowServer(WindowServerDelegate* delegate);
  ~WindowServer() override;

  WindowServerDelegate* delegate() { return delegate_; }

  UserIdTracker* user_id_tracker() { return &user_id_tracker_; }
  const UserIdTracker* user_id_tracker() const { return &user_id_tracker_; }

  DisplayManager* display_manager() { return display_manager_.get(); }
  const DisplayManager* display_manager() const {
    return display_manager_.get();
  }

  GpuServiceProxy* gpu_proxy() { return gpu_proxy_.get(); }

  // Creates a new ServerWindow. The return value is owned by the caller, but
  // must be destroyed before WindowServer.
  ServerWindow* CreateServerWindow(
      const WindowId& id,
      const std::map<std::string, std::vector<uint8_t>>& properties);

  // Returns the id for the next WindowTree.
  ClientSpecificId GetAndAdvanceNextClientId();

  // See description of WindowTree::Embed() for details. This assumes
  // |transport_window_id| is valid.
  WindowTree* EmbedAtWindow(ServerWindow* root,
                            const UserId& user_id,
                            mojom::WindowTreeClientPtr client,
                            uint32_t flags,
                            std::unique_ptr<AccessPolicy> access_policy);

  // Adds |tree_impl_ptr| to the set of known trees. Use DestroyTree() to
  // destroy the tree.
  void AddTree(std::unique_ptr<WindowTree> tree_impl_ptr,
               std::unique_ptr<WindowTreeBinding> binding,
               mojom::WindowTreePtr tree_ptr);
  WindowTree* CreateTreeForWindowManager(
      const UserId& user_id,
      mojom::WindowTreeRequest window_tree_request,
      mojom::WindowTreeClientPtr window_tree_client);
  // Invoked when a WindowTree's connection encounters an error.
  void DestroyTree(WindowTree* tree);

  // Returns the tree by client id.
  WindowTree* GetTreeWithId(ClientSpecificId client_id);

  WindowTree* GetTreeWithClientName(const std::string& client_name);

  size_t num_trees() const { return tree_map_.size(); }

  // Returns the Window identified by |id|.
  ServerWindow* GetWindow(const WindowId& id);

  OperationType current_operation_type() const {
    return current_operation_ ? current_operation_->type()
                              : OperationType::NONE;
  }

  // Returns true if the specified client issued the current operation.
  bool IsOperationSource(ClientSpecificId client_id) const {
    return current_operation_ &&
           current_operation_->source_tree_id() == client_id;
  }

  // Invoked when a client messages a client about the change. This is used
  // to avoid sending ServerChangeIdAdvanced() unnecessarily.
  void OnTreeMessagedClient(ClientSpecificId id);

  // Returns true if OnTreeMessagedClient() was invoked for id.
  bool DidTreeMessageClient(ClientSpecificId id) const;

  // Returns the WindowTree that has |id| as a root.
  WindowTree* GetTreeWithRoot(const ServerWindow* window) {
    return const_cast<WindowTree*>(
        const_cast<const WindowServer*>(this)->GetTreeWithRoot(window));
  }
  const WindowTree* GetTreeWithRoot(const ServerWindow* window) const;

  UserActivityMonitor* GetUserActivityMonitorForUser(const UserId& user_id);

  WindowManagerWindowTreeFactorySet* window_manager_window_tree_factory_set() {
    return &window_manager_window_tree_factory_set_;
  }

  // Sets focus to |window|. Returns true if |window| already has focus, or
  // focus was successfully changed. Returns |false| if |window| is not a valid
  // window to receive focus.
  bool SetFocusedWindow(ServerWindow* window);
  ServerWindow* GetFocusedWindow();

  bool IsActiveUserInHighContrastMode() const;
  void SetHighContrastMode(const UserId& user, bool enabled);

  // Returns a change id for the window manager that is associated with
  // |source| and |client_change_id|. When the window manager replies
  // WindowManagerChangeCompleted() is called to obtain the original source
  // and client supplied change_id that initiated the called.
  uint32_t GenerateWindowManagerChangeId(WindowTree* source,
                                         uint32_t client_change_id);

  // Called when a response from the window manager is obtained. Calls to
  // the client that initiated the change with the change id originally
  // supplied by the client.
  void WindowManagerChangeCompleted(uint32_t window_manager_change_id,
                                    bool success);
  void WindowManagerCreatedTopLevelWindow(WindowTree* wm_tree,
                                          uint32_t window_manager_change_id,
                                          const ServerWindow* window);

  // Called when we get an unexpected message from the WindowManager.
  // TODO(sky): decide what we want to do here.
  void WindowManagerSentBogusMessage() {}

  // These functions trivially delegate to all WindowTrees, which in
  // term notify their clients.
  void ProcessWindowBoundsChanged(const ServerWindow* window,
                                  const gfx::Rect& old_bounds,
                                  const gfx::Rect& new_bounds);
  void ProcessClientAreaChanged(
      const ServerWindow* window,
      const gfx::Insets& new_client_area,
      const std::vector<gfx::Rect>& new_additional_client_areas);
  void ProcessCaptureChanged(const ServerWindow* new_capture,
                             const ServerWindow* old_capture);
  void ProcessWillChangeWindowHierarchy(const ServerWindow* window,
                                        const ServerWindow* new_parent,
                                        const ServerWindow* old_parent);
  void ProcessWindowHierarchyChanged(const ServerWindow* window,
                                     const ServerWindow* new_parent,
                                     const ServerWindow* old_parent);
  void ProcessWindowReorder(const ServerWindow* window,
                            const ServerWindow* relative_window,
                            const mojom::OrderDirection direction);
  void ProcessWindowDeleted(ServerWindow* window);
  void ProcessWillChangeWindowPredefinedCursor(ServerWindow* window,
                                               mojom::Cursor cursor_id);

  // Sends an |event| to all WindowTrees belonging to |user_id| that might be
  // observing events. Skips |ignore_tree| if it is non-null. |target_window| is
  // the target of the event.
  void SendToPointerWatchers(const ui::Event& event,
                             const UserId& user_id,
                             ServerWindow* target_window,
                             WindowTree* ignore_tree);

  // Sets a callback to be called whenever a ServerWindow is scheduled for
  // a [re]paint. This should only be called in a test configuration.
  void SetPaintCallback(const base::Callback<void(ServerWindow*)>& callback);

  void StartMoveLoop(uint32_t change_id,
                     ServerWindow* window,
                     WindowTree* initiator,
                     const gfx::Rect& revert_bounds);
  void EndMoveLoop();
  uint32_t GetCurrentMoveLoopChangeId();
  ServerWindow* GetCurrentMoveLoopWindow();
  WindowTree* GetCurrentMoveLoopInitiator();
  gfx::Rect GetCurrentMoveLoopRevertBounds();
  bool in_move_loop() const { return !!current_move_loop_; }

  void StartDragLoop(uint32_t change_id,
                     ServerWindow* window,
                     WindowTree* initiator);
  void EndDragLoop();
  uint32_t GetCurrentDragLoopChangeId();
  ServerWindow* GetCurrentDragLoopWindow();
  WindowTree* GetCurrentDragLoopInitiator();
  bool in_drag_loop() const { return !!current_drag_loop_; }

  void OnDisplayReady(Display* display, bool is_first);
  void OnNoMoreDisplays();
  WindowManagerState* GetWindowManagerStateForUser(const UserId& user_id);

  // ServerWindowDelegate:
  ui::DisplayCompositor* GetDisplayCompositor() override;

  // UserDisplayManagerDelegate:
  bool GetFrameDecorationsForUser(
      const UserId& user_id,
      mojom::FrameDecorationValuesPtr* values) override;

 private:
  struct CurrentMoveLoopState;
  struct CurrentDragLoopState;
  friend class Operation;

  using WindowTreeMap =
      std::map<ClientSpecificId, std::unique_ptr<WindowTree>>;
  using UserActivityMonitorMap =
      std::map<UserId, std::unique_ptr<UserActivityMonitor>>;

  struct InFlightWindowManagerChange {
    // Identifies the client that initiated the change.
    ClientSpecificId client_id;

    // Change id supplied by the client.
    uint32_t client_change_id;
  };

  using InFlightWindowManagerChangeMap =
      std::map<uint32_t, InFlightWindowManagerChange>;

  bool GetAndClearInFlightWindowManagerChange(
      uint32_t window_manager_change_id,
      InFlightWindowManagerChange* change);

  // Invoked when a client is about to execute a window server operation.
  // Subsequently followed by FinishOperation() once the change is done.
  //
  // Changes should never nest, meaning each PrepareForOperation() must be
  // balanced with a call to FinishOperation() with no PrepareForOperation()
  // in between.
  void PrepareForOperation(Operation* op);

  // Balances a call to PrepareForOperation().
  void FinishOperation();

  // Updates the native cursor by figuring out what window is under the mouse
  // cursor. This is run in response to events that change the bounds or window
  // hierarchy.
  void UpdateNativeCursorFromMouseLocation(ServerWindow* window);

  // Updates the native cursor if the cursor is currently inside |window|. This
  // is run in response to events that change the mouse cursor properties of
  // |window|.
  void UpdateNativeCursorIfOver(ServerWindow* window);

  bool IsUserInHighContrastMode(const UserId& user) const;

  // Overridden from ServerWindowDelegate:
  ServerWindow* GetRootWindow(const ServerWindow* window) override;

  // Overridden from ServerWindowObserver:
  void OnWindowDestroyed(ServerWindow* window) override;
  void OnWillChangeWindowHierarchy(ServerWindow* window,
                                   ServerWindow* new_parent,
                                   ServerWindow* old_parent) override;
  void OnWindowHierarchyChanged(ServerWindow* window,
                                ServerWindow* new_parent,
                                ServerWindow* old_parent) override;
  void OnWindowBoundsChanged(ServerWindow* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowClientAreaChanged(
      ServerWindow* window,
      const gfx::Insets& new_client_area,
      const std::vector<gfx::Rect>& new_additional_client_areas) override;
  void OnWindowReordered(ServerWindow* window,
                         ServerWindow* relative,
                         mojom::OrderDirection direction) override;
  void OnWillChangeWindowVisibility(ServerWindow* window) override;
  void OnWindowVisibilityChanged(ServerWindow* window) override;
  void OnWindowOpacityChanged(ServerWindow* window,
                              float old_opacity,
                              float new_opacity) override;
  void OnWindowSharedPropertyChanged(
      ServerWindow* window,
      const std::string& name,
      const std::vector<uint8_t>* new_data) override;
  void OnWindowPredefinedCursorChanged(ServerWindow* window,
                                       mojom::Cursor cursor_id) override;
  void OnWindowNonClientCursorChanged(ServerWindow* window,
                                      mojom::Cursor cursor_id) override;
  void OnWindowTextInputStateChanged(ServerWindow* window,
                                     const ui::TextInputState& state) override;
  void OnTransientWindowAdded(ServerWindow* window,
                              ServerWindow* transient_child) override;
  void OnTransientWindowRemoved(ServerWindow* window,
                                ServerWindow* transient_child) override;

  // GpuServiceProxyDelegate:
  void OnGpuChannelEstablished(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel) override;

  // cc::mojom::DisplayCompositorClient:
  void OnSurfaceCreated(const cc::SurfaceId& surface_id,
                        const gfx::Size& frame_size,
                        float device_scale_factor) override;

  // UserIdTrackerObserver:
  void OnActiveUserIdChanged(const UserId& previously_active_id,
                             const UserId& active_id) override;
  void OnUserIdAdded(const UserId& id) override;
  void OnUserIdRemoved(const UserId& id) override;

  UserIdTracker user_id_tracker_;

  WindowServerDelegate* delegate_;

  // ID to use for next WindowTree.
  ClientSpecificId next_client_id_;

  std::unique_ptr<DisplayManager> display_manager_;

  std::unique_ptr<CurrentDragLoopState> current_drag_loop_;
  std::unique_ptr<CurrentMoveLoopState> current_move_loop_;

  // Set of WindowTrees.
  WindowTreeMap tree_map_;

  // If non-null then we're processing a client operation. The Operation is
  // not owned by us (it's created on the stack by WindowTree).
  Operation* current_operation_;

  bool in_destructor_;
  std::map<UserId, bool> high_contrast_mode_;

  // Maps from window manager change id to the client that initiated the
  // request.
  InFlightWindowManagerChangeMap in_flight_wm_change_map_;

  // Next id supplied to the window manager.
  uint32_t next_wm_change_id_;

  std::unique_ptr<GpuServiceProxy> gpu_proxy_;
  // TODO(fsamuel): The window server should not have a GPU channel.
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_;
  base::Callback<void(ServerWindow*)> window_paint_callback_;

  UserActivityMonitorMap activity_monitor_map_;

  WindowManagerWindowTreeFactorySet window_manager_window_tree_factory_set_;

  mojo::Binding<cc::mojom::DisplayCompositorClient>
      display_compositor_client_binding_;
  // State for rendering into a Surface.
  scoped_refptr<ui::DisplayCompositor> display_compositor_;

  DISALLOW_COPY_AND_ASSIGN(WindowServer);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_WINDOW_SERVER_H_
