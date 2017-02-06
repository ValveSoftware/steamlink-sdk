// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_provider_context.h"

#include <utility>

#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/child/child_thread_impl.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_registration_handle_reference.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_thread_registry.h"

namespace content {

class ServiceWorkerProviderContext::Delegate {
 public:
  virtual ~Delegate() {}
  virtual void AssociateRegistration(
      std::unique_ptr<ServiceWorkerRegistrationHandleReference> registration,
      std::unique_ptr<ServiceWorkerHandleReference> installing,
      std::unique_ptr<ServiceWorkerHandleReference> waiting,
      std::unique_ptr<ServiceWorkerHandleReference> active) = 0;
  virtual void DisassociateRegistration() = 0;
  virtual void GetAssociatedRegistration(
      ServiceWorkerRegistrationObjectInfo* info,
      ServiceWorkerVersionAttributes* attrs) = 0;
  virtual bool HasAssociatedRegistration() = 0;
  virtual void SetController(
      std::unique_ptr<ServiceWorkerHandleReference> controller) = 0;
  virtual ServiceWorkerHandleReference* controller() = 0;
};

// Delegate class for ServiceWorker client (Document, SharedWorker, etc) to
// keep the associated registration and the controller until
// ServiceWorkerContainer is initialized.
class ServiceWorkerProviderContext::ControlleeDelegate
    : public ServiceWorkerProviderContext::Delegate {
 public:
  ControlleeDelegate() {}
  ~ControlleeDelegate() override {}

  void AssociateRegistration(
      std::unique_ptr<ServiceWorkerRegistrationHandleReference> registration,
      std::unique_ptr<ServiceWorkerHandleReference> installing,
      std::unique_ptr<ServiceWorkerHandleReference> waiting,
      std::unique_ptr<ServiceWorkerHandleReference> active) override {
    DCHECK(!registration_);
    registration_ = std::move(registration);
  }

  void DisassociateRegistration() override {
    controller_.reset();
    registration_.reset();
  }

  void SetController(
      std::unique_ptr<ServiceWorkerHandleReference> controller) override {
    DCHECK(registration_);
    DCHECK(!controller ||
           controller->handle_id() != kInvalidServiceWorkerHandleId);
    controller_ = std::move(controller);
  }

  bool HasAssociatedRegistration() override { return !!registration_; }

  void GetAssociatedRegistration(
      ServiceWorkerRegistrationObjectInfo* info,
      ServiceWorkerVersionAttributes* attrs) override {
    NOTREACHED();
  }

  ServiceWorkerHandleReference* controller() override {
    return controller_.get();
  }

 private:
  std::unique_ptr<ServiceWorkerRegistrationHandleReference> registration_;
  std::unique_ptr<ServiceWorkerHandleReference> controller_;

  DISALLOW_COPY_AND_ASSIGN(ControlleeDelegate);
};

// Delegate class for ServiceWorkerGlobalScope to keep the associated
// registration and its versions until the execution context is initialized.
class ServiceWorkerProviderContext::ControllerDelegate
    : public ServiceWorkerProviderContext::Delegate {
 public:
  ControllerDelegate() {}
  ~ControllerDelegate() override {}

  void AssociateRegistration(
      std::unique_ptr<ServiceWorkerRegistrationHandleReference> registration,
      std::unique_ptr<ServiceWorkerHandleReference> installing,
      std::unique_ptr<ServiceWorkerHandleReference> waiting,
      std::unique_ptr<ServiceWorkerHandleReference> active) override {
    DCHECK(!registration_);
    registration_ = std::move(registration);
    installing_ = std::move(installing);
    waiting_ = std::move(waiting);
    active_ = std::move(active);
  }

  void DisassociateRegistration() override {
    // ServiceWorkerGlobalScope is never disassociated.
    NOTREACHED();
  }

  void SetController(
      std::unique_ptr<ServiceWorkerHandleReference> controller) override {
    NOTREACHED();
  }

  bool HasAssociatedRegistration() override { return !!registration_; }

  void GetAssociatedRegistration(
      ServiceWorkerRegistrationObjectInfo* info,
      ServiceWorkerVersionAttributes* attrs) override {
    DCHECK(HasAssociatedRegistration());
    *info = registration_->info();
    if (installing_)
      attrs->installing = installing_->info();
    if (waiting_)
      attrs->waiting = waiting_->info();
    if (active_)
      attrs->active = active_->info();
  }

  ServiceWorkerHandleReference* controller() override {
    NOTREACHED();
    return nullptr;
  }

 private:
  std::unique_ptr<ServiceWorkerRegistrationHandleReference> registration_;
  std::unique_ptr<ServiceWorkerHandleReference> installing_;
  std::unique_ptr<ServiceWorkerHandleReference> waiting_;
  std::unique_ptr<ServiceWorkerHandleReference> active_;

  DISALLOW_COPY_AND_ASSIGN(ControllerDelegate);
};

ServiceWorkerProviderContext::ServiceWorkerProviderContext(
    int provider_id,
    ServiceWorkerProviderType provider_type,
    ThreadSafeSender* thread_safe_sender)
    : provider_id_(provider_id),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      thread_safe_sender_(thread_safe_sender) {
  if (provider_type == SERVICE_WORKER_PROVIDER_FOR_CONTROLLER)
    delegate_.reset(new ControllerDelegate);
  else
    delegate_.reset(new ControlleeDelegate);

  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance(
          thread_safe_sender_.get(), main_thread_task_runner_.get());
  dispatcher->AddProviderContext(this);
}

ServiceWorkerProviderContext::~ServiceWorkerProviderContext() {
  if (ServiceWorkerDispatcher* dispatcher =
          ServiceWorkerDispatcher::GetThreadSpecificInstance()) {
    // Remove this context from the dispatcher living on the main thread.
    dispatcher->RemoveProviderContext(this);
  }
}

void ServiceWorkerProviderContext::OnAssociateRegistration(
    std::unique_ptr<ServiceWorkerRegistrationHandleReference> registration,
    std::unique_ptr<ServiceWorkerHandleReference> installing,
    std::unique_ptr<ServiceWorkerHandleReference> waiting,
    std::unique_ptr<ServiceWorkerHandleReference> active) {
  DCHECK(main_thread_task_runner_->RunsTasksOnCurrentThread());
  delegate_->AssociateRegistration(std::move(registration),
                                   std::move(installing), std::move(waiting),
                                   std::move(active));
}

void ServiceWorkerProviderContext::OnDisassociateRegistration() {
  DCHECK(main_thread_task_runner_->RunsTasksOnCurrentThread());
  delegate_->DisassociateRegistration();
}

void ServiceWorkerProviderContext::OnSetControllerServiceWorker(
    std::unique_ptr<ServiceWorkerHandleReference> controller) {
  DCHECK(main_thread_task_runner_->RunsTasksOnCurrentThread());
  delegate_->SetController(std::move(controller));
}

void ServiceWorkerProviderContext::GetAssociatedRegistration(
    ServiceWorkerRegistrationObjectInfo* info,
    ServiceWorkerVersionAttributes* attrs) {
  DCHECK(!main_thread_task_runner_->RunsTasksOnCurrentThread());
  delegate_->GetAssociatedRegistration(info, attrs);
}

bool ServiceWorkerProviderContext::HasAssociatedRegistration() {
  return delegate_->HasAssociatedRegistration();
}

ServiceWorkerHandleReference* ServiceWorkerProviderContext::controller() {
  DCHECK(main_thread_task_runner_->RunsTasksOnCurrentThread());
  return delegate_->controller();
}

void ServiceWorkerProviderContext::DestructOnMainThread() const {
  if (!main_thread_task_runner_->RunsTasksOnCurrentThread() &&
      main_thread_task_runner_->DeleteSoon(FROM_HERE, this)) {
    return;
  }
  delete this;
}

}  // namespace content
