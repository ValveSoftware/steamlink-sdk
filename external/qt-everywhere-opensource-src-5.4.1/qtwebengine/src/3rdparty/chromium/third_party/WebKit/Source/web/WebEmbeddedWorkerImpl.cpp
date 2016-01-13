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

#include "config.h"
#include "web/WebEmbeddedWorkerImpl.h"

#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/inspector/WorkerDebuggerAgent.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/SubstituteData.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerScriptLoader.h"
#include "core/workers/WorkerScriptLoaderClient.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "modules/serviceworkers/ServiceWorkerThread.h"
#include "platform/NotImplemented.h"
#include "platform/SharedBuffer.h"
#include "platform/heap/Handle.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "public/web/WebServiceWorkerContextClient.h"
#include "public/web/WebServiceWorkerNetworkProvider.h"
#include "public/web/WebSettings.h"
#include "public/web/WebView.h"
#include "public/web/WebWorkerPermissionClientProxy.h"
#include "web/ServiceWorkerGlobalScopeClientImpl.h"
#include "web/ServiceWorkerGlobalScopeProxy.h"
#include "web/WebDataSourceImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WorkerPermissionClient.h"
#include "wtf/Functional.h"

using namespace WebCore;

namespace blink {

// A thin wrapper for one-off script loading.
class WebEmbeddedWorkerImpl::Loader : public WorkerScriptLoaderClient {
public:
    static PassOwnPtr<Loader> create()
    {
        return adoptPtr(new Loader());
    }

    virtual ~Loader()
    {
        m_scriptLoader->setClient(0);
    }

    void load(ExecutionContext* loadingContext, const KURL& scriptURL, const Closure& callback)
    {
        ASSERT(loadingContext);
        m_callback = callback;
        m_scriptLoader->setTargetType(ResourceRequest::TargetIsServiceWorker);
        m_scriptLoader->loadAsynchronously(
            *loadingContext, scriptURL, DenyCrossOriginRequests, this);
    }

    virtual void notifyFinished() OVERRIDE
    {
        m_callback();
    }

    void cancel()
    {
        m_scriptLoader->cancel();
    }

    bool failed() const { return m_scriptLoader->failed(); }
    const KURL& url() const { return m_scriptLoader->responseURL(); }
    String script() const { return m_scriptLoader->script(); }

private:
    Loader() : m_scriptLoader(WorkerScriptLoader::create())
    {
    }

    RefPtr<WorkerScriptLoader> m_scriptLoader;
    Closure m_callback;
};

class WebEmbeddedWorkerImpl::LoaderProxy : public WorkerLoaderProxy {
public:
    static PassOwnPtr<LoaderProxy> create(WebEmbeddedWorkerImpl& embeddedWorker)
    {
        return adoptPtr(new LoaderProxy(embeddedWorker));
    }

    virtual void postTaskToLoader(PassOwnPtr<ExecutionContextTask> task) OVERRIDE
    {
        toWebLocalFrameImpl(m_embeddedWorker.m_mainFrame)->frame()->document()->postTask(task);
    }

    virtual bool postTaskToWorkerGlobalScope(PassOwnPtr<ExecutionContextTask> task) OVERRIDE
    {
        if (m_embeddedWorker.m_askedToTerminate || !m_embeddedWorker.m_workerThread)
            return false;
        return m_embeddedWorker.m_workerThread->runLoop().postTask(task);
    }

private:
    explicit LoaderProxy(WebEmbeddedWorkerImpl& embeddedWorker)
        : m_embeddedWorker(embeddedWorker)
    {
    }

    // Not owned, embedded worker owns this.
    WebEmbeddedWorkerImpl& m_embeddedWorker;
};

WebEmbeddedWorker* WebEmbeddedWorker::create(
    WebServiceWorkerContextClient* client,
    WebWorkerPermissionClientProxy* permissionClient)
{
    return new WebEmbeddedWorkerImpl(adoptPtr(client), adoptPtr(permissionClient));
}

WebEmbeddedWorkerImpl::WebEmbeddedWorkerImpl(
    PassOwnPtr<WebServiceWorkerContextClient> client,
    PassOwnPtr<WebWorkerPermissionClientProxy> permissionClient)
    : m_workerContextClient(client)
    , m_permissionClient(permissionClient)
    , m_webView(0)
    , m_mainFrame(0)
    , m_askedToTerminate(false)
    , m_pauseAfterDownloadState(DontPauseAfterDownload)
{
}

WebEmbeddedWorkerImpl::~WebEmbeddedWorkerImpl()
{
    ASSERT(m_webView);

    // Detach the client before closing the view to avoid getting called back.
    toWebLocalFrameImpl(m_mainFrame)->setClient(0);

    m_webView->close();
    m_mainFrame->close();
}

void WebEmbeddedWorkerImpl::startWorkerContext(
    const WebEmbeddedWorkerStartData& data)
{
    ASSERT(!m_askedToTerminate);
    ASSERT(!m_mainScriptLoader);
    ASSERT(m_pauseAfterDownloadState == DontPauseAfterDownload);
    m_workerStartData = data;
    if (data.pauseAfterDownloadMode == WebEmbeddedWorkerStartData::PauseAfterDownload)
        m_pauseAfterDownloadState = DoPauseAfterDownload;
    prepareShadowPageForLoader();
}

void WebEmbeddedWorkerImpl::terminateWorkerContext()
{
    if (m_askedToTerminate)
        return;
    m_askedToTerminate = true;
    if (m_mainScriptLoader)
        m_mainScriptLoader->cancel();
    if (m_workerThread)
        m_workerThread->stop();
}

namespace {

void resumeWorkerContextTask(ExecutionContext* context, bool)
{
    toWorkerGlobalScope(context)->workerInspectorController()->resume();
}

void connectToWorkerContextInspectorTask(ExecutionContext* context, bool)
{
    toWorkerGlobalScope(context)->workerInspectorController()->connectFrontend();
}

void reconnectToWorkerContextInspectorTask(ExecutionContext* context, const String& savedState)
{
    WorkerInspectorController* ic = toWorkerGlobalScope(context)->workerInspectorController();
    ic->restoreInspectorStateFromCookie(savedState);
    ic->resume();
}

void disconnectFromWorkerContextInspectorTask(ExecutionContext* context, bool)
{
    toWorkerGlobalScope(context)->workerInspectorController()->disconnectFrontend();
}

void dispatchOnInspectorBackendTask(ExecutionContext* context, const String& message)
{
    toWorkerGlobalScope(context)->workerInspectorController()->dispatchMessageFromFrontend(message);
}

} // namespace

void WebEmbeddedWorkerImpl::resumeAfterDownload()
{
    bool wasPaused = (m_pauseAfterDownloadState == IsPausedAfterDownload);
    m_pauseAfterDownloadState = DontPauseAfterDownload;
    if (wasPaused)
        startWorkerThread();
}

void WebEmbeddedWorkerImpl::resumeWorkerContext()
{
    if (m_workerThread)
        m_workerThread->runLoop().postDebuggerTask(createCallbackTask(resumeWorkerContextTask, true));
}

void WebEmbeddedWorkerImpl::attachDevTools()
{
    if (m_workerThread)
        m_workerThread->runLoop().postDebuggerTask(createCallbackTask(connectToWorkerContextInspectorTask, true));
}

void WebEmbeddedWorkerImpl::reattachDevTools(const WebString& savedState)
{
    m_workerThread->runLoop().postDebuggerTask(createCallbackTask(reconnectToWorkerContextInspectorTask, String(savedState)));
}

void WebEmbeddedWorkerImpl::detachDevTools()
{
    m_workerThread->runLoop().postDebuggerTask(createCallbackTask(disconnectFromWorkerContextInspectorTask, true));
}

void WebEmbeddedWorkerImpl::dispatchDevToolsMessage(const WebString& message)
{
    m_workerThread->runLoop().postDebuggerTask(createCallbackTask(dispatchOnInspectorBackendTask, String(message)));
    WorkerDebuggerAgent::interruptAndDispatchInspectorCommands(m_workerThread.get());
}

void WebEmbeddedWorkerImpl::prepareShadowPageForLoader()
{
    // Create 'shadow page', which is never displayed and is used mainly to
    // provide a context for loading on the main thread.
    //
    // FIXME: This does mostly same as WebSharedWorkerImpl::initializeLoader.
    // This code, and probably most of the code in this class should be shared
    // with SharedWorker.
    ASSERT(!m_webView);
    m_webView = WebView::create(0);
    // FIXME: http://crbug.com/363843. This needs to find a better way to
    // not create graphics layers.
    m_webView->settings()->setAcceleratedCompositingEnabled(false);
    m_mainFrame = WebLocalFrame::create(this);
    m_webView->setMainFrame(m_mainFrame);

    WebLocalFrameImpl* webFrame = toWebLocalFrameImpl(m_webView->mainFrame());

    // Construct substitute data source for the 'shadow page'. We only need it
    // to have same origin as the worker so the loading checks work correctly.
    CString content("");
    int length = static_cast<int>(content.length());
    RefPtr<SharedBuffer> buffer(SharedBuffer::create(content.data(), length));
    webFrame->frame()->loader().load(FrameLoadRequest(0, ResourceRequest(m_workerStartData.scriptURL), SubstituteData(buffer, "text/html", "UTF-8", KURL())));
}

void WebEmbeddedWorkerImpl::willSendRequest(
    WebLocalFrame* frame, unsigned, WebURLRequest& request,
    const WebURLResponse& redirectResponse)
{
    if (m_networkProvider)
        m_networkProvider->willSendRequest(frame->dataSource(), request);
}

void WebEmbeddedWorkerImpl::didFinishDocumentLoad(WebLocalFrame* frame)
{
    ASSERT(!m_mainScriptLoader);
    ASSERT(!m_networkProvider);
    ASSERT(m_mainFrame);
    ASSERT(m_workerContextClient);
    m_networkProvider = adoptPtr(m_workerContextClient->createServiceWorkerNetworkProvider(frame->dataSource()));
    m_mainScriptLoader = Loader::create();
    m_mainScriptLoader->load(
        toWebLocalFrameImpl(m_mainFrame)->frame()->document(),
        m_workerStartData.scriptURL,
        bind(&WebEmbeddedWorkerImpl::onScriptLoaderFinished, this));
}

void WebEmbeddedWorkerImpl::onScriptLoaderFinished()
{
    ASSERT(m_mainScriptLoader);

    if (m_mainScriptLoader->failed() || m_askedToTerminate) {
        m_mainScriptLoader.clear();
        // This may delete 'this'.
        m_workerContextClient->workerContextFailedToStart();
        return;
    }

    if (m_pauseAfterDownloadState == DoPauseAfterDownload) {
        m_pauseAfterDownloadState = IsPausedAfterDownload;
        m_workerContextClient->didPauseAfterDownload();
        return;
    }
    startWorkerThread();
}

void WebEmbeddedWorkerImpl::startWorkerThread()
{
    ASSERT(m_pauseAfterDownloadState == DontPauseAfterDownload);
    if (m_askedToTerminate)
        return;

    // FIXME: startMode is deprecated, switch to waitForDebuggerMode once chromium is setting that value.
    WorkerThreadStartMode startMode =
        (m_workerStartData.startMode == WebEmbeddedWorkerStartModePauseOnStart)
        ? PauseWorkerGlobalScopeOnStart : DontPauseWorkerGlobalScopeOnStart;

    OwnPtrWillBeRawPtr<WorkerClients> workerClients = WorkerClients::create();
    providePermissionClientToWorker(workerClients.get(), m_permissionClient.release());
    provideServiceWorkerGlobalScopeClientToWorker(workerClients.get(), ServiceWorkerGlobalScopeClientImpl::create(*m_workerContextClient));

    OwnPtrWillBeRawPtr<WorkerThreadStartupData> startupData =
        WorkerThreadStartupData::create(
            m_mainScriptLoader->url(),
            m_workerStartData.userAgent,
            m_mainScriptLoader->script(),
            startMode,
            // FIXME: fill appropriate CSP info and policy type.
            String(),
            ContentSecurityPolicyHeaderTypeEnforce,
            workerClients.release());

    m_mainScriptLoader.clear();

    m_workerGlobalScopeProxy = ServiceWorkerGlobalScopeProxy::create(*this, *toWebLocalFrameImpl(m_mainFrame)->frame()->document(), *m_workerContextClient);
    m_loaderProxy = LoaderProxy::create(*this);

    m_workerThread = ServiceWorkerThread::create(*m_loaderProxy, *m_workerGlobalScopeProxy, startupData.release());
    m_workerThread->start();
}

} // namespace blink
