// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_IMPL_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"

namespace content {

class ServiceWorkerProviderContext;

// A unique provider_id is generated for each instance.
// Instantiated prior to the main resource load being started and remains
// allocated until after the last subresource load has occurred.
// This is used to track the lifetime of a Document to create
// and dispose the ServiceWorkerProviderHost in the browser process
// to match its lifetime. Each request coming from the Document is
// tagged with this id in willSendRequest.
//
// Basically, it's a scoped integer that sends an ipc upon construction
// and destruction.
class CONTENT_EXPORT ServiceWorkerNetworkProvider
    : public base::SupportsUserData::Data {
 public:
  // Ownership is transferred to the DocumentState.
  static void AttachToDocumentState(
      base::SupportsUserData* document_state,
      scoped_ptr<ServiceWorkerNetworkProvider> network_provider);

  static ServiceWorkerNetworkProvider* FromDocumentState(
      base::SupportsUserData* document_state);

  ServiceWorkerNetworkProvider();
  virtual ~ServiceWorkerNetworkProvider();

  int provider_id() const { return provider_id_; }
  ServiceWorkerProviderContext* context() { return context_.get(); }

  // This method is called for a provider that's associated with a
  // running service worker script. The version_id indicates which
  // ServiceWorkerVersion should be used.
  void SetServiceWorkerVersionId(int64 version_id);

 private:
  const int provider_id_;
  scoped_refptr<ServiceWorkerProviderContext> context_;
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerNetworkProvider);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_PROVIDER_IMPL_H_
