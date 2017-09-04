// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HANDLE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HANDLE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"

namespace content {

class ServiceWorkerContextCore;

// Roughly corresponds to one WebServiceWorker object in the renderer process.
//
// The renderer process maintains the reference count by owning a
// ServiceWorkerHandleReference for each reference it has to the service worker
// object. ServiceWorkerHandleReference creation and destruction sends an IPC to
// the browser process, which adjusts the ServiceWorkerHandle refcount.
//
// Has references to the corresponding ServiceWorkerVersion in order to ensure
// that the version is alive while this handle is around.
class CONTENT_EXPORT ServiceWorkerHandle
    : NON_EXPORTED_BASE(public ServiceWorkerVersion::Listener) {
 public:
  // Creates a handle for a live version. This may return nullptr if any of
  // |context|, |provider_host| and |version| is nullptr.
  static std::unique_ptr<ServiceWorkerHandle> Create(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      ServiceWorkerVersion* version);

  ServiceWorkerHandle(base::WeakPtr<ServiceWorkerContextCore> context,
                      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
                      ServiceWorkerVersion* version);
  ~ServiceWorkerHandle() override;

  // ServiceWorkerVersion::Listener overrides.
  void OnVersionStateChanged(ServiceWorkerVersion* version) override;

  ServiceWorkerObjectInfo GetObjectInfo();

  int provider_id() const { return provider_id_; }
  int handle_id() const { return handle_id_; }
  ServiceWorkerVersion* version() { return version_.get(); }

  int ref_count() const { return ref_count_; }
  bool HasNoRefCount() const { return ref_count_ <= 0; }
  void IncrementRefCount();
  void DecrementRefCount();

 private:
  base::WeakPtr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  const int provider_id_;
  const int handle_id_;
  int ref_count_;  // Created with 1.
  scoped_refptr<ServiceWorkerVersion> version_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerHandle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HANDLE_H_
