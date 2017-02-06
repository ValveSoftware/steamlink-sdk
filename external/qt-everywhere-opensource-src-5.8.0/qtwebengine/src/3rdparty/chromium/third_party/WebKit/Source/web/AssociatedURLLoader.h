/*
 * Copyright (C) 2010, 2011 Google Inc. All rights reserved.
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

#ifndef AssociatedURLLoader_h
#define AssociatedURLLoader_h

#include "platform/heap/Handle.h"
#include "public/platform/WebURLLoader.h"
#include "public/web/WebURLLoaderOptions.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class DocumentThreadableLoader;
class WebLocalFrameImpl;

// This class is used to implement WebFrame::createAssociatedURLLoader.
class AssociatedURLLoader final : public WebURLLoader {
    WTF_MAKE_NONCOPYABLE(AssociatedURLLoader);
public:
    AssociatedURLLoader(WebLocalFrameImpl*, const WebURLLoaderOptions&);
    ~AssociatedURLLoader();

    // WebURLLoader methods:
    void loadSynchronously(const WebURLRequest&, WebURLResponse&, WebURLError&, WebData&) override;
    void loadAsynchronously(const WebURLRequest&, WebURLLoaderClient*) override;
    void cancel() override;
    void setDefersLoading(bool) override;
    void setLoadingTaskRunner(blink::WebTaskRunner*) override;

    // Called by |m_observer| to handle destruction of the Document associated
    // with the frame given to the constructor.
    void documentDestroyed();

    // Called by ClientAdapter to handle completion of loading.
    void clientAdapterDone();

private:
    class ClientAdapter;
    class Observer;

    void cancelLoader();
    void disposeObserver();

    WebURLLoaderClient* releaseClient()
    {
        WebURLLoaderClient* client = m_client;
        m_client = nullptr;
        return client;
    }

    WebURLLoaderClient* m_client;
    WebURLLoaderOptions m_options;

    // An adapter which converts the DocumentThreadableLoaderClient method
    // calls into the WebURLLoaderClient method calls.
    std::unique_ptr<ClientAdapter> m_clientAdapter;
    std::unique_ptr<DocumentThreadableLoader> m_loader;

    // A ContextLifecycleObserver for cancelling |m_loader| when the Document
    // is detached.
    Persistent<Observer> m_observer;
};

} // namespace blink

#endif
