// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"

namespace blink {
class WebDataSource;
class WebLocalFrame;
}  // namespace blink

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
      std::unique_ptr<ServiceWorkerNetworkProvider> network_provider);

  static ServiceWorkerNetworkProvider* FromDocumentState(
      base::SupportsUserData* document_state);

  static std::unique_ptr<ServiceWorkerNetworkProvider> CreateForNavigation(
      int route_id,
      blink::WebLocalFrame* frame,
      bool content_initiated);

  ServiceWorkerNetworkProvider(int route_id,
                               ServiceWorkerProviderType type,
                               bool is_parent_frame_secure);
  ServiceWorkerNetworkProvider();
  ~ServiceWorkerNetworkProvider() override;

  int provider_id() const { return provider_id_; }
  ServiceWorkerProviderContext* context() const { return context_.get(); }

  // This method is called for a provider that's associated with a
  // running service worker script. The version_id indicates which
  // ServiceWorkerVersion should be used.
  void SetServiceWorkerVersionId(int64_t version_id, int embedded_worker_id);

  bool IsControlledByServiceWorker() const;

 private:
  const int provider_id_;
  scoped_refptr<ServiceWorkerProviderContext> context_;
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerNetworkProvider);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_H_
