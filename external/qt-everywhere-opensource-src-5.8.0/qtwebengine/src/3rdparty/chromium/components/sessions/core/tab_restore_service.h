// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_H_
#define COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_H_

#include <list>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/sessions_export.h"
#include "ui/base/window_open_disposition.h"

namespace sessions {

class LiveTab;
class PlatformSpecificTabData;
class LiveTabContext;
class TabRestoreServiceObserver;

// TabRestoreService is responsible for maintaining the most recently closed
// tabs and windows. When a tab is closed
// TabRestoreService::CreateHistoricalTab is invoked and a Tab is created to
// represent the tab. Similarly, when a browser is closed, BrowserClosing is
// invoked and a Window is created to represent the window.
//
// To restore a tab/window from the TabRestoreService invoke RestoreEntryById
// or RestoreMostRecentEntry.
//
// To listen for changes to the set of entries managed by the TabRestoreService
// add an observer.
class SESSIONS_EXPORT TabRestoreService : public KeyedService {
 public:
  // Interface used to allow the test to provide a custom time.
  class SESSIONS_EXPORT TimeFactory {
   public:
    virtual ~TimeFactory();
    virtual base::Time TimeNow() = 0;
  };

  // The type of entry.
  enum Type {
    TAB,
    WINDOW
  };

  struct SESSIONS_EXPORT Entry {
    Entry();
    explicit Entry(Type type);
    virtual ~Entry();

    // Unique id for this entry. The id is guaranteed to be unique for a
    // session.
    SessionID::id_type id;

    // The type of the entry.
    Type type;

    // The time when the window or tab was closed.
    base::Time timestamp;

    // Is this entry from the last session? This is set to true for entries that
    // were closed during the last session, and false for entries that were
    // closed during this session.
    bool from_last_session;
  };

  // Represents a previously open tab.
  struct SESSIONS_EXPORT Tab : public Entry {
    Tab();
    Tab(const Tab& tab);
    ~Tab() override;

    Tab& operator=(const Tab& tab);

    bool has_browser() const { return browser_id > 0; }

    // The navigations.
    std::vector<SerializedNavigationEntry> navigations;

    // Index of the selected navigation in navigations.
    int current_navigation_index;

    // The ID of the browser to which this tab belonged, so it can be restored
    // there. May be 0 (an invalid SessionID) when restoring an entire session.
    SessionID::id_type browser_id;

    // Index within the tab strip. May be -1 for an unknown index.
    int tabstrip_index;

    // True if the tab was pinned.
    bool pinned;

    // If non-empty gives the id of the extension for the tab.
    std::string extension_app_id;

    // The associated client data.
    std::unique_ptr<PlatformSpecificTabData> platform_data;

    // The user agent override used for the tab's navigations (if applicable).
    std::string user_agent_override;
  };

  // Represents a previously open window.
  struct SESSIONS_EXPORT Window : public Entry {
    Window();
    ~Window() override;

    // The tabs that comprised the window, in order.
    std::vector<Tab> tabs;

    // Index of the selected tab.
    int selected_tab_index;

    // If an application window, the name of the app.
    std::string app_name;
  };

  typedef std::list<Entry*> Entries;

  ~TabRestoreService() override;

  // Adds/removes an observer. TabRestoreService does not take ownership of
  // the observer.
  virtual void AddObserver(TabRestoreServiceObserver* observer) = 0;
  virtual void RemoveObserver(TabRestoreServiceObserver* observer) = 0;

  // Creates a Tab to represent |live_tab| and notifies observers the list of
  // entries has changed.
  virtual void CreateHistoricalTab(LiveTab* live_tab, int index) = 0;

  // TODO(blundell): Rename and fix comment.
  // Invoked when a browser is closing. If |context| is a tabbed browser with
  // at least one tab, a Window is created, added to entries and observers are
  // notified.
  virtual void BrowserClosing(LiveTabContext* context) = 0;

  // TODO(blundell): Rename and fix comment.
  // Invoked when the browser is done closing.
  virtual void BrowserClosed(LiveTabContext* context) = 0;

  // Removes all entries from the list and notifies observers the list
  // of tabs has changed.
  virtual void ClearEntries() = 0;

  // Returns the entries, ordered with most recently closed entries at the
  // front.
  virtual const Entries& entries() const = 0;

  // Restores the most recently closed entry. Does nothing if there are no
  // entries to restore. If the most recently restored entry is a tab, it is
  // added to |context|. Returns the LiveTab instances of the restored tab(s).
  virtual std::vector<LiveTab*> RestoreMostRecentEntry(
      LiveTabContext* context) = 0;

  // Removes the Tab with id |id| from the list and returns it; ownership is
  // passed to the caller.
  virtual Tab* RemoveTabEntryById(SessionID::id_type id) = 0;

  // Restores an entry by id. If there is no entry with an id matching |id|,
  // this does nothing. If |context| is NULL, this creates a new window for the
  // entry. |disposition| is respected, but the attributes (tabstrip index,
  // browser window) of the tab when it was closed will be respected if
  // disposition is UNKNOWN. Returns the LiveTab instances of the restored
  // tab(s).
  virtual std::vector<LiveTab*> RestoreEntryById(
      LiveTabContext* context,
      SessionID::id_type id,
      WindowOpenDisposition disposition) = 0;

  // Loads the tabs and previous session. This does nothing if the tabs
  // from the previous session have already been loaded.
  virtual void LoadTabsFromLastSession() = 0;

  // Returns true if the tab entries have been loaded.
  virtual bool IsLoaded() const = 0;

  // Deletes the last session.
  virtual void DeleteLastSession() = 0;
};

// A class that is used to associate platform-specific data with
// TabRestoreService::Tab. See LiveTab::GetPlatformSpecificTabData().
// Subclasses of this class must be copyable by implementing the Clone() method
// for usage by the Tab struct, which is itself copyable and assignable.
class SESSIONS_EXPORT PlatformSpecificTabData {
 public:
  virtual ~PlatformSpecificTabData();

 private:
  friend TabRestoreService::Tab;

  virtual std::unique_ptr<PlatformSpecificTabData> Clone() = 0;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_H_
