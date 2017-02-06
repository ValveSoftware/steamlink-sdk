// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/app_state_tracker.h"

#include "base/lazy_instance.h"
#include "chromecast/crash/cast_crash_keys.h"

namespace {

struct CurrentAppState {
  std::string previous_app;
  std::string current_app;
  std::string last_launched_app;
};

base::LazyInstance<CurrentAppState> g_app_state = LAZY_INSTANCE_INITIALIZER;

CurrentAppState* GetAppState() {
  return g_app_state.Pointer();
}

}  // namespace

namespace chromecast {

// static
std::string AppStateTracker::GetLastLaunchedApp() {
  return GetAppState()->last_launched_app;
}

// static
std::string AppStateTracker::GetCurrentApp() {
  return GetAppState()->current_app;
}

// static
std::string AppStateTracker::GetPreviousApp() {
  return GetAppState()->previous_app;
}

// static
void AppStateTracker::SetLastLaunchedApp(const std::string& app_id) {
  GetAppState()->last_launched_app = app_id;

  // TODO(slan): Currently SetCrashKeyValue is a no-op on chromecast until
  // we add call to InitCrashKeys
  base::debug::SetCrashKeyValue(crash_keys::kLastApp, app_id);
}

// static
void AppStateTracker::SetCurrentApp(const std::string& app_id) {
  CurrentAppState* app_state = GetAppState();
  app_state->previous_app = app_state->current_app;
  app_state->current_app = app_id;

  base::debug::SetCrashKeyValue(crash_keys::kCurrentApp, app_id);
  base::debug::SetCrashKeyValue(crash_keys::kPreviousApp,
                                app_state->previous_app);
}

}  // namespace chromecast
