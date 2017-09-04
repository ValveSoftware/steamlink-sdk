// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_BASE_SESSION_SERVICE_COMMANDS_H_
#define COMPONENTS_SESSIONS_CORE_BASE_SESSION_SERVICE_COMMANDS_H_

#include <memory>
#include <string>

#include "components/sessions/core/session_id.h"
#include "components/sessions/core/sessions_export.h"

namespace sessions {
class SessionCommand;
class SerializedNavigationEntry;

// These commands create and read common base commands for SessionService and
// PersistentTabRestoreService.

// Creates a SessionCommand that represents a navigation.
SESSIONS_EXPORT std::unique_ptr<SessionCommand>
CreateUpdateTabNavigationCommand(
    SessionID::id_type command_id,
    SessionID::id_type tab_id,
    const sessions::SerializedNavigationEntry& navigation);

// Creates a SessionCommand that represents marking a tab as an application.
SESSIONS_EXPORT std::unique_ptr<SessionCommand>
CreateSetTabExtensionAppIDCommand(SessionID::id_type command_id,
                                  SessionID::id_type tab_id,
                                  const std::string& extension_id);

// Creates a SessionCommand that containing user agent override used by a
// tab's navigations.
SESSIONS_EXPORT std::unique_ptr<SessionCommand>
CreateSetTabUserAgentOverrideCommand(SessionID::id_type command_id,
                                     SessionID::id_type tab_id,
                                     const std::string& user_agent_override);

// Creates a SessionCommand stores a browser window's app name.
SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateSetWindowAppNameCommand(
    SessionID::id_type command_id,
    SessionID::id_type window_id,
    const std::string& app_name);

// Converts a SessionCommand previously created by
// CreateUpdateTabNavigationCommand into a
// SerializedNavigationEntry. Returns true on success. If
// successful |tab_id| is set to the id of the restored tab.
SESSIONS_EXPORT bool RestoreUpdateTabNavigationCommand(
    const SessionCommand& command,
    sessions::SerializedNavigationEntry* navigation,
    SessionID::id_type* tab_id);

// Extracts a SessionCommand as previously created by
// CreateSetTabExtensionAppIDCommand into the tab id and application
// extension id.
SESSIONS_EXPORT bool RestoreSetTabExtensionAppIDCommand(
    const SessionCommand& command,
    SessionID::id_type* tab_id,
    std::string* extension_app_id);

// Extracts a SessionCommand as previously created by
// CreateSetTabUserAgentOverrideCommand into the tab id and user agent.
SESSIONS_EXPORT bool RestoreSetTabUserAgentOverrideCommand(
    const SessionCommand& command,
    SessionID::id_type* tab_id,
    std::string* user_agent_override);

// Extracts a SessionCommand as previously created by
// CreateSetWindowAppNameCommand into the window id and application name.
SESSIONS_EXPORT bool RestoreSetWindowAppNameCommand(
    const SessionCommand& command,
    SessionID::id_type* window_id,
    std::string* app_name);

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_BASE_SESSION_SERVICE_COMMANDS_H_
