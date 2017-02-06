// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_manager_window_tree_factory_set.h"

#include "components/mus/ws/user_id_tracker_observer.h"
#include "components/mus/ws/window_manager_window_tree_factory.h"
#include "components/mus/ws/window_manager_window_tree_factory_set_observer.h"
#include "components/mus/ws/window_server.h"
#include "components/mus/ws/window_tree.h"

namespace mus {
namespace ws {

WindowManagerWindowTreeFactorySet::WindowManagerWindowTreeFactorySet(
    WindowServer* window_server,
    UserIdTracker* id_tracker)
    : id_tracker_(id_tracker), window_server_(window_server) {
  id_tracker_->AddObserver(this);
}

WindowManagerWindowTreeFactorySet::~WindowManagerWindowTreeFactorySet() {
  id_tracker_->RemoveObserver(this);
}

WindowManagerWindowTreeFactory* WindowManagerWindowTreeFactorySet::Add(
    const UserId& user_id,
    mojo::InterfaceRequest<mojom::WindowManagerWindowTreeFactory> request) {
  if (factories_.count(user_id)) {
    DVLOG(1) << "can only have one factory per user";
    return nullptr;
  }

  std::unique_ptr<WindowManagerWindowTreeFactory> factory_ptr(
      new WindowManagerWindowTreeFactory(this, user_id, std::move(request)));
  WindowManagerWindowTreeFactory* factory = factory_ptr.get();
  factories_[user_id] = std::move(factory_ptr);
  return factory;
}

WindowManagerState*
WindowManagerWindowTreeFactorySet::GetWindowManagerStateForUser(
    const UserId& user_id) {
  auto it = factories_.find(user_id);
  if (it == factories_.end())
    return nullptr;
  return it->second->window_tree()
             ? it->second->window_tree()->window_manager_state()
             : nullptr;
}

void WindowManagerWindowTreeFactorySet::DeleteFactoryAssociatedWithTree(
    WindowTree* window_tree) {
  for (auto it = factories_.begin(); it != factories_.end(); ++it) {
    if (it->second->window_tree() == window_tree) {
      factories_.erase(it);
      return;
    }
  }
}

std::vector<WindowManagerWindowTreeFactory*>
WindowManagerWindowTreeFactorySet::GetFactories() {
  std::vector<WindowManagerWindowTreeFactory*> result;
  for (auto& pair : factories_)
    result.push_back(pair.second.get());
  return result;
}

void WindowManagerWindowTreeFactorySet::AddObserver(
    WindowManagerWindowTreeFactorySetObserver* observer) {
  observers_.AddObserver(observer);
}

void WindowManagerWindowTreeFactorySet::RemoveObserver(
    WindowManagerWindowTreeFactorySetObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WindowManagerWindowTreeFactorySet::OnWindowManagerWindowTreeFactoryReady(
    WindowManagerWindowTreeFactory* factory) {
  const bool is_first_valid_factory = !got_valid_factory_;
  got_valid_factory_ = true;
  FOR_EACH_OBSERVER(WindowManagerWindowTreeFactorySetObserver, observers_,
                    OnWindowManagerWindowTreeFactoryReady(factory));

  // Notify after other observers as WindowServer triggers other
  // observers being added, which will have already processed the add.
  if (is_first_valid_factory)
    window_server_->OnFirstWindowManagerWindowTreeFactoryReady();
}

void WindowManagerWindowTreeFactorySet::OnUserIdRemoved(const UserId& id) {
  factories_.erase(id);
}

}  // namespace ws
}  // namespace mus
