// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/tab_node_pool.h"

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/session_specifics.pb.h"
#include "sync/protocol/sync.pb.h"

namespace browser_sync {

const size_t TabNodePool::kFreeNodesLowWatermark = 25;
const size_t TabNodePool::kFreeNodesHighWatermark = 100;

TabNodePool::TabNodePool() : max_used_tab_node_id_(kInvalidTabNodeID) {}

// static
// We start vending tab node IDs at 0.
const int TabNodePool::kInvalidTabNodeID = -1;

TabNodePool::~TabNodePool() {}

// Static
std::string TabNodePool::TabIdToTag(const std::string& machine_tag,
                                    int tab_node_id) {
  return base::StringPrintf("%s %d", machine_tag.c_str(), tab_node_id);
}

void TabNodePool::AddTabNode(int tab_node_id) {
  DCHECK_GT(tab_node_id, kInvalidTabNodeID);
  DCHECK(nodeid_tabid_map_.find(tab_node_id) == nodeid_tabid_map_.end());
  unassociated_nodes_.insert(tab_node_id);
  if (max_used_tab_node_id_ < tab_node_id)
    max_used_tab_node_id_ = tab_node_id;
}

void TabNodePool::AssociateTabNode(int tab_node_id, SessionID::id_type tab_id) {
  DCHECK_GT(tab_node_id, kInvalidTabNodeID);
  // Remove sync node if it is in unassociated nodes pool.
  std::set<int>::iterator u_it = unassociated_nodes_.find(tab_node_id);
  if (u_it != unassociated_nodes_.end()) {
    unassociated_nodes_.erase(u_it);
  } else {
    // This is a new node association, the sync node should be free.
    // Remove node from free node pool and then associate it with the tab.
    std::set<int>::iterator it = free_nodes_pool_.find(tab_node_id);
    DCHECK(it != free_nodes_pool_.end());
    free_nodes_pool_.erase(it);
  }
  DCHECK(nodeid_tabid_map_.find(tab_node_id) == nodeid_tabid_map_.end());
  nodeid_tabid_map_[tab_node_id] = tab_id;
}

int TabNodePool::GetFreeTabNode(syncer::SyncChangeList* append_changes) {
  DCHECK_GT(machine_tag_.length(), 0U);
  DCHECK(append_changes);
  if (free_nodes_pool_.empty()) {
    // Tab pool has no free nodes, allocate new one.
    int tab_node_id = ++max_used_tab_node_id_;
    std::string tab_node_tag = TabIdToTag(machine_tag_, tab_node_id);

    // We fill the new node with just enough data so that in case of a crash/bug
    // we can identify the node as our own on re-association and reuse it.
    sync_pb::EntitySpecifics entity;
    sync_pb::SessionSpecifics* specifics = entity.mutable_session();
    specifics->set_session_tag(machine_tag_);
    specifics->set_tab_node_id(tab_node_id);
    append_changes->push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_ADD,
        syncer::SyncData::CreateLocalData(tab_node_tag, tab_node_tag, entity)));

    // Grow the pool by 1 since we created a new node.
    DVLOG(1) << "Adding sync node " << tab_node_id << " to tab node id pool";
    free_nodes_pool_.insert(tab_node_id);
    return tab_node_id;
  } else {
    // Return the next free node.
    return *free_nodes_pool_.begin();
  }
}

void TabNodePool::FreeTabNode(int tab_node_id,
                              syncer::SyncChangeList* append_changes) {
  DCHECK(append_changes);
  TabNodeIDToTabIDMap::iterator it = nodeid_tabid_map_.find(tab_node_id);
  DCHECK(it != nodeid_tabid_map_.end());
  nodeid_tabid_map_.erase(it);
  FreeTabNodeInternal(tab_node_id, append_changes);
}

void TabNodePool::FreeTabNodeInternal(int tab_node_id,
                                      syncer::SyncChangeList* append_changes) {
  DCHECK(free_nodes_pool_.find(tab_node_id) == free_nodes_pool_.end());
  DCHECK(append_changes);
  free_nodes_pool_.insert(tab_node_id);

  // If number of free nodes exceed kFreeNodesHighWatermark,
  // delete sync nodes till number reaches kFreeNodesLowWatermark.
  // Note: This logic is to mitigate temporary disassociation issues with old
  // clients: http://crbug.com/259918. Newer versions do not need this.
  if (free_nodes_pool_.size() > kFreeNodesHighWatermark) {
    for (std::set<int>::iterator free_it = free_nodes_pool_.begin();
         free_it != free_nodes_pool_.end();) {
      const std::string tab_node_tag = TabIdToTag(machine_tag_, *free_it);
      append_changes->push_back(syncer::SyncChange(
          FROM_HERE, syncer::SyncChange::ACTION_DELETE,
          syncer::SyncData::CreateLocalDelete(tab_node_tag, syncer::SESSIONS)));
      free_nodes_pool_.erase(free_it++);
      if (free_nodes_pool_.size() <= kFreeNodesLowWatermark) {
        return;
      }
    }
  }
}

bool TabNodePool::IsUnassociatedTabNode(int tab_node_id) {
  return unassociated_nodes_.find(tab_node_id) != unassociated_nodes_.end();
}

void TabNodePool::ReassociateTabNode(int tab_node_id,
                                     SessionID::id_type tab_id) {
  // Remove from list of unassociated sync_nodes if present.
  std::set<int>::iterator it = unassociated_nodes_.find(tab_node_id);
  if (it != unassociated_nodes_.end()) {
    unassociated_nodes_.erase(it);
  } else {
    // tab_node_id must be an already associated node.
    DCHECK(nodeid_tabid_map_.find(tab_node_id) != nodeid_tabid_map_.end());
  }
  nodeid_tabid_map_[tab_node_id] = tab_id;
}

SessionID::id_type TabNodePool::GetTabIdFromTabNodeId(int tab_node_id) const {
  TabNodeIDToTabIDMap::const_iterator it = nodeid_tabid_map_.find(tab_node_id);
  if (it != nodeid_tabid_map_.end()) {
    return it->second;
  }
  return kInvalidTabID;
}

void TabNodePool::DeleteUnassociatedTabNodes(
    syncer::SyncChangeList* append_changes) {
  for (std::set<int>::iterator it = unassociated_nodes_.begin();
       it != unassociated_nodes_.end();) {
    FreeTabNodeInternal(*it, append_changes);
    unassociated_nodes_.erase(it++);
  }
  DCHECK(unassociated_nodes_.empty());
}

// Clear tab pool.
void TabNodePool::Clear() {
  unassociated_nodes_.clear();
  free_nodes_pool_.clear();
  nodeid_tabid_map_.clear();
  max_used_tab_node_id_ = kInvalidTabNodeID;
}

size_t TabNodePool::Capacity() const {
  return nodeid_tabid_map_.size() + unassociated_nodes_.size() +
         free_nodes_pool_.size();
}

bool TabNodePool::Empty() const {
  return free_nodes_pool_.empty();
}

bool TabNodePool::Full() {
  return nodeid_tabid_map_.empty();
}

void TabNodePool::SetMachineTag(const std::string& machine_tag) {
  machine_tag_ = machine_tag;
}

}  // namespace browser_sync
