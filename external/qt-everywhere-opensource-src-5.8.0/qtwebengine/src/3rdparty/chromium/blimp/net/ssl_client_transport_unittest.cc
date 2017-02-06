// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/blimp_connection_statistics.h"
#include "blimp/net/ssl_client_transport.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/socket/socket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace {

const uint8_t kIPV4Address[] = {127, 0, 0, 1};
const uint16_t kPort = 6667;

}  // namespace

class SSLClientTransportTest : public testing::Test {
 public:
  SSLClientTransportTest() {}

  ~SSLClientTransportTest() override {}

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

  MOCK_METHOD1(ConnectComplete, void(int));

 protected:
  // Methods for provisioning simulated connection outcomes.
  void SetupTCPSyncSocketConnect(net::IPEndPoint endpoint, int result) {
    tcp_connect_.set_connect_data(
        net::MockConnect(net::SYNCHRONOUS, result, endpoint));
    socket_factory_.AddSocketDataProvider(&tcp_connect_);
  }

  void SetupTCPAsyncSocketConnect(net::IPEndPoint endpoint, int result) {
    tcp_connect_.set_connect_data(
        net::MockConnect(net::ASYNC, result, endpoint));
    socket_factory_.AddSocketDataProvider(&tcp_connect_);
  }

  void SetupSSLSyncSocketConnect(int result) {
    ssl_connect_.reset(
        new net::SSLSocketDataProvider(net::SYNCHRONOUS, result));
    socket_factory_.AddSSLSocketDataProvider(ssl_connect_.get());
  }

  void SetupSSLAsyncSocketConnect(int result) {
    ssl_connect_.reset(new net::SSLSocketDataProvider(net::ASYNC, result));
    socket_factory_.AddSSLSocketDataProvider(ssl_connect_.get());
  }

  void ConfigureTransport(const net::IPEndPoint& ip_endpoint) {
    // The mock does not interact with the cert directly, so just leave it null.
    scoped_refptr<net::X509Certificate> cert;
    transport_.reset(
        new SSLClientTransport(ip_endpoint, cert, &statistics_, &net_log_));
    transport_->SetClientSocketFactoryForTest(&socket_factory_);
  }

  base::MessageLoop message_loop;
  BlimpConnectionStatistics statistics_;
  net::NetLog net_log_;
  net::StaticSocketDataProvider tcp_connect_;
  std::unique_ptr<net::SSLSocketDataProvider> ssl_connect_;
  net::MockClientSocketFactory socket_factory_;
  std::unique_ptr<SSLClientTransport> transport_;
};

TEST_F(SSLClientTransportTest, ConnectSyncOK) {
  net::IPEndPoint endpoint(kIPV4Address, kPort);
  ConfigureTransport(endpoint);
  for (int i = 0; i < 5; ++i) {
    EXPECT_CALL(*this, ConnectComplete(net::OK));
    SetupTCPSyncSocketConnect(endpoint, net::OK);
    SetupSSLSyncSocketConnect(net::OK);
    transport_->Connect(base::Bind(&SSLClientTransportTest::ConnectComplete,
                                   base::Unretained(this)));
    EXPECT_NE(nullptr, transport_->TakeConnection().get());
    base::RunLoop().RunUntilIdle();
  }
}

TEST_F(SSLClientTransportTest, ConnectAsyncOK) {
  net::IPEndPoint endpoint(kIPV4Address, kPort);
  ConfigureTransport(endpoint);
  for (int i = 0; i < 5; ++i) {
    EXPECT_CALL(*this, ConnectComplete(net::OK));
    SetupTCPAsyncSocketConnect(endpoint, net::OK);
    SetupSSLAsyncSocketConnect(net::OK);
    transport_->Connect(base::Bind(&SSLClientTransportTest::ConnectComplete,
                                   base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    EXPECT_NE(nullptr, transport_->TakeConnection().get());
  }
}

TEST_F(SSLClientTransportTest, ConnectSyncTCPError) {
  net::IPEndPoint endpoint(kIPV4Address, kPort);
  ConfigureTransport(endpoint);
  EXPECT_CALL(*this, ConnectComplete(net::ERR_FAILED));
  SetupTCPSyncSocketConnect(endpoint, net::ERR_FAILED);
  transport_->Connect(base::Bind(&SSLClientTransportTest::ConnectComplete,
                                 base::Unretained(this)));
}

TEST_F(SSLClientTransportTest, ConnectAsyncTCPError) {
  net::IPEndPoint endpoint(kIPV4Address, kPort);
  ConfigureTransport(endpoint);
  EXPECT_CALL(*this, ConnectComplete(net::ERR_FAILED));
  SetupTCPAsyncSocketConnect(endpoint, net::ERR_FAILED);
  transport_->Connect(base::Bind(&SSLClientTransportTest::ConnectComplete,
                                 base::Unretained(this)));
}

TEST_F(SSLClientTransportTest, ConnectSyncSSLError) {
  net::IPEndPoint endpoint(kIPV4Address, kPort);
  ConfigureTransport(endpoint);
  EXPECT_CALL(*this, ConnectComplete(net::ERR_FAILED));
  SetupTCPSyncSocketConnect(endpoint, net::OK);
  SetupSSLSyncSocketConnect(net::ERR_FAILED);
  transport_->Connect(base::Bind(&SSLClientTransportTest::ConnectComplete,
                                 base::Unretained(this)));
}

TEST_F(SSLClientTransportTest, ConnectAsyncSSLError) {
  net::IPEndPoint endpoint(kIPV4Address, kPort);
  ConfigureTransport(endpoint);
  EXPECT_CALL(*this, ConnectComplete(net::ERR_FAILED));
  SetupTCPAsyncSocketConnect(endpoint, net::OK);
  SetupSSLAsyncSocketConnect(net::ERR_FAILED);
  transport_->Connect(base::Bind(&SSLClientTransportTest::ConnectComplete,
                                 base::Unretained(this)));
}

TEST_F(SSLClientTransportTest, ConnectAfterError) {
  net::IPEndPoint endpoint(kIPV4Address, kPort);
  ConfigureTransport(endpoint);

  // TCP connection fails.
  EXPECT_CALL(*this, ConnectComplete(net::ERR_FAILED));
  SetupTCPSyncSocketConnect(endpoint, net::ERR_FAILED);
  transport_->Connect(base::Bind(&SSLClientTransportTest::ConnectComplete,
                                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  // Subsequent TCP+SSL connections succeed.
  EXPECT_CALL(*this, ConnectComplete(net::OK));
  SetupTCPSyncSocketConnect(endpoint, net::OK);
  SetupSSLSyncSocketConnect(net::OK);
  transport_->Connect(base::Bind(&SSLClientTransportTest::ConnectComplete,
                                 base::Unretained(this)));
  EXPECT_NE(nullptr, transport_->TakeConnection().get());
  base::RunLoop().RunUntilIdle();
}

}  // namespace blimp
