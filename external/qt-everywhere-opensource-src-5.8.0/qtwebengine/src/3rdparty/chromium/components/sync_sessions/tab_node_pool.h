// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_TAB_NODE_POOL_H_
#define COMPONENTS_SYNC_SESSIONS_TAB_NODE_POOL_H_

#include <stddef.h>

#include <map>
#include <set>
#include <string>

#include "base/macros.h"
#include "components/sessions/core/session_id.h"
#include "sync/api/sync_change_processor.h"

namespace syncer {
class SyncChangeProcessor;
}

namespace browser_sync {

// A pool for managing free/used tab sync nodes for the *local* session.
// Performs lazy creation of sync nodes when necessary.
// Note: We make use of the following "id's"
// - a tab_id: created by session service, unique to this client
// - a tab_node_id: the id for a particular sync tab node. This is used
//   to generate the sync tab node tag through:
//       tab_tag = StringPrintf("%s_%ui", local_session_tag, tab_node_id);
//
// A sync node can be in one of the three states:
// 1. Associated   : Sync node is used and associated with a tab.
// 2. Unassociated : Sync node is used but currently unassociated with any tab.
//                   This is true for old nodes that remain from a session
//                   restart. Nodes are only unassociated temporarily while the
//                   model associator figures out which tabs belong to which
//                   nodes. Eventually any remaining unassociated nodes are
//                   freed.
// 3. Free         : Sync node is unused.

class TabNodePool {
 public:
  TabNodePool();
  ~TabNodePool();
  enum InvalidTab { kInvalidTabID = -1 };

  // If free nodes > kFreeNodesHighWatermark, delete all free nodes until
  // free nodes <= kFreeNodesLowWatermark.
  static const size_t kFreeNodesLowWatermark;

  // Maximum limit of FreeNodes allowed on the client.
  static const size_t kFreeNodesHighWatermark;

  static const int kInvalidTabNodeID;

  // Build a sync tag from tab_node_id.
  static std::string TabIdToTag(const std::string& machine_tag,
                                int tab_node_id);

  // Returns the tab_node_id for the next free tab node. If none are available,
  // creates a new tab node and adds it to free nodes pool. The free node can
  // then be used to associate with a tab by calling AssociateTabNode.
  // Note: The node is considered free until it has been associated. Repeated
  // calls to GetFreeTabNode will return the same id until node has been
  // associated.
  // |change_output| *must* be provided. It is the TabNodePool's link to
  // the SyncChange pipeline that exists in the caller context. If the need
  // to create nodes arises in the implementation, associated SyncChanges will
  // be appended to this list for later application by the caller via the
  // SyncChangeProcessor.
  int GetFreeTabNode(syncer::SyncChangeList* change_output);

  // Removes association for |tab_node_id| and returns it to the free node pool.
  // |change_output| *must* be provided. It is the TabNodePool's link to
  // the SyncChange pipeline that exists in the caller's context. If the need
  // to delete sync nodes arises in the implementation, associated SyncChanges
  // will be appended to this list for later application by the caller via the
  // SyncChangeProcessor.
  void FreeTabNode(int tab_node_id, syncer::SyncChangeList* change_output);

  // Associates |tab_node_id| with |tab_id|. |tab_node_id| should either be
  // unassociated or free. If |tab_node_id| is free, |tab_node_id| is removed
  // from the free node pool In order to associate a non free sync node,
  // use ReassociateTabNode.
  void AssociateTabNode(int tab_node_id, SessionID::id_type tab_id);

  // Adds |tab_node_id| as an unassociated sync node.
  // Note: this should only be called when we discover tab sync nodes from
  // previous sessions, not for freeing tab nodes we created through
  // GetFreeTabNode (use FreeTabNode below for that).
  void AddTabNode(int tab_node_id);

  // Returns the tab_id for |tab_node_id| if it is associated else returns
  // kInvalidTabID.
  SessionID::id_type GetTabIdFromTabNodeId(int tab_node_id) const;

  // Reassociates |tab_node_id| with |tab_id|. |tab_node_id| must be either
  // associated with a tab or in the set of unassociated nodes.
  void ReassociateTabNode(int tab_node_id, SessionID::id_type tab_id);

  // Returns true if |tab_node_id| is an unassociated tab node.
  bool IsUnassociatedTabNode(int tab_node_id);

  // Returns any unassociated nodes to the free node pool.
  // |change_output| *must* be provided. It is the TabNodePool's link to
  // the SyncChange pipeline that exists in the caller's context.
  // See FreeTabNode for more detail.
  void DeleteUnassociatedTabNodes(syncer::SyncChangeList* change_output);

  // Clear tab pool.
  void Clear();

  // Return the number of tab nodes this client currently has allocated
  // (including both free, unassociated and associated nodes)
  size_t Capacity() const;

  // Return empty status (all tab nodes are in use).
  bool Empty() const;

  // Return full status (no tab nodes are in use).
  bool Full();

  void SetMachineTag(const std::string& machine_tag);

 private:
  friend class SyncTabNodePoolTest;
  typedef std::map<int, SessionID::id_type> TabNodeIDToTabIDMap;

  // Adds |tab_node_id| to free node pool.
  // |change_output| *must* be provided. It is the TabNodePool's link to
  // the SyncChange pipeline that exists in the caller's context.
  // See FreeTabNode for more detail.
  void FreeTabNodeInternal(int tab_node_id,
                           syncer::SyncChangeList* change_output);

  // Stores mapping of node ids associated with tab_ids, these are the used
  // nodes of tab node pool.
  // The nodes in the map can be returned to free tab node pool by calling
  // FreeTabNode(tab_node_id).
  TabNodeIDToTabIDMap nodeid_tabid_map_;

  // The node ids for the set of free sync nodes.
  std::set<int> free_nodes_pool_;

  // The node ids that are added to pool using AddTabNode and are currently
  // not associated with any tab. They can be reassociated using
  // ReassociateTabNode.
  std::set<int> unassociated_nodes_;

  // The maximum used tab_node id for a sync node. A new sync node will always
  // be created with max_used_tab_node_id_ + 1.
  int max_used_tab_node_id_;

  // The machine tag associated with this tab pool. Used in the title of new
  // sync nodes.
  std::string machine_tag_;

  DISALLOW_COPY_AND_ASSIGN(TabNodePool);
};

}  // namespace browser_sync

#endif  // COMPONENTS_SYNC_SESSIONS_TAB_NODE_POOL_H_
