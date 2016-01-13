// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_SERVICE_IMPL_H_
#define MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_SERVICE_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/node_delegate.h"
#include "mojo/services/view_manager/view_manager_export.h"

namespace gfx {
class Rect;
}

namespace mojo {
namespace view_manager {
namespace service {

class Node;
class RootNodeManager;
class View;

#if defined(OS_WIN)
// Equivalent of NON_EXPORTED_BASE which does not work with the template snafu
// below.
#pragma warning(push)
#pragma warning(disable : 4275)
#endif

// Manages a connection from the client.
class MOJO_VIEW_MANAGER_EXPORT ViewManagerServiceImpl
    : public InterfaceImpl<ViewManagerService>,
      public NodeDelegate {
 public:
  ViewManagerServiceImpl(RootNodeManager* root_node_manager,
                         ConnectionSpecificId creator_id,
                         const std::string& creator_url,
                         const std::string& url);
  virtual ~ViewManagerServiceImpl();

  // Used to mark this connection as originating from a call to
  // ViewManagerService::Connect(). When set OnConnectionError() deletes |this|.
  void set_delete_on_connection_error() { delete_on_connection_error_ = true; }

  ConnectionSpecificId id() const { return id_; }
  ConnectionSpecificId creator_id() const { return creator_id_; }
  const std::string& url() const { return url_; }

  // Returns the Node with the specified id.
  Node* GetNode(const NodeId& id) {
    return const_cast<Node*>(
        const_cast<const ViewManagerServiceImpl*>(this)->GetNode(id));
  }
  const Node* GetNode(const NodeId& id) const;

  // Returns the View with the specified id.
  View* GetView(const ViewId& id) {
    return const_cast<View*>(
        const_cast<const ViewManagerServiceImpl*>(this)->GetView(id));
  }
  const View* GetView(const ViewId& id) const;

  void SetRoots(const Array<Id>& node_ids);

  // Invoked when a connection is destroyed.
  void OnViewManagerServiceImplDestroyed(ConnectionSpecificId id);

  // The following methods are invoked after the corresponding change has been
  // processed. They do the appropriate bookkeeping and update the client as
  // necessary.
  void ProcessNodeBoundsChanged(const Node* node,
                                const gfx::Rect& old_bounds,
                                const gfx::Rect& new_bounds,
                                bool originated_change);
  void ProcessNodeHierarchyChanged(const Node* node,
                                   const Node* new_parent,
                                   const Node* old_parent,
                                   Id server_change_id,
                                   bool originated_change);
  void ProcessNodeReorder(const Node* node,
                          const Node* relative_node,
                          OrderDirection direction,
                          Id server_change_id,
                          bool originated_change);
  void ProcessNodeViewReplaced(const Node* node,
                               const View* new_view,
                               const View* old_view,
                               bool originated_change);
  void ProcessNodeDeleted(const NodeId& node,
                          Id server_change_id,
                          bool originated_change);
  void ProcessViewDeleted(const ViewId& view, bool originated_change);

  // TODO(sky): move this to private section (currently can't because of
  // bindings).
  // InterfaceImp overrides:
  virtual void OnConnectionError() MOJO_OVERRIDE;

 private:
  typedef std::map<ConnectionSpecificId, Node*> NodeMap;
  typedef std::map<ConnectionSpecificId, View*> ViewMap;
  typedef base::hash_set<Id> NodeIdSet;

  // These functions return true if the corresponding mojom function is allowed
  // for this connection.
  bool CanRemoveNodeFromParent(const Node* node) const;
  bool CanAddNode(const Node* parent, const Node* child) const;
  bool CanReorderNode(const Node* node,
                      const Node* relative_node,
                      OrderDirection direction) const;
  bool CanDeleteNode(const NodeId& node_id) const;
  bool CanDeleteView(const ViewId& view_id) const;
  bool CanSetView(const Node* node, const ViewId& view_id) const;
  bool CanSetFocus(const Node* node) const;
  bool CanGetNodeTree(const Node* node) const;
  bool CanEmbed(const mojo::Array<uint32_t>& node_ids) const;

  // Deletes a node owned by this connection. Returns true on success. |source|
  // is the connection that originated the change.
  bool DeleteNodeImpl(ViewManagerServiceImpl* source, const NodeId& node_id);

  // Deletes a view owned by this connection. Returns true on success. |source|
  // is the connection that originated the change.
  bool DeleteViewImpl(ViewManagerServiceImpl* source, const ViewId& view_id);

  // Sets the view associated with a node.
  bool SetViewImpl(Node* node, const ViewId& view_id);

  // If |node| is known (in |known_nodes_|) does nothing. Otherwise adds |node|
  // to |nodes|, marks |node| as known and recurses.
  void GetUnknownNodesFrom(const Node* node, std::vector<const Node*>* nodes);

  // Removes |node| and all its descendants from |known_nodes_|. This does not
  // recurse through nodes that were created by this connection.
  void RemoveFromKnown(const Node* node);

  // Adds |node_ids| to roots, returning true if at least one of the nodes was
  // not already a root. If at least one of the nodes was not already a root
  // the client is told of the new roots.
  bool AddRoots(const std::vector<Id>& node_ids);

  // Returns true if |node| is a non-null and a descendant of |roots_| (or
  // |roots_| is empty).
  bool IsNodeDescendantOfRoots(const Node* node) const;

  // Returns true if notification should be sent of a hierarchy change. If true
  // is returned, any nodes that need to be sent to the client are added to
  // |to_send|.
  bool ShouldNotifyOnHierarchyChange(const Node* node,
                                     const Node** new_parent,
                                     const Node** old_parent,
                                     std::vector<const Node*>* to_send);

  // Converts an array of Nodes to NodeDatas. This assumes all the nodes are
  // valid for the client. The parent of nodes the client is not allowed to see
  // are set to NULL (in the returned NodeDatas).
  Array<NodeDataPtr> NodesToNodeDatas(const std::vector<const Node*>& nodes);

  // Overridden from ViewManagerService:
  virtual void CreateNode(Id transport_node_id,
                          const Callback<void(bool)>& callback) OVERRIDE;
  virtual void DeleteNode(Id transport_node_id,
                          Id server_change_id,
                          const Callback<void(bool)>& callback) OVERRIDE;
  virtual void AddNode(Id parent_id,
                       Id child_id,
                       Id server_change_id,
                       const Callback<void(bool)>& callback) OVERRIDE;
  virtual void RemoveNodeFromParent(
      Id node_id,
      Id server_change_id,
      const Callback<void(bool)>& callback) OVERRIDE;
  virtual void ReorderNode(Id node_id,
                           Id relative_node_id,
                           OrderDirection direction,
                           Id server_change_id,
                           const Callback<void(bool)>& callback) OVERRIDE;
  virtual void GetNodeTree(
      Id node_id,
      const Callback<void(Array<NodeDataPtr>)>& callback) OVERRIDE;
  virtual void CreateView(Id transport_view_id,
                          const Callback<void(bool)>& callback) OVERRIDE;
  virtual void DeleteView(Id transport_view_id,
                          const Callback<void(bool)>& callback) OVERRIDE;
  virtual void SetView(Id transport_node_id,
                       Id transport_view_id,
                       const Callback<void(bool)>& callback) OVERRIDE;
  virtual void SetViewContents(Id view_id,
                               ScopedSharedBufferHandle buffer,
                               uint32_t buffer_size,
                               const Callback<void(bool)>& callback) OVERRIDE;
  virtual void SetFocus(Id node_id,
                        const Callback<void(bool)> & callback) OVERRIDE;
  virtual void SetNodeBounds(Id node_id,
                             RectPtr bounds,
                             const Callback<void(bool)>& callback) OVERRIDE;
  virtual void Embed(const mojo::String& url,
                     mojo::Array<uint32_t> node_ids,
                     const mojo::Callback<void(bool)>& callback) OVERRIDE;
  virtual void DispatchOnViewInputEvent(Id transport_view_id,
                                        EventPtr event) OVERRIDE;

  // Overridden from NodeDelegate:
  virtual void OnNodeHierarchyChanged(const Node* node,
                                      const Node* new_parent,
                                      const Node* old_parent) OVERRIDE;
  virtual void OnNodeViewReplaced(const Node* node,
                                  const View* new_view,
                                  const View* old_view) OVERRIDE;
  virtual void OnViewInputEvent(const View* view,
                                const ui::Event* event) OVERRIDE;

  // InterfaceImp overrides:
  virtual void OnConnectionEstablished() MOJO_OVERRIDE;

  RootNodeManager* root_node_manager_;

  // Id of this connection as assigned by RootNodeManager.
  const ConnectionSpecificId id_;

  // URL this connection was created for.
  const std::string url_;

  // ID of the connection that created us. If 0 it indicates either we were
  // created by the root, or the connection that created us has been destroyed.
  ConnectionSpecificId creator_id_;

  // The URL of the app that embedded the app this connection was created for.
  const std::string creator_url_;

  NodeMap node_map_;

  ViewMap view_map_;

  // The set of nodes that has been communicated to the client.
  NodeIdSet known_nodes_;

  // This is the set of nodes the connection can parent nodes to (in addition to
  // any nodes created by this connection). If empty the connection can
  // manipulate any nodes (except for deleting other connections nodes/views).
  // The connection can not delete or move these. If this is set to a non-empty
  // value and all the nodes are deleted (by another connection), then an
  // invalid node is added here to ensure this connection is still constrained.
  NodeIdSet roots_;

  // See description above setter.
  bool delete_on_connection_error_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerServiceImpl);
};

#if defined(OS_WIN)
#pragma warning(pop)
#endif

}  // namespace service
}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_SERVICE_IMPL_H_
