// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/watcher_client_win.h"

#include <windows.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/win/windows_version.h"

namespace browser_watcher {

namespace {

base::Process OpenOwnProcessInheritable() {
  return base::Process(
      ::OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION,
                    TRUE,  // Ineritable handle.
                    base::GetCurrentProcId()));
}

}  // namespace

WatcherClient::WatcherClient(const CommandLineGenerator& command_line_generator)
    : use_legacy_launch_(base::win::GetVersion() < base::win::VERSION_VISTA),
      command_line_generator_(command_line_generator),
      process_(base::kNullProcessHandle) {
}

WatcherClient::~WatcherClient() {
}

void WatcherClient::LaunchWatcher() {
  DCHECK(!process_.IsValid());

  // Build the command line for the watcher process.
  base::Process self = OpenOwnProcessInheritable();
  DCHECK(self.IsValid());
  base::CommandLine cmd_line(command_line_generator_.Run(self.Handle()));

  base::HandlesToInheritVector to_inherit;
  base::LaunchOptions options;
  options.start_hidden = true;
  if (use_legacy_launch_) {
    // Launch the child process inheriting all handles on XP.
    options.inherit_handles = true;
  } else {
    // Launch the child process inheriting only |self| on
    // Vista and better.
    to_inherit.push_back(self.Handle());
    to_inherit.insert(to_inherit.end(), inherited_handles_.begin(),
                      inherited_handles_.end());
    options.handles_to_inherit = &to_inherit;
  }

  process_ = base::LaunchProcess(cmd_line, options);
  if (!process_.IsValid())
    LOG(ERROR) << "Failed to launch browser watcher.";
}

void WatcherClient::AddInheritedHandle(HANDLE handle) {
  inherited_handles_.push_back(handle);
}

}  // namespace browser_watcher
