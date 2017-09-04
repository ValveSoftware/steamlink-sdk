// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_manager/embedded_service_runner.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace content {

class EmbeddedServiceRunner::InstanceManager
    : public base::RefCountedThreadSafe<InstanceManager> {
 public:
  InstanceManager(const base::StringPiece& name,
                  const ServiceInfo& info,
                  const base::Closure& quit_closure)
      : name_(name.as_string()),
        factory_callback_(info.factory),
        use_own_thread_(!info.task_runner && info.use_own_thread),
        quit_closure_(quit_closure),
        quit_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        service_task_runner_(info.task_runner) {
    if (!use_own_thread_ && !service_task_runner_)
      service_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  }

  void BindServiceRequest(service_manager::mojom::ServiceRequest request) {
    DCHECK(runner_thread_checker_.CalledOnValidThread());

    if (use_own_thread_ && !thread_) {
      // Start a new thread if necessary.
      thread_.reset(new base::Thread(name_));
      thread_->Start();
      service_task_runner_ = thread_->task_runner();
    }

    DCHECK(service_task_runner_);
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&InstanceManager::BindServiceRequestOnServiceThread,
                   this, base::Passed(&request)));
  }

  void ShutDown() {
    DCHECK(runner_thread_checker_.CalledOnValidThread());
    if (!service_task_runner_)
      return;
    // Any extant ServiceContexts must be destroyed on the application thread.
    if (service_task_runner_->BelongsToCurrentThread()) {
      QuitOnServiceThread();
    } else {
      service_task_runner_->PostTask(
          FROM_HERE, base::Bind(&InstanceManager::QuitOnServiceThread, this));
    }
  }

 private:
  friend class base::RefCountedThreadSafe<InstanceManager>;

  ~InstanceManager() {
    // If this instance had its own thread, it MUST be explicitly destroyed by
    // QuitOnRunnerThread() by the time this destructor is run.
    DCHECK(!thread_);
  }

  void BindServiceRequestOnServiceThread(
      service_manager::mojom::ServiceRequest request) {
    DCHECK(service_task_runner_->BelongsToCurrentThread());

    int instance_id = next_instance_id_++;

    std::unique_ptr<service_manager::ServiceContext> context =
        base::MakeUnique<service_manager::ServiceContext>(
            factory_callback_.Run(), std::move(request));

    service_manager::ServiceContext* raw_context = context.get();
    context->SetConnectionLostClosure(
        base::Bind(&InstanceManager::OnInstanceLost, this, instance_id));
    contexts_.insert(std::make_pair(raw_context, std::move(context)));
    id_to_context_map_.insert(std::make_pair(instance_id, raw_context));
  }

  void OnInstanceLost(int instance_id) {
    DCHECK(service_task_runner_->BelongsToCurrentThread());

    auto id_iter = id_to_context_map_.find(instance_id);
    CHECK(id_iter != id_to_context_map_.end());

    auto context_iter = contexts_.find(id_iter->second);
    CHECK(context_iter != contexts_.end());
    contexts_.erase(context_iter);
    id_to_context_map_.erase(id_iter);

    // If we've lost the last instance, run the quit closure.
    if (contexts_.empty())
      QuitOnServiceThread();
  }

  void QuitOnServiceThread() {
    DCHECK(service_task_runner_->BelongsToCurrentThread());

    contexts_.clear();
    if (quit_task_runner_->BelongsToCurrentThread()) {
      QuitOnRunnerThread();
    } else {
      quit_task_runner_->PostTask(
          FROM_HERE, base::Bind(&InstanceManager::QuitOnRunnerThread, this));
    }
  }

  void QuitOnRunnerThread() {
    DCHECK(runner_thread_checker_.CalledOnValidThread());
    if (thread_) {
      thread_.reset();
      service_task_runner_ = nullptr;
    }
    quit_closure_.Run();
  }

  const std::string name_;
  const ServiceInfo::ServiceFactory factory_callback_;
  const bool use_own_thread_;
  const base::Closure quit_closure_;
  const scoped_refptr<base::SingleThreadTaskRunner> quit_task_runner_;

  // Thread checker used to ensure certain operations happen only on the
  // runner's (i.e. our owner's) thread.
  base::ThreadChecker runner_thread_checker_;

  // These fields must only be accessed from the runner's thread.
  std::unique_ptr<base::Thread> thread_;
  scoped_refptr<base::SingleThreadTaskRunner> service_task_runner_;

  // These fields must only be accessed from the service thread, except in
  // the destructor which may run on either the runner thread or the service
  // thread.

  // A map which owns all existing Service instances for this service.
  using ServiceContextMap =
      std::map<service_manager::ServiceContext*,
               std::unique_ptr<service_manager::ServiceContext>>;
  ServiceContextMap contexts_;

  int next_instance_id_ = 0;

  // A mapping from instance ID to (not owned) ServiceContext.
  //
  // TODO(rockot): Remove this once we get rid of the quit closure argument to
  // service factory functions.
  std::map<int, service_manager::ServiceContext*> id_to_context_map_;

  DISALLOW_COPY_AND_ASSIGN(InstanceManager);
};

EmbeddedServiceRunner::EmbeddedServiceRunner(const base::StringPiece& name,
                                             const ServiceInfo& info)
    : weak_factory_(this) {
  instance_manager_ = new InstanceManager(
      name, info, base::Bind(&EmbeddedServiceRunner::OnQuit,
                             weak_factory_.GetWeakPtr()));
}

EmbeddedServiceRunner::~EmbeddedServiceRunner() {
  instance_manager_->ShutDown();
}

void EmbeddedServiceRunner::BindServiceRequest(
    service_manager::mojom::ServiceRequest request) {
  instance_manager_->BindServiceRequest(std::move(request));
}

void EmbeddedServiceRunner::SetQuitClosure(
    const base::Closure& quit_closure) {
  quit_closure_ = quit_closure;
}

void EmbeddedServiceRunner::OnQuit() {
  if (!quit_closure_.is_null())
    quit_closure_.Run();
}

}  // namespace content
