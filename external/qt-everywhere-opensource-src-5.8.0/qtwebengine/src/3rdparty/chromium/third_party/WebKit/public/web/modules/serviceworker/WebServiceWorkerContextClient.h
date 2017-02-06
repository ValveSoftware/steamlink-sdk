/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebServiceWorkerContextClient_h
#define WebServiceWorkerContextClient_h

#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebURL.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerClientsClaimCallbacks.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerClientsInfo.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerEventResult.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerSkipWaitingCallbacks.h"
#include "public/web/WebDevToolsAgentClient.h"
#include <v8.h>

namespace blink {

struct WebCrossOriginServiceWorkerClient;
struct WebServiceWorkerClientQueryOptions;
class WebDataSource;
class WebServiceWorkerContextProxy;
class WebServiceWorkerNetworkProvider;
class WebServiceWorkerProvider;
class WebServiceWorkerResponse;
class WebString;

// This interface is implemented by the client. It is supposed to be created
// on the main thread and then passed on to the worker thread by a newly
// created WorkerGlobalScope. Unless otherwise noted, all methods of this class
// are called on the worker thread.
class WebServiceWorkerContextClient {
public:
    virtual ~WebServiceWorkerContextClient() { }

    // ServiceWorker specific method. Called when script accesses the
    // the |scope| attribute of the ServiceWorkerGlobalScope. Immutable per spec.
    virtual WebURL scope() const { return WebURL(); }

    // ServiceWorker has prepared everything for script loading and is now ready for inspection.
    virtual void workerReadyForInspection() { }

    // The worker script is successfully loaded and a new thread is about to
    // be started. Called on the main thread.
    virtual void workerScriptLoaded() { }

    virtual bool hasAssociatedRegistration() { return false; }

    // A new WorkerGlobalScope is created and started to run on the
    // worker thread.
    // This also gives back a proxy to the client to talk to the
    // newly created WorkerGlobalScope. The proxy is held by WorkerGlobalScope
    // and should not be held by the caller. No proxy methods should be called
    // after willDestroyWorkerContext() is called.
    virtual void workerContextStarted(WebServiceWorkerContextProxy*) { }

    // WorkerGlobalScope is about to be destroyed. The client should clear
    // the WebServiceWorkerGlobalScopeProxy when this is called.
    virtual void willDestroyWorkerContext(v8::Local<v8::Context> context) { }

    // WorkerGlobalScope is destroyed and the worker is ready to be terminated.
    virtual void workerContextDestroyed() { }

    // Starting worker context is failed. This could happen when loading
    // worker script fails, or is asked to terminated before the context starts.
    // This is called on the main thread.
    virtual void workerContextFailedToStart() { }

    // Called when the worker script is evaluated. |success| is true if the
    // evaluation completed with no uncaught exception.
    virtual void didEvaluateWorkerScript(bool success) { }

    // Called when the worker context is initialized.
    virtual void didInitializeWorkerContext(v8::Local<v8::Context> context) { }

    // Called when the WorkerGlobalScope had an error or an exception.
    virtual void reportException(const WebString& errorMessage, int lineNumber, int columnNumber, const WebString& sourceURL) { }

    // Called when the console message is reported.
    virtual void reportConsoleMessage(int source, int level, const WebString& message, int lineNumber, const WebString& sourceURL) { }

    // Inspector related messages.
    virtual void sendDevToolsMessage(int sessionId, int callId, const WebString& message, const WebString& state) { }

    // Message loop for debugging.
    virtual WebDevToolsAgentClient::WebKitClientMessageLoop* createDevToolsMessageLoop() { return nullptr; }

    // ServiceWorker specific method.
    virtual void didHandleActivateEvent(int eventID, WebServiceWorkerEventResult result) { }

    // Called after ExtendableMessageEvent is handled by the ServiceWorker's
    // script context.
    virtual void didHandleExtendableMessageEvent(int eventID, WebServiceWorkerEventResult result) { }

    // ServiceWorker specific methods. respondFetchEvent will be called after
    // FetchEvent returns a response by the ServiceWorker's script context, and
    // didHandleFetchEvent will be called after the end of FetchEvent's
    // lifecycle. When no response is provided, the browser should fallback to
    // native fetch. EventIDs are the same with the ids passed from
    // dispatchFetchEvent respectively.
    virtual void respondToFetchEvent(int responseID) { };
    virtual void respondToFetchEvent(int responseID, const WebServiceWorkerResponse& response) { };
    virtual void didHandleFetchEvent(int eventFinishID, WebServiceWorkerEventResult result) { };

    // ServiceWorker specific method. Called after InstallEvent (dispatched
    // via WebServiceWorkerContextProxy) is handled by the ServiceWorker's
    // script context.
    virtual void didHandleInstallEvent(int installEventID, WebServiceWorkerEventResult result) { }

    // ServiceWorker specific method. Called after NotificationClickEvent
    // (dispatched via WebServiceWorkerContextProxy) is handled by the
    // ServiceWorker's script context.
    virtual void didHandleNotificationClickEvent(int eventID, WebServiceWorkerEventResult result) { }

    // ServiceWorker specific method. Called after NotificationCloseEvent
    // (dispatched via WebServiceWorkerContextProxy) is handled by the
    // ServiceWorker's script context.
    virtual void didHandleNotificationCloseEvent(int eventID, WebServiceWorkerEventResult result) { }

    // ServiceWorker specific method. Called after PushEvent (dispatched via
    // WebServiceWorkerContextProxy) is handled by the ServiceWorker's script
    // context.
    virtual void didHandlePushEvent(int pushEventID, WebServiceWorkerEventResult result) { }

    // ServiceWorker specific method. Called after SyncEvent (dispatched via
    // WebServiceWorkerContextProxy) is handled by the ServiceWorker's script
    // context.
    virtual void didHandleSyncEvent(int syncEventID, WebServiceWorkerEventResult result) { }

    // Ownership of the returned object is transferred to the caller.
    // This is called on the main thread.
    virtual WebServiceWorkerNetworkProvider* createServiceWorkerNetworkProvider(WebDataSource*) { return nullptr; }

    // Ownership of the returned object is transferred to the caller.
    // This is called on the main thread.
    virtual WebServiceWorkerProvider* createServiceWorkerProvider() { return nullptr; }

    // Ownership of the passed callbacks is transferred to the callee, callee
    // should delete the callbacks after calling either onSuccess or onError.
    // WebServiceWorkerClientInfo and WebServiceWorkerError ownerships are
    // passed to the WebServiceWorkerClientCallbacks implementation.
    virtual void getClient(const WebString&, WebServiceWorkerClientCallbacks*) = 0;

    // Ownership of the passed callbacks is transferred to the callee, callee
    // should delete the callbacks after calling either onSuccess or onError.
    // WebServiceWorkerClientsInfo and WebServiceWorkerError ownerships are
    // passed to the WebServiceWorkerClientsCallbacks implementation.
    virtual void getClients(const WebServiceWorkerClientQueryOptions&, WebServiceWorkerClientsCallbacks*) = 0;

    // Ownership of the passed callbacks is transferred to the callee, callee
    // should delete the callbacks after calling either onSuccess or onError.
    // WebServiceWorkerClientInfo and WebServiceWorkerError ownerships are
    // passed to the WebServiceWorkerClientsCallbacks implementation.
    virtual void openWindow(const WebURL&, WebServiceWorkerClientCallbacks*) = 0;

    // A suggestion to cache this metadata in association with this URL.
    virtual void setCachedMetadata(const WebURL& url, const char* data, size_t size) { }

    // A suggestion to clear the cached metadata in association with this URL.
    virtual void clearCachedMetadata(const WebURL& url) { }

    // Callee receives ownership of the passed vector.
    // FIXME: Blob refs should be passed to maintain ref counts. crbug.com/351753
    virtual void postMessageToClient(const WebString& uuid, const WebString&, WebMessagePortChannelArray*) = 0;

    // Callee receives ownership of the passed vector.
    // FIXME: Blob refs should be passed to maintain ref counts. crbug.com/351753
    virtual void postMessageToCrossOriginClient(const WebCrossOriginServiceWorkerClient&, const WebString&, WebMessagePortChannelArray*) = 0;

    // Ownership of the passed callbacks is transferred to the callee, callee
    // should delete the callbacks after run.
    virtual void skipWaiting(WebServiceWorkerSkipWaitingCallbacks*) = 0;

    // Ownership of the passed callbacks is transferred to the callee, callee
    // should delete the callbacks after run.
    virtual void claim(WebServiceWorkerClientsClaimCallbacks*) = 0;

    // Ownership of the passed callbacks is transferred to the callee, callee
    // should delete the callback after calling either onSuccess or onError.
    virtual void focus(const WebString& uuid, WebServiceWorkerClientCallbacks*) = 0;

    // Ownership of the passed callbacks is transferred to the callee, callee
    // should delete the callbacks after calling either onSuccess or onError.
    // WebServiceWorkerClientInfo and WebServiceWorkerError ownerships are
    // passed to the WebServiceWorkerClientsCallbacks implementation.
    virtual void navigate(const WebString& uuid, const WebURL&, WebServiceWorkerClientCallbacks*) = 0;

    // Called when the worker wants to register subscopes to handle via foreign
    // fetch. Will only be called while an install event is in progress.
    virtual void registerForeignFetchScopes(const WebVector<WebURL>& subScopes, const WebVector<WebSecurityOrigin>& origins) = 0;
};

} // namespace blink

#endif // WebServiceWorkerContextClient_h
