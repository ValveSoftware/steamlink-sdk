/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebSharedWorkerImpl_h
#define WebSharedWorkerImpl_h

#include "public/web/WebSharedWorker.h"

#include "core/dom/ExecutionContext.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThread.h"
#include "public/platform/WebAddressSpace.h"
#include "public/web/WebContentSecurityPolicy.h"
#include "public/web/WebDevToolsAgentClient.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebSharedWorkerClient.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class ConsoleMessage;
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebLocalFrameImpl;
class WebServiceWorkerNetworkProvider;
class WebSharedWorkerClient;
class WebString;
class WebURL;
class WebView;
class WorkerInspectorProxy;
class WorkerScriptLoader;

// This class is used by the worker process code to talk to the SharedWorker
// implementation.
// It can't use it directly since it uses WebKit types, so this class converts
// the data types.  When the SharedWorker object wants to call
// WorkerReportingProxy, this class will convert to Chrome data types first and
// then call the supplied WebCommonWorkerClient.
class WebSharedWorkerImpl final
    : public WorkerReportingProxy
    , public WebFrameClient
    , public WebSharedWorker
    , public WebDevToolsAgentClient
    , private WorkerLoaderProxyProvider {
public:
    explicit WebSharedWorkerImpl(WebSharedWorkerClient*);

    // WorkerReportingProxy methods:
    void reportException(const WTF::String&, std::unique_ptr<SourceLocation>) override;
    void reportConsoleMessage(ConsoleMessage*) override;
    void postMessageToPageInspector(const WTF::String&) override;
    void postWorkerConsoleAgentEnabled() override { }
    void didEvaluateWorkerScript(bool success) override { }
    void workerGlobalScopeStarted(WorkerGlobalScope*) override;
    void workerGlobalScopeClosed() override;
    void workerThreadTerminated() override;
    void willDestroyWorkerGlobalScope() override { }

    // WebFrameClient methods to support resource loading thru the 'shadow page'.
    WebApplicationCacheHost* createApplicationCacheHost(WebApplicationCacheHostClient*) override;
    void willSendRequest(WebLocalFrame*, unsigned identifier, WebURLRequest&, const WebURLResponse& redirectResponse) override;
    void didFinishDocumentLoad(WebLocalFrame*) override;
    bool isControlledByServiceWorker(WebDataSource&) override;
    int64_t serviceWorkerID(WebDataSource&) override;

    // WebDevToolsAgentClient overrides.
    void sendProtocolMessage(int sessionId, int callId, const WebString&, const WebString&) override;
    void resumeStartup() override;
    WebDevToolsAgentClient::WebKitClientMessageLoop* createClientMessageLoop() override;

    // WebSharedWorker methods:
    void startWorkerContext(const WebURL&, const WebString& name, const WebString& contentSecurityPolicy, WebContentSecurityPolicyType, WebAddressSpace) override;
    void connect(WebMessagePortChannel*) override;
    void terminateWorkerContext() override;

    void pauseWorkerContextOnStart() override;
    void attachDevTools(const WebString& hostId, int sessionId) override;
    void reattachDevTools(const WebString& hostId, int sesionId, const WebString& savedState) override;
    void detachDevTools() override;
    void dispatchDevToolsMessage(int sessionId, int callId, const WebString& method, const WebString& message) override;

private:
    ~WebSharedWorkerImpl() override;

    WorkerThread* workerThread() { return m_workerThread.get(); }

    // Shuts down the worker thread.
    void terminateWorkerThread();

    // Creates the shadow loader used for worker network requests.
    void initializeLoader();

    void loadShadowPage();
    void didReceiveScriptLoaderResponse();
    void onScriptLoaderFinished();

    static void connectTask(WebMessagePortChannelUniquePtr, ExecutionContext*);
    // Tasks that are run on the main thread.
    void workerGlobalScopeClosedOnMainThread();
    void workerThreadTerminatedOnMainThread();

    void postMessageToPageInspectorOnMainThread(const String& message);

    // WorkerLoaderProxyProvider
    void postTaskToLoader(std::unique_ptr<ExecutionContextTask>) override;
    bool postTaskToWorkerGlobalScope(std::unique_ptr<ExecutionContextTask>) override;

    // 'shadow page' - created to proxy loading requests from the worker.
    Persistent<ExecutionContext> m_loadingDocument;
    WebView* m_webView;
    Persistent<WebLocalFrameImpl> m_mainFrame;
    bool m_askedToTerminate;

    // This one is bound to and used only on the main thread.
    std::unique_ptr<WebServiceWorkerNetworkProvider> m_networkProvider;

    Persistent<WorkerInspectorProxy> m_workerInspectorProxy;

    std::unique_ptr<WorkerThread> m_workerThread;

    WebSharedWorkerClient* m_client;

    bool m_pauseWorkerContextOnStart;
    bool m_isPausedOnStart;

    // Kept around only while main script loading is ongoing.
    RefPtr<WorkerScriptLoader> m_mainScriptLoader;

    RefPtr<WorkerLoaderProxy> m_loaderProxy;

    WebURL m_url;
    WebString m_name;
    WebAddressSpace m_creationAddressSpace;
};

} // namespace blink

#endif // WebSharedWorkerImpl_h
