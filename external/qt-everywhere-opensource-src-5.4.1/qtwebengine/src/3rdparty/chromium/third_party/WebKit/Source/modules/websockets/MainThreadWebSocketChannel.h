/*
 * Copyright (C) 2011, 2012 Google Inc.  All rights reserved.
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

#ifndef MainThreadWebSocketChannel_h
#define MainThreadWebSocketChannel_h

#include "core/fileapi/FileError.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/frame/ConsoleTypes.h"
#include "modules/websockets/WebSocketChannel.h"
#include "modules/websockets/WebSocketDeflateFramer.h"
#include "modules/websockets/WebSocketFrame.h"
#include "modules/websockets/WebSocketHandshake.h"
#include "modules/websockets/WebSocketPerMessageDeflate.h"
#include "platform/Timer.h"
#include "platform/network/SocketStreamHandleClient.h"
#include "wtf/Deque.h"
#include "wtf/Forward.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"

namespace WebCore {

class BlobDataHandle;
class Document;
class FileReaderLoader;
class SocketStreamHandle;
class SocketStreamError;
class WebSocketChannelClient;

class MainThreadWebSocketChannel FINAL : public WebSocketChannel, public SocketStreamHandleClient, public FileReaderLoaderClient {
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
public:
    // You can specify the source file and the line number information
    // explicitly by passing the last parameter.
    // In the usual case, they are set automatically and you don't have to
    // pass it.
    static PassRefPtrWillBeRawPtr<MainThreadWebSocketChannel> create(Document* document, WebSocketChannelClient* client, const String& sourceURL = String(), unsigned lineNumber = 0)
    {
        return adoptRefWillBeRefCountedGarbageCollected(new MainThreadWebSocketChannel(document, client, sourceURL, lineNumber));
    }
    virtual ~MainThreadWebSocketChannel();

    // WebSocketChannel functions.
    virtual bool connect(const KURL&, const String& protocol) OVERRIDE;
    virtual WebSocketChannel::SendResult send(const String& message) OVERRIDE;
    virtual WebSocketChannel::SendResult send(const ArrayBuffer&, unsigned byteOffset, unsigned byteLength) OVERRIDE;
    virtual WebSocketChannel::SendResult send(PassRefPtr<BlobDataHandle>) OVERRIDE;
    virtual WebSocketChannel::SendResult send(PassOwnPtr<Vector<char> > data) OVERRIDE;
    // Start closing handshake. Use the CloseEventCodeNotSpecified for the code
    // argument to omit payload.
    virtual void close(int code, const String& reason) OVERRIDE;
    virtual void fail(const String& reason, MessageLevel, const String&, unsigned lineNumber) OVERRIDE;
    virtual void disconnect() OVERRIDE;

    virtual void suspend() OVERRIDE;
    virtual void resume() OVERRIDE;

    // SocketStreamHandleClient functions.
    virtual void didOpenSocketStream(SocketStreamHandle*) OVERRIDE;
    virtual void didCloseSocketStream(SocketStreamHandle*) OVERRIDE;
    virtual void didReceiveSocketStreamData(SocketStreamHandle*, const char*, int) OVERRIDE;
    virtual void didConsumeBufferedAmount(SocketStreamHandle*, size_t consumed) OVERRIDE;
    virtual void didFailSocketStream(SocketStreamHandle*, const SocketStreamError&) OVERRIDE;

    // FileReaderLoaderClient functions.
    virtual void didStartLoading() OVERRIDE;
    virtual void didReceiveData() OVERRIDE;
    virtual void didFinishLoading() OVERRIDE;
    virtual void didFail(FileError::ErrorCode) OVERRIDE;

private:
    MainThreadWebSocketChannel(Document*, WebSocketChannelClient*, const String&, unsigned);

    class FramingOverhead {
    public:
        FramingOverhead(WebSocketFrame::OpCode opcode, size_t frameDataSize, size_t originalPayloadLength)
            : m_opcode(opcode)
            , m_frameDataSize(frameDataSize)
            , m_originalPayloadLength(originalPayloadLength)
        {
        }

        WebSocketFrame::OpCode opcode() const { return m_opcode; }
        size_t frameDataSize() const { return m_frameDataSize; }
        size_t originalPayloadLength() const { return m_originalPayloadLength; }

    private:
        WebSocketFrame::OpCode m_opcode;
        size_t m_frameDataSize;
        size_t m_originalPayloadLength;
    };

    void clearDocument();

    void disconnectHandle();

    // Calls didReceiveMessageError() on m_client if we haven't yet.
    void callDidReceiveMessageError();

    bool appendToBuffer(const char* data, size_t len);
    void skipBuffer(size_t len);
    // Repeats parsing data from m_buffer until instructed to stop.
    void processBuffer();
    // Parses a handshake response or one frame from m_buffer and processes it.
    bool processOneItemFromBuffer();
    void resumeTimerFired(Timer<MainThreadWebSocketChannel>*);
    void startClosingHandshake(int code, const String& reason);
    void closingTimerFired(Timer<MainThreadWebSocketChannel>*);

    // Parses one frame from m_buffer and processes it.
    bool processFrame();

    // It is allowed to send a Blob as a binary frame if hybi-10 protocol is in use. Sending a Blob
    // can be delayed because it must be read asynchronously. Other types of data (String or
    // ArrayBuffer) may also be blocked by preceding sending request of a Blob.
    //
    // To address this situation, messages to be sent need to be stored in a queue. Whenever a new
    // data frame is going to be sent, it first must go to the queue. Items in the queue are processed
    // in the order they were put into the queue. Sending request of a Blob blocks further processing
    // until the Blob is completely read and sent to the socket stream.
    enum QueuedFrameType {
        QueuedFrameTypeString,
        QueuedFrameTypeVector,
        QueuedFrameTypeBlob
    };
    struct QueuedFrame {
        WebSocketFrame::OpCode opCode;
        QueuedFrameType frameType;
        // Only one of the following items is used, according to the value of frameType.
        CString stringData;
        Vector<char> vectorData;
        RefPtr<BlobDataHandle> blobData;
    };
    void enqueueTextFrame(const CString&);
    void enqueueRawFrame(WebSocketFrame::OpCode, const char* data, size_t dataLength);
    void enqueueVector(WebSocketFrame::OpCode, PassOwnPtr<Vector<char> >);
    void enqueueBlobFrame(WebSocketFrame::OpCode, PassRefPtr<BlobDataHandle>);

    void failAsError(const String& reason) { fail(reason, ErrorMessageLevel, m_sourceURLAtConstruction, m_lineNumberAtConstruction); }
    void processOutgoingFrameQueue();
    void abortOutgoingFrameQueue();

    enum OutgoingFrameQueueStatus {
        // It is allowed to put a new item into the queue.
        OutgoingFrameQueueOpen,
        // Close frame has already been put into the queue but may not have been sent yet;
        // m_handle->close() will be called as soon as the queue is cleared. It is not
        // allowed to put a new item into the queue.
        OutgoingFrameQueueClosing,
        // Close frame has been sent or the queue was aborted. It is not allowed to put
        // a new item to the queue.
        OutgoingFrameQueueClosed
    };

    // In principle, this method is called only by processOutgoingFrameQueue().
    // It does work necessary to build frames including Blob loading for queued
    // data in order. Calling this method directly jumps in the process.
    bool sendFrame(WebSocketFrame::OpCode, const char* data, size_t dataLength);

    enum BlobLoaderStatus {
        BlobLoaderNotStarted,
        BlobLoaderStarted,
        BlobLoaderFinished,
        BlobLoaderFailed
    };

    enum ChannelState {
        ChannelIdle,
        ChannelClosing,
        ChannelClosed
    };

    Document* m_document;
    WebSocketChannelClient* m_client;
    OwnPtr<WebSocketHandshake> m_handshake;
    RefPtr<SocketStreamHandle> m_handle;
    Vector<char> m_buffer;

    Timer<MainThreadWebSocketChannel> m_resumeTimer;
    bool m_suspended;
    bool m_didFailOfClientAlreadyRun;
    // Set to true iff this instance called disconnect() on m_handle.
    bool m_hasCalledDisconnectOnHandle;
    bool m_receivedClosingHandshake;
    Timer<MainThreadWebSocketChannel> m_closingTimer;
    ChannelState m_state;
    bool m_shouldDiscardReceivedData;

    unsigned long m_identifier; // m_identifier == 0 means that we could not obtain a valid identifier.

    // Private members only for hybi-10 protocol.
    bool m_hasContinuousFrame;
    WebSocketFrame::OpCode m_continuousFrameOpCode;
    Vector<char> m_continuousFrameData;
    unsigned short m_closeEventCode;
    String m_closeEventReason;

    Deque<OwnPtr<QueuedFrame> > m_outgoingFrameQueue;
    OutgoingFrameQueueStatus m_outgoingFrameQueueStatus;
    Deque<FramingOverhead> m_framingOverheadQueue;
    // The number of bytes that are already consumed (i.e. sent) in the
    // current frame.
    size_t m_numConsumedBytesInCurrentFrame;

    // FIXME: Load two or more Blobs simultaneously for better performance.
    OwnPtr<FileReaderLoader> m_blobLoader;
    BlobLoaderStatus m_blobLoaderStatus;

    // Source code position where construction happened. To be used to show a
    // console message where no JS callstack info available.
    String m_sourceURLAtConstruction;
    unsigned m_lineNumberAtConstruction;

    WebSocketPerMessageDeflate m_perMessageDeflate;

    WebSocketDeflateFramer m_deflateFramer;
};

} // namespace WebCore

#endif // MainThreadWebSocketChannel_h
