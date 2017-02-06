// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_socket.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/mock_timer.h"
#include "extensions/browser/api/cast_channel/cast_auth_util.h"
#include "extensions/browser/api/cast_channel/cast_framer.h"
#include "extensions/browser/api/cast_channel/cast_message_util.h"
#include "extensions/browser/api/cast_channel/cast_test_util.h"
#include "extensions/browser/api/cast_channel/cast_transport.h"
#include "extensions/browser/api/cast_channel/logger.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/log/test_net_log.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

const int64_t kDistantTimeoutMillis = 100000;  // 100 seconds (never hit).

using ::testing::_;
using ::testing::A;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::InvokeArgument;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;

namespace extensions {
namespace api {
namespace cast_channel {
const char kAuthNamespace[] = "urn:x-cast:com.google.cast.tp.deviceauth";

// Returns an auth challenge message inline.
CastMessage CreateAuthChallenge() {
  CastMessage output;
  CreateAuthChallengeMessage(&output);
  return output;
}

// Returns an auth challenge response message inline.
CastMessage CreateAuthReply() {
  CastMessage output;
  output.set_protocol_version(CastMessage::CASTV2_1_0);
  output.set_source_id("sender-0");
  output.set_destination_id("receiver-0");
  output.set_payload_type(CastMessage::BINARY);
  output.set_payload_binary("abcd");
  output.set_namespace_(kAuthNamespace);
  return output;
}

CastMessage CreateTestMessage() {
  CastMessage test_message;
  test_message.set_protocol_version(CastMessage::CASTV2_1_0);
  test_message.set_namespace_("ns");
  test_message.set_source_id("source");
  test_message.set_destination_id("dest");
  test_message.set_payload_type(CastMessage::STRING);
  test_message.set_payload_utf8("payload");
  return test_message;
}

class MockTCPSocket : public net::TCPClientSocket {
 public:
  explicit MockTCPSocket(const net::MockConnect& connect_data)
      : TCPClientSocket(net::AddressList(),
                        nullptr,
                        nullptr,
                        net::NetLog::Source()),
        connect_data_(connect_data),
        do_nothing_(false) {}

  explicit MockTCPSocket(bool do_nothing)
      : TCPClientSocket(net::AddressList(),
                        nullptr,
                        nullptr,
                        net::NetLog::Source()) {
    CHECK(do_nothing);
    do_nothing_ = do_nothing;
  }

  virtual int Connect(const net::CompletionCallback& callback) {
    if (do_nothing_) {
      // Stall the I/O event loop.
      return net::ERR_IO_PENDING;
    }

    if (connect_data_.mode == net::ASYNC) {
      CHECK_NE(connect_data_.result, net::ERR_IO_PENDING);
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(callback, connect_data_.result));
      return net::ERR_IO_PENDING;
    } else {
      return connect_data_.result;
    }
  }

  virtual bool SetKeepAlive(bool enable, int delay) {
    // Always return true in tests
    return true;
  }

  virtual bool SetNoDelay(bool no_delay) {
    // Always return true in tests
    return true;
  }

  MOCK_METHOD3(Read,
               int(net::IOBuffer*, int, const net::CompletionCallback&));
  MOCK_METHOD3(Write,
               int(net::IOBuffer*, int, const net::CompletionCallback&));

  virtual void Disconnect() {
    // Do nothing in tests
  }

 private:
  net::MockConnect connect_data_;
  bool do_nothing_;

  DISALLOW_COPY_AND_ASSIGN(MockTCPSocket);
};

class MockDelegate : public CastTransport::Delegate {
 public:
  MockDelegate() {}
  virtual ~MockDelegate() {}
  MOCK_METHOD1(OnError, void(ChannelError error_state));
  MOCK_METHOD1(OnMessage, void(const CastMessage& message));
  MOCK_METHOD0(Start, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

class CompleteHandler {
 public:
  CompleteHandler() {}
  MOCK_METHOD1(OnCloseComplete, void(int result));
  MOCK_METHOD1(OnConnectComplete, void(ChannelError error_state));
  MOCK_METHOD1(OnWriteComplete, void(int result));
  MOCK_METHOD1(OnReadComplete, void(int result));

 private:
  DISALLOW_COPY_AND_ASSIGN(CompleteHandler);
};

class TestCastSocket : public CastSocketImpl {
 public:
  static std::unique_ptr<TestCastSocket> Create(
      Logger* logger,
      uint64_t device_capabilities = cast_channel::CastDeviceCapability::NONE) {
    return std::unique_ptr<TestCastSocket>(
        new TestCastSocket(CreateIPEndPointForTest(), CHANNEL_AUTH_TYPE_SSL,
                           kDistantTimeoutMillis, logger, device_capabilities));
  }

  static std::unique_ptr<TestCastSocket> CreateSecure(
      Logger* logger,
      uint64_t device_capabilities = cast_channel::CastDeviceCapability::NONE) {
    return std::unique_ptr<TestCastSocket>(new TestCastSocket(
        CreateIPEndPointForTest(), CHANNEL_AUTH_TYPE_SSL_VERIFIED,
        kDistantTimeoutMillis, logger, device_capabilities));
  }

  TestCastSocket(const net::IPEndPoint& ip_endpoint,
                 ChannelAuthType channel_auth,
                 int64_t timeout_ms,
                 Logger* logger,
                 uint64_t device_capabilities)
      : TestCastSocket(ip_endpoint,
                       channel_auth,
                       timeout_ms,
                       logger,
                       new net::TestNetLog(),
                       device_capabilities) {}

  TestCastSocket(const net::IPEndPoint& ip_endpoint,
                 ChannelAuthType channel_auth,
                 int64_t timeout_ms,
                 Logger* logger,
                 net::TestNetLog* capturing_net_log,
                 uint64_t device_capabilities)
      : CastSocketImpl("some_extension_id",
                       ip_endpoint,
                       channel_auth,
                       capturing_net_log,
                       base::TimeDelta::FromMilliseconds(timeout_ms),
                       false,
                       logger,
                       device_capabilities),
        capturing_net_log_(capturing_net_log),
        ip_(ip_endpoint),
        extract_cert_result_(true),
        verify_challenge_result_(true),
        verify_challenge_disallow_(false),
        tcp_unresponsive_(false),
        mock_timer_(new base::MockTimer(false, false)),
        mock_transport_(nullptr) {}

  ~TestCastSocket() override {}

  void SetupMockTransport() {
    mock_transport_ = new MockCastTransport;
    SetTransportForTesting(base::WrapUnique(mock_transport_));
  }

  // Socket connection helpers.
  void SetupTcpConnect(net::IoMode mode, int result) {
    tcp_connect_data_.reset(new net::MockConnect(mode, result));
  }
  void SetupSslConnect(net::IoMode mode, int result) {
    ssl_connect_data_.reset(new net::MockConnect(mode, result));
  }

  // Socket I/O helpers.
  void AddWriteResult(const net::MockWrite& write) {
    writes_.push_back(write);
  }
  void AddWriteResult(net::IoMode mode, int result) {
    AddWriteResult(net::MockWrite(mode, result));
  }
  void AddWriteResultForData(net::IoMode mode, const std::string& msg) {
    AddWriteResult(mode, msg.size());
  }
  void AddReadResult(const net::MockRead& read) {
    reads_.push_back(read);
  }
  void AddReadResult(net::IoMode mode, int result) {
    AddReadResult(net::MockRead(mode, result));
  }
  void AddReadResultForData(net::IoMode mode, const std::string& data) {
    AddReadResult(net::MockRead(mode, data.c_str(), data.size()));
  }

  // Helpers for modifying other connection-related behaviors.
  void SetupTcpConnectUnresponsive() { tcp_unresponsive_ = true; }

  void SetExtractCertResult(bool value) {
    extract_cert_result_ = value;
  }

  void SetVerifyChallengeResult(bool value) {
    verify_challenge_result_ = value;
  }

  void TriggerTimeout() {
    mock_timer_->Fire();
  }

  bool TestVerifyChannelPolicyNone() {
    AuthResult authResult;
    return VerifyChannelPolicy(authResult);
  }

  bool TestVerifyChannelPolicyAudioOnly() {
    AuthResult authResult;
    authResult.channel_policies |= AuthResult::POLICY_AUDIO_ONLY;
    return VerifyChannelPolicy(authResult);
  }

  void DisallowVerifyChallengeResult() { verify_challenge_disallow_ = true; }

  MockCastTransport* GetMockTransport() {
    CHECK(mock_transport_);
    return mock_transport_;
  }

 private:
  std::unique_ptr<net::TCPClientSocket> CreateTcpSocket() override {
    if (tcp_unresponsive_) {
      return std::unique_ptr<net::TCPClientSocket>(new MockTCPSocket(true));
    } else {
      net::MockConnect* connect_data = tcp_connect_data_.get();
      connect_data->peer_addr = ip_;
      return std::unique_ptr<net::TCPClientSocket>(
          new MockTCPSocket(*connect_data));
    }
  }

  std::unique_ptr<net::SSLClientSocket> CreateSslSocket(
      std::unique_ptr<net::StreamSocket> socket) override {
    net::MockConnect* connect_data = ssl_connect_data_.get();
    connect_data->peer_addr = ip_;

    ssl_data_.reset(new net::StaticSocketDataProvider(
        reads_.data(), reads_.size(), writes_.data(), writes_.size()));
    ssl_data_->set_connect_data(*connect_data);
    // NOTE: net::MockTCPClientSocket inherits from net::SSLClientSocket !!
    return std::unique_ptr<net::SSLClientSocket>(new net::MockTCPClientSocket(
        net::AddressList(), capturing_net_log_.get(), ssl_data_.get()));
  }

  scoped_refptr<net::X509Certificate> ExtractPeerCert() override {
    return extract_cert_result_
               ? net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                         "ok_cert.pem")
               : nullptr;
  }

  bool VerifyChallengeReply() override {
    EXPECT_FALSE(verify_challenge_disallow_);
    return verify_challenge_result_;
  }

  base::Timer* GetTimer() override { return mock_timer_.get(); }

  std::unique_ptr<net::TestNetLog> capturing_net_log_;
  net::IPEndPoint ip_;
  // Simulated connect data
  std::unique_ptr<net::MockConnect> tcp_connect_data_;
  std::unique_ptr<net::MockConnect> ssl_connect_data_;
  // Simulated read / write data
  std::vector<net::MockWrite> writes_;
  std::vector<net::MockRead> reads_;
  std::unique_ptr<net::SocketDataProvider> ssl_data_;
  // Simulated result of peer cert extraction.
  bool extract_cert_result_;
  // Simulated result of verifying challenge reply.
  bool verify_challenge_result_;
  bool verify_challenge_disallow_;
  // If true, makes TCP connection process stall. For timeout testing.
  bool tcp_unresponsive_;
  std::unique_ptr<base::MockTimer> mock_timer_;
  MockCastTransport* mock_transport_;

  DISALLOW_COPY_AND_ASSIGN(TestCastSocket);
};

class CastSocketTest : public testing::Test {
 public:
  CastSocketTest()
      : logger_(
            new Logger(base::WrapUnique<base::Clock>(new base::SimpleTestClock),
                       base::Time())),
        delegate_(new MockDelegate) {}
  ~CastSocketTest() override {}

  void SetUp() override { EXPECT_CALL(*delegate_, OnMessage(_)).Times(0); }

  void TearDown() override {
    if (socket_.get()) {
      EXPECT_CALL(handler_, OnCloseComplete(net::OK));
      socket_->Close(base::Bind(&CompleteHandler::OnCloseComplete,
                                base::Unretained(&handler_)));
    }
  }

  void CreateCastSocket() { socket_ = TestCastSocket::Create(logger_); }

  void CreateCastSocketSecure() {
    socket_ = TestCastSocket::CreateSecure(logger_);
  }

  void HandleAuthHandshake() {
    socket_->SetupMockTransport();
    CastMessage challenge_proto = CreateAuthChallenge();
    EXPECT_CALL(*socket_->GetMockTransport(),
                SendMessage(EqualsProto(challenge_proto), _))
        .WillOnce(PostCompletionCallbackTask<1>(net::OK));
    EXPECT_CALL(*socket_->GetMockTransport(), Start());
    EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_NONE));
    socket_->Connect(std::move(delegate_),
                     base::Bind(&CompleteHandler::OnConnectComplete,
                                base::Unretained(&handler_)));
    RunPendingTasks();
    socket_->GetMockTransport()->current_delegate()->OnMessage(
        CreateAuthReply());
    RunPendingTasks();
  }

 protected:
  // Runs all pending tasks in the message loop.
  void RunPendingTasks() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  base::MessageLoop message_loop_;
  Logger* logger_;
  std::unique_ptr<TestCastSocket> socket_;
  CompleteHandler handler_;
  std::unique_ptr<MockDelegate> delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastSocketTest);
};

// Tests connecting and closing the socket.
TEST_F(CastSocketTest, TestConnectAndClose) {
  CreateCastSocket();
  socket_->SetupMockTransport();
  socket_->SetupTcpConnect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSslConnect(net::SYNCHRONOUS, net::OK);

  EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_NONE));
  socket_->Connect(std::move(delegate_),
                   base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());

  EXPECT_CALL(handler_, OnCloseComplete(net::OK));
  socket_->Close(base::Bind(&CompleteHandler::OnCloseComplete,
                            base::Unretained(&handler_)));
  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Tests that the following connection flow works:
// - TCP connection succeeds (async)
// - SSL connection succeeds (async)
TEST_F(CastSocketTest, TestConnect) {
  CreateCastSocket();
  socket_->SetupTcpConnect(net::ASYNC, net::OK);
  socket_->SetupSslConnect(net::ASYNC, net::OK);
  socket_->AddReadResult(net::ASYNC, net::ERR_IO_PENDING);

  EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_NONE));
  socket_->Connect(std::move(delegate_),
                   base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Tests that the following connection flow works:
// - TCP connection fails (async)
TEST_F(CastSocketTest, TestConnectFails) {
  CreateCastSocket();
  socket_->SetupTcpConnect(net::ASYNC, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_CONNECT_ERROR));
  socket_->Connect(std::move(delegate_),
                   base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
  EXPECT_EQ(proto::TCP_SOCKET_CONNECT_COMPLETE,
            logger_->GetLastErrors(socket_->id()).event_type);
  EXPECT_EQ(net::ERR_FAILED,
            logger_->GetLastErrors(socket_->id()).net_return_value);
}

// Tests that the following connection flow works:
// - TCP connection succeeds (async)
// - SSL connection succeeds (async)
// - Cert is extracted successfully
// - Challenge request is sent (async)
// - Challenge response is received (async)
// - Credentials are verified successfuly
TEST_F(CastSocketTest, TestConnectFullSecureFlowAsync) {
  CreateCastSocketSecure();
  socket_->SetupTcpConnect(net::ASYNC, net::OK);
  socket_->SetupSslConnect(net::ASYNC, net::OK);

  HandleAuthHandshake();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Tests that the following connection flow works:
// - TCP connection succeeds (sync)
// - SSL connection succeeds (sync)
// - Cert is extracted successfully
// - Challenge request is sent (sync)
// - Challenge response is received (sync)
// - Credentials are verified successfuly
TEST_F(CastSocketTest, TestConnectFullSecureFlowSync) {
  CreateCastSocketSecure();
  socket_->SetupTcpConnect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSslConnect(net::SYNCHRONOUS, net::OK);

  HandleAuthHandshake();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test that an AuthMessage with a mangled namespace triggers cancelation
// of the connection event loop.
TEST_F(CastSocketTest, TestConnectAuthMessageCorrupted) {
  CreateCastSocketSecure();
  socket_->SetupMockTransport();

  socket_->SetupTcpConnect(net::ASYNC, net::OK);
  socket_->SetupSslConnect(net::ASYNC, net::OK);

  CastMessage challenge_proto = CreateAuthChallenge();
  EXPECT_CALL(*socket_->GetMockTransport(),
              SendMessage(EqualsProto(challenge_proto), _))
      .WillOnce(PostCompletionCallbackTask<1>(net::OK));
  EXPECT_CALL(*socket_->GetMockTransport(), Start());
  EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_TRANSPORT_ERROR));
  socket_->Connect(std::move(delegate_),
                   base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();
  CastMessage mangled_auth_reply = CreateAuthReply();
  mangled_auth_reply.set_namespace_("BOGUS_NAMESPACE");

  socket_->GetMockTransport()->current_delegate()->OnMessage(
      mangled_auth_reply);
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_TRANSPORT_ERROR,
            socket_->error_state());

  // Verifies that the CastSocket's resources were torn down during channel
  // close. (see http://crbug.com/504078)
  EXPECT_EQ(nullptr, socket_->transport());
}

// Test connection error - TCP connect fails (async)
TEST_F(CastSocketTest, TestConnectTcpConnectErrorAsync) {
  CreateCastSocketSecure();

  socket_->SetupTcpConnect(net::ASYNC, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_CONNECT_ERROR));
  socket_->Connect(std::move(delegate_),
                   base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test connection error - TCP connect fails (sync)
TEST_F(CastSocketTest, TestConnectTcpConnectErrorSync) {
  CreateCastSocketSecure();

  socket_->SetupTcpConnect(net::SYNCHRONOUS, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_CONNECT_ERROR));
  socket_->Connect(std::move(delegate_),
                   base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test connection error - timeout
TEST_F(CastSocketTest, TestConnectTcpTimeoutError) {
  CreateCastSocketSecure();
  socket_->SetupTcpConnectUnresponsive();
  EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_CONNECT_TIMEOUT));
  EXPECT_CALL(*delegate_, OnError(CHANNEL_ERROR_CONNECT_TIMEOUT));
  socket_->Connect(std::move(delegate_),
                   base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CONNECTING, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
  socket_->TriggerTimeout();
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_TIMEOUT,
            socket_->error_state());
}

// Test connection error - TCP socket returns timeout
TEST_F(CastSocketTest, TestConnectTcpSocketTimeoutError) {
  CreateCastSocketSecure();
  socket_->SetupTcpConnect(net::SYNCHRONOUS, net::ERR_CONNECTION_TIMED_OUT);
  EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_CONNECT_TIMEOUT));
  EXPECT_CALL(*delegate_, OnError(CHANNEL_ERROR_CONNECT_TIMEOUT));
  socket_->Connect(std::move(delegate_),
                   base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_TIMEOUT,
            socket_->error_state());
  EXPECT_EQ(net::ERR_CONNECTION_TIMED_OUT,
            logger_->GetLastErrors(socket_->id()).net_return_value);
}

// Test connection error - SSL connect fails (async)
TEST_F(CastSocketTest, TestConnectSslConnectErrorAsync) {
  CreateCastSocketSecure();

  socket_->SetupTcpConnect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSslConnect(net::SYNCHRONOUS, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_AUTHENTICATION_ERROR));
  socket_->Connect(std::move(delegate_),
                   base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_AUTHENTICATION_ERROR,
            socket_->error_state());
}

// Test connection error - SSL connect fails (sync)
TEST_F(CastSocketTest, TestConnectSslConnectErrorSync) {
  CreateCastSocketSecure();

  socket_->SetupTcpConnect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSslConnect(net::SYNCHRONOUS, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_AUTHENTICATION_ERROR));
  socket_->Connect(std::move(delegate_),
                   base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_AUTHENTICATION_ERROR,
            socket_->error_state());
  EXPECT_EQ(net::ERR_FAILED,
            logger_->GetLastErrors(socket_->id()).net_return_value);
}

// Test connection error - SSL connect times out (sync)
TEST_F(CastSocketTest, TestConnectSslConnectTimeoutSync) {
  CreateCastSocketSecure();

  socket_->SetupTcpConnect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSslConnect(net::SYNCHRONOUS, net::ERR_CONNECTION_TIMED_OUT);

  EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_CONNECT_TIMEOUT));
  socket_->Connect(std::move(delegate_),
                   base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_TIMEOUT,
            socket_->error_state());
  EXPECT_EQ(net::ERR_CONNECTION_TIMED_OUT,
            logger_->GetLastErrors(socket_->id()).net_return_value);
}

// Test connection error - SSL connect times out (async)
TEST_F(CastSocketTest, TestConnectSslConnectTimeoutAsync) {
  CreateCastSocketSecure();

  socket_->SetupTcpConnect(net::ASYNC, net::OK);
  socket_->SetupSslConnect(net::ASYNC, net::ERR_CONNECTION_TIMED_OUT);

  EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_CONNECT_TIMEOUT));
  socket_->Connect(std::move(delegate_),
                   base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_TIMEOUT,
            socket_->error_state());
}

// Test connection error - cert extraction error (async)
TEST_F(CastSocketTest, TestConnectCertExtractionErrorAsync) {
  CreateCastSocket();
  socket_->SetupTcpConnect(net::ASYNC, net::OK);
  socket_->SetupSslConnect(net::ASYNC, net::OK);
  // Set cert extraction to fail
  socket_->SetExtractCertResult(false);

  EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_AUTHENTICATION_ERROR));
  socket_->Connect(std::move(delegate_),
                   base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_AUTHENTICATION_ERROR,
            socket_->error_state());
}

// Test connection error - cert extraction error (sync)
TEST_F(CastSocketTest, TestConnectCertExtractionErrorSync) {
  CreateCastSocket();
  socket_->SetupTcpConnect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSslConnect(net::SYNCHRONOUS, net::OK);
  // Set cert extraction to fail
  socket_->SetExtractCertResult(false);

  EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_AUTHENTICATION_ERROR));
  socket_->Connect(std::move(delegate_),
                   base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_AUTHENTICATION_ERROR,
            socket_->error_state());
}

// Test connection error - challenge send fails
TEST_F(CastSocketTest, TestConnectChallengeSendError) {
  CreateCastSocketSecure();
  socket_->SetupMockTransport();

  socket_->SetupTcpConnect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSslConnect(net::SYNCHRONOUS, net::OK);
  EXPECT_CALL(*socket_->GetMockTransport(),
              SendMessage(EqualsProto(CreateAuthChallenge()), _))
      .WillOnce(PostCompletionCallbackTask<1>(net::ERR_CONNECTION_RESET));

  EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_SOCKET_ERROR));
  socket_->Connect(std::move(delegate_),
                   base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_SOCKET_ERROR, socket_->error_state());
}

// Test connection error - challenge reply receive fails
TEST_F(CastSocketTest, TestConnectChallengeReplyReceiveError) {
  CreateCastSocketSecure();
  socket_->SetupMockTransport();

  socket_->SetupTcpConnect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSslConnect(net::SYNCHRONOUS, net::OK);
  EXPECT_CALL(*socket_->GetMockTransport(),
              SendMessage(EqualsProto(CreateAuthChallenge()), _))
      .WillOnce(PostCompletionCallbackTask<1>(net::OK));
  socket_->AddReadResult(net::SYNCHRONOUS, net::ERR_FAILED);
  EXPECT_CALL(*delegate_, OnError(CHANNEL_ERROR_SOCKET_ERROR));
  EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_SOCKET_ERROR));
  EXPECT_CALL(*socket_->GetMockTransport(), Start());
  socket_->Connect(std::move(delegate_),
                   base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();
  socket_->GetMockTransport()->current_delegate()->OnError(
      CHANNEL_ERROR_SOCKET_ERROR);
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_SOCKET_ERROR, socket_->error_state());
}

TEST_F(CastSocketTest, TestConnectChallengeVerificationFails) {
  CreateCastSocketSecure();
  socket_->SetupMockTransport();
  socket_->SetupTcpConnect(net::ASYNC, net::OK);
  socket_->SetupSslConnect(net::ASYNC, net::OK);
  socket_->SetVerifyChallengeResult(false);

  EXPECT_CALL(*delegate_, OnError(CHANNEL_ERROR_AUTHENTICATION_ERROR));
  CastMessage challenge_proto = CreateAuthChallenge();
  EXPECT_CALL(*socket_->GetMockTransport(),
              SendMessage(EqualsProto(challenge_proto), _))
      .WillOnce(PostCompletionCallbackTask<1>(net::OK));
  EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_AUTHENTICATION_ERROR));
  EXPECT_CALL(*socket_->GetMockTransport(), Start());
  socket_->Connect(std::move(delegate_),
                   base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();
  socket_->GetMockTransport()->current_delegate()->OnMessage(CreateAuthReply());
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_AUTHENTICATION_ERROR,
            socket_->error_state());
}

// Sends message data through an actual non-mocked CastTransport object,
// testing the two components in integration.
TEST_F(CastSocketTest, TestConnectEndToEndWithRealTransportAsync) {
  CreateCastSocketSecure();
  socket_->SetupTcpConnect(net::ASYNC, net::OK);
  socket_->SetupSslConnect(net::ASYNC, net::OK);

  // Set low-level auth challenge expectations.
  CastMessage challenge = CreateAuthChallenge();
  std::string challenge_str;
  EXPECT_TRUE(MessageFramer::Serialize(challenge, &challenge_str));
  socket_->AddWriteResultForData(net::ASYNC, challenge_str);

  // Set low-level auth reply expectations.
  CastMessage reply = CreateAuthReply();
  std::string reply_str;
  EXPECT_TRUE(MessageFramer::Serialize(reply, &reply_str));
  socket_->AddReadResultForData(net::ASYNC, reply_str);
  socket_->AddReadResult(net::ASYNC, net::ERR_IO_PENDING);

  CastMessage test_message = CreateTestMessage();
  std::string test_message_str;
  EXPECT_TRUE(MessageFramer::Serialize(test_message, &test_message_str));
  socket_->AddWriteResultForData(net::ASYNC, test_message_str);

  EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_NONE));
  socket_->Connect(std::move(delegate_),
                   base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();
  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());

  // Send the test message through a real transport object.
  EXPECT_CALL(handler_, OnWriteComplete(net::OK));
  socket_->transport()->SendMessage(
      test_message, base::Bind(&CompleteHandler::OnWriteComplete,
                               base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Same as TestConnectEndToEndWithRealTransportAsync, except synchronous.
TEST_F(CastSocketTest, TestConnectEndToEndWithRealTransportSync) {
  CreateCastSocketSecure();
  socket_->SetupTcpConnect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSslConnect(net::SYNCHRONOUS, net::OK);

  // Set low-level auth challenge expectations.
  CastMessage challenge = CreateAuthChallenge();
  std::string challenge_str;
  EXPECT_TRUE(MessageFramer::Serialize(challenge, &challenge_str));
  socket_->AddWriteResultForData(net::SYNCHRONOUS, challenge_str);

  // Set low-level auth reply expectations.
  CastMessage reply = CreateAuthReply();
  std::string reply_str;
  EXPECT_TRUE(MessageFramer::Serialize(reply, &reply_str));
  socket_->AddReadResultForData(net::SYNCHRONOUS, reply_str);
  socket_->AddReadResult(net::ASYNC, net::ERR_IO_PENDING);

  CastMessage test_message = CreateTestMessage();
  std::string test_message_str;
  EXPECT_TRUE(MessageFramer::Serialize(test_message, &test_message_str));
  socket_->AddWriteResultForData(net::SYNCHRONOUS, test_message_str);

  EXPECT_CALL(handler_, OnConnectComplete(CHANNEL_ERROR_NONE));
  socket_->Connect(std::move(delegate_),
                   base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();
  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());

  // Send the test message through a real transport object.
  EXPECT_CALL(handler_, OnWriteComplete(net::OK));
  socket_->transport()->SendMessage(
      test_message, base::Bind(&CompleteHandler::OnWriteComplete,
                               base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Tests channel policy verification for device with no capabilities.
TEST_F(CastSocketTest, TestChannelPolicyVerificationCapabilitiesNone) {
  socket_ =
      TestCastSocket::Create(logger_, cast_channel::CastDeviceCapability::NONE);
  EXPECT_TRUE(socket_->TestVerifyChannelPolicyNone());
  EXPECT_TRUE(socket_->TestVerifyChannelPolicyAudioOnly());
}

// Tests channel policy verification for device with video out capability.
TEST_F(CastSocketTest, TestChannelPolicyVerificationCapabilitiesVideoOut) {
  socket_ = TestCastSocket::Create(
      logger_, cast_channel::CastDeviceCapability::VIDEO_OUT);
  EXPECT_FALSE(socket_->audio_only());
  EXPECT_TRUE(socket_->TestVerifyChannelPolicyNone());
  EXPECT_FALSE(socket_->audio_only());
  EXPECT_FALSE(socket_->TestVerifyChannelPolicyAudioOnly());
  EXPECT_TRUE(socket_->audio_only());
}
}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
