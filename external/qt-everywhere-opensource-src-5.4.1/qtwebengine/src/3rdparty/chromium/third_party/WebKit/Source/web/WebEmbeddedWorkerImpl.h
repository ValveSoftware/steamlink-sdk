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

#ifndef WebEmbeddedWorkerImpl_h
#define WebEmbeddedWorkerImpl_h

#include "public/web/WebContentSecurityPolicy.h"
#include "public/web/WebEmbeddedWorker.h"
#include "public/web/WebEmbeddedWorkerStartData.h"
#include "public/web/WebFrameClient.h"

namespace WebCore {
class WorkerScriptLoader;
class WorkerThread;
}

namespace blink {

class ServiceWorkerGlobalScopeProxy;
class WebServiceWorkerNetworkProvider;
class WebView;

class WebEmbeddedWorkerImpl FINAL :
    public WebEmbeddedWorker,
    public WebFrameClient {
    WTF_MAKE_NONCOPYABLE(WebEmbeddedWorkerImpl);
public:
    WebEmbeddedWorkerImpl(
        PassOwnPtr<WebServiceWorkerContextClient>,
        PassOwnPtr<WebWorkerPermissionClientProxy>);
    virtual ~WebEmbeddedWorkerImpl();

    // WebEmbeddedWorker overrides.
    virtual void startWorkerContext(const WebEmbeddedWorkerStartData&) OVERRIDE;
    virtual void resumeAfterDownload() OVERRIDE;
    virtual void terminateWorkerContext() OVERRIDE;
    virtual void resumeWorkerContext() OVERRIDE;
    virtual void attachDevTools() OVERRIDE;
    virtual void reattachDevTools(const WebString& savedState) OVERRIDE;
    virtual void detachDevTools() OVERRIDE;
    virtual void dispatchDevToolsMessage(const WebString&) OVERRIDE;


private:
    class Loader;
    class LoaderProxy;

    void prepareShadowPageForLoader();

    // WebFrameClient overrides.
    virtual void willSendRequest(
        WebLocalFrame*, unsigned identifier, WebURLRequest&,
        const WebURLResponse& redirectResponse) OVERRIDE;
    virtual void didFinishDocumentLoad(WebLocalFrame*) OVERRIDE;

    void onScriptLoaderFinished();
    void startWorkerThread();

    WebEmbeddedWorkerStartData m_workerStartData;

    OwnPtr<WebServiceWorkerContextClient> m_workerContextClient;

    // This is kept until startWorkerContext is called, and then passed on
    // to WorkerContext.
    OwnPtr<WebWorkerPermissionClientProxy> m_permissionClient;

    // We retain ownership of this one which is for use on the
    // main thread only.
    OwnPtr<WebServiceWorkerNetworkProvider> m_networkProvider;

    // Kept around only while main script loading is ongoing.
    OwnPtr<Loader> m_mainScriptLoader;

    RefPtr<WebCore::WorkerThread> m_workerThread;
    OwnPtr<LoaderProxy> m_loaderProxy;
    OwnPtr<ServiceWorkerGlobalScopeProxy> m_workerGlobalScopeProxy;

    // 'shadow page' - created to proxy loading requests from the worker.
    // Both WebView and WebFrame objects are close()'ed (where they're
    // deref'ed) when this EmbeddedWorkerImpl is destructed, therefore they
    // are guaranteed to exist while this object is around.
    WebView* m_webView;
    WebFrame* m_mainFrame;

    bool m_askedToTerminate;

    enum {
        DontPauseAfterDownload,
        DoPauseAfterDownload,
        IsPausedAfterDownload
    } m_pauseAfterDownloadState;
};

} // namespace blink

#endif // WebEmbeddedWorkerImpl_h
