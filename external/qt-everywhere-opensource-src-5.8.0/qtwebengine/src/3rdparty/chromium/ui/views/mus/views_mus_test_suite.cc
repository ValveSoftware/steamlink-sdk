// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/views_mus_test_suite.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "components/mus/common/gpu_service.h"
#include "components/mus/common/switches.h"
#include "services/shell/background/background_shell.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/cpp/shell_connection.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/test/platform_test_helper.h"
#include "ui/views/views_delegate.h"

namespace views {
namespace {

void EnsureCommandLineSwitch(const std::string& name) {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(name))
    cmd_line->AppendSwitch(name);
}

class DefaultShellClient : public shell::ShellClient {
 public:
  DefaultShellClient() {}
  ~DefaultShellClient() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultShellClient);
};

class PlatformTestHelperMus : public PlatformTestHelper {
 public:
  PlatformTestHelperMus(shell::Connector* connector,
                        const shell::Identity& identity) {
    mus::GpuService::Initialize(connector);
    // It is necessary to recreate the WindowManagerConnection for each test,
    // since a new MessageLoop is created for each test.
    connection_ = WindowManagerConnection::Create(connector, identity);
  }
  ~PlatformTestHelperMus() override {
    mus::GpuService::Terminate();
  }

 private:
  std::unique_ptr<WindowManagerConnection> connection_;

  DISALLOW_COPY_AND_ASSIGN(PlatformTestHelperMus);
};

std::unique_ptr<PlatformTestHelper> CreatePlatformTestHelper(
    const shell::Identity& identity,
    const base::Callback<shell::Connector*(void)>& callback) {
  return base::WrapUnique(new PlatformTestHelperMus(callback.Run(), identity));
}

}  // namespace

class ShellConnection {
 public:
  ShellConnection() : thread_("Persistent shell connections") {
    base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    base::Thread::Options options;
    thread_.StartWithOptions(options);
    thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&ShellConnection::SetUpConnections,
                              base::Unretained(this), &wait));
    wait.Wait();

    // WindowManagerConnection cannot be created from here yet, although the
    // connector and identity are available at this point. This is because
    // WindowManagerConnection needs a ViewsDelegate and a MessageLoop to have
    // been installed first. So delay the creation until the necessary
    // dependencies have been met.
    PlatformTestHelper::set_factory(base::Bind(
        &CreatePlatformTestHelper, shell_identity_,
        base::Bind(&ShellConnection::GetConnector, base::Unretained(this))));
  }

  ~ShellConnection() {
    base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&ShellConnection::TearDownConnections,
                              base::Unretained(this), &wait));
    wait.Wait();
  }

 private:
  shell::Connector* GetConnector() {
    shell_connector_.reset();
    base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread_.task_runner()->PostTask(FROM_HERE,
                                    base::Bind(&ShellConnection::CloneConnector,
                                               base::Unretained(this), &wait));
    wait.Wait();
    DCHECK(shell_connector_);
    return shell_connector_.get();
  }

  void CloneConnector(base::WaitableEvent* wait) {
    shell_connector_ = shell_connection_->connector()->Clone();
    wait->Signal();
  }

  void SetUpConnections(base::WaitableEvent* wait) {
    background_shell_.reset(new shell::BackgroundShell);
    background_shell_->Init(nullptr);
    shell_client_.reset(new DefaultShellClient);
    shell_connection_.reset(new shell::ShellConnection(
        shell_client_.get(),
        background_shell_->CreateShellClientRequest(GetTestName())));

    // ui/views/mus requires a WindowManager running, so launch test_wm.
    shell::Connector* connector = shell_connection_->connector();
    connector->Connect("mojo:test_wm");
    shell_connector_ = connector->Clone();
    shell_identity_ = shell_connection_->identity();
    wait->Signal();
  }

  void TearDownConnections(base::WaitableEvent* wait) {
    shell_connection_.reset();
    wait->Signal();
  }

  // Returns the name of the test executable, e.g. "exe:views_mus_unittests".
  std::string GetTestName() {
    base::FilePath executable = base::CommandLine::ForCurrentProcess()
                                    ->GetProgram()
                                    .BaseName()
                                    .RemoveExtension();
    return std::string("exe:") + executable.MaybeAsASCII();
  }

  base::Thread thread_;
  std::unique_ptr<shell::BackgroundShell> background_shell_;
  std::unique_ptr<shell::ShellConnection> shell_connection_;
  std::unique_ptr<DefaultShellClient> shell_client_;
  std::unique_ptr<shell::Connector> shell_connector_;
  shell::Identity shell_identity_;

  DISALLOW_COPY_AND_ASSIGN(ShellConnection);
};

ViewsMusTestSuite::ViewsMusTestSuite(int argc, char** argv)
    : ViewsTestSuite(argc, argv) {}

ViewsMusTestSuite::~ViewsMusTestSuite() {}

void ViewsMusTestSuite::Initialize() {
  PlatformTestHelper::SetIsMus();
  // Let other mojo apps know that we're running in tests. Do this with a
  // command line flag to avoid making blocking calls to other processes for
  // setup for tests (e.g. to unlock the screen in the window manager).
  EnsureCommandLineSwitch(mus::switches::kUseTestConfig);

  ViewsTestSuite::Initialize();
  shell_connections_.reset(new ShellConnection);
}

void ViewsMusTestSuite::Shutdown() {
  shell_connections_.reset();
  ViewsTestSuite::Shutdown();
}

}  // namespace views
