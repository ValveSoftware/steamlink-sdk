// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationController.h"

#include "core/frame/LocalFrame.h"
#include "modules/presentation/PresentationConnection.h"
#include "public/platform/modules/presentation/WebPresentationClient.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

PresentationController::PresentationController(LocalFrame& frame, WebPresentationClient* client)
    : LocalFrameLifecycleObserver(&frame)
    , m_client(client)
{
    if (m_client)
        m_client->setController(this);
}

PresentationController::~PresentationController()
{
    if (m_client)
        m_client->setController(nullptr);
}

// static
PresentationController* PresentationController::create(LocalFrame& frame, WebPresentationClient* client)
{
    return new PresentationController(frame, client);
}

// static
const char* PresentationController::supplementName()
{
    return "PresentationController";
}

// static
PresentationController* PresentationController::from(LocalFrame& frame)
{
    return static_cast<PresentationController*>(Supplement<LocalFrame>::from(frame, supplementName()));
}

// static
void PresentationController::provideTo(LocalFrame& frame, WebPresentationClient* client)
{
    Supplement<LocalFrame>::provideTo(frame, PresentationController::supplementName(), PresentationController::create(frame, client));
}

WebPresentationClient* PresentationController::client()
{
    return m_client;
}

DEFINE_TRACE(PresentationController)
{
    visitor->trace(m_presentation);
    visitor->trace(m_connections);
    Supplement<LocalFrame>::trace(visitor);
    LocalFrameLifecycleObserver::trace(visitor);
}

void PresentationController::didStartDefaultSession(WebPresentationConnectionClient* connectionClient)
{
    if (!m_presentation || !m_presentation->defaultRequest())
        return;
    PresentationConnection::take(this, wrapUnique(connectionClient), m_presentation->defaultRequest());
}

void PresentationController::didChangeSessionState(WebPresentationConnectionClient* connectionClient, WebPresentationConnectionState state)
{
    std::unique_ptr<WebPresentationConnectionClient> client = wrapUnique(connectionClient);

    PresentationConnection* connection = findConnection(client.get());
    if (!connection)
        return;
    connection->didChangeState(state);
}

void PresentationController::didCloseConnection(WebPresentationConnectionClient* connectionClient, WebPresentationConnectionCloseReason reason, const WebString& message)
{
    std::unique_ptr<WebPresentationConnectionClient> client = wrapUnique(connectionClient);

    PresentationConnection* connection = findConnection(client.get());
    if (!connection)
        return;
    connection->didClose(reason, message);
}

void PresentationController::didReceiveSessionTextMessage(WebPresentationConnectionClient* connectionClient, const WebString& message)
{
    std::unique_ptr<WebPresentationConnectionClient> client = wrapUnique(connectionClient);

    PresentationConnection* connection = findConnection(client.get());
    if (!connection)
        return;
    connection->didReceiveTextMessage(message);
}

void PresentationController::didReceiveSessionBinaryMessage(WebPresentationConnectionClient* connectionClient, const uint8_t* data, size_t length)
{
    std::unique_ptr<WebPresentationConnectionClient> client = wrapUnique(connectionClient);

    PresentationConnection* connection = findConnection(client.get());
    if (!connection)
        return;
    connection->didReceiveBinaryMessage(data, length);
}

void PresentationController::setPresentation(Presentation* presentation)
{
    m_presentation = presentation;
}

void PresentationController::setDefaultRequestUrl(const KURL& url)
{
    if (!m_client)
        return;

    if (url.isValid())
        m_client->setDefaultPresentationUrl(url.getString());
    else
        m_client->setDefaultPresentationUrl(blink::WebString());
}

void PresentationController::registerConnection(PresentationConnection* connection)
{
    m_connections.add(connection);
}

void PresentationController::willDetachFrameHost()
{
    if (m_client) {
        m_client->setController(nullptr);
        m_client = nullptr;
    }
}

PresentationConnection* PresentationController::findConnection(WebPresentationConnectionClient* connectionClient)
{
    for (const auto& connection : m_connections) {
        if (connection->matches(connectionClient))
            return connection.get();
    }

    return nullptr;
}

} // namespace blink
