// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/test_change_tracker.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/view_manager/util.h"

namespace mojo {
namespace view_manager {
namespace service {

std::string NodeIdToString(Id id) {
  return (id == 0) ? "null" :
      base::StringPrintf("%d,%d", HiWord(id), LoWord(id));
}

namespace {

std::string RectToString(const gfx::Rect& rect) {
  return base::StringPrintf("%d,%d %dx%d", rect.x(), rect.y(), rect.width(),
                            rect.height());
}

std::string DirectionToString(OrderDirection direction) {
  return direction == ORDER_ABOVE ? "above" : "below";
}

std::string ChangeToDescription1(const Change& change) {
  switch (change.type) {
    case CHANGE_TYPE_CONNECTION_ESTABLISHED:
      return base::StringPrintf("OnConnectionEstablished creator=%s",
                                change.creator_url.data());

    case CHANGE_TYPE_ROOTS_ADDED:
      return "OnRootsAdded";

    case CHANGE_TYPE_SERVER_CHANGE_ID_ADVANCED:
      return base::StringPrintf(
          "ServerChangeIdAdvanced %d", static_cast<int>(change.change_id));


    case CHANGE_TYPE_NODE_BOUNDS_CHANGED:
      return base::StringPrintf(
          "BoundsChanged node=%s old_bounds=%s new_bounds=%s",
          NodeIdToString(change.node_id).c_str(),
          RectToString(change.bounds).c_str(),
          RectToString(change.bounds2).c_str());

    case CHANGE_TYPE_NODE_HIERARCHY_CHANGED:
      return base::StringPrintf(
            "HierarchyChanged change_id=%d node=%s new_parent=%s old_parent=%s",
            static_cast<int>(change.change_id),
            NodeIdToString(change.node_id).c_str(),
            NodeIdToString(change.node_id2).c_str(),
            NodeIdToString(change.node_id3).c_str());

    case CHANGE_TYPE_NODE_REORDERED:
      return base::StringPrintf(
          "Reordered change_id=%d node=%s relative=%s direction=%s",
          static_cast<int>(change.change_id),
          NodeIdToString(change.node_id).c_str(),
          NodeIdToString(change.node_id2).c_str(),
          DirectionToString(change.direction).c_str());

    case CHANGE_TYPE_NODE_DELETED:
      return base::StringPrintf("NodeDeleted change_id=%d node=%s",
                                static_cast<int>(change.change_id),
                                NodeIdToString(change.node_id).c_str());

    case CHANGE_TYPE_VIEW_DELETED:
      return base::StringPrintf("ViewDeleted view=%s",
                                NodeIdToString(change.view_id).c_str());

    case CHANGE_TYPE_VIEW_REPLACED:
      return base::StringPrintf(
          "ViewReplaced node=%s new_view=%s old_view=%s",
          NodeIdToString(change.node_id).c_str(),
          NodeIdToString(change.view_id).c_str(),
          NodeIdToString(change.view_id2).c_str());

    case CHANGE_TYPE_INPUT_EVENT:
      return base::StringPrintf(
          "InputEvent view=%s event_action=%d",
          NodeIdToString(change.view_id).c_str(),
          change.event_action);
  }
  return std::string();
}

}  // namespace

std::vector<std::string> ChangesToDescription1(
    const std::vector<Change>& changes) {
  std::vector<std::string> strings(changes.size());
  for (size_t i = 0; i < changes.size(); ++i)
    strings[i] = ChangeToDescription1(changes[i]);
  return strings;
}

std::string ChangeNodeDescription(const std::vector<Change>& changes) {
  if (changes.size() != 1)
    return std::string();
  std::vector<std::string> node_strings(changes[0].nodes.size());
  for (size_t i = 0; i < changes[0].nodes.size(); ++i)
    node_strings[i] = "[" + changes[0].nodes[i].ToString() + "]";
  return JoinString(node_strings, ',');
}

void NodeDatasToTestNodes(const Array<NodeDataPtr>& data,
                          std::vector<TestNode>* test_nodes) {
  for (size_t i = 0; i < data.size(); ++i) {
    TestNode node;
    node.parent_id = data[i]->parent_id;
    node.node_id = data[i]->node_id;
    node.view_id = data[i]->view_id;
    test_nodes->push_back(node);
  }
}

Change::Change()
    : type(CHANGE_TYPE_CONNECTION_ESTABLISHED),
      connection_id(0),
      change_id(0),
      node_id(0),
      node_id2(0),
      node_id3(0),
      view_id(0),
      view_id2(0),
      event_action(0),
      direction(ORDER_ABOVE) {}

Change::~Change() {
}

TestChangeTracker::TestChangeTracker()
    : delegate_(NULL) {
}

TestChangeTracker::~TestChangeTracker() {
}

void TestChangeTracker::OnViewManagerConnectionEstablished(
    ConnectionSpecificId connection_id,
    const String& creator_url,
    Id next_server_change_id,
    Array<NodeDataPtr> nodes) {
  Change change;
  change.type = CHANGE_TYPE_CONNECTION_ESTABLISHED;
  change.connection_id = connection_id;
  change.change_id = next_server_change_id;
  change.creator_url = creator_url;
  NodeDatasToTestNodes(nodes, &change.nodes);
  AddChange(change);
}

void TestChangeTracker::OnRootsAdded(Array<NodeDataPtr> nodes) {
  Change change;
  change.type = CHANGE_TYPE_ROOTS_ADDED;
  NodeDatasToTestNodes(nodes, &change.nodes);
  AddChange(change);
}

void TestChangeTracker::OnServerChangeIdAdvanced(Id change_id) {
  Change change;
  change.type = CHANGE_TYPE_SERVER_CHANGE_ID_ADVANCED;
  change.change_id = change_id;
  AddChange(change);
}

void TestChangeTracker::OnNodeBoundsChanged(Id node_id,
                                            RectPtr old_bounds,
                                            RectPtr new_bounds) {
  Change change;
  change.type = CHANGE_TYPE_NODE_BOUNDS_CHANGED;
  change.node_id = node_id;
  change.bounds = old_bounds.To<gfx::Rect>();
  change.bounds2 = new_bounds.To<gfx::Rect>();
  AddChange(change);
}

void TestChangeTracker::OnNodeHierarchyChanged(Id node_id,
                                               Id new_parent_id,
                                               Id old_parent_id,
                                               Id server_change_id,
                                               Array<NodeDataPtr> nodes) {
  Change change;
  change.type = CHANGE_TYPE_NODE_HIERARCHY_CHANGED;
  change.node_id = node_id;
  change.node_id2 = new_parent_id;
  change.node_id3 = old_parent_id;
  change.change_id = server_change_id;
  NodeDatasToTestNodes(nodes, &change.nodes);
  AddChange(change);
}

void TestChangeTracker::OnNodeReordered(Id node_id,
                                        Id relative_node_id,
                                        OrderDirection direction,
                                        Id server_change_id) {
  Change change;
  change.type = CHANGE_TYPE_NODE_REORDERED;
  change.node_id = node_id;
  change.node_id2 = relative_node_id;
  change.direction = direction;
  change.change_id = server_change_id;
  AddChange(change);
}

void TestChangeTracker::OnNodeDeleted(Id node_id, Id server_change_id) {
  Change change;
  change.type = CHANGE_TYPE_NODE_DELETED;
  change.node_id = node_id;
  change.change_id = server_change_id;
  AddChange(change);
}

void TestChangeTracker::OnViewDeleted(Id view_id) {
  Change change;
  change.type = CHANGE_TYPE_VIEW_DELETED;
  change.view_id = view_id;
  AddChange(change);
}

void TestChangeTracker::OnNodeViewReplaced(Id node_id,
                                           Id new_view_id,
                                           Id old_view_id) {
  Change change;
  change.type = CHANGE_TYPE_VIEW_REPLACED;
  change.node_id = node_id;
  change.view_id = new_view_id;
  change.view_id2 = old_view_id;
  AddChange(change);
}

void TestChangeTracker::OnViewInputEvent(Id view_id, EventPtr event) {
  Change change;
  change.type = CHANGE_TYPE_INPUT_EVENT;
  change.view_id = view_id;
  change.event_action = event->action;
  AddChange(change);
}

void TestChangeTracker::AddChange(const Change& change) {
  changes_.push_back(change);
  if (delegate_)
    delegate_->OnChangeAdded();
}

std::string TestNode::ToString() const {
  return base::StringPrintf("node=%s parent=%s view=%s",
                            NodeIdToString(node_id).c_str(),
                            NodeIdToString(parent_id).c_str(),
                            NodeIdToString(view_id).c_str());
}

}  // namespace service
}  // namespace view_manager
}  // namespace mojo
