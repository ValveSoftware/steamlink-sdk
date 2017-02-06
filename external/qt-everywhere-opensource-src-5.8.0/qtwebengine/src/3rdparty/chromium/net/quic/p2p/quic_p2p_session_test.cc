// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/p2p/quic_p2p_session.h"

#include <algorithm>
#include <deque>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/p2p/quic_p2p_crypto_config.h"
#include "net/quic/p2p/quic_p2p_stream.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/quic/test_tools/quic_session_peer.h"
#include "net/socket/socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const char kTestSharedKey[] = "Shared key exchanged out of bound.";

class FakeP2PDatagramSocket : public Socket {
 public:
  FakeP2PDatagramSocket() : weak_factory_(this) {}
  ~FakeP2PDatagramSocket() override {}

  base::WeakPtr<FakeP2PDatagramSocket> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void ConnectWith(FakeP2PDatagramSocket* peer_socket) {
    peer_socket_ = peer_socket->GetWeakPtr();
    peer_socket->peer_socket_ = GetWeakPtr();
  }

  void SetReadError(int error) {
    read_error_ = error;
    if (!read_callback_.is_null()) {
      base::ResetAndReturn(&read_callback_).Run(error);
    }
  }

  void SetWriteError(int error) { write_error_ = error; }

  void AppendInputPacket(const std::vector<char>& data) {
    if (!read_callback_.is_null()) {
      int size = std::min(read_buffer_size_, static_cast<int>(data.size()));
      memcpy(read_buffer_->data(), &data[0], data.size());
      read_buffer_ = nullptr;
      base::ResetAndReturn(&read_callback_).Run(size);
    } else {
      incoming_packets_.push_back(data);
    }
  }

  // Socket interface.
  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override {
    DCHECK(read_callback_.is_null());

    if (read_error_ != OK) {
      return read_error_;
    }

    if (!incoming_packets_.empty()) {
      scoped_refptr<IOBuffer> buffer(buf);
      int size =
          std::min(static_cast<int>(incoming_packets_.front().size()), buf_len);
      memcpy(buffer->data(), &*incoming_packets_.front().begin(), size);
      incoming_packets_.pop_front();
      return size;
    } else {
      read_callback_ = callback;
      read_buffer_ = buf;
      read_buffer_size_ = buf_len;
      return ERR_IO_PENDING;
    }
  }

  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback) override {
    if (write_error_ != OK) {
      return write_error_;
    }

    if (peer_socket_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&FakeP2PDatagramSocket::AppendInputPacket, peer_socket_,
                     std::vector<char>(buf->data(), buf->data() + buf_len)));
    }

    return buf_len;
  }

  int SetReceiveBufferSize(int32_t size) override {
    NOTIMPLEMENTED();
    return ERR_NOT_IMPLEMENTED;
  }
  int SetSendBufferSize(int32_t size) override {
    NOTIMPLEMENTED();
    return ERR_NOT_IMPLEMENTED;
  }

 private:
  int read_error_ = OK;
  int write_error_ = OK;

  scoped_refptr<IOBuffer> read_buffer_;
  int read_buffer_size_;
  CompletionCallback read_callback_;

  std::deque<std::vector<char>> incoming_packets_;

  base::WeakPtr<FakeP2PDatagramSocket> peer_socket_;

  base::WeakPtrFactory<FakeP2PDatagramSocket> weak_factory_;
};

class TestP2PStreamDelegate : public QuicP2PStream::Delegate {
 public:
  TestP2PStreamDelegate() {}
  ~TestP2PStreamDelegate() override {}

  void OnDataReceived(const char* data, int length) override {
    received_data_.append(data, length);
  }
  void OnClose(QuicErrorCode error) override {
    is_closed_ = true;
    error_ = error;
  }

  const std::string& received_data() { return received_data_; }
  bool is_closed() { return is_closed_; }
  QuicErrorCode error() { return error_; }

 private:
  std::string received_data_;
  bool is_closed_ = false;
  QuicErrorCode error_ = QUIC_NO_ERROR;

  DISALLOW_COPY_AND_ASSIGN(TestP2PStreamDelegate);
};

class TestP2PSessionDelegate : public QuicP2PSession::Delegate {
 public:
  void OnIncomingStream(QuicP2PStream* stream) override {
    last_incoming_stream_ = stream;
    stream->SetDelegate(next_incoming_stream_delegate_);
    next_incoming_stream_delegate_ = nullptr;
    if (!on_incoming_stream_callback_.is_null())
      base::ResetAndReturn(&on_incoming_stream_callback_).Run();
  }

  void OnConnectionClosed(QuicErrorCode error) override {
    is_closed_ = true;
    error_ = error;
  }

  void set_next_incoming_stream_delegate(QuicP2PStream::Delegate* delegate) {
    next_incoming_stream_delegate_ = delegate;
  }
  void set_on_incoming_stream_callback(const base::Closure& callback) {
    on_incoming_stream_callback_ = callback;
  }
  QuicP2PStream* last_incoming_stream() { return last_incoming_stream_; }
  bool is_closed() { return is_closed_; }
  QuicErrorCode error() { return error_; }

 private:
  QuicP2PStream::Delegate* next_incoming_stream_delegate_ = nullptr;
  base::Closure on_incoming_stream_callback_;
  QuicP2PStream* last_incoming_stream_ = nullptr;
  bool is_closed_ = false;
  QuicErrorCode error_ = QUIC_NO_ERROR;
};

}  // namespace

class QuicP2PSessionTest : public ::testing::Test {
 public:
  void OnWriteResult(int result);

 protected:
  QuicP2PSessionTest()
      : quic_helper_(&quic_clock_, QuicRandom::GetInstance()),
        alarm_factory_(base::ThreadTaskRunnerHandle::Get().get(),
                       &quic_clock_) {
    // Simulate out-of-bound config handshake.
    CryptoHandshakeMessage hello_message;
    config_.ToHandshakeMessage(&hello_message);
    std::string error_detail;
    EXPECT_EQ(QUIC_NO_ERROR,
              config_.ProcessPeerHello(hello_message, CLIENT, &error_detail));
  }

  void CreateSessions() {
    std::unique_ptr<FakeP2PDatagramSocket> socket1(new FakeP2PDatagramSocket());
    std::unique_ptr<FakeP2PDatagramSocket> socket2(new FakeP2PDatagramSocket());
    socket1->ConnectWith(socket2.get());

    socket1_ = socket1->GetWeakPtr();
    socket2_ = socket2->GetWeakPtr();

    QuicP2PCryptoConfig crypto_config(kTestSharedKey);

    session1_ = CreateP2PSession(std::move(socket1), crypto_config,
                                 Perspective::IS_SERVER);
    session2_ = CreateP2PSession(std::move(socket2), crypto_config,
                                 Perspective::IS_CLIENT);
  }

  std::unique_ptr<QuicP2PSession> CreateP2PSession(
      std::unique_ptr<Socket> socket,
      QuicP2PCryptoConfig crypto_config,
      Perspective perspective) {
    QuicChromiumPacketWriter* writer =
        new QuicChromiumPacketWriter(socket.get());
    std::unique_ptr<QuicConnection> quic_connection1(new QuicConnection(
        0, IPEndPoint(IPAddress::IPv4AllZeros(), 0), &quic_helper_,
        &alarm_factory_, writer, true /* owns_writer */, perspective,
        QuicSupportedVersions()));
    writer->SetConnection(quic_connection1.get());

    std::unique_ptr<QuicP2PSession> result(
        new QuicP2PSession(config_, crypto_config, std::move(quic_connection1),
                           std::move(socket)));
    result->Initialize();
    return result;
  }

  void TestStreamConnection(QuicP2PSession* from_session,
                            QuicP2PSession* to_session,
                            QuicStreamId expected_stream_id);

  QuicClock quic_clock_;
  QuicChromiumConnectionHelper quic_helper_;
  QuicChromiumAlarmFactory alarm_factory_;
  QuicConfig config_;

  base::WeakPtr<FakeP2PDatagramSocket> socket1_;
  std::unique_ptr<QuicP2PSession> session1_;

  base::WeakPtr<FakeP2PDatagramSocket> socket2_;
  std::unique_ptr<QuicP2PSession> session2_;
};

void QuicP2PSessionTest::OnWriteResult(int result) {
  EXPECT_EQ(OK, result);
}

void QuicP2PSessionTest::TestStreamConnection(QuicP2PSession* from_session,
                                              QuicP2PSession* to_session,
                                              QuicStreamId expected_stream_id) {
  QuicP2PStream* outgoing_stream =
      from_session->CreateOutgoingDynamicStream(kDefaultPriority);
  EXPECT_TRUE(outgoing_stream);
  TestP2PStreamDelegate outgoing_stream_delegate;
  outgoing_stream->SetDelegate(&outgoing_stream_delegate);
  EXPECT_EQ(expected_stream_id, outgoing_stream->id());

  // Add streams to write_blocked_lists of both QuicSession objects.
  QuicWriteBlockedList* write_blocked_list1 =
      test::QuicSessionPeer::GetWriteBlockedStreams(from_session);
  write_blocked_list1->RegisterStream(expected_stream_id, kV3HighestPriority);
  QuicWriteBlockedList* write_blocked_list2 =
      test::QuicSessionPeer::GetWriteBlockedStreams(to_session);
  write_blocked_list2->RegisterStream(expected_stream_id, kV3HighestPriority);

  // Send a test message to the client.
  const char kTestMessage[] = "Hi";
  const char kTestResponse[] = "Response";
  outgoing_stream->Write(
      std::string(kTestMessage),
      base::Bind(&QuicP2PSessionTest::OnWriteResult, base::Unretained(this)));

  // Wait for the incoming stream to be created.
  TestP2PStreamDelegate incoming_stream_delegate;
  base::RunLoop run_loop;
  TestP2PSessionDelegate session_delegate;
  session_delegate.set_next_incoming_stream_delegate(&incoming_stream_delegate);
  session_delegate.set_on_incoming_stream_callback(
      base::Bind(&base::RunLoop::Quit, base::Unretained(&run_loop)));
  to_session->SetDelegate(&session_delegate);
  run_loop.Run();
  to_session->SetDelegate(nullptr);

  QuicP2PStream* incoming_stream = session_delegate.last_incoming_stream();
  ASSERT_TRUE(incoming_stream);
  EXPECT_EQ(expected_stream_id, incoming_stream->id());
  EXPECT_EQ(kTestMessage, incoming_stream_delegate.received_data());

  incoming_stream->Write(
      std::string(kTestResponse),
      base::Bind(&QuicP2PSessionTest::OnWriteResult, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTestResponse, outgoing_stream_delegate.received_data());

  from_session->CloseStream(outgoing_stream->id());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(outgoing_stream_delegate.is_closed());
  EXPECT_TRUE(incoming_stream_delegate.is_closed());
}

TEST_F(QuicP2PSessionTest, ClientToServer) {
  CreateSessions();
  TestStreamConnection(session2_.get(), session1_.get(), 3);
}

TEST_F(QuicP2PSessionTest, ServerToClient) {
  CreateSessions();
  TestStreamConnection(session1_.get(), session2_.get(), 2);
}

TEST_F(QuicP2PSessionTest, DestroySocketWhenClosed) {
  CreateSessions();

  // The socket must be destroyed when connection is closed.
  EXPECT_TRUE(socket1_);
  session1_->connection()->CloseConnection(
      QUIC_NO_ERROR, "test", ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_FALSE(socket1_);
}

TEST_F(QuicP2PSessionTest, TransportWriteError) {
  CreateSessions();

  TestP2PSessionDelegate session_delegate;
  session1_->SetDelegate(&session_delegate);

  QuicP2PStream* stream =
      session1_->CreateOutgoingDynamicStream(kDefaultPriority);
  EXPECT_TRUE(stream);
  TestP2PStreamDelegate stream_delegate;
  stream->SetDelegate(&stream_delegate);
  EXPECT_EQ(2U, stream->id());

  // Add stream to write_blocked_list.
  QuicWriteBlockedList* write_blocked_list =
      test::QuicSessionPeer::GetWriteBlockedStreams(session1_.get());
  write_blocked_list->RegisterStream(stream->id(), kV3HighestPriority);

  socket1_->SetWriteError(ERR_INTERNET_DISCONNECTED);

  const char kTestMessage[] = "Hi";
  stream->Write(
      std::string(kTestMessage),
      base::Bind(&QuicP2PSessionTest::OnWriteResult, base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(stream_delegate.is_closed());
  EXPECT_EQ(QUIC_PACKET_WRITE_ERROR, stream_delegate.error());
  EXPECT_TRUE(session_delegate.is_closed());
  EXPECT_EQ(QUIC_PACKET_WRITE_ERROR, session_delegate.error());

  // Verify that the socket was destroyed.
  EXPECT_FALSE(socket1_);
}

TEST_F(QuicP2PSessionTest, TransportReceiveError) {
  CreateSessions();

  TestP2PSessionDelegate session_delegate;
  session1_->SetDelegate(&session_delegate);

  QuicP2PStream* stream =
      session1_->CreateOutgoingDynamicStream(kDefaultPriority);
  EXPECT_TRUE(stream);
  TestP2PStreamDelegate stream_delegate;
  stream->SetDelegate(&stream_delegate);
  EXPECT_EQ(2U, stream->id());

  socket1_->SetReadError(ERR_INTERNET_DISCONNECTED);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(stream_delegate.is_closed());
  EXPECT_EQ(QUIC_PACKET_READ_ERROR, stream_delegate.error());
  EXPECT_TRUE(session_delegate.is_closed());
  EXPECT_EQ(QUIC_PACKET_READ_ERROR, session_delegate.error());

  // Verify that the socket was destroyed.
  EXPECT_FALSE(socket1_);
}

}  // namespace net
