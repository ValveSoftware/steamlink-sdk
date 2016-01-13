// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_TEST_CHANGE_TRACKER_H_
#define MOJO_SERVICES_VIEW_MANAGER_TEST_CHANGE_TRACKER_H_

#include <string>
#include <vector>

#include "mojo/public/cpp/bindings/array.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "ui/gfx/rect.h"

namespace mojo {
namespace view_manager {
namespace service {

enum ChangeType {
  CHANGE_TYPE_CONNECTION_ESTABLISHED,
  CHANGE_TYPE_ROOTS_ADDED,
  CHANGE_TYPE_SERVER_CHANGE_ID_ADVANCED,
  CHANGE_TYPE_NODE_BOUNDS_CHANGED,
  CHANGE_TYPE_NODE_HIERARCHY_CHANGED,
  CHANGE_TYPE_NODE_REORDERED,
  CHANGE_TYPE_NODE_DELETED,
  CHANGE_TYPE_VIEW_DELETED,
  CHANGE_TYPE_VIEW_REPLACED,
  CHANGE_TYPE_INPUT_EVENT,
};

// TODO(sky): consider nuking and converting directly to NodeData.
struct TestNode {
  // Returns a string description of this.
  std::string ToString() const;

  Id parent_id;
  Id node_id;
  Id view_id;
};

// Tracks a call to ViewManagerClient. See the individual functions for the
// fields that are used.
struct Change {
  Change();
  ~Change();

  ChangeType type;
  ConnectionSpecificId connection_id;
  Id change_id;
  std::vector<TestNode> nodes;
  Id node_id;
  Id node_id2;
  Id node_id3;
  Id view_id;
  Id view_id2;
  gfx::Rect bounds;
  gfx::Rect bounds2;
  int32 event_action;
  String creator_url;
  OrderDirection direction;
};

// Converts Changes to string descriptions.
std::vector<std::string> ChangesToDescription1(
    const std::vector<Change>& changes);

// Returns a string description of |changes[0].nodes|. Returns an empty string
// if change.size() != 1.
std::string ChangeNodeDescription(const std::vector<Change>& changes);

// Converts NodeDatas to TestNodes.
void NodeDatasToTestNodes(const Array<NodeDataPtr>& data,
                          std::vector<TestNode>* test_nodes);

// TestChangeTracker is used to record ViewManagerClient functions. It notifies
// a delegate any time a change is added.
class TestChangeTracker {
 public:
  // Used to notify the delegate when a change is added. A change corresponds to
  // a single ViewManagerClient function.
  class Delegate {
   public:
    virtual void OnChangeAdded() = 0;

   protected:
    virtual ~Delegate() {}
  };

  TestChangeTracker();
  ~TestChangeTracker();

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  std::vector<Change>* changes() { return &changes_; }

  // Each of these functions generate a Change. There is one per
  // ViewManagerClient function.
  void OnViewManagerConnectionEstablished(ConnectionSpecificId connection_id,
                                          const String& creator_url,
                                          Id next_server_change_id,
                                          Array<NodeDataPtr> nodes);
  void OnRootsAdded(Array<NodeDataPtr> nodes);
  void OnServerChangeIdAdvanced(Id change_id);
  void OnNodeBoundsChanged(Id node_id, RectPtr old_bounds, RectPtr new_bounds);
  void OnNodeHierarchyChanged(Id node_id,
                              Id new_parent_id,
                              Id old_parent_id,
                              Id server_change_id,
                              Array<NodeDataPtr> nodes);
  void OnNodeReordered(Id node_id,
                       Id relative_node_id,
                       OrderDirection direction,
                       Id server_change_id);
  void OnNodeDeleted(Id node_id, Id server_change_id);
  void OnViewDeleted(Id view_id);
  void OnNodeViewReplaced(Id node_id, Id new_view_id, Id old_view_id);
  void OnViewInputEvent(Id view_id, EventPtr event);

 private:
  void AddChange(const Change& change);

  Delegate* delegate_;
  std::vector<Change> changes_;

  DISALLOW_COPY_AND_ASSIGN(TestChangeTracker);
};

}  // namespace service
}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_TEST_CHANGE_TRACKER_H_
