// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSION_MANAGER_SESSION_MANAGER_TYPES_H_
#define COMPONENTS_SESSION_MANAGER_SESSION_MANAGER_TYPES_H_

#include "components/signin/core/account_id/account_id.h"

namespace session_manager {

// TODO(xiyuan): Get rid/consolidate with chromeos::LoggedInState.
enum class SessionState {
  // Default value, when session state hasn't been initialized yet.
  UNKNOWN = 0,

  // Running out of box UI.
  OOBE,

  // Running login UI (primary user) but user sign in hasn't completed yet.
  LOGIN_PRIMARY,

  // Running login UI (primary or secondary user), user sign in has been
  // completed but login UI hasn't been hidden yet. This means that either
  // some session initialization is happening or user has to go through some
  // UI flow on the same login UI like select avatar, agree to terms of
  // service etc.
  LOGGED_IN_NOT_ACTIVE,

  // A user(s) has logged in *and* login UI is hidden i.e. user session is
  // not blocked.
  ACTIVE,

  // The session screen is locked.
  LOCKED,

  // Same as SESSION_STATE_LOGIN_PRIMARY but for multi-profiles sign in i.e.
  // when there's at least one user already active in the session.
  LOGIN_SECONDARY,
};

// A type for session id.
using SessionId = int;

// Info about a user session.
struct Session {
  SessionId id;
  AccountId user_account_id;
};

}  // namespace session_manager

#endif  // COMPONENTS_SESSION_MANAGER_SESSION_MANAGER_TYPES_H_
