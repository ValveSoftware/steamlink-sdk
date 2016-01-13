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

#include "config.h"
#include "web/WebSharedWorkerImpl.h"

#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/events/MessageEvent.h"
#include "core/html/HTMLFormElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/WorkerDebuggerAgent.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/page/Page.h"
#include "core/workers/SharedWorkerGlobalScope.h"
#include "core/workers/SharedWorkerThread.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerScriptLoader.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "modules/webdatabase/DatabaseTask.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/heap/Handle.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebFileError.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/web/WebFrame.h"
#include "public/web/WebSettings.h"
#include "public/web/WebView.h"
#include "public/web/WebWorkerPermissionClientProxy.h"
#include "web/DatabaseClientImpl.h"
#include "web/LocalFileSystemClient.h"
#include "web/WebDataSourceImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WorkerPermissionClient.h"
#include "wtf/Functional.h"
#include "wtf/MainThread.h"

using namespace WebCore;

namespace blink {

// A thin wrapper for one-off script loading.
class WebSharedWorkerImpl::Loader : public WorkerScriptLoaderClient {
public:
    static PassOwnPtr<Loader> create()
    {
        return adoptPtr(new Loader());
    }

    virtual ~Loader()
    {
        m_scriptLoader->setClient(0);
    }

    void load(ExecutionContext* loadingContext, const KURL& scriptURL, const Closure& receiveResponseCallback, const Closure& finishCallback)
    {
        ASSERT(loadingContext);
        m_receiveResponseCallback = receiveResponseCallback;
        m_finishCallback = finishCallback;
        m_scriptLoader->setTargetType(ResourceRequest::TargetIsSharedWorker);
        m_scriptLoader->loadAsynchronously(
            *loadingContext, scriptURL, DenyCrossOriginRequests, this);
    }

    void didReceiveResponse(unsigned long identifier, const ResourceResponse& response) OVERRIDE
    {
        m_identifier = identifier;
        m_appCacheID = response.appCacheID();
        m_receiveResponseCallback();
    }

    virtual void notifyFinished() OVERRIDE
    {
        m_finishCallback();
    }

    void cancel()
    {
        m_scriptLoader->cancel();
    }

    bool failed() const { return m_scriptLoader->failed(); }
    const KURL& url() const { return m_scriptLoader->responseURL(); }
    String script() const { return m_scriptLoader->script(); }
    unsigned long identifier() const { return m_identifier; }
    long long appCacheID() const { return m_appCacheID; }

private:
    Loader() : m_scriptLoader(WorkerScriptLoader::create()), m_identifier(0), m_appCacheID(0)
    {
    }

    RefPtr<WorkerScriptLoader> m_scriptLoader;
    unsigned long m_identifier;
    long long m_appCacheID;
    Closure m_receiveResponseCallback;
    Closure m_finishCallback;
};

// This function is called on the main thread to force to initialize some static
// values used in WebKit before any worker thread is started. This is because in
// our worker processs, we do not run any WebKit code in main thread and thus
// when multiple workers try to start at the same time, we might hit crash due
// to contention for initializing static values.
static void initializeWebKitStaticValues()
{
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        // Note that we have to pass a URL with valid protocol in order to follow
        // the path to do static value initializations.
        RefPtr<SecurityOrigin> origin =
            SecurityOrigin::create(KURL(ParsedURLString, "http://localhost"));
        origin.release();
    }
}

WebSharedWorkerImpl::WebSharedWorkerImpl(WebSharedWorkerClient* client)
    : m_webView(0)
    , m_mainFrame(0)
    , m_askedToTerminate(false)
    , m_client(WeakReference<WebSharedWorkerClient>::create(client))
    , m_clientWeakPtr(WeakPtr<WebSharedWorkerClient>(m_client))
    , m_pauseWorkerContextOnStart(false)
    , m_attachDevToolsOnStart(false)
{
    initializeWebKitStaticValues();
}

WebSharedWorkerImpl::~WebSharedWorkerImpl()
{
    ASSERT(m_webView);
    // Detach the client before closing the view to avoid getting called back.
    toWebLocalFrameImpl(m_mainFrame)->setClient(0);

    m_webView->close();
    m_mainFrame->close();
}

void WebSharedWorkerImpl::stopWorkerThread()
{
    if (m_askedToTerminate)
        return;
    m_askedToTerminate = true;
    if (m_mainScriptLoader)
        m_mainScriptLoader->cancel();
    if (m_workerThread)
        m_workerThread->stop();
}

void WebSharedWorkerImpl::initializeLoader(const WebURL& url)
{
    // Create 'shadow page'. This page is never displayed, it is used to proxy the
    // loading requests from the worker context to the rest of WebKit and Chromium
    // infrastructure.
    ASSERT(!m_webView);
    m_webView = WebView::create(0);
    m_webView->settings()->setOfflineWebApplicationCacheEnabled(RuntimeEnabledFeatures::applicationCacheEnabled());
    // FIXME: http://crbug.com/363843. This needs to find a better way to
    // not create graphics layers.
    m_webView->settings()->setAcceleratedCompositingEnabled(false);
    // FIXME: Settings information should be passed to the Worker process from Browser process when the worker
    // is created (similar to RenderThread::OnCreateNewView).
    m_mainFrame = WebLocalFrame::create(this);
    m_webView->setMainFrame(m_mainFrame);

    WebLocalFrameImpl* webFrame = toWebLocalFrameImpl(m_webView->mainFrame());

    // Construct substitute data source for the 'shadow page'. We only need it
    // to have same origin as the worker so the loading checks work correctly.
    CString content("");
    int length = static_cast<int>(content.length());
    RefPtr<SharedBuffer> buffer(SharedBuffer::create(content.data(), length));
    webFrame->frame()->loader().load(FrameLoadRequest(0, ResourceRequest(url), SubstituteData(buffer, "text/html", "UTF-8", KURL())));
}

WebApplicationCacheHost* WebSharedWorkerImpl::createApplicationCacheHost(WebLocalFrame*, WebApplicationCacheHostClient* appcacheHostClient)
{
    if (client())
        return client()->createApplicationCacheHost(appcacheHostClient);
    return 0;
}

void WebSharedWorkerImpl::didFinishDocumentLoad(WebLocalFrame* frame)
{
    ASSERT(!m_loadingDocument);
    ASSERT(!m_mainScriptLoader);
    m_mainScriptLoader = Loader::create();
    m_loadingDocument = toWebLocalFrameImpl(frame)->frame()->document();
    m_mainScriptLoader->load(
        m_loadingDocument.get(),
        m_url,
        bind(&WebSharedWorkerImpl::didReceiveScriptLoaderResponse, this),
        bind(&WebSharedWorkerImpl::onScriptLoaderFinished, this));
}

// WorkerReportingProxy --------------------------------------------------------

void WebSharedWorkerImpl::reportException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL)
{
    // Not suppported in SharedWorker.
}

void WebSharedWorkerImpl::reportConsoleMessage(MessageSource source, MessageLevel level, const String& message, int lineNumber, const String& sourceURL)
{
    // Not supported in SharedWorker.
}

void WebSharedWorkerImpl::postMessageToPageInspector(const String& message)
{
    // Note that we need to keep the closure creation on a separate line so
    // that the temporary created by isolatedCopy() will always be destroyed
    // before the copy in the closure is used on the main thread.
    const Closure& boundFunction = bind(&WebSharedWorkerClient::dispatchDevToolsMessage, m_clientWeakPtr, message.isolatedCopy());
    callOnMainThread(boundFunction);
}

void WebSharedWorkerImpl::updateInspectorStateCookie(const String& cookie)
{
    // Note that we need to keep the closure creation on a separate line so
    // that the temporary created by isolatedCopy() will always be destroyed
    // before the copy in the closure is used on the main thread.
    const Closure& boundFunction = bind(&WebSharedWorkerClient::saveDevToolsAgentState, m_clientWeakPtr, cookie.isolatedCopy());
    callOnMainThread(boundFunction);
}

void WebSharedWorkerImpl::workerGlobalScopeClosed()
{
    callOnMainThread(bind(&WebSharedWorkerImpl::workerGlobalScopeClosedOnMainThread, this));
}

void WebSharedWorkerImpl::workerGlobalScopeClosedOnMainThread()
{
    if (client())
        client()->workerContextClosed();

    stopWorkerThread();
}

void WebSharedWorkerImpl::workerGlobalScopeStarted(WorkerGlobalScope*)
{
}

void WebSharedWorkerImpl::workerGlobalScopeDestroyed()
{
    callOnMainThread(bind(&WebSharedWorkerImpl::workerGlobalScopeDestroyedOnMainThread, this));
}

void WebSharedWorkerImpl::workerGlobalScopeDestroyedOnMainThread()
{
    if (client())
        client()->workerContextDestroyed();
    // The lifetime of this proxy is controlled by the worker context.
    delete this;
}

// WorkerLoaderProxy -----------------------------------------------------------

void WebSharedWorkerImpl::postTaskToLoader(PassOwnPtr<ExecutionContextTask> task)
{
    toWebLocalFrameImpl(m_mainFrame)->frame()->document()->postTask(task);
}

bool WebSharedWorkerImpl::postTaskToWorkerGlobalScope(PassOwnPtr<ExecutionContextTask> task)
{
    m_workerThread->runLoop().postTask(task);
    return true;
}

void WebSharedWorkerImpl::connect(WebMessagePortChannel* webChannel)
{
    workerThread()->runLoop().postTask(
        createCallbackTask(&connectTask, adoptPtr(webChannel)));
}

void WebSharedWorkerImpl::connectTask(ExecutionContext* context, PassOwnPtr<WebMessagePortChannel> channel)
{
    // Wrap the passed-in channel in a MessagePort, and send it off via a connect event.
    RefPtrWillBeRawPtr<MessagePort> port = MessagePort::create(*context);
    port->entangle(channel);
    WorkerGlobalScope* workerGlobalScope = toWorkerGlobalScope(context);
    ASSERT_WITH_SECURITY_IMPLICATION(workerGlobalScope->isSharedWorkerGlobalScope());
    workerGlobalScope->dispatchEvent(createConnectEvent(port.release()));
}

void WebSharedWorkerImpl::startWorkerContext(const WebURL& url, const WebString& name, const WebString& contentSecurityPolicy, WebContentSecurityPolicyType policyType)
{
    m_url = url;
    m_name = name;
    m_contentSecurityPolicy = contentSecurityPolicy;
    m_policyType = policyType;
    initializeLoader(url);
}

void WebSharedWorkerImpl::didReceiveScriptLoaderResponse()
{
    InspectorInstrumentation::didReceiveScriptResponse(m_loadingDocument.get(), m_mainScriptLoader->identifier());
    if (client())
        client()->selectAppCacheID(m_mainScriptLoader->appCacheID());
}

static void connectToWorkerContextInspectorTask(ExecutionContext* context, bool)
{
    toWorkerGlobalScope(context)->workerInspectorController()->connectFrontend();
}

void WebSharedWorkerImpl::onScriptLoaderFinished()
{
    ASSERT(m_loadingDocument);
    ASSERT(m_mainScriptLoader);
    if (m_mainScriptLoader->failed() || m_askedToTerminate) {
        m_mainScriptLoader.clear();
        if (client())
            client()->workerScriptLoadFailed();
        return;
    }
    WorkerThreadStartMode startMode = m_pauseWorkerContextOnStart ? PauseWorkerGlobalScopeOnStart : DontPauseWorkerGlobalScopeOnStart;
    OwnPtrWillBeRawPtr<WorkerClients> workerClients = WorkerClients::create();
    provideLocalFileSystemToWorker(workerClients.get(), LocalFileSystemClient::create());
    provideDatabaseClientToWorker(workerClients.get(), DatabaseClientImpl::create());
    WebSecurityOrigin webSecurityOrigin(m_loadingDocument->securityOrigin());
    providePermissionClientToWorker(workerClients.get(), adoptPtr(client()->createWorkerPermissionClientProxy(webSecurityOrigin)));
    OwnPtrWillBeRawPtr<WorkerThreadStartupData> startupData = WorkerThreadStartupData::create(m_url, m_loadingDocument->userAgent(m_url), m_mainScriptLoader->script(), startMode, m_contentSecurityPolicy, static_cast<WebCore::ContentSecurityPolicyHeaderType>(m_policyType), workerClients.release());
    setWorkerThread(SharedWorkerThread::create(m_name, *this, *this, startupData.release()));
    InspectorInstrumentation::scriptImported(m_loadingDocument.get(), m_mainScriptLoader->identifier(), m_mainScriptLoader->script());
    m_mainScriptLoader.clear();

    if (m_attachDevToolsOnStart)
        workerThread()->runLoop().postDebuggerTask(createCallbackTask(connectToWorkerContextInspectorTask, true));

    workerThread()->start();
    if (client())
        client()->workerScriptLoaded();
}

void WebSharedWorkerImpl::terminateWorkerContext()
{
    stopWorkerThread();
}

void WebSharedWorkerImpl::clientDestroyed()
{
    m_client.clear();
}

void WebSharedWorkerImpl::pauseWorkerContextOnStart()
{
    m_pauseWorkerContextOnStart = true;
}

static void resumeWorkerContextTask(ExecutionContext* context, bool)
{
    toWorkerGlobalScope(context)->workerInspectorController()->resume();
}

void WebSharedWorkerImpl::resumeWorkerContext()
{
    m_pauseWorkerContextOnStart = false;
    if (workerThread())
        workerThread()->runLoop().postDebuggerTask(createCallbackTask(resumeWorkerContextTask, true));
}

void WebSharedWorkerImpl::attachDevTools()
{
    if (workerThread())
        workerThread()->runLoop().postDebuggerTask(createCallbackTask(connectToWorkerContextInspectorTask, true));
    else
        m_attachDevToolsOnStart = true;
}

static void reconnectToWorkerContextInspectorTask(ExecutionContext* context, const String& savedState)
{
    WorkerInspectorController* ic = toWorkerGlobalScope(context)->workerInspectorController();
    ic->restoreInspectorStateFromCookie(savedState);
    ic->resume();
}

void WebSharedWorkerImpl::reattachDevTools(const WebString& savedState)
{
    workerThread()->runLoop().postDebuggerTask(createCallbackTask(reconnectToWorkerContextInspectorTask, String(savedState)));
}

static void disconnectFromWorkerContextInspectorTask(ExecutionContext* context, bool)
{
    toWorkerGlobalScope(context)->workerInspectorController()->disconnectFrontend();
}

void WebSharedWorkerImpl::detachDevTools()
{
    m_attachDevToolsOnStart = false;
    workerThread()->runLoop().postDebuggerTask(createCallbackTask(disconnectFromWorkerContextInspectorTask, true));
}

static void dispatchOnInspectorBackendTask(ExecutionContext* context, const String& message)
{
    toWorkerGlobalScope(context)->workerInspectorController()->dispatchMessageFromFrontend(message);
}

void WebSharedWorkerImpl::dispatchDevToolsMessage(const WebString& message)
{
    workerThread()->runLoop().postDebuggerTask(createCallbackTask(dispatchOnInspectorBackendTask, String(message)));
    WorkerDebuggerAgent::interruptAndDispatchInspectorCommands(workerThread());
}

WebSharedWorker* WebSharedWorker::create(WebSharedWorkerClient* client)
{
    return new WebSharedWorkerImpl(client);
}

} // namespace blink
