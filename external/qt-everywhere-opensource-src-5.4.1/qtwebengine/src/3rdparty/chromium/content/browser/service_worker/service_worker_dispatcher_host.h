// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_

#include "base/id_map.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/public/browser/browser_message_filter.h"

class GURL;
struct EmbeddedWorkerHostMsg_ReportConsoleMessage_Params;

namespace content {

class MessagePortMessageFilter;
class ServiceWorkerContextCore;
class ServiceWorkerContextWrapper;
class ServiceWorkerHandle;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;

class CONTENT_EXPORT ServiceWorkerDispatcherHost : public BrowserMessageFilter {
 public:
  ServiceWorkerDispatcherHost(
      int render_process_id,
      MessagePortMessageFilter* message_port_message_filter);

  void Init(ServiceWorkerContextWrapper* context_wrapper);

  // BrowserMessageFilter implementation
  virtual void OnFilterAdded(IPC::Sender* sender) OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC::Sender implementation

  // Send() queues the message until the underlying sender is ready.  This
  // class assumes that Send() can only fail after that when the renderer
  // process has terminated, at which point the whole instance will eventually
  // be destroyed.
  virtual bool Send(IPC::Message* message) OVERRIDE;

  void RegisterServiceWorkerHandle(scoped_ptr<ServiceWorkerHandle> handle);

  MessagePortMessageFilter* message_port_message_filter() {
    return message_port_message_filter_;
  }

 protected:
  virtual ~ServiceWorkerDispatcherHost();

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<ServiceWorkerDispatcherHost>;
  friend class TestingServiceWorkerDispatcherHost;

  // IPC Message handlers
  void OnRegisterServiceWorker(int thread_id,
                               int request_id,
                               int provider_id,
                               const GURL& pattern,
                               const GURL& script_url);
  void OnUnregisterServiceWorker(int thread_id,
                                 int request_id,
                                 int provider_id,
                                 const GURL& pattern);
  void OnProviderCreated(int provider_id);
  void OnProviderDestroyed(int provider_id);
  void OnSetHostedVersionId(int provider_id, int64 version_id);
  void OnWorkerScriptLoaded(int embedded_worker_id);
  void OnWorkerScriptLoadFailed(int embedded_worker_id);
  void OnWorkerStarted(int thread_id,
                       int embedded_worker_id);
  void OnWorkerStopped(int embedded_worker_id);
  void OnReportException(int embedded_worker_id,
                         const base::string16& error_message,
                         int line_number,
                         int column_number,
                         const GURL& source_url);
  void OnReportConsoleMessage(
      int embedded_worker_id,
      const EmbeddedWorkerHostMsg_ReportConsoleMessage_Params& params);
  void OnPostMessage(int handle_id,
                     const base::string16& message,
                     const std::vector<int>& sent_message_port_ids);
  void OnIncrementServiceWorkerRefCount(int handle_id);
  void OnDecrementServiceWorkerRefCount(int handle_id);
  void OnPostMessageToWorker(int handle_id,
                             const base::string16& message,
                             const std::vector<int>& sent_message_port_ids);
  void OnServiceWorkerObjectDestroyed(int handle_id);

  // Callbacks from ServiceWorkerContextCore
  void RegistrationComplete(int thread_id,
                            int request_id,
                            ServiceWorkerStatusCode status,
                            int64 registration_id,
                            int64 version_id);

  void UnregistrationComplete(int thread_id,
                              int request_id,
                              ServiceWorkerStatusCode status);

  void SendRegistrationError(int thread_id,
                             int request_id,
                             ServiceWorkerStatusCode status);

  ServiceWorkerContextCore* GetContext();

  int render_process_id_;
  MessagePortMessageFilter* const message_port_message_filter_;
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;

  IDMap<ServiceWorkerHandle, IDMapOwnPointer> handles_;

  bool channel_ready_;  // True after BrowserMessageFilter::sender_ != NULL.
  ScopedVector<IPC::Message> pending_messages_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_
