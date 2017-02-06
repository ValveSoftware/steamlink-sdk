// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/exit_funnel_win.h"

#include <windows.h>
#include <stdint.h>

#include "base/strings/stringprintf.h"
#include "base/time/time.h"

namespace browser_watcher {

ExitFunnel::ExitFunnel() {
}

ExitFunnel::~ExitFunnel() {
}

bool ExitFunnel::Init(const base::char16* registry_path,
                      base::ProcessHandle process_handle) {
  // Concatenate the pid and the creation time of the process to get a
  // unique key name.
  base::ProcessId pid = base::GetProcId(process_handle);

  FILETIME creation_time = {};
  FILETIME dummy = {};
  if (!::GetProcessTimes(process_handle, &creation_time,
                         &dummy, &dummy, &dummy)) {
    LOG(ERROR) << "Invalid process handle, can't get process times.";
    return false;
  }

  return InitImpl(registry_path, pid, base::Time::FromFileTime(creation_time));
}

bool  ExitFunnel::InitImpl(const base::char16* registry_path,
                           base::ProcessId pid,
                           base::Time creation_time) {
  base::string16 key_name = registry_path;
  base::StringAppendF(
      &key_name, L"\\%d-%lld", pid, creation_time.ToInternalValue());

  LONG res = key_.Create(HKEY_CURRENT_USER, key_name.c_str(), KEY_SET_VALUE);
  if (res != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to create key " << key_name << " error " << res;
    return false;
  }

  return true;
}

bool ExitFunnel::RecordEvent(const base::char16* event_name) {
  if (!key_.Valid())
    return false;

  int64_t now = base::Time::Now().ToInternalValue();

  LONG res = key_.WriteValue(event_name, &now, sizeof(now), REG_QWORD);
  if (res != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to write value " << event_name << " error " << res;
    return false;
  }

  return true;
}

bool ExitFunnel::RecordSingleEvent(const base::char16* registry_path,
                                   const base::char16* event_name) {
  ExitFunnel funnel;

  if (!funnel.Init(registry_path, base::GetCurrentProcessHandle()))
    return false;

  return funnel.RecordEvent(event_name);
}

}  // namespace browser_watcher
