// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_ROOT_NODE_MANAGER_H_
#define MOJO_SERVICES_VIEW_MANAGER_ROOT_NODE_MANAGER_H_

#include <map>
#include <set>

#include "base/basictypes.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/node.h"
#include "mojo/services/view_manager/node_delegate.h"
#include "mojo/services/view_manager/root_view_manager.h"
#include "mojo/services/view_manager/view_manager_export.h"

namespace ui {
class Event;
}

namespace mojo {

class ServiceProvider;

namespace view_manager {
namespace service {

class RootViewManagerDelegate;
class View;
class ViewManagerServiceImpl;

// RootNodeManager is responsible for managing the set of
// ViewManagerServiceImpls as well as providing the root of the node hierarchy.
class MOJO_VIEW_MANAGER_EXPORT RootNodeManager : public NodeDelegate {
 public:
  // Used to indicate if the server id should be incremented after notifiying
  // clients of the change.
  enum ChangeType {
    CHANGE_TYPE_ADVANCE_SERVER_CHANGE_ID,
    CHANGE_TYPE_DONT_ADVANCE_SERVER_CHANGE_ID,
  };

  // Create when a ViewManagerServiceImpl is about to make a change. Ensures
  // clients are notified of the correct change id.
  class ScopedChange {
   public:
    ScopedChange(ViewManagerServiceImpl* connection,
                 RootNodeManager* root,
                 RootNodeManager::ChangeType change_type,
                 bool is_delete_node);
    ~ScopedChange();

    ConnectionSpecificId connection_id() const { return connection_id_; }
    ChangeType change_type() const { return change_type_; }
    bool is_delete_node() const { return is_delete_node_; }

    // Marks the connection with the specified id as having seen a message.
    void MarkConnectionAsMessaged(ConnectionSpecificId connection_id) {
      message_ids_.insert(connection_id);
    }

    // Returns true if MarkConnectionAsMessaged(connection_id) was invoked.
    bool DidMessageConnection(ConnectionSpecificId connection_id) const {
      return message_ids_.count(connection_id) > 0;
    }

   private:
    RootNodeManager* root_;
    const ConnectionSpecificId connection_id_;
    const ChangeType change_type_;
    const bool is_delete_node_;

    // See description of MarkConnectionAsMessaged/DidMessageConnection.
    std::set<ConnectionSpecificId> message_ids_;

    DISALLOW_COPY_AND_ASSIGN(ScopedChange);
  };

  RootNodeManager(ServiceProvider* service_provider,
                  RootViewManagerDelegate* view_manager_delegate);
  virtual ~RootNodeManager();

  // Returns the id for the next ViewManagerServiceImpl.
  ConnectionSpecificId GetAndAdvanceNextConnectionId();

  Id next_server_change_id() const {
    return next_server_change_id_;
  }

  void AddConnection(ViewManagerServiceImpl* connection);
  void RemoveConnection(ViewManagerServiceImpl* connection);

  // Establishes the initial client. Similar to Connect(), but the resulting
  // client is allowed to do anything.
  void EmbedRoot(const std::string& url);

  // See description of ViewManagerService::Embed() for details. This assumes
  // |node_ids| has been validated.
  void Embed(ConnectionSpecificId creator_id,
             const String& url,
             const Array<Id>& node_ids);

  // Returns the connection by id.
  ViewManagerServiceImpl* GetConnection(ConnectionSpecificId connection_id);

  // Returns the Node identified by |id|.
  Node* GetNode(const NodeId& id);

  // Returns the View identified by |id|.
  View* GetView(const ViewId& id);

  Node* root() { return &root_; }

  bool IsProcessingChange() const { return current_change_ != NULL; }

  bool is_processing_delete_node() const {
    return current_change_ && current_change_->is_delete_node(); }

  // Invoked when a connection messages a client about the change. This is used
  // to avoid sending ServerChangeIdAdvanced() unnecessarily.
  void OnConnectionMessagedClient(ConnectionSpecificId id);

  // Returns true if OnConnectionMessagedClient() was invoked for id.
  bool DidConnectionMessageClient(ConnectionSpecificId id) const;

  ViewManagerServiceImpl* GetConnectionByCreator(
      ConnectionSpecificId creator_id,
      const std::string& url) const;

  void DispatchViewInputEventToWindowManager(const View* view,
                                             const ui::Event* event);

  // These functions trivially delegate to all ViewManagerServiceImpls, which in
  // term notify their clients.
  void ProcessNodeBoundsChanged(const Node* node,
                                const gfx::Rect& old_bounds,
                                const gfx::Rect& new_bounds);
  void ProcessNodeHierarchyChanged(const Node* node,
                                   const Node* new_parent,
                                   const Node* old_parent);
  void ProcessNodeReorder(const Node* node,
                          const Node* relative_node,
                          const OrderDirection direction);
  void ProcessNodeViewReplaced(const Node* node,
                               const View* new_view_id,
                               const View* old_view_id);
  void ProcessNodeDeleted(const NodeId& node);
  void ProcessViewDeleted(const ViewId& view);

 private:
  // Used to setup any static state needed by RootNodeManager.
  struct Context {
    Context();
    ~Context();
  };

  typedef std::map<ConnectionSpecificId, ViewManagerServiceImpl*> ConnectionMap;

  // Invoked when a connection is about to make a change.  Subsequently followed
  // by FinishChange() once the change is done.
  //
  // Changes should never nest, meaning each PrepareForChange() must be
  // balanced with a call to FinishChange() with no PrepareForChange()
  // in between.
  void PrepareForChange(ScopedChange* change);

  // Balances a call to PrepareForChange().
  void FinishChange();

  // Returns true if the specified connection originated the current change.
  bool IsChangeSource(ConnectionSpecificId connection_id) const {
    return current_change_ && current_change_->connection_id() == connection_id;
  }

  // Implementation of the two embed variants.
  ViewManagerServiceImpl* EmbedImpl(ConnectionSpecificId creator_id,
                                   const String& url,
                                   const Array<Id>& node_ids);

  // Overridden from NodeDelegate:
  virtual void OnNodeHierarchyChanged(const Node* node,
                                      const Node* new_parent,
                                      const Node* old_parent) OVERRIDE;
  virtual void OnNodeViewReplaced(const Node* node,
                                  const View* new_view,
                                  const View* old_view) OVERRIDE;
  virtual void OnViewInputEvent(const View* view,
                                const ui::Event* event) OVERRIDE;

  Context context_;

  ServiceProvider* service_provider_;

  // ID to use for next ViewManagerServiceImpl.
  ConnectionSpecificId next_connection_id_;

  Id next_server_change_id_;

  // Set of ViewManagerServiceImpls.
  ConnectionMap connection_map_;

  RootViewManager root_view_manager_;

  // Root node.
  Node root_;

  // Set of ViewManagerServiceImpls created by way of Connect(). These have to
  // be explicitly destroyed.
  std::set<ViewManagerServiceImpl*> connections_created_by_connect_;

  // If non-null we're processing a change. The ScopedChange is not owned by us
  // (it's created on the stack by ViewManagerServiceImpl).
  ScopedChange* current_change_;

  DISALLOW_COPY_AND_ASSIGN(RootNodeManager);
};

}  // namespace service
}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_ROOT_NODE_MANAGER_H_
