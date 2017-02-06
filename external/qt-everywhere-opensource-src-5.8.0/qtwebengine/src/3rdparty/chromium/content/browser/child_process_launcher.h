// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_H_
#define CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_H_

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/threading/non_thread_safe.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox_types.h"
#endif

namespace base {
class CommandLine;
}

namespace content {

// Note: These codes are listed in a histogram and any new codes should be added
// at the end.
enum LaunchResultCode {
  // Launch start code, to not overlap with sandbox::ResultCode.
  LAUNCH_RESULT_START = 1001,
  // Launch success.
  LAUNCH_RESULT_SUCCESS,
  // Generic launch failure.
  LAUNCH_RESULT_FAILURE,
  // Placeholder for last item of the enum.
  LAUNCH_RESULT_CODE_LAST_CODE
};

#if defined(OS_WIN)
static_assert(static_cast<int>(LAUNCH_RESULT_START) >
                  static_cast<int>(sandbox::SBOX_ERROR_LAST),
              "LaunchResultCode must not overlap with sandbox::ResultCode");
#endif

// Launches a process asynchronously and notifies the client of the process
// handle when it's available.  It's used to avoid blocking the calling thread
// on the OS since often it can take > 100 ms to create the process.
class CONTENT_EXPORT ChildProcessLauncher : public base::NonThreadSafe {
 public:
  class CONTENT_EXPORT Client {
   public:
    // Will be called on the thread that the ChildProcessLauncher was
    // constructed on.
    virtual void OnProcessLaunched() = 0;

    virtual void OnProcessLaunchFailed(int error_code) {};

   protected:
    virtual ~Client() {}
  };

  // Launches the process asynchronously, calling the client when the result is
  // ready.  Deleting this object before the process is created is safe, since
  // the callback won't be called.  If the process is still running by the time
  // this object destructs, it will be terminated.
  // Takes ownership of cmd_line.
  //
  // If |process_error_callback| is provided, it will be called if a Mojo error
  // is encountered when processing messages from the child process. This
  // callback must be safe to call from any thread.
  ChildProcessLauncher(
      SandboxedProcessLauncherDelegate* delegate,
      base::CommandLine* cmd_line,
      int child_process_id,
      Client* client,
      const std::string& mojo_child_token,
      const mojo::edk::ProcessErrorCallback& process_error_callback,
      bool terminate_on_shutdown = true);
  ~ChildProcessLauncher();

  // True if the process is being launched and so the handle isn't available.
  bool IsStarting();

  // Getter for the process.  Only call after the process has started.
  const base::Process& GetProcess() const;

  // Call this when the child process exits to know what happened to it.
  // |known_dead| can be true if we already know the process is dead as it can
  // help the implemention figure the proper TerminationStatus.
  // On Linux, the use of |known_dead| is subtle and can be crucial if an
  // accurate status is important. With |known_dead| set to false, a dead
  // process could be seen as running. With |known_dead| set to true, the
  // process will be killed if it was still running. See ZygoteHostImpl for
  // more discussion of Linux implementation details.
  // |exit_code| is the exit code of the process if it exited (e.g. status from
  // waitpid if on posix, from GetExitCodeProcess on Windows). |exit_code| may
  // be NULL.
  base::TerminationStatus GetChildTerminationStatus(bool known_dead,
                                                    int* exit_code);

  // Changes whether the process runs in the background or not.  Only call
  // this after the process has started.
  void SetProcessBackgrounded(bool background);

  // Replaces the ChildProcessLauncher::Client for testing purposes. Returns the
  // previous  client.
  Client* ReplaceClientForTest(Client* client);

 private:
  // Posts a task to the launcher thread to do the actual work.
  void Launch(SandboxedProcessLauncherDelegate* delegate,
              base::CommandLine* cmd_line,
              int child_process_id);

  void UpdateTerminationStatus(bool known_dead);

  // This is always called on the client thread after an attempt
  // to launch the child process on the launcher thread.
  // It makes sure we always perform the necessary cleanup if the
  // client went away.
  static void DidLaunch(base::WeakPtr<ChildProcessLauncher> instance,
                        bool terminate_on_shutdown,
                        ZygoteHandle zygote,
#if defined(OS_ANDROID)
                        base::ScopedFD ipcfd,
                        base::ScopedFD mojo_fd,
#endif
                        base::Process process,
                        int error_code);

  // Notifies the client about the result of the operation.
  void Notify(ZygoteHandle zygote,
#if defined(OS_ANDROID)
              base::ScopedFD ipcfd,
#endif
              base::Process process,
              int error_code);

#if defined(MOJO_SHELL_CLIENT)
  // When this process is run from an external Mojo shell, this function will
  // create a channel and pass one end to the spawned process and register the
  // other end with the external shell, allowing the spawned process to bind an
  // Application request from the shell.
  void CreateMojoShellChannel(base::CommandLine* command_line,
                              int child_process_id);
#endif

  Client* client_;
  BrowserThread::ID client_thread_id_;
  base::Process process_;
  base::TerminationStatus termination_status_;
  int exit_code_;
  ZygoteHandle zygote_;
  bool starting_;
  const mojo::edk::ProcessErrorCallback process_error_callback_;

  // Controls whether the child process should be terminated on browser
  // shutdown. Default behavior is to terminate the child.
  const bool terminate_child_on_shutdown_;

  // Host side platform handle to establish Mojo IPC.
  mojo::edk::ScopedPlatformHandle mojo_host_platform_handle_;
  const std::string mojo_child_token_;

  base::WeakPtrFactory<ChildProcessLauncher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessLauncher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_H_
