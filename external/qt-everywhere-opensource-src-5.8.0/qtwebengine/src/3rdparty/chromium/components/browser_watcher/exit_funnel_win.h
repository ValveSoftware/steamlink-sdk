// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_WATCHER_EXIT_FUNNEL_WIN_H_
#define COMPONENTS_BROWSER_WATCHER_EXIT_FUNNEL_WIN_H_

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/win/registry.h"

namespace browser_watcher {

// A wrapper class that takes care of persistently recording a trace of named,
// timed events in registry, associated with a process instance. This allows
// reconstructing and reporting an event trace on a subsequent launch.
// The event trace is stored in registry under a new key named
// <pid>-<uniquifier>, where each event is a value named after the event, with
// an associated QWORD value noting the event time.
// Note: this is deprecated, and is only kept around for testing the cleanup
// in the WatcherMetricsProvider.
// TODO(siggi): Kill this class - see http://crbug.com/442526.
class ExitFunnel {
 public:
  ExitFunnel();
  ~ExitFunnel();

  // Initializes the exit funnel with |registry_path| and |process|.
  // |process| must be open for PROCESS_QUERY_INFORMATION at least.
  // Returns false on failure to create or open the registry path corresponding
  // to |process|.
  bool Init(const base::char16* registry_path, base::ProcessHandle process);

  // Initializes the exit funnel with |registry_path|, |pid| and
  // |creation_time|.
  // Returns false on failure to create or open the registry path corresponding
  // to |process|.
  // Exposed for testing.
  bool InitImpl(const base::char16* registry_path,
                base::ProcessId pid,
                base::Time creation_time);

  // Records |event_name| at the current time in the registry for the process
  // this instance is associated with. Returns false on failure to record the
  // event.
  bool RecordEvent(const base::char16* event_name);

  // Records |event_name| at the current time in the registry at |registry_path|
  // for this process.
  static bool RecordSingleEvent(const base::char16* registry_path,
                                const base::char16* event_name);

 private:
  base::win::RegKey key_;

  DISALLOW_COPY_AND_ASSIGN(ExitFunnel);
};

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_EXIT_FUNNEL_WIN_H_
