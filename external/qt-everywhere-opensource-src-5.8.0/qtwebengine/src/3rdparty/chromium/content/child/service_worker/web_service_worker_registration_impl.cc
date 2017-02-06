// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/web_service_worker_registration_impl.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_registration_handle_reference.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/service_worker/web_service_worker_provider_impl.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRegistrationProxy.h"

namespace content {

namespace {

class HandleImpl : public blink::WebServiceWorkerRegistration::Handle {
 public:
  explicit HandleImpl(
      const scoped_refptr<WebServiceWorkerRegistrationImpl>& registration)
      : registration_(registration) {}
  ~HandleImpl() override {}

  blink::WebServiceWorkerRegistration* registration() override {
    return registration_.get();
  }

 private:
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration_;

  DISALLOW_COPY_AND_ASSIGN(HandleImpl);
};

}  // namespace

WebServiceWorkerRegistrationImpl::QueuedTask::QueuedTask(
    QueuedTaskType type,
    const scoped_refptr<WebServiceWorkerImpl>& worker)
    : type(type), worker(worker) {}

WebServiceWorkerRegistrationImpl::QueuedTask::QueuedTask(
    const QueuedTask& other) = default;

WebServiceWorkerRegistrationImpl::QueuedTask::~QueuedTask() {}

WebServiceWorkerRegistrationImpl::WebServiceWorkerRegistrationImpl(
    std::unique_ptr<ServiceWorkerRegistrationHandleReference> handle_ref)
    : handle_ref_(std::move(handle_ref)), proxy_(nullptr) {
  DCHECK(handle_ref_);
  DCHECK_NE(kInvalidServiceWorkerRegistrationHandleId,
            handle_ref_->handle_id());
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  dispatcher->AddServiceWorkerRegistration(handle_ref_->handle_id(), this);
}

void WebServiceWorkerRegistrationImpl::SetInstalling(
    const scoped_refptr<WebServiceWorkerImpl>& service_worker) {
  if (proxy_)
    proxy_->setInstalling(WebServiceWorkerImpl::CreateHandle(service_worker));
  else
    queued_tasks_.push_back(QueuedTask(INSTALLING, service_worker));
}

void WebServiceWorkerRegistrationImpl::SetWaiting(
    const scoped_refptr<WebServiceWorkerImpl>& service_worker) {
  if (proxy_)
    proxy_->setWaiting(WebServiceWorkerImpl::CreateHandle(service_worker));
  else
    queued_tasks_.push_back(QueuedTask(WAITING, service_worker));
}

void WebServiceWorkerRegistrationImpl::SetActive(
    const scoped_refptr<WebServiceWorkerImpl>& service_worker) {
  if (proxy_)
    proxy_->setActive(WebServiceWorkerImpl::CreateHandle(service_worker));
  else
    queued_tasks_.push_back(QueuedTask(ACTIVE, service_worker));
}

void WebServiceWorkerRegistrationImpl::OnUpdateFound() {
  if (proxy_)
    proxy_->dispatchUpdateFoundEvent();
  else
    queued_tasks_.push_back(QueuedTask(UPDATE_FOUND, nullptr));
}

void WebServiceWorkerRegistrationImpl::setProxy(
    blink::WebServiceWorkerRegistrationProxy* proxy) {
  proxy_ = proxy;
  RunQueuedTasks();
}

void WebServiceWorkerRegistrationImpl::RunQueuedTasks() {
  DCHECK(proxy_);
  for (const QueuedTask& task : queued_tasks_) {
    if (task.type == INSTALLING)
      proxy_->setInstalling(WebServiceWorkerImpl::CreateHandle(task.worker));
    else if (task.type == WAITING)
      proxy_->setWaiting(WebServiceWorkerImpl::CreateHandle(task.worker));
    else if (task.type == ACTIVE)
      proxy_->setActive(WebServiceWorkerImpl::CreateHandle(task.worker));
    else if (task.type == UPDATE_FOUND)
      proxy_->dispatchUpdateFoundEvent();
  }
  queued_tasks_.clear();
}

blink::WebServiceWorkerRegistrationProxy*
WebServiceWorkerRegistrationImpl::proxy() {
  return proxy_;
}

blink::WebURL WebServiceWorkerRegistrationImpl::scope() const {
  return handle_ref_->scope();
}

void WebServiceWorkerRegistrationImpl::update(
    blink::WebServiceWorkerProvider* provider,
    WebServiceWorkerUpdateCallbacks* callbacks) {
  WebServiceWorkerProviderImpl* provider_impl =
      static_cast<WebServiceWorkerProviderImpl*>(provider);
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  dispatcher->UpdateServiceWorker(provider_impl->provider_id(),
                                  registration_id(), callbacks);
}

void WebServiceWorkerRegistrationImpl::unregister(
    blink::WebServiceWorkerProvider* provider,
    WebServiceWorkerUnregistrationCallbacks* callbacks) {
  WebServiceWorkerProviderImpl* provider_impl =
      static_cast<WebServiceWorkerProviderImpl*>(provider);
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  dispatcher->UnregisterServiceWorker(provider_impl->provider_id(),
                                      registration_id(), callbacks);
}

int64_t WebServiceWorkerRegistrationImpl::registration_id() const {
  return handle_ref_->registration_id();
}

// static
std::unique_ptr<blink::WebServiceWorkerRegistration::Handle>
WebServiceWorkerRegistrationImpl::CreateHandle(
    const scoped_refptr<WebServiceWorkerRegistrationImpl>& registration) {
  if (!registration)
    return nullptr;
  return base::WrapUnique(new HandleImpl(registration));
}

blink::WebServiceWorkerRegistration::Handle*
WebServiceWorkerRegistrationImpl::CreateLeakyHandle(
    const scoped_refptr<WebServiceWorkerRegistrationImpl>& registration) {
  if (!registration)
    return nullptr;
  return new HandleImpl(registration);
}

WebServiceWorkerRegistrationImpl::~WebServiceWorkerRegistrationImpl() {
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  if (dispatcher)
    dispatcher->RemoveServiceWorkerRegistration(handle_ref_->handle_id());
}

}  // namespace content
