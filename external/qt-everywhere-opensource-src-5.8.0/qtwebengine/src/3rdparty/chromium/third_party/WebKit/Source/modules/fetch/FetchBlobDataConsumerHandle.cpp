// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FetchBlobDataConsumerHandle.h"

#include "core/dom/ExecutionContext.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "modules/fetch/CompositeDataConsumerHandle.h"
#include "modules/fetch/CrossThreadHolder.h"
#include "modules/fetch/DataConsumerHandleUtil.h"
#include "platform/blob/BlobRegistry.h"
#include "platform/blob/BlobURL.h"
#include "platform/network/ResourceRequest.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

using Result = FetchBlobDataConsumerHandle::Result;

namespace {

// Object graph:
//                                           +-------------+
//                                           |ReaderContext|
//   +-------------+  +-----------+   +---+  |             |
//   |LoaderContext|<-|CTH::Bridge|<->|CTH|<-|             |
//   +-------------+  +-----------+   +---+  +-------------+
//               |
//               +--> ThreadableLoader
//
// When the loader thread is stopped, CrossThreadHolder::Bridge and
// LoaderContext (and thus ThreadableLoader) is destructed:
//                                           +-------------+
//                                           |ReaderContext|
//                                    +---+  |             |
//                                    |CTH|<-|             |
//                                    +---+  +-------------+
// and the rest will be destructed when ReaderContext is destructed.
//
// When ReaderContext is destructed, CrossThreadHolder is destructed:
//
//  +-------------+  +-----------+
//  |LoaderContext|<-|CTH::Bridge|
//  +-------------+  +-----------+
//               |
//               +--> ThreadableLoader
// and the rest will be shortly destructed when CrossThreadHolder::Bridge
// is garbage collected.

// LoaderContext is created and destructed on the same thread
// (call this thread loader thread).
// All methods must be called on the loader thread.
class LoaderContext {
public:
    virtual ~LoaderContext() { }
    virtual void start(ExecutionContext*) = 0;
};

// All methods must be called on the loader thread.
class BlobLoaderContext final
    : public LoaderContext
    , public ThreadableLoaderClient {
public:
    BlobLoaderContext(CompositeDataConsumerHandle::Updater* updater, PassRefPtr<BlobDataHandle> blobDataHandle, FetchBlobDataConsumerHandle::LoaderFactory* loaderFactory)
        : m_updater(updater)
        , m_blobDataHandle(blobDataHandle)
        , m_loaderFactory(loaderFactory)
        , m_receivedResponse(false) { }

    ~BlobLoaderContext() override
    {
        if (m_loader && !m_receivedResponse)
            m_updater->update(createUnexpectedErrorDataConsumerHandle());
        if (m_loader) {
            m_loader->cancel();
            m_loader.reset();
        }
        if (!m_registeredBlobURL.isEmpty())
            BlobRegistry::revokePublicBlobURL(m_registeredBlobURL);
    }

    void start(ExecutionContext* executionContext) override
    {
        ASSERT(executionContext->isContextThread());
        ASSERT(!m_loader);

        m_registeredBlobURL = BlobURL::createPublicURL(executionContext->getSecurityOrigin());
        if (m_registeredBlobURL.isEmpty()) {
            m_updater->update(createUnexpectedErrorDataConsumerHandle());
            return;
        }
        BlobRegistry::registerPublicBlobURL(executionContext->getSecurityOrigin(), m_registeredBlobURL, m_blobDataHandle);

        m_loader = createLoader(executionContext, this);
        ASSERT(m_loader);

        ResourceRequest request(m_registeredBlobURL);
        request.setRequestContext(WebURLRequest::RequestContextInternal);
        request.setUseStreamOnResponse(true);
        // We intentionally skip 'setExternalRequestStateFromRequestorAddressSpace', as 'data:' can never be external.
        m_loader->start(request);
    }

private:
    std::unique_ptr<ThreadableLoader> createLoader(ExecutionContext* executionContext, ThreadableLoaderClient* client) const
    {
        ThreadableLoaderOptions options;
        options.preflightPolicy = ConsiderPreflight;
        options.crossOriginRequestPolicy = DenyCrossOriginRequests;
        options.contentSecurityPolicyEnforcement = DoNotEnforceContentSecurityPolicy;
        options.initiator = FetchInitiatorTypeNames::internal;

        ResourceLoaderOptions resourceLoaderOptions;
        resourceLoaderOptions.dataBufferingPolicy = DoNotBufferData;

        return m_loaderFactory->create(*executionContext, client, options, resourceLoaderOptions);
    }

    // ThreadableLoaderClient
    void didReceiveResponse(unsigned long, const ResourceResponse&, std::unique_ptr<WebDataConsumerHandle> handle) override
    {
        ASSERT(!m_receivedResponse);
        m_receivedResponse = true;
        if (!handle) {
            // Here we assume WebURLLoader must return the response body as
            // |WebDataConsumerHandle| since we call
            // request.setUseStreamOnResponse().
            m_updater->update(createUnexpectedErrorDataConsumerHandle());
            return;
        }
        m_updater->update(std::move(handle));
    }

    void didFinishLoading(unsigned long, double) override
    {
        m_loader.reset();
    }

    void didFail(const ResourceError&) override
    {
        if (!m_receivedResponse)
            m_updater->update(createUnexpectedErrorDataConsumerHandle());
        m_loader.reset();
    }

    void didFailRedirectCheck() override
    {
        // We don't expect redirects for Blob loading.
        ASSERT_NOT_REACHED();
    }

    Persistent<CompositeDataConsumerHandle::Updater> m_updater;

    RefPtr<BlobDataHandle> m_blobDataHandle;
    Persistent<FetchBlobDataConsumerHandle::LoaderFactory> m_loaderFactory;
    std::unique_ptr<ThreadableLoader> m_loader;
    KURL m_registeredBlobURL;

    bool m_receivedResponse;
};

class DefaultLoaderFactory final : public FetchBlobDataConsumerHandle::LoaderFactory {
public:
    std::unique_ptr<ThreadableLoader> create(
        ExecutionContext& executionContext,
        ThreadableLoaderClient* client,
        const ThreadableLoaderOptions& options,
        const ResourceLoaderOptions& resourceLoaderOptions) override
    {
        return ThreadableLoader::create(executionContext, client, options, resourceLoaderOptions);
    }
};

} // namespace

// ReaderContext is referenced from FetchBlobDataConsumerHandle and
// ReaderContext::ReaderImpl.
// All functions/members must be called/accessed only on the reader thread,
// except for constructor, destructor, and obtainReader().
class FetchBlobDataConsumerHandle::ReaderContext final : public ThreadSafeRefCounted<ReaderContext> {
public:
    class ReaderImpl : public FetchDataConsumerHandle::Reader {
    public:
        ReaderImpl(Client* client, PassRefPtr<ReaderContext> readerContext, std::unique_ptr<WebDataConsumerHandle::Reader> reader)
            : m_readerContext(readerContext)
            , m_reader(std::move(reader))
            , m_notifier(client) { }
        ~ReaderImpl() override { }

        Result read(void* data, size_t size, Flags flags, size_t* readSize) override
        {
            if (m_readerContext->drained())
                return Done;
            m_readerContext->ensureStartLoader();
            Result r = m_reader->read(data, size, flags, readSize);
            if (!(size == 0 && (r == Ok || r == ShouldWait))) {
                // We read non-empty data, so we cannot use the blob data
                // handle which represents the whole data.
                m_readerContext->clearBlobDataHandleForDrain();
            }
            return r;
        }

        Result beginRead(const void** buffer, Flags flags, size_t* available) override
        {
            if (m_readerContext->drained())
                return Done;
            m_readerContext->ensureStartLoader();
            m_readerContext->clearBlobDataHandleForDrain();
            return m_reader->beginRead(buffer, flags, available);
        }

        Result endRead(size_t readSize) override
        {
            return m_reader->endRead(readSize);
        }

        PassRefPtr<BlobDataHandle> drainAsBlobDataHandle(BlobSizePolicy blobSizePolicy) override
        {
            if (!m_readerContext->m_blobDataHandleForDrain)
                return nullptr;
            if (blobSizePolicy == DisallowBlobWithInvalidSize && m_readerContext->m_blobDataHandleForDrain->size() == UINT64_MAX)
                return nullptr;
            RefPtr<BlobDataHandle> blobDataHandle = m_readerContext->m_blobDataHandleForDrain;
            m_readerContext->setDrained();
            m_readerContext->clearBlobDataHandleForDrain();
            return blobDataHandle.release();
        }

        PassRefPtr<EncodedFormData> drainAsFormData() override
        {
            RefPtr<BlobDataHandle> handle = drainAsBlobDataHandle(AllowBlobWithInvalidSize);
            if (!handle)
                return nullptr;
            RefPtr<EncodedFormData> formData = EncodedFormData::create();
            formData->appendBlob(handle->uuid(), handle);
            return formData.release();
        }

    private:
        RefPtr<ReaderContext> m_readerContext;
        std::unique_ptr<WebDataConsumerHandle::Reader> m_reader;
        NotifyOnReaderCreationHelper m_notifier;
    };

    ReaderContext(ExecutionContext* executionContext, PassRefPtr<BlobDataHandle> blobDataHandle, FetchBlobDataConsumerHandle::LoaderFactory* loaderFactory)
        : m_blobDataHandleForDrain(blobDataHandle)
        , m_loaderStarted(false)
        , m_drained(false)
    {
        CompositeDataConsumerHandle::Updater* updater = nullptr;
        m_handle = CompositeDataConsumerHandle::create(createWaitingDataConsumerHandle(), &updater);
        m_loaderContextHolder = CrossThreadHolder<LoaderContext>::create(executionContext, wrapUnique(new BlobLoaderContext(updater, m_blobDataHandleForDrain, loaderFactory)));
    }

    std::unique_ptr<FetchDataConsumerHandle::Reader> obtainReader(WebDataConsumerHandle::Client* client)
    {
        return wrapUnique(new ReaderImpl(client, this, m_handle->obtainReader(client)));
    }

private:
    void ensureStartLoader()
    {
        if (m_loaderStarted)
            return;
        m_loaderStarted = true;
        m_loaderContextHolder->postTask(crossThreadBind(&LoaderContext::start));
    }

    void clearBlobDataHandleForDrain()
    {
        m_blobDataHandleForDrain.clear();
    }

    bool drained() const { return m_drained; }
    void setDrained() { m_drained = true; }

    std::unique_ptr<WebDataConsumerHandle> m_handle;
    RefPtr<BlobDataHandle> m_blobDataHandleForDrain;
    std::unique_ptr<CrossThreadHolder<LoaderContext>> m_loaderContextHolder;

    bool m_loaderStarted;
    bool m_drained;
};

FetchBlobDataConsumerHandle::FetchBlobDataConsumerHandle(ExecutionContext* executionContext, PassRefPtr<BlobDataHandle> blobDataHandle, LoaderFactory* loaderFactory)
    : m_readerContext(adoptRef(new ReaderContext(executionContext, blobDataHandle, loaderFactory)))
{
}

FetchBlobDataConsumerHandle::~FetchBlobDataConsumerHandle()
{
}

std::unique_ptr<FetchDataConsumerHandle> FetchBlobDataConsumerHandle::create(ExecutionContext* executionContext, PassRefPtr<BlobDataHandle> blobDataHandle, LoaderFactory* loaderFactory)
{
    if (!blobDataHandle)
        return createFetchDataConsumerHandleFromWebHandle(createDoneDataConsumerHandle());

    return wrapUnique(new FetchBlobDataConsumerHandle(executionContext, blobDataHandle, loaderFactory));
}

std::unique_ptr<FetchDataConsumerHandle> FetchBlobDataConsumerHandle::create(ExecutionContext* executionContext, PassRefPtr<BlobDataHandle> blobDataHandle)
{
    if (!blobDataHandle)
        return createFetchDataConsumerHandleFromWebHandle(createDoneDataConsumerHandle());

    return wrapUnique(new FetchBlobDataConsumerHandle(executionContext, blobDataHandle, new DefaultLoaderFactory));
}

FetchDataConsumerHandle::Reader* FetchBlobDataConsumerHandle::obtainReaderInternal(Client* client)
{
    return m_readerContext->obtainReader(client).release();
}

} // namespace blink
