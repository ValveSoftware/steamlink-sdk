// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_HANDLE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_HANDLE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace content {

class ServiceWorkerContextWrapper;
class ServiceWorkerNavigationHandleCore;

// This class is used to manage the lifetime of ServiceWorkerProviderHosts
// created during navigation. This is a UI thread class, with a pendant class
// on the IO thread, the ServiceWorkerNavigationHandleCore.
//
// The lifetime of the ServiceWorkerNavigationHandle, the
// ServiceWorkerNavigationHandleCore and the ServiceWorkerProviderHost are the
// following :
//   1) We create a ServiceWorkerNavigationHandle on the UI thread with a
//   service worker provider id of -1. This also leads to the creation of a
//   ServiceWorkerNavigationHandleCore with an id of -1.
//
//   2) When the navigation request is sent to the IO thread, we include a
//   pointer to the ServiceWorkerNavigationHandleCore.
//
//   3) If we pre-create a ServiceWorkerProviderHost for this navigation, its
//   ownershipped is passed to the ServiceWorkerNavigationHandleCore. The
//   ServiceWorkerNavigationHandleCore id is updated.
//
//   4) The ServiceWorkerNavigationHandleCore informs the
//   ServiceWorkerNavigationHandle on the UI that the service worker provider
//   id was updated.
//
//   5) When the navigation is ready to commit, the NavigationRequest will
//   update the RequestNavigationParams based on the id from the
//   ServiceWorkerNavigationHandle.
//
//   6) If the commit leads to the creation of a ServiceWorkerNetworkProvider
//   in the renderer, a ServiceWorkerHostMsg_ProviderCreated will be received
//   in the browser. The ServiceWorkerDispatcherHost will retrieve the
//   ServiceWorkerProviderHost from the ServiceWorkerNavigationHandleCore and
//   put it in the ServiceWorkerContextCore map of ServiceWorkerProviderHosts.
//
//   7) When the navigation finishes, the ServiceWorkerNavigationHandle is
//   destroyed. The destructor of the ServiceWorkerNavigationHandle posts a
//   task to destroy the ServiceWorkerNavigationHandleCore on the IO thread.
//   This in turn leads to the destruction of an unclaimed
//   ServiceWorkerProviderHost.
class ServiceWorkerNavigationHandle {
 public:
  explicit ServiceWorkerNavigationHandle(
      ServiceWorkerContextWrapper* context_wrapper);
  ~ServiceWorkerNavigationHandle();

  int service_worker_provider_host_id() const {
    return service_worker_provider_host_id_;
  }
  ServiceWorkerNavigationHandleCore* core() const { return core_; }

  // Called after a ServiceWorkerProviderHost with id
  // |service_worker_provider_host_id| was pre-created for the navigation on the
  // IO thread.
  void DidCreateServiceWorkerProviderHost(int service_worker_provider_host_id);

 private:
  int service_worker_provider_host_id_;
  ServiceWorkerNavigationHandleCore* core_;
  base::WeakPtrFactory<ServiceWorkerNavigationHandle> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerNavigationHandle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_HANDLE_H_
