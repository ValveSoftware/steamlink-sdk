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

#ifndef WebServiceWorker_h
#define WebServiceWorker_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerState.h"

namespace blink {

class WebSecurityOrigin;
class WebServiceWorkerProvider;
class WebServiceWorkerProxy;
typedef WebVector<class WebMessagePortChannel*> WebMessagePortChannelArray;

class WebServiceWorker {
public:
    // The handle interface that retains a reference to the implementation of
    // WebServiceWorker in the embedder and is owned by ServiceWorker object in
    // Blink. The embedder must keep the service worker representation while
    // Blink is owning this handle.
    class Handle {
    public:
        virtual ~Handle() {}
        virtual WebServiceWorker* serviceWorker() { return nullptr; }
    };

    virtual ~WebServiceWorker() { }

    // Sets ServiceWorkerProxy, with which callee can start making upcalls
    // to the ServiceWorker object via the client. This doesn't pass the
    // ownership to the callee, and the proxy's lifetime is same as that of
    // WebServiceWorker.
    virtual void setProxy(WebServiceWorkerProxy*) { }
    virtual WebServiceWorkerProxy* proxy() { return nullptr; }

    virtual WebURL url() const { return WebURL(); }
    virtual WebServiceWorkerState state() const { return WebServiceWorkerStateUnknown; }

    // Callee receives ownership of the passed vector.
    // FIXME: Blob refs should be passed to maintain ref counts. crbug.com/351753
    virtual void postMessage(WebServiceWorkerProvider*, const WebString&, const WebSecurityOrigin&, WebMessagePortChannelArray*) = 0;

    virtual void terminate() { }
};

}

#endif // WebServiceWorker_h
