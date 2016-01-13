// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "modules/websockets/WebSocket.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/ExceptionCode.h"
#include "core/fileapi/Blob.h"
#include "core/frame/ConsoleTypes.h"
#include "core/testing/DummyPageHolder.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/OwnPtr.h"
#include "wtf/Uint8Array.h"
#include "wtf/Vector.h"
#include "wtf/testing/WTFTestHelpers.h"
#include "wtf/text/WTFString.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <v8.h>

using testing::_;
using testing::AnyNumber;
using testing::InSequence;
using testing::Ref;
using testing::Return;

namespace WebCore {

namespace {

typedef testing::StrictMock<testing::MockFunction<void(int)> > Checkpoint;  // NOLINT

class MockWebSocketChannel : public WebSocketChannel {
public:
    static PassRefPtrWillBeRawPtr<MockWebSocketChannel> create()
    {
        return adoptRefWillBeRefCountedGarbageCollected(new testing::StrictMock<MockWebSocketChannel>());
    }

    virtual ~MockWebSocketChannel()
    {
    }

    MOCK_METHOD2(connect, bool(const KURL&, const String&));
    MOCK_METHOD1(send, SendResult(const String&));
    MOCK_METHOD3(send, SendResult(const ArrayBuffer&, unsigned, unsigned));
    MOCK_METHOD1(send, SendResult(PassRefPtr<BlobDataHandle>));
    MOCK_METHOD1(send, SendResult(PassOwnPtr<Vector<char> >));
    MOCK_CONST_METHOD0(bufferedAmount, unsigned long());
    MOCK_METHOD2(close, void(int, const String&));
    MOCK_METHOD4(fail, void(const String&, MessageLevel, const String&, unsigned));
    MOCK_METHOD0(disconnect, void());
    MOCK_METHOD0(suspend, void());
    MOCK_METHOD0(resume, void());

    MockWebSocketChannel()
    {
    }
};

class WebSocketWithMockChannel FINAL : public WebSocket {
public:
    static PassRefPtrWillBeRawPtr<WebSocketWithMockChannel> create(ExecutionContext* context)
    {
        RefPtrWillBeRawPtr<WebSocketWithMockChannel> websocket = adoptRefWillBeRefCountedGarbageCollected(new WebSocketWithMockChannel(context));
        websocket->suspendIfNeeded();
        return websocket.release();
    }

    MockWebSocketChannel* channel() { return m_channel.get(); }

    virtual PassRefPtrWillBeRawPtr<WebSocketChannel> createChannel(ExecutionContext*, WebSocketChannelClient*) OVERRIDE
    {
        ASSERT(!m_hasCreatedChannel);
        m_hasCreatedChannel = true;
        return m_channel.get();
    }

    virtual void trace(Visitor* visitor) OVERRIDE
    {
        visitor->trace(m_channel);
        WebSocket::trace(visitor);
    }

private:
    WebSocketWithMockChannel(ExecutionContext* context)
        : WebSocket(context)
        , m_channel(MockWebSocketChannel::create())
        , m_hasCreatedChannel(false) { }

    RefPtrWillBeMember<MockWebSocketChannel> m_channel;
    bool m_hasCreatedChannel;
};

class WebSocketTestBase {
public:
    WebSocketTestBase()
        : m_pageHolder(DummyPageHolder::create())
        , m_websocket(WebSocketWithMockChannel::create(&m_pageHolder->document()))
        , m_executionScope(v8::Isolate::GetCurrent())
        , m_exceptionState(ExceptionState::ConstructionContext, "property", "interface", m_executionScope.scriptState()->context()->Global(), m_executionScope.isolate())
    {
    }

    virtual ~WebSocketTestBase()
    {
        if (!m_websocket)
            return;
        // These statements are needed to clear WebSocket::m_channel to
        // avoid ASSERTION failure on ~WebSocket.
        ASSERT(m_websocket->channel());
        ::testing::Mock::VerifyAndClear(m_websocket->channel());
        EXPECT_CALL(channel(), disconnect()).Times(AnyNumber());

        m_websocket->didClose(WebSocketChannelClient::ClosingHandshakeIncomplete, 1006, "");
        m_websocket.clear();
        Heap::collectAllGarbage();
    }

    MockWebSocketChannel& channel() { return *m_websocket->channel(); }

    OwnPtr<DummyPageHolder> m_pageHolder;
    RefPtrWillBePersistent<WebSocketWithMockChannel> m_websocket;
    V8TestingScope m_executionScope;
    ExceptionState m_exceptionState;
};

class WebSocketTest : public WebSocketTestBase, public ::testing::Test {
public:
};

TEST_F(WebSocketTest, connectToBadURL)
{
    m_websocket->connect("xxx", Vector<String>(), m_exceptionState);


    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(SyntaxError, m_exceptionState.code());
    EXPECT_EQ("The URL 'xxx' is invalid.", m_exceptionState.message());
    EXPECT_EQ(WebSocket::CLOSED, m_websocket->readyState());
}

TEST_F(WebSocketTest, connectToNonWsURL)
{
    m_websocket->connect("http://example.com/", Vector<String>(), m_exceptionState);


    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(SyntaxError, m_exceptionState.code());
    EXPECT_EQ("The URL's scheme must be either 'ws' or 'wss'. 'http' is not allowed.", m_exceptionState.message());
    EXPECT_EQ(WebSocket::CLOSED, m_websocket->readyState());
}

TEST_F(WebSocketTest, connectToURLHavingFragmentIdentifier)
{
    m_websocket->connect("ws://example.com/#fragment", Vector<String>(), m_exceptionState);


    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(SyntaxError, m_exceptionState.code());
    EXPECT_EQ("The URL contains a fragment identifier ('fragment'). Fragment identifiers are not allowed in WebSocket URLs.", m_exceptionState.message());
    EXPECT_EQ(WebSocket::CLOSED, m_websocket->readyState());
}

TEST_F(WebSocketTest, invalidPort)
{
    m_websocket->connect("ws://example.com:7", Vector<String>(), m_exceptionState);


    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(SecurityError, m_exceptionState.code());
    EXPECT_EQ("The port 7 is not allowed.", m_exceptionState.message());
    EXPECT_EQ(WebSocket::CLOSED, m_websocket->readyState());
}

// FIXME: Add a test for Content Security Policy.

TEST_F(WebSocketTest, invalidSubprotocols)
{
    Vector<String> subprotocols;
    subprotocols.append("@subprotocol-|'\"x\x01\x02\x03x");

    {
        InSequence s;
        EXPECT_CALL(channel(), disconnect());
    }

    m_websocket->connect("ws://example.com/", subprotocols, m_exceptionState);

    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(SyntaxError, m_exceptionState.code());
    EXPECT_EQ("The subprotocol '@subprotocol-|'\"x\\u0001\\u0002\\u0003x' is invalid.", m_exceptionState.message());
    EXPECT_EQ(WebSocket::CLOSED, m_websocket->readyState());
}

TEST_F(WebSocketTest, channelConnectSuccess)
{
    Vector<String> subprotocols;
    subprotocols.append("aa");
    subprotocols.append("bb");

    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/hoge"), String("aa, bb"))).WillOnce(Return(true));
    }

    m_websocket->connect("ws://example.com/hoge", Vector<String>(subprotocols), m_exceptionState);


    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CONNECTING, m_websocket->readyState());
    EXPECT_EQ(KURL(KURL(), "ws://example.com/hoge"), m_websocket->url());
}

TEST_F(WebSocketTest, channelConnectFail)
{
    Vector<String> subprotocols;
    subprotocols.append("aa");
    subprotocols.append("bb");

    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String("aa, bb"))).WillOnce(Return(false));
        EXPECT_CALL(channel(), disconnect());
    }

    m_websocket->connect("ws://example.com/", Vector<String>(subprotocols), m_exceptionState);


    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(SecurityError, m_exceptionState.code());
    EXPECT_EQ("An insecure WebSocket connection may not be initiated from a page loaded over HTTPS.", m_exceptionState.message());
    EXPECT_EQ(WebSocket::CLOSED, m_websocket->readyState());
}

TEST_F(WebSocketTest, isValidSubprotocolString)
{
    EXPECT_TRUE(WebSocket::isValidSubprotocolString("Helloworld!!"));
    EXPECT_FALSE(WebSocket::isValidSubprotocolString("Hello, world!!"));
    EXPECT_FALSE(WebSocket::isValidSubprotocolString(String()));
    EXPECT_FALSE(WebSocket::isValidSubprotocolString(""));

    const char validCharacters[] = "!#$%&'*+-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ^_`abcdefghijklmnopqrstuvwxyz|~";
    size_t length = strlen(validCharacters);
    for (size_t i = 0; i < length; ++i) {
        String s;
        s.append(static_cast<UChar>(validCharacters[i]));
        EXPECT_TRUE(WebSocket::isValidSubprotocolString(s));
    }
    for (size_t i = 0; i < 256; ++i) {
        if (std::find(validCharacters, validCharacters + length, static_cast<char>(i)) != validCharacters + length) {
            continue;
        }
        String s;
        s.append(static_cast<UChar>(i));
        EXPECT_FALSE(WebSocket::isValidSubprotocolString(s));
    }
}

TEST_F(WebSocketTest, connectSuccess)
{
    Vector<String> subprotocols;
    subprotocols.append("aa");
    subprotocols.append("bb");
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String("aa, bb"))).WillOnce(Return(true));
    }
    m_websocket->connect("ws://example.com/", subprotocols, m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CONNECTING, m_websocket->readyState());

    m_websocket->didConnect("bb", "cc");

    EXPECT_EQ(WebSocket::OPEN, m_websocket->readyState());
    EXPECT_EQ("bb", m_websocket->protocol());
    EXPECT_EQ("cc", m_websocket->extensions());
}

TEST_F(WebSocketTest, didClose)
{
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(channel(), disconnect());
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CONNECTING, m_websocket->readyState());

    m_websocket->didClose(WebSocketChannelClient::ClosingHandshakeIncomplete, 1006, "");

    EXPECT_EQ(WebSocket::CLOSED, m_websocket->readyState());
}

TEST_F(WebSocketTest, maximumReasonSize)
{
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(channel(), fail(_, _, _, _));
    }
    String reason;
    for (size_t i = 0; i < 123; ++i)
        reason.append("a");
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CONNECTING, m_websocket->readyState());

    m_websocket->close(1000, reason, m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CLOSING, m_websocket->readyState());
}

TEST_F(WebSocketTest, reasonSizeExceeding)
{
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
    }
    String reason;
    for (size_t i = 0; i < 124; ++i)
        reason.append("a");
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CONNECTING, m_websocket->readyState());

    m_websocket->close(1000, reason, m_exceptionState);

    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(SyntaxError, m_exceptionState.code());
    EXPECT_EQ("The message must not be greater than 123 bytes.", m_exceptionState.message());
    EXPECT_EQ(WebSocket::CONNECTING, m_websocket->readyState());
}

TEST_F(WebSocketTest, closeWhenConnecting)
{
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(channel(), fail(String("WebSocket is closed before the connection is established."), WarningMessageLevel, String(), 0));
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CONNECTING, m_websocket->readyState());

    m_websocket->close(1000, "bye", m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CLOSING, m_websocket->readyState());
}

TEST_F(WebSocketTest, close)
{
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(channel(), close(3005, String("bye")));
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CONNECTING, m_websocket->readyState());

    m_websocket->didConnect("", "");
    EXPECT_EQ(WebSocket::OPEN, m_websocket->readyState());
    m_websocket->close(3005, "bye", m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CLOSING, m_websocket->readyState());
}

TEST_F(WebSocketTest, closeWithoutReason)
{
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(channel(), close(3005, String()));
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CONNECTING, m_websocket->readyState());

    m_websocket->didConnect("", "");
    EXPECT_EQ(WebSocket::OPEN, m_websocket->readyState());
    m_websocket->close(3005, m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CLOSING, m_websocket->readyState());
}

TEST_F(WebSocketTest, closeWithoutCodeAndReason)
{
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(channel(), close(-1, String()));
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CONNECTING, m_websocket->readyState());

    m_websocket->didConnect("", "");
    EXPECT_EQ(WebSocket::OPEN, m_websocket->readyState());
    m_websocket->close(m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CLOSING, m_websocket->readyState());
}

TEST_F(WebSocketTest, closeWhenClosing)
{
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(channel(), close(-1, String()));
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CONNECTING, m_websocket->readyState());

    m_websocket->didConnect("", "");
    EXPECT_EQ(WebSocket::OPEN, m_websocket->readyState());
    m_websocket->close(m_exceptionState);
    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CLOSING, m_websocket->readyState());

    m_websocket->close(m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CLOSING, m_websocket->readyState());
}

TEST_F(WebSocketTest, closeWhenClosed)
{
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(channel(), close(-1, String()));
        EXPECT_CALL(channel(), disconnect());
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CONNECTING, m_websocket->readyState());

    m_websocket->didConnect("", "");
    EXPECT_EQ(WebSocket::OPEN, m_websocket->readyState());
    m_websocket->close(m_exceptionState);
    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CLOSING, m_websocket->readyState());

    m_websocket->didClose(WebSocketChannelClient::ClosingHandshakeComplete, 1000, String());
    EXPECT_EQ(WebSocket::CLOSED, m_websocket->readyState());
    m_websocket->close(m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CLOSED, m_websocket->readyState());
}

TEST_F(WebSocketTest, sendStringWhenConnecting)
{
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());

    m_websocket->send("hello", m_exceptionState);

    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(InvalidStateError, m_exceptionState.code());
    EXPECT_EQ("Still in CONNECTING state.", m_exceptionState.message());
    EXPECT_EQ(WebSocket::CONNECTING, m_websocket->readyState());
}

TEST_F(WebSocketTest, sendStringWhenClosing)
{
    Checkpoint checkpoint;
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(channel(), fail(_, _, _, _));
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());

    m_websocket->close(m_exceptionState);
    EXPECT_FALSE(m_exceptionState.hadException());

    m_websocket->send("hello", m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CLOSING, m_websocket->readyState());
}

TEST_F(WebSocketTest, sendStringWhenClosed)
{
    Checkpoint checkpoint;
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(channel(), disconnect());
        EXPECT_CALL(checkpoint, Call(1));
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());

    m_websocket->didClose(WebSocketChannelClient::ClosingHandshakeIncomplete, 1006, "");
    checkpoint.Call(1);

    m_websocket->send("hello", m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CLOSED, m_websocket->readyState());
}

TEST_F(WebSocketTest, sendStringSuccess)
{
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(channel(), send(String("hello"))).WillOnce(Return(WebSocketChannel::SendSuccess));
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());

    m_websocket->didConnect("", "");
    m_websocket->send("hello", m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::OPEN, m_websocket->readyState());
}

TEST_F(WebSocketTest, sendStringFail)
{
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(channel(), send(String("hello"))).WillOnce(Return(WebSocketChannel::SendFail));
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());

    m_websocket->didConnect("", "");
    m_websocket->send("hello", m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::OPEN, m_websocket->readyState());
}

TEST_F(WebSocketTest, sendStringInvalidMessage)
{
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(channel(), send(String("hello"))).WillOnce(Return(WebSocketChannel::InvalidMessage));
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());

    m_websocket->didConnect("", "");
    m_websocket->send("hello", m_exceptionState);

    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(SyntaxError, m_exceptionState.code());
    EXPECT_EQ("The message contains invalid characters.", m_exceptionState.message());
    EXPECT_EQ(WebSocket::OPEN, m_websocket->readyState());
}

TEST_F(WebSocketTest, sendArrayBufferWhenConnecting)
{
    RefPtr<ArrayBufferView> view = Uint8Array::create(8);
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());

    m_websocket->send(view->buffer().get(), m_exceptionState);

    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(InvalidStateError, m_exceptionState.code());
    EXPECT_EQ("Still in CONNECTING state.", m_exceptionState.message());
    EXPECT_EQ(WebSocket::CONNECTING, m_websocket->readyState());
}

TEST_F(WebSocketTest, sendArrayBufferWhenClosing)
{
    RefPtr<ArrayBufferView> view = Uint8Array::create(8);
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(channel(), fail(_, _, _, _));
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());

    m_websocket->close(m_exceptionState);
    EXPECT_FALSE(m_exceptionState.hadException());

    m_websocket->send(view->buffer().get(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CLOSING, m_websocket->readyState());
}

TEST_F(WebSocketTest, sendArrayBufferWhenClosed)
{
    Checkpoint checkpoint;
    RefPtr<ArrayBufferView> view = Uint8Array::create(8);
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(channel(), disconnect());
        EXPECT_CALL(checkpoint, Call(1));
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());

    m_websocket->didClose(WebSocketChannelClient::ClosingHandshakeIncomplete, 1006, "");
    checkpoint.Call(1);

    m_websocket->send(view->buffer().get(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CLOSED, m_websocket->readyState());
}

TEST_F(WebSocketTest, sendArrayBufferSuccess)
{
    RefPtr<ArrayBufferView> view = Uint8Array::create(8);
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(channel(), send(Ref(*view->buffer()), 0, 8)).WillOnce(Return(WebSocketChannel::SendSuccess));
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());

    m_websocket->didConnect("", "");
    m_websocket->send(view->buffer().get(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::OPEN, m_websocket->readyState());
}

TEST_F(WebSocketTest, sendArrayBufferFail)
{
    RefPtr<ArrayBufferView> view = Uint8Array::create(8);
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(channel(), send(Ref(*view->buffer()), 0, 8)).WillOnce(Return(WebSocketChannel::SendFail));
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());

    m_websocket->didConnect("", "");
    m_websocket->send(view->buffer().get(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::OPEN, m_websocket->readyState());
}

TEST_F(WebSocketTest, sendArrayBufferInvalidMessage)
{
    RefPtr<ArrayBufferView> view = Uint8Array::create(8);
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(channel(), send(Ref(*view->buffer()), 0, 8)).WillOnce(Return(WebSocketChannel::InvalidMessage));
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());

    m_websocket->didConnect("", "");
    m_websocket->send(view->buffer().get(), m_exceptionState);

    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(SyntaxError, m_exceptionState.code());
    EXPECT_EQ("The message contains invalid characters.", m_exceptionState.message());
    EXPECT_EQ(WebSocket::OPEN, m_websocket->readyState());
}

// FIXME: We should have Blob tests here.
// We can't create a Blob because the blob registration cannot be mocked yet.

// FIXME: We should add tests for bufferedAmount.

// FIXME: We should add tests for data receiving.

TEST_F(WebSocketTest, binaryType)
{
    EXPECT_EQ("blob", m_websocket->binaryType());

    m_websocket->setBinaryType("hoge");

    EXPECT_EQ("blob", m_websocket->binaryType());

    m_websocket->setBinaryType("arraybuffer");

    EXPECT_EQ("arraybuffer", m_websocket->binaryType());

    m_websocket->setBinaryType("fuga");

    EXPECT_EQ("arraybuffer", m_websocket->binaryType());

    m_websocket->setBinaryType("blob");

    EXPECT_EQ("blob", m_websocket->binaryType());
}

// FIXME: We should add tests for suspend / resume.

class WebSocketValidClosingCodeTest : public WebSocketTestBase, public ::testing::TestWithParam<unsigned short> {
public:
};

TEST_P(WebSocketValidClosingCodeTest, test)
{
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(channel(), fail(_, _, _, _));
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CONNECTING, m_websocket->readyState());

    m_websocket->close(GetParam(), "bye", m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CLOSING, m_websocket->readyState());
}

INSTANTIATE_TEST_CASE_P(WebSocketValidClosingCode, WebSocketValidClosingCodeTest, ::testing::Values(1000, 3000, 3001, 4998, 4999));

class WebSocketInvalidClosingCodeTest : public WebSocketTestBase, public ::testing::TestWithParam<unsigned short> {
public:
};

TEST_P(WebSocketInvalidClosingCodeTest, test)
{
    {
        InSequence s;
        EXPECT_CALL(channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
    }
    m_websocket->connect("ws://example.com/", Vector<String>(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ(WebSocket::CONNECTING, m_websocket->readyState());

    m_websocket->close(GetParam(), "bye", m_exceptionState);

    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(InvalidAccessError, m_exceptionState.code());
    EXPECT_EQ(String::format("The code must be either 1000, or between 3000 and 4999. %d is neither.", GetParam()), m_exceptionState.message());
    EXPECT_EQ(WebSocket::CONNECTING, m_websocket->readyState());
}

INSTANTIATE_TEST_CASE_P(WebSocketInvalidClosingCode, WebSocketInvalidClosingCodeTest, ::testing::Values(0, 1, 998, 999, 1001, 2999, 5000, 9999, 65535));

} // namespace

} // namespace WebCore
