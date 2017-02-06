// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/DataConsumerTee.h"

#include "core/dom/ActiveDOMObject.h"
#include "core/dom/ExecutionContext.h"
#include "modules/fetch/DataConsumerHandleUtil.h"
#include "modules/fetch/FetchBlobDataConsumerHandle.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/heap/Handle.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/Deque.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

using Result = WebDataConsumerHandle::Result;
using Flags = WebDataConsumerHandle::Flags;

namespace {

// This file contains the "tee" implementation. There are several classes and
// their relationship is complicated, so let me describe here.
//
// Tee::create function creates two DestinationHandles (destinations) from one
// WebDataConsumerHandle (source). In fact, it uses a reader of the source
// handle.
//
// SourceContext reads data from the source reader and enques it to two
// destination contexts. Destination readers read data from its associated
// contexts. Here is an object graph.
//
//   R: the root object
//  SR: the source reader
//  SC: the SourceContext
// DCn: nth DestinationContext
// DRn: nth DestinationReader
// DHn: nth DestinationHandle
// ---------
// (normal)
//       ---> DC1 <--- DR1 / DH1
//       |
//       |
// SR <--SC <-> R
//       |
//       |
//       ---> DC2 <--- DR2 / DH2
//
// ---------
//
// The root object (R) refers to the SourceContext, and is referred by many
// objects including the SourceContext. As the root object is a
// ThreadSafeRefCounted that reference cycle keeps the entire pipe alive.
// The root object only has "stop" function that breaks the reference cycle.
// It will be called when:
//  - The source context finishes reading,
//  - The source context gets errored while reading,
//  - The execution context associated with the source context is stopped or
//  - All destination handles and readers are gone.
//
// ---------
// (stopped)
//       ---> DC1 <--- DR1 / DH1
//       |
//       |
// SR <--SC --> R
//       |
//       |
//       ---> DC2 <--- DR2 / DH2
//
// -------
// When |stop| is called, no one has a strong reference to the source context
// and it will be collected.
//

class SourceContext;

class TeeRootObject final : public ThreadSafeRefCounted<TeeRootObject> {
public:
    static PassRefPtr<TeeRootObject> create() { return adoptRef(new TeeRootObject()); }

    void initialize(SourceContext* sourceContext)
    {
        m_sourceContext = sourceContext;
    }

    // This function can be called from any thread.
    void stop()
    {
        m_sourceContext = nullptr;
    }

private:
    TeeRootObject() = default;

    CrossThreadPersistent<SourceContext> m_sourceContext;
};

class DestinationTracker final : public ThreadSafeRefCounted<DestinationTracker> {
public:
    static PassRefPtr<DestinationTracker> create(PassRefPtr<TeeRootObject> root) { return adoptRef(new DestinationTracker(root)); }
    ~DestinationTracker()
    {
        m_root->stop();
    }

private:
    explicit DestinationTracker(PassRefPtr<TeeRootObject> root) : m_root(root) { }

    RefPtr<TeeRootObject> m_root;
};

class DestinationContext final : public ThreadSafeRefCounted<DestinationContext> {
public:
    class Proxy : public ThreadSafeRefCounted<Proxy> {
    public:
        static PassRefPtr<Proxy> create(PassRefPtr<DestinationContext> context, PassRefPtr<DestinationTracker> tracker)
        {
            return adoptRef(new Proxy(context, tracker));
        }
        ~Proxy()
        {
            m_context->detach();
        }

        DestinationContext* context() { return m_context.get(); }

    private:
        Proxy(PassRefPtr<DestinationContext> context, PassRefPtr<DestinationTracker> tracker) : m_context(context), m_tracker(tracker) { }

        RefPtr<DestinationContext> m_context;
        RefPtr<DestinationTracker> m_tracker;
    };

    static PassRefPtr<DestinationContext> create() { return adoptRef(new DestinationContext()); }

    void enqueue(const char* buffer, size_t size)
    {
        bool needsNotification = false;
        {
            MutexLocker locker(m_mutex);
            needsNotification = m_queue.isEmpty();
            std::unique_ptr<Vector<char>> data = wrapUnique(new Vector<char>);
            data->append(buffer, size);
            m_queue.append(std::move(data));
        }
        if (needsNotification)
            notify();
    }

    void setResult(Result r)
    {
        ASSERT(r != WebDataConsumerHandle::Ok);
        ASSERT(r != WebDataConsumerHandle::ShouldWait);
        {
            MutexLocker locker(m_mutex);
            if (m_result != WebDataConsumerHandle::ShouldWait) {
                // The result was already set.
                return;
            }
            m_result = r;
            if (r != WebDataConsumerHandle::Done && !m_isTwoPhaseReadInProgress)
                m_queue.clear();
        }
        notify();
    }

    void notify()
    {
        {
            MutexLocker locker(m_mutex);
            if (!m_client) {
                // No client is registered.
                return;
            }
            ASSERT(m_readerThread);
            if (!m_readerThread->isCurrentThread()) {
                m_readerThread->getWebTaskRunner()->postTask(BLINK_FROM_HERE, crossThreadBind(&DestinationContext::notify, wrapPassRefPtr(this)));
                return;
            }
        }
        // The reading thread is the current thread.
        if (m_client)
            m_client->didGetReadable();
    }

    Mutex& mutex() { return m_mutex; }

    // The following functions don't use lock. They should be protected by the
    // caller.
    void attachReader(WebDataConsumerHandle::Client* client)
    {
        ASSERT(!m_readerThread);
        ASSERT(!m_client);
        m_readerThread = Platform::current()->currentThread();
        m_client = client;
    }
    void detachReader()
    {
        ASSERT(m_readerThread && m_readerThread->isCurrentThread());
        m_readerThread = nullptr;
        m_client = nullptr;
    }
    const std::unique_ptr<Vector<char>>& top() const { return m_queue.first(); }
    bool isEmpty() const { return m_queue.isEmpty(); }
    size_t offset() const { return m_offset; }
    void consume(size_t size)
    {
        const auto& top = m_queue.first();
        ASSERT(m_offset <= m_offset + size);
        ASSERT(m_offset + size <= top->size());
        if (top->size() <= m_offset + size) {
            m_offset = 0;
            m_queue.removeFirst();
        } else {
            m_offset += size;
        }
    }
    Result getResult() { return m_result; }

private:
    DestinationContext()
        : m_result(WebDataConsumerHandle::ShouldWait)
        , m_readerThread(nullptr)
        , m_client(nullptr)
        , m_offset(0)
        , m_isTwoPhaseReadInProgress(false)
    {
    }

    void detach()
    {
        MutexLocker locker(m_mutex);
        ASSERT(!m_client);
        ASSERT(!m_readerThread);
        m_queue.clear();
    }

    Result m_result;
    Deque<std::unique_ptr<Vector<char>>> m_queue;
    // Note: Holding a WebThread raw pointer is not generally safe, but we can
    // do that in this case because:
    //  1. Destructing a ReaderImpl when the bound thread ends is a user's
    //     responsibility.
    //  2. |m_readerThread| will never be used after the associated reader is
    //     detached.
    WebThread* m_readerThread;
    WebDataConsumerHandle::Client* m_client;
    size_t m_offset;
    bool m_isTwoPhaseReadInProgress;
    Mutex m_mutex;
};

class DestinationReader final : public WebDataConsumerHandle::Reader {
public:
    DestinationReader(PassRefPtr<DestinationContext::Proxy> contextProxy, WebDataConsumerHandle::Client* client)
        : m_contextProxy(contextProxy)
    {
        MutexLocker locker(context()->mutex());
        context()->attachReader(client);
        if (client) {
            // We need to use crossThreadBind here to retain the context. Note
            // |context()| return value is of type DestinationContext*, not
            // PassRefPtr<DestinationContext>.
            Platform::current()->currentThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, crossThreadBind(&DestinationContext::notify, wrapPassRefPtr(context())));
        }
    }
    ~DestinationReader() override
    {
        MutexLocker locker(context()->mutex());
        context()->detachReader();
    }

    Result beginRead(const void** buffer, Flags, size_t* available) override
    {
        MutexLocker locker(context()->mutex());
        *available = 0;
        *buffer = nullptr;
        if (context()->isEmpty())
            return context()->getResult();

        const std::unique_ptr<Vector<char>>& chunk = context()->top();
        *available = chunk->size() - context()->offset();
        *buffer = chunk->data() + context()->offset();
        return WebDataConsumerHandle::Ok;
    }

    Result endRead(size_t readSize) override
    {
        MutexLocker locker(context()->mutex());
        if (context()->isEmpty())
            return WebDataConsumerHandle::UnexpectedError;
        context()->consume(readSize);
        return WebDataConsumerHandle::Ok;
    }

private:
    DestinationContext* context() { return m_contextProxy->context(); }

    RefPtr<DestinationContext::Proxy> m_contextProxy;
};

class DestinationHandle final : public WebDataConsumerHandle {
public:
    static std::unique_ptr<WebDataConsumerHandle> create(PassRefPtr<DestinationContext::Proxy> contextProxy)
    {
        return wrapUnique(new DestinationHandle(contextProxy));
    }

private:
    DestinationHandle(PassRefPtr<DestinationContext::Proxy> contextProxy) : m_contextProxy(contextProxy) { }
    DestinationReader* obtainReaderInternal(Client* client) { return new DestinationReader(m_contextProxy, client); }
    const char* debugName() const override { return "DestinationHandle"; }

    RefPtr<DestinationContext::Proxy> m_contextProxy;
};

// Bound to the created thread.
class SourceContext final : public GarbageCollectedFinalized<SourceContext>, public ActiveDOMObject, public WebDataConsumerHandle::Client {
    USING_GARBAGE_COLLECTED_MIXIN(SourceContext);
public:
    SourceContext(
        PassRefPtr<TeeRootObject> root,
        std::unique_ptr<WebDataConsumerHandle> src,
        PassRefPtr<DestinationContext> dest1,
        PassRefPtr<DestinationContext> dest2,
        ExecutionContext* executionContext)
        : ActiveDOMObject(executionContext)
        , m_root(root)
        , m_reader(src->obtainReader(this))
        , m_dest1(dest1)
        , m_dest2(dest2)
    {
        suspendIfNeeded();
    }
    ~SourceContext() override
    {
        stopInternal();
    }

    void didGetReadable() override
    {
        ASSERT(m_reader);
        Result r = WebDataConsumerHandle::Ok;
        while (true) {
            const void* buffer = nullptr;
            size_t available = 0;
            r = m_reader->beginRead(&buffer, WebDataConsumerHandle::FlagNone, &available);
            if (r == WebDataConsumerHandle::ShouldWait)
                return;
            if (r != WebDataConsumerHandle::Ok)
                break;
            m_dest1->enqueue(static_cast<const char*>(buffer), available);
            m_dest2->enqueue(static_cast<const char*>(buffer), available);
            m_reader->endRead(available);
        }
        m_dest1->setResult(r);
        m_dest2->setResult(r);
        stopInternal();
    }

    void stop() override
    {
        stopInternal();
        ActiveDOMObject::stop();
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        ActiveDOMObject::trace(visitor);
    }

private:
    void stopInternal()
    {
        if (!m_root)
            return;
        // When we already set a result, this result setting will be ignored.
        m_dest1->setResult(WebDataConsumerHandle::UnexpectedError);
        m_dest2->setResult(WebDataConsumerHandle::UnexpectedError);
        m_root->stop();
        m_root = nullptr;
        m_reader = nullptr;
        m_dest1 = nullptr;
        m_dest2 = nullptr;
    }

    RefPtr<TeeRootObject> m_root;
    std::unique_ptr<WebDataConsumerHandle::Reader> m_reader;
    RefPtr<DestinationContext> m_dest1;
    RefPtr<DestinationContext> m_dest2;
};

} // namespace

void DataConsumerTee::create(ExecutionContext* executionContext, std::unique_ptr<WebDataConsumerHandle> src, std::unique_ptr<WebDataConsumerHandle>* dest1, std::unique_ptr<WebDataConsumerHandle>* dest2)
{
    RefPtr<TeeRootObject> root = TeeRootObject::create();
    RefPtr<DestinationTracker> tracker = DestinationTracker::create(root);
    RefPtr<DestinationContext> context1 = DestinationContext::create();
    RefPtr<DestinationContext> context2 = DestinationContext::create();

    root->initialize(new SourceContext(root, std::move(src), context1, context2, executionContext));

    *dest1 = DestinationHandle::create(DestinationContext::Proxy::create(context1, tracker));
    *dest2 = DestinationHandle::create(DestinationContext::Proxy::create(context2, tracker));
}

void DataConsumerTee::create(ExecutionContext* executionContext, std::unique_ptr<FetchDataConsumerHandle> src, std::unique_ptr<FetchDataConsumerHandle>* dest1, std::unique_ptr<FetchDataConsumerHandle>* dest2)
{
    RefPtr<BlobDataHandle> blobDataHandle = src->obtainReader(nullptr)->drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::AllowBlobWithInvalidSize);
    if (blobDataHandle) {
        *dest1 = FetchBlobDataConsumerHandle::create(executionContext, blobDataHandle);
        *dest2 = FetchBlobDataConsumerHandle::create(executionContext, blobDataHandle);
        return;
    }

    std::unique_ptr<WebDataConsumerHandle> webDest1, webDest2;
    DataConsumerTee::create(executionContext, static_cast<std::unique_ptr<WebDataConsumerHandle>>(std::move(src)), &webDest1, &webDest2);
    *dest1 = createFetchDataConsumerHandleFromWebHandle(std::move(webDest1));
    *dest2 = createFetchDataConsumerHandleFromWebHandle(std::move(webDest2));
    return;
}

} // namespace blink
