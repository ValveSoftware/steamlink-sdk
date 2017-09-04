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

#include "modules/websockets/DocumentWebSocketChannel.h"

#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/fetch/UniqueIdentifier.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/MixedContentChecker.h"
#include "modules/websockets/InspectorWebSocketEvents.h"
#include "modules/websockets/WebSocketChannelClient.h"
#include "modules/websockets/WebSocketFrame.h"
#include "modules/websockets/WebSocketHandleImpl.h"
#include "platform/network/NetworkLog.h"
#include "platform/network/WebSocketHandshakeRequest.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class DocumentWebSocketChannel::BlobLoader final
    : public GarbageCollectedFinalized<DocumentWebSocketChannel::BlobLoader>,
      public FileReaderLoaderClient {
 public:
  BlobLoader(PassRefPtr<BlobDataHandle>, DocumentWebSocketChannel*);
  ~BlobLoader() override {}

  void cancel();

  // FileReaderLoaderClient functions.
  void didStartLoading() override {}
  void didReceiveData() override {}
  void didFinishLoading() override;
  void didFail(FileError::ErrorCode) override;

  DEFINE_INLINE_TRACE() { visitor->trace(m_channel); }

 private:
  Member<DocumentWebSocketChannel> m_channel;
  std::unique_ptr<FileReaderLoader> m_loader;
};

class DocumentWebSocketChannel::Message
    : public GarbageCollectedFinalized<DocumentWebSocketChannel::Message> {
 public:
  explicit Message(const CString&);
  explicit Message(PassRefPtr<BlobDataHandle>);
  explicit Message(DOMArrayBuffer*);
  // For WorkerWebSocketChannel
  explicit Message(std::unique_ptr<Vector<char>>, MessageType);
  // Close message
  Message(unsigned short code, const String& reason);

  DEFINE_INLINE_TRACE() { visitor->trace(arrayBuffer); }

  MessageType type;

  CString text;
  RefPtr<BlobDataHandle> blobDataHandle;
  Member<DOMArrayBuffer> arrayBuffer;
  std::unique_ptr<Vector<char>> vectorData;
  unsigned short code;
  String reason;
};

DocumentWebSocketChannel::BlobLoader::BlobLoader(
    PassRefPtr<BlobDataHandle> blobDataHandle,
    DocumentWebSocketChannel* channel)
    : m_channel(channel),
      m_loader(
          FileReaderLoader::create(FileReaderLoader::ReadAsArrayBuffer, this)) {
  m_loader->start(channel->document(), std::move(blobDataHandle));
}

void DocumentWebSocketChannel::BlobLoader::cancel() {
  m_loader->cancel();
  // didFail will be called immediately.
  // |this| is deleted here.
}

void DocumentWebSocketChannel::BlobLoader::didFinishLoading() {
  m_channel->didFinishLoadingBlob(m_loader->arrayBufferResult());
  // |this| is deleted here.
}

void DocumentWebSocketChannel::BlobLoader::didFail(
    FileError::ErrorCode errorCode) {
  m_channel->didFailLoadingBlob(errorCode);
  // |this| is deleted here.
}

DocumentWebSocketChannel::DocumentWebSocketChannel(
    Document* document,
    WebSocketChannelClient* client,
    std::unique_ptr<SourceLocation> location,
    WebSocketHandle* handle)
    : m_handle(wrapUnique(handle ? handle : new WebSocketHandleImpl())),
      m_client(client),
      m_identifier(createUniqueIdentifier()),
      m_document(document),
      m_sendingQuota(0),
      m_receivedDataSizeForFlowControl(
          receivedDataSizeForFlowControlHighWaterMark * 2),  // initial quota
      m_sentSizeOfTopMessage(0),
      m_locationAtConstruction(std::move(location)) {}

DocumentWebSocketChannel::~DocumentWebSocketChannel() {
  DCHECK(!m_blobLoader);
}

bool DocumentWebSocketChannel::connect(const KURL& url,
                                       const String& protocol) {
  NETWORK_DVLOG(1) << this << " connect()";
  if (!m_handle)
    return false;

  if (document()->frame()) {
    if (MixedContentChecker::shouldBlockWebSocket(document()->frame(), url))
      return false;
  }
  if (MixedContentChecker::isMixedContent(document()->getSecurityOrigin(),
                                          url)) {
    String message =
        "Connecting to a non-secure WebSocket server from a secure origin is "
        "deprecated.";
    document()->addConsoleMessage(
        ConsoleMessage::create(JSMessageSource, WarningMessageLevel, message));
  }

  m_url = url;
  Vector<String> protocols;
  // Avoid placing an empty token in the Vector when the protocol string is
  // empty.
  if (!protocol.isEmpty()) {
    // Since protocol is already verified and escaped, we can simply split
    // it.
    protocol.split(", ", true, protocols);
  }

  if (document()->frame() &&
      document()->frame()->interfaceProvider() !=
          InterfaceProvider::getEmptyInterfaceProvider()) {
    // Initialize the WebSocketHandle with the frame's InterfaceProvider to
    // provide the WebSocket implementation with context about this frame.
    // This is important so that the browser can show UI associated with
    // the WebSocket (e.g., for certificate errors).
    m_handle->initialize(document()->frame()->interfaceProvider());
  } else {
    m_handle->initialize(Platform::current()->interfaceProvider());
  }
  m_handle->connect(url, protocols, document()->getSecurityOrigin(),
                    document()->firstPartyForCookies(), document()->userAgent(),
                    this);

  flowControlIfNecessary();
  TRACE_EVENT_INSTANT1("devtools.timeline", "WebSocketCreate",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorWebSocketCreateEvent::data(
                           document(), m_identifier, url, protocol));
  InspectorInstrumentation::didCreateWebSocket(document(), m_identifier, url,
                                               protocol);
  return true;
}

void DocumentWebSocketChannel::send(const CString& message) {
  NETWORK_DVLOG(1) << this << " sendText(" << message << ")";
  // FIXME: Change the inspector API to show the entire message instead
  // of individual frames.
  InspectorInstrumentation::didSendWebSocketFrame(
      document(), m_identifier, WebSocketFrame::OpCodeText, true,
      message.data(), message.length());
  m_messages.append(new Message(message));
  processSendQueue();
}

void DocumentWebSocketChannel::send(PassRefPtr<BlobDataHandle> blobDataHandle) {
  NETWORK_DVLOG(1) << this << " sendBlob(" << blobDataHandle->uuid() << ", "
                   << blobDataHandle->type() << ", " << blobDataHandle->size()
                   << ")";
  // FIXME: Change the inspector API to show the entire message instead
  // of individual frames.
  // FIXME: We can't access the data here.
  // Since Binary data are not displayed in Inspector, this does not
  // affect actual behavior.
  InspectorInstrumentation::didSendWebSocketFrame(
      document(), m_identifier, WebSocketFrame::OpCodeBinary, true, "", 0);
  m_messages.append(new Message(std::move(blobDataHandle)));
  processSendQueue();
}

void DocumentWebSocketChannel::send(const DOMArrayBuffer& buffer,
                                    unsigned byteOffset,
                                    unsigned byteLength) {
  NETWORK_DVLOG(1) << this << " sendArrayBuffer(" << buffer.data() << ", "
                   << byteOffset << ", " << byteLength << ")";
  // FIXME: Change the inspector API to show the entire message instead
  // of individual frames.
  InspectorInstrumentation::didSendWebSocketFrame(
      document(), m_identifier, WebSocketFrame::OpCodeBinary, true,
      static_cast<const char*>(buffer.data()) + byteOffset, byteLength);
  // buffer.slice copies its contents.
  // FIXME: Reduce copy by sending the data immediately when we don't need to
  // queue the data.
  m_messages.append(
      new Message(buffer.slice(byteOffset, byteOffset + byteLength)));
  processSendQueue();
}

void DocumentWebSocketChannel::sendTextAsCharVector(
    std::unique_ptr<Vector<char>> data) {
  NETWORK_DVLOG(1) << this << " sendTextAsCharVector("
                   << static_cast<void*>(data.get()) << ", " << data->size()
                   << ")";
  // FIXME: Change the inspector API to show the entire message instead
  // of individual frames.
  InspectorInstrumentation::didSendWebSocketFrame(
      document(), m_identifier, WebSocketFrame::OpCodeText, true, data->data(),
      data->size());
  m_messages.append(new Message(std::move(data), MessageTypeTextAsCharVector));
  processSendQueue();
}

void DocumentWebSocketChannel::sendBinaryAsCharVector(
    std::unique_ptr<Vector<char>> data) {
  NETWORK_DVLOG(1) << this << " sendBinaryAsCharVector("
                   << static_cast<void*>(data.get()) << ", " << data->size()
                   << ")";
  // FIXME: Change the inspector API to show the entire message instead
  // of individual frames.
  InspectorInstrumentation::didSendWebSocketFrame(
      document(), m_identifier, WebSocketFrame::OpCodeBinary, true,
      data->data(), data->size());
  m_messages.append(
      new Message(std::move(data), MessageTypeBinaryAsCharVector));
  processSendQueue();
}

void DocumentWebSocketChannel::close(int code, const String& reason) {
  NETWORK_DVLOG(1) << this << " close(" << code << ", " << reason << ")";
  DCHECK(m_handle);
  unsigned short codeToSend = static_cast<unsigned short>(
      code == CloseEventCodeNotSpecified ? CloseEventCodeNoStatusRcvd : code);
  m_messages.append(new Message(codeToSend, reason));
  processSendQueue();
}

void DocumentWebSocketChannel::fail(const String& reason,
                                    MessageLevel level,
                                    std::unique_ptr<SourceLocation> location) {
  NETWORK_DVLOG(1) << this << " fail(" << reason << ")";
  // m_handle and m_client can be null here.

  InspectorInstrumentation::didReceiveWebSocketFrameError(document(),
                                                          m_identifier, reason);
  const String message = "WebSocket connection to '" + m_url.elidedString() +
                         "' failed: " + reason;
  document()->addConsoleMessage(ConsoleMessage::create(
      JSMessageSource, level, message, std::move(location)));

  if (m_client)
    m_client->didError();
  // |reason| is only for logging and should not be provided for scripts,
  // hence close reason must be empty.
  handleDidClose(false, CloseEventCodeAbnormalClosure, String());
  // handleDidClose may delete this object.
}

void DocumentWebSocketChannel::disconnect() {
  NETWORK_DVLOG(1) << this << " disconnect()";
  if (m_identifier) {
    TRACE_EVENT_INSTANT1(
        "devtools.timeline", "WebSocketDestroy", TRACE_EVENT_SCOPE_THREAD,
        "data", InspectorWebSocketEvent::data(document(), m_identifier));
    InspectorInstrumentation::didCloseWebSocket(document(), m_identifier);
  }
  abortAsyncOperations();
  m_handle.reset();
  m_client = nullptr;
  m_identifier = 0;
}

DocumentWebSocketChannel::Message::Message(const CString& text)
    : type(MessageTypeText), text(text) {}

DocumentWebSocketChannel::Message::Message(
    PassRefPtr<BlobDataHandle> blobDataHandle)
    : type(MessageTypeBlob), blobDataHandle(blobDataHandle) {}

DocumentWebSocketChannel::Message::Message(DOMArrayBuffer* arrayBuffer)
    : type(MessageTypeArrayBuffer), arrayBuffer(arrayBuffer) {}

DocumentWebSocketChannel::Message::Message(
    std::unique_ptr<Vector<char>> vectorData,
    MessageType type)
    : type(type), vectorData(std::move(vectorData)) {
  DCHECK(type == MessageTypeTextAsCharVector ||
         type == MessageTypeBinaryAsCharVector);
}

DocumentWebSocketChannel::Message::Message(unsigned short code,
                                           const String& reason)
    : type(MessageTypeClose), code(code), reason(reason) {}

void DocumentWebSocketChannel::sendInternal(
    WebSocketHandle::MessageType messageType,
    const char* data,
    size_t totalSize,
    uint64_t* consumedBufferedAmount) {
  WebSocketHandle::MessageType frameType =
      m_sentSizeOfTopMessage ? WebSocketHandle::MessageTypeContinuation
                             : messageType;
  DCHECK_GE(totalSize, m_sentSizeOfTopMessage);
  // The first cast is safe since the result of min() never exceeds
  // the range of size_t. The second cast is necessary to compile
  // min() on ILP32.
  size_t size = static_cast<size_t>(
      std::min(m_sendingQuota,
               static_cast<uint64_t>(totalSize - m_sentSizeOfTopMessage)));
  bool final = (m_sentSizeOfTopMessage + size == totalSize);

  m_handle->send(final, frameType, data + m_sentSizeOfTopMessage, size);

  m_sentSizeOfTopMessage += size;
  m_sendingQuota -= size;
  *consumedBufferedAmount += size;

  if (final) {
    m_messages.removeFirst();
    m_sentSizeOfTopMessage = 0;
  }
}

void DocumentWebSocketChannel::processSendQueue() {
  DCHECK(m_handle);
  uint64_t consumedBufferedAmount = 0;
  while (!m_messages.isEmpty() && !m_blobLoader) {
    Message* message = m_messages.first().get();
    if (m_sendingQuota == 0 && message->type != MessageTypeClose)
      break;
    switch (message->type) {
      case MessageTypeText:
        sendInternal(WebSocketHandle::MessageTypeText, message->text.data(),
                     message->text.length(), &consumedBufferedAmount);
        break;
      case MessageTypeBlob:
        DCHECK(!m_blobLoader);
        m_blobLoader = new BlobLoader(message->blobDataHandle, this);
        break;
      case MessageTypeArrayBuffer:
        sendInternal(WebSocketHandle::MessageTypeBinary,
                     static_cast<const char*>(message->arrayBuffer->data()),
                     message->arrayBuffer->byteLength(),
                     &consumedBufferedAmount);
        break;
      case MessageTypeTextAsCharVector:
        sendInternal(WebSocketHandle::MessageTypeText,
                     message->vectorData->data(), message->vectorData->size(),
                     &consumedBufferedAmount);
        break;
      case MessageTypeBinaryAsCharVector:
        sendInternal(WebSocketHandle::MessageTypeBinary,
                     message->vectorData->data(), message->vectorData->size(),
                     &consumedBufferedAmount);
        break;
      case MessageTypeClose: {
        // No message should be sent from now on.
        DCHECK_EQ(m_messages.size(), 1u);
        DCHECK_EQ(m_sentSizeOfTopMessage, 0u);
        m_handle->close(message->code, message->reason);
        m_messages.removeFirst();
        break;
      }
    }
  }
  if (m_client && consumedBufferedAmount > 0)
    m_client->didConsumeBufferedAmount(consumedBufferedAmount);
}

void DocumentWebSocketChannel::flowControlIfNecessary() {
  if (!m_handle ||
      m_receivedDataSizeForFlowControl <
          receivedDataSizeForFlowControlHighWaterMark) {
    return;
  }
  m_handle->flowControl(m_receivedDataSizeForFlowControl);
  m_receivedDataSizeForFlowControl = 0;
}

void DocumentWebSocketChannel::abortAsyncOperations() {
  if (m_blobLoader) {
    m_blobLoader->cancel();
    m_blobLoader.clear();
  }
}

void DocumentWebSocketChannel::handleDidClose(bool wasClean,
                                              unsigned short code,
                                              const String& reason) {
  m_handle.reset();
  abortAsyncOperations();
  if (!m_client) {
    return;
  }
  WebSocketChannelClient* client = m_client;
  m_client = nullptr;
  WebSocketChannelClient::ClosingHandshakeCompletionStatus status =
      wasClean ? WebSocketChannelClient::ClosingHandshakeComplete
               : WebSocketChannelClient::ClosingHandshakeIncomplete;
  client->didClose(status, code, reason);
  // client->didClose may delete this object.
}

Document* DocumentWebSocketChannel::document() {
  return m_document;
}

void DocumentWebSocketChannel::didConnect(WebSocketHandle* handle,
                                          const String& selectedProtocol,
                                          const String& extensions) {
  NETWORK_DVLOG(1) << this << " didConnect(" << handle << ", "
                   << String(selectedProtocol) << ", " << String(extensions)
                   << ")";

  DCHECK(m_handle);
  DCHECK_EQ(handle, m_handle.get());
  DCHECK(m_client);

  m_client->didConnect(selectedProtocol, extensions);
}

void DocumentWebSocketChannel::didStartOpeningHandshake(
    WebSocketHandle* handle,
    PassRefPtr<WebSocketHandshakeRequest> request) {
  NETWORK_DVLOG(1) << this << " didStartOpeningHandshake(" << handle << ")";

  DCHECK(m_handle);
  DCHECK_EQ(handle, m_handle.get());

  TRACE_EVENT_INSTANT1("devtools.timeline", "WebSocketSendHandshakeRequest",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorWebSocketEvent::data(document(), m_identifier));
  InspectorInstrumentation::willSendWebSocketHandshakeRequest(
      document(), m_identifier, request.get());
  m_handshakeRequest = request;
}

void DocumentWebSocketChannel::didFinishOpeningHandshake(
    WebSocketHandle* handle,
    const WebSocketHandshakeResponse* response) {
  NETWORK_DVLOG(1) << this << " didFinishOpeningHandshake(" << handle << ")";

  DCHECK(m_handle);
  DCHECK_EQ(handle, m_handle.get());

  TRACE_EVENT_INSTANT1("devtools.timeline", "WebSocketReceiveHandshakeResponse",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorWebSocketEvent::data(document(), m_identifier));
  InspectorInstrumentation::didReceiveWebSocketHandshakeResponse(
      document(), m_identifier, m_handshakeRequest.get(), response);
  m_handshakeRequest.clear();
}

void DocumentWebSocketChannel::didFail(WebSocketHandle* handle,
                                       const String& message) {
  NETWORK_DVLOG(1) << this << " didFail(" << handle << ", " << String(message)
                   << ")";

  DCHECK(m_handle);
  DCHECK_EQ(handle, m_handle.get());

  // This function is called when the browser is required to fail the
  // WebSocketConnection. Hence we fail this channel by calling
  // |this->failAsError| function.
  failAsError(message);
  // |this| may be deleted.
}

void DocumentWebSocketChannel::didReceiveData(WebSocketHandle* handle,
                                              bool fin,
                                              WebSocketHandle::MessageType type,
                                              const char* data,
                                              size_t size) {
  NETWORK_DVLOG(1) << this << " didReceiveData(" << handle << ", " << fin
                   << ", " << type << ", (" << static_cast<const void*>(data)
                   << ", " << size << "))";

  DCHECK(m_handle);
  DCHECK_EQ(handle, m_handle.get());
  DCHECK(m_client);
  // Non-final frames cannot be empty.
  DCHECK(fin || size);

  switch (type) {
    case WebSocketHandle::MessageTypeText:
      DCHECK(m_receivingMessageData.isEmpty());
      m_receivingMessageTypeIsText = true;
      break;
    case WebSocketHandle::MessageTypeBinary:
      DCHECK(m_receivingMessageData.isEmpty());
      m_receivingMessageTypeIsText = false;
      break;
    case WebSocketHandle::MessageTypeContinuation:
      DCHECK(!m_receivingMessageData.isEmpty());
      break;
  }

  m_receivingMessageData.append(data, size);
  m_receivedDataSizeForFlowControl += size;
  flowControlIfNecessary();
  if (!fin) {
    return;
  }
  // FIXME: Change the inspector API to show the entire message instead
  // of individual frames.
  WebSocketFrame::OpCode opcode = m_receivingMessageTypeIsText
                                      ? WebSocketFrame::OpCodeText
                                      : WebSocketFrame::OpCodeBinary;
  WebSocketFrame frame(opcode, m_receivingMessageData.data(),
                       m_receivingMessageData.size(), WebSocketFrame::Final);
  InspectorInstrumentation::didReceiveWebSocketFrame(
      document(), m_identifier, frame.opCode, frame.masked, frame.payload,
      frame.payloadLength);
  if (m_receivingMessageTypeIsText) {
    String message = m_receivingMessageData.isEmpty()
                         ? emptyString()
                         : String::fromUTF8(m_receivingMessageData.data(),
                                            m_receivingMessageData.size());
    m_receivingMessageData.clear();
    if (message.isNull()) {
      failAsError("Could not decode a text frame as UTF-8.");
      // failAsError may delete this object.
    } else {
      m_client->didReceiveTextMessage(message);
    }
  } else {
    std::unique_ptr<Vector<char>> binaryData = wrapUnique(new Vector<char>);
    binaryData->swap(m_receivingMessageData);
    m_client->didReceiveBinaryMessage(std::move(binaryData));
  }
}

void DocumentWebSocketChannel::didClose(WebSocketHandle* handle,
                                        bool wasClean,
                                        unsigned short code,
                                        const String& reason) {
  NETWORK_DVLOG(1) << this << " didClose(" << handle << ", " << wasClean << ", "
                   << code << ", " << String(reason) << ")";

  DCHECK(m_handle);
  DCHECK_EQ(handle, m_handle.get());

  m_handle.reset();

  if (m_identifier) {
    TRACE_EVENT_INSTANT1(
        "devtools.timeline", "WebSocketDestroy", TRACE_EVENT_SCOPE_THREAD,
        "data", InspectorWebSocketEvent::data(document(), m_identifier));
    InspectorInstrumentation::didCloseWebSocket(document(), m_identifier);
    m_identifier = 0;
  }

  handleDidClose(wasClean, code, reason);
  // handleDidClose may delete this object.
}

void DocumentWebSocketChannel::didReceiveFlowControl(WebSocketHandle* handle,
                                                     int64_t quota) {
  NETWORK_DVLOG(1) << this << " didReceiveFlowControl(" << handle << ", "
                   << quota << ")";

  DCHECK(m_handle);
  DCHECK_EQ(handle, m_handle.get());
  DCHECK_GE(quota, 0);

  m_sendingQuota += quota;
  processSendQueue();
}

void DocumentWebSocketChannel::didStartClosingHandshake(
    WebSocketHandle* handle) {
  NETWORK_DVLOG(1) << this << " didStartClosingHandshake(" << handle << ")";

  DCHECK(m_handle);
  DCHECK_EQ(handle, m_handle.get());

  if (m_client)
    m_client->didStartClosingHandshake();
}

void DocumentWebSocketChannel::didFinishLoadingBlob(DOMArrayBuffer* buffer) {
  m_blobLoader.clear();
  DCHECK(m_handle);
  // The loaded blob is always placed on m_messages[0].
  DCHECK_GT(m_messages.size(), 0u);
  DCHECK_EQ(m_messages.first()->type, MessageTypeBlob);
  // We replace it with the loaded blob.
  m_messages.first() = new Message(buffer);
  processSendQueue();
}

void DocumentWebSocketChannel::didFailLoadingBlob(
    FileError::ErrorCode errorCode) {
  m_blobLoader.clear();
  if (errorCode == FileError::kAbortErr) {
    // The error is caused by cancel().
    return;
  }
  // FIXME: Generate human-friendly reason message.
  failAsError("Failed to load Blob: error code = " + String::number(errorCode));
  // |this| can be deleted here.
}

DEFINE_TRACE(DocumentWebSocketChannel) {
  visitor->trace(m_blobLoader);
  visitor->trace(m_messages);
  visitor->trace(m_client);
  visitor->trace(m_document);
  WebSocketChannel::trace(visitor);
}

std::ostream& operator<<(std::ostream& ostream,
                         const DocumentWebSocketChannel* channel) {
  return ostream << "DocumentWebSocketChannel "
                 << static_cast<const void*>(channel);
}

}  // namespace blink
