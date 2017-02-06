// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_TEST_COMMON_H_
#define BLIMP_NET_TEST_COMMON_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_transport.h"
#include "blimp/net/connection_error_observer.h"
#include "blimp/net/connection_handler.h"
#include "blimp/net/packet_reader.h"
#include "blimp/net/packet_writer.h"
#include "net/socket/stream_socket.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {
class GrowableIOBuffer;
}  // namespace net

namespace blimp {

// Checks if the contents of a buffer are an exact match with std::string.
// Using this matcher for inequality checks will result in undefined behavior,
// due to IOBuffer's lack of a size field.
//
// arg (type: IOBuffer*) The buffer to check.
// data (type: std::string) The string to compare with |arg|.
MATCHER_P(BufferEquals, expected, "") {
  return expected == std::string(arg->data(), expected.size());
}

// Checks if two proto messages are the same.
MATCHER_P(EqualsProto, message, "") {
  std::string expected_serialized;
  std::string actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

// Checks if the contents of a buffer are an exact match with BlimpMessage.
// arg (type: net::DrainableIOBuffer*) The buffer to check.
// message (type: BlimpMessage) The message to compare with |arg|.
MATCHER_P(BufferEqualsProto, message, "") {
  BlimpMessage actual_message;
  actual_message.ParseFromArray(arg->data(), message.ByteSize());
  std::string expected_serialized;
  std::string actual_serialized;
  message.SerializeToString(&expected_serialized);
  actual_message.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

// Checks if the contents of a BlobDataPtr match the string |expected|.
MATCHER_P(BlobDataPtrEqualsString, expected, "") {
  return expected == arg->data;
}

// GMock action that writes data from a string to an IOBuffer.
//
//   buf_idx (template parameter 0): 0-based index of the IOBuffer arg.
//   str: the string containing data to be written to the IOBuffer.
ACTION_TEMPLATE(FillBufferFromString,
                HAS_1_TEMPLATE_PARAMS(int, buf_idx),
                AND_1_VALUE_PARAMS(str)) {
  memcpy(testing::get<buf_idx>(args)->data(), str.data(), str.size());
}

// Returns true if |buf| is prefixed by |str|.
bool BufferStartsWith(net::GrowableIOBuffer* buf,
                      size_t buf_size,
                      const std::string& str);

// GMock action that writes data from a BlimpMessage to a GrowableIOBuffer.
// Advances the buffer's |offset| to the end of the message.
//
//   buf_idx (template parameter 0): 0-based index of the IOBuffer arg.
//   message: the blimp message containing data to be written to the IOBuffer
ACTION_TEMPLATE(FillBufferFromMessage,
                HAS_1_TEMPLATE_PARAMS(int, buf_idx),
                AND_1_VALUE_PARAMS(message)) {
  message->SerializeToArray(testing::get<buf_idx>(args)->data(),
                            message->ByteSize());
}

// Calls |set_offset()| for a GrowableIOBuffer.
ACTION_TEMPLATE(SetBufferOffset,
                HAS_1_TEMPLATE_PARAMS(int, buf_idx),
                AND_1_VALUE_PARAMS(offset)) {
  testing::get<buf_idx>(args)->set_offset(offset);
}

// Formats a string-based representation of a BlimpMessage header.
std::string EncodeHeader(size_t size);

class MockStreamSocket : public net::StreamSocket {
 public:
  MockStreamSocket();
  virtual ~MockStreamSocket();

  MOCK_METHOD3(Read, int(net::IOBuffer*, int, const net::CompletionCallback&));
  MOCK_METHOD3(Write, int(net::IOBuffer*, int, const net::CompletionCallback&));
  MOCK_METHOD1(SetReceiveBufferSize, int(int32_t));
  MOCK_METHOD1(SetSendBufferSize, int(int32_t));
  MOCK_METHOD1(Connect, int(const net::CompletionCallback&));
  MOCK_METHOD0(Disconnect, void());
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_CONST_METHOD0(IsConnectedAndIdle, bool());
  MOCK_CONST_METHOD1(GetPeerAddress, int(net::IPEndPoint*));
  MOCK_CONST_METHOD1(GetLocalAddress, int(net::IPEndPoint*));
  MOCK_CONST_METHOD0(NetLog, const net::BoundNetLog&());
  MOCK_METHOD0(SetSubresourceSpeculation, void());
  MOCK_METHOD0(SetOmniboxSpeculation, void());
  MOCK_CONST_METHOD0(WasEverUsed, bool());
  MOCK_CONST_METHOD0(UsingTCPFastOpen, bool());
  MOCK_CONST_METHOD0(NumBytesRead, int64_t());
  MOCK_CONST_METHOD0(GetConnectTimeMicros, base::TimeDelta());
  MOCK_CONST_METHOD0(WasNpnNegotiated, bool());
  MOCK_CONST_METHOD0(GetNegotiatedProtocol, net::NextProto());
  MOCK_METHOD1(GetSSLInfo, bool(net::SSLInfo*));
  MOCK_CONST_METHOD1(GetConnectionAttempts, void(net::ConnectionAttempts*));
  MOCK_METHOD0(ClearConnectionAttempts, void());
  MOCK_METHOD1(AddConnectionAttempts, void(const net::ConnectionAttempts&));
  MOCK_CONST_METHOD0(GetTotalReceivedBytes, int64_t());
};

class MockTransport : public BlimpTransport {
 public:
  MockTransport();
  ~MockTransport() override;

  MOCK_METHOD1(Connect, void(const net::CompletionCallback& callback));
  MOCK_METHOD0(TakeConnectionPtr, BlimpConnection*());

  std::unique_ptr<BlimpConnection> TakeConnection() override;
  const char* GetName() const override;
};

class MockConnectionHandler : public ConnectionHandler {
 public:
  MockConnectionHandler();
  ~MockConnectionHandler() override;

  MOCK_METHOD1(HandleConnectionPtr, void(BlimpConnection* connection));
  void HandleConnection(std::unique_ptr<BlimpConnection> connection) override;
};

class MockPacketReader : public PacketReader {
 public:
  MockPacketReader();
  ~MockPacketReader() override;

  MOCK_METHOD2(ReadPacket,
               void(const scoped_refptr<net::GrowableIOBuffer>&,
                    const net::CompletionCallback&));
};

class MockPacketWriter : public PacketWriter {
 public:
  MockPacketWriter();
  ~MockPacketWriter() override;

  MOCK_METHOD2(WritePacket,
               void(const scoped_refptr<net::DrainableIOBuffer>&,
                    const net::CompletionCallback&));
};

class MockBlimpConnection : public BlimpConnection {
 public:
  MockBlimpConnection();
  ~MockBlimpConnection() override;

  MOCK_METHOD1(SetConnectionErrorObserver,
               void(ConnectionErrorObserver* observer));

  MOCK_METHOD1(SetIncomingMessageProcessor,
               void(BlimpMessageProcessor* processor));

  MOCK_METHOD1(AddConnectionErrorObserver, void(ConnectionErrorObserver*));

  MOCK_METHOD1(RemoveConnectionErrorObserver, void(ConnectionErrorObserver*));

  MOCK_METHOD0(GetOutgoingMessageProcessor, BlimpMessageProcessor*(void));
};

class MockConnectionErrorObserver : public ConnectionErrorObserver {
 public:
  MockConnectionErrorObserver();
  ~MockConnectionErrorObserver() override;

  MOCK_METHOD1(OnConnectionError, void(int error));
};

class MockBlimpMessageProcessor : public BlimpMessageProcessor {
 public:
  MockBlimpMessageProcessor();

  ~MockBlimpMessageProcessor() override;

  // Adapts calls from ProcessMessage to MockableProcessMessage by
  // unboxing the |message| std::unique_ptr for GMock compatibility.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

  MOCK_METHOD2(MockableProcessMessage,
               void(const BlimpMessage& message,
                    const net::CompletionCallback& callback));
};

}  // namespace blimp

#endif  // BLIMP_NET_TEST_COMMON_H_
