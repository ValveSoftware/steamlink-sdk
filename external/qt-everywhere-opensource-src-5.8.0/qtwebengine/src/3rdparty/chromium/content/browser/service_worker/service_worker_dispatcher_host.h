// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_message_filter.h"

class GURL;
struct EmbeddedWorkerHostMsg_ReportConsoleMessage_Params;

namespace url {
class Origin;
}

namespace content {

class MessagePortMessageFilter;
class ResourceContext;
class ServiceWorkerContextCore;
class ServiceWorkerContextWrapper;
class ServiceWorkerHandle;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
class ServiceWorkerRegistrationHandle;
class ServiceWorkerVersion;
struct ServiceWorkerObjectInfo;
struct ServiceWorkerRegistrationInfo;
struct ServiceWorkerRegistrationObjectInfo;
struct ServiceWorkerVersionAttributes;

class CONTENT_EXPORT ServiceWorkerDispatcherHost : public BrowserMessageFilter {
 public:
  ServiceWorkerDispatcherHost(
      int render_process_id,
      MessagePortMessageFilter* message_port_message_filter,
      ResourceContext* resource_context);

  void Init(ServiceWorkerContextWrapper* context_wrapper);

  // BrowserMessageFilter implementation
  void OnFilterAdded(IPC::Sender* sender) override;
  void OnFilterRemoved() override;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC::Sender implementation

  // Send() queues the message until the underlying sender is ready.  This
  // class assumes that Send() can only fail after that when the renderer
  // process has terminated, at which point the whole instance will eventually
  // be destroyed.
  bool Send(IPC::Message* message) override;

  void RegisterServiceWorkerHandle(std::unique_ptr<ServiceWorkerHandle> handle);
  void RegisterServiceWorkerRegistrationHandle(
      std::unique_ptr<ServiceWorkerRegistrationHandle> handle);

  ServiceWorkerHandle* FindServiceWorkerHandle(int provider_id,
                                               int64_t version_id);

  // Returns the existing registration handle whose reference count is
  // incremented or a newly created one if it doesn't exist.
  ServiceWorkerRegistrationHandle* GetOrCreateRegistrationHandle(
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      ServiceWorkerRegistration* registration);

  MessagePortMessageFilter* message_port_message_filter() {
    return message_port_message_filter_;
  }

 protected:
  ~ServiceWorkerDispatcherHost() override;

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<ServiceWorkerDispatcherHost>;
  friend class ServiceWorkerDispatcherHostTest;
  friend class TestingServiceWorkerDispatcherHost;

  using StatusCallback = base::Callback<void(ServiceWorkerStatusCode status)>;

  // IPC Message handlers
  void OnRegisterServiceWorker(int thread_id,
                               int request_id,
                               int provider_id,
                               const GURL& pattern,
                               const GURL& script_url);
  void OnUpdateServiceWorker(int thread_id,
                             int request_id,
                             int provider_id,
                             int64_t registration_id);
  void OnUnregisterServiceWorker(int thread_id,
                                 int request_id,
                                 int provider_id,
                                 int64_t registration_id);
  void OnGetRegistration(int thread_id,
                         int request_id,
                         int provider_id,
                         const GURL& document_url);
  void OnGetRegistrations(int thread_id, int request_id, int provider_id);
  void OnGetRegistrationForReady(int thread_id,
                                 int request_id,
                                 int provider_id);
  void OnProviderCreated(int provider_id,
                         int route_id,
                         ServiceWorkerProviderType provider_type,
                         bool is_parent_frame_secure);
  void OnProviderDestroyed(int provider_id);
  void OnSetHostedVersionId(int provider_id,
                            int64_t version_id,
                            int embedded_worker_id);
  void OnWorkerReadyForInspection(int embedded_worker_id);
  void OnWorkerScriptLoaded(int embedded_worker_id);
  void OnWorkerThreadStarted(int embedded_worker_id,
                             int thread_id,
                             int provider_id);
  void OnWorkerScriptLoadFailed(int embedded_worker_id);
  void OnWorkerScriptEvaluated(int embedded_worker_id, bool success);
  void OnWorkerStarted(int embedded_worker_id);
  void OnWorkerStopped(int embedded_worker_id);
  void OnReportException(int embedded_worker_id,
                         const base::string16& error_message,
                         int line_number,
                         int column_number,
                         const GURL& source_url);
  void OnReportConsoleMessage(
      int embedded_worker_id,
      const EmbeddedWorkerHostMsg_ReportConsoleMessage_Params& params);
  void OnIncrementServiceWorkerRefCount(int handle_id);
  void OnDecrementServiceWorkerRefCount(int handle_id);
  void OnIncrementRegistrationRefCount(int registration_handle_id);
  void OnDecrementRegistrationRefCount(int registration_handle_id);
  void OnPostMessageToWorker(int handle_id,
                             int provider_id,
                             const base::string16& message,
                             const url::Origin& source_origin,
                             const std::vector<int>& sent_message_ports);

  void OnTerminateWorker(int handle_id);

  void DispatchExtendableMessageEvent(
      scoped_refptr<ServiceWorkerVersion> worker,
      const base::string16& message,
      const url::Origin& source_origin,
      const std::vector<int>& sent_message_ports,
      ServiceWorkerProviderHost* sender_provider_host,
      const StatusCallback& callback);
  template <typename SourceInfo>
  void DispatchExtendableMessageEventInternal(
      scoped_refptr<ServiceWorkerVersion> worker,
      const base::string16& message,
      const url::Origin& source_origin,
      const std::vector<int>& sent_message_ports,
      const StatusCallback& callback,
      const SourceInfo& source_info);
  void DispatchExtendableMessageEventAfterStartWorker(
      scoped_refptr<ServiceWorkerVersion> worker,
      const base::string16& message,
      const url::Origin& source_origin,
      const std::vector<int>& sent_message_ports,
      const ExtendableMessageEventSource& source,
      const StatusCallback& callback);
  template <typename SourceInfo>
  void DidFailToDispatchExtendableMessageEvent(
      const std::vector<int>& sent_message_ports,
      const SourceInfo& source_info,
      const StatusCallback& callback,
      ServiceWorkerStatusCode status);
  void ReleaseSourceInfo(const ServiceWorkerClientInfo& source_info);
  void ReleaseSourceInfo(const ServiceWorkerObjectInfo& source_info);

  ServiceWorkerRegistrationHandle* FindRegistrationHandle(
      int provider_id,
      int64_t registration_handle_id);

  void GetRegistrationObjectInfoAndVersionAttributes(
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      ServiceWorkerRegistration* registration,
      ServiceWorkerRegistrationObjectInfo* info,
      ServiceWorkerVersionAttributes* attrs);

  // Callbacks from ServiceWorkerContextCore
  void RegistrationComplete(int thread_id,
                            int provider_id,
                            int request_id,
                            ServiceWorkerStatusCode status,
                            const std::string& status_message,
                            int64_t registration_id);
  void UpdateComplete(int thread_id,
                      int provider_id,
                      int request_id,
                      ServiceWorkerStatusCode status,
                      const std::string& status_message,
                      int64_t registration_id);
  void UnregistrationComplete(int thread_id,
                              int request_id,
                              ServiceWorkerStatusCode status);
  void GetRegistrationComplete(
      int thread_id,
      int provider_id,
      int request_id,
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);
  void GetRegistrationsComplete(
      int thread_id,
      int provider_id,
      int request_id,
      ServiceWorkerStatusCode status,
      const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
          registrations);
  void GetRegistrationForReadyComplete(
      int thread_id,
      int request_id,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      ServiceWorkerRegistration* registration);

  ServiceWorkerContextCore* GetContext();

  const int render_process_id_;
  MessagePortMessageFilter* const message_port_message_filter_;
  ResourceContext* resource_context_;
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;

  IDMap<ServiceWorkerHandle, IDMapOwnPointer> handles_;

  using RegistrationHandleMap =
      IDMap<ServiceWorkerRegistrationHandle, IDMapOwnPointer>;
  RegistrationHandleMap registration_handles_;

  bool channel_ready_;  // True after BrowserMessageFilter::sender_ != NULL.
  std::vector<std::unique_ptr<IPC::Message>> pending_messages_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_
