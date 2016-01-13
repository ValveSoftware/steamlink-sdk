// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HANDLE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HANDLE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"

namespace IPC {
class Sender;
}

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerRegistration;

// Roughly corresponds to one ServiceWorker object in the renderer process
// (WebServiceWorkerImpl).
// Has references to the corresponding ServiceWorkerVersion and
// ServiceWorkerRegistration (therefore they're guaranteed to be alive while
// this handle is around).
class CONTENT_EXPORT ServiceWorkerHandle
    : public ServiceWorkerVersion::Listener {
 public:
  // Creates a handle for a live version. The version's corresponding
  // registration must be also alive.
  // This may return NULL if |context|.get() or |version| is NULL.
  // |sender| and |thread_id| will be used to send messages to the
  // corresponding WebServiceWorkerImpl (which should live on |thread_id|
  // in the child process).
  static scoped_ptr<ServiceWorkerHandle> Create(
      base::WeakPtr<ServiceWorkerContextCore> context,
      IPC::Sender* sender,
      int thread_id,
      ServiceWorkerVersion* version);

  ServiceWorkerHandle(base::WeakPtr<ServiceWorkerContextCore> context,
                      IPC::Sender* sender,
                      int thread_id,
                      ServiceWorkerRegistration* registration,
                      ServiceWorkerVersion* version);
  virtual ~ServiceWorkerHandle();

  // ServiceWorkerVersion::Listener overrides.
  virtual void OnWorkerStarted(ServiceWorkerVersion* version) OVERRIDE;
  virtual void OnWorkerStopped(ServiceWorkerVersion* version) OVERRIDE;
  virtual void OnErrorReported(ServiceWorkerVersion* version,
                               const base::string16& error_message,
                               int line_number,
                               int column_number,
                               const GURL& source_url) OVERRIDE;
  virtual void OnReportConsoleMessage(ServiceWorkerVersion* version,
                                      int source_identifier,
                                      int message_level,
                                      const base::string16& message,
                                      int line_number,
                                      const GURL& source_url) OVERRIDE;
  virtual void OnVersionStateChanged(ServiceWorkerVersion* version) OVERRIDE;

  ServiceWorkerObjectInfo GetObjectInfo();

  ServiceWorkerRegistration* registration() { return registration_.get(); }
  ServiceWorkerVersion* version() { return version_.get(); }
  int handle_id() const { return handle_id_; }

  bool HasNoRefCount() const { return ref_count_ <= 0; }
  void IncrementRefCount();
  void DecrementRefCount();

 private:
  base::WeakPtr<ServiceWorkerContextCore> context_;
  IPC::Sender* sender_;  // Not owned, it should always outlive this.
  const int thread_id_;
  const int handle_id_;
  int ref_count_;  // Created with 1.
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerHandle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HANDLE_H_
