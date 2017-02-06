// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SYNCED_SESSION_TRACKER_H_
#define COMPONENTS_SYNC_SESSIONS_SYNCED_SESSION_TRACKER_H_

#include <stddef.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/synced_session.h"
#include "components/sync_sessions/tab_node_pool.h"

namespace sync_sessions {
class SyncSessionsClient;
}

namespace browser_sync {

// Class to manage synced sessions. The tracker will own all SyncedSession
// and SyncedSessionTab objects it creates, and deletes them appropriately on
// destruction.
// Note: SyncedSession objects are created for all synced sessions, including
// the local session (whose tag we maintain separately).
class SyncedSessionTracker {
 public:
  // Different ways to lookup/filter tabs.
  enum SessionLookup {
    RAW,         // Return all foreign sessions.
    PRESENTABLE  // Have one window with at least one tab with syncable content.
  };

  explicit SyncedSessionTracker(
      sync_sessions::SyncSessionsClient* sessions_client);
  ~SyncedSessionTracker();

  // We track and distinguish the local session from foreign sessions.
  void SetLocalSessionTag(const std::string& local_session_tag);

  // Fill a preallocated vector with all foreign sessions we're tracking (skips
  // the local session object). SyncedSession ownership remains within the
  // SyncedSessionTracker. Lookup parameter is used to decide which foreign tabs
  // should be include.
  // Returns true if we had foreign sessions to fill it with, false otherwise.
  bool LookupAllForeignSessions(
      std::vector<const sync_driver::SyncedSession*>* sessions,
      SessionLookup lookup) const;

  // Attempts to look up the session windows associatd with the session given
  // by |session_tag|. Ownership Of SessionWindows stays within the
  // SyncedSessionTracker.
  // If lookup succeeds:
  // - Fills windows with the SessionWindow pointers, returns true.
  // Else
  // - Returns false.
  bool LookupSessionWindows(
      const std::string& session_tag,
      std::vector<const sessions::SessionWindow*>* windows) const;

  // Attempts to look up the tab associated with the given tag and tab id.
  // Ownership of the SessionTab remains within the SyncedSessionTracker.
  // If lookup succeeds:
  // - Sets tab to point to the SessionTab, and returns true.
  // Else
  // - Returns false, tab is set to NULL.
  bool LookupSessionTab(const std::string& session_tag,
                        SessionID::id_type tab_id,
                        const sessions::SessionTab** tab) const;

  // Allows retrieval of existing data for the local session. Unlike GetSession
  // this won't create-if-not-present.
  bool LookupLocalSession(const sync_driver::SyncedSession** output) const;

  // Returns a pointer to the SyncedSession object associated with
  // |session_tag|. If none exists, creates one. Ownership of the
  // SyncedSession remains within the SyncedSessionTracker.
  sync_driver::SyncedSession* GetSession(const std::string& session_tag);

  // Deletes the session associated with |session_tag| if it exists.
  // Returns true if the session existed and was deleted, false otherwise.
  bool DeleteSession(const std::string& session_tag);

  // Resets the tracking information for the session specified by |session_tag|.
  // This involves clearing all the windows and tabs from the session, while
  // keeping pointers saved in the synced_window_map_ and synced_tab_map_.
  // Once reset, all calls to PutWindowInSession and PutTabInWindow will denote
  // that the requested windows and tabs are owned (by setting the boolean
  // in their SessionWindowWrapper/SessionTabWrapper to true) and add them back
  // to their session. The next call to CleanupSession(...) will delete those
  // windows and tabs not owned.
  void ResetSessionTracking(const std::string& session_tag);

  // Tracks the deletion of a foreign tab by removing the given |tab_node_id|
  // from the parent session. Doesn't actually remove any tab objects because
  // the header may have or may not have already been updated to no longer
  // parent this tab. Regardless, when the header is updated then cleanup will
  // remove the actual tab data. However, this method always needs to be called
  // upon foreign tab deletion, otherwise LookupTabNodeIds(...) may return
  // already deleted tab node ids.
  void DeleteForeignTab(const std::string& session_tag, int tab_node_id);

  // Deletes those windows and tabs associated with |session_tag| that are no
  // longer owned.
  // See ResetSessionTracking(...).
  void CleanupSession(const std::string& session_tag);

  // Adds the window with id |window_id| to the session specified by
  // |session_tag|, and markes the window as being owned. If none existed for
  // that session, creates one. Similarly, if the session did not exist yet,
  // creates it. Ownership of the SessionWindow remains within the
  // SyncedSessionTracker.
  void PutWindowInSession(const std::string& session_tag,
                          SessionID::id_type window_id);

  // Adds the tab with id |tab_id| to the window |window_id|, and marks it as
  // being owned. If none existed for that session, creates one. Ownership of
  // the SessionTab remains within the SyncedSessionTracker.
  // Note: GetSession(..) must have already been called with |session_tag| to
  // ensure we having mapping information for this session.
  void PutTabInWindow(const std::string& session_tag,
                      SessionID::id_type window_id,
                      SessionID::id_type tab_id,
                      size_t tab_index);

  // Returns a pointer to the SessionTab object associated with |tab_id| for
  // the session specified with |session_tag|. If none exists, creates one.
  // Ownership of the SessionTab remains within the SyncedSessionTracker.
  // |tab_node_id| must be a valid node id for the node backing this tab.
  sessions::SessionTab* GetTab(const std::string& session_tag,
                               SessionID::id_type tab_id,
                               int tab_node_id);

  // Fills |tab_node_ids| with the tab node ids (see GetTab) for all the tabs*
  // associated with the session having tag |session_tag|.
  void LookupTabNodeIds(const std::string& session_tag,
                        std::set<int>* tab_node_ids);

  // Free the memory for all dynamically allocated objects and clear the
  // tracking structures.
  void Clear();

  bool Empty() const {
    return synced_tab_map_.empty() && synced_session_map_.empty();
  }

  // Includes both foreign sessions and the local session.
  size_t num_synced_sessions() const { return synced_session_map_.size(); }

  // Returns the number of tabs associated with the specified session tag.
  size_t num_synced_tabs(const std::string& session_tag) const {
    SyncedTabMap::const_iterator iter = synced_tab_map_.find(session_tag);
    if (iter != synced_tab_map_.end()) {
      return iter->second.size();
    } else {
      return 0;
    }
  }

 private:
  // This enum is only used as a named input param for wrapper constructors. The
  // name of this enum and the wrapper's |owned| member variable are a bit
  // misleading. Technically you could argue that the data objects are in charge
  // of deleting their children data objects during their destructors, but this
  // tracker will often circumvent this mechanism. This tracker ensures that the
  // lifetime of both data object and wrapper are as identical as possible.

  // Alternatively, this |owned| concept can be thought of as being orphaned or
  // not. Although all data objects are tied to a session (via tag), they don't
  // always have a parent. Suppose we have information about a tab but no
  // information about it's parent window. This tab would be 'not owned', aka
  // orphaned. CleanupSession(...) can then delete any orphanted data objects
  // and wrapper for a given session via session tag.
  enum OwnedState { IS_OWNED, NOT_OWNED };

  // Datatypes for accessing session data. Neither of the *Wrappers actually
  // have ownership of the Windows/Tabs, they just provide id-based access to
  // them. The ownership remains within its containing session (for windows and
  // mapped tabs, unmapped tabs are owned by the unmapped_tabs_ container).
  // Note, we pair pointers with bools so that we can track what is owned and
  // what can be deleted (see ResetSessionTracking(..) and CleanupSession(..)
  // above). IsOwned is used as a wrapper constructor parameter for readability.
  struct SessionTabWrapper {
    SessionTabWrapper() : tab_ptr(NULL), owned(false) {}

    SessionTabWrapper(sessions::SessionTab* tab_ptr, OwnedState owned)
        : tab_ptr(tab_ptr), owned(owned == IS_OWNED) {}

    sessions::SessionTab* tab_ptr;

    // This is used as part of a mark-and-sweep approach to garbage
    // collection for closed tabs that are no longer "in use", or "owned".
    // ResetSessionTracking will clear |owned| bits, and if it is not claimed
    // by a window by the time CleanupSession is called it will be deleted.
    bool owned;
  };
  typedef std::map<SessionID::id_type, SessionTabWrapper> IDToSessionTabMap;
  typedef std::map<std::string, IDToSessionTabMap> SyncedTabMap;

  struct SessionWindowWrapper {
    SessionWindowWrapper() : window_ptr(NULL), owned(false) {}
    SessionWindowWrapper(sessions::SessionWindow* window_ptr, OwnedState owned)
        : window_ptr(window_ptr), owned(owned == IS_OWNED) {}
    sessions::SessionWindow* window_ptr;
    bool owned;
  };
  typedef std::map<SessionID::id_type, SessionWindowWrapper>
      IDToSessionWindowMap;
  typedef std::map<std::string, IDToSessionWindowMap> SyncedWindowMap;

  typedef std::map<std::string, sync_driver::SyncedSession*> SyncedSessionMap;

  // Helper methods for deleting SessionWindows and SessionTabs without owners.
  bool DeleteOldSessionWindowIfNecessary(
      const SessionWindowWrapper& window_wrapper);
  bool DeleteOldSessionTabIfNecessary(const SessionTabWrapper& tab_wrapper);

  // Implementation for GetTab(...) above, permits invalid tab_node_id.
  sessions::SessionTab* GetTabImpl(const std::string& session_tag,
                                   SessionID::id_type tab_id,
                                   int tab_node_id);

  // The client of the sync sessions datatype.
  sync_sessions::SyncSessionsClient* const sessions_client_;

  // Per client mapping of tab id's to their SessionTab objects.
  // Key: session tag.
  // Value: Tab id to SessionTabWrapper map.
  SyncedTabMap synced_tab_map_;

  // Per client mapping of the window id's to their SessionWindow objects.
  // Key: session_tag
  // Value: Window id to SessionWindowWrapper map.
  SyncedWindowMap synced_window_map_;

  // Per client mapping synced session objects.
  // Key: session tag.
  // Value: SyncedSession object pointer.
  SyncedSessionMap synced_session_map_;

  // The set of tabs that we have seen, and created SessionTab objects for, but
  // have not yet mapped to SyncedSessions. These are temporarily orphaned
  // tabs, and won't be deleted if we delete synced_session_map_, but are still
  // owned by the SyncedSessionTracker itself (and deleted on Clear()).
  std::set<sessions::SessionTab*> unmapped_tabs_;

  // The tag for this machine's local session, so we can distinguish the foreign
  // sessions.
  std::string local_session_tag_;

  DISALLOW_COPY_AND_ASSIGN(SyncedSessionTracker);
};

}  // namespace browser_sync

#endif  // COMPONENTS_SYNC_SESSIONS_SYNCED_SESSION_TRACKER_H_
