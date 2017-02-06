// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/websockets/DOMWebSocket.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/DOMTypedArray.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/SecurityContext.h"
#include "core/fileapi/Blob.h"
#include "core/inspector/ConsoleTypes.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"
#include <memory>
#include <v8.h>

using testing::_;
using testing::AnyNumber;
using testing::InSequence;
using testing::Ref;
using testing::Return;

namespace blink {

namespace {

typedef testing::StrictMock<testing::MockFunction<void(int)>> Checkpoint;  // NOLINT

class MockWebSocketChannel : public WebSocketChannel {
public:
    static MockWebSocketChannel* create()
    {
        return new testing::StrictMock<MockWebSocketChannel>();
    }

    ~MockWebSocketChannel() override
    {
    }

    MOCK_METHOD2(connect, bool(const KURL&, const String&));
    MOCK_METHOD1(send, void(const CString&));
    MOCK_METHOD3(send, void(const DOMArrayBuffer&, unsigned, unsigned));
    MOCK_METHOD1(send, void(PassRefPtr<BlobDataHandle>));
    MOCK_METHOD1(sendTextAsCharVectorMock, void(Vector<char>*));
    void sendTextAsCharVector(std::unique_ptr<Vector<char>> vector)
    {
        sendTextAsCharVectorMock(vector.get());
    }
    MOCK_METHOD1(sendBinaryAsCharVectorMock, void(Vector<char>*));
    void sendBinaryAsCharVector(std::unique_ptr<Vector<char>> vector)
    {
        sendBinaryAsCharVectorMock(vector.get());
    }
    MOCK_CONST_METHOD0(bufferedAmount, unsigned());
    MOCK_METHOD2(close, void(int, const String&));
    MOCK_METHOD3(failMock, void(const String&, MessageLevel, SourceLocation*));
    void fail(const String& reason, MessageLevel level, std::unique_ptr<SourceLocation> location)
    {
        failMock(reason, level, location.get());
    }
    MOCK_METHOD0(disconnect, void());

    MockWebSocketChannel()
    {
    }
};

class DOMWebSocketWithMockChannel final : public DOMWebSocket {
public:
    static DOMWebSocketWithMockChannel* create(ExecutionContext* context)
    {
        DOMWebSocketWithMockChannel* websocket = new DOMWebSocketWithMockChannel(context);
        websocket->suspendIfNeeded();
        return websocket;
    }

    MockWebSocketChannel* channel() { return m_channel.get(); }

    WebSocketChannel* createChannel(ExecutionContext*, WebSocketChannelClient*) override
    {
        ASSERT(!m_hasCreatedChannel);
        m_hasCreatedChannel = true;
        return m_channel.get();
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_channel);
        DOMWebSocket::trace(visitor);
    }

private:
    DOMWebSocketWithMockChannel(ExecutionContext* context)
        : DOMWebSocket(context)
        , m_channel(MockWebSocketChannel::create())
        , m_hasCreatedChannel(false) { }

    Member<MockWebSocketChannel> m_channel;
    bool m_hasCreatedChannel;
};

class DOMWebSocketTestScope {
public:
    explicit DOMWebSocketTestScope(ExecutionContext* executionContext)
        : m_websocket(DOMWebSocketWithMockChannel::create(executionContext))
    {
    }

    ~DOMWebSocketTestScope()
    {
        if (!m_websocket)
            return;
        // These statements are needed to clear WebSocket::m_channel to
        // avoid ASSERTION failure on ~DOMWebSocket.
        DCHECK(socket().channel());
        ::testing::Mock::VerifyAndClear(socket().channel());
        EXPECT_CALL(channel(), disconnect()).Times(AnyNumber());

        socket().didClose(WebSocketChannelClient::ClosingHandshakeIncomplete, 1006, "");
    }

    MockWebSocketChannel& channel() { return *m_websocket->channel(); }
    DOMWebSocketWithMockChannel& socket() { return *m_websocket.get(); }

private:
    Persistent<DOMWebSocketWithMockChannel> m_websocket;
};

TEST(DOMWebSocketTest, connectToBadURL)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    webSocketScope.socket().connect("xxx", Vector<String>(), scope.getExceptionState());


    EXPECT_TRUE(scope.getExceptionState().hadException());
    EXPECT_EQ(SyntaxError, scope.getExceptionState().code());
    EXPECT_EQ("The URL 'xxx' is invalid.", scope.getExceptionState().message());
    EXPECT_EQ(DOMWebSocket::CLOSED, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, connectToNonWsURL)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    webSocketScope.socket().connect("http://example.com/", Vector<String>(), scope.getExceptionState());


    EXPECT_TRUE(scope.getExceptionState().hadException());
    EXPECT_EQ(SyntaxError, scope.getExceptionState().code());
    EXPECT_EQ("The URL's scheme must be either 'ws' or 'wss'. 'http' is not allowed.", scope.getExceptionState().message());
    EXPECT_EQ(DOMWebSocket::CLOSED, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, connectToURLHavingFragmentIdentifier)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    webSocketScope.socket().connect("ws://example.com/#fragment", Vector<String>(), scope.getExceptionState());


    EXPECT_TRUE(scope.getExceptionState().hadException());
    EXPECT_EQ(SyntaxError, scope.getExceptionState().code());
    EXPECT_EQ("The URL contains a fragment identifier ('fragment'). Fragment identifiers are not allowed in WebSocket URLs.", scope.getExceptionState().message());
    EXPECT_EQ(DOMWebSocket::CLOSED, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, invalidPort)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    webSocketScope.socket().connect("ws://example.com:7", Vector<String>(), scope.getExceptionState());


    EXPECT_TRUE(scope.getExceptionState().hadException());
    EXPECT_EQ(SecurityError, scope.getExceptionState().code());
    EXPECT_EQ("The port 7 is not allowed.", scope.getExceptionState().message());
    EXPECT_EQ(DOMWebSocket::CLOSED, webSocketScope.socket().readyState());
}

// FIXME: Add a test for Content Security Policy.

TEST(DOMWebSocketTest, invalidSubprotocols)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    Vector<String> subprotocols;
    subprotocols.append("@subprotocol-|'\"x\x01\x02\x03x");

    webSocketScope.socket().connect("ws://example.com/", subprotocols, scope.getExceptionState());

    EXPECT_TRUE(scope.getExceptionState().hadException());
    EXPECT_EQ(SyntaxError, scope.getExceptionState().code());
    EXPECT_EQ("The subprotocol '@subprotocol-|'\"x\\u0001\\u0002\\u0003x' is invalid.", scope.getExceptionState().message());
    EXPECT_EQ(DOMWebSocket::CLOSED, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, insecureRequestsUpgrade)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "wss://example.com/endpoint"), String())).WillOnce(Return(true));
    }

    scope.document().setInsecureRequestPolicy(kUpgradeInsecureRequests);
    webSocketScope.socket().connect("ws://example.com/endpoint", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CONNECTING, webSocketScope.socket().readyState());
    EXPECT_EQ(KURL(KURL(), "wss://example.com/endpoint"), webSocketScope.socket().url());
}

TEST(DOMWebSocketTest, insecureRequestsDoNotUpgrade)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/endpoint"), String())).WillOnce(Return(true));
    }

    scope.document().setInsecureRequestPolicy(kLeaveInsecureRequestsAlone);
    webSocketScope.socket().connect("ws://example.com/endpoint", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CONNECTING, webSocketScope.socket().readyState());
    EXPECT_EQ(KURL(KURL(), "ws://example.com/endpoint"), webSocketScope.socket().url());
}

TEST(DOMWebSocketTest, channelConnectSuccess)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    Vector<String> subprotocols;
    subprotocols.append("aa");
    subprotocols.append("bb");

    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/hoge"), String("aa, bb"))).WillOnce(Return(true));
    }

    webSocketScope.socket().connect("ws://example.com/hoge", Vector<String>(subprotocols), scope.getExceptionState());


    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CONNECTING, webSocketScope.socket().readyState());
    EXPECT_EQ(KURL(KURL(), "ws://example.com/hoge"), webSocketScope.socket().url());
}

TEST(DOMWebSocketTest, channelConnectFail)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    Vector<String> subprotocols;
    subprotocols.append("aa");
    subprotocols.append("bb");

    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String("aa, bb"))).WillOnce(Return(false));
        EXPECT_CALL(webSocketScope.channel(), disconnect());
    }

    webSocketScope.socket().connect("ws://example.com/", Vector<String>(subprotocols), scope.getExceptionState());


    EXPECT_TRUE(scope.getExceptionState().hadException());
    EXPECT_EQ(SecurityError, scope.getExceptionState().code());
    EXPECT_EQ("An insecure WebSocket connection may not be initiated from a page loaded over HTTPS.", scope.getExceptionState().message());
    EXPECT_EQ(DOMWebSocket::CLOSED, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, isValidSubprotocolString)
{
    EXPECT_TRUE(DOMWebSocket::isValidSubprotocolString("Helloworld!!"));
    EXPECT_FALSE(DOMWebSocket::isValidSubprotocolString("Hello, world!!"));
    EXPECT_FALSE(DOMWebSocket::isValidSubprotocolString(String()));
    EXPECT_FALSE(DOMWebSocket::isValidSubprotocolString(""));

    const char validCharacters[] = "!#$%&'*+-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ^_`abcdefghijklmnopqrstuvwxyz|~";
    size_t length = strlen(validCharacters);
    for (size_t i = 0; i < length; ++i) {
        String s;
        s.append(static_cast<UChar>(validCharacters[i]));
        EXPECT_TRUE(DOMWebSocket::isValidSubprotocolString(s));
    }
    for (size_t i = 0; i < 256; ++i) {
        if (std::find(validCharacters, validCharacters + length, static_cast<char>(i)) != validCharacters + length) {
            continue;
        }
        String s;
        s.append(static_cast<UChar>(i));
        EXPECT_FALSE(DOMWebSocket::isValidSubprotocolString(s));
    }
}

TEST(DOMWebSocketTest, connectSuccess)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    Vector<String> subprotocols;
    subprotocols.append("aa");
    subprotocols.append("bb");
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String("aa, bb"))).WillOnce(Return(true));
    }
    webSocketScope.socket().connect("ws://example.com/", subprotocols, scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CONNECTING, webSocketScope.socket().readyState());

    webSocketScope.socket().didConnect("bb", "cc");

    EXPECT_EQ(DOMWebSocket::OPEN, webSocketScope.socket().readyState());
    EXPECT_EQ("bb", webSocketScope.socket().protocol());
    EXPECT_EQ("cc", webSocketScope.socket().extensions());
}

TEST(DOMWebSocketTest, didClose)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(webSocketScope.channel(), disconnect());
    }
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CONNECTING, webSocketScope.socket().readyState());

    webSocketScope.socket().didClose(WebSocketChannelClient::ClosingHandshakeIncomplete, 1006, "");

    EXPECT_EQ(DOMWebSocket::CLOSED, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, maximumReasonSize)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(webSocketScope.channel(), failMock(_, _, _));
    }
    String reason;
    for (size_t i = 0; i < 123; ++i)
        reason.append("a");
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CONNECTING, webSocketScope.socket().readyState());

    webSocketScope.socket().close(1000, reason, scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CLOSING, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, reasonSizeExceeding)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
    }
    String reason;
    for (size_t i = 0; i < 124; ++i)
        reason.append("a");
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CONNECTING, webSocketScope.socket().readyState());

    webSocketScope.socket().close(1000, reason, scope.getExceptionState());

    EXPECT_TRUE(scope.getExceptionState().hadException());
    EXPECT_EQ(SyntaxError, scope.getExceptionState().code());
    EXPECT_EQ("The message must not be greater than 123 bytes.", scope.getExceptionState().message());
    EXPECT_EQ(DOMWebSocket::CONNECTING, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, closeWhenConnecting)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(webSocketScope.channel(), failMock(String("WebSocket is closed before the connection is established."), WarningMessageLevel, _));
    }
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CONNECTING, webSocketScope.socket().readyState());

    webSocketScope.socket().close(1000, "bye", scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CLOSING, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, close)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(webSocketScope.channel(), close(3005, String("bye")));
    }
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CONNECTING, webSocketScope.socket().readyState());

    webSocketScope.socket().didConnect("", "");
    EXPECT_EQ(DOMWebSocket::OPEN, webSocketScope.socket().readyState());
    webSocketScope.socket().close(3005, "bye", scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CLOSING, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, closeWithoutReason)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(webSocketScope.channel(), close(3005, String()));
    }
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CONNECTING, webSocketScope.socket().readyState());

    webSocketScope.socket().didConnect("", "");
    EXPECT_EQ(DOMWebSocket::OPEN, webSocketScope.socket().readyState());
    webSocketScope.socket().close(3005, scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CLOSING, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, closeWithoutCodeAndReason)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(webSocketScope.channel(), close(-1, String()));
    }
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CONNECTING, webSocketScope.socket().readyState());

    webSocketScope.socket().didConnect("", "");
    EXPECT_EQ(DOMWebSocket::OPEN, webSocketScope.socket().readyState());
    webSocketScope.socket().close(scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CLOSING, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, closeWhenClosing)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(webSocketScope.channel(), close(-1, String()));
    }
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CONNECTING, webSocketScope.socket().readyState());

    webSocketScope.socket().didConnect("", "");
    EXPECT_EQ(DOMWebSocket::OPEN, webSocketScope.socket().readyState());
    webSocketScope.socket().close(scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CLOSING, webSocketScope.socket().readyState());

    webSocketScope.socket().close(scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CLOSING, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, closeWhenClosed)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(webSocketScope.channel(), close(-1, String()));
        EXPECT_CALL(webSocketScope.channel(), disconnect());
    }
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CONNECTING, webSocketScope.socket().readyState());

    webSocketScope.socket().didConnect("", "");
    EXPECT_EQ(DOMWebSocket::OPEN, webSocketScope.socket().readyState());
    webSocketScope.socket().close(scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CLOSING, webSocketScope.socket().readyState());

    webSocketScope.socket().didClose(WebSocketChannelClient::ClosingHandshakeComplete, 1000, String());
    EXPECT_EQ(DOMWebSocket::CLOSED, webSocketScope.socket().readyState());
    webSocketScope.socket().close(scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CLOSED, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, sendStringWhenConnecting)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
    }
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());

    webSocketScope.socket().send("hello", scope.getExceptionState());

    EXPECT_TRUE(scope.getExceptionState().hadException());
    EXPECT_EQ(InvalidStateError, scope.getExceptionState().code());
    EXPECT_EQ("Still in CONNECTING state.", scope.getExceptionState().message());
    EXPECT_EQ(DOMWebSocket::CONNECTING, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, sendStringWhenClosing)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    Checkpoint checkpoint;
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(webSocketScope.channel(), failMock(_, _, _));
    }
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());

    webSocketScope.socket().close(scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());

    webSocketScope.socket().send("hello", scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CLOSING, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, sendStringWhenClosed)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    Checkpoint checkpoint;
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(webSocketScope.channel(), disconnect());
        EXPECT_CALL(checkpoint, Call(1));
    }
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());

    webSocketScope.socket().didClose(WebSocketChannelClient::ClosingHandshakeIncomplete, 1006, "");
    checkpoint.Call(1);

    webSocketScope.socket().send("hello", scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CLOSED, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, sendStringSuccess)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(webSocketScope.channel(), send(CString("hello")));
    }
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());

    webSocketScope.socket().didConnect("", "");
    webSocketScope.socket().send("hello", scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::OPEN, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, sendNonLatin1String)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(webSocketScope.channel(), send(CString("\xe7\x8b\x90\xe0\xa4\x94")));
    }
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());

    webSocketScope.socket().didConnect("", "");
    UChar nonLatin1String[] = {
        0x72d0,
        0x0914,
        0x0000
    };
    webSocketScope.socket().send(nonLatin1String, scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::OPEN, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, sendArrayBufferWhenConnecting)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    DOMArrayBufferView* view = DOMUint8Array::create(8);
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
    }
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());

    webSocketScope.socket().send(view->buffer(), scope.getExceptionState());

    EXPECT_TRUE(scope.getExceptionState().hadException());
    EXPECT_EQ(InvalidStateError, scope.getExceptionState().code());
    EXPECT_EQ("Still in CONNECTING state.", scope.getExceptionState().message());
    EXPECT_EQ(DOMWebSocket::CONNECTING, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, sendArrayBufferWhenClosing)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    DOMArrayBufferView* view = DOMUint8Array::create(8);
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(webSocketScope.channel(), failMock(_, _, _));
    }
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());

    webSocketScope.socket().close(scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());

    webSocketScope.socket().send(view->buffer(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CLOSING, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, sendArrayBufferWhenClosed)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    Checkpoint checkpoint;
    DOMArrayBufferView* view = DOMUint8Array::create(8);
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(webSocketScope.channel(), disconnect());
        EXPECT_CALL(checkpoint, Call(1));
    }
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());

    webSocketScope.socket().didClose(WebSocketChannelClient::ClosingHandshakeIncomplete, 1006, "");
    checkpoint.Call(1);

    webSocketScope.socket().send(view->buffer(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CLOSED, webSocketScope.socket().readyState());
}

TEST(DOMWebSocketTest, sendArrayBufferSuccess)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    DOMArrayBufferView* view = DOMUint8Array::create(8);
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(webSocketScope.channel(), send(Ref(*view->buffer()), 0, 8));
    }
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());

    webSocketScope.socket().didConnect("", "");
    webSocketScope.socket().send(view->buffer(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::OPEN, webSocketScope.socket().readyState());
}

// FIXME: We should have Blob tests here.
// We can't create a Blob because the blob registration cannot be mocked yet.

// FIXME: We should add tests for bufferedAmount.

// FIXME: We should add tests for data receiving.

TEST(DOMWebSocketTest, binaryType)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    EXPECT_EQ("blob", webSocketScope.socket().binaryType());

    webSocketScope.socket().setBinaryType("arraybuffer");

    EXPECT_EQ("arraybuffer", webSocketScope.socket().binaryType());

    webSocketScope.socket().setBinaryType("blob");

    EXPECT_EQ("blob", webSocketScope.socket().binaryType());
}

// FIXME: We should add tests for suspend / resume.

class DOMWebSocketValidClosingTest : public ::testing::TestWithParam<unsigned short> {};

TEST_P(DOMWebSocketValidClosingTest, test)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
        EXPECT_CALL(webSocketScope.channel(), failMock(_, _, _));
    }
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CONNECTING, webSocketScope.socket().readyState());

    webSocketScope.socket().close(GetParam(), "bye", scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CLOSING, webSocketScope.socket().readyState());
}

INSTANTIATE_TEST_CASE_P(DOMWebSocketValidClosing, DOMWebSocketValidClosingTest, ::testing::Values(1000, 3000, 3001, 4998, 4999));

class DOMWebSocketInvalidClosingCodeTest : public ::testing::TestWithParam<unsigned short> {};

TEST_P(DOMWebSocketInvalidClosingCodeTest, test)
{
    V8TestingScope scope;
    DOMWebSocketTestScope webSocketScope(scope.getExecutionContext());
    {
        InSequence s;
        EXPECT_CALL(webSocketScope.channel(), connect(KURL(KURL(), "ws://example.com/"), String())).WillOnce(Return(true));
    }
    webSocketScope.socket().connect("ws://example.com/", Vector<String>(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ(DOMWebSocket::CONNECTING, webSocketScope.socket().readyState());

    webSocketScope.socket().close(GetParam(), "bye", scope.getExceptionState());

    EXPECT_TRUE(scope.getExceptionState().hadException());
    EXPECT_EQ(InvalidAccessError, scope.getExceptionState().code());
    EXPECT_EQ(String::format("The code must be either 1000, or between 3000 and 4999. %d is neither.", GetParam()), scope.getExceptionState().message());
    EXPECT_EQ(DOMWebSocket::CONNECTING, webSocketScope.socket().readyState());
}

INSTANTIATE_TEST_CASE_P(DOMWebSocketInvalidClosingCode, DOMWebSocketInvalidClosingCodeTest, ::testing::Values(0, 1, 998, 999, 1001, 2999, 5000, 9999, 65535));

} // namespace

} // namespace blink
