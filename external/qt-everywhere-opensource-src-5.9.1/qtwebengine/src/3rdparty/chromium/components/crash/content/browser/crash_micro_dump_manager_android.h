// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_MICRO_DUMP_MANAGER_ANDROID_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_MICRO_DUMP_MANAGER_ANDROID_H_

#include <map>
#include <memory>

#include "base/files/scoped_file.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "base/sync_socket.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace breakpad {

// This class manages behavior of the browser on renderer crashes when
// microdumps are used for capturing the crash stack.  Normally, in this case
// the browser doesn't need to do much, because a microdump is written into
// Android log by the renderer process itself.  However, the browser may need to
// crash itself on a renderer crash.  Since on Android renderers are not child
// processes of the browser, it can't access the exit code. Instead, the browser
// uses a dedicated pipe in order to receive the information about the renderer
// crash status.
class CrashMicroDumpManager : public content::NotificationObserver {
 public:
  // There is only a single instance of CrashMicroDumpManager per browser
  // process. It needs to be created on the UI thread.
  static CrashMicroDumpManager* GetInstance();

  // Returns a pipe that should be used to transfer termination cause of
  // |child_process_id|.
  base::ScopedFD CreateCrashInfoChannel(int child_process_id);

 private:
  friend struct base::DefaultSingletonTraits<CrashMicroDumpManager>;

  CrashMicroDumpManager();
  ~CrashMicroDumpManager() override;

  // NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Called on child process exit (including crash).
  void HandleChildTerminationOnFileThread(
      int child_process_id,
      base::TerminationStatus termination_status);

  content::NotificationRegistrar notification_registrar_;

  // This map should only be accessed with its lock aquired as it is accessed
  // from the PROCESS_LAUNCHER, FILE, and UI threads.
  base::Lock child_process_id_to_pipe_lock_;
  std::map<int, std::unique_ptr<base::SyncSocket>> child_process_id_to_pipe_;
  base::WeakPtrFactory<CrashMicroDumpManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrashMicroDumpManager);
};

}  // namespace breakpad

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_MICRO_DUMP_MANAGER_ANDROID_H_
