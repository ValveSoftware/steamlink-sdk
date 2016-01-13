// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "FetchManager.h"

#include "bindings/v8/ScriptPromiseResolverWithContext.h"
#include "bindings/v8/ScriptState.h"
#include "bindings/v8/V8ThrowException.h"
#include "core/dom/ExceptionCode.h"
#include "core/fileapi/Blob.h"
#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "modules/serviceworkers/Response.h"
#include "platform/network/ResourceRequest.h"
#include "wtf/HashSet.h"

namespace WebCore {

class FetchManager::Loader : public ThreadableLoaderClient {
public:
    Loader(ExecutionContext*, FetchManager*, PassRefPtr<ScriptPromiseResolverWithContext>, PassOwnPtr<ResourceRequest>);
    ~Loader();
    virtual void didReceiveResponse(unsigned long, const ResourceResponse&);
    virtual void didFinishLoading(unsigned long, double);
    virtual void didFail(const ResourceError&);
    virtual void didFailAccessControlCheck(const ResourceError&);
    virtual void didFailRedirectCheck();
    virtual void didDownloadData(int);

    void start();
    void cleanup();

private:
    void failed();
    void notifyFinished();

    ExecutionContext* m_executionContext;
    FetchManager* m_fetchManager;
    RefPtr<ScriptPromiseResolverWithContext> m_resolver;
    OwnPtr<ResourceRequest> m_request;
    RefPtr<ThreadableLoader> m_loader;
    ResourceResponse m_response;
    long long m_downloadedBlobLength;
    bool m_failed;
};

FetchManager::Loader::Loader(ExecutionContext* executionContext, FetchManager* fetchManager, PassRefPtr<ScriptPromiseResolverWithContext> resolver, PassOwnPtr<ResourceRequest> request)
    : m_executionContext(executionContext)
    , m_fetchManager(fetchManager)
    , m_resolver(resolver)
    , m_request(request)
    , m_downloadedBlobLength(0)
    , m_failed(false)
{
}

FetchManager::Loader::~Loader()
{
    if (m_loader)
        m_loader->cancel();
}

void FetchManager::Loader::didReceiveResponse(unsigned long, const ResourceResponse& response)
{
    m_response = response;
}

void FetchManager::Loader::didFinishLoading(unsigned long, double)
{
    OwnPtr<BlobData> blobData = BlobData::create();
    String filePath = m_response.downloadedFilePath();
    if (!filePath.isEmpty() && m_downloadedBlobLength) {
        blobData->appendFile(filePath);
        // FIXME: Set the ContentType correctly.
    }
    Dictionary options;
    // FIXME: fill options.
    RefPtrWillBeRawPtr<Blob> blob = Blob::create(BlobDataHandle::create(blobData.release(), m_downloadedBlobLength));
    // FIXME: Handle response status correctly.
    m_resolver->resolve(Response::create(blob.get(), options));
    notifyFinished();
}

void FetchManager::Loader::didFail(const ResourceError& error)
{
    failed();
}

void FetchManager::Loader::didFailAccessControlCheck(const ResourceError& error)
{
    failed();
}

void FetchManager::Loader::didFailRedirectCheck()
{
    failed();
}

void FetchManager::Loader::didDownloadData(int dataLength)
{
    m_downloadedBlobLength += dataLength;
}

void FetchManager::Loader::start()
{
    m_request->setDownloadToFile(true);
    ThreadableLoaderOptions options;
    // FIXME: Fill options.
    ResourceLoaderOptions resourceLoaderOptions;
    resourceLoaderOptions.dataBufferingPolicy = DoNotBufferData;
    // FIXME: Fill resourceLoaderOptions.
    m_loader = ThreadableLoader::create(*m_executionContext, this, *m_request, options, resourceLoaderOptions);
}

void FetchManager::Loader::cleanup()
{
    // Prevent notification
    m_fetchManager = 0;

    if (m_loader) {
        m_loader->cancel();
        m_loader.clear();
    }
}

void FetchManager::Loader::failed()
{
    if (m_failed)
        return;
    m_failed = true;
    ScriptState* state = m_resolver->scriptState();
    ScriptState::Scope scope(state);
    m_resolver->reject(V8ThrowException::createTypeError("Failed to fetch", state->isolate()));
    notifyFinished();
}

void FetchManager::Loader::notifyFinished()
{
    if (m_fetchManager)
        m_fetchManager->onLoaderFinished(this);
}

FetchManager::FetchManager(ExecutionContext* executionContext)
    : m_executionContext(executionContext)
{
}

FetchManager::~FetchManager()
{
    for (HashSet<OwnPtr<Loader> >::iterator it = m_loaders.begin(); it != m_loaders.end(); ++it) {
        (*it)->cleanup();
    }
}

ScriptPromise FetchManager::fetch(ScriptState* scriptState, PassOwnPtr<ResourceRequest> request)
{
    RefPtr<ScriptPromiseResolverWithContext> resolver = ScriptPromiseResolverWithContext::create(scriptState);
    ScriptPromise promise = resolver->promise();

    OwnPtr<Loader> loader(adoptPtr(new Loader(m_executionContext, this, resolver.release(), request)));
    (*m_loaders.add(loader.release()).storedValue)->start();
    return promise;
}

void FetchManager::onLoaderFinished(Loader* loader)
{
    m_loaders.remove(loader);
}

} // namespace WebCore
