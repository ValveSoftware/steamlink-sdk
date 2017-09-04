// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/views_aura_mus_test_suite.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "services/service_manager/background/background_service_manager.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/ui/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/views/mus/desktop_window_tree_host_mus.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/mus/test_utils.h"
#include "ui/views/test/platform_test_helper.h"
#include "ui/views/test/views_test_helper_aura.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

namespace views {
namespace {

void EnsureCommandLineSwitch(const std::string& name) {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(name))
    cmd_line->AppendSwitch(name);
}

class DefaultService : public service_manager::Service {
 public:
  DefaultService() {}
  ~DefaultService() override {}

  // service_manager::Service:
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultService);
};

class PlatformTestHelperMus : public PlatformTestHelper {
 public:
  PlatformTestHelperMus(service_manager::Connector* connector,
                        const service_manager::Identity& identity) {
    aura::test::EnvTestHelper().SetWindowTreeClient(nullptr);
    // It is necessary to recreate the MusClient for each test,
    // since a new MessageLoop is created for each test.
    mus_client_ = test::MusClientTestApi::Create(connector, identity);
  }
  ~PlatformTestHelperMus() override {
    aura::test::EnvTestHelper().SetWindowTreeClient(nullptr);
  }

  // PlatformTestHelper:
  void OnTestHelperCreated(ViewsTestHelper* helper) override {
    static_cast<ViewsTestHelperAura*>(helper)->EnableMusWithWindowTreeClient(
        mus_client_->window_tree_client());
  }
  void SimulateNativeDestroy(Widget* widget) override {
    // TODO: this should call through to a WindowTreeClientDelegate
    // function that destroys the WindowTreeHost. http://crbug.com/663868.
    widget->CloseNow();
  }

 private:
  std::unique_ptr<MusClient> mus_client_;

  DISALLOW_COPY_AND_ASSIGN(PlatformTestHelperMus);
};

std::unique_ptr<PlatformTestHelper> CreatePlatformTestHelper(
    const service_manager::Identity& identity,
    const base::Callback<service_manager::Connector*(void)>& callback) {
  return base::MakeUnique<PlatformTestHelperMus>(callback.Run(), identity);
}

}  // namespace

class ServiceManagerConnection {
 public:
  ServiceManagerConnection()
      : thread_("Persistent service_manager connections") {
    base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    base::Thread::Options options;
    thread_.StartWithOptions(options);
    thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&ServiceManagerConnection::SetUpConnections,
                              base::Unretained(this), &wait));
    wait.Wait();

    // WindowManagerConnection cannot be created from here yet, although the
    // connector and identity are available at this point. This is because
    // WindowManagerConnection needs a ViewsDelegate and a MessageLoop to have
    // been installed first. So delay the creation until the necessary
    // dependencies have been met.
    PlatformTestHelper::set_factory(
        base::Bind(&CreatePlatformTestHelper, service_manager_identity_,
                   base::Bind(&ServiceManagerConnection::GetConnector,
                              base::Unretained(this))));
  }

  ~ServiceManagerConnection() {
    base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&ServiceManagerConnection::TearDownConnections,
                              base::Unretained(this), &wait));
    wait.Wait();
  }

 private:
  service_manager::Connector* GetConnector() {
    service_manager_connector_.reset();
    base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&ServiceManagerConnection::CloneConnector,
                              base::Unretained(this), &wait));
    wait.Wait();
    DCHECK(service_manager_connector_);
    return service_manager_connector_.get();
  }

  void CloneConnector(base::WaitableEvent* wait) {
    service_manager_connector_ = context_->connector()->Clone();
    wait->Signal();
  }

  void SetUpConnections(base::WaitableEvent* wait) {
    background_service_manager_ =
        base::MakeUnique<service_manager::BackgroundServiceManager>();
    background_service_manager_->Init(nullptr);
    context_ =
        base::MakeUnique<service_manager::ServiceContext>(
            base::MakeUnique<DefaultService>(),
            background_service_manager_->CreateServiceRequest(GetTestName()));

    // ui/views/mus requires a WindowManager running, so launch test_wm.
    service_manager::Connector* connector = context_->connector();
    connector->Connect("test_wm");
    service_manager_connector_ = connector->Clone();
    service_manager_identity_ = context_->identity();
    wait->Signal();
  }

  void TearDownConnections(base::WaitableEvent* wait) {
    context_.reset();
    wait->Signal();
  }

  // Returns the name of the test executable, e.g.
  // "views_mus_unittests".
  std::string GetTestName() {
    base::FilePath executable = base::CommandLine::ForCurrentProcess()
                                    ->GetProgram()
                                    .BaseName()
                                    .RemoveExtension();
    return std::string("") + executable.MaybeAsASCII();
  }

  base::Thread thread_;
  std::unique_ptr<service_manager::BackgroundServiceManager>
      background_service_manager_;
  std::unique_ptr<service_manager::ServiceContext> context_;
  std::unique_ptr<service_manager::Connector> service_manager_connector_;
  service_manager::Identity service_manager_identity_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManagerConnection);
};

ViewsAuraMusTestSuite::ViewsAuraMusTestSuite(int argc, char** argv)
    : ViewsTestSuite(argc, argv) {}

ViewsAuraMusTestSuite::~ViewsAuraMusTestSuite() {}

void ViewsAuraMusTestSuite::Initialize() {
  PlatformTestHelper::SetIsAuraMusClient();
  // Let other services know that we're running in tests. Do this with a
  // command line flag to avoid making blocking calls to other processes for
  // setup for tests (e.g. to unlock the screen in the window manager).
  EnsureCommandLineSwitch(ui::switches::kUseTestConfig);

  ViewsTestSuite::Initialize();
  service_manager_connections_ = base::MakeUnique<ServiceManagerConnection>();
}

void ViewsAuraMusTestSuite::Shutdown() {
  service_manager_connections_.reset();
  ViewsTestSuite::Shutdown();
}

}  // namespace views
