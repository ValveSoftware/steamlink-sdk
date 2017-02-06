// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/synced_session_tracker.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/sync_sessions/sync_sessions_client.h"

namespace browser_sync {

namespace {

// Helper for iterating through all tabs within a window, and all navigations
// within a tab, to find if there's a valid syncable url.
bool ShouldSyncSessionWindow(sync_sessions::SyncSessionsClient* sessions_client,
                             const sessions::SessionWindow& window) {
  for (sessions::SessionTab* const tab : window.tabs) {
    for (const sessions::SerializedNavigationEntry& navigation :
         tab->navigations) {
      if (sessions_client->ShouldSyncURL(navigation.virtual_url())) {
        return true;
      }
    }
  }
  return false;
}

// Presentable means |foreign_session| must have syncable content.
bool IsPresentable(sync_sessions::SyncSessionsClient* sessions_client,
                   sync_driver::SyncedSession* foreign_session) {
  for (sync_driver::SyncedSession::SyncedWindowMap::const_iterator iter =
           foreign_session->windows.begin();
       iter != foreign_session->windows.end(); ++iter) {
    if (ShouldSyncSessionWindow(sessions_client, *(iter->second))) {
      return true;
    }
  }
  return false;
}

}  // namespace

SyncedSessionTracker::SyncedSessionTracker(
    sync_sessions::SyncSessionsClient* sessions_client)
    : sessions_client_(sessions_client) {}

SyncedSessionTracker::~SyncedSessionTracker() {
  Clear();
}

void SyncedSessionTracker::SetLocalSessionTag(
    const std::string& local_session_tag) {
  local_session_tag_ = local_session_tag;
}

bool SyncedSessionTracker::LookupAllForeignSessions(
    std::vector<const sync_driver::SyncedSession*>* sessions,
    SessionLookup lookup) const {
  DCHECK(sessions);
  sessions->clear();
  for (SyncedSessionMap::const_iterator i = synced_session_map_.begin();
       i != synced_session_map_.end(); ++i) {
    sync_driver::SyncedSession* foreign_session = i->second;
    if (i->first != local_session_tag_ &&
        (lookup == RAW || IsPresentable(sessions_client_, foreign_session))) {
      sessions->push_back(foreign_session);
    }
  }
  return !sessions->empty();
}

bool SyncedSessionTracker::LookupSessionWindows(
    const std::string& session_tag,
    std::vector<const sessions::SessionWindow*>* windows) const {
  DCHECK(windows);
  windows->clear();
  SyncedSessionMap::const_iterator iter = synced_session_map_.find(session_tag);
  if (iter == synced_session_map_.end())
    return false;
  windows->clear();
  for (sync_driver::SyncedSession::SyncedWindowMap::const_iterator window_iter =
           iter->second->windows.begin();
       window_iter != iter->second->windows.end(); ++window_iter) {
    windows->push_back(window_iter->second);
  }
  return true;
}

bool SyncedSessionTracker::LookupSessionTab(
    const std::string& tag,
    SessionID::id_type tab_id,
    const sessions::SessionTab** tab) const {
  DCHECK(tab);
  SyncedTabMap::const_iterator tab_map_iter = synced_tab_map_.find(tag);
  if (tab_map_iter == synced_tab_map_.end()) {
    // We have no record of this session.
    *tab = nullptr;
    return false;
  }
  IDToSessionTabMap::const_iterator tab_iter =
      tab_map_iter->second.find(tab_id);
  if (tab_iter == tab_map_iter->second.end()) {
    // We have no record of this tab.
    *tab = nullptr;
    return false;
  }
  *tab = tab_iter->second.tab_ptr;
  return true;
}

void SyncedSessionTracker::LookupTabNodeIds(const std::string& session_tag,
                                            std::set<int>* tab_node_ids) {
  tab_node_ids->clear();
  SyncedSessionMap::const_iterator session_iter =
      synced_session_map_.find(session_tag);
  if (session_iter != synced_session_map_.end()) {
    tab_node_ids->insert(session_iter->second->tab_node_ids.begin(),
                         session_iter->second->tab_node_ids.end());
  }
  // Incase an invalid node id was included, remove it.
  tab_node_ids->erase(TabNodePool::kInvalidTabNodeID);
}

bool SyncedSessionTracker::LookupLocalSession(
    const sync_driver::SyncedSession** output) const {
  SyncedSessionMap::const_iterator it =
      synced_session_map_.find(local_session_tag_);
  if (it != synced_session_map_.end()) {
    *output = it->second;
    return true;
  }
  return false;
}

sync_driver::SyncedSession* SyncedSessionTracker::GetSession(
    const std::string& session_tag) {
  sync_driver::SyncedSession* synced_session = NULL;
  if (synced_session_map_.find(session_tag) != synced_session_map_.end()) {
    synced_session = synced_session_map_[session_tag];
  } else {
    synced_session = new sync_driver::SyncedSession;
    DVLOG(1) << "Creating new session with tag " << session_tag << " at "
             << synced_session;
    synced_session->session_tag = session_tag;
    synced_session_map_[session_tag] = synced_session;
  }
  DCHECK(synced_session);
  return synced_session;
}

bool SyncedSessionTracker::DeleteSession(const std::string& session_tag) {
  // Cleanup first, which will take care of orphaned SessionTab and
  // SessionWindow objects. The SyncedSession destructor will only delete things
  // that it is currently the parent of.
  CleanupSession(session_tag);

  bool header_existed = false;
  SyncedSessionMap::iterator iter = synced_session_map_.find(session_tag);
  if (iter != synced_session_map_.end()) {
    sync_driver::SyncedSession* session = iter->second;
    // An implicitly created session that has children tabs but no header node
    // will have never had the device_type changed from unset.
    header_existed =
        session->device_type != sync_driver::SyncedSession::TYPE_UNSET;
    synced_session_map_.erase(iter);
    // SyncedSession's destructor will trigger deletion of windows which will in
    // turn trigger the deletion of tabs. This doesn't affect wrappers.
    delete session;
  }

  // These two erase(...) calls only affect the wrappers.
  synced_window_map_.erase(session_tag);
  synced_tab_map_.erase(session_tag);

  return header_existed;
}

void SyncedSessionTracker::ResetSessionTracking(
    const std::string& session_tag) {
  // Reset window tracking.
  GetSession(session_tag)->windows.clear();
  SyncedWindowMap::iterator window_iter = synced_window_map_.find(session_tag);
  if (window_iter != synced_window_map_.end()) {
    for (IDToSessionWindowMap::iterator window_map_iter =
             window_iter->second.begin();
         window_map_iter != window_iter->second.end(); ++window_map_iter) {
      window_map_iter->second.owned = false;
      // We clear out the tabs to prevent double referencing of the same tab.
      // All tabs that are in use will be added back as needed.
      window_map_iter->second.window_ptr->tabs.clear();
    }
  }

  // Reset tab tracking.
  SyncedTabMap::iterator tab_iter = synced_tab_map_.find(session_tag);
  if (tab_iter != synced_tab_map_.end()) {
    for (IDToSessionTabMap::iterator tab_map_iter = tab_iter->second.begin();
         tab_map_iter != tab_iter->second.end(); ++tab_map_iter) {
      tab_map_iter->second.owned = false;
    }
  }
}

void SyncedSessionTracker::DeleteForeignTab(const std::string& session_tag,
                                            int tab_node_id) {
  SyncedSessionMap::const_iterator session_iter =
      synced_session_map_.find(session_tag);
  if (session_iter != synced_session_map_.end()) {
    session_iter->second->tab_node_ids.erase(tab_node_id);
  }
}

bool SyncedSessionTracker::DeleteOldSessionWindowIfNecessary(
    const SessionWindowWrapper& window_wrapper) {
  if (!window_wrapper.owned) {
    DVLOG(1) << "Deleting closed window "
             << window_wrapper.window_ptr->window_id.id();
    // Clear the tabs first, since we don't want the destructor to destroy
    // them. Their deletion will be handled by DeleteOldSessionTabIfNecessary.
    window_wrapper.window_ptr->tabs.clear();
    delete window_wrapper.window_ptr;
    return true;
  }
  return false;
}

bool SyncedSessionTracker::DeleteOldSessionTabIfNecessary(
    const SessionTabWrapper& tab_wrapper) {
  if (!tab_wrapper.owned) {
    if (VLOG_IS_ON(1)) {
      sessions::SessionTab* tab_ptr = tab_wrapper.tab_ptr;
      std::string title;
      if (tab_ptr->navigations.size() > 0) {
        title =
            " (" +
            base::UTF16ToUTF8(
                tab_ptr->navigations[tab_ptr->navigations.size() - 1].title()) +
            ")";
      }
      DVLOG(1) << "Deleting closed tab " << tab_ptr->tab_id.id() << title
               << " from window " << tab_ptr->window_id.id();
    }
    unmapped_tabs_.erase(tab_wrapper.tab_ptr);
    delete tab_wrapper.tab_ptr;
    return true;
  }
  return false;
}

void SyncedSessionTracker::CleanupSession(const std::string& session_tag) {
  // Go through and delete any windows or tabs without owners.
  SyncedWindowMap::iterator window_iter = synced_window_map_.find(session_tag);
  if (window_iter != synced_window_map_.end()) {
    for (IDToSessionWindowMap::iterator iter = window_iter->second.begin();
         iter != window_iter->second.end();) {
      SessionWindowWrapper window_wrapper = iter->second;
      if (DeleteOldSessionWindowIfNecessary(window_wrapper))
        window_iter->second.erase(iter++);
      else
        ++iter;
    }
  }

  SyncedTabMap::iterator tab_iter = synced_tab_map_.find(session_tag);
  if (tab_iter != synced_tab_map_.end()) {
    for (IDToSessionTabMap::iterator iter = tab_iter->second.begin();
         iter != tab_iter->second.end();) {
      SessionTabWrapper tab_wrapper = iter->second;
      if (DeleteOldSessionTabIfNecessary(tab_wrapper)) {
        tab_iter->second.erase(iter++);
      } else {
        ++iter;
      }
    }
  }
}

void SyncedSessionTracker::PutWindowInSession(const std::string& session_tag,
                                              SessionID::id_type window_id) {
  sessions::SessionWindow* window_ptr = nullptr;
  IDToSessionWindowMap::iterator iter =
      synced_window_map_[session_tag].find(window_id);
  if (iter != synced_window_map_[session_tag].end()) {
    iter->second.owned = true;
    window_ptr = iter->second.window_ptr;
    DVLOG(1) << "Putting seen window " << window_id << " at " << window_ptr
             << "in " << (session_tag == local_session_tag_ ? "local session"
                                                            : session_tag);
  } else {
    // Create the window.
    window_ptr = new sessions::SessionWindow();
    window_ptr->window_id.set_id(window_id);
    synced_window_map_[session_tag][window_id] =
        SessionWindowWrapper(window_ptr, IS_OWNED);
    DVLOG(1) << "Putting new window " << window_id << " at " << window_ptr
             << "in " << (session_tag == local_session_tag_ ? "local session"
                                                            : session_tag);
  }
  DCHECK(window_ptr);
  DCHECK_EQ(window_ptr->window_id.id(), window_id);
  DCHECK_EQ(reinterpret_cast<sessions::SessionWindow*>(NULL),
            GetSession(session_tag)->windows[window_id]);
  GetSession(session_tag)->windows[window_id] = window_ptr;
}

void SyncedSessionTracker::PutTabInWindow(const std::string& session_tag,
                                          SessionID::id_type window_id,
                                          SessionID::id_type tab_id,
                                          size_t tab_index) {
  // We're called here for two reasons. 1) We've received an update to the
  // SessionWindow information of a SessionHeader node for a foreign session,
  // and 2) The SessionHeader node for our local session changed. In both cases
  // we need to update our tracking state to reflect the change.
  //
  // Because the SessionHeader nodes are separate from the individual tab nodes
  // and we don't store tab_node_ids in the header / SessionWindow specifics,
  // the tab_node_ids are not always available when processing headers.
  // We know that we will eventually process (via GetTab) every single tab node
  // in the system, so we permit ourselves to use kInvalidTabNodeID here and
  // rely on the later update to build the mapping (or a restart).
  sessions::SessionTab* tab_ptr =
      GetTabImpl(session_tag, tab_id, TabNodePool::kInvalidTabNodeID);

  // It's up to the caller to ensure this never happens.  Tabs should not
  // belong to more than one window or appear twice within the same window.
  //
  // If this condition were violated, we would double-free during shutdown.
  // That could cause all sorts of hard to diagnose crashes, possibly in code
  // far away from here.  We crash early to avoid this.
  //
  // See http://crbug.com/360822.
  CHECK(!synced_tab_map_[session_tag][tab_id].owned);

  // Only tabs that were just created in GetTabImpl(...) are still present in
  // unmapped_tabs_. Most of the time when PutTabInWindow(...) is invoked, the
  // specified tab was already a child of the specified window, and hasn't been
  // in unmapped_tabs_ for quite some time. However, this is not a problem, as
  // std::set::erase(...) will simply have no effect in these cases.
  unmapped_tabs_.erase(tab_ptr);
  synced_tab_map_[session_tag][tab_id].owned = true;

  tab_ptr->window_id.set_id(window_id);
  DVLOG(1) << "  - tab " << tab_id << " added to window " << window_id;
  DCHECK(GetSession(session_tag)->windows.find(window_id) !=
         GetSession(session_tag)->windows.end());
  std::vector<sessions::SessionTab*>& window_tabs =
      GetSession(session_tag)->windows[window_id]->tabs;
  if (window_tabs.size() <= tab_index) {
    window_tabs.resize(tab_index + 1, nullptr);
  }
  DCHECK(!window_tabs[tab_index]);
  window_tabs[tab_index] = tab_ptr;
}

sessions::SessionTab* SyncedSessionTracker::GetTab(
    const std::string& session_tag,
    SessionID::id_type tab_id,
    int tab_node_id) {
  DCHECK_NE(TabNodePool::kInvalidTabNodeID, tab_node_id);
  return GetTabImpl(session_tag, tab_id, tab_node_id);
}

sessions::SessionTab* SyncedSessionTracker::GetTabImpl(
    const std::string& session_tag,
    SessionID::id_type tab_id,
    int tab_node_id) {
  sessions::SessionTab* tab_ptr = nullptr;
  IDToSessionTabMap::iterator iter = synced_tab_map_[session_tag].find(tab_id);
  if (iter != synced_tab_map_[session_tag].end()) {
    tab_ptr = iter->second.tab_ptr;
    if (tab_node_id != TabNodePool::kInvalidTabNodeID &&
        tab_id != TabNodePool::kInvalidTabID) {
      // TabIDs are not stable across restarts of a client. Consider this
      // example with two tabs:
      //
      // http://a.com  TabID1 --> NodeIDA
      // http://b.com  TabID2 --> NodeIDB
      //
      // After restart, tab ids are reallocated. e.g, one possibility:
      // http://a.com TabID2 --> NodeIDA
      // http://b.com TabID1 --> NodeIDB
      //
      // If that happend on a remote client, here we will see an update to
      // TabID1 with tab_node_id changing from NodeIDA to NodeIDB, and TabID2
      // with tab_node_id changing from NodeIDB to NodeIDA.
      //
      // We can also wind up here if we created this tab as an out-of-order
      // update to the header node for this session before actually associating
      // the tab itself, so the tab node id wasn't available at the time and
      // is currenlty kInvalidTabNodeID.
      //
      // In both cases, we can safely throw it into the set of node ids.
      GetSession(session_tag)->tab_node_ids.insert(tab_node_id);
    }

    if (VLOG_IS_ON(1)) {
      std::string title;
      if (tab_ptr->navigations.size() > 0) {
        title =
            " (" +
            base::UTF16ToUTF8(
                tab_ptr->navigations[tab_ptr->navigations.size() - 1].title()) +
            ")";
      }
      DVLOG(1) << "Getting "
               << (session_tag == local_session_tag_ ? "local session"
                                                     : session_tag)
               << "'s seen tab " << tab_id << " at " << tab_ptr << title;
    }
  } else {
    tab_ptr = new sessions::SessionTab();
    tab_ptr->tab_id.set_id(tab_id);
    synced_tab_map_[session_tag][tab_id] =
        SessionTabWrapper(tab_ptr, NOT_OWNED);
    unmapped_tabs_.insert(tab_ptr);
    GetSession(session_tag)->tab_node_ids.insert(tab_node_id);
    DVLOG(1) << "Getting "
             << (session_tag == local_session_tag_ ? "local session"
                                                   : session_tag)
             << "'s new tab " << tab_id << " at " << tab_ptr;
  }
  DCHECK(tab_ptr);
  DCHECK_EQ(tab_ptr->tab_id.id(), tab_id);
  return tab_ptr;
}

void SyncedSessionTracker::Clear() {
  // Cleanup first, which will take care of orphaned SessionTab and
  // SessionWindow objects. The SyncedSession destructor will only delete things
  // that is currently parents.
  for (const auto& kv : synced_session_map_) {
    CleanupSession(kv.first);
  }

  // Delete SyncedSession objects (which also deletes all their windows/tabs).
  STLDeleteValues(&synced_session_map_);

  // Go through and delete any tabs we had allocated but had not yet placed into
  // a SyncedSession object.
  STLDeleteElements(&unmapped_tabs_);

  // Get rid of our Window/Tab maps (does not delete the actual Window/Tabs
  // themselves; they should have all been deleted above).
  synced_window_map_.clear();
  synced_tab_map_.clear();
  local_session_tag_.clear();
}

}  // namespace browser_sync
