// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/cpp/application_runner.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/cpp/shell_connection.h"

namespace shell {

int g_application_runner_argc;
const char* const* g_application_runner_argv;

ApplicationRunner::ApplicationRunner(ShellClient* client)
    : client_(std::unique_ptr<ShellClient>(client)),
      message_loop_type_(base::MessageLoop::TYPE_DEFAULT),
      has_run_(false) {}

ApplicationRunner::~ApplicationRunner() {}

void ApplicationRunner::InitBaseCommandLine() {
  base::CommandLine::Init(g_application_runner_argc, g_application_runner_argv);
}

void ApplicationRunner::set_message_loop_type(base::MessageLoop::Type type) {
  DCHECK_NE(base::MessageLoop::TYPE_CUSTOM, type);
  DCHECK(!has_run_);

  message_loop_type_ = type;
}

MojoResult ApplicationRunner::Run(MojoHandle shell_client_request_handle,
                                  bool init_base) {
  DCHECK(!has_run_);
  has_run_ = true;

  std::unique_ptr<base::AtExitManager> at_exit;
  if (init_base) {
    InitBaseCommandLine();
    at_exit.reset(new base::AtExitManager);
  }

  {
    std::unique_ptr<base::MessageLoop> loop;
    loop.reset(new base::MessageLoop(message_loop_type_));

    connection_.reset(new ShellConnection(
        client_.get(),
        mojo::MakeRequest<mojom::ShellClient>(mojo::MakeScopedHandle(
            mojo::MessagePipeHandle(shell_client_request_handle)))));
    base::RunLoop run_loop;
    connection_->SetConnectionLostClosure(run_loop.QuitClosure());
    run_loop.Run();
    // It's very common for the client to cache the app and terminate on errors.
    // If we don't delete the client before the app we run the risk of the
    // client having a stale reference to the app and trying to use it.
    // Note that we destruct the message loop first because that might trigger
    // connection error handlers and they might access objects created by the
    // client.
    loop.reset();
    client_.reset();
    connection_.reset();
  }
  return MOJO_RESULT_OK;
}

MojoResult ApplicationRunner::Run(MojoHandle shell_client_request_handle) {
  bool init_base = true;
  if (base::CommandLine::InitializedForCurrentProcess()) {
    init_base =
        !base::CommandLine::ForCurrentProcess()->HasSwitch("single-process");
  }
  return Run(shell_client_request_handle, init_base);
}

void ApplicationRunner::DestroyShellConnection() {
  connection_.reset();
}

void ApplicationRunner::Quit() {
  base::MessageLoop::current()->QuitWhenIdle();
}

}  // namespace shell
