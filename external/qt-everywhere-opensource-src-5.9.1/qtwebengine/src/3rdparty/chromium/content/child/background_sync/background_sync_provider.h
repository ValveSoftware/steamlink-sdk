// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BACKGROUND_SYNC_BACKGROUND_SYNC_PROVIDER_H_
#define CONTENT_CHILD_BACKGROUND_SYNC_BACKGROUND_SYNC_PROVIDER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "content/public/child/worker_thread.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncProvider.h"
#include "third_party/WebKit/public/platform/modules/background_sync/background_sync.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

// The BackgroundSyncProvider is called by the SyncManager and SyncRegistration
// objects and communicates with the BackgroundSyncManager object in the browser
// process. Each thread will have its own instance (e.g. main thread, worker
// threads), instantiated as needed by BlinkPlatformImpl.  Each instance of
// the provider creates a new mojo connection to a new
// BackgroundSyncManagerImpl, which then talks to the BackgroundSyncManager
// object.
class BackgroundSyncProvider : public blink::WebSyncProvider,
                               public WorkerThread::Observer {
 public:
  // Constructor made public to allow BlinkPlatformImpl to own a copy for the
  // main thread. Everyone else should use GetOrCreateThreadSpecificInstance().
  explicit BackgroundSyncProvider(
      const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);

  ~BackgroundSyncProvider() override;

  // Returns thread-specific instance (if exists).  Otherwise, a new instance
  // will be created for the thread, except when called for a worker thread that
  // has already been stopped.
  static BackgroundSyncProvider* GetOrCreateThreadSpecificInstance(
      base::SingleThreadTaskRunner* main_thread_task_runner);

  // blink::WebSyncProvider implementation
  void registerBackgroundSync(
      const blink::WebSyncRegistration* options,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebSyncRegistrationCallbacks* callbacks) override;
  void getRegistrations(
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebSyncGetRegistrationsCallbacks* callbacks) override;

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

 private:
  // Callback handlers
  void RegisterCallback(
      std::unique_ptr<blink::WebSyncRegistrationCallbacks> callbacks,
      blink::mojom::BackgroundSyncError error,
      blink::mojom::SyncRegistrationPtr options);
  void GetRegistrationsCallback(
      std::unique_ptr<blink::WebSyncGetRegistrationsCallbacks> callbacks,
      blink::mojom::BackgroundSyncError error,
      mojo::Array<blink::mojom::SyncRegistrationPtr> registrations);

  // Helper method that returns an initialized BackgroundSyncServicePtr.
  blink::mojom::BackgroundSyncServicePtr& GetBackgroundSyncServicePtr();

  blink::mojom::BackgroundSyncServicePtr background_sync_service_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncProvider);
};

}  // namespace content

#endif  // CONTENT_CHILD_BACKGROUND_SYNC_BACKGROUND_SYNC_PROVIDER_H_
