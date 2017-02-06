/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009, 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef WorkerScriptLoader_h
#define WorkerScriptLoader_h

#include "core/CoreExport.h"
#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/Allocator.h"
#include "wtf/Functional.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/StringBuilder.h"
#include <memory>

namespace blink {

class ContentSecurityPolicy;
class ResourceRequest;
class ResourceResponse;
class ExecutionContext;
class TextResourceDecoder;

class CORE_EXPORT WorkerScriptLoader final : public RefCounted<WorkerScriptLoader>, public ThreadableLoaderClient {
    USING_FAST_MALLOC(WorkerScriptLoader);
public:
    static PassRefPtr<WorkerScriptLoader> create()
    {
        return adoptRef(new WorkerScriptLoader());
    }

    void loadSynchronously(ExecutionContext&, const KURL&, CrossOriginRequestPolicy, WebAddressSpace);
    // TODO: |finishedCallback| is not currently guaranteed to be invoked if
    // used from worker context and the worker shuts down in the middle of an
    // operation. This will cause leaks when we support nested workers.
    // Note that callbacks could be invoked before loadAsynchronously() returns.
    void loadAsynchronously(ExecutionContext&, const KURL&, CrossOriginRequestPolicy, WebAddressSpace, std::unique_ptr<WTF::Closure> responseCallback, std::unique_ptr<WTF::Closure> finishedCallback);

    // This will immediately invoke |finishedCallback| if loadAsynchronously()
    // is in progress.
    void cancel();

    String script();
    const KURL& url() const { return m_url; }
    const KURL& responseURL() const;
    bool failed() const { return m_failed; }
    unsigned long identifier() const { return m_identifier; }
    long long appCacheID() const { return m_appCacheID; }

    std::unique_ptr<Vector<char>> releaseCachedMetadata() { return std::move(m_cachedMetadata); }
    const Vector<char>* cachedMetadata() const { return m_cachedMetadata.get(); }

    ContentSecurityPolicy* contentSecurityPolicy() { return m_contentSecurityPolicy.get(); }
    ContentSecurityPolicy* releaseContentSecurityPolicy() { return m_contentSecurityPolicy.release(); }

    String referrerPolicy() { return m_referrerPolicy; }

    WebAddressSpace responseAddressSpace() const { return m_responseAddressSpace; }

    const Vector<String>* originTrialTokens() const { return m_originTrialTokens.get(); }

    // ThreadableLoaderClient
    void didReceiveResponse(unsigned long /*identifier*/, const ResourceResponse&, std::unique_ptr<WebDataConsumerHandle>) override;
    void didReceiveData(const char* data, unsigned dataLength) override;
    void didReceiveCachedMetadata(const char*, int /*dataLength*/) override;
    void didFinishLoading(unsigned long identifier, double) override;
    void didFail(const ResourceError&) override;
    void didFailRedirectCheck() override;

    void setRequestContext(WebURLRequest::RequestContext requestContext) { m_requestContext = requestContext; }

private:
    friend class WTF::RefCounted<WorkerScriptLoader>;

    WorkerScriptLoader();
    ~WorkerScriptLoader() override;

    ResourceRequest createResourceRequest(WebAddressSpace);
    void notifyError();
    void notifyFinished();

    void processContentSecurityPolicy(const ResourceResponse&);

    // Callbacks for loadAsynchronously().
    std::unique_ptr<WTF::Closure> m_responseCallback;
    std::unique_ptr<WTF::Closure> m_finishedCallback;

    std::unique_ptr<ThreadableLoader> m_threadableLoader;
    String m_responseEncoding;
    std::unique_ptr<TextResourceDecoder> m_decoder;
    StringBuilder m_script;
    KURL m_url;
    KURL m_responseURL;
    bool m_failed;
    bool m_needToCancel;
    unsigned long m_identifier;
    long long m_appCacheID;
    std::unique_ptr<Vector<char>> m_cachedMetadata;
    WebURLRequest::RequestContext m_requestContext;
    Persistent<ContentSecurityPolicy> m_contentSecurityPolicy;
    WebAddressSpace m_responseAddressSpace;
    std::unique_ptr<Vector<String>> m_originTrialTokens;
    String m_referrerPolicy;
};

} // namespace blink

#endif // WorkerScriptLoader_h
