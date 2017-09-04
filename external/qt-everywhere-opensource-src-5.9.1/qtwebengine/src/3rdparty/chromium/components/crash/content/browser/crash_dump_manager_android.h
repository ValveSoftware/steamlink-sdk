// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_DUMP_MANAGER_ANDROID_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_DUMP_MANAGER_ANDROID_H_

#include <map>

#include "base/android/application_status_listener.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/process_type.h"

namespace content {
class RenderProcessHost;
}

namespace breakpad {

// This class manages the crash minidumps.
// On Android, because of process isolation, each renderer process runs with a
// different UID. As a result, we cannot generate the minidumps in the browser
// (as the browser process does not have access to some system files for the
// crashed process). So the minidump is generated in the renderer process.
// Since the isolated process cannot open files, we provide it on creation with
// a file descriptor where to write the minidump in the event of a crash.
// This class creates these file descriptors and associates them with render
// processes and take the appropriate action when the render process terminates.
class CrashDumpManager : public content::BrowserChildProcessObserver,
                         public content::NotificationObserver {
 public:
  // The embedder should create a single instance of the CrashDumpManager.
  static CrashDumpManager* GetInstance();

  // Should be created on the UI thread.
  explicit CrashDumpManager(const base::FilePath& crash_dump_dir);

  ~CrashDumpManager() override;

  // Returns a file that should be used to generate a minidump for the process
  // |child_process_id|.
  base::File CreateMinidumpFile(int child_process_id);

 private:
  typedef std::map<int, base::FilePath> ChildProcessIDToMinidumpPath;

  // This enum is used to back a UMA histogram, and must be treated as
  // append-only.
  enum ExitStatus {
    EMPTY_MINIDUMP_WHILE_RUNNING,
    EMPTY_MINIDUMP_WHILE_PAUSED,
    EMPTY_MINIDUMP_WHILE_BACKGROUND,
    VALID_MINIDUMP_WHILE_RUNNING,
    VALID_MINIDUMP_WHILE_PAUSED,
    VALID_MINIDUMP_WHILE_BACKGROUND,
    MINIDUMP_STATUS_COUNT
  };

  static void ProcessMinidump(const base::FilePath& minidump_path,
                              base::ProcessHandle pid,
                              content::ProcessType process_type,
                              base::TerminationStatus termination_status,
                              base::android::ApplicationState app_state);

  // content::BrowserChildProcessObserver implementation:
  void BrowserChildProcessHostDisconnected(
      const content::ChildProcessData& data) override;
  void BrowserChildProcessCrashed(
      const content::ChildProcessData& data,
      int exit_code) override;

  // NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Called on child process exit (including crash).
  void OnChildExit(int child_process_id,
                   base::ProcessHandle pid,
                   content::ProcessType process_type,
                   base::TerminationStatus termination_status,
                   base::android::ApplicationState app_state);

  content::NotificationRegistrar notification_registrar_;

  // This map should only be accessed with its lock aquired as it is accessed
  // from the PROCESS_LAUNCHER and UI threads.
  base::Lock child_process_id_to_minidump_path_lock_;
  ChildProcessIDToMinidumpPath child_process_id_to_minidump_path_;

  base::FilePath crash_dump_dir_;

  static CrashDumpManager* instance_;

  DISALLOW_COPY_AND_ASSIGN(CrashDumpManager);
};

}  // namespace breakpad

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_DUMP_MANAGER_ANDROID_H_
