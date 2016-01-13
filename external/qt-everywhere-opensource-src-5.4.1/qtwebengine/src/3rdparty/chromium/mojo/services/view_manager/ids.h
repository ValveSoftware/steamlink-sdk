// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_IDS_H_
#define MOJO_SERVICES_VIEW_MANAGER_IDS_H_

#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/cpp/view_manager/util.h"
#include "mojo/services/view_manager/view_manager_export.h"

namespace mojo {
namespace view_manager {
namespace service {

// Connection id reserved for the root.
const ConnectionSpecificId kRootConnection = 0;

// TODO(sky): remove this, temporary while window manager API is in existing
// api.
const ConnectionSpecificId kWindowManagerConnection = 1;

// Adds a bit of type safety to node ids.
struct MOJO_VIEW_MANAGER_EXPORT NodeId {
  NodeId(ConnectionSpecificId connection_id, ConnectionSpecificId node_id)
      : connection_id(connection_id),
        node_id(node_id) {}
  NodeId() : connection_id(0), node_id(0) {}

  bool operator==(const NodeId& other) const {
    return other.connection_id == connection_id &&
        other.node_id == node_id;
  }

  bool operator!=(const NodeId& other) const {
    return !(*this == other);
  }

  ConnectionSpecificId connection_id;
  ConnectionSpecificId node_id;
};

// Adds a bit of type safety to view ids.
struct MOJO_VIEW_MANAGER_EXPORT ViewId {
  ViewId(ConnectionSpecificId connection_id, ConnectionSpecificId view_id)
      : connection_id(connection_id),
        view_id(view_id) {}
  ViewId() : connection_id(0), view_id(0) {}

  bool operator==(const ViewId& other) const {
    return other.connection_id == connection_id &&
        other.view_id == view_id;
  }

  bool operator!=(const ViewId& other) const {
    return !(*this == other);
  }

  ConnectionSpecificId connection_id;
  ConnectionSpecificId view_id;
};

// Functions for converting to/from structs and transport values.
inline NodeId NodeIdFromTransportId(Id id) {
  return NodeId(HiWord(id), LoWord(id));
}

inline Id NodeIdToTransportId(const NodeId& id) {
  return (id.connection_id << 16) | id.node_id;
}

inline ViewId ViewIdFromTransportId(Id id) {
  return ViewId(HiWord(id), LoWord(id));
}

inline Id ViewIdToTransportId(const ViewId& id) {
  return (id.connection_id << 16) | id.view_id;
}

inline NodeId RootNodeId() {
  return NodeId(kRootConnection, 1);
}

// Returns a NodeId that is reserved to indicate no node. That is, no node will
// ever be created with this id.
inline NodeId InvalidNodeId() {
  return NodeId(kRootConnection, 0);
}

}  // namespace service
}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_IDS_H_
