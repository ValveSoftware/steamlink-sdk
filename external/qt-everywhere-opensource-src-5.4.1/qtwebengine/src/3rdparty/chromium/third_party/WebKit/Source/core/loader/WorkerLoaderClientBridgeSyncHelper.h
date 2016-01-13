/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef WorkerLoaderClientBridgeSyncHelper_h
#define WorkerLoaderClientBridgeSyncHelper_h

#include "core/loader/ThreadableLoaderClient.h"
#include "wtf/Forward.h"
#include "wtf/Functional.h"
#include "wtf/Vector.h"

namespace blink {
class WebWaitableEvent;
}

namespace WebCore {

// This bridge is created and destroyed on the worker thread, but is
// passed to and used on the main thread. Each did* method records the given
// data so that they can be run on the worker thread later (by run()).
class WorkerLoaderClientBridgeSyncHelper : public ThreadableLoaderClient {
public:
    static PassOwnPtr<WorkerLoaderClientBridgeSyncHelper> create(ThreadableLoaderClient&, PassOwnPtr<blink::WebWaitableEvent>);
    virtual ~WorkerLoaderClientBridgeSyncHelper();

    // Called on the worker context thread.
    void run();

    // Called on the main thread.
    virtual void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent) OVERRIDE;
    virtual void didReceiveResponse(unsigned long identifier, const ResourceResponse&) OVERRIDE;
    virtual void didReceiveData(const char*, int dataLength) OVERRIDE;
    virtual void didDownloadData(int dataLength) OVERRIDE;
    virtual void didReceiveCachedMetadata(const char*, int dataLength) OVERRIDE;
    virtual void didFinishLoading(unsigned long identifier, double finishTime) OVERRIDE;
    virtual void didFail(const ResourceError&) OVERRIDE;
    virtual void didFailAccessControlCheck(const ResourceError&) OVERRIDE;
    virtual void didFailRedirectCheck() OVERRIDE;

private:
    WorkerLoaderClientBridgeSyncHelper(ThreadableLoaderClient&, PassOwnPtr<blink::WebWaitableEvent>);

    bool m_done;
    ThreadableLoaderClient& m_client;
    OwnPtr<blink::WebWaitableEvent> m_event;
    Vector<Vector<char>*> m_receivedData;
    Vector<Closure> m_clientTasks;
};

} // namespace WebCore

#endif // WorkerLoaderClientBridgeSyncHelper_h
