// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_NODE_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_NODE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/interfaces/view_manager/view_manager_constants.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace mojo {
namespace view_manager {

class View;
class ViewManager;
class NodeObserver;

// Nodes are owned by the ViewManager.
// TODO(beng): Right now, you'll have to implement a NodeObserver to track
//             destruction and NULL any pointers you have.
//             Investigate some kind of smart pointer or weak pointer for these.
class Node {
 public:
  typedef std::vector<Node*> Children;

  static Node* Create(ViewManager* view_manager);

  // Destroys this node and all its children.
  void Destroy();

  // Configuration.
  Id id() const { return id_; }

  // Geometric disposition.
  const gfx::Rect& bounds() { return bounds_; }
  void SetBounds(const gfx::Rect& bounds);

  // Observation.
  void AddObserver(NodeObserver* observer);
  void RemoveObserver(NodeObserver* observer);

  // Tree.
  Node* parent() { return parent_; }
  const Node* parent() const { return parent_; }
  const Children& children() const { return children_; }

  void AddChild(Node* child);
  void RemoveChild(Node* child);

  void Reorder(Node* relative, OrderDirection direction);
  void MoveToFront();
  void MoveToBack();

  bool Contains(Node* child) const;

  Node* GetChildById(Id id);

  // View.
  void SetActiveView(View* view);
  View* active_view() { return active_view_; }

  // Focus.
  void SetFocus();

  // Embedding.
  void Embed(const String& url);

 protected:
  // This class is subclassed only by test classes that provide a public ctor.
  Node();
  ~Node();

 private:
  friend class NodePrivate;

  explicit Node(ViewManager* manager);

  void LocalDestroy();
  void LocalAddChild(Node* child);
  void LocalRemoveChild(Node* child);
  // Returns true if the order actually changed.
  bool LocalReorder(Node* relative, OrderDirection direction);
  void LocalSetActiveView(View* view);
  void LocalSetBounds(const gfx::Rect& old_bounds, const gfx::Rect& new_bounds);

  ViewManager* manager_;
  Id id_;
  Node* parent_;
  Children children_;

  ObserverList<NodeObserver> observers_;

  gfx::Rect bounds_;
  View* active_view_;

  DISALLOW_COPY_AND_ASSIGN(Node);
};

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_NODE_H_
