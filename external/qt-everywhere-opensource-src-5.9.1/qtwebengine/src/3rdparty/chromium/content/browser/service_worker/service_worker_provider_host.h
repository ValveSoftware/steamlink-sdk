// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_type.h"

namespace storage {
class BlobStorageContext;
}

namespace content {

class ResourceRequestBodyImpl;
class ServiceWorkerContextCore;
class ServiceWorkerDispatcherHost;
class ServiceWorkerRequestHandler;
class ServiceWorkerVersion;

// This class is the browser-process representation of a service worker
// provider. There are two general types of providers: 1) those for a client
// (windows, dedicated workers, or shared workers), and 2) those for hosting a
// running service worker.
//
// For client providers, there is a provider per document or a worker and the
// lifetime of this object is tied to the lifetime of its document or the worker
// in the renderer process. This class holds service worker state that is scoped
// to an individual document or a worker.
//
// For providers hosting a running service worker, this class will observe
// resource loads made directly by the service worker.
class CONTENT_EXPORT ServiceWorkerProviderHost
    : public NON_EXPORTED_BASE(ServiceWorkerRegistration::Listener),
      public base::SupportsWeakPtr<ServiceWorkerProviderHost> {
 public:
  using GetRegistrationForReadyCallback =
      base::Callback<void(ServiceWorkerRegistration* reigstration)>;

  // PlzNavigate
  // Used to pre-create a ServiceWorkerProviderHost for a navigation. The
  // ServiceWorkerNetworkProvider will later be created in the renderer, should
  // the navigation succeed. |is_parent_frame_is_secure| should be true for main
  // frames. Otherwise it is true iff all ancestor frames of this frame have a
  // secure origin.
  static std::unique_ptr<ServiceWorkerProviderHost> PreCreateNavigationHost(
      base::WeakPtr<ServiceWorkerContextCore> context,
      bool are_ancestors_secure);

  enum class FrameSecurityLevel { UNINITIALIZED, INSECURE, SECURE };

  // When this provider host is for a Service Worker context, |route_id| is
  // MSG_ROUTING_NONE. When this provider host is for a Document,
  // |route_id| is the frame ID of the Document. When this provider host is for
  // a Shared Worker, |route_id| is the Shared Worker route ID.
  // |provider_type| gives additional information whether the provider is
  // created for controller (ServiceWorker) or controllee (Document or
  // SharedWorker).
  ServiceWorkerProviderHost(int render_process_id,
                            int route_id,
                            int provider_id,
                            ServiceWorkerProviderType provider_type,
                            FrameSecurityLevel parent_frame_security_level,
                            base::WeakPtr<ServiceWorkerContextCore> context,
                            ServiceWorkerDispatcherHost* dispatcher_host);
  virtual ~ServiceWorkerProviderHost();

  const std::string& client_uuid() const { return client_uuid_; }
  int process_id() const { return render_process_id_; }
  int provider_id() const { return provider_id_; }
  int frame_id() const;
  int route_id() const { return route_id_; }

  bool is_parent_frame_secure() const {
    return parent_frame_security_level_ == FrameSecurityLevel::SECURE;
  }
  void set_parent_frame_secure(bool is_parent_frame_secure) {
    CHECK_EQ(parent_frame_security_level_, FrameSecurityLevel::UNINITIALIZED);
    parent_frame_security_level_ = is_parent_frame_secure
                                       ? FrameSecurityLevel::SECURE
                                       : FrameSecurityLevel::INSECURE;
  }

  // Returns whether this provider host is secure enough to have a service
  // worker controller.
  // Analogous to Blink's Document::isSecureContext. Because of how service
  // worker intercepts main resource requests, this check must be done
  // browser-side once the URL is known (see comments in
  // ServiceWorkerNetworkProvider::CreateForNavigation). This function uses
  // |document_url_| and |is_parent_frame_secure_| to determine context
  // security, so they must be set properly before calling this function.
  bool IsContextSecureForServiceWorker() const;

  bool IsHostToRunningServiceWorker() {
    return running_hosted_version_.get() != NULL;
  }

  ServiceWorkerVersion* controlling_version() const {
    return controlling_version_.get();
  }
  ServiceWorkerVersion* active_version() const {
    return associated_registration_.get() ?
        associated_registration_->active_version() : NULL;
  }
  ServiceWorkerVersion* waiting_version() const {
    return associated_registration_.get() ?
        associated_registration_->waiting_version() : NULL;
  }
  ServiceWorkerVersion* installing_version() const {
    return associated_registration_.get() ?
        associated_registration_->installing_version() : NULL;
  }

  ServiceWorkerRegistration* associated_registration() const {
    return associated_registration_.get();
  }

  // The running version, if any, that this provider is providing resource
  // loads for.
  ServiceWorkerVersion* running_hosted_version() const {
    return running_hosted_version_.get();
  }

  // Sets the |document_url_|.  When this object is for a client,
  // |matching_registrations_| gets also updated to ensure that |document_url_|
  // is in scope of all |matching_registrations_|.
  void SetDocumentUrl(const GURL& url);
  const GURL& document_url() const { return document_url_; }

  void SetTopmostFrameUrl(const GURL& url);
  const GURL& topmost_frame_url() const { return topmost_frame_url_; }

  ServiceWorkerProviderType provider_type() const { return provider_type_; }
  bool IsProviderForClient() const;
  blink::WebServiceWorkerClientType client_type() const;

  // Associates to |registration| to listen for its version change events and
  // sets the controller. If |notify_controllerchange| is true, instructs the
  // renderer to dispatch a 'controllerchange' event.
  void AssociateRegistration(ServiceWorkerRegistration* registration,
                             bool notify_controllerchange);

  // Clears the associated registration and stop listening to it.
  void DisassociateRegistration();

  void SetHostedVersion(ServiceWorkerVersion* version);

  // Returns a handler for a request, the handler may return NULL if
  // the request doesn't require special handling.
  std::unique_ptr<ServiceWorkerRequestHandler> CreateRequestHandler(
      FetchRequestMode request_mode,
      FetchCredentialsMode credentials_mode,
      FetchRedirectMode redirect_mode,
      ResourceType resource_type,
      RequestContextType request_context_type,
      RequestContextFrameType frame_type,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      scoped_refptr<ResourceRequestBodyImpl> body);

  // Used to get a ServiceWorkerObjectInfo to send to the renderer. Finds an
  // existing ServiceWorkerHandle, and increments its reference count, or else
  // creates a new one (initialized to ref count 1). Returns the
  // ServiceWorkerInfo from the handle. The renderer is expected to use
  // ServiceWorkerHandleReference::Adopt to balance out the ref count.
  ServiceWorkerObjectInfo GetOrCreateServiceWorkerHandle(
      ServiceWorkerVersion* version);

  // Returns true if |registration| can be associated with this provider.
  bool CanAssociateRegistration(ServiceWorkerRegistration* registration);

  // For use by the ServiceWorkerControlleeRequestHandler to disallow
  // new registration association while a navigation is occurring and
  // an existing registration is being looked for.
  void SetAllowAssociation(bool allow) { allow_association_ = allow; }

  // Returns true if the context referred to by this host (i.e. |context_|) is
  // still alive.
  bool IsContextAlive();

  // Dispatches message event to the document.
  void PostMessageToClient(ServiceWorkerVersion* version,
                           const base::string16& message,
                           const std::vector<int>& sent_message_ports);

  // Adds reference of this host's process to the |pattern|, the reference will
  // be removed in destructor.
  void AddScopedProcessReferenceToPattern(const GURL& pattern);

  // |registration| claims the document to be controlled.
  void ClaimedByRegistration(ServiceWorkerRegistration* registration);

  // Called by dispatcher host to get the registration for the "ready" property.
  // Returns false if there's a completed or ongoing request for the document.
  // https://slightlyoff.github.io/ServiceWorker/spec/service_worker/#navigator-service-worker-ready
  bool GetRegistrationForReady(const GetRegistrationForReadyCallback& callback);

  // Methods to support cross site navigations.
  void PrepareForCrossSiteTransfer();
  void CompleteCrossSiteTransfer(
      int new_process_id,
      int new_frame_id,
      int new_provider_id,
      ServiceWorkerProviderType new_provider_type,
      ServiceWorkerDispatcherHost* dispatcher_host);
  ServiceWorkerDispatcherHost* dispatcher_host() const {
    return dispatcher_host_;
  }

  // PlzNavigate
  // Completes initialization of provider hosts used for navigation requests.
  void CompleteNavigationInitialized(
      int process_id,
      int frame_routing_id,
      ServiceWorkerDispatcherHost* dispatcher_host);

  // Sends event messages to the renderer. Events for the worker are queued up
  // until the worker thread id is known via SetReadyToSendMessagesToWorker().
  void SendUpdateFoundMessage(
      int registration_handle_id);
  void SendSetVersionAttributesMessage(
      int registration_handle_id,
      ChangedVersionAttributesMask changed_mask,
      ServiceWorkerVersion* installing_version,
      ServiceWorkerVersion* waiting_version,
      ServiceWorkerVersion* active_version);
  void SendServiceWorkerStateChangedMessage(
      int worker_handle_id,
      blink::WebServiceWorkerState state);

  // Sets the worker thread id and flushes queued events.
  void SetReadyToSendMessagesToWorker(int render_thread_id);

  void AddMatchingRegistration(ServiceWorkerRegistration* registration);
  void RemoveMatchingRegistration(ServiceWorkerRegistration* registration);

  // An optimized implementation of [[Match Service Worker Registration]]
  // for current document.
  ServiceWorkerRegistration* MatchRegistration() const;

  // Called when our controller has been terminated and doomed due to an
  // exceptional condition like it could no longer be read from the script
  // cache.
  void NotifyControllerLost();

 private:
  friend class LinkHeaderServiceWorkerTest;
  friend class ServiceWorkerProviderHostTestP;
  friend class ServiceWorkerWriteToCacheJobTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerWriteToCacheJobTest, Update_SameScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerWriteToCacheJobTest,
                           Update_SameSizeScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerWriteToCacheJobTest,
                           Update_TruncatedScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerWriteToCacheJobTest,
                           Update_ElongatedScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerWriteToCacheJobTest,
                           Update_EmptyScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDispatcherHostTestP,
                           DispatchExtendableMessageEvent);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDispatcherHostTestP,
                           DispatchExtendableMessageEvent_Fail);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextRequestHandlerTestP,
                           UpdateBefore24Hours);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextRequestHandlerTestP,
                           UpdateAfter24Hours);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextRequestHandlerTestP,
                           UpdateForceBypassCache);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextRequestHandlerTestP,
                           ServiceWorkerDataRequestAnnotation);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerProviderHostTestP, ContextSecurity);

  struct OneShotGetReadyCallback {
    GetRegistrationForReadyCallback callback;
    bool called;

    explicit OneShotGetReadyCallback(
        const GetRegistrationForReadyCallback& callback);
    ~OneShotGetReadyCallback();
  };

  // ServiceWorkerRegistration::Listener overrides.
  void OnVersionAttributesChanged(
      ServiceWorkerRegistration* registration,
      ChangedVersionAttributesMask changed_mask,
      const ServiceWorkerRegistrationInfo& info) override;
  void OnRegistrationFailed(ServiceWorkerRegistration* registration) override;
  void OnRegistrationFinishedUninstalling(
      ServiceWorkerRegistration* registration) override;
  void OnSkippedWaiting(ServiceWorkerRegistration* registration) override;

  // Sets the controller version field to |version| or if |version| is NULL,
  // clears the field. If |notify_controllerchange| is true, instructs the
  // renderer to dispatch a 'controller' change event.
  void SetControllerVersionAttribute(ServiceWorkerVersion* version,
                                     bool notify_controllerchange);

  void SendAssociateRegistrationMessage();

  // Syncs matching registrations with live registrations.
  void SyncMatchingRegistrations();

  // Discards all references to matching registrations.
  void RemoveAllMatchingRegistrations();

  // Increase/decrease this host's process reference for |pattern|.
  void IncreaseProcessReference(const GURL& pattern);
  void DecreaseProcessReference(const GURL& pattern);

  void ReturnRegistrationForReadyIfNeeded();

  bool IsReadyToSendMessages() const;
  void Send(IPC::Message* message) const;

  // Finalizes cross-site transfers and navigation-initalized hosts.
  void FinalizeInitialization(int process_id,
                              int frame_routing_id,
                              ServiceWorkerDispatcherHost* dispatcher_host);

  std::string client_uuid_;
  int render_process_id_;

  // See the constructor's documentation.
  int route_id_;

  // For provider hosts that are hosting a running service worker, the id of the
  // service worker thread. Otherwise, |kDocumentMainThreadId|. May be
  // |kInvalidEmbeddedWorkerThreadId| before the hosted service worker starts
  // up, or during cross-site transfers.
  int render_thread_id_;

  // Unique within the renderer process.
  int provider_id_;

  ServiceWorkerProviderType provider_type_;
  FrameSecurityLevel parent_frame_security_level_;
  GURL document_url_;
  GURL topmost_frame_url_;

  std::vector<GURL> associated_patterns_;
  scoped_refptr<ServiceWorkerRegistration> associated_registration_;

  // Keyed by registration scope URL length.
  typedef std::map<size_t, scoped_refptr<ServiceWorkerRegistration>>
      ServiceWorkerRegistrationMap;
  // Contains all living registrations whose pattern this document's URL
  // starts with. It is empty if IsContextSecureForServiceWorker() is
  // false.
  ServiceWorkerRegistrationMap matching_registrations_;

  std::unique_ptr<OneShotGetReadyCallback> get_ready_callback_;
  scoped_refptr<ServiceWorkerVersion> controlling_version_;
  scoped_refptr<ServiceWorkerVersion> running_hosted_version_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
  ServiceWorkerDispatcherHost* dispatcher_host_;
  bool allow_association_;

  std::vector<base::Closure> queued_events_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
