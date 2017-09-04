// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/background/background_service_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump_default.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "services/catalog/store.h"
#include "services/service_manager/connect_params.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/service_manager.h"
#include "services/service_manager/standalone/context.h"

namespace service_manager {

namespace {

std::unique_ptr<base::MessagePump> CreateDefaultMessagePump() {
  return base::WrapUnique(new base::MessagePumpDefault);
}

class MojoMessageLoop : public base::MessageLoop {
 public:
  MojoMessageLoop()
      : base::MessageLoop(base::MessageLoop::TYPE_CUSTOM,
                          base::Bind(&CreateDefaultMessagePump)) {}
  ~MojoMessageLoop() override {}

  void BindToCurrentThread() { base::MessageLoop::BindToCurrentThread(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoMessageLoop);
};

}  // namespace

// Manages the thread to startup mojo.
class BackgroundServiceManager::MojoThread : public base::SimpleThread {
 public:
  explicit MojoThread(
      std::unique_ptr<BackgroundServiceManager::InitParams> init_params)
      : SimpleThread("background-service-manager"),
        init_params_(std::move(init_params)) {}
  ~MojoThread() override {}

  void CreateServiceRequest(base::WaitableEvent* signal,
                            const std::string& name,
                            mojom::ServiceRequest* request) {
    // Only valid to call this on the background thread.
    DCHECK(message_loop_->task_runner()->BelongsToCurrentThread());
    *request = context_->service_manager()->StartEmbedderService(name);
    signal->Signal();
  }

  void Connect(std::unique_ptr<ConnectParams> params) {
    context_->service_manager()->Connect(std::move(params));
  }

  base::MessageLoop* message_loop() { return message_loop_; }

  // Stops the background thread.
  void Stop() {
    DCHECK_NE(message_loop_, base::MessageLoop::current());
    message_loop_->task_runner()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
    Join();
  }

  void RunServiceManagerCallback(
      const BackgroundServiceManager::ServiceManagerThreadCallback& callback) {
    DCHECK(message_loop_->task_runner()->BelongsToCurrentThread());
    callback.Run(context_->service_manager());
  }

  // base::SimpleThread:
  void Start() override {
    DCHECK(!message_loop_);
    message_loop_ = new MojoMessageLoop;
    base::SimpleThread::Start();
  }
  void Run() override {
    // The construction/destruction order is very finicky and has to be done
    // in the order here.
    std::unique_ptr<base::MessageLoop> message_loop(message_loop_);

    std::unique_ptr<Context::InitParams> context_init_params(
        new Context::InitParams);
    if (init_params_) {
      context_init_params->catalog_store =
          std::move(init_params_->catalog_store);
      context_init_params->native_runner_delegate =
          init_params_->native_runner_delegate;
      context_init_params->init_edk = init_params_->init_edk;
    }
    if (context_init_params->init_edk)
      Context::EnsureEmbedderIsInitialized();

    message_loop_->BindToCurrentThread();

    std::unique_ptr<Context> context(new Context);
    context_ = context.get();
    context_->Init(std::move(context_init_params));

    base::RunLoop().Run();

    // Has to happen after run, but while messageloop still valid.
    context_->Shutdown();

    // Context has to be destroyed after the MessageLoop has been destroyed.
    message_loop.reset();
    context_ = nullptr;
  }

 private:
  // We own this. It's created on the main thread, but destroyed on the
  // background thread.
  MojoMessageLoop* message_loop_ = nullptr;
  // Created in Run() on the background thread.
  Context* context_ = nullptr;

  std::unique_ptr<BackgroundServiceManager::InitParams> init_params_;

  DISALLOW_COPY_AND_ASSIGN(MojoThread);
};

BackgroundServiceManager::InitParams::InitParams() {}
BackgroundServiceManager::InitParams::~InitParams() {}

BackgroundServiceManager::BackgroundServiceManager() {}

BackgroundServiceManager::~BackgroundServiceManager() {
  thread_->Stop();
}

void BackgroundServiceManager::Init(std::unique_ptr<InitParams> init_params) {
  DCHECK(!thread_);
  thread_.reset(new MojoThread(std::move(init_params)));
  thread_->Start();
}

mojom::ServiceRequest BackgroundServiceManager::CreateServiceRequest(
    const std::string& name) {
  std::unique_ptr<ConnectParams> params(new ConnectParams);
  params->set_source(CreateServiceManagerIdentity());
  params->set_target(Identity(name, mojom::kRootUserID));
  mojom::ServiceRequest request;
  base::WaitableEvent signal(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread_->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&MojoThread::CreateServiceRequest,
                 base::Unretained(thread_.get()), &signal, name, &request));
  signal.Wait();
  thread_->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&MojoThread::Connect, base::Unretained(thread_.get()),
                 base::Passed(&params)));
  return request;
}

void BackgroundServiceManager::ExecuteOnServiceManagerThread(
    const ServiceManagerThreadCallback& callback) {
  thread_->message_loop()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&MojoThread::RunServiceManagerCallback,
                            base::Unretained(thread_.get()), callback));
}

}  // namespace service_manager
