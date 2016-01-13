// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_UTILITY_PROCESS_HOST_IMPL_H_
#define CONTENT_BROWSER_UTILITY_PROCESS_HOST_IMPL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/utility_process_host.h"

namespace base {
class SequencedTaskRunner;
class Thread;
}

namespace content {
class BrowserChildProcessHostImpl;

typedef base::Thread* (*UtilityMainThreadFactoryFunction)(
    const std::string& id);

class CONTENT_EXPORT UtilityProcessHostImpl
    : public NON_EXPORTED_BASE(UtilityProcessHost),
      public BrowserChildProcessHostDelegate {
 public:
  static void RegisterUtilityMainThreadFactory(
      UtilityMainThreadFactoryFunction create);

  UtilityProcessHostImpl(UtilityProcessHostClient* client,
                         base::SequencedTaskRunner* client_task_runner);
  virtual ~UtilityProcessHostImpl();

  // UtilityProcessHost implementation:
  virtual bool Send(IPC::Message* message) OVERRIDE;
  virtual bool StartBatchMode() OVERRIDE;
  virtual void EndBatchMode() OVERRIDE;
  virtual void SetExposedDir(const base::FilePath& dir) OVERRIDE;
  virtual void EnableMDns() OVERRIDE;
  virtual void DisableSandbox() OVERRIDE;
#if defined(OS_WIN)
  virtual void ElevatePrivileges() OVERRIDE;
#endif
  virtual const ChildProcessData& GetData() OVERRIDE;
#if defined(OS_POSIX)
  virtual void SetEnv(const base::EnvironmentMap& env) OVERRIDE;
#endif

  void set_child_flags(int flags) { child_flags_ = flags; }

 private:
  // Starts a process if necessary.  Returns true if it succeeded or a process
  // has already been started via StartBatchMode().
  bool StartProcess();

  // BrowserChildProcessHost:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnProcessLaunchFailed() OVERRIDE;
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;

  // A pointer to our client interface, who will be informed of progress.
  scoped_refptr<UtilityProcessHostClient> client_;
  scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  // True when running in batch mode, i.e., StartBatchMode() has been called
  // and the utility process will run until EndBatchMode().
  bool is_batch_mode_;

  base::FilePath exposed_dir_;

  // Whether the utility process needs to perform presandbox initialization
  // for mDNS.
  bool is_mdns_enabled_;

  // Whether to pass switches::kNoSandbox to the child.
  bool no_sandbox_;

  // Whether to launch the process with elevated privileges.
  bool run_elevated_;

  // Flags defined in ChildProcessHost with which to start the process.
  int child_flags_;

  base::EnvironmentMap env_;

  bool started_;

  scoped_ptr<BrowserChildProcessHostImpl> process_;

  // Used in single-process mode instead of process_.
  scoped_ptr<base::Thread> in_process_thread_;

  DISALLOW_COPY_AND_ASSIGN(UtilityProcessHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_UTILITY_PROCESS_HOST_IMPL_H_
