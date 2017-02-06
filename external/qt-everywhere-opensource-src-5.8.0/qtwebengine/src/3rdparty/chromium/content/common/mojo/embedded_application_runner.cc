// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mojo/embedded_application_runner.h"

#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/shell/public/cpp/shell_connection.h"

namespace content {

class EmbeddedApplicationRunner::Instance
    : public base::RefCountedThreadSafe<Instance> {
 public:
  Instance(const base::StringPiece& name,
           const MojoApplicationInfo& info,
           const base::Closure& quit_closure)
      : name_(name.as_string()),
        factory_callback_(info.application_factory),
        use_own_thread_(!info.application_task_runner && info.use_own_thread),
        quit_closure_(quit_closure),
        quit_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        application_task_runner_(info.application_task_runner) {
    if (!use_own_thread_ && !application_task_runner_)
      application_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  }

  void BindShellClientRequest(shell::mojom::ShellClientRequest request) {
    DCHECK(runner_thread_checker_.CalledOnValidThread());

    if (use_own_thread_ && !thread_) {
      // Start a new thread if necessary.
      thread_.reset(new base::Thread(name_));
      thread_->Start();
      application_task_runner_ = thread_->task_runner();
    }

    DCHECK(application_task_runner_);
    application_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Instance::BindShellClientRequestOnApplicationThread, this,
                   base::Passed(&request)));
  }

  void ShutDown() {
    DCHECK(runner_thread_checker_.CalledOnValidThread());
    if (thread_) {
      application_task_runner_ = nullptr;
      thread_.reset();
    }
  }

 private:
  void BindShellClientRequestOnApplicationThread(
      shell::mojom::ShellClientRequest request) {
    DCHECK(application_task_runner_->BelongsToCurrentThread());

    if (!shell_client_) {
      shell_client_ = factory_callback_.Run(
          base::Bind(&Instance::Quit, base::Unretained(this)));
    }

    shell::ShellConnection* new_connection =
        new shell::ShellConnection(shell_client_.get(), std::move(request));
    shell_connections_.push_back(base::WrapUnique(new_connection));
    new_connection->SetConnectionLostClosure(
        base::Bind(&Instance::OnShellConnectionLost, base::Unretained(this),
                   new_connection));
  }

 private:
  friend class base::RefCountedThreadSafe<Instance>;

  ~Instance() {
    // If this instance had its own thread, it MUST be explicitly destroyed by
    // ShutDown() on the runner's thread by the time this destructor is run.
    DCHECK(!thread_);
  }

  void OnShellConnectionLost(shell::ShellConnection* connection) {
    DCHECK(application_task_runner_->BelongsToCurrentThread());

    for (auto it = shell_connections_.begin(); it != shell_connections_.end();
         ++it) {
      if (it->get() == connection) {
        shell_connections_.erase(it);
        break;
      }
    }
  }

  void Quit() {
    DCHECK(application_task_runner_->BelongsToCurrentThread());

    shell_connections_.clear();
    shell_client_.reset();
    quit_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Instance::QuitOnRunnerThread, this));
  }

  void QuitOnRunnerThread() {
    DCHECK(runner_thread_checker_.CalledOnValidThread());
    ShutDown();
    quit_closure_.Run();
  }

  const std::string name_;
  const MojoApplicationInfo::ApplicationFactory factory_callback_;
  const bool use_own_thread_;
  const base::Closure quit_closure_;
  const scoped_refptr<base::SingleThreadTaskRunner> quit_task_runner_;

  // Thread checker used to ensure certain operations happen only on the
  // runner's (i.e. our owner's) thread.
  base::ThreadChecker runner_thread_checker_;

  // These fields must only be accessed from the runner's thread.
  std::unique_ptr<base::Thread> thread_;
  scoped_refptr<base::SingleThreadTaskRunner> application_task_runner_;

  // These fields must only be accessed from the application thread, except in
  // the destructor which may run on either the runner thread or the application
  // thread.
  std::unique_ptr<shell::ShellClient> shell_client_;
  std::vector<std::unique_ptr<shell::ShellConnection>> shell_connections_;

  DISALLOW_COPY_AND_ASSIGN(Instance);
};

EmbeddedApplicationRunner::EmbeddedApplicationRunner(
    const base::StringPiece& name,
    const MojoApplicationInfo& info)
    : weak_factory_(this) {
  instance_ = new Instance(name, info,
                           base::Bind(&EmbeddedApplicationRunner::OnQuit,
                                      weak_factory_.GetWeakPtr()));
}

EmbeddedApplicationRunner::~EmbeddedApplicationRunner() {
  instance_->ShutDown();
}

void EmbeddedApplicationRunner::BindShellClientRequest(
    shell::mojom::ShellClientRequest request) {
  instance_->BindShellClientRequest(std::move(request));
}

void EmbeddedApplicationRunner::SetQuitClosure(
    const base::Closure& quit_closure) {
  quit_closure_ = quit_closure;
}

void EmbeddedApplicationRunner::OnQuit() {
  if (!quit_closure_.is_null())
    quit_closure_.Run();
}

}  // namespace content
