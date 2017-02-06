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

#include "web/SharedWorkerRepositoryClientImpl.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/Event.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/workers/SharedWorker.h"
#include "platform/network/ResourceResponse.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/web/WebContentSecurityPolicy.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebKit.h"
#include "public/web/WebSharedWorker.h"
#include "public/web/WebSharedWorkerCreationErrors.h"
#include "public/web/WebSharedWorkerRepositoryClient.h"
#include "web/WebLocalFrameImpl.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

// Callback class that keeps the SharedWorker and WebSharedWorker objects alive while connecting.
class SharedWorkerConnector : private WebSharedWorkerConnector::ConnectListener {
public:
    SharedWorkerConnector(SharedWorker* worker, const KURL& url, const String& name, WebMessagePortChannelUniquePtr channel, std::unique_ptr<WebSharedWorkerConnector> webWorkerConnector)
        : m_worker(worker)
        , m_url(url)
        , m_name(name)
        , m_webWorkerConnector(std::move(webWorkerConnector))
        , m_channel(std::move(channel)) { }

    virtual ~SharedWorkerConnector();
    void connect();

private:
    // WebSharedWorkerConnector::ConnectListener overrides.
    void connected() override;
    void scriptLoadFailed() override;

    Persistent<SharedWorker> m_worker;
    KURL m_url;
    String m_name;
    std::unique_ptr<WebSharedWorkerConnector> m_webWorkerConnector;
    WebMessagePortChannelUniquePtr m_channel;
};

SharedWorkerConnector::~SharedWorkerConnector()
{
    m_worker->setIsBeingConnected(false);
}

void SharedWorkerConnector::connect()
{
    m_worker->setIsBeingConnected(true);
    m_webWorkerConnector->connect(m_channel.release(), this);
}

void SharedWorkerConnector::connected()
{
    // Free ourselves (this releases the SharedWorker so it can be freed as well if unreferenced).
    delete this;
}

void SharedWorkerConnector::scriptLoadFailed()
{
    m_worker->dispatchEvent(Event::createCancelable(EventTypeNames::error));
    // Free ourselves (this releases the SharedWorker so it can be freed as well if unreferenced).
    delete this;
}

static WebSharedWorkerRepositoryClient::DocumentID getId(void* document)
{
    DCHECK(document);
    return reinterpret_cast<WebSharedWorkerRepositoryClient::DocumentID>(document);
}

void SharedWorkerRepositoryClientImpl::connect(SharedWorker* worker, WebMessagePortChannelUniquePtr port, const KURL& url, const String& name, ExceptionState& exceptionState)
{
    DCHECK(m_client);

    // No nested workers (for now) - connect() should only be called from document context.
    DCHECK(worker->getExecutionContext()->isDocument());
    Document* document = toDocument(worker->getExecutionContext());

    // TODO(estark): this is broken, as it only uses the first header
    // when multiple might have been sent. Fix by making the
    // SharedWorkerConnector interface take a map that can contain
    // multiple headers.
    std::unique_ptr<Vector<CSPHeaderAndType>> headers = worker->getExecutionContext()->contentSecurityPolicy()->headers();
    WebString header;
    WebContentSecurityPolicyType headerType = WebContentSecurityPolicyTypeReport;

    if (headers->size() > 0) {
        header = (*headers)[0].first;
        headerType = static_cast<WebContentSecurityPolicyType>((*headers)[0].second);
    }

    WebWorkerCreationError creationError;
    String unusedSecureContextError;
    bool isSecureContext = worker->getExecutionContext()->isSecureContext(unusedSecureContextError);
    std::unique_ptr<WebSharedWorkerConnector> webWorkerConnector = wrapUnique(m_client->createSharedWorkerConnector(url, name, getId(document), header, headerType, worker->getExecutionContext()->securityContext().addressSpace(), isSecureContext ? WebSharedWorkerCreationContextTypeSecure : WebSharedWorkerCreationContextTypeNonsecure, &creationError));
    if (creationError != WebWorkerCreationErrorNone) {
        if (creationError == WebWorkerCreationErrorURLMismatch) {
            // Existing worker does not match this url, so return an error back to the caller.
            exceptionState.throwDOMException(URLMismatchError, "The location of the SharedWorker named '" + name + "' does not exactly match the provided URL ('" + url.elidedString() + "').");
            return;
        } else if (creationError == WebWorkerCreationErrorSecureContextMismatch) {
            if (isSecureContext) {
                UseCounter::count(document, UseCounter::NonSecureSharedWorkerAccessedFromSecureContext);
            } else {
                UseCounter::count(document, UseCounter::SecureSharedWorkerAccessedFromNonSecureContext);
            }
        }
    }

    // The connector object manages its own lifecycle (and the lifecycles of the two worker objects).
    // It will free itself once connecting is completed.
    SharedWorkerConnector* connector = new SharedWorkerConnector(worker, url, name, std::move(port), std::move(webWorkerConnector));
    connector->connect();
}

void SharedWorkerRepositoryClientImpl::documentDetached(Document* document)
{
    DCHECK(m_client);
    m_client->documentDetached(getId(document));
}

SharedWorkerRepositoryClientImpl::SharedWorkerRepositoryClientImpl(WebSharedWorkerRepositoryClient* client)
    : m_client(client)
{
}

} // namespace blink
