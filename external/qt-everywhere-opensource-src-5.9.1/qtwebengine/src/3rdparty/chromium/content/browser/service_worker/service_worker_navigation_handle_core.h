// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_HANDLE_CORE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_HANDLE_CORE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace content {

class ServiceWorkerContextWrapper;
class ServiceWorkerNavigationHandle;
class ServiceWorkerProviderHost;

// PlzNavigate
// This class is used to manage the lifetime of ServiceWorkerProviderHosts
// created during navigations. This class is created on the UI thread, but
// should only be accessed from the IO thread afterwards. It is the IO thread
// pendant of ServiceWorkerNavigationHandle. See the
// ServiceWorkerNavigationHandle header for more details about the lifetime of
// both classes.
class ServiceWorkerNavigationHandleCore {
 public:
  ServiceWorkerNavigationHandleCore(
      base::WeakPtr<ServiceWorkerNavigationHandle> ui_handle,
      ServiceWorkerContextWrapper* context_wrapper);
  ~ServiceWorkerNavigationHandleCore();

  // Called when a ServiceWorkerProviderHost was pre-created for the navigation
  // tracked by this ServiceWorkerNavigationHandleCore. Takes ownership of
  // |precreated_host|.
  void DidPreCreateProviderHost(
      std::unique_ptr<ServiceWorkerProviderHost> precreated_host);

  // Called when the renderer created a ServiceWorkerNetworkProvider matching
  // |precreated_host_|. This releases ownership of |precreated_host_|.
  std::unique_ptr<ServiceWorkerProviderHost> RetrievePreCreatedHost();

  ServiceWorkerContextWrapper* context_wrapper() const {
    return context_wrapper_.get();
  }

 private:
  std::unique_ptr<ServiceWorkerProviderHost> precreated_host_;
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;
  base::WeakPtr<ServiceWorkerNavigationHandle> ui_handle_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerNavigationHandleCore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_HANDLE_CORE_H_
