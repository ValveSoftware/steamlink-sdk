// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_

#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

class ServiceWorkerHandleReference;
class ServiceWorkerRegistrationHandleReference;
struct ServiceWorkerProviderContextDeleter;
class ThreadSafeSender;

// An instance of this class holds information related to Document/Worker.
// Created and destructed on the main thread. Unless otherwise noted, all
// methods are called on the main thread. The lifetime of this class is equals
// to the corresponding ServiceWorkerNetworkProvider.
//
// The role of this class varies for controllees and controllers:
//  - For controllees, this is used for keeping the associated registration and
//    the controller alive to create controllee's ServiceWorkerContainer. The
//    references to them are kept until OnDisassociateRegistration() is called
//    or OnSetControllerServiceWorker() is called with an invalid worker info.
//  - For controllers, this is used for keeping the associated registration and
//    its versions alive to create controller's ServiceWorkerGlobalScope. The
//    references to them are kept until OnDisassociateRegistration() is called.
//
// These operations are actually done in delegate classes owned by this class:
// ControlleeDelegate and ControllerDelegate.
class CONTENT_EXPORT ServiceWorkerProviderContext
    : public base::RefCountedThreadSafe<ServiceWorkerProviderContext,
                                        ServiceWorkerProviderContextDeleter> {
 public:
  ServiceWorkerProviderContext(int provider_id,
                               ServiceWorkerProviderType provider_type,
                               ThreadSafeSender* thread_safe_sender);

  // Called from ServiceWorkerDispatcher.
  void OnAssociateRegistration(
      std::unique_ptr<ServiceWorkerRegistrationHandleReference> registration,
      std::unique_ptr<ServiceWorkerHandleReference> installing,
      std::unique_ptr<ServiceWorkerHandleReference> waiting,
      std::unique_ptr<ServiceWorkerHandleReference> active);
  void OnDisassociateRegistration();
  void OnSetControllerServiceWorker(
      std::unique_ptr<ServiceWorkerHandleReference> controller);

  // Called on the worker thread. Used for initializing
  // ServiceWorkerGlobalScope.
  void GetAssociatedRegistration(ServiceWorkerRegistrationObjectInfo* info,
                                 ServiceWorkerVersionAttributes* attrs);

  // May be called on the main or worker thread.
  bool HasAssociatedRegistration();

  int provider_id() const { return provider_id_; }

  ServiceWorkerHandleReference* controller();

 private:
  friend class base::DeleteHelper<ServiceWorkerProviderContext>;
  friend class base::RefCountedThreadSafe<ServiceWorkerProviderContext,
                                          ServiceWorkerProviderContextDeleter>;
  friend struct ServiceWorkerProviderContextDeleter;

  class Delegate;
  class ControlleeDelegate;
  class ControllerDelegate;

  ~ServiceWorkerProviderContext();
  void DestructOnMainThread() const;

  const int provider_id_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  std::unique_ptr<Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderContext);
};

struct ServiceWorkerProviderContextDeleter {
  static void Destruct(const ServiceWorkerProviderContext* context) {
    context->DestructOnMainThread();
  }
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_
