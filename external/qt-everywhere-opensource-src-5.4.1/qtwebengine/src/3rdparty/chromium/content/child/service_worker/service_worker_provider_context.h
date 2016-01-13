// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_

#include <set>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/synchronization/lock.h"
#include "content/common/service_worker/service_worker_types.h"

namespace base {
class MessageLoopProxy;
}

namespace IPC {
class Message;
}

namespace content {

class ServiceWorkerHandleReference;
struct ServiceWorkerProviderContextDeleter;
class ThreadSafeSender;

// An instance of this class holds document-related information (e.g.
// .current). Created and destructed on the main thread.
// TODO(kinuko): To support navigator.serviceWorker in dedicated workers
// this needs to be RefCountedThreadSafe and .current info needs to be
// handled in a thread-safe manner (e.g. by a lock etc).
class ServiceWorkerProviderContext
    : public base::RefCounted<ServiceWorkerProviderContext> {
 public:
  explicit ServiceWorkerProviderContext(int provider_id);

  // Called from ServiceWorkerDispatcher.
  void OnServiceWorkerStateChanged(int handle_id,
                                   blink::WebServiceWorkerState state);
  void OnSetWaitingServiceWorker(int provider_id,
                                 const ServiceWorkerObjectInfo& info);
  void OnSetCurrentServiceWorker(int provider_id,
                                 const ServiceWorkerObjectInfo& info);

  int provider_id() const { return provider_id_; }

  ServiceWorkerHandleReference* waiting();
  // Gets the context's handle reference for .controller.
  // TODO(dominicc): Rename this to "controller".
  ServiceWorkerHandleReference* current();

  // Gets the handle ID of the controller, or
  // kInvalidServiceWorkerHandleId if the provider is not controlled
  // by a Service Worker.
  int current_handle_id() const;

  // Gets the handle ID of the waiting Service Worker, or
  // kInvalidServiceWorkerHandleId if the provider does not have a
  // waiting Service Worker.
  int waiting_handle_id() const;

 private:
  friend class base::RefCounted<ServiceWorkerProviderContext>;
  ~ServiceWorkerProviderContext();

  const int provider_id_;
  scoped_refptr<base::MessageLoopProxy> main_thread_loop_proxy_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_ptr<ServiceWorkerHandleReference> waiting_;
  scoped_ptr<ServiceWorkerHandleReference> current_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderContext);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_
