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

#ifndef ThreadableLoaderClientWrapper_h
#define ThreadableLoaderClientWrapper_h

#include "core/loader/ThreadableLoaderClient.h"
#include "platform/network/ResourceResponse.h"
#include "platform/network/ResourceTimingInfo.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/Threading.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class ThreadableLoaderClientWrapper : public ThreadSafeRefCounted<ThreadableLoaderClientWrapper> {
public:
    class ResourceTimingClient {
    public:
        virtual void didReceiveResourceTiming(const ResourceTimingInfo&) = 0;
    };

    static PassRefPtr<ThreadableLoaderClientWrapper> create(ThreadableLoaderClient* client)
    {
        return adoptRef(new ThreadableLoaderClientWrapper(client));
    }

    void clearClient()
    {
        m_done = true;
        m_client = 0;
        clearResourceTimingClient();
    }

    bool done() const
    {
        return m_done;
    }

    void setResourceTimingClient(ResourceTimingClient* client)
    {
        m_resourceTimingClient = client;
    }

    void clearResourceTimingClient()
    {
        m_resourceTimingClient = nullptr;
    }

    void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
    {
        if (m_client)
            m_client->didSendData(bytesSent, totalBytesToBeSent);
    }

    void didReceiveResponse(unsigned long identifier, std::unique_ptr<CrossThreadResourceResponseData> responseData, std::unique_ptr<WebDataConsumerHandle> handle)
    {
        ResourceResponse response(responseData.get());

        if (m_client)
            m_client->didReceiveResponse(identifier, response, std::move(handle));
    }

    void didReceiveData(std::unique_ptr<Vector<char>> data)
    {
        RELEASE_ASSERT(data->size() <= std::numeric_limits<unsigned>::max());

        if (m_client)
            m_client->didReceiveData(data->data(), data->size());
    }

    void didReceiveCachedMetadata(std::unique_ptr<Vector<char>> data)
    {
        if (m_client)
            m_client->didReceiveCachedMetadata(data->data(), data->size());
    }

    void didFinishLoading(unsigned long identifier, double finishTime)
    {
        m_done = true;
        if (m_client)
            m_client->didFinishLoading(identifier, finishTime);
    }

    void didFail(const ResourceError& error)
    {
        m_done = true;
        if (m_client)
            m_client->didFail(error);
    }

    void didFailAccessControlCheck(const ResourceError& error)
    {
        m_done = true;
        if (m_client)
            m_client->didFailAccessControlCheck(error);
    }

    void didFailRedirectCheck()
    {
        m_done = true;
        if (m_client)
            m_client->didFailRedirectCheck();
    }

    void didDownloadData(int dataLength)
    {
        if (m_client)
            m_client->didDownloadData(dataLength);
    }

    void didReceiveResourceTiming(std::unique_ptr<CrossThreadResourceTimingInfoData> timingData)
    {
        std::unique_ptr<ResourceTimingInfo> info(ResourceTimingInfo::adopt(std::move(timingData)));

        if (m_resourceTimingClient)
            m_resourceTimingClient->didReceiveResourceTiming(*info);
    }

protected:
    explicit ThreadableLoaderClientWrapper(ThreadableLoaderClient* client)
        : m_client(client)
        , m_resourceTimingClient(nullptr)
        , m_done(false)
    {
    }

    ThreadableLoaderClient* m_client;
    ResourceTimingClient* m_resourceTimingClient;
    bool m_done;
};

} // namespace blink

#endif // ThreadableLoaderClientWrapper_h
