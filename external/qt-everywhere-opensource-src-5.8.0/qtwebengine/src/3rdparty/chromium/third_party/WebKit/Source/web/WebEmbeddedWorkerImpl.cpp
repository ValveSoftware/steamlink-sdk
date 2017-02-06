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

#include "web/WebEmbeddedWorkerImpl.h"

#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/dom/SecurityContext.h"
#include "core/fetch/SubstituteData.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerScriptLoader.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "modules/serviceworkers/ServiceWorkerContainerClient.h"
#include "modules/serviceworkers/ServiceWorkerThread.h"
#include "platform/Histogram.h"
#include "platform/SharedBuffer.h"
#include "platform/heap/Handle.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/ContentSecurityPolicyResponseHeaders.h"
#include "platform/network/NetworkUtils.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProvider.h"
#include "public/web/WebConsoleMessage.h"
#include "public/web/WebDevToolsAgent.h"
#include "public/web/WebSettings.h"
#include "public/web/WebView.h"
#include "public/web/WebWorkerContentSettingsClientProxy.h"
#include "public/web/modules/serviceworker/WebServiceWorkerContextClient.h"
#include "public/web/modules/serviceworker/WebServiceWorkerNetworkProvider.h"
#include "web/IndexedDBClientImpl.h"
#include "web/ServiceWorkerGlobalScopeClientImpl.h"
#include "web/ServiceWorkerGlobalScopeProxy.h"
#include "web/WebDataSourceImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WorkerContentSettingsClient.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

WebEmbeddedWorker* WebEmbeddedWorker::create(WebServiceWorkerContextClient* client, WebWorkerContentSettingsClientProxy* contentSettingsClient)
{
    return new WebEmbeddedWorkerImpl(wrapUnique(client), wrapUnique(contentSettingsClient));
}

static HashSet<WebEmbeddedWorkerImpl*>& runningWorkerInstances()
{
    DEFINE_STATIC_LOCAL(HashSet<WebEmbeddedWorkerImpl*>, set, ());
    return set;
}

WebEmbeddedWorkerImpl::WebEmbeddedWorkerImpl(std::unique_ptr<WebServiceWorkerContextClient> client, std::unique_ptr<WebWorkerContentSettingsClientProxy> contentSettingsClient)
    : m_workerContextClient(std::move(client))
    , m_contentSettingsClient(std::move(contentSettingsClient))
    , m_workerInspectorProxy(WorkerInspectorProxy::create())
    , m_webView(nullptr)
    , m_mainFrame(nullptr)
    , m_loadingShadowPage(false)
    , m_askedToTerminate(false)
    , m_pauseAfterDownloadState(DontPauseAfterDownload)
    , m_waitingForDebuggerState(NotWaitingForDebugger)
{
    runningWorkerInstances().add(this);
}

WebEmbeddedWorkerImpl::~WebEmbeddedWorkerImpl()
{
    // Prevent onScriptLoaderFinished from deleting 'this'.
    m_askedToTerminate = true;

    if (m_workerThread)
        m_workerThread->terminateAndWait();

    DCHECK(runningWorkerInstances().contains(this));
    runningWorkerInstances().remove(this);
    DCHECK(m_webView);

    // Detach the client before closing the view to avoid getting called back.
    m_mainFrame->setClient(0);

    if (m_workerGlobalScopeProxy) {
        m_workerGlobalScopeProxy->detach();
        m_workerGlobalScopeProxy.clear();
    }

    m_webView->close();
    m_mainFrame->close();
    if (m_loaderProxy)
        m_loaderProxy->detachProvider(this);
}

void WebEmbeddedWorkerImpl::startWorkerContext(
    const WebEmbeddedWorkerStartData& data)
{
    DCHECK(!m_askedToTerminate);
    DCHECK(!m_mainScriptLoader);
    DCHECK_EQ(m_pauseAfterDownloadState, DontPauseAfterDownload);
    m_workerStartData = data;

    // TODO(mkwst): This really needs to be piped through from the requesting
    // document, like we're doing for SharedWorkers. That turns out to be
    // incredibly convoluted, and since ServiceWorkers are locked to the same
    // origin as the page which requested them, the only time it would come
    // into play is a DNS poisoning attack after the page load. It's something
    // we should fix, but we're taking this shortcut for the prototype.
    //
    // https://crbug.com/590714
    KURL scriptURL = m_workerStartData.scriptURL;
    m_workerStartData.addressSpace = WebAddressSpacePublic;
    if (NetworkUtils::isReservedIPAddress(scriptURL.host()))
        m_workerStartData.addressSpace = WebAddressSpacePrivate;
    if (SecurityOrigin::create(scriptURL)->isLocalhost())
        m_workerStartData.addressSpace = WebAddressSpaceLocal;

    if (data.pauseAfterDownloadMode == WebEmbeddedWorkerStartData::PauseAfterDownload)
        m_pauseAfterDownloadState = DoPauseAfterDownload;
    prepareShadowPageForLoader();
}

void WebEmbeddedWorkerImpl::terminateWorkerContext()
{
    if (m_askedToTerminate)
        return;
    m_askedToTerminate = true;
    if (m_loadingShadowPage) {
        // This deletes 'this'.
        m_workerContextClient->workerContextFailedToStart();
        return;
    }
    if (m_mainScriptLoader) {
        m_mainScriptLoader->cancel();
        m_mainScriptLoader.clear();
        // This deletes 'this'.
        m_workerContextClient->workerContextFailedToStart();
        return;
    }
    if (!m_workerThread) {
        // The worker thread has not been created yet if the worker is asked to
        // terminate during waiting for debugger or paused after download.
        DCHECK(m_workerStartData.waitForDebuggerMode == WebEmbeddedWorkerStartData::WaitForDebugger || m_pauseAfterDownloadState == IsPausedAfterDownload);
        // This deletes 'this'.
        m_workerContextClient->workerContextFailedToStart();
        return;
    }
    m_workerThread->terminate();
    m_workerInspectorProxy->workerThreadTerminated();
}

void WebEmbeddedWorkerImpl::resumeAfterDownload()
{
    DCHECK(!m_askedToTerminate);
    DCHECK_EQ(m_pauseAfterDownloadState, IsPausedAfterDownload);

    m_pauseAfterDownloadState = DontPauseAfterDownload;
    startWorkerThread();
}

void WebEmbeddedWorkerImpl::attachDevTools(const WebString& hostId, int sessionId)
{
    WebDevToolsAgent* devtoolsAgent = m_mainFrame->devToolsAgent();
    if (devtoolsAgent)
        devtoolsAgent->attach(hostId, sessionId);
}

void WebEmbeddedWorkerImpl::reattachDevTools(const WebString& hostId, int sessionId, const WebString& savedState)
{
    WebDevToolsAgent* devtoolsAgent = m_mainFrame->devToolsAgent();
    if (devtoolsAgent)
        devtoolsAgent->reattach(hostId, sessionId, savedState);
    resumeStartup();
}

void WebEmbeddedWorkerImpl::detachDevTools()
{
    WebDevToolsAgent* devtoolsAgent = m_mainFrame->devToolsAgent();
    if (devtoolsAgent)
        devtoolsAgent->detach();
}

void WebEmbeddedWorkerImpl::dispatchDevToolsMessage(int sessionId, int callId, const WebString& method, const WebString& message)
{
    if (m_askedToTerminate)
        return;
    WebDevToolsAgent* devtoolsAgent = m_mainFrame->devToolsAgent();
    if (devtoolsAgent)
        devtoolsAgent->dispatchOnInspectorBackend(sessionId, callId, method, message);
}

void WebEmbeddedWorkerImpl::addMessageToConsole(const WebConsoleMessage& message)
{
    MessageLevel webCoreMessageLevel;
    switch (message.level) {
    case WebConsoleMessage::LevelDebug:
        webCoreMessageLevel = DebugMessageLevel;
        break;
    case WebConsoleMessage::LevelLog:
        webCoreMessageLevel = LogMessageLevel;
        break;
    case WebConsoleMessage::LevelWarning:
        webCoreMessageLevel = WarningMessageLevel;
        break;
    case WebConsoleMessage::LevelError:
        webCoreMessageLevel = ErrorMessageLevel;
        break;
    default:
        NOTREACHED();
        return;
    }

    m_mainFrame->frame()->document()->addConsoleMessage(ConsoleMessage::create(OtherMessageSource, webCoreMessageLevel, message.text, SourceLocation::create(message.url, message.lineNumber, message.columnNumber, nullptr)));
}

void WebEmbeddedWorkerImpl::postMessageToPageInspector(const String& message)
{
    m_workerInspectorProxy->dispatchMessageFromWorker(message);
}

void WebEmbeddedWorkerImpl::postTaskToLoader(std::unique_ptr<ExecutionContextTask> task)
{
    m_mainFrame->frame()->document()->postTask(BLINK_FROM_HERE, std::move(task));
}

bool WebEmbeddedWorkerImpl::postTaskToWorkerGlobalScope(std::unique_ptr<ExecutionContextTask> task)
{
    if (m_askedToTerminate || !m_workerThread)
        return false;

    m_workerThread->postTask(BLINK_FROM_HERE, std::move(task));
    return !m_workerThread->terminated();
}

void WebEmbeddedWorkerImpl::prepareShadowPageForLoader()
{
    // Create 'shadow page', which is never displayed and is used mainly to
    // provide a context for loading on the main thread.
    //
    // FIXME: This does mostly same as WebSharedWorkerImpl::initializeLoader.
    // This code, and probably most of the code in this class should be shared
    // with SharedWorker.
    DCHECK(!m_webView);
    m_webView = WebView::create(nullptr, WebPageVisibilityStateVisible);
    WebSettings* settings = m_webView->settings();
    // FIXME: http://crbug.com/363843. This needs to find a better way to
    // not create graphics layers.
    settings->setAcceleratedCompositingEnabled(false);
    // Currently we block all mixed-content requests from a ServiceWorker.
    // FIXME: When we support FetchEvent.default(), we should relax this
    // restriction.
    settings->setStrictMixedContentChecking(true);
    settings->setAllowDisplayOfInsecureContent(false);
    settings->setAllowRunningOfInsecureContent(false);
    settings->setDataSaverEnabled(m_workerStartData.dataSaverEnabled);
    m_mainFrame = toWebLocalFrameImpl(WebLocalFrame::create(WebTreeScopeType::Document, this));
    m_webView->setMainFrame(m_mainFrame.get());
    m_mainFrame->setDevToolsAgentClient(this);

    // If we were asked to wait for debugger then it is the good time to do that.
    m_workerContextClient->workerReadyForInspection();
    if (m_workerStartData.waitForDebuggerMode == WebEmbeddedWorkerStartData::WaitForDebugger) {
        m_waitingForDebuggerState = WaitingForDebugger;
        return;
    }

    loadShadowPage();
}

void WebEmbeddedWorkerImpl::loadShadowPage()
{
    // Construct substitute data source for the 'shadow page'. We only need it
    // to have same origin as the worker so the loading checks work correctly.
    CString content("");
    RefPtr<SharedBuffer> buffer(SharedBuffer::create(content.data(), content.length()));
    m_loadingShadowPage = true;
    m_mainFrame->frame()->loader().load(FrameLoadRequest(0, ResourceRequest(m_workerStartData.scriptURL), SubstituteData(buffer, "text/html", "UTF-8", KURL())));
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
    DCHECK(!m_mainScriptLoader);
    DCHECK(!m_networkProvider);
    DCHECK(m_mainFrame);
    DCHECK(m_workerContextClient);
    DCHECK(m_loadingShadowPage);
    DCHECK(!m_askedToTerminate);
    m_loadingShadowPage = false;
    m_networkProvider = wrapUnique(m_workerContextClient->createServiceWorkerNetworkProvider(frame->dataSource()));
    m_mainScriptLoader = WorkerScriptLoader::create();
    m_mainScriptLoader->setRequestContext(WebURLRequest::RequestContextServiceWorker);
    m_mainScriptLoader->loadAsynchronously(
        *m_mainFrame->frame()->document(),
        m_workerStartData.scriptURL,
        DenyCrossOriginRequests,
        m_workerStartData.addressSpace,
        nullptr,
        bind(&WebEmbeddedWorkerImpl::onScriptLoaderFinished, WTF::unretained(this)));
    // Do nothing here since onScriptLoaderFinished() might have been already
    // invoked and |this| might have been deleted at this point.
}

void WebEmbeddedWorkerImpl::sendProtocolMessage(int sessionId, int callId, const WebString& message, const WebString& state)
{
    m_workerContextClient->sendDevToolsMessage(sessionId, callId, message, state);
}

void WebEmbeddedWorkerImpl::resumeStartup()
{
    bool wasWaiting = (m_waitingForDebuggerState == WaitingForDebugger);
    m_waitingForDebuggerState = NotWaitingForDebugger;
    if (wasWaiting)
        loadShadowPage();
}

WebDevToolsAgentClient::WebKitClientMessageLoop* WebEmbeddedWorkerImpl::createClientMessageLoop()
{
    return m_workerContextClient->createDevToolsMessageLoop();
}

void WebEmbeddedWorkerImpl::onScriptLoaderFinished()
{
    DCHECK(m_mainScriptLoader);
    if (m_askedToTerminate)
        return;

    // The browser is expected to associate a registration and then load the
    // script. If there's no associated registration, the browser could not
    // successfully handle the SetHostedVersionID IPC, and the script load came
    // through the normal network stack rather than through service worker
    // loading code.
    if (!m_workerContextClient->hasAssociatedRegistration() || m_mainScriptLoader->failed()) {
        m_mainScriptLoader.clear();
        // This deletes 'this'.
        m_workerContextClient->workerContextFailedToStart();
        return;
    }
    m_workerContextClient->workerScriptLoaded();

    DEFINE_STATIC_LOCAL(CustomCountHistogram, scriptSizeHistogram, ("ServiceWorker.ScriptSize", 1000, 5000000, 50));
    scriptSizeHistogram.count(m_mainScriptLoader->script().length());
    if (m_mainScriptLoader->cachedMetadata()) {
        DEFINE_STATIC_LOCAL(CustomCountHistogram, scriptCachedMetadataSizeHistogram, ("ServiceWorker.ScriptCachedMetadataSize", 1000, 50000000, 50));
        scriptCachedMetadataSizeHistogram.count(m_mainScriptLoader->cachedMetadata()->size());
    }

    if (m_pauseAfterDownloadState == DoPauseAfterDownload) {
        m_pauseAfterDownloadState = IsPausedAfterDownload;
        return;
    }
    startWorkerThread();
}

void WebEmbeddedWorkerImpl::startWorkerThread()
{
    DCHECK_EQ(m_pauseAfterDownloadState, DontPauseAfterDownload);
    DCHECK(!m_askedToTerminate);

    Document* document = m_mainFrame->frame()->document();

    // FIXME: this document's origin is pristine and without any extra privileges. (crbug.com/254993)
    SecurityOrigin* starterOrigin = document->getSecurityOrigin();

    WorkerClients* workerClients = WorkerClients::create();
    provideContentSettingsClientToWorker(workerClients, std::move(m_contentSettingsClient));
    provideIndexedDBClientToWorker(workerClients, IndexedDBClientImpl::create());
    provideServiceWorkerGlobalScopeClientToWorker(workerClients, ServiceWorkerGlobalScopeClientImpl::create(*m_workerContextClient));
    provideServiceWorkerContainerClientToWorker(workerClients, wrapUnique(m_workerContextClient->createServiceWorkerProvider()));

    // We need to set the CSP to both the shadow page's document and the ServiceWorkerGlobalScope.
    document->initContentSecurityPolicy(m_mainScriptLoader->releaseContentSecurityPolicy());
    if (!m_mainScriptLoader->referrerPolicy().isNull())
        document->parseAndSetReferrerPolicy(m_mainScriptLoader->referrerPolicy());

    KURL scriptURL = m_mainScriptLoader->url();
    WorkerThreadStartMode startMode = m_workerInspectorProxy->workerStartMode(document);

    std::unique_ptr<WorkerThreadStartupData> startupData = WorkerThreadStartupData::create(
        scriptURL,
        m_workerStartData.userAgent,
        m_mainScriptLoader->script(),
        m_mainScriptLoader->releaseCachedMetadata(),
        startMode,
        document->contentSecurityPolicy()->headers().get(),
        m_mainScriptLoader->referrerPolicy(),
        starterOrigin,
        workerClients,
        m_mainScriptLoader->responseAddressSpace(),
        m_mainScriptLoader->originTrialTokens(),
        static_cast<V8CacheOptions>(m_workerStartData.v8CacheOptions));

    m_mainScriptLoader.clear();

    m_workerGlobalScopeProxy = ServiceWorkerGlobalScopeProxy::create(*this, *document, *m_workerContextClient);
    m_loaderProxy = WorkerLoaderProxy::create(this);
    m_workerThread = ServiceWorkerThread::create(m_loaderProxy, *m_workerGlobalScopeProxy);
    m_workerThread->start(std::move(startupData));
    m_workerInspectorProxy->workerThreadCreated(document, m_workerThread.get(), scriptURL);
}

} // namespace blink
