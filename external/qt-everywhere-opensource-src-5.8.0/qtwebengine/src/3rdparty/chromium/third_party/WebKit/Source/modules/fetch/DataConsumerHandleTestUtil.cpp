// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/DataConsumerHandleTestUtil.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

DataConsumerHandleTestUtil::Thread::Thread(const char* name, InitializationPolicy initializationPolicy)
    : m_thread(WebThreadSupportingGC::create(name))
    , m_initializationPolicy(initializationPolicy)
    , m_waitableEvent(wrapUnique(new WaitableEvent()))
{
    m_thread->postTask(BLINK_FROM_HERE, crossThreadBind(&Thread::initialize, crossThreadUnretained(this)));
    m_waitableEvent->wait();
}

DataConsumerHandleTestUtil::Thread::~Thread()
{
    m_thread->postTask(BLINK_FROM_HERE, crossThreadBind(&Thread::shutdown, crossThreadUnretained(this)));
    m_waitableEvent->wait();
}

void DataConsumerHandleTestUtil::Thread::initialize()
{
    if (m_initializationPolicy >= ScriptExecution) {
        m_isolateHolder = wrapUnique(new gin::IsolateHolder());
        isolate()->Enter();
    }
    m_thread->initialize();
    if (m_initializationPolicy >= ScriptExecution) {
        v8::HandleScope handleScope(isolate());
        m_scriptState = ScriptState::create(v8::Context::New(isolate()), DOMWrapperWorld::create(isolate()));
    }
    if (m_initializationPolicy >= WithExecutionContext) {
        m_executionContext = new NullExecutionContext();
    }
    m_waitableEvent->signal();
}

void DataConsumerHandleTestUtil::Thread::shutdown()
{
    m_executionContext = nullptr;
    if (m_scriptState) {
        m_scriptState->disposePerContextData();
    }
    m_scriptState = nullptr;
    m_thread->shutdown();
    if (m_isolateHolder) {
        isolate()->Exit();
        isolate()->RequestGarbageCollectionForTesting(isolate()->kFullGarbageCollection);
        m_isolateHolder = nullptr;
    }
    m_waitableEvent->signal();
}

class DataConsumerHandleTestUtil::ReplayingHandle::ReaderImpl final : public Reader {
public:
    ReaderImpl(PassRefPtr<Context> context, Client* client)
        : m_context(context)
    {
        m_context->attachReader(client);
    }
    ~ReaderImpl()
    {
        m_context->detachReader();
    }

    Result beginRead(const void** buffer, Flags flags, size_t* available) override
    {
        return m_context->beginRead(buffer, flags, available);
    }
    Result endRead(size_t readSize) override
    {
        return m_context->endRead(readSize);
    }

private:
    RefPtr<Context> m_context;
};

void DataConsumerHandleTestUtil::ReplayingHandle::Context::add(const Command& command)
{
    MutexLocker locker(m_mutex);
    m_commands.append(command);
}

void DataConsumerHandleTestUtil::ReplayingHandle::Context::attachReader(WebDataConsumerHandle::Client* client)
{
    MutexLocker locker(m_mutex);
    ASSERT(!m_readerThread);
    ASSERT(!m_client);
    m_readerThread = Platform::current()->currentThread();
    m_client = client;

    if (m_client && !(isEmpty() && m_result == ShouldWait))
        notify();
}

void DataConsumerHandleTestUtil::ReplayingHandle::Context::detachReader()
{
    MutexLocker locker(m_mutex);
    ASSERT(m_readerThread && m_readerThread->isCurrentThread());
    m_readerThread = nullptr;
    m_client = nullptr;
    if (!m_isHandleAttached)
        m_detached->signal();
}

void DataConsumerHandleTestUtil::ReplayingHandle::Context::detachHandle()
{
    MutexLocker locker(m_mutex);
    m_isHandleAttached = false;
    if (!m_readerThread)
        m_detached->signal();
}

WebDataConsumerHandle::Result DataConsumerHandleTestUtil::ReplayingHandle::Context::beginRead(const void** buffer, Flags, size_t* available)
{
    MutexLocker locker(m_mutex);
    *buffer = nullptr;
    *available = 0;
    if (isEmpty())
        return m_result;

    const Command& command = top();
    Result result = Ok;
    switch (command.getName()) {
    case Command::Data: {
        auto& body = command.body();
        *available = body.size() - offset();
        *buffer = body.data() + offset();
        result = Ok;
        break;
    }
    case Command::Done:
        m_result = result = Done;
        consume(0);
        break;
    case Command::Wait:
        consume(0);
        result = ShouldWait;
        notify();
        break;
    case Command::Error:
        m_result = result = UnexpectedError;
        consume(0);
        break;
    }
    return result;
}

WebDataConsumerHandle::Result DataConsumerHandleTestUtil::ReplayingHandle::Context::endRead(size_t readSize)
{
    MutexLocker locker(m_mutex);
    consume(readSize);
    return Ok;
}

DataConsumerHandleTestUtil::ReplayingHandle::Context::Context()
    : m_offset(0)
    , m_readerThread(nullptr)
    , m_client(nullptr)
    , m_result(ShouldWait)
    , m_isHandleAttached(true)
    , m_detached(wrapUnique(new WaitableEvent()))
{
}

const DataConsumerHandleTestUtil::Command& DataConsumerHandleTestUtil::ReplayingHandle::Context::top()
{
    ASSERT(!isEmpty());
    return m_commands.first();
}

void DataConsumerHandleTestUtil::ReplayingHandle::Context::consume(size_t size)
{
    ASSERT(!isEmpty());
    ASSERT(size + m_offset <= top().body().size());
    bool fullyConsumed = (size + m_offset >= top().body().size());
    if (fullyConsumed) {
        m_offset = 0;
        m_commands.removeFirst();
    } else {
        m_offset += size;
    }
}

void DataConsumerHandleTestUtil::ReplayingHandle::Context::notify()
{
    if (!m_client)
        return;
    ASSERT(m_readerThread);
    m_readerThread->getWebTaskRunner()->postTask(BLINK_FROM_HERE, crossThreadBind(&Context::notifyInternal, wrapPassRefPtr(this)));
}

void DataConsumerHandleTestUtil::ReplayingHandle::Context::notifyInternal()
{
    {
        MutexLocker locker(m_mutex);
        if (!m_client || !m_readerThread->isCurrentThread()) {
            // There is no client, or a new reader is attached.
            return;
        }
    }
    // The reading thread is the current thread.
    m_client->didGetReadable();
}

DataConsumerHandleTestUtil::ReplayingHandle::ReplayingHandle()
    : m_context(Context::create())
{
}

DataConsumerHandleTestUtil::ReplayingHandle::~ReplayingHandle()
{
    m_context->detachHandle();
}

WebDataConsumerHandle::Reader* DataConsumerHandleTestUtil::ReplayingHandle::obtainReaderInternal(Client* client)
{
    return new ReaderImpl(m_context, client);
}

void DataConsumerHandleTestUtil::ReplayingHandle::add(const Command& command)
{
    m_context->add(command);
}

DataConsumerHandleTestUtil::HandleReader::HandleReader(std::unique_ptr<WebDataConsumerHandle> handle, std::unique_ptr<OnFinishedReading> onFinishedReading)
    : m_reader(handle->obtainReader(this))
    , m_onFinishedReading(std::move(onFinishedReading))
{
}

void DataConsumerHandleTestUtil::HandleReader::didGetReadable()
{
    WebDataConsumerHandle::Result r = WebDataConsumerHandle::UnexpectedError;
    char buffer[3];
    while (true) {
        size_t size;
        r = m_reader->read(buffer, sizeof(buffer), WebDataConsumerHandle::FlagNone, &size);
        if (r == WebDataConsumerHandle::ShouldWait)
            return;
        if (r != WebDataConsumerHandle::Ok)
            break;
        m_data.append(buffer, size);
    }
    std::unique_ptr<HandleReadResult> result = wrapUnique(new HandleReadResult(r, m_data));
    m_data.clear();
    Platform::current()->currentThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, WTF::bind(&HandleReader::runOnFinishedReading, WTF::unretained(this), passed(std::move(result))));
    m_reader = nullptr;
}

void DataConsumerHandleTestUtil::HandleReader::runOnFinishedReading(std::unique_ptr<HandleReadResult> result)
{
    ASSERT(m_onFinishedReading);
    std::unique_ptr<OnFinishedReading> onFinishedReading(std::move(m_onFinishedReading));
    (*onFinishedReading)(std::move(result));
}

DataConsumerHandleTestUtil::HandleTwoPhaseReader::HandleTwoPhaseReader(std::unique_ptr<WebDataConsumerHandle> handle, std::unique_ptr<OnFinishedReading> onFinishedReading)
    : m_reader(handle->obtainReader(this))
    , m_onFinishedReading(std::move(onFinishedReading))
{
}

void DataConsumerHandleTestUtil::HandleTwoPhaseReader::didGetReadable()
{
    WebDataConsumerHandle::Result r = WebDataConsumerHandle::UnexpectedError;
    while (true) {
        const void* buffer = nullptr;
        size_t size;
        r = m_reader->beginRead(&buffer, WebDataConsumerHandle::FlagNone, &size);
        if (r == WebDataConsumerHandle::ShouldWait)
            return;
        if (r != WebDataConsumerHandle::Ok)
            break;
        // Read smaller than available in order to test |endRead|.
        size_t readSize = std::min(size, std::max(size * 2 / 3, static_cast<size_t>(1)));
        m_data.append(static_cast<const char*>(buffer), readSize);
        m_reader->endRead(readSize);
    }
    std::unique_ptr<HandleReadResult> result = wrapUnique(new HandleReadResult(r, m_data));
    m_data.clear();
    Platform::current()->currentThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, WTF::bind(&HandleTwoPhaseReader::runOnFinishedReading, WTF::unretained(this), passed(std::move(result))));
    m_reader = nullptr;
}

void DataConsumerHandleTestUtil::HandleTwoPhaseReader::runOnFinishedReading(std::unique_ptr<HandleReadResult> result)
{
    ASSERT(m_onFinishedReading);
    std::unique_ptr<OnFinishedReading> onFinishedReading(std::move(m_onFinishedReading));
    (*onFinishedReading)(std::move(result));
}

} // namespace blink
