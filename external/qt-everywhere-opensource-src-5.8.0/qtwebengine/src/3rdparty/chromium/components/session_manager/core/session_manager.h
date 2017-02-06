// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_H_
#define COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "components/session_manager/session_manager_export.h"

namespace session_manager {

class SessionManagerDelegate;

// TODO(nkostylev): Get rid/consolidate with:
// ash::SessionStateDelegate::SessionState and chromeos::LoggedInState.
enum SessionState {
  // Default value, when session state hasn't been initialized yet.
  SESSION_STATE_UNKNOWN = 0,

  // Running out of box UI.
  SESSION_STATE_OOBE,

  // Running login UI (primary user) but user sign in hasn't completed yet.
  SESSION_STATE_LOGIN_PRIMARY,

  // Running login UI (primary or secondary user), user sign in has been
  // completed but login UI hasn't been hidden yet. This means that either
  // some session initialization is happening or user has to go through some
  // UI flow on the same login UI like select avatar, agree to terms of
  // service etc.
  SESSION_STATE_LOGGED_IN_NOT_ACTIVE,

  // A user(s) has logged in *and* login UI is hidden i.e. user session is
  // not blocked.
  SESSION_STATE_ACTIVE,

  // Same as SESSION_STATE_LOGIN_PRIMARY but for multi-profiles sign in i.e.
  // when there's at least one user already active in the session.
  SESSION_STATE_LOGIN_SECONDARY,
};

class SESSION_EXPORT SessionManager {
 public:
  SessionManager();
  virtual ~SessionManager();

  // Returns current SessionManager instance and NULL if it hasn't been
  // initialized yet.
  static SessionManager* Get();

  SessionState session_state() const { return session_state_; }
  virtual void SetSessionState(SessionState state);

  // Let session delegate executed on its plan of actions depending on the
  // current session type / state.
  void Start();

 protected:
  // Initializes SessionManager with delegate.
  void Initialize(SessionManagerDelegate* delegate);

  // Sets SessionManager instance.
  static void SetInstance(SessionManager* session_manager);

 private:
  // Pointer to the existing SessionManager instance (if any).
  // Set in ctor, reset in dtor. Not owned since specific implementation of
  // SessionManager should decide on its own appropriate owner of SessionManager
  // instance. For src/chrome implementation such place is
  // g_browser_process->platform_part().
  static SessionManager* instance;

  SessionState session_state_;
  std::unique_ptr<SessionManagerDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(SessionManager);
};

class SESSION_EXPORT SessionManagerDelegate {
 public:
  SessionManagerDelegate();
  virtual ~SessionManagerDelegate();

  virtual void SetSessionManager(
      session_manager::SessionManager* session_manager);

  // Executes specific actions defined by this delegate.
  virtual void Start() = 0;

 protected:
  session_manager::SessionManager* session_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionManagerDelegate);
};

}  // namespace session_manager

#endif  // COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_H_
