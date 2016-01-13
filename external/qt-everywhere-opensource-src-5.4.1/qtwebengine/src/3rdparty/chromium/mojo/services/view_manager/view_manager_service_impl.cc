// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view_manager_service_impl.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/input_events/input_events_type_converters.h"
#include "mojo/services/view_manager/node.h"
#include "mojo/services/view_manager/root_node_manager.h"
#include "mojo/services/view_manager/view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window.h"
#include "ui/gfx/codec/png_codec.h"

namespace mojo {
namespace view_manager {
namespace service {
namespace {

// Places |node| in |nodes| and recurses through the children.
void GetDescendants(const Node* node, std::vector<const Node*>* nodes) {
  if (!node)
    return;

  nodes->push_back(node);

  std::vector<const Node*> children(node->GetChildren());
  for (size_t i = 0 ; i < children.size(); ++i)
    GetDescendants(children[i], nodes);
}

}  // namespace

ViewManagerServiceImpl::ViewManagerServiceImpl(
    RootNodeManager* root_node_manager,
    ConnectionSpecificId creator_id,
    const std::string& creator_url,
    const std::string& url)
    : root_node_manager_(root_node_manager),
      id_(root_node_manager_->GetAndAdvanceNextConnectionId()),
      url_(url),
      creator_id_(creator_id),
      creator_url_(creator_url),
      delete_on_connection_error_(false) {
}

ViewManagerServiceImpl::~ViewManagerServiceImpl() {
  // Delete any views we own.
  while (!view_map_.empty()) {
    bool result = DeleteViewImpl(this, view_map_.begin()->second->id());
    DCHECK(result);
  }

  // We're about to destroy all our nodes. Detach any views from them.
  for (NodeMap::iterator i = node_map_.begin(); i != node_map_.end(); ++i) {
    if (i->second->view()) {
      bool result = SetViewImpl(i->second, ViewId());
      DCHECK(result);
    }
  }

  if (!node_map_.empty()) {
    RootNodeManager::ScopedChange change(
        this, root_node_manager_,
        RootNodeManager::CHANGE_TYPE_ADVANCE_SERVER_CHANGE_ID, true);
    while (!node_map_.empty()) {
      scoped_ptr<Node> node(node_map_.begin()->second);
      Node* parent = node->GetParent();
      const NodeId node_id(node->id());
      if (parent)
        parent->Remove(node.get());
      root_node_manager_->ProcessNodeDeleted(node_id);
      node_map_.erase(NodeIdToTransportId(node_id));
    }
  }

  root_node_manager_->RemoveConnection(this);
}

const Node* ViewManagerServiceImpl::GetNode(const NodeId& id) const {
  if (id_ == id.connection_id) {
    NodeMap::const_iterator i = node_map_.find(id.node_id);
    return i == node_map_.end() ? NULL : i->second;
  }
  return root_node_manager_->GetNode(id);
}

const View* ViewManagerServiceImpl::GetView(const ViewId& id) const {
  if (id_ == id.connection_id) {
    ViewMap::const_iterator i = view_map_.find(id.view_id);
    return i == view_map_.end() ? NULL : i->second;
  }
  return root_node_manager_->GetView(id);
}

void ViewManagerServiceImpl::SetRoots(const Array<Id>& node_ids) {
  DCHECK(roots_.empty());
  NodeIdSet roots;
  for (size_t i = 0; i < node_ids.size(); ++i) {
    DCHECK(GetNode(NodeIdFromTransportId(node_ids[i])));
    roots.insert(node_ids[i]);
  }
  roots_.swap(roots);
}

void ViewManagerServiceImpl::OnViewManagerServiceImplDestroyed(
    ConnectionSpecificId id) {
  if (creator_id_ == id)
    creator_id_ = kRootConnection;
}

void ViewManagerServiceImpl::ProcessNodeBoundsChanged(
    const Node* node,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    bool originated_change) {
  if (originated_change)
    return;
  Id node_id = NodeIdToTransportId(node->id());
  if (known_nodes_.count(node_id) > 0) {
    client()->OnNodeBoundsChanged(node_id,
                                  Rect::From(old_bounds),
                                  Rect::From(new_bounds));
  }
}

void ViewManagerServiceImpl::ProcessNodeHierarchyChanged(
    const Node* node,
    const Node* new_parent,
    const Node* old_parent,
    Id server_change_id,
    bool originated_change) {
  if (known_nodes_.count(NodeIdToTransportId(node->id())) > 0) {
    if (originated_change)
      return;
    if (node->id().connection_id != id_ && !IsNodeDescendantOfRoots(node)) {
      // Node was a descendant of roots and is no longer, treat it as though the
      // node was deleted.
      RemoveFromKnown(node);
      client()->OnNodeDeleted(NodeIdToTransportId(node->id()),
                              server_change_id);
      root_node_manager_->OnConnectionMessagedClient(id_);
      return;
    }
  }

  if (originated_change || root_node_manager_->is_processing_delete_node())
    return;
  std::vector<const Node*> to_send;
  if (!ShouldNotifyOnHierarchyChange(node, &new_parent, &old_parent,
                                     &to_send)) {
    if (root_node_manager_->IsProcessingChange()) {
      client()->OnServerChangeIdAdvanced(
          root_node_manager_->next_server_change_id() + 1);
    }
    return;
  }
  const NodeId new_parent_id(new_parent ? new_parent->id() : NodeId());
  const NodeId old_parent_id(old_parent ? old_parent->id() : NodeId());
  DCHECK((node->id().connection_id == id_) ||
         (roots_.count(NodeIdToTransportId(node->id())) > 0) ||
         (new_parent && IsNodeDescendantOfRoots(new_parent)) ||
         (old_parent && IsNodeDescendantOfRoots(old_parent)));
  client()->OnNodeHierarchyChanged(NodeIdToTransportId(node->id()),
                                   NodeIdToTransportId(new_parent_id),
                                   NodeIdToTransportId(old_parent_id),
                                   server_change_id,
                                   NodesToNodeDatas(to_send));
}

void ViewManagerServiceImpl::ProcessNodeReorder(const Node* node,
                                                const Node* relative_node,
                                                OrderDirection direction,
                                                Id server_change_id,
                                                bool originated_change) {
  if (originated_change ||
      !known_nodes_.count(NodeIdToTransportId(node->id())) ||
      !known_nodes_.count(NodeIdToTransportId(relative_node->id()))) {
    return;
  }

  client()->OnNodeReordered(NodeIdToTransportId(node->id()),
                            NodeIdToTransportId(relative_node->id()),
                            direction,
                            server_change_id);
}

void ViewManagerServiceImpl::ProcessNodeViewReplaced(
    const Node* node,
    const View* new_view,
    const View* old_view,
    bool originated_change) {
  if (originated_change || !known_nodes_.count(NodeIdToTransportId(node->id())))
    return;
  const Id new_view_id = new_view ?
      ViewIdToTransportId(new_view->id()) : 0;
  const Id old_view_id = old_view ?
      ViewIdToTransportId(old_view->id()) : 0;
  client()->OnNodeViewReplaced(NodeIdToTransportId(node->id()),
                               new_view_id, old_view_id);
}

void ViewManagerServiceImpl::ProcessNodeDeleted(const NodeId& node,
                                                Id server_change_id,
                                                bool originated_change) {
  const bool in_known = known_nodes_.erase(NodeIdToTransportId(node)) > 0;
  const bool in_roots = roots_.erase(NodeIdToTransportId(node)) > 0;

  if (in_roots && roots_.empty())
    roots_.insert(NodeIdToTransportId(InvalidNodeId()));

  if (originated_change)
    return;

  if (in_known) {
    client()->OnNodeDeleted(NodeIdToTransportId(node), server_change_id);
    root_node_manager_->OnConnectionMessagedClient(id_);
  } else if (root_node_manager_->IsProcessingChange() &&
             !root_node_manager_->DidConnectionMessageClient(id_)) {
    client()->OnServerChangeIdAdvanced(
        root_node_manager_->next_server_change_id() + 1);
    root_node_manager_->OnConnectionMessagedClient(id_);
  }
}

void ViewManagerServiceImpl::ProcessViewDeleted(const ViewId& view,
                                                bool originated_change) {
  if (originated_change)
    return;
  client()->OnViewDeleted(ViewIdToTransportId(view));
}

void ViewManagerServiceImpl::OnConnectionError() {
  if (delete_on_connection_error_)
    delete this;
}

bool ViewManagerServiceImpl::CanRemoveNodeFromParent(const Node* node) const {
  if (!node)
    return false;

  const Node* parent = node->GetParent();
  if (!parent)
    return false;

  // Always allow the remove if there are no roots. Otherwise the remove is
  // allowed if the parent is a descendant of the roots, or the node and its
  // parent were created by this connection. We explicitly disallow removal of
  // the node from its parent if the parent isn't visible to this connection
  // (not in roots).
  return (roots_.empty() ||
          (IsNodeDescendantOfRoots(parent) ||
           (node->id().connection_id == id_ &&
            parent->id().connection_id == id_)));
}

bool ViewManagerServiceImpl::CanAddNode(const Node* parent,
                                        const Node* child) const {
  if (!parent || !child)
    return false;  // Both nodes must be valid.

  if (child->GetParent() == parent || child->Contains(parent))
    return false;  // Would result in an invalid hierarchy.

  if (roots_.empty())
    return true;  // No restriction if there are no roots.

  if (!IsNodeDescendantOfRoots(parent) && parent->id().connection_id != id_)
    return false;  // |parent| is not visible to this connection.

  // Allow the add if the child is already a descendant of the roots or was
  // created by this connection.
  return (IsNodeDescendantOfRoots(child) || child->id().connection_id == id_);
}

bool ViewManagerServiceImpl::CanReorderNode(const Node* node,
                                            const Node* relative_node,
                                            OrderDirection direction) const {
  if (!node || !relative_node)
    return false;

  if (node->id().connection_id != id_)
    return false;

  const Node* parent = node->GetParent();
  if (!parent || parent != relative_node->GetParent())
    return false;

  if (known_nodes_.count(NodeIdToTransportId(parent->id())) == 0)
    return false;

  std::vector<const Node*> children = parent->GetChildren();
  const size_t child_i =
      std::find(children.begin(), children.end(), node) - children.begin();
  const size_t target_i =
      std::find(children.begin(), children.end(), relative_node) -
      children.begin();
  if ((direction == ORDER_ABOVE && child_i == target_i + 1) ||
      (direction == ORDER_BELOW && child_i + 1 == target_i)) {
    return false;
  }

  return true;
}

bool ViewManagerServiceImpl::CanDeleteNode(const NodeId& node_id) const {
  return node_id.connection_id == id_;
}

bool ViewManagerServiceImpl::CanDeleteView(const ViewId& view_id) const {
  return view_id.connection_id == id_;
}

bool ViewManagerServiceImpl::CanSetView(const Node* node,
                                        const ViewId& view_id) const {
  if (!node || !IsNodeDescendantOfRoots(node))
    return false;

  const View* view = GetView(view_id);
  return (view && view_id.connection_id == id_) || view_id == ViewId();
}

bool ViewManagerServiceImpl::CanSetFocus(const Node* node) const {
  // TODO(beng): security.
  return true;
}

bool ViewManagerServiceImpl::CanGetNodeTree(const Node* node) const {
  return node &&
      (IsNodeDescendantOfRoots(node) || node->id().connection_id == id_);
}

bool ViewManagerServiceImpl::CanEmbed(
    const mojo::Array<uint32_t>& node_ids) const {
  for (size_t i = 0; i < node_ids.size(); ++i) {
    const Node* node = GetNode(NodeIdFromTransportId(node_ids[i]));
    if (!node || node->id().connection_id != id_)
      return false;
  }
  return node_ids.size() > 0;
}

bool ViewManagerServiceImpl::DeleteNodeImpl(ViewManagerServiceImpl* source,
                                            const NodeId& node_id) {
  DCHECK_EQ(node_id.connection_id, id_);
  Node* node = GetNode(node_id);
  if (!node)
    return false;
  RootNodeManager::ScopedChange change(
      source, root_node_manager_,
      RootNodeManager::CHANGE_TYPE_ADVANCE_SERVER_CHANGE_ID, true);
  if (node->GetParent())
    node->GetParent()->Remove(node);
  std::vector<Node*> children(node->GetChildren());
  for (size_t i = 0; i < children.size(); ++i)
    node->Remove(children[i]);
  DCHECK(node->GetChildren().empty());
  node_map_.erase(node_id.node_id);
  delete node;
  node = NULL;
  root_node_manager_->ProcessNodeDeleted(node_id);
  return true;
}

bool ViewManagerServiceImpl::DeleteViewImpl(ViewManagerServiceImpl* source,
                                            const ViewId& view_id) {
  DCHECK_EQ(view_id.connection_id, id_);
  View* view = GetView(view_id);
  if (!view)
    return false;
  RootNodeManager::ScopedChange change(
      source, root_node_manager_,
      RootNodeManager::CHANGE_TYPE_DONT_ADVANCE_SERVER_CHANGE_ID, false);
  if (view->node())
    view->node()->SetView(NULL);
  view_map_.erase(view_id.view_id);
  // Make a copy of |view_id| as once we delete view |view_id| may no longer be
  // valid.
  const ViewId view_id_copy(view_id);
  delete view;
  root_node_manager_->ProcessViewDeleted(view_id_copy);
  return true;
}

bool ViewManagerServiceImpl::SetViewImpl(Node* node, const ViewId& view_id) {
  DCHECK(node);  // CanSetView() should have verified node exists.
  View* view = GetView(view_id);
  RootNodeManager::ScopedChange change(
      this, root_node_manager_,
      RootNodeManager::CHANGE_TYPE_DONT_ADVANCE_SERVER_CHANGE_ID, false);
  node->SetView(view);

  // TODO(sky): this is temporary, need a real focus API.
  if (view && root_node_manager_->root()->Contains(node))
    node->window()->Focus();

  return true;
}

void ViewManagerServiceImpl::GetUnknownNodesFrom(
    const Node* node,
    std::vector<const Node*>* nodes) {
  const Id transport_id = NodeIdToTransportId(node->id());
  if (known_nodes_.count(transport_id) == 1)
    return;
  nodes->push_back(node);
  known_nodes_.insert(transport_id);
  std::vector<const Node*> children(node->GetChildren());
  for (size_t i = 0 ; i < children.size(); ++i)
    GetUnknownNodesFrom(children[i], nodes);
}

void ViewManagerServiceImpl::RemoveFromKnown(const Node* node) {
  if (node->id().connection_id == id_)
    return;
  known_nodes_.erase(NodeIdToTransportId(node->id()));
  std::vector<const Node*> children = node->GetChildren();
  for (size_t i = 0; i < children.size(); ++i)
    RemoveFromKnown(children[i]);
}

bool ViewManagerServiceImpl::AddRoots(
    const std::vector<Id>& node_ids) {
  std::vector<const Node*> to_send;
  bool did_add_root = false;
  for (size_t i = 0; i < node_ids.size(); ++i) {
    CHECK_EQ(creator_id_, NodeIdFromTransportId(node_ids[i]).connection_id);
    if (roots_.count(node_ids[i]) > 0)
      continue;

    did_add_root = true;
    roots_.insert(node_ids[i]);
    Node* node = GetNode(NodeIdFromTransportId(node_ids[i]));
    DCHECK(node);
    if (known_nodes_.count(node_ids[i]) == 0) {
      GetUnknownNodesFrom(node, &to_send);
    } else {
      // Even though the connection knows about the new root we need to tell it
      // |node| is now a root.
      to_send.push_back(node);
    }
  }

  if (!did_add_root)
    return false;

  client()->OnRootsAdded(NodesToNodeDatas(to_send));
  return true;
}

bool ViewManagerServiceImpl::IsNodeDescendantOfRoots(const Node* node) const {
  if (roots_.empty())
    return true;
  if (!node)
    return false;
  const Id invalid_node_id =
      NodeIdToTransportId(InvalidNodeId());
  for (NodeIdSet::const_iterator i = roots_.begin(); i != roots_.end(); ++i) {
    if (*i == invalid_node_id)
      continue;
    const Node* root = GetNode(NodeIdFromTransportId(*i));
    DCHECK(root);
    if (root->Contains(node))
      return true;
  }
  return false;
}

bool ViewManagerServiceImpl::ShouldNotifyOnHierarchyChange(
    const Node* node,
    const Node** new_parent,
    const Node** old_parent,
    std::vector<const Node*>* to_send) {
  // If the node is not in |roots_| or was never known to this connection then
  // don't notify the client about it.
  if (node->id().connection_id != id_ &&
      known_nodes_.count(NodeIdToTransportId(node->id())) == 0 &&
      !IsNodeDescendantOfRoots(node)) {
    return false;
  }
  if (!IsNodeDescendantOfRoots(*new_parent))
    *new_parent = NULL;
  if (!IsNodeDescendantOfRoots(*old_parent))
    *old_parent = NULL;

  if (*new_parent) {
    // On getting a new parent we may need to communicate new nodes to the
    // client. We do that in the following cases:
    // . New parent is a descendant of the roots. In this case the client
    //   already knows all ancestors, so we only have to communicate descendants
    //   of node the client doesn't know about.
    // . If the client knew about the parent, we have to do the same.
    // . If the client knows about the node and is added to a tree the client
    //   doesn't know about we have to communicate from the root down (the
    //   client is learning about a new root).
    if (root_node_manager_->root()->Contains(*new_parent) ||
        known_nodes_.count(NodeIdToTransportId((*new_parent)->id()))) {
      GetUnknownNodesFrom(node, to_send);
      return true;
    }
    // If parent wasn't known we have to communicate from the root down.
    if (known_nodes_.count(NodeIdToTransportId(node->id()))) {
      // No need to check against |roots_| as client should always know it's
      // |roots_|.
      GetUnknownNodesFrom((*new_parent)->GetRoot(), to_send);
      return true;
    }
  }
  // Otherwise only communicate the change if the node was known. We shouldn't
  // need to communicate any nodes on a remove.
  return known_nodes_.count(NodeIdToTransportId(node->id())) > 0;
}

Array<NodeDataPtr> ViewManagerServiceImpl::NodesToNodeDatas(
    const std::vector<const Node*>& nodes) {
  Array<NodeDataPtr> array(nodes.size());
  for (size_t i = 0; i < nodes.size(); ++i) {
    const Node* node = nodes[i];
    DCHECK(known_nodes_.count(NodeIdToTransportId(node->id())) > 0);
    const Node* parent = node->GetParent();
    // If the parent isn't known, it means the parent is not visible to us (not
    // in roots), and should not be sent over.
    if (parent && known_nodes_.count(NodeIdToTransportId(parent->id())) == 0)
      parent = NULL;
    NodeDataPtr inode(NodeData::New());
    inode->parent_id = NodeIdToTransportId(parent ? parent->id() : NodeId());
    inode->node_id = NodeIdToTransportId(node->id());
    inode->view_id =
        ViewIdToTransportId(node->view() ? node->view()->id() : ViewId());
    inode->bounds = Rect::From(node->bounds());
    array[i] = inode.Pass();
  }
  return array.Pass();
}

void ViewManagerServiceImpl::CreateNode(
    Id transport_node_id,
    const Callback<void(bool)>& callback) {
  const NodeId node_id(NodeIdFromTransportId(transport_node_id));
  if (node_id.connection_id != id_ ||
      node_map_.find(node_id.node_id) != node_map_.end()) {
    callback.Run(false);
    return;
  }
  node_map_[node_id.node_id] = new Node(this, node_id);
  known_nodes_.insert(transport_node_id);
  callback.Run(true);
}

void ViewManagerServiceImpl::DeleteNode(
    Id transport_node_id,
    Id server_change_id,
    const Callback<void(bool)>& callback) {
  const NodeId node_id(NodeIdFromTransportId(transport_node_id));
  bool success = false;
  if (server_change_id == root_node_manager_->next_server_change_id() &&
      CanDeleteNode(node_id)) {
    ViewManagerServiceImpl* connection = root_node_manager_->GetConnection(
        node_id.connection_id);
    success = connection && connection->DeleteNodeImpl(this, node_id);
  }
  callback.Run(success);
}

void ViewManagerServiceImpl::AddNode(
    Id parent_id,
    Id child_id,
    Id server_change_id,
    const Callback<void(bool)>& callback) {
  bool success = false;
  if (server_change_id == root_node_manager_->next_server_change_id()) {
    Node* parent = GetNode(NodeIdFromTransportId(parent_id));
    Node* child = GetNode(NodeIdFromTransportId(child_id));
    if (CanAddNode(parent, child)) {
      success = true;
      RootNodeManager::ScopedChange change(
          this, root_node_manager_,
          RootNodeManager::CHANGE_TYPE_ADVANCE_SERVER_CHANGE_ID, false);
      parent->Add(child);
    }
  }
  callback.Run(success);
}

void ViewManagerServiceImpl::RemoveNodeFromParent(
    Id node_id,
    Id server_change_id,
    const Callback<void(bool)>& callback) {
  bool success = false;
  if (server_change_id == root_node_manager_->next_server_change_id()) {
    Node* node = GetNode(NodeIdFromTransportId(node_id));
    if (CanRemoveNodeFromParent(node)) {
      success = true;
      RootNodeManager::ScopedChange change(
          this, root_node_manager_,
          RootNodeManager::CHANGE_TYPE_ADVANCE_SERVER_CHANGE_ID, false);
      node->GetParent()->Remove(node);
    }
  }
  callback.Run(success);
}

void ViewManagerServiceImpl::ReorderNode(Id node_id,
                                         Id relative_node_id,
                                         OrderDirection direction,
                                         Id server_change_id,
                                         const Callback<void(bool)>& callback) {
  bool success = false;
  if (server_change_id == root_node_manager_->next_server_change_id()) {
    Node* node = GetNode(NodeIdFromTransportId(node_id));
    Node* relative_node = GetNode(NodeIdFromTransportId(relative_node_id));
    if (CanReorderNode(node, relative_node, direction)) {
      success = true;
      RootNodeManager::ScopedChange change(
          this, root_node_manager_,
          RootNodeManager::CHANGE_TYPE_ADVANCE_SERVER_CHANGE_ID, false);
      node->GetParent()->Reorder(node, relative_node, direction);
      root_node_manager_->ProcessNodeReorder(node, relative_node, direction);
    }
  }
  callback.Run(success);
}

void ViewManagerServiceImpl::GetNodeTree(
    Id node_id,
    const Callback<void(Array<NodeDataPtr>)>& callback) {
  Node* node = GetNode(NodeIdFromTransportId(node_id));
  std::vector<const Node*> nodes;
  if (CanGetNodeTree(node)) {
    GetDescendants(node, &nodes);
    for (size_t i = 0; i < nodes.size(); ++i)
      known_nodes_.insert(NodeIdToTransportId(nodes[i]->id()));
  }
  callback.Run(NodesToNodeDatas(nodes));
}

void ViewManagerServiceImpl::CreateView(
    Id transport_view_id,
    const Callback<void(bool)>& callback) {
  const ViewId view_id(ViewIdFromTransportId(transport_view_id));
  if (view_id.connection_id != id_ || view_map_.count(view_id.view_id)) {
    callback.Run(false);
    return;
  }
  view_map_[view_id.view_id] = new View(view_id);
  callback.Run(true);
}

void ViewManagerServiceImpl::DeleteView(
    Id transport_view_id,
    const Callback<void(bool)>& callback) {
  const ViewId view_id(ViewIdFromTransportId(transport_view_id));
  bool did_delete = CanDeleteView(view_id);
  if (did_delete) {
    ViewManagerServiceImpl* connection = root_node_manager_->GetConnection(
        view_id.connection_id);
    did_delete = (connection && connection->DeleteViewImpl(this, view_id));
  }
  callback.Run(did_delete);
}

void ViewManagerServiceImpl::SetView(Id transport_node_id,
                                     Id transport_view_id,
                                     const Callback<void(bool)>& callback) {
  Node* node = GetNode(NodeIdFromTransportId(transport_node_id));
  const ViewId view_id(ViewIdFromTransportId(transport_view_id));
  callback.Run(CanSetView(node, view_id) && SetViewImpl(node, view_id));
}

void ViewManagerServiceImpl::SetViewContents(
    Id view_id,
    ScopedSharedBufferHandle buffer,
    uint32_t buffer_size,
    const Callback<void(bool)>& callback) {
  View* view = GetView(ViewIdFromTransportId(view_id));
  if (!view) {
    callback.Run(false);
    return;
  }
  void* handle_data;
  if (MapBuffer(buffer.get(), 0, buffer_size, &handle_data,
                MOJO_MAP_BUFFER_FLAG_NONE) != MOJO_RESULT_OK) {
    callback.Run(false);
    return;
  }
  SkBitmap bitmap;
  gfx::PNGCodec::Decode(static_cast<const unsigned char*>(handle_data),
                        buffer_size, &bitmap);
  view->SetBitmap(bitmap);
  UnmapBuffer(handle_data);
  callback.Run(true);
}

void ViewManagerServiceImpl::SetFocus(Id node_id,
                                      const Callback<void(bool)> & callback) {
  bool success = false;
  Node* node = GetNode(NodeIdFromTransportId(node_id));
  if (CanSetFocus(node)) {
    success = true;
    node->window()->Focus();
  }
  callback.Run(success);
}

void ViewManagerServiceImpl::SetNodeBounds(
    Id node_id,
    RectPtr bounds,
    const Callback<void(bool)>& callback) {
  if (NodeIdFromTransportId(node_id).connection_id != id_) {
    callback.Run(false);
    return;
  }

  Node* node = GetNode(NodeIdFromTransportId(node_id));
  if (!node) {
    callback.Run(false);
    return;
  }

  RootNodeManager::ScopedChange change(
      this, root_node_manager_,
      RootNodeManager::CHANGE_TYPE_DONT_ADVANCE_SERVER_CHANGE_ID, false);
  gfx::Rect old_bounds = node->window()->bounds();
  node->window()->SetBounds(bounds.To<gfx::Rect>());
  root_node_manager_->ProcessNodeBoundsChanged(
      node, old_bounds, bounds.To<gfx::Rect>());
  callback.Run(true);
}

void ViewManagerServiceImpl::Embed(const String& url,
                                   Array<uint32_t> node_ids,
                                   const Callback<void(bool)>& callback) {
  bool success = CanEmbed(node_ids);
  if (success) {
    // We may already have this connection, if so reuse it.
    ViewManagerServiceImpl* existing_connection =
        root_node_manager_->GetConnectionByCreator(id_, url.To<std::string>());
    if (existing_connection)
      success = existing_connection->AddRoots(node_ids.storage());
    else
      root_node_manager_->Embed(id_, url, node_ids);
  }
  callback.Run(success);
}

void ViewManagerServiceImpl::DispatchOnViewInputEvent(Id transport_view_id,
                                                      EventPtr event) {
  // We only allow the WM to dispatch events. At some point this function will
  // move to a separate interface and the check can go away.
  if (id_ != kWindowManagerConnection)
    return;

  const ViewId view_id(ViewIdFromTransportId(transport_view_id));
  ViewManagerServiceImpl* connection = root_node_manager_->GetConnection(
      view_id.connection_id);
  if (connection)
    connection->client()->OnViewInputEvent(
        transport_view_id,
        event.Pass(),
        base::Bind(&base::DoNothing));
}

void ViewManagerServiceImpl::OnNodeHierarchyChanged(const Node* node,
                                                    const Node* new_parent,
                                                    const Node* old_parent) {
  root_node_manager_->ProcessNodeHierarchyChanged(node, new_parent, old_parent);
}

void ViewManagerServiceImpl::OnNodeViewReplaced(const Node* node,
                                                const View* new_view,
                                                const View* old_view) {
  root_node_manager_->ProcessNodeViewReplaced(node, new_view, old_view);
}

void ViewManagerServiceImpl::OnViewInputEvent(const View* view,
                                              const ui::Event* event) {
  root_node_manager_->DispatchViewInputEventToWindowManager(view, event);
}

void ViewManagerServiceImpl::OnConnectionEstablished() {
  root_node_manager_->AddConnection(this);

  std::vector<const Node*> to_send;
  if (roots_.empty()) {
    GetUnknownNodesFrom(root_node_manager_->root(), &to_send);
  } else {
    for (NodeIdSet::const_iterator i = roots_.begin(); i != roots_.end(); ++i)
      GetUnknownNodesFrom(GetNode(NodeIdFromTransportId(*i)), &to_send);
  }

  client()->OnViewManagerConnectionEstablished(
      id_,
      creator_url_,
      root_node_manager_->next_server_change_id(),
      NodesToNodeDatas(to_send));
}

}  // namespace service
}  // namespace view_manager
}  // namespace mojo
