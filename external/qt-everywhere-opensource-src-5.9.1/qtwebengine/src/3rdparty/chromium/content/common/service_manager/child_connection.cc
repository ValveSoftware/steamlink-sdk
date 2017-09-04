// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_manager/child_connection.h"

#include <stdint.h>
#include <utility>

#include "base/macros.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/interfaces/service.mojom.h"

namespace content {

namespace {

void CallBinderOnTaskRunner(
    const service_manager::InterfaceRegistry::Binder& binder,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle request_handle) {
  task_runner->PostTask(
      FROM_HERE,
      base::Bind(binder, interface_name, base::Passed(&request_handle)));
}

}  // namespace

class ChildConnection::IOThreadContext
    : public base::RefCountedThreadSafe<IOThreadContext> {
 public:
  IOThreadContext() {}

  void Initialize(const service_manager::Identity& child_identity,
                  service_manager::Connector* connector,
                  mojo::ScopedMessagePipeHandle service_pipe,
                  scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
    DCHECK(!io_task_runner_);
    io_task_runner_ = io_task_runner;
    std::unique_ptr<service_manager::Connector> io_thread_connector;
    if (connector)
      io_thread_connector = connector->Clone();
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&IOThreadContext::InitializeOnIOThread, this,
                   child_identity,
                   base::Passed(&io_thread_connector),
                   base::Passed(&service_pipe)));
  }

  void ShutDown() {
    if (!io_task_runner_)
      return;
    bool posted = io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&IOThreadContext::ShutDownOnIOThread, this));
    DCHECK(posted);
  }

  void GetRemoteInterfaceOnIOThread(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle request_handle) {
    if (connection_) {
      connection_->GetRemoteInterfaces()->GetInterface(
          interface_name, std::move(request_handle));
    }
  }

  void SetProcessHandle(base::ProcessHandle handle) {
    DCHECK(io_task_runner_);
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&IOThreadContext::SetProcessHandleOnIOThread, this, handle));
  }

 private:
  friend class base::RefCountedThreadSafe<IOThreadContext>;

  virtual ~IOThreadContext() {}

  void InitializeOnIOThread(
      const service_manager::Identity& child_identity,
      std::unique_ptr<service_manager::Connector> connector,
      mojo::ScopedMessagePipeHandle service_pipe) {
    service_manager::mojom::ServicePtr service;
    service.Bind(mojo::InterfacePtrInfo<service_manager::mojom::Service>(
        std::move(service_pipe), 0u));
    service_manager::mojom::PIDReceiverRequest pid_receiver_request =
        mojo::GetProxy(&pid_receiver_);

    service_manager::Connector::ConnectParams params(child_identity);
    params.set_client_process_connection(std::move(service),
                                         std::move(pid_receiver_request));

    // In some unit testing scenarios a null connector is passed.
    if (connector)
      connection_ = connector->Connect(&params);
  }

  void ShutDownOnIOThread() {
    connection_.reset();
    pid_receiver_.reset();
  }

  void SetProcessHandleOnIOThread(base::ProcessHandle handle) {
    DCHECK(pid_receiver_.is_bound());
    pid_receiver_->SetPID(base::GetProcId(handle));
    pid_receiver_.reset();
  }

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  std::unique_ptr<service_manager::Connection> connection_;
  service_manager::mojom::PIDReceiverPtr pid_receiver_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadContext);
};

ChildConnection::ChildConnection(
    const std::string& service_name,
    const std::string& instance_id,
    const std::string& child_token,
    service_manager::Connector* connector,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : child_token_(child_token),
      context_(new IOThreadContext),
      child_identity_(service_name,
                      service_manager::mojom::kInheritUserID,
                      instance_id),
      service_token_(mojo::edk::GenerateRandomToken()),
      weak_factory_(this) {
  mojo::ScopedMessagePipeHandle service_pipe =
      mojo::edk::CreateParentMessagePipe(service_token_, child_token_);

  context_->Initialize(child_identity_, connector, std::move(service_pipe),
                       io_task_runner);
  remote_interfaces_.Forward(
      base::Bind(&CallBinderOnTaskRunner,
                 base::Bind(&IOThreadContext::GetRemoteInterfaceOnIOThread,
                            context_), io_task_runner));
}

ChildConnection::~ChildConnection() {
  context_->ShutDown();

  if (process_handle_ == base::kNullProcessHandle) {
    // The process handle was never set, so we have to assume the process was
    // not successfully launched. Note that ChildProcessLauncher may also call
    // call ChildProcessLaunchFailed for the same token, so this is (harmlessly)
    // redundant in some cases.
    mojo::edk::ChildProcessLaunchFailed(child_token_);
  }
}

void ChildConnection::SetProcessHandle(base::ProcessHandle handle) {
  process_handle_ = handle;
  context_->SetProcessHandle(handle);
}

}  // namespace content
