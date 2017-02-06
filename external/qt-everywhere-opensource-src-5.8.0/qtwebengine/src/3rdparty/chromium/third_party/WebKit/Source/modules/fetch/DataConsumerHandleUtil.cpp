// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/DataConsumerHandleUtil.h"

#include "platform/blob/BlobData.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

using Result = WebDataConsumerHandle::Result;

namespace {

class WaitingHandle final : public WebDataConsumerHandle {
private:
    class ReaderImpl final : public WebDataConsumerHandle::Reader {
    public:
        Result beginRead(const void** buffer, WebDataConsumerHandle::Flags, size_t *available) override
        {
            *available = 0;
            *buffer = nullptr;
            return ShouldWait;
        }
        Result endRead(size_t) override
        {
            return UnexpectedError;
        }
    };
    Reader* obtainReaderInternal(Client*) override { return new ReaderImpl; }

    const char* debugName() const override { return "WaitingHandle"; }
};

class RepeatingReader final : public WebDataConsumerHandle::Reader {
public:
    explicit RepeatingReader(Result result, WebDataConsumerHandle::Client* client) : m_result(result), m_notifier(client) { }

private:
    Result beginRead(const void** buffer, WebDataConsumerHandle::Flags, size_t *available) override
    {
        *available = 0;
        *buffer = nullptr;
        return m_result;
    }
    Result endRead(size_t) override
    {
        return WebDataConsumerHandle::UnexpectedError;
    }

    Result m_result;
    NotifyOnReaderCreationHelper m_notifier;
};

class DoneHandle final : public WebDataConsumerHandle {
private:
    Reader* obtainReaderInternal(Client* client) override { return new RepeatingReader(Done, client); }
    const char* debugName() const override { return "DoneHandle"; }
};

class UnexpectedErrorHandle final : public WebDataConsumerHandle {
private:
    Reader* obtainReaderInternal(Client* client) override { return new RepeatingReader(UnexpectedError, client); }
    const char* debugName() const override { return "UnexpectedErrorHandle"; }
};

class WebToFetchDataConsumerHandleAdapter : public FetchDataConsumerHandle {
public:
    WebToFetchDataConsumerHandleAdapter(std::unique_ptr<WebDataConsumerHandle> handle) : m_handle(std::move(handle)) { }
private:
    class ReaderImpl final : public FetchDataConsumerHandle::Reader {
    public:
        ReaderImpl(std::unique_ptr<WebDataConsumerHandle::Reader> reader) : m_reader(std::move(reader)) { }
        Result read(void* data, size_t size, Flags flags, size_t* readSize) override
        {
            return m_reader->read(data, size, flags, readSize);
        }

        Result beginRead(const void** buffer, Flags flags, size_t* available) override
        {
            return m_reader->beginRead(buffer, flags, available);
        }
        Result endRead(size_t readSize) override
        {
            return m_reader->endRead(readSize);
        }
    private:
        std::unique_ptr<WebDataConsumerHandle::Reader> m_reader;
    };

    Reader* obtainReaderInternal(Client* client) override { return new ReaderImpl(m_handle->obtainReader(client)); }

    const char* debugName() const override { return m_handle->debugName(); }

    std::unique_ptr<WebDataConsumerHandle> m_handle;
};

} // namespace

std::unique_ptr<WebDataConsumerHandle> createWaitingDataConsumerHandle()
{
    return wrapUnique(new WaitingHandle);
}

std::unique_ptr<WebDataConsumerHandle> createDoneDataConsumerHandle()
{
    return wrapUnique(new DoneHandle);
}

std::unique_ptr<WebDataConsumerHandle> createUnexpectedErrorDataConsumerHandle()
{
    return wrapUnique(new UnexpectedErrorHandle);
}

std::unique_ptr<FetchDataConsumerHandle> createFetchDataConsumerHandleFromWebHandle(std::unique_ptr<WebDataConsumerHandle> handle)
{
    return wrapUnique(new WebToFetchDataConsumerHandleAdapter(std::move(handle)));
}

NotifyOnReaderCreationHelper::NotifyOnReaderCreationHelper(WebDataConsumerHandle::Client* client)
    : m_factory(this)
{
    if (!client)
        return;
    // Note we don't need thread safety here because this object is
    // bound to the current thread.
    Platform::current()->currentThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, WTF::bind(&NotifyOnReaderCreationHelper::notify, m_factory.createWeakPtr(), WTF::unretained(client)));
}

void NotifyOnReaderCreationHelper::notify(WebDataConsumerHandle::Client* client)
{
    // |client| dereference is safe here because:
    // - |this| is owned by a reader,
    //   so the reader outlives |this|, and
    // - |client| is the client of the reader, so |client| outlives the reader
    //   (guaranteed by the user of the reader),
    // and therefore |client| outlives |this|.
    client->didGetReadable();
}

} // namespace blink
