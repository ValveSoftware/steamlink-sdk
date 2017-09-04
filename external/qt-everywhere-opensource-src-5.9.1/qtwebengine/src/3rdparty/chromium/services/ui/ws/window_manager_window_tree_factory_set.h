// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_WINDOW_MANAGER_WINDOW_TREE_FACTORY_SET_H_
#define SERVICES_UI_WS_WINDOW_MANAGER_WINDOW_TREE_FACTORY_SET_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "services/ui/public/interfaces/window_manager_window_tree_factory.mojom.h"
#include "services/ui/ws/user_id_tracker_observer.h"

namespace ui {
namespace ws {

class UserIdTracker;
class WindowManagerState;
class WindowManagerWindowTreeFactory;
class WindowManagerWindowTreeFactorySetObserver;
class WindowServer;
class WindowTree;

// WindowManagerWindowTreeFactorySet tracks the set of registered
// WindowManagerWindowTreeHostFactories.
class WindowManagerWindowTreeFactorySet : public UserIdTrackerObserver {
 public:
  WindowManagerWindowTreeFactorySet(WindowServer* window_server,
                                    UserIdTracker* tracker);
  ~WindowManagerWindowTreeFactorySet() override;

  WindowServer* window_server() { return window_server_; }

  // Creates a new WindowManagerWindowTreeFactory for the specified user,
  // unless one has been set, in which case the call is ignored. The newly
  // created WindowManagerWindowTreeFactory does not immediately have a
  // WindowTree associated with it.
  WindowManagerWindowTreeFactory* Add(
      const UserId& user_id,
      mojo::InterfaceRequest<mojom::WindowManagerWindowTreeFactory> request);

  // Returns the WindowManagerState for the specified user, or null if
  // not yet set.
  WindowManagerState* GetWindowManagerStateForUser(const UserId& user_id);

  // Deletes the WindowManagerWindowTreeFactory associated with |tree|. Does
  // nothing if there is no WindowManagerWindowTreeFactory associated with
  // |tree|.
  void DeleteFactoryAssociatedWithTree(WindowTree* tree);

  // Returns all the factories, even those that may not have a WindowTree
  // associated with them.
  std::vector<WindowManagerWindowTreeFactory*> GetFactories();

  void AddObserver(WindowManagerWindowTreeFactorySetObserver* observer);
  void RemoveObserver(WindowManagerWindowTreeFactorySetObserver* observer);

 private:
  friend class WindowManagerWindowTreeFactory;

  // Called by WindowManagerWindowTreeFactory when CreateWindowTree() has
  // been called.
  void OnWindowManagerWindowTreeFactoryReady(
      WindowManagerWindowTreeFactory* factory);

  // UserIdTrackerObserver:
  void OnUserIdRemoved(const UserId& id) override;

  UserIdTracker* id_tracker_;
  WindowServer* window_server_;

  std::map<UserId, std::unique_ptr<WindowManagerWindowTreeFactory>> factories_;

  base::ObserverList<WindowManagerWindowTreeFactorySetObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerWindowTreeFactorySet);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_WINDOW_MANAGER_WINDOW_TREE_FACTORY_SET_H_
