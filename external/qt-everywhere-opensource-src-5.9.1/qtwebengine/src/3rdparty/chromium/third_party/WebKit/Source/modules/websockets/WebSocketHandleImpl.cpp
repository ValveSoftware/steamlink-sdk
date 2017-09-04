// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/websockets/WebSocketHandleImpl.h"

#include "modules/websockets/WebSocketHandleClient.h"
#include "platform/WebTaskRunner.h"
#include "platform/network/NetworkLog.h"
#include "platform/network/WebSocketHandshakeRequest.h"
#include "platform/network/WebSocketHandshakeResponse.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "wtf/Functional.h"
#include "wtf/text/WTFString.h"

namespace blink {
namespace {

const uint16_t kAbnormalShutdownOpCode = 1006;

}  // namespace

WebSocketHandleImpl::WebSocketHandleImpl()
    : m_client(nullptr), m_clientBinding(this) {
  NETWORK_DVLOG(1) << this << " created";
}

WebSocketHandleImpl::~WebSocketHandleImpl() {
  NETWORK_DVLOG(1) << this << " deleted";

  if (m_websocket)
    m_websocket->StartClosingHandshake(kAbnormalShutdownOpCode, emptyString());
}

void WebSocketHandleImpl::initialize(InterfaceProvider* interfaceProvider) {
  NETWORK_DVLOG(1) << this << " initialize(...)";

  DCHECK(!m_websocket);
  interfaceProvider->getInterface(mojo::GetProxy(&m_websocket));

  m_websocket.set_connection_error_with_reason_handler(convertToBaseCallback(
      WTF::bind(&WebSocketHandleImpl::onConnectionError, unretained(this))));
}

void WebSocketHandleImpl::connect(const KURL& url,
                                  const Vector<String>& protocols,
                                  SecurityOrigin* origin,
                                  const KURL& firstPartyForCookies,
                                  const String& userAgentOverride,
                                  WebSocketHandleClient* client) {
  DCHECK(m_websocket);

  NETWORK_DVLOG(1) << this << " connect(" << url.getString() << ", "
                   << origin->toString() << ")";

  DCHECK(!m_client);
  DCHECK(client);
  m_client = client;

  m_websocket->AddChannelRequest(
      url, protocols, origin, firstPartyForCookies,
      userAgentOverride.isNull() ? emptyString() : userAgentOverride,
      m_clientBinding.CreateInterfacePtrAndBind(
          Platform::current()
              ->currentThread()
              ->scheduler()
              ->loadingTaskRunner()
              ->toSingleThreadTaskRunner()));
}

void WebSocketHandleImpl::send(bool fin,
                               WebSocketHandle::MessageType type,
                               const char* data,
                               size_t size) {
  DCHECK(m_websocket);

  mojom::blink::WebSocketMessageType typeToPass;
  switch (type) {
    case WebSocketHandle::MessageTypeContinuation:
      typeToPass = mojom::blink::WebSocketMessageType::CONTINUATION;
      break;
    case WebSocketHandle::MessageTypeText:
      typeToPass = mojom::blink::WebSocketMessageType::TEXT;
      break;
    case WebSocketHandle::MessageTypeBinary:
      typeToPass = mojom::blink::WebSocketMessageType::BINARY;
      break;
    default:
      NOTREACHED();
      return;
  }

  NETWORK_DVLOG(1) << this << " send(" << fin << ", " << typeToPass << ", "
                   << "(data size = " << size << "))";

  // TODO(darin): Avoid this copy.
  Vector<uint8_t> dataToPass(size);
  std::copy(data, data + size, dataToPass.begin());

  m_websocket->SendFrame(fin, typeToPass, dataToPass);
}

void WebSocketHandleImpl::flowControl(int64_t quota) {
  DCHECK(m_websocket);

  NETWORK_DVLOG(1) << this << " flowControl(" << quota << ")";

  m_websocket->SendFlowControl(quota);
}

void WebSocketHandleImpl::close(unsigned short code, const String& reason) {
  DCHECK(m_websocket);

  NETWORK_DVLOG(1) << this << " close(" << code << ", " << reason << ")";

  m_websocket->StartClosingHandshake(code,
                                     reason.isNull() ? emptyString() : reason);
}

void WebSocketHandleImpl::disconnect() {
  m_websocket.reset();
  m_client = nullptr;
}

void WebSocketHandleImpl::onConnectionError(uint32_t customReason,
                                            const std::string& description) {
  if (!blink::Platform::current()) {
    // In the renderrer shutdown sequence, mojo channels are destructed and this
    // function is called. On the other hand, blink objects became invalid
    // *silently*, which means we must not touch |*client_| any more.
    // TODO(yhirano): Remove this code once the shutdown sequence is fixed.
    return;
  }

  // Our connection to the WebSocket was dropped. This could be due to
  // exceeding the maximum number of concurrent websockets from this process.
  String failureMessage;
  if (customReason == mojom::blink::WebSocket::kInsufficientResources) {
    failureMessage = description.empty() ? "Insufficient resources"
                                         : String::fromUTF8(description.c_str(),
                                                            description.size());
  } else {
    DCHECK(description.empty());
    failureMessage = "Unspecified reason";
  }
  OnFailChannel(failureMessage);
}

void WebSocketHandleImpl::OnFailChannel(const String& message) {
  NETWORK_DVLOG(1) << this << " OnFailChannel(" << message << ")";

  WebSocketHandleClient* client = m_client;
  disconnect();
  if (!client)
    return;

  client->didFail(this, message);
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnStartOpeningHandshake(
    mojom::blink::WebSocketHandshakeRequestPtr request) {
  NETWORK_DVLOG(1) << this << " OnStartOpeningHandshake("
                   << request->url.getString() << ")";

  RefPtr<WebSocketHandshakeRequest> requestToPass =
      WebSocketHandshakeRequest::create(request->url);
  for (size_t i = 0; i < request->headers.size(); ++i) {
    const mojom::blink::HttpHeaderPtr& header = request->headers[i];
    requestToPass->addHeaderField(AtomicString(header->name),
                                  AtomicString(header->value));
  }
  requestToPass->setHeadersText(request->headers_text);
  m_client->didStartOpeningHandshake(this, requestToPass);
}

void WebSocketHandleImpl::OnFinishOpeningHandshake(
    mojom::blink::WebSocketHandshakeResponsePtr response) {
  NETWORK_DVLOG(1) << this << " OnFinishOpeningHandshake("
                   << response->url.getString() << ")";

  WebSocketHandshakeResponse responseToPass;
  responseToPass.setStatusCode(response->status_code);
  responseToPass.setStatusText(response->status_text);
  for (size_t i = 0; i < response->headers.size(); ++i) {
    const mojom::blink::HttpHeaderPtr& header = response->headers[i];
    responseToPass.addHeaderField(AtomicString(header->name),
                                  AtomicString(header->value));
  }
  responseToPass.setHeadersText(response->headers_text);
  m_client->didFinishOpeningHandshake(this, &responseToPass);
}

void WebSocketHandleImpl::OnAddChannelResponse(const String& protocol,
                                               const String& extensions) {
  NETWORK_DVLOG(1) << this << " OnAddChannelResponse(" << protocol << ", "
                   << extensions << ")";

  if (!m_client)
    return;

  m_client->didConnect(this, protocol, extensions);
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnDataFrame(bool fin,
                                      mojom::blink::WebSocketMessageType type,
                                      const Vector<uint8_t>& data) {
  NETWORK_DVLOG(1) << this << " OnDataFrame(" << fin << ", " << type << ", "
                   << "(data size = " << data.size() << "))";
  if (!m_client)
    return;

  WebSocketHandle::MessageType typeToPass =
      WebSocketHandle::MessageTypeContinuation;
  switch (type) {
    case mojom::blink::WebSocketMessageType::CONTINUATION:
      typeToPass = WebSocketHandle::MessageTypeContinuation;
      break;
    case mojom::blink::WebSocketMessageType::TEXT:
      typeToPass = WebSocketHandle::MessageTypeText;
      break;
    case mojom::blink::WebSocketMessageType::BINARY:
      typeToPass = WebSocketHandle::MessageTypeBinary;
      break;
  }
  const char* dataToPass =
      reinterpret_cast<const char*>(data.isEmpty() ? nullptr : &data[0]);
  m_client->didReceiveData(this, fin, typeToPass, dataToPass, data.size());
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnFlowControl(int64_t quota) {
  NETWORK_DVLOG(1) << this << " OnFlowControl(" << quota << ")";
  if (!m_client)
    return;

  m_client->didReceiveFlowControl(this, quota);
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnDropChannel(bool wasClean,
                                        uint16_t code,
                                        const String& reason) {
  NETWORK_DVLOG(1) << this << " OnDropChannel(" << wasClean << ", " << code
                   << ", " << reason << ")";

  WebSocketHandleClient* client = m_client;
  disconnect();
  if (!client)
    return;

  client->didClose(this, wasClean, code, reason);
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnClosingHandshake() {
  NETWORK_DVLOG(1) << this << " OnClosingHandshake()";
  if (!m_client)
    return;

  m_client->didStartClosingHandshake(this);
  // |this| can be deleted here.
}

}  // namespace blink
