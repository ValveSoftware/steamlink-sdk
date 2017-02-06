// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_APPLICATION_RUNNER_H_
#define SERVICES_SHELL_PUBLIC_CPP_APPLICATION_RUNNER_H_

#include <memory>

#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/system/core.h"

namespace shell {

class ShellClient;
class ShellConnection;

// A utility for running a chromium based mojo Application. The typical use
// case is to use when writing your MojoMain:
//
//  MojoResult MojoMain(MojoHandle shell_handle) {
//    shell::ApplicationRunner runner(new MyDelegate());
//    return runner.Run(shell_handle);
//  }
//
// ApplicationRunner takes care of chromium environment initialization and
// shutdown, and starting a MessageLoop from which your application can run and
// ultimately Quit().
class ApplicationRunner {
 public:
  // Takes ownership of |client|.
  explicit ApplicationRunner(ShellClient* client);
  ~ApplicationRunner();

  static void InitBaseCommandLine();

  void set_message_loop_type(base::MessageLoop::Type type);

  // Once the various parameters have been set above, use Run to initialize an
  // ShellConnection wired to the provided delegate, and run a MessageLoop until
  // the application exits.
  //
  // Iff |init_base| is true, the runner will perform some initialization of
  // base globals (e.g. CommandLine and AtExitManager) before starting the
  // application.
  MojoResult Run(MojoHandle shell_handle, bool init_base);

  // Calls Run above with |init_base| set to |true|.
  MojoResult Run(MojoHandle shell_handle);

  // Allows the caller to shut down the connection with the shell. After the
  // shell notices the pipe has closed, it will no longer track an instance of
  // this application, though this application may continue to run and service
  // requests from others.
  void DestroyShellConnection();

  // Allows the caller to explicitly quit the application. Must be called from
  // the thread which created the ApplicationRunner.
  void Quit();

 private:
  std::unique_ptr<ShellConnection> connection_;
  std::unique_ptr<ShellClient> client_;

  // MessageLoop type. TYPE_CUSTOM is default (MessagePumpMojo will be used as
  // the underlying message pump).
  base::MessageLoop::Type message_loop_type_;
  // Whether Run() has been called.
  bool has_run_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationRunner);
};

}  // namespace shell

#endif  // SERVICES_SHELL_PUBLIC_CPP_APPLICATION_RUNNER_H_
