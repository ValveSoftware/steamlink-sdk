// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/background_sync/background_sync_provider.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_local.h"
#include "content/child/background_sync/background_sync_type_converters.h"
#include "content/child/child_thread_impl.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncError.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncRegistration.h"

using base::LazyInstance;
using base::ThreadLocalPointer;

namespace content {
namespace {

// Returns the id of the given |service_worker_registration|, which
// is only available on the implementation of the interface.
int64_t GetServiceWorkerRegistrationId(
    blink::WebServiceWorkerRegistration* service_worker_registration) {
  return static_cast<WebServiceWorkerRegistrationImpl*>(
             service_worker_registration)->registration_id();
}

void ConnectToServiceOnMainThread(
    blink::mojom::BackgroundSyncServiceRequest request) {
  DCHECK(ChildThreadImpl::current());
  ChildThreadImpl::current()->GetRemoteInterfaces()->GetInterface(
      std::move(request));
}

LazyInstance<ThreadLocalPointer<BackgroundSyncProvider>>::Leaky
    g_sync_provider_tls = LAZY_INSTANCE_INITIALIZER;

}  // namespace

BackgroundSyncProvider::BackgroundSyncProvider(
    const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : main_thread_task_runner_(main_task_runner) {
  DCHECK(main_task_runner);
  g_sync_provider_tls.Pointer()->Set(this);
}

BackgroundSyncProvider::~BackgroundSyncProvider() {
  g_sync_provider_tls.Pointer()->Set(nullptr);
}

// static
BackgroundSyncProvider*
BackgroundSyncProvider::GetOrCreateThreadSpecificInstance(
    base::SingleThreadTaskRunner* main_thread_task_runner) {
  DCHECK(main_thread_task_runner);
  if (g_sync_provider_tls.Pointer()->Get())
    return g_sync_provider_tls.Pointer()->Get();

  bool have_worker_id = (WorkerThread::GetCurrentId() > 0);
  if (!main_thread_task_runner->BelongsToCurrentThread() && !have_worker_id) {
    // On a worker thread, this could happen if this method is called
    // very late (say by a garbage collected blink::mojom::SyncRegistration).
    return nullptr;
  }

  BackgroundSyncProvider* instance =
      new BackgroundSyncProvider(main_thread_task_runner);

  if (have_worker_id) {
    // For worker threads, use the observer interface to clean up when workers
    // are stopped.
    WorkerThread::AddObserver(instance);
  }

  return instance;
}

void BackgroundSyncProvider::registerBackgroundSync(
    const blink::WebSyncRegistration* options,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebSyncRegistrationCallbacks* callbacks) {
  DCHECK(options);
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  int64_t service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  std::unique_ptr<const blink::WebSyncRegistration> optionsPtr(options);
  std::unique_ptr<blink::WebSyncRegistrationCallbacks> callbacksPtr(callbacks);

  // base::Unretained is safe here, as the mojo channel will be deleted (and
  // will wipe its callbacks) before 'this' is deleted.
  GetBackgroundSyncServicePtr()->Register(
      mojo::ConvertTo<blink::mojom::SyncRegistrationPtr>(*(optionsPtr.get())),
      service_worker_registration_id,
      base::Bind(&BackgroundSyncProvider::RegisterCallback,
                 base::Unretained(this),
                 base::Passed(std::move(callbacksPtr))));
}

void BackgroundSyncProvider::getRegistrations(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebSyncGetRegistrationsCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  int64_t service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  std::unique_ptr<blink::WebSyncGetRegistrationsCallbacks> callbacksPtr(
      callbacks);

  // base::Unretained is safe here, as the mojo channel will be deleted (and
  // will wipe its callbacks) before 'this' is deleted.
  GetBackgroundSyncServicePtr()->GetRegistrations(
      service_worker_registration_id,
      base::Bind(&BackgroundSyncProvider::GetRegistrationsCallback,
                 base::Unretained(this),
                 base::Passed(std::move(callbacksPtr))));
}

void BackgroundSyncProvider::WillStopCurrentWorkerThread() {
  delete this;
}

void BackgroundSyncProvider::RegisterCallback(
    std::unique_ptr<blink::WebSyncRegistrationCallbacks> callbacks,
    blink::mojom::BackgroundSyncError error,
    blink::mojom::SyncRegistrationPtr options) {
  // TODO(iclelland): Determine the correct error message to return in each case
  std::unique_ptr<blink::WebSyncRegistration> result;
  switch (error) {
    case blink::mojom::BackgroundSyncError::NONE:
      if (!options.is_null())
        result = mojo::ConvertTo<std::unique_ptr<blink::WebSyncRegistration>>(
            options);
      callbacks->onSuccess(std::move(result));
      break;
    case blink::mojom::BackgroundSyncError::NOT_FOUND:
      NOTREACHED();
      break;
    case blink::mojom::BackgroundSyncError::STORAGE:
      callbacks->onError(
          blink::WebSyncError(blink::WebSyncError::ErrorTypeUnknown,
                              "Background Sync is disabled."));
      break;
    case blink::mojom::BackgroundSyncError::NOT_ALLOWED:
      callbacks->onError(
          blink::WebSyncError(blink::WebSyncError::ErrorTypeNoPermission,
                              "Attempted to register a sync event without a "
                              "window or registration tag too long."));
      break;
    case blink::mojom::BackgroundSyncError::PERMISSION_DENIED:
      callbacks->onError(
          blink::WebSyncError(blink::WebSyncError::ErrorTypePermissionDenied,
                              "Permission denied."));
      break;
    case blink::mojom::BackgroundSyncError::NO_SERVICE_WORKER:
      callbacks->onError(
          blink::WebSyncError(blink::WebSyncError::ErrorTypeUnknown,
                              "No service worker is active."));
      break;
  }
}

void BackgroundSyncProvider::GetRegistrationsCallback(
    std::unique_ptr<blink::WebSyncGetRegistrationsCallbacks> callbacks,
    blink::mojom::BackgroundSyncError error,
    mojo::Array<blink::mojom::SyncRegistrationPtr> registrations) {
  // TODO(iclelland): Determine the correct error message to return in each case
  switch (error) {
    case blink::mojom::BackgroundSyncError::NONE: {
      blink::WebVector<blink::WebSyncRegistration*> results(
          registrations.size());
      for (size_t i = 0; i < registrations.size(); ++i) {
        results[i] =
            mojo::ConvertTo<std::unique_ptr<blink::WebSyncRegistration>>(
                registrations[i])
                .release();
      }
      callbacks->onSuccess(results);
      break;
    }
    case blink::mojom::BackgroundSyncError::NOT_FOUND:
    case blink::mojom::BackgroundSyncError::NOT_ALLOWED:
    case blink::mojom::BackgroundSyncError::PERMISSION_DENIED:
      // These errors should never be returned from
      // BackgroundSyncManager::GetRegistrations
      NOTREACHED();
      break;
    case blink::mojom::BackgroundSyncError::STORAGE:
      callbacks->onError(
          blink::WebSyncError(blink::WebSyncError::ErrorTypeUnknown,
                              "Background Sync is disabled."));
      break;
    case blink::mojom::BackgroundSyncError::NO_SERVICE_WORKER:
      callbacks->onError(
          blink::WebSyncError(blink::WebSyncError::ErrorTypeUnknown,
                              "No service worker is active."));
      break;
  }
}

blink::mojom::BackgroundSyncServicePtr&
BackgroundSyncProvider::GetBackgroundSyncServicePtr() {
  if (!background_sync_service_.get()) {
    mojo::InterfaceRequest<blink::mojom::BackgroundSyncService> request =
        mojo::GetProxy(&background_sync_service_);
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ConnectToServiceOnMainThread, base::Passed(&request)));
  }
  return background_sync_service_;
}

}  // namespace content
