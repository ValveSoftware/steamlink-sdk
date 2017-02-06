// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_TREE_H_
#define COMPONENTS_MUS_WS_WINDOW_TREE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "cc/ipc/surface_id.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/ws/access_policy_delegate.h"
#include "components/mus/ws/ids.h"
#include "components/mus/ws/user_id.h"
#include "components/mus/ws/window_tree_binding.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace gfx {
class Insets;
class Rect;
}

namespace ui {
class Event;
}

namespace mus {
namespace ws {

class AccessPolicy;
class DisplayManager;
class Display;
class EventMatcher;
class ServerWindow;
class TargetedEvent;
class WindowManagerDisplayRoot;
class WindowManagerState;
class WindowServer;
class WindowTreeTest;

namespace test {
class WindowTreeTestApi;
}

// WindowTree represents a view onto portions of the window tree. The parts of
// the tree exposed to the client start at the root windows. A WindowTree may
// have any number of roots (including none). A WindowTree may not have
// visibility of all the descendants of its roots.
//
// WindowTree notifies its client as changes happen to windows exposed to the
// the client.
//
// See ids.h for details on how WindowTree handles identity of windows.
class WindowTree : public mojom::WindowTree,
                   public AccessPolicyDelegate,
                   public mojom::WindowManagerClient {
 public:
  WindowTree(WindowServer* window_server,
             const UserId& user_id,
             ServerWindow* root,
             std::unique_ptr<AccessPolicy> access_policy);
  ~WindowTree() override;

  void Init(std::unique_ptr<WindowTreeBinding> binding,
            mojom::WindowTreePtr tree);

  // Called if this WindowTree hosts a WindowManager.
  void ConfigureWindowManager();

  ClientSpecificId id() const { return id_; }

  void set_embedder_intercepts_events() { embedder_intercepts_events_ = true; }
  bool embedder_intercepts_events() const {
    return embedder_intercepts_events_;
  }

  const UserId& user_id() const { return user_id_; }

  mojom::WindowTreeClient* client() { return binding_->client(); }

  // Returns the Window with the specified id.
  ServerWindow* GetWindow(const WindowId& id) {
    return const_cast<ServerWindow*>(
        const_cast<const WindowTree*>(this)->GetWindow(id));
  }
  const ServerWindow* GetWindow(const WindowId& id) const;

  // Returns the Window with the specified client id *only* if known to this
  // client, returns null if not known.
  ServerWindow* GetWindowByClientId(const ClientWindowId& id) {
    return const_cast<ServerWindow*>(
        const_cast<const WindowTree*>(this)->GetWindowByClientId(id));
  }
  const ServerWindow* GetWindowByClientId(const ClientWindowId& id) const;

  bool IsWindowKnown(const ServerWindow* window) const {
    return IsWindowKnown(window, nullptr);
  }
  // Returns whether |window| is known to this tree. If |window| is known and
  // |client_window_id| is non-null |client_window_id| is set to the
  // ClientWindowId of the window.
  bool IsWindowKnown(const ServerWindow* window,
                     ClientWindowId* client_window_id) const;

  // Returns true if |window| is one of this trees roots.
  bool HasRoot(const ServerWindow* window) const;

  std::set<const ServerWindow*> roots() { return roots_; }

  void set_name(const std::string& name) { name_ = name; }
  const std::string& name() const { return name_; }

  bool janky() const { return janky_; }

  const Display* GetDisplay(const ServerWindow* window) const;
  Display* GetDisplay(const ServerWindow* window) {
    return const_cast<Display*>(
        const_cast<const WindowTree*>(this)->GetDisplay(window));
  }

  const WindowManagerDisplayRoot* GetWindowManagerDisplayRoot(
      const ServerWindow* window) const;
  WindowManagerDisplayRoot* GetWindowManagerDisplayRoot(
      const ServerWindow* window) {
    return const_cast<WindowManagerDisplayRoot*>(
        const_cast<const WindowTree*>(this)->GetWindowManagerDisplayRoot(
            window));
  }
  WindowManagerState* window_manager_state() {
    return window_manager_state_.get();
  }

  DisplayManager* display_manager();
  const DisplayManager* display_manager() const;

  WindowServer* window_server() { return window_server_; }
  const WindowServer* window_server() const { return window_server_; }

  // Adds a new root to this tree. This is only valid for window managers.
  void AddRootForWindowManager(const ServerWindow* root);

  // Invoked when a tree is about to be destroyed.
  void OnWindowDestroyingTreeImpl(WindowTree* tree);

  // These functions are synchronous variants of those defined in the mojom. The
  // WindowTree implementations all call into these. See the mojom for details.
  bool SetCapture(const ClientWindowId& client_window_id);
  bool NewWindow(const ClientWindowId& client_window_id,
                 const std::map<std::string, std::vector<uint8_t>>& properties);
  bool AddWindow(const ClientWindowId& parent_id,
                 const ClientWindowId& child_id);
  bool AddTransientWindow(const ClientWindowId& window_id,
                          const ClientWindowId& transient_window_id);
  bool SetModal(const ClientWindowId& window_id);
  std::vector<const ServerWindow*> GetWindowTree(
      const ClientWindowId& window_id) const;
  bool SetWindowVisibility(const ClientWindowId& window_id, bool visible);
  bool SetWindowOpacity(const ClientWindowId& window_id, float opacity);
  bool SetFocus(const ClientWindowId& window_id);
  bool Embed(const ClientWindowId& window_id,
             mojom::WindowTreeClientPtr client,
             uint32_t flags);
  void DispatchInputEvent(ServerWindow* target, const ui::Event& event);

  bool IsWaitingForNewTopLevelWindow(uint32_t wm_change_id);
  void OnWindowManagerCreatedTopLevelWindow(uint32_t wm_change_id,
                                            uint32_t client_change_id,
                                            const ServerWindow* window);
  void AddActivationParent(const ClientWindowId& window_id);

  // Calls through to the client.
  void OnChangeCompleted(uint32_t change_id, bool success);
  void OnAccelerator(uint32_t accelerator_id, const ui::Event& event);

  // Called when |tree|'s jankiness changes (see janky_ for definition).
  // Notifies the window manager client so it can update UI for the affected
  // window(s).
  void ClientJankinessChanged(WindowTree* tree);

  // The following methods are invoked after the corresponding change has been
  // processed. They do the appropriate bookkeeping and update the client as
  // necessary.
  void ProcessWindowBoundsChanged(const ServerWindow* window,
                                  const gfx::Rect& old_bounds,
                                  const gfx::Rect& new_bounds,
                                  bool originated_change);
  void ProcessClientAreaChanged(
      const ServerWindow* window,
      const gfx::Insets& new_client_area,
      const std::vector<gfx::Rect>& new_additional_client_areas,
      bool originated_change);
  void ProcessWillChangeWindowHierarchy(const ServerWindow* window,
                                        const ServerWindow* new_parent,
                                        const ServerWindow* old_parent,
                                        bool originated_change);
  void ProcessWindowPropertyChanged(const ServerWindow* window,
                                    const std::string& name,
                                    const std::vector<uint8_t>* new_data,
                                    bool originated_change);
  void ProcessWindowHierarchyChanged(const ServerWindow* window,
                                     const ServerWindow* new_parent,
                                     const ServerWindow* old_parent,
                                     bool originated_change);
  void ProcessWindowReorder(const ServerWindow* window,
                            const ServerWindow* relative_window,
                            mojom::OrderDirection direction,
                            bool originated_change);
  void ProcessWindowDeleted(const ServerWindow* window, bool originated_change);
  void ProcessWillChangeWindowVisibility(const ServerWindow* window,
                                         bool originated_change);
  void ProcessWindowOpacityChanged(const ServerWindow* window,
                                   float old_opacity,
                                   float new_opacity,
                                   bool originated_change);
  void ProcessCursorChanged(const ServerWindow* window,
                            int32_t cursor_id,
                            bool originated_change);
  void ProcessFocusChanged(const ServerWindow* old_focused_window,
                           const ServerWindow* new_focused_window);
  void ProcessLostCapture(const ServerWindow* old_lost_capture,
                          bool originated_change);
  void ProcessTransientWindowAdded(const ServerWindow* window,
                                   const ServerWindow* transient_window,
                                   bool originated_change);
  void ProcessTransientWindowRemoved(const ServerWindow* window,
                                     const ServerWindow* transient_window,
                                     bool originated_change);

  // Sends this event to the client if it matches an active event observer.
  void SendToEventObserver(const ui::Event& event);

 private:
  friend class test::WindowTreeTestApi;

  struct WaitingForTopLevelWindowInfo {
    WaitingForTopLevelWindowInfo(const ClientWindowId& client_window_id,
                                 uint32_t wm_change_id)
        : client_window_id(client_window_id), wm_change_id(wm_change_id) {}
    ~WaitingForTopLevelWindowInfo() {}

    // Id supplied from the client.
    ClientWindowId client_window_id;

    // Change id we created for the window manager.
    uint32_t wm_change_id;
  };

  enum class RemoveRootReason {
    // The window is being removed.
    DELETED,

    // Another client is being embedded in the window.
    EMBED,
  };

  bool ShouldRouteToWindowManager(const ServerWindow* window) const;

  ClientWindowId ClientWindowIdForWindow(const ServerWindow* window) const;

  // Returns true if |id| is a valid WindowId for a new window.
  bool IsValidIdForNewWindow(const ClientWindowId& id) const;

  WindowId GenerateNewWindowId();

  // These functions return true if the corresponding mojom function is allowed
  // for this tree.
  bool CanReorderWindow(const ServerWindow* window,
                        const ServerWindow* relative_window,
                        mojom::OrderDirection direction) const;

  // Deletes a window owned by this tree. Returns true on success. |source| is
  // the tree that originated the change.
  bool DeleteWindowImpl(WindowTree* source, ServerWindow* window);

  // If |window| is known does nothing. Otherwise adds |window| to |windows|,
  // marks |window| as known and recurses.
  void GetUnknownWindowsFrom(const ServerWindow* window,
                             std::vector<const ServerWindow*>* windows);

  // Removes |window| from the appropriate maps. If |window| is known to this
  // client true is returned.
  bool RemoveFromMaps(const ServerWindow* window);

  // Removes |window| and all its descendants from the necessary maps. This
  // does not recurse through windows that were created by this tree. All
  // windows owned by this tree are added to |local_windows|.
  void RemoveFromKnown(const ServerWindow* window,
                       std::vector<ServerWindow*>* local_windows);

  // Removes a root from set of roots of this tree. This does not remove
  // the window from the window tree, only from the set of roots.
  void RemoveRoot(const ServerWindow* window, RemoveRootReason reason);

  // Converts Window(s) to WindowData(s) for transport. This assumes all the
  // windows are valid for the client. The parent of windows the client is not
  // allowed to see are set to NULL (in the returned WindowData(s)).
  mojo::Array<mojom::WindowDataPtr> WindowsToWindowDatas(
      const std::vector<const ServerWindow*>& windows);
  mojom::WindowDataPtr WindowToWindowData(const ServerWindow* window);

  // Implementation of GetWindowTree(). Adds |window| to |windows| and recurses
  // if CanDescendIntoWindowForWindowTree() returns true.
  void GetWindowTreeImpl(const ServerWindow* window,
                         std::vector<const ServerWindow*>* windows) const;

  // Notify the client if the drawn state of any of the roots changes.
  // |window| is the window that is changing to the drawn state
  // |new_drawn_value|.
  void NotifyDrawnStateChanged(const ServerWindow* window,
                               bool new_drawn_value);

  // Deletes all Windows we own.
  void DestroyWindows();

  bool CanEmbed(const ClientWindowId& window_id) const;
  void PrepareForEmbed(ServerWindow* window);
  void RemoveChildrenAsPartOfEmbed(ServerWindow* window);

  void DispatchInputEventImpl(ServerWindow* target, const ui::Event& event);

  // Calls OnChangeCompleted() on the client.
  void NotifyChangeCompleted(uint32_t change_id,
                             mojom::WindowManagerErrorCode error_code);

  // WindowTree:
  void NewWindow(uint32_t change_id,
                 Id transport_window_id,
                 mojo::Map<mojo::String, mojo::Array<uint8_t>>
                     transport_properties) override;
  void NewTopLevelWindow(uint32_t change_id,
                         Id transport_window_id,
                         mojo::Map<mojo::String, mojo::Array<uint8_t>>
                             transport_properties) override;
  void DeleteWindow(uint32_t change_id, Id transport_window_id) override;
  void AddWindow(uint32_t change_id, Id parent_id, Id child_id) override;
  void RemoveWindowFromParent(uint32_t change_id, Id window_id) override;
  void AddTransientWindow(uint32_t change_id,
                          Id window,
                          Id transient_window) override;
  void RemoveTransientWindowFromParent(uint32_t change_id,
                                       Id transient_window_id) override;
  void SetModal(uint32_t change_id, Id window_id) override;
  void ReorderWindow(uint32_t change_Id,
                     Id window_id,
                     Id relative_window_id,
                     mojom::OrderDirection direction) override;
  void GetWindowTree(
      Id window_id,
      const base::Callback<void(mojo::Array<mojom::WindowDataPtr>)>& callback)
      override;
  void SetCapture(uint32_t change_id, Id window_id) override;
  void ReleaseCapture(uint32_t change_id, Id window_id) override;
  void SetEventObserver(mojom::EventMatcherPtr matcher,
                        uint32_t observer_id) override;
  void SetWindowBounds(uint32_t change_id,
                       Id window_id,
                       const gfx::Rect& bounds) override;
  void SetWindowVisibility(uint32_t change_id,
                           Id window_id,
                           bool visible) override;
  void SetWindowProperty(uint32_t change_id,
                         Id transport_window_id,
                         const mojo::String& name,
                         mojo::Array<uint8_t> value) override;
  void SetWindowOpacity(uint32_t change_id,
                        Id window_id,
                        float opacity) override;
  void AttachSurface(Id transport_window_id,
                     mojom::SurfaceType type,
                     mojo::InterfaceRequest<mojom::Surface> surface,
                     mojom::SurfaceClientPtr client) override;
  void Embed(Id transport_window_id,
             mojom::WindowTreeClientPtr client,
             uint32_t flags,
             const EmbedCallback& callback) override;
  void SetFocus(uint32_t change_id, Id transport_window_id) override;
  void SetCanFocus(Id transport_window_id, bool can_focus) override;
  void SetPredefinedCursor(uint32_t change_id,
                           Id transport_window_id,
                           mus::mojom::Cursor cursor_id) override;
  void SetWindowTextInputState(Id transport_window_id,
                               mojo::TextInputStatePtr state) override;
  void SetImeVisibility(Id transport_window_id,
                        bool visible,
                        mojo::TextInputStatePtr state) override;
  void OnWindowInputEventAck(uint32_t event_id,
                             mojom::EventResult result) override;
  void SetClientArea(
      Id transport_window_id,
      const gfx::Insets& insets,
      mojo::Array<gfx::Rect> transport_additional_client_areas) override;
  void SetHitTestMask(Id transport_window_id, const gfx::Rect& mask) override;
  void GetWindowManagerClient(
      mojo::AssociatedInterfaceRequest<mojom::WindowManagerClient> internal)
      override;
  void GetCursorLocationMemory(const GetCursorLocationMemoryCallback& callback)
      override;

  // mojom::WindowManagerClient:
  void AddAccelerator(uint32_t id,
                      mojom::EventMatcherPtr event_matcher,
                      const AddAcceleratorCallback& callback) override;
  void RemoveAccelerator(uint32_t id) override;
  void AddActivationParent(Id transport_window_id) override;
  void RemoveActivationParent(Id transport_window_id) override;
  void ActivateNextWindow() override;
  void SetUnderlaySurfaceOffsetAndExtendedHitArea(
      Id window_id,
      int32_t x_offset,
      int32_t y_offset,
      const gfx::Insets& hit_area) override;
  void WmResponse(uint32_t change_id, bool response) override;
  void WmRequestClose(Id transport_window_id) override;
  void WmSetFrameDecorationValues(
      mojom::FrameDecorationValuesPtr values) override;
  void WmSetNonClientCursor(uint32_t window_id,
                            mojom::Cursor cursor_id) override;
  void OnWmCreatedTopLevelWindow(uint32_t change_id,
                                 Id transport_window_id) override;

  // AccessPolicyDelegate:
  bool HasRootForAccessPolicy(const ServerWindow* window) const override;
  bool IsWindowKnownForAccessPolicy(const ServerWindow* window) const override;
  bool IsWindowRootOfAnotherTreeForAccessPolicy(
      const ServerWindow* window) const override;

  WindowServer* window_server_;

  UserId user_id_;

  // Id of this tree as assigned by WindowServer.
  const ClientSpecificId id_;
  std::string name_;

  ClientSpecificId next_window_id_;

  std::unique_ptr<WindowTreeBinding> binding_;

  std::unique_ptr<mus::ws::AccessPolicy> access_policy_;

  // The roots, or embed points, of this tree. A WindowTree may have any
  // number of roots, including 0.
  std::set<const ServerWindow*> roots_;

  // The windows created by this tree. This tree owns these objects.
  base::hash_map<WindowId, ServerWindow*> created_window_map_;

  // The client is allowed to assign ids. These two maps providing the mapping
  // from the ids native to the server (WindowId) to those understood by the
  // client (ClientWindowId).
  base::hash_map<ClientWindowId, WindowId> client_id_to_window_id_map_;
  base::hash_map<WindowId, ClientWindowId> window_id_to_client_id_map_;

  uint32_t event_ack_id_;

  // A client is considered janky if it hasn't ACK'ed input events within a
  // reasonable timeframe.
  bool janky_ = false;

  // Set when the client is using SetEventObserver() to observe events,
  // otherwise null.
  std::unique_ptr<EventMatcher> event_observer_matcher_;

  // The ID supplied by the client for the current event observer.
  uint32_t event_observer_id_ = 0;

  // WindowManager the current event came from.
  WindowManagerState* event_source_wms_ = nullptr;

  std::queue<std::unique_ptr<TargetedEvent>> event_queue_;

  std::unique_ptr<mojo::AssociatedBinding<mojom::WindowManagerClient>>
      window_manager_internal_client_binding_;
  mojom::WindowManager* window_manager_internal_;
  std::unique_ptr<WindowManagerState> window_manager_state_;

  std::unique_ptr<WaitingForTopLevelWindowInfo>
      waiting_for_top_level_window_info_;
  bool embedder_intercepts_events_ = false;

  DISALLOW_COPY_AND_ASSIGN(WindowTree);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_TREE_H_
