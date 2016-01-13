// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_script_context.h"

#include "base/logging.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/renderer/service_worker/embedded_worker_context_client.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerRequest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebServiceWorkerContextClient.h"
#include "third_party/WebKit/public/web/WebServiceWorkerContextProxy.h"

namespace content {

namespace {

void SendPostMessageToDocumentOnMainThread(
    ThreadSafeSender* sender,
    int routing_id,
    int client_id,
    const base::string16& message,
    scoped_ptr<blink::WebMessagePortChannelArray> channels) {
  sender->Send(new ServiceWorkerHostMsg_PostMessageToDocument(
      routing_id, client_id, message,
      WebMessagePortChannelImpl::ExtractMessagePortIDs(channels.release())));
}

}  // namespace

ServiceWorkerScriptContext::ServiceWorkerScriptContext(
    EmbeddedWorkerContextClient* embedded_context,
    blink::WebServiceWorkerContextProxy* proxy)
    : embedded_context_(embedded_context),
      proxy_(proxy) {
}

ServiceWorkerScriptContext::~ServiceWorkerScriptContext() {}

void ServiceWorkerScriptContext::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerScriptContext, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ActivateEvent, OnActivateEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_FetchEvent, OnFetchEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_InstallEvent, OnInstallEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_SyncEvent, OnSyncEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_PushEvent, OnPushEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_MessageToWorker, OnPostMessage)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidGetClientDocuments,
                        OnDidGetClientDocuments)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
}

void ServiceWorkerScriptContext::DidHandleActivateEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  Send(new ServiceWorkerHostMsg_ActivateEventFinished(
      GetRoutingID(), request_id, result));
}

void ServiceWorkerScriptContext::DidHandleInstallEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  Send(new ServiceWorkerHostMsg_InstallEventFinished(
      GetRoutingID(), request_id, result));
}

void ServiceWorkerScriptContext::DidHandleFetchEvent(
    int request_id,
    ServiceWorkerFetchEventResult result,
    const ServiceWorkerResponse& response) {
  Send(new ServiceWorkerHostMsg_FetchEventFinished(
      GetRoutingID(), request_id, result, response));
}

void ServiceWorkerScriptContext::DidHandleSyncEvent(int request_id) {
  Send(new ServiceWorkerHostMsg_SyncEventFinished(
      GetRoutingID(), request_id));
}

void ServiceWorkerScriptContext::GetClientDocuments(
    blink::WebServiceWorkerClientsCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = pending_clients_callbacks_.Add(callbacks);
  Send(new ServiceWorkerHostMsg_GetClientDocuments(
      GetRoutingID(), request_id));
}

void ServiceWorkerScriptContext::PostMessageToDocument(
    int client_id,
    const base::string16& message,
    scoped_ptr<blink::WebMessagePortChannelArray> channels) {
  // This may send channels for MessagePorts, and all internal book-keeping
  // messages for MessagePort (e.g. QueueMessages) are sent from main thread
  // (with thread hopping), so we need to do the same thread hopping here not
  // to overtake those messages.
  embedded_context_->main_thread_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&SendPostMessageToDocumentOnMainThread,
                 make_scoped_refptr(embedded_context_->thread_safe_sender()),
                 GetRoutingID(), client_id, message, base::Passed(&channels)));
}

void ServiceWorkerScriptContext::Send(IPC::Message* message) {
  embedded_context_->Send(message);
}

void ServiceWorkerScriptContext::OnActivateEvent(int request_id) {
  proxy_->dispatchActivateEvent(request_id);
}

void ServiceWorkerScriptContext::OnInstallEvent(int request_id,
                                                int active_version_id) {
  proxy_->dispatchInstallEvent(request_id);
}

void ServiceWorkerScriptContext::OnFetchEvent(
    int request_id,
    const ServiceWorkerFetchRequest& request) {
  blink::WebServiceWorkerRequest webRequest;
  webRequest.setURL(blink::WebURL(request.url));
  webRequest.setMethod(blink::WebString::fromUTF8(request.method));
  for (std::map<std::string, std::string>::const_iterator it =
           request.headers.begin();
       it != request.headers.end();
       ++it) {
    webRequest.setHeader(blink::WebString::fromUTF8(it->first),
                         blink::WebString::fromUTF8(it->second));
  }
  proxy_->dispatchFetchEvent(request_id, webRequest);
}

void ServiceWorkerScriptContext::OnSyncEvent(int request_id) {
  proxy_->dispatchSyncEvent(request_id);
}

void ServiceWorkerScriptContext::OnPushEvent(int request_id,
                                             const std::string& data) {
  proxy_->dispatchPushEvent(request_id, blink::WebString::fromUTF8(data));
  Send(new ServiceWorkerHostMsg_PushEventFinished(
      GetRoutingID(), request_id));
}

void ServiceWorkerScriptContext::OnPostMessage(
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids,
    const std::vector<int>& new_routing_ids) {
  std::vector<WebMessagePortChannelImpl*> ports;
  if (!sent_message_port_ids.empty()) {
    base::MessageLoopProxy* loop_proxy = embedded_context_->main_thread_proxy();
    ports.resize(sent_message_port_ids.size());
    for (size_t i = 0; i < sent_message_port_ids.size(); ++i) {
      ports[i] = new WebMessagePortChannelImpl(
          new_routing_ids[i], sent_message_port_ids[i], loop_proxy);
    }
  }

  proxy_->dispatchMessageEvent(message, ports);
}

void ServiceWorkerScriptContext::OnDidGetClientDocuments(
    int request_id, const std::vector<int>& client_ids) {
  blink::WebServiceWorkerClientsCallbacks* callbacks =
      pending_clients_callbacks_.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  scoped_ptr<blink::WebServiceWorkerClientsInfo> info(
      new blink::WebServiceWorkerClientsInfo);
  info->clientIDs = client_ids;
  callbacks->onSuccess(info.release());
  pending_clients_callbacks_.Remove(request_id);
}

int ServiceWorkerScriptContext::GetRoutingID() const {
  return embedded_context_->embedded_worker_id();
}

}  // namespace content
