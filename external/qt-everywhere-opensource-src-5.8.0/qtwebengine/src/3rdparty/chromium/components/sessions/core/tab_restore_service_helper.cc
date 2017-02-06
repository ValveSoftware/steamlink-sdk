// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/tab_restore_service_helper.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "components/sessions/core/live_tab.h"
#include "components/sessions/core/live_tab_context.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/tab_restore_service_client.h"
#include "components/sessions/core/tab_restore_service_observer.h"

namespace sessions {

// TabRestoreServiceHelper::Observer -------------------------------------------

TabRestoreServiceHelper::Observer::~Observer() {}

void TabRestoreServiceHelper::Observer::OnClearEntries() {}

void TabRestoreServiceHelper::Observer::OnRestoreEntryById(
    SessionID::id_type id,
    Entries::const_iterator entry_iterator) {
}

void TabRestoreServiceHelper::Observer::OnAddEntry() {}

// TabRestoreServiceHelper -----------------------------------------------------

TabRestoreServiceHelper::TabRestoreServiceHelper(
    TabRestoreService* tab_restore_service,
    Observer* observer,
    TabRestoreServiceClient* client,
    TabRestoreService::TimeFactory* time_factory)
    : tab_restore_service_(tab_restore_service),
      observer_(observer),
      client_(client),
      restoring_(false),
      time_factory_(time_factory) {
  DCHECK(tab_restore_service_);
}

TabRestoreServiceHelper::~TabRestoreServiceHelper() {
  FOR_EACH_OBSERVER(TabRestoreServiceObserver, observer_list_,
                    TabRestoreServiceDestroyed(tab_restore_service_));
  STLDeleteElements(&entries_);
}

void TabRestoreServiceHelper::AddObserver(
    TabRestoreServiceObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TabRestoreServiceHelper::RemoveObserver(
    TabRestoreServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void TabRestoreServiceHelper::CreateHistoricalTab(LiveTab* live_tab,
                                                  int index) {
  if (restoring_)
    return;

  LiveTabContext* context = client_->FindLiveTabContextForTab(live_tab);
  if (closing_contexts_.find(context) != closing_contexts_.end())
    return;

  std::unique_ptr<Tab> local_tab(new Tab());
  PopulateTab(local_tab.get(), index, context, live_tab);
  if (local_tab->navigations.empty())
    return;

  AddEntry(local_tab.release(), true, true);
}

void TabRestoreServiceHelper::BrowserClosing(LiveTabContext* context) {
  closing_contexts_.insert(context);

  std::unique_ptr<Window> window(new Window());
  window->selected_tab_index = context->GetSelectedIndex();
  window->timestamp = TimeNow();
  window->app_name = context->GetAppName();

  // Don't use std::vector::resize() because it will push copies of an empty tab
  // into the vector, which will give all tabs in a window the same ID.
  for (int i = 0; i < context->GetTabCount(); ++i) {
    window->tabs.push_back(Tab());
  }
  size_t entry_index = 0;
  for (int tab_index = 0; tab_index < context->GetTabCount(); ++tab_index) {
    PopulateTab(&(window->tabs[entry_index]), tab_index, context,
                context->GetLiveTabAt(tab_index));
    if (window->tabs[entry_index].navigations.empty()) {
      window->tabs.erase(window->tabs.begin() + entry_index);
    } else {
      window->tabs[entry_index].browser_id = context->GetSessionID().id();
      entry_index++;
    }
  }
  if (window->tabs.size() == 1 && window->app_name.empty()) {
    // Short-circuit creating a Window if only 1 tab was present. This fixes
    // http://crbug.com/56744. Copy the Tab because it's owned by an object on
    // the stack.
    AddEntry(new Tab(window->tabs[0]), true, true);
  } else if (!window->tabs.empty()) {
    window->selected_tab_index =
        std::min(static_cast<int>(window->tabs.size() - 1),
                 window->selected_tab_index);
    AddEntry(window.release(), true, true);
  }
}

void TabRestoreServiceHelper::BrowserClosed(LiveTabContext* context) {
  closing_contexts_.erase(context);
}

void TabRestoreServiceHelper::ClearEntries() {
  if (observer_)
    observer_->OnClearEntries();
  STLDeleteElements(&entries_);
  NotifyTabsChanged();
}

const TabRestoreService::Entries& TabRestoreServiceHelper::entries() const {
  return entries_;
}

std::vector<LiveTab*> TabRestoreServiceHelper::RestoreMostRecentEntry(
    LiveTabContext* context) {
  if (entries_.empty())
    return std::vector<LiveTab*>();

  return RestoreEntryById(context, entries_.front()->id, UNKNOWN);
}

TabRestoreService::Tab* TabRestoreServiceHelper::RemoveTabEntryById(
    SessionID::id_type id) {
  Entries::iterator i = GetEntryIteratorById(id);
  if (i == entries_.end())
    return NULL;

  Entry* entry = *i;
  if (entry->type != TabRestoreService::TAB)
    return NULL;

  Tab* tab = static_cast<Tab*>(entry);
  entries_.erase(i);
  return tab;
}

std::vector<LiveTab*> TabRestoreServiceHelper::RestoreEntryById(
    LiveTabContext* context,
    SessionID::id_type id,
    WindowOpenDisposition disposition) {
  Entries::iterator entry_iterator = GetEntryIteratorById(id);
  if (entry_iterator == entries_.end())
    // Don't hoark here, we allow an invalid id.
    return std::vector<LiveTab*>();

  if (observer_)
    observer_->OnRestoreEntryById(id, entry_iterator);
  restoring_ = true;
  Entry* entry = *entry_iterator;

  // If the entry's ID does not match the ID that is being restored, then the
  // entry is a window from which a single tab will be restored.
  bool restoring_tab_in_window = entry->id != id;

  if (!restoring_tab_in_window) {
    entries_.erase(entry_iterator);
    entry_iterator = entries_.end();
  }

  // |context| will be NULL in cases where one isn't already available (eg,
  // when invoked on Mac OS X with no windows open). In this case, create a
  // new browser into which we restore the tabs.
  std::vector<LiveTab*> live_tabs;
  if (entry->type == TabRestoreService::TAB) {
    Tab* tab = static_cast<Tab*>(entry);
    LiveTab* restored_tab = NULL;
    context = RestoreTab(*tab, context, disposition, &restored_tab);
    live_tabs.push_back(restored_tab);
    context->ShowBrowserWindow();
  } else if (entry->type == TabRestoreService::WINDOW) {
    LiveTabContext* current_context = context;
    Window* window = static_cast<Window*>(entry);

    // When restoring a window, either the entire window can be restored, or a
    // single tab within it. If the entry's ID matches the one to restore, then
    // the entire window will be restored.
    if (!restoring_tab_in_window) {
      context = client_->CreateLiveTabContext(window->app_name);
      for (size_t tab_i = 0; tab_i < window->tabs.size(); ++tab_i) {
        const Tab& tab = window->tabs[tab_i];
        LiveTab* restored_tab = context->AddRestoredTab(
            tab.navigations, context->GetTabCount(),
            tab.current_navigation_index, tab.extension_app_id,
            static_cast<int>(tab_i) == window->selected_tab_index, tab.pinned,
            tab.from_last_session, tab.platform_data.get(),
            tab.user_agent_override);
        if (restored_tab) {
          restored_tab->LoadIfNecessary();
          client_->OnTabRestored(
              tab.navigations.at(tab.current_navigation_index).virtual_url());
          live_tabs.push_back(restored_tab);
        }
      }
      // All the window's tabs had the same former browser_id.
      if (window->tabs[0].has_browser()) {
        UpdateTabBrowserIDs(window->tabs[0].browser_id,
                            context->GetSessionID().id());
      }
    } else {
      // Restore a single tab from the window. Find the tab that matches the ID
      // in the window and restore it.
      for (std::vector<Tab>::iterator tab_i = window->tabs.begin();
           tab_i != window->tabs.end(); ++tab_i) {
        const Tab& tab = *tab_i;
        if (tab.id == id) {
          LiveTab* restored_tab = NULL;
          context = RestoreTab(tab, context, disposition, &restored_tab);
          live_tabs.push_back(restored_tab);
          window->tabs.erase(tab_i);
          // If restoring the tab leaves the window with nothing else, delete it
          // as well.
          if (!window->tabs.size()) {
            entries_.erase(entry_iterator);
            delete entry;
          } else {
            // Update the browser ID of the rest of the tabs in the window so if
            // any one is restored, it goes into the same window as the tab
            // being restored now.
            UpdateTabBrowserIDs(tab.browser_id, context->GetSessionID().id());
            for (std::vector<Tab>::iterator tab_j = window->tabs.begin();
                 tab_j != window->tabs.end(); ++tab_j) {
              (*tab_j).browser_id = context->GetSessionID().id();
            }
          }
          break;
        }
      }
    }
    context->ShowBrowserWindow();

    if (disposition == CURRENT_TAB && current_context &&
        current_context->GetActiveLiveTab()) {
      current_context->CloseTab();
    }
  } else {
    NOTREACHED();
  }

  if (!restoring_tab_in_window) {
    delete entry;
  }

  restoring_ = false;
  NotifyTabsChanged();
  return live_tabs;
}

void TabRestoreServiceHelper::NotifyTabsChanged() {
  FOR_EACH_OBSERVER(TabRestoreServiceObserver, observer_list_,
                    TabRestoreServiceChanged(tab_restore_service_));
}

void TabRestoreServiceHelper::NotifyLoaded() {
  FOR_EACH_OBSERVER(TabRestoreServiceObserver, observer_list_,
                    TabRestoreServiceLoaded(tab_restore_service_));
}

void TabRestoreServiceHelper::AddEntry(Entry* entry,
                                       bool notify,
                                       bool to_front) {
  if (!FilterEntry(entry) || (entries_.size() >= kMaxEntries && !to_front)) {
    delete entry;
    return;
  }

  if (to_front)
    entries_.push_front(entry);
  else
    entries_.push_back(entry);

  PruneEntries();

  if (notify)
    NotifyTabsChanged();

  if (observer_)
    observer_->OnAddEntry();
}

void TabRestoreServiceHelper::PruneEntries() {
  Entries new_entries;

  for (TabRestoreService::Entries::const_iterator iter = entries_.begin();
       iter != entries_.end(); ++iter) {
    TabRestoreService::Entry* entry = *iter;

    if (FilterEntry(entry) &&
        new_entries.size() < kMaxEntries) {
      new_entries.push_back(entry);
    } else {
      delete entry;
    }
  }

  entries_ = new_entries;
}

TabRestoreService::Entries::iterator
TabRestoreServiceHelper::GetEntryIteratorById(SessionID::id_type id) {
  for (Entries::iterator i = entries_.begin(); i != entries_.end(); ++i) {
    if ((*i)->id == id)
      return i;

    // For Window entries, see if the ID matches a tab. If so, report the window
    // as the Entry.
    if ((*i)->type == TabRestoreService::WINDOW) {
      std::vector<Tab>& tabs = static_cast<Window*>(*i)->tabs;
      for (std::vector<Tab>::iterator j = tabs.begin();
           j != tabs.end(); ++j) {
        if ((*j).id == id) {
          return i;
        }
      }
    }
  }
  return entries_.end();
}

// static
bool TabRestoreServiceHelper::ValidateEntry(Entry* entry) {
  if (entry->type == TabRestoreService::TAB)
    return ValidateTab(static_cast<Tab*>(entry));

  if (entry->type == TabRestoreService::WINDOW)
    return ValidateWindow(static_cast<Window*>(entry));

  NOTREACHED();
  return false;
}

void TabRestoreServiceHelper::PopulateTab(Tab* tab,
                                          int index,
                                          LiveTabContext* context,
                                          LiveTab* live_tab) {
  const int pending_index = live_tab->GetPendingEntryIndex();
  int entry_count =
      live_tab->IsInitialBlankNavigation() ? 0 : live_tab->GetEntryCount();
  if (entry_count == 0 && pending_index == 0)
    entry_count++;
  tab->navigations.resize(static_cast<int>(entry_count));
  for (int i = 0; i < entry_count; ++i) {
    SerializedNavigationEntry entry = (i == pending_index)
                                          ? live_tab->GetPendingEntry()
                                          : live_tab->GetEntryAtIndex(i);
    tab->navigations[i] = entry;
  }
  tab->timestamp = TimeNow();
  tab->current_navigation_index = live_tab->GetCurrentEntryIndex();
  if (tab->current_navigation_index == -1 && entry_count > 0)
    tab->current_navigation_index = 0;
  tab->tabstrip_index = index;

  tab->extension_app_id = client_->GetExtensionAppIDForTab(live_tab);

  tab->user_agent_override = live_tab->GetUserAgentOverride();

  tab->platform_data = live_tab->GetPlatformSpecificTabData();

  // Delegate may be NULL during unit tests.
  if (context) {
    tab->browser_id = context->GetSessionID().id();
    tab->pinned = context->IsTabPinned(tab->tabstrip_index);
  }
}

LiveTabContext* TabRestoreServiceHelper::RestoreTab(
    const Tab& tab,
    LiveTabContext* context,
    WindowOpenDisposition disposition,
    LiveTab** live_tab) {
  LiveTab* restored_tab;
  if (disposition == CURRENT_TAB && context) {
    restored_tab = context->ReplaceRestoredTab(
        tab.navigations, tab.current_navigation_index, tab.from_last_session,
        tab.extension_app_id, tab.platform_data.get(), tab.user_agent_override);
  } else {
    // We only respsect the tab's original browser if there's no disposition.
    if (disposition == UNKNOWN && tab.has_browser()) {
      context = client_->FindLiveTabContextWithID(tab.browser_id);
    }

    int tab_index = -1;

    // |context| will be NULL in cases where one isn't already available (eg,
    // when invoked on Mac OS X with no windows open). In this case, create a
    // new browser into which we restore the tabs.
    if (context && disposition != NEW_WINDOW) {
      tab_index = tab.tabstrip_index;
    } else {
      context = client_->CreateLiveTabContext(std::string());
      if (tab.has_browser())
        UpdateTabBrowserIDs(tab.browser_id, context->GetSessionID().id());
    }

    // Place the tab at the end if the tab index is no longer valid or
    // we were passed a specific disposition.
    if (tab_index < 0 || tab_index > context->GetTabCount() ||
        disposition != UNKNOWN) {
      tab_index = context->GetTabCount();
    }

    restored_tab = context->AddRestoredTab(
        tab.navigations, tab_index, tab.current_navigation_index,
        tab.extension_app_id, disposition != NEW_BACKGROUND_TAB, tab.pinned,
        tab.from_last_session, tab.platform_data.get(),
        tab.user_agent_override);
    restored_tab->LoadIfNecessary();
  }
  client_->OnTabRestored(
      tab.navigations.at(tab.current_navigation_index).virtual_url());
  if (live_tab)
    *live_tab = restored_tab;

  return context;
}


bool TabRestoreServiceHelper::ValidateTab(Tab* tab) {
  if (tab->navigations.empty())
    return false;

  tab->current_navigation_index =
      std::max(0, std::min(tab->current_navigation_index,
                           static_cast<int>(tab->navigations.size()) - 1));

  return true;
}

bool TabRestoreServiceHelper::ValidateWindow(Window* window) {
  window->selected_tab_index =
      std::max(0, std::min(window->selected_tab_index,
                           static_cast<int>(window->tabs.size() - 1)));

  int i = 0;
  for (std::vector<Tab>::iterator tab_i = window->tabs.begin();
       tab_i != window->tabs.end();) {
    if (!ValidateTab(&(*tab_i))) {
      tab_i = window->tabs.erase(tab_i);
      if (i < window->selected_tab_index)
        window->selected_tab_index--;
      else if (i == window->selected_tab_index)
        window->selected_tab_index = 0;
    } else {
      ++tab_i;
      ++i;
    }
  }

  if (window->tabs.empty())
    return false;

  return true;
}

bool TabRestoreServiceHelper::IsTabInteresting(const Tab* tab) {
  if (tab->navigations.empty())
    return false;

  if (tab->navigations.size() > 1)
    return true;

  return tab->pinned ||
         tab->navigations.at(0).virtual_url() != client_->GetNewTabURL();
}

bool TabRestoreServiceHelper::IsWindowInteresting(const Window* window) {
  if (window->tabs.empty())
    return false;

  if (window->tabs.size() > 1)
    return true;

  return IsTabInteresting(&window->tabs[0]);
}

bool TabRestoreServiceHelper::FilterEntry(Entry* entry) {
  if (!ValidateEntry(entry))
    return false;

  if (entry->type == TabRestoreService::TAB)
    return IsTabInteresting(static_cast<Tab*>(entry));
  else if (entry->type == TabRestoreService::WINDOW)
    return IsWindowInteresting(static_cast<Window*>(entry));

  NOTREACHED();
  return false;
}

void TabRestoreServiceHelper::UpdateTabBrowserIDs(SessionID::id_type old_id,
                                                  SessionID::id_type new_id) {
  for (Entries::iterator i = entries_.begin(); i != entries_.end(); ++i) {
    Entry* entry = *i;
    if (entry->type == TabRestoreService::TAB) {
      Tab* tab = static_cast<Tab*>(entry);
      if (tab->browser_id == old_id)
        tab->browser_id = new_id;
    }
  }
}

base::Time TabRestoreServiceHelper::TimeNow() const {
  return time_factory_ ? time_factory_->TimeNow() : base::Time::Now();
}

}  // namespace sessions
