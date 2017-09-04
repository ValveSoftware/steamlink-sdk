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

TabRestoreService::Entry::~Entry() = default;
TabRestoreService::Entry::Entry(Type type) : id(next_entry_id++), type(type) {}

TabRestoreService::Tab::Tab() : Entry(TAB) {}
TabRestoreService::Tab::~Tab() = default;

TabRestoreService::Window::Window() : Entry(WINDOW) {}
TabRestoreService::Window::~Window() = default;

// TabRestoreService ----------------------------------------------------------

TabRestoreService::~TabRestoreService() {
}

// PlatformSpecificTabData
// ------------------------------------------------------

PlatformSpecificTabData::~PlatformSpecificTabData() {}

}  // namespace sessions
