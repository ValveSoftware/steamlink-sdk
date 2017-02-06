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

#ifndef ServiceWorkerGlobalScopeProxy_h
#define ServiceWorkerGlobalScopeProxy_h

#include "core/workers/WorkerReportingProxy.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebString.h"
#include "public/web/modules/serviceworker/WebServiceWorkerContextProxy.h"
#include "wtf/Forward.h"
#include <memory>

namespace blink {

class ConsoleMessage;
class Document;
class ServiceWorkerGlobalScope;
class WebEmbeddedWorkerImpl;
class WebServiceWorkerContextClient;
class WebServiceWorkerRequest;

// This class is created and destructed on the main thread, but live most
// of its time as a resident of the worker thread.
// All methods other than its ctor/dtor are called on the worker thread.
//
// This implements WebServiceWorkerContextProxy, which connects ServiceWorker's
// WorkerGlobalScope and embedder/chrome, and implements ServiceWorker-specific
// events/upcall methods that are to be called by embedder/chromium,
// e.g. onfetch.
//
// An instance of this class is supposed to outlive until
// workerThreadTerminated() is called by its corresponding
// WorkerGlobalScope.
class ServiceWorkerGlobalScopeProxy final
    : public GarbageCollectedFinalized<ServiceWorkerGlobalScopeProxy>
    , public WebServiceWorkerContextProxy
    , public WorkerReportingProxy {
    WTF_MAKE_NONCOPYABLE(ServiceWorkerGlobalScopeProxy);
public:
    static ServiceWorkerGlobalScopeProxy* create(WebEmbeddedWorkerImpl&, Document&, WebServiceWorkerContextClient&);
    ~ServiceWorkerGlobalScopeProxy() override;

    // WebServiceWorkerContextProxy overrides:
    void setRegistration(std::unique_ptr<WebServiceWorkerRegistration::Handle>) override;
    void dispatchActivateEvent(int) override;
    void dispatchExtendableMessageEvent(int eventID, const WebString& message, const WebSecurityOrigin& sourceOrigin, const WebMessagePortChannelArray&, const WebServiceWorkerClientInfo&) override;
    void dispatchExtendableMessageEvent(int eventID, const WebString& message, const WebSecurityOrigin& sourceOrigin, const WebMessagePortChannelArray&, std::unique_ptr<WebServiceWorker::Handle>) override;
    void dispatchFetchEvent(int responseID, int eventFinishID, const WebServiceWorkerRequest&) override;
    void dispatchForeignFetchEvent(int responseID, int eventFinishID, const WebServiceWorkerRequest&) override;
    void dispatchInstallEvent(int) override;
    void dispatchNotificationClickEvent(int, int64_t notificationID, const WebNotificationData&, int actionIndex) override;
    void dispatchNotificationCloseEvent(int, int64_t notificationID, const WebNotificationData&) override;
    void dispatchPushEvent(int, const WebString& data) override;
    void dispatchSyncEvent(int, const WebString& tag, LastChanceOption) override;
    bool hasFetchEventHandler() override;

    // WorkerReportingProxy overrides:
    void reportException(const String& errorMessage, std::unique_ptr<SourceLocation>) override;
    void reportConsoleMessage(ConsoleMessage*) override;
    void postMessageToPageInspector(const String&) override;
    void postWorkerConsoleAgentEnabled() override { }
    void didEvaluateWorkerScript(bool success) override;
    void didInitializeWorkerContext() override;
    void workerGlobalScopeStarted(WorkerGlobalScope*) override;
    void workerGlobalScopeClosed() override;
    void willDestroyWorkerGlobalScope() override;
    void workerThreadTerminated() override;

    DECLARE_TRACE();

    // Detach this proxy object entirely from the outside world,
    // clearing out all references.
    //
    // It is called during WebEmbeddedWorkerImpl finalization _after_
    // the worker thread using the proxy has been terminated.
    void detach();

private:
    ServiceWorkerGlobalScopeProxy(WebEmbeddedWorkerImpl&, Document&, WebServiceWorkerContextClient&);

    WebServiceWorkerContextClient& client() const;
    Document& document() const;
    ServiceWorkerGlobalScope* workerGlobalScope() const;

    // Non-null until the WebEmbeddedWorkerImpl explicitly detach()es
    // as part of its finalization.
    WebEmbeddedWorkerImpl* m_embeddedWorker;
    Member<Document> m_document;

    WebServiceWorkerContextClient* m_client;

    Member<ServiceWorkerGlobalScope> m_workerGlobalScope;
};

} // namespace blink

#endif // ServiceWorkerGlobalScopeProxy_h
