// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CLIENT_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/service_worker/service_worker_types.h"
#include "ipc/ipc_listener.h"
#include "services/shell/public/interfaces/interface_provider.mojom.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannel.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_event_status.mojom.h"
#include "third_party/WebKit/public/web/modules/serviceworker/WebServiceWorkerContextClient.h"
#include "third_party/WebKit/public/web/modules/serviceworker/WebServiceWorkerContextProxy.h"
#include "v8/include/v8.h"

struct ServiceWorkerMsg_ExtendableMessageEvent_Params;

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace blink {
class WebDataSource;
struct WebServiceWorkerClientQueryOptions;
class WebServiceWorkerContextProxy;
class WebServiceWorkerProvider;
struct WebSyncRegistration;
}

namespace IPC {
class Message;
}

namespace content {

struct PlatformNotificationData;
struct PushEventPayload;
struct ServiceWorkerClientInfo;
class ServiceWorkerProviderContext;
class ServiceWorkerContextClient;
class ThreadSafeSender;
class WebServiceWorkerRegistrationImpl;

// This class provides access to/from an ServiceWorker's WorkerGlobalScope.
// Unless otherwise noted, all methods are called on the worker thread.
class ServiceWorkerContextClient
    : public blink::WebServiceWorkerContextClient {
 public:
  using SyncCallback =
      base::Callback<void(blink::mojom::ServiceWorkerEventStatus)>;

  // Returns a thread-specific client instance.  This does NOT create a
  // new instance.
  static ServiceWorkerContextClient* ThreadSpecificInstance();

  // Called on the main thread.
  ServiceWorkerContextClient(int embedded_worker_id,
                             int64_t service_worker_version_id,
                             const GURL& service_worker_scope,
                             const GURL& script_url,
                             int worker_devtools_agent_route_id);
  ~ServiceWorkerContextClient() override;

  void OnMessageReceived(int thread_id,
                         int embedded_worker_id,
                         const IPC::Message& message);

  // Called some time after the worker has started.
  void BindInterfaceProviders(
      shell::mojom::InterfaceProviderRequest request,
      shell::mojom::InterfaceProviderPtr remote_interfaces);

  // WebServiceWorkerContextClient overrides.
  blink::WebURL scope() const override;
  void getClient(const blink::WebString&,
                 blink::WebServiceWorkerClientCallbacks*) override;
  void getClients(const blink::WebServiceWorkerClientQueryOptions&,
                  blink::WebServiceWorkerClientsCallbacks*) override;
  void openWindow(const blink::WebURL&,
                  blink::WebServiceWorkerClientCallbacks*) override;
  void setCachedMetadata(const blink::WebURL&,
                         const char* data,
                         size_t size) override;
  void clearCachedMetadata(const blink::WebURL&) override;
  void workerReadyForInspection() override;

  // Called on the main thread.
  void workerContextFailedToStart() override;
  void workerScriptLoaded() override;
  bool hasAssociatedRegistration() override;

  void workerContextStarted(
      blink::WebServiceWorkerContextProxy* proxy) override;
  void didEvaluateWorkerScript(bool success) override;
  void didInitializeWorkerContext(v8::Local<v8::Context> context) override;
  void willDestroyWorkerContext(v8::Local<v8::Context> context) override;
  void workerContextDestroyed() override;
  void reportException(const blink::WebString& error_message,
                       int line_number,
                       int column_number,
                       const blink::WebString& source_url) override;
  void reportConsoleMessage(int source,
                            int level,
                            const blink::WebString& message,
                            int line_number,
                            const blink::WebString& source_url) override;
  void sendDevToolsMessage(int session_id,
                           int call_id,
                           const blink::WebString& message,
                           const blink::WebString& state) override;
  blink::WebDevToolsAgentClient::WebKitClientMessageLoop*
  createDevToolsMessageLoop() override;
  void didHandleActivateEvent(int request_id,
                              blink::WebServiceWorkerEventResult) override;
  void didHandleExtendableMessageEvent(
      int request_id,
      blink::WebServiceWorkerEventResult result) override;
  void didHandleInstallEvent(
      int request_id,
      blink::WebServiceWorkerEventResult result) override;
  void respondToFetchEvent(int response_id) override;
  void respondToFetchEvent(
      int response_id,
      const blink::WebServiceWorkerResponse& response) override;
  void didHandleFetchEvent(int event_finish_id,
                           blink::WebServiceWorkerEventResult result) override;
  void didHandleNotificationClickEvent(
      int request_id,
      blink::WebServiceWorkerEventResult result) override;
  void didHandleNotificationCloseEvent(
      int request_id,
      blink::WebServiceWorkerEventResult result) override;
  void didHandlePushEvent(int request_id,
                          blink::WebServiceWorkerEventResult result) override;
  void didHandleSyncEvent(int request_id,
                          blink::WebServiceWorkerEventResult result) override;

  // Called on the main thread.
  blink::WebServiceWorkerNetworkProvider* createServiceWorkerNetworkProvider(
      blink::WebDataSource* data_source) override;
  blink::WebServiceWorkerProvider* createServiceWorkerProvider() override;

  void postMessageToClient(
      const blink::WebString& uuid,
      const blink::WebString& message,
      blink::WebMessagePortChannelArray* channels) override;
  void postMessageToCrossOriginClient(
      const blink::WebCrossOriginServiceWorkerClient&,
      const blink::WebString&,
      blink::WebMessagePortChannelArray*) override;
  void focus(const blink::WebString& uuid,
             blink::WebServiceWorkerClientCallbacks*) override;
  void navigate(const blink::WebString& uuid,
                const blink::WebURL&,
                blink::WebServiceWorkerClientCallbacks*) override;
  void skipWaiting(
      blink::WebServiceWorkerSkipWaitingCallbacks* callbacks) override;
  void claim(blink::WebServiceWorkerClientsClaimCallbacks* callbacks) override;
  void registerForeignFetchScopes(
      const blink::WebVector<blink::WebURL>& sub_scopes,
      const blink::WebVector<blink::WebSecurityOrigin>& origins) override;

  virtual void DispatchSyncEvent(
      const std::string& tag,
      blink::WebServiceWorkerContextProxy::LastChanceOption last_chance,
      const SyncCallback& callback);

 private:
  struct WorkerContextData;

  // Get routing_id for sending message to the ServiceWorkerVersion
  // in the browser process.
  int GetRoutingID() const { return embedded_worker_id_; }

  void Send(IPC::Message* message);
  void SendWorkerStarted();
  void SetRegistrationInServiceWorkerGlobalScope(
      const ServiceWorkerRegistrationObjectInfo& info,
      const ServiceWorkerVersionAttributes& attrs);

  void OnActivateEvent(int request_id);
  void OnExtendableMessageEvent(
      int request_id,
      const ServiceWorkerMsg_ExtendableMessageEvent_Params& params);
  void OnInstallEvent(int request_id);
  void OnFetchEvent(int response_id,
                    int event_finish_id,
                    const ServiceWorkerFetchRequest& request);
  void OnNotificationClickEvent(
      int request_id,
      int64_t persistent_notification_id,
      const PlatformNotificationData& notification_data,
      int action_index);
  void OnPushEvent(int request_id, const PushEventPayload& payload);
  void OnNotificationCloseEvent(
      int request_id,
      int64_t persistent_notification_id,
      const PlatformNotificationData& notification_data);

  void OnDidGetClient(int request_id, const ServiceWorkerClientInfo& client);
  void OnDidGetClients(
      int request_id, const std::vector<ServiceWorkerClientInfo>& clients);
  void OnOpenWindowResponse(int request_id,
                            const ServiceWorkerClientInfo& client);
  void OnOpenWindowError(int request_id, const std::string& message);
  void OnFocusClientResponse(int request_id,
                             const ServiceWorkerClientInfo& client);
  void OnNavigateClientResponse(int request_id,
                                const ServiceWorkerClientInfo& client);
  void OnNavigateClientError(int request_id, const GURL& url);
  void OnDidSkipWaiting(int request_id);
  void OnDidClaimClients(int request_id);
  void OnClaimClientsError(int request_id,
                           blink::WebServiceWorkerError::ErrorType error_type,
                           const base::string16& message);
  void OnPing();

  base::WeakPtr<ServiceWorkerContextClient> GetWeakPtr();

  const int embedded_worker_id_;
  const int64_t service_worker_version_id_;
  const GURL service_worker_scope_;
  const GURL script_url_;
  const int worker_devtools_agent_route_id_;
  scoped_refptr<ThreadSafeSender> sender_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_refptr<base::TaskRunner> worker_task_runner_;

  scoped_refptr<ServiceWorkerProviderContext> provider_context_;

  // Not owned; this object is destroyed when proxy_ becomes invalid.
  blink::WebServiceWorkerContextProxy* proxy_;

  // Initialized on the worker thread in workerContextStarted and
  // destructed on the worker thread in willDestroyWorkerContext.
  std::unique_ptr<WorkerContextData> context_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextClient);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CLIENT_H_
