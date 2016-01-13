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

#ifndef WebWorkerPermissionClientProxy_h
#define WebWorkerPermissionClientProxy_h

#include "public/platform/WebPermissionCallbacks.h"

namespace blink {

class WebPermissionCallbacks;
class WebString;

// Proxy interface to talk to the document's PermissionClient implementation.
// This proxy is created by the embedder and is passed to the worker's
// WorkerGlobalScope in blink. Each allow method is called on the worker thread
// and may destructed on the worker thread.
class WebWorkerPermissionClientProxy {
public:
    virtual ~WebWorkerPermissionClientProxy() { }

    virtual bool allowDatabase(const WebString& name, const WebString& displayName, unsigned long estimatedSize)
    {
        return true;
    }

    virtual bool requestFileSystemAccessSync()
    {
        return true;
    }

    virtual bool allowIndexedDB(const WebString& name)
    {
        return true;
    }
};

} // namespace blink

#endif // WebWorkerPermissionClientProxy_h
