// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerClient.h"
#include "modules/serviceworkers/ServiceWorkerWindowClient.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "public/platform/WebString.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

ServiceWorkerClient* ServiceWorkerClient::take(ScriptPromiseResolver*, std::unique_ptr<WebServiceWorkerClientInfo> webClient)
{
    if (!webClient)
        return nullptr;

    switch (webClient->clientType) {
    case WebServiceWorkerClientTypeWindow:
        return ServiceWorkerWindowClient::create(*webClient);
    case WebServiceWorkerClientTypeWorker:
    case WebServiceWorkerClientTypeSharedWorker:
        return ServiceWorkerClient::create(*webClient);
    case WebServiceWorkerClientTypeLast:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

ServiceWorkerClient* ServiceWorkerClient::create(const WebServiceWorkerClientInfo& info)
{
    return new ServiceWorkerClient(info);
}

ServiceWorkerClient::ServiceWorkerClient(const WebServiceWorkerClientInfo& info)
    : m_uuid(info.uuid)
    , m_url(info.url.string())
    , m_frameType(info.frameType)
{
}

ServiceWorkerClient::~ServiceWorkerClient()
{
}

String ServiceWorkerClient::frameType() const
{
    switch (m_frameType) {
    case WebURLRequest::FrameTypeAuxiliary:
        return "auxiliary";
    case WebURLRequest::FrameTypeNested:
        return "nested";
    case WebURLRequest::FrameTypeNone:
        return "none";
    case WebURLRequest::FrameTypeTopLevel:
        return "top-level";
    }

    ASSERT_NOT_REACHED();
    return String();
}

void ServiceWorkerClient::postMessage(ExecutionContext* context, PassRefPtr<SerializedScriptValue> message, const MessagePortArray& ports, ExceptionState& exceptionState)
{
    // Disentangle the port in preparation for sending it to the remote context.
    std::unique_ptr<MessagePortChannelArray> channels = MessagePort::disentanglePorts(context, ports, exceptionState);
    if (exceptionState.hadException())
        return;

    if (message->containsTransferableArrayBuffer())
        context->addConsoleMessage(ConsoleMessage::create(JSMessageSource, WarningMessageLevel, "ServiceWorkerClient cannot send an ArrayBuffer as a transferable object yet. See http://crbug.com/511119"));

    WebString messageString = message->toWireString();
    std::unique_ptr<WebMessagePortChannelArray> webChannels = MessagePort::toWebMessagePortChannelArray(std::move(channels));
    ServiceWorkerGlobalScopeClient::from(context)->postMessageToClient(m_uuid, messageString, std::move(webChannels));
}

} // namespace blink
