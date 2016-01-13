// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_UTILITY_PROCESS_HOST_H_
#define CONTENT_PUBLIC_BROWSER_UTILITY_PROCESS_HOST_H_

#include "base/environment.h"
#include "base/process/launch.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "ipc/ipc_sender.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace content {
class UtilityProcessHostClient;
struct ChildProcessData;

// This class acts as the browser-side host to a utility child process.  A
// utility process is a short-lived process that is created to run a specific
// task.  This class lives solely on the IO thread.
// If you need a single method call in the process, use StartFooBar(p).
// If you need multiple batches of work to be done in the process, use
// StartBatchMode(), then multiple calls to StartFooBar(p), then finish with
// EndBatchMode().
//
// Note: If your class keeps a ptr to an object of this type, grab a weak ptr to
// avoid a use after free since this object is deleted synchronously but the
// client notification is asynchronous.  See http://crbug.com/108871.
class UtilityProcessHost : public IPC::Sender,
                           public base::SupportsWeakPtr<UtilityProcessHost> {
 public:
  // Used to create a utility process.
  CONTENT_EXPORT static UtilityProcessHost* Create(
      UtilityProcessHostClient* client,
      base::SequencedTaskRunner* client_task_runner);

  virtual ~UtilityProcessHost() {}

  // Starts utility process in batch mode. Caller must call EndBatchMode()
  // to finish the utility process.
  virtual bool StartBatchMode() = 0;

  // Ends the utility process. Must be called after StartBatchMode().
  virtual void EndBatchMode() = 0;

  // Allows a directory to be opened through the sandbox, in case it's needed by
  // the operation.
  virtual void SetExposedDir(const base::FilePath& dir) = 0;

  // Perform presandbox initialization for mDNS.
  virtual void EnableMDns() = 0;

  // Make the process run without a sandbox.
  virtual void DisableSandbox() = 0;

#if defined(OS_WIN)
  // Make the process run elevated.
  virtual void ElevatePrivileges() = 0;
#endif

  // Returns information about the utility child process.
  virtual const ChildProcessData& GetData() = 0;

#if defined(OS_POSIX)
  virtual void SetEnv(const base::EnvironmentMap& env) = 0;
#endif
};

};  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_UTILITY_PROCESS_HOST_H_
