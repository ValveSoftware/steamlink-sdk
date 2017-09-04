// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_HANDLE_REFERENCE_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_HANDLE_REFERENCE_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/service_worker/service_worker_types.h"
#include "url/gurl.h"

namespace content {

class ThreadSafeSender;

// Represents an interprocess reference to ServiceWorkerRegistrationHandle
// managed in the browser process. The constructor and destructor sends a
// message to increment or decrement the reference count to the browser
// process.
class ServiceWorkerRegistrationHandleReference {
 public:
  // Creates a new ServiceWorkerRegistrationHandleReference and increments
  // ref-count.
  static std::unique_ptr<ServiceWorkerRegistrationHandleReference> Create(
      const ServiceWorkerRegistrationObjectInfo& info,
      ThreadSafeSender* sender);

  // Creates a new ServiceWorkerRegistrationHandleReference by adopting a
  // ref-count.
  static std::unique_ptr<ServiceWorkerRegistrationHandleReference> Adopt(
      const ServiceWorkerRegistrationObjectInfo& info,
      ThreadSafeSender* sender);

  ~ServiceWorkerRegistrationHandleReference();

  const ServiceWorkerRegistrationObjectInfo& info() const { return info_; }
  int handle_id() const { return info_.handle_id; }
  GURL scope() const { return info_.scope; }
  int64_t registration_id() const { return info_.registration_id; }

 private:
  ServiceWorkerRegistrationHandleReference(
      const ServiceWorkerRegistrationObjectInfo& info,
      ThreadSafeSender* sender,
      bool increment_ref_in_ctor);

  ServiceWorkerRegistrationObjectInfo info_;
  scoped_refptr<ThreadSafeSender> sender_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegistrationHandleReference);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_HANDLE_REFERENCE_H_
