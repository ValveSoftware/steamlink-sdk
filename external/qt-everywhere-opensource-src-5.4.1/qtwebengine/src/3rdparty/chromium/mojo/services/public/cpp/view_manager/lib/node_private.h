// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_NODE_PRIVATE_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_NODE_PRIVATE_H_

#include "base/basictypes.h"

#include "mojo/services/public/cpp/view_manager/node.h"

namespace mojo {
namespace view_manager {

class NodePrivate {
 public:
  explicit NodePrivate(Node* node);
  ~NodePrivate();

  static Node* LocalCreate();

  ObserverList<NodeObserver>* observers() { return &node_->observers_; }

  void ClearParent() { node_->parent_ = NULL; }

  void set_id(Id id) { node_->id_ = id; }

  ViewManager* view_manager() { return node_->manager_; }
  void set_view_manager(ViewManager* manager) {
    node_->manager_ = manager;
  }

  void LocalDestroy() {
    node_->LocalDestroy();
  }
  void LocalAddChild(Node* child) {
    node_->LocalAddChild(child);
  }
  void LocalRemoveChild(Node* child) {
    node_->LocalRemoveChild(child);
  }
  void LocalReorder(Node* relative, OrderDirection direction) {
    node_->LocalReorder(relative, direction);
  }
  void LocalSetActiveView(View* view) {
    node_->LocalSetActiveView(view);
  }
  void LocalSetBounds(const gfx::Rect& old_bounds,
                      const gfx::Rect& new_bounds) {
    node_->LocalSetBounds(old_bounds, new_bounds);
  }

 private:
  Node* node_;

  DISALLOW_COPY_AND_ASSIGN(NodePrivate);
};

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_NODE_PRIVATE_H_
