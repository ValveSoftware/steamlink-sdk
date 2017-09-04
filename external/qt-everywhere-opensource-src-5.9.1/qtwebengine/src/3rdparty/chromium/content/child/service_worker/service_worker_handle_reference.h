// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_HANDLE_REFERENCE_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_HANDLE_REFERENCE_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"

namespace content {

class ThreadSafeSender;

// Represents an interprocess reference to ServiceWorkerHandle managed in the
// browser process. The constructor and destructor sends a message to increment
// or decrement the reference count to the browser process.
class CONTENT_EXPORT ServiceWorkerHandleReference {
 public:
  // Creates a new ServiceWorkerHandleReference and increments ref-count. If
  // the handle id is kInvalidServiceWorkerHandleId, returns null instead.
  static std::unique_ptr<ServiceWorkerHandleReference> Create(
      const ServiceWorkerObjectInfo& info,
      ThreadSafeSender* sender);

  // Creates a new ServiceWorkerHandleReference by adopting a ref-count. If
  // the handle id is kInvalidServiceWorkerHandleId, returns null instead.
  static std::unique_ptr<ServiceWorkerHandleReference> Adopt(
      const ServiceWorkerObjectInfo& info,
      ThreadSafeSender* sender);

  ~ServiceWorkerHandleReference();

  const ServiceWorkerObjectInfo& info() const { return info_; }
  int handle_id() const { return info_.handle_id; }
  const GURL& url() const { return info_.url; }
  blink::WebServiceWorkerState state() const { return info_.state; }
  int64_t version_id() const { return info_.version_id; }

 private:
  ServiceWorkerHandleReference(const ServiceWorkerObjectInfo& info,
                               ThreadSafeSender* sender,
                               bool increment_ref_in_ctor);
  ServiceWorkerObjectInfo info_;
  scoped_refptr<ThreadSafeSender> sender_;
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerHandleReference);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_HANDLE_REFERENCE_H_
