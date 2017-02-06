// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "content/public/child/worker_thread.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProvider.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerState.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
}

namespace IPC {
class Message;
}

struct ServiceWorkerMsg_MessageToDocument_Params;

namespace content {

class ServiceWorkerHandleReference;
class ServiceWorkerMessageFilter;
class ServiceWorkerProviderContext;
class ServiceWorkerRegistrationHandleReference;
class ThreadSafeSender;
class WebServiceWorkerImpl;
class WebServiceWorkerRegistrationImpl;
struct ServiceWorkerObjectInfo;
struct ServiceWorkerRegistrationObjectInfo;
struct ServiceWorkerVersionAttributes;

// This class manages communication with the browser process about
// registration of the service worker, exposed to renderer and worker
// scripts through methods like navigator.registerServiceWorker().
class CONTENT_EXPORT ServiceWorkerDispatcher : public WorkerThread::Observer {
 public:
  typedef blink::WebServiceWorkerProvider::WebServiceWorkerRegistrationCallbacks
      WebServiceWorkerRegistrationCallbacks;
  typedef blink::WebServiceWorkerRegistration::WebServiceWorkerUpdateCallbacks
      WebServiceWorkerUpdateCallbacks;
  typedef blink::WebServiceWorkerRegistration::
      WebServiceWorkerUnregistrationCallbacks
          WebServiceWorkerUnregistrationCallbacks;
  typedef
      blink::WebServiceWorkerProvider::WebServiceWorkerGetRegistrationCallbacks
      WebServiceWorkerGetRegistrationCallbacks;
  typedef
      blink::WebServiceWorkerProvider::WebServiceWorkerGetRegistrationsCallbacks
      WebServiceWorkerGetRegistrationsCallbacks;
  typedef blink::WebServiceWorkerProvider::
      WebServiceWorkerGetRegistrationForReadyCallbacks
          WebServiceWorkerGetRegistrationForReadyCallbacks;

  ServiceWorkerDispatcher(
      ThreadSafeSender* thread_safe_sender,
      base::SingleThreadTaskRunner* main_thread_task_runner);
  ~ServiceWorkerDispatcher() override;

  void OnMessageReceived(const IPC::Message& msg);

  // Corresponds to navigator.serviceWorker.register().
  void RegisterServiceWorker(
      int provider_id,
      const GURL& pattern,
      const GURL& script_url,
      WebServiceWorkerRegistrationCallbacks* callbacks);
  // Corresponds to ServiceWorkerRegistration.update().
  void UpdateServiceWorker(int provider_id,
                           int64_t registration_id,
                           WebServiceWorkerUpdateCallbacks* callbacks);
  // Corresponds to ServiceWorkerRegistration.unregister().
  void UnregisterServiceWorker(
      int provider_id,
      int64_t registration_id,
      WebServiceWorkerUnregistrationCallbacks* callbacks);
  // Corresponds to navigator.serviceWorker.getRegistration().
  void GetRegistration(int provider_id,
                       const GURL& document_url,
                       WebServiceWorkerGetRegistrationCallbacks* callbacks);
  // Corresponds to navigator.serviceWorker.getRegistrations().
  void GetRegistrations(
      int provider_id,
      WebServiceWorkerGetRegistrationsCallbacks* callbacks);

  void GetRegistrationForReady(
      int provider_id,
      WebServiceWorkerGetRegistrationForReadyCallbacks* callbacks);

  // Called when a new provider context for a document is created. Usually
  // this happens when a new document is being loaded, and is called much
  // earlier than AddScriptClient.
  // (This is attached only to the document thread's ServiceWorkerDispatcher)
  void AddProviderContext(ServiceWorkerProviderContext* provider_context);
  void RemoveProviderContext(ServiceWorkerProviderContext* provider_context);

  // Called when navigator.serviceWorker is instantiated or detached
  // for a document whose provider can be identified by |provider_id|.
  void AddProviderClient(int provider_id,
                         blink::WebServiceWorkerProviderClient* client);
  void RemoveProviderClient(int provider_id);

  // Returns the existing service worker or a newly created one with the given
  // handle reference. Returns nullptr if the given reference is invalid.
  scoped_refptr<WebServiceWorkerImpl> GetOrCreateServiceWorker(
      std::unique_ptr<ServiceWorkerHandleReference> handle_ref);

  // Returns the existing registration or a newly created one. When a new one is
  // created, increments interprocess references to the registration and its
  // versions via ServiceWorker(Registration)HandleReference.
  scoped_refptr<WebServiceWorkerRegistrationImpl> GetOrCreateRegistration(
      const ServiceWorkerRegistrationObjectInfo& info,
      const ServiceWorkerVersionAttributes& attrs);

  // Returns the existing registration or a newly created one. Always adopts
  // interprocess references to the registration and its versions via
  // ServiceWorker(Registration)HandleReference.
  scoped_refptr<WebServiceWorkerRegistrationImpl> GetOrAdoptRegistration(
      const ServiceWorkerRegistrationObjectInfo& info,
      const ServiceWorkerVersionAttributes& attrs);

  static ServiceWorkerDispatcher* GetOrCreateThreadSpecificInstance(
      ThreadSafeSender* thread_safe_sender,
      base::SingleThreadTaskRunner* main_thread_task_runner);

  // Unlike GetOrCreateThreadSpecificInstance() this doesn't create a new
  // instance if thread-local instance doesn't exist.
  static ServiceWorkerDispatcher* GetThreadSpecificInstance();

  base::SingleThreadTaskRunner* main_thread_task_runner() {
    return main_thread_task_runner_.get();
  }

 private:
  typedef IDMap<WebServiceWorkerRegistrationCallbacks,
      IDMapOwnPointer> RegistrationCallbackMap;
  typedef IDMap<WebServiceWorkerUpdateCallbacks, IDMapOwnPointer>
      UpdateCallbackMap;
  typedef IDMap<WebServiceWorkerUnregistrationCallbacks,
      IDMapOwnPointer> UnregistrationCallbackMap;
  typedef IDMap<WebServiceWorkerGetRegistrationCallbacks,
      IDMapOwnPointer> GetRegistrationCallbackMap;
  typedef IDMap<WebServiceWorkerGetRegistrationsCallbacks,
      IDMapOwnPointer> GetRegistrationsCallbackMap;
  typedef IDMap<WebServiceWorkerGetRegistrationForReadyCallbacks,
      IDMapOwnPointer> GetRegistrationForReadyCallbackMap;

  typedef std::map<int, blink::WebServiceWorkerProviderClient*>
      ProviderClientMap;
  typedef std::map<int, ServiceWorkerProviderContext*> ProviderContextMap;
  typedef std::map<int, ServiceWorkerProviderContext*> WorkerToProviderMap;
  typedef std::map<int, WebServiceWorkerImpl*> WorkerObjectMap;
  typedef std::map<int, WebServiceWorkerRegistrationImpl*>
      RegistrationObjectMap;

  friend class ServiceWorkerDispatcherTest;
  friend class WebServiceWorkerImpl;
  friend class WebServiceWorkerRegistrationImpl;

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

  void OnAssociateRegistration(int thread_id,
                               int provider_id,
                               const ServiceWorkerRegistrationObjectInfo& info,
                               const ServiceWorkerVersionAttributes& attrs);
  void OnDisassociateRegistration(int thread_id,
                                  int provider_id);
  void OnRegistered(int thread_id,
                    int request_id,
                    const ServiceWorkerRegistrationObjectInfo& info,
                    const ServiceWorkerVersionAttributes& attrs);
  void OnUpdated(int thread_id, int request_id);
  void OnUnregistered(int thread_id,
                      int request_id,
                      bool is_success);
  void OnDidGetRegistration(int thread_id,
                            int request_id,
                            const ServiceWorkerRegistrationObjectInfo& info,
                            const ServiceWorkerVersionAttributes& attrs);
  void OnDidGetRegistrations(
      int thread_id,
      int request_id,
      const std::vector<ServiceWorkerRegistrationObjectInfo>& infos,
      const std::vector<ServiceWorkerVersionAttributes>& attrs);
  void OnDidGetRegistrationForReady(
      int thread_id,
      int request_id,
      const ServiceWorkerRegistrationObjectInfo& info,
      const ServiceWorkerVersionAttributes& attrs);
  void OnRegistrationError(int thread_id,
                           int request_id,
                           blink::WebServiceWorkerError::ErrorType error_type,
                           const base::string16& message);
  void OnUpdateError(int thread_id,
                     int request_id,
                     blink::WebServiceWorkerError::ErrorType error_type,
                     const base::string16& message);
  void OnUnregistrationError(int thread_id,
                             int request_id,
                             blink::WebServiceWorkerError::ErrorType error_type,
                             const base::string16& message);
  void OnGetRegistrationError(
      int thread_id,
      int request_id,
      blink::WebServiceWorkerError::ErrorType error_type,
      const base::string16& message);
  void OnGetRegistrationsError(
      int thread_id,
      int request_id,
      blink::WebServiceWorkerError::ErrorType error_type,
      const base::string16& message);
  void OnServiceWorkerStateChanged(int thread_id,
                                   int handle_id,
                                   blink::WebServiceWorkerState state);
  void OnSetVersionAttributes(int thread_id,
                              int registration_handle_id,
                              int changed_mask,
                              const ServiceWorkerVersionAttributes& attributes);
  void OnUpdateFound(int thread_id,
                     int registration_handle_id);
  void OnSetControllerServiceWorker(int thread_id,
                                    int provider_id,
                                    const ServiceWorkerObjectInfo& info,
                                    bool should_notify_controllerchange);
  void OnPostMessage(const ServiceWorkerMsg_MessageToDocument_Params& params);

  // Keeps map from handle_id to ServiceWorker object.
  void AddServiceWorker(int handle_id, WebServiceWorkerImpl* worker);
  void RemoveServiceWorker(int handle_id);

  // Keeps map from registration_handle_id to ServiceWorkerRegistration object.
  void AddServiceWorkerRegistration(
      int registration_handle_id,
      WebServiceWorkerRegistrationImpl* registration);
  void RemoveServiceWorkerRegistration(
      int registration_handle_id);

  // Assumes that the given object information retains an interprocess handle
  // reference passed from the browser process, and adopts it.
  std::unique_ptr<ServiceWorkerRegistrationHandleReference> Adopt(
      const ServiceWorkerRegistrationObjectInfo& info);
  std::unique_ptr<ServiceWorkerHandleReference> Adopt(
      const ServiceWorkerObjectInfo& info);

  RegistrationCallbackMap pending_registration_callbacks_;
  UpdateCallbackMap pending_update_callbacks_;
  UnregistrationCallbackMap pending_unregistration_callbacks_;
  GetRegistrationCallbackMap pending_get_registration_callbacks_;
  GetRegistrationsCallbackMap pending_get_registrations_callbacks_;
  GetRegistrationForReadyCallbackMap get_for_ready_callbacks_;

  ProviderClientMap provider_clients_;
  ProviderContextMap provider_contexts_;

  WorkerObjectMap service_workers_;
  RegistrationObjectMap registrations_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_
