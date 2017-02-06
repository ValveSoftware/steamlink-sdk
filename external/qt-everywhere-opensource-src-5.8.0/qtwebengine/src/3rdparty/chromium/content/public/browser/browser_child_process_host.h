// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_CHILD_PROCESS_HOST_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_CHILD_PROCESS_HOST_H_

#include "base/environment.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/process_type.h"
#include "ipc/ipc_sender.h"

#if defined(OS_MACOSX)
#include "base/process/port_provider_mac.h"
#endif

namespace base {
class CommandLine;
class FilePath;
}

namespace shell {
class InterfaceProvider;
class InterfaceRegistry;
}

namespace content {

class BrowserChildProcessHostDelegate;
class ChildProcessHost;
class SandboxedProcessLauncherDelegate;
struct ChildProcessData;

// This represents child processes of the browser process, i.e. plugins. They
// will get terminated at browser shutdown.
class CONTENT_EXPORT BrowserChildProcessHost : public IPC::Sender {
 public:
  // Used to create a child process host. The delegate must outlive this object.
  // |process_type| needs to be either an enum value from ProcessType or an
  // embedder-defined value.
  static BrowserChildProcessHost* Create(
      content::ProcessType process_type,
      BrowserChildProcessHostDelegate* delegate);

  // Used to create a child process host, with a unique token to identify the
  // child process to Mojo. |mojo_child_token| should be a unique string
  // generated using mojo::edk::GenerateRandomToken().
  static BrowserChildProcessHost* Create(
      content::ProcessType process_type,
      BrowserChildProcessHostDelegate* delegate,
      const std::string& mojo_child_token);

  // Returns the child process host with unique id |child_process_id|, or
  // nullptr if it doesn't exist. |child_process_id| is NOT the process ID, but
  // is the same unique ID as |ChildProcessData::id|.
  static BrowserChildProcessHost* FromID(int child_process_id);

  ~BrowserChildProcessHost() override {}

  // Derived classes call this to launch the child process asynchronously.
  // Takes ownership of |cmd_line| and |delegate|.
  virtual void Launch(
      SandboxedProcessLauncherDelegate* delegate,
      base::CommandLine* cmd_line,
      bool terminate_on_shutdown) = 0;

  virtual const ChildProcessData& GetData() const = 0;

  // Returns the ChildProcessHost object used by this object.
  virtual ChildProcessHost* GetHost() const = 0;

  // Returns the termination status of a child.  |exit_code| is the
  // status returned when the process exited (for posix, as returned
  // from waitpid(), for Windows, as returned from
  // GetExitCodeProcess()).  |exit_code| may be nullptr.
  // |known_dead| indicates that the child is already dead. On Linux, this
  // information is necessary to retrieve accurate information. See
  // ChildProcessLauncher::GetChildTerminationStatus() for more details.
  virtual base::TerminationStatus GetTerminationStatus(
      bool known_dead, int* exit_code) = 0;

  // Sets the user-visible name of the process.
  virtual void SetName(const base::string16& name) = 0;

  // Set the handle of the process. BrowserChildProcessHost will do this when
  // the Launch method is used to start the process. However if the owner
  // of this object doesn't call Launch and starts the process in another way,
  // they need to call this method so that the process handle is associated with
  // this object.
  virtual void SetHandle(base::ProcessHandle handle) = 0;

  // Returns the shell::InterfaceRegistry the browser process uses to expose
  // interfaces to the child.
  virtual shell::InterfaceRegistry* GetInterfaceRegistry() = 0;

  // Returns the shell::InterfaceProvider the browser process can use to bind
  // interfaces exposed to it from the child.
  virtual shell::InterfaceProvider* GetRemoteInterfaces() = 0;

#if defined(OS_MACOSX)
  // Returns a PortProvider used to get the task port for child processes.
  static base::PortProvider* GetPortProvider();
#endif
};

};  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_CHILD_PROCESS_HOST_H_
