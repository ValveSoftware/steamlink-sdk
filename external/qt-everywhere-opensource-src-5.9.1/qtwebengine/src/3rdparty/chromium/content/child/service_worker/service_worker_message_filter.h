// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_MESSAGE_FILTER_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_MESSAGE_FILTER_H_

#include <vector>

#include "base/macros.h"
#include "content/child/worker_thread_message_filter.h"
#include "content/common/content_export.h"

struct ServiceWorkerMsg_MessageToDocument_Params;

namespace content {

struct ServiceWorkerObjectInfo;
struct ServiceWorkerRegistrationObjectInfo;
struct ServiceWorkerVersionAttributes;

class CONTENT_EXPORT ServiceWorkerMessageFilter
    : public NON_EXPORTED_BASE(WorkerThreadMessageFilter) {
 public:
  explicit ServiceWorkerMessageFilter(ThreadSafeSender* thread_safe_sender);

 protected:
  ~ServiceWorkerMessageFilter() override;

 private:
  // WorkerThreadMessageFilter:
  bool ShouldHandleMessage(const IPC::Message& msg) const override;
  void OnFilteredMessageReceived(const IPC::Message& msg) override;
  bool GetWorkerThreadIdForMessage(const IPC::Message& msg,
                                   int* ipc_thread_id) override;

  // ChildMessageFilter:
  void OnStaleMessageReceived(const IPC::Message& msg) override;

  // Message handlers for stale messages.
  void OnStaleAssociateRegistration(
      int thread_id,
      int provider_id,
      const ServiceWorkerRegistrationObjectInfo& info,
      const ServiceWorkerVersionAttributes& attrs);
  void OnStaleGetRegistration(int thread_id,
                              int request_id,
                              const ServiceWorkerRegistrationObjectInfo& info,
                              const ServiceWorkerVersionAttributes& attrs);
  void OnStaleGetRegistrations(
      int thread_id,
      int request_id,
      const std::vector<ServiceWorkerRegistrationObjectInfo>& info,
      const std::vector<ServiceWorkerVersionAttributes>& attrs);
  void OnStaleSetVersionAttributes(
      int thread_id,
      int registration_handle_id,
      int changed_mask,
      const ServiceWorkerVersionAttributes& attrs);
  void OnStaleSetControllerServiceWorker(
      int thread_id,
      int provider_id,
      const ServiceWorkerObjectInfo& info,
      bool should_notify_controllerchange);
  void OnStaleMessageToDocument(
      const ServiceWorkerMsg_MessageToDocument_Params& params);

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerMessageFilter);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_MESSAGE_FILTER_H_
