// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_CLIENT_IMPL_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_CLIENT_IMPL_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"

class SkBitmap;

namespace mojo {
namespace view_manager {

class ViewManager;
class ViewManagerTransaction;

// Manages the connection with the View Manager service.
class ViewManagerClientImpl : public ViewManager,
                              public InterfaceImpl<ViewManagerClient> {
 public:
  explicit ViewManagerClientImpl(ViewManagerDelegate* delegate);
  virtual ~ViewManagerClientImpl();

  bool connected() const { return connected_; }
  ConnectionSpecificId connection_id() const { return connection_id_; }

  // API exposed to the node/view implementations that pushes local changes to
  // the service.
  Id CreateNode();
  void DestroyNode(Id node_id);

  Id CreateView();
  void DestroyView(Id view_id);

  // These methods take TransportIds. For views owned by the current connection,
  // the connection id high word can be zero. In all cases, the TransportId 0x1
  // refers to the root node.
  void AddChild(Id child_id, Id parent_id);
  void RemoveChild(Id child_id, Id parent_id);

  void Reorder(Id node_id, Id relative_node_id, OrderDirection direction);

  // Returns true if the specified node/view was created by this connection.
  bool OwnsNode(Id id) const;
  bool OwnsView(Id id) const;

  void SetActiveView(Id node_id, Id view_id);
  void SetBounds(Id node_id, const gfx::Rect& bounds);
  void SetViewContents(Id view_id, const SkBitmap& contents);
  void SetFocus(Id node_id);

  void Embed(const String& url, Id node_id);

  void set_changes_acked_callback(const base::Callback<void(void)>& callback) {
    changes_acked_callback_ = callback;
  }
  void ClearChangesAckedCallback() {
    changes_acked_callback_ = base::Callback<void(void)>();
  }

  // Start/stop tracking nodes & views. While tracked, they can be retrieved via
  // ViewManager::GetNode/ViewById.
  void AddNode(Node* node);
  void RemoveNode(Id node_id);

  void AddView(View* view);
  void RemoveView(Id view_id);

 private:
  friend class RootObserver;
  friend class ViewManagerTransaction;

  typedef ScopedVector<ViewManagerTransaction> Transactions;
  typedef std::map<Id, Node*> IdToNodeMap;
  typedef std::map<Id, View*> IdToViewMap;

  // Overridden from ViewManager:
  virtual const std::string& GetEmbedderURL() const OVERRIDE;
  virtual const std::vector<Node*>& GetRoots() const OVERRIDE;
  virtual Node* GetNodeById(Id id) OVERRIDE;
  virtual View* GetViewById(Id id) OVERRIDE;

  // Overridden from InterfaceImpl:
  virtual void OnConnectionEstablished() OVERRIDE;

  // Overridden from ViewManagerClient:
  virtual void OnViewManagerConnectionEstablished(
      ConnectionSpecificId connection_id,
      const String& creator_url,
      Id next_server_change_id,
      Array<NodeDataPtr> nodes) OVERRIDE;
  virtual void OnRootsAdded(Array<NodeDataPtr> nodes) OVERRIDE;
  virtual void OnServerChangeIdAdvanced(Id next_server_change_id) OVERRIDE;
  virtual void OnNodeBoundsChanged(Id node_id,
                                   RectPtr old_bounds,
                                   RectPtr new_bounds) OVERRIDE;
  virtual void OnNodeHierarchyChanged(Id node_id,
                                      Id new_parent_id,
                                      Id old_parent_id,
                                      Id server_change_id,
                                      Array<NodeDataPtr> nodes) OVERRIDE;
  virtual void OnNodeReordered(Id node_id,
                               Id relative_node_id,
                               OrderDirection direction,
                               Id server_change_id) OVERRIDE;
  virtual void OnNodeDeleted(Id node_id, Id server_change_id) OVERRIDE;
  virtual void OnNodeViewReplaced(Id node,
                                  Id new_view_id,
                                  Id old_view_id) OVERRIDE;
  virtual void OnViewDeleted(Id view_id) OVERRIDE;
  virtual void OnViewInputEvent(Id view,
                                EventPtr event,
                                const Callback<void()>& callback) OVERRIDE;
  virtual void DispatchOnViewInputEvent(Id view_id, EventPtr event) OVERRIDE;

  // Sync the client model with the service by enumerating the pending
  // transaction queue and applying them in order.
  void Sync();

  // Removes |transaction| from the pending queue. |transaction| must be at the
  // front of the queue.
  void RemoveFromPendingQueue(ViewManagerTransaction* transaction);

  void AddRoot(Node* root);
  void RemoveRoot(Node* root);

  bool connected_;
  ConnectionSpecificId connection_id_;
  ConnectionSpecificId next_id_;
  Id next_server_change_id_;

  std::string creator_url_;

  Transactions pending_transactions_;

  base::Callback<void(void)> changes_acked_callback_;

  ViewManagerDelegate* delegate_;

  std::vector<Node*> roots_;

  IdToNodeMap nodes_;
  IdToViewMap views_;

  ViewManagerService* service_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerClientImpl);
};

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_CLIENT_IMPL_H_
