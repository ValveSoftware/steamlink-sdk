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
#include "public/platform/WebString.h"
#include "public/web/WebServiceWorkerContextProxy.h"
#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"

namespace WebCore {
class ExecutionContext;
}

namespace blink {

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
// workerGlobalScopeDestroyed() is called by its corresponding
// WorkerGlobalScope.
class ServiceWorkerGlobalScopeProxy FINAL :
    public WebServiceWorkerContextProxy,
    public WebCore::WorkerReportingProxy {
    WTF_MAKE_NONCOPYABLE(ServiceWorkerGlobalScopeProxy);
public:
    static PassOwnPtr<ServiceWorkerGlobalScopeProxy> create(WebEmbeddedWorkerImpl&, WebCore::ExecutionContext&, WebServiceWorkerContextClient&);
    virtual ~ServiceWorkerGlobalScopeProxy();

    // WebServiceWorkerContextProxy overrides:
    virtual void dispatchActivateEvent(int) OVERRIDE;
    virtual void dispatchInstallEvent(int) OVERRIDE;
    virtual void dispatchFetchEvent(int, const WebServiceWorkerRequest&) OVERRIDE;
    virtual void dispatchMessageEvent(const WebString& message, const WebMessagePortChannelArray&) OVERRIDE;
    virtual void dispatchPushEvent(int, const WebString& data) OVERRIDE;
    virtual void dispatchSyncEvent(int) OVERRIDE;

    // WorkerReportingProxy overrides:
    virtual void reportException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL) OVERRIDE;
    virtual void reportConsoleMessage(WebCore::MessageSource, WebCore::MessageLevel, const String& message, int lineNumber, const String& sourceURL) OVERRIDE;
    virtual void postMessageToPageInspector(const String&) OVERRIDE;
    virtual void updateInspectorStateCookie(const String&) OVERRIDE;
    virtual void workerGlobalScopeStarted(WebCore::WorkerGlobalScope*) OVERRIDE;
    virtual void workerGlobalScopeClosed() OVERRIDE;
    virtual void willDestroyWorkerGlobalScope() OVERRIDE;
    virtual void workerGlobalScopeDestroyed() OVERRIDE;

private:
    ServiceWorkerGlobalScopeProxy(WebEmbeddedWorkerImpl&, WebCore::ExecutionContext&, WebServiceWorkerContextClient&);

    WebEmbeddedWorkerImpl& m_embeddedWorker;
    WebCore::ExecutionContext& m_executionContext;

    WebServiceWorkerContextClient& m_client;

    WebCore::WorkerGlobalScope* m_workerGlobalScope;
};

} // namespace blink

#endif // ServiceWorkerGlobalScopeProxy_h
