// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_NODE_OBSERVER_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_NODE_OBSERVER_H_

#include <vector>

#include "base/basictypes.h"

#include "mojo/services/public/cpp/view_manager/node.h"

namespace gfx {
class Rect;
}

namespace mojo {
namespace view_manager {

class Node;
class View;

class NodeObserver {
 public:
  enum DispositionChangePhase {
    DISPOSITION_CHANGING,
    DISPOSITION_CHANGED
  };

  struct TreeChangeParams {
    TreeChangeParams();
    Node* target;
    Node* old_parent;
    Node* new_parent;
    Node* receiver;
    DispositionChangePhase phase;
  };

  virtual void OnTreeChange(const TreeChangeParams& params) {}

  virtual void OnNodeReordered(Node* node,
                               Node* relative_node,
                               OrderDirection direction,
                               DispositionChangePhase phase) {}

  virtual void OnNodeDestroy(Node* node, DispositionChangePhase phase) {}

  virtual void OnNodeActiveViewChange(Node* node,
                                      View* old_view,
                                      View* new_view,
                                      DispositionChangePhase phase) {}

  virtual void OnNodeBoundsChange(Node* node,
                                  const gfx::Rect& old_bounds,
                                  const gfx::Rect& new_bounds,
                                  DispositionChangePhase phase) {}

 protected:
  virtual ~NodeObserver() {}
};

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_NODE_OBSERVER_H_
