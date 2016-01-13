// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_HANDLE_REFERENCE_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_HANDLE_REFERENCE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/service_worker/service_worker_types.h"

namespace content {

class ThreadSafeSender;

// Automatically increments and decrements ServiceWorkerHandle's ref-count
// (in the browser side) in ctor and dtor.
class ServiceWorkerHandleReference {
 public:
  // Creates a new ServiceWorkerHandleReference and increments ref-count.
  static scoped_ptr<ServiceWorkerHandleReference> Create(
      const ServiceWorkerObjectInfo& info,
      ThreadSafeSender* sender);

  // Creates a new ServiceWorkerHandleReference by adopting a
  // ref-count. ServiceWorkerHandleReferences created this way must
  // have a matching
  // ServiceWorkerDispatcherHost::RegisterServiceWorkerHandle call on
  // the browser side.
  static scoped_ptr<ServiceWorkerHandleReference> Adopt(
      const ServiceWorkerObjectInfo& info,
      ThreadSafeSender* sender);

  ~ServiceWorkerHandleReference();

  const ServiceWorkerObjectInfo& info() const { return info_; }
  int handle_id() const { return info_.handle_id; }
  const GURL& scope() const { return info_.scope; }
  const GURL& url() const { return info_.url; }
  blink::WebServiceWorkerState state() const { return info_.state; }
  void set_state(blink::WebServiceWorkerState state) { info_.state = state; }

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
