// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/tab_restore_service.h"

namespace sessions {

// TimeFactory-----------------------------------------------------------------

TabRestoreService::TimeFactory::~TimeFactory() {}

// Entry ----------------------------------------------------------------------

// ID of the next Entry.
static SessionID::id_type next_entry_id = 1;

TabRestoreService::Entry::Entry()
    : id(next_entry_id++),
      type(TAB),
      from_last_session(false) {}

TabRestoreService::Entry::Entry(Type type)
    : id(next_entry_id++),
      type(type),
      from_last_session(false) {}

TabRestoreService::Entry::~Entry() {}

// Tab ------------------------------------------------------------------------

TabRestoreService::Tab::Tab()
    : Entry(TAB),
      current_navigation_index(-1),
      browser_id(0),
      tabstrip_index(-1),
      pinned(false) {
}

TabRestoreService::Tab::Tab(const TabRestoreService::Tab& tab)
    : Entry(TAB),
      navigations(tab.navigations),
      current_navigation_index(tab.current_navigation_index),
      browser_id(tab.browser_id),
      tabstrip_index(tab.tabstrip_index),
      pinned(tab.pinned),
      extension_app_id(tab.extension_app_id),
      user_agent_override(tab.user_agent_override) {
  if (tab.platform_data)
    platform_data = tab.platform_data->Clone();
}

TabRestoreService::Tab::~Tab() {
}

TabRestoreService::Tab& TabRestoreService::Tab::operator=(
    const TabRestoreService::Tab& tab) {
  navigations = tab.navigations;
  current_navigation_index = tab.current_navigation_index;
  browser_id = tab.browser_id;
  tabstrip_index = tab.tabstrip_index;
  pinned = tab.pinned;
  extension_app_id = tab.extension_app_id;
  user_agent_override = tab.user_agent_override;

  if (tab.platform_data)
    platform_data = tab.platform_data->Clone();

  return *this;
}

// Window ---------------------------------------------------------------------

TabRestoreService::Window::Window() : Entry(WINDOW), selected_tab_index(-1) {
}

TabRestoreService::Window::~Window() {
}

// TabRestoreService ----------------------------------------------------------

TabRestoreService::~TabRestoreService() {
}

// PlatformSpecificTabData
// ------------------------------------------------------

PlatformSpecificTabData::~PlatformSpecificTabData() {}

}  // namespace sessions
