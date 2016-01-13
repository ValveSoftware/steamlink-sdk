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

#ifndef WorkerLoaderClientBridge_h
#define WorkerLoaderClientBridge_h

#include "core/loader/ThreadableLoaderClient.h"
#include "wtf/Forward.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ThreadableLoaderClientWrapper;
class WorkerLoaderProxy;

// This bridge is created and destroyed on the worker thread, but is
// passed to and used on the main thread. Each did* method relays the given
// data to the client wrapper on the worker context thread.
class WorkerLoaderClientBridge : public ThreadableLoaderClient {
public:
    static PassOwnPtr<ThreadableLoaderClient> create(PassRefPtr<ThreadableLoaderClientWrapper>, WorkerLoaderProxy&);
    virtual ~WorkerLoaderClientBridge();

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
    WorkerLoaderClientBridge(PassRefPtr<ThreadableLoaderClientWrapper>, WorkerLoaderProxy&);

    // Used on the worker context thread, while its refcounting is done on
    // either thread.
    RefPtr<ThreadableLoaderClientWrapper> m_workerClientWrapper;

    WorkerLoaderProxy& m_loaderProxy;
};

} // namespace WebCore

#endif // WorkerLoaderClientBridge_h
