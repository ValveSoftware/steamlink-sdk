// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_handle_reference.h"

#include "base/memory/ptr_util.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_messages.h"

namespace content {

std::unique_ptr<ServiceWorkerHandleReference>
ServiceWorkerHandleReference::Create(const ServiceWorkerObjectInfo& info,
                                     ThreadSafeSender* sender) {
  DCHECK(sender);
  if (info.handle_id == kInvalidServiceWorkerHandleId)
    return nullptr;
  return base::WrapUnique(new ServiceWorkerHandleReference(info, sender, true));
}

std::unique_ptr<ServiceWorkerHandleReference>
ServiceWorkerHandleReference::Adopt(const ServiceWorkerObjectInfo& info,
                                    ThreadSafeSender* sender) {
  DCHECK(sender);
  if (info.handle_id == kInvalidServiceWorkerHandleId)
    return nullptr;
  return base::WrapUnique(
      new ServiceWorkerHandleReference(info, sender, false));
}

ServiceWorkerHandleReference::ServiceWorkerHandleReference(
    const ServiceWorkerObjectInfo& info,
    ThreadSafeSender* sender,
    bool increment_ref_in_ctor)
    : info_(info),
      sender_(sender) {
  DCHECK_NE(info.handle_id, kInvalidServiceWorkerHandleId);
  if (increment_ref_in_ctor) {
    sender_->Send(
        new ServiceWorkerHostMsg_IncrementServiceWorkerRefCount(
            info_.handle_id));
  }
}

ServiceWorkerHandleReference::~ServiceWorkerHandleReference() {
  DCHECK_NE(info_.handle_id, kInvalidServiceWorkerHandleId);
  sender_->Send(
      new ServiceWorkerHostMsg_DecrementServiceWorkerRefCount(info_.handle_id));
}

}  // namespace content
