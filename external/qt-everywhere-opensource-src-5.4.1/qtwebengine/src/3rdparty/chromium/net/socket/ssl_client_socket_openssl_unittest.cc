// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket.h"

#include <errno.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/values.h"
#include "crypto/openssl_util.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_data_directory.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/test_root_certs.h"
#include "net/dns/host_resolver.h"
#include "net/http/transport_security_state.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/tcp_client_socket.h"
#include "net/ssl/openssl_client_key_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_config_service.h"
#include "net/test/cert_test_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {

// These client auth tests are currently dependent on OpenSSL's struct X509.
#if defined(USE_OPENSSL_CERTS)
typedef OpenSSLClientKeyStore::ScopedEVP_PKEY ScopedEVP_PKEY;

// BIO_free is a macro, it can't be used as a template parameter.
void BIO_free_func(BIO* bio) {
    BIO_free(bio);
}

typedef crypto::ScopedOpenSSL<BIO, BIO_free_func> ScopedBIO;
typedef crypto::ScopedOpenSSL<RSA, RSA_free> ScopedRSA;
typedef crypto::ScopedOpenSSL<BIGNUM, BN_free> ScopedBIGNUM;

const SSLConfig kDefaultSSLConfig;

// Loads a PEM-encoded private key file into a scoped EVP_PKEY object.
// |filepath| is the private key file path.
// |*pkey| is reset to the new EVP_PKEY on success, untouched otherwise.
// Returns true on success, false on failure.
bool LoadPrivateKeyOpenSSL(
    const base::FilePath& filepath,
    OpenSSLClientKeyStore::ScopedEVP_PKEY* pkey) {
  std::string data;
  if (!base::ReadFileToString(filepath, &data)) {
    LOG(ERROR) << "Could not read private key file: "
               << filepath.value() << ": " << strerror(errno);
    return false;
  }
  ScopedBIO bio(
      BIO_new_mem_buf(
          const_cast<char*>(reinterpret_cast<const char*>(data.data())),
          static_cast<int>(data.size())));
  if (!bio.get()) {
    LOG(ERROR) << "Could not allocate BIO for buffer?";
    return false;
  }
  EVP_PKEY* result = PEM_read_bio_PrivateKey(bio.get(), NULL, NULL, NULL);
  if (result == NULL) {
    LOG(ERROR) << "Could not decode private key file: "
               << filepath.value();
    return false;
  }
  pkey->reset(result);
  return true;
}

class SSLClientSocketOpenSSLClientAuthTest : public PlatformTest {
 public:
  SSLClientSocketOpenSSLClientAuthTest()
      : socket_factory_(net::ClientSocketFactory::GetDefaultFactory()),
        cert_verifier_(new net::MockCertVerifier),
        transport_security_state_(new net::TransportSecurityState) {
    cert_verifier_->set_default_result(net::OK);
    context_.cert_verifier = cert_verifier_.get();
    context_.transport_security_state = transport_security_state_.get();
    key_store_ = net::OpenSSLClientKeyStore::GetInstance();
  }

  virtual ~SSLClientSocketOpenSSLClientAuthTest() {
    key_store_->Flush();
  }

 protected:
  scoped_ptr<SSLClientSocket> CreateSSLClientSocket(
      scoped_ptr<StreamSocket> transport_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config) {
    scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
    connection->SetSocket(transport_socket.Pass());
    return socket_factory_->CreateSSLClientSocket(connection.Pass(),
                                                  host_and_port,
                                                  ssl_config,
                                                  context_);
  }

  // Connect to a HTTPS test server.
  bool ConnectToTestServer(SpawnedTestServer::SSLOptions& ssl_options) {
    test_server_.reset(new SpawnedTestServer(SpawnedTestServer::TYPE_HTTPS,
                                             ssl_options,
                                             base::FilePath()));
    if (!test_server_->Start()) {
      LOG(ERROR) << "Could not start SpawnedTestServer";
      return false;
    }

    if (!test_server_->GetAddressList(&addr_)) {
      LOG(ERROR) << "Could not get SpawnedTestServer address list";
      return false;
    }

    transport_.reset(new TCPClientSocket(
        addr_, &log_, NetLog::Source()));
    int rv = callback_.GetResult(
        transport_->Connect(callback_.callback()));
    if (rv != OK) {
      LOG(ERROR) << "Could not connect to SpawnedTestServer";
      return false;
    }
    return true;
  }

  // Record a certificate's private key to ensure it can be used
  // by the OpenSSL-based SSLClientSocket implementation.
  // |ssl_config| provides a client certificate.
  // |private_key| must be an EVP_PKEY for the corresponding private key.
  // Returns true on success, false on failure.
  bool RecordPrivateKey(SSLConfig& ssl_config,
                        EVP_PKEY* private_key) {
    return key_store_->RecordClientCertPrivateKey(
        ssl_config.client_cert.get(), private_key);
  }

  // Create an SSLClientSocket object and use it to connect to a test
  // server, then wait for connection results. This must be called after
  // a succesful ConnectToTestServer() call.
  // |ssl_config| the SSL configuration to use.
  // |result| will retrieve the ::Connect() result value.
  // Returns true on succes, false otherwise. Success means that the socket
  // could be created and its Connect() was called, not that the connection
  // itself was a success.
  bool CreateAndConnectSSLClientSocket(SSLConfig& ssl_config,
                                       int* result) {
    sock_ = CreateSSLClientSocket(transport_.Pass(),
                                  test_server_->host_port_pair(),
                                  ssl_config);

    if (sock_->IsConnected()) {
      LOG(ERROR) << "SSL Socket prematurely connected";
      return false;
    }

    *result = callback_.GetResult(sock_->Connect(callback_.callback()));
    return true;
  }


  // Check that the client certificate was sent.
  // Returns true on success.
  bool CheckSSLClientSocketSentCert() {
    SSLInfo ssl_info;
    sock_->GetSSLInfo(&ssl_info);
    return ssl_info.client_cert_sent;
  }

  ClientSocketFactory* socket_factory_;
  scoped_ptr<MockCertVerifier> cert_verifier_;
  scoped_ptr<TransportSecurityState> transport_security_state_;
  SSLClientSocketContext context_;
  OpenSSLClientKeyStore* key_store_;
  scoped_ptr<SpawnedTestServer> test_server_;
  AddressList addr_;
  TestCompletionCallback callback_;
  CapturingNetLog log_;
  scoped_ptr<StreamSocket> transport_;
  scoped_ptr<SSLClientSocket> sock_;
};

// Connect to a server requesting client authentication, do not send
// any client certificates. It should refuse the connection.
TEST_F(SSLClientSocketOpenSSLClientAuthTest, NoCert) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;

  ASSERT_TRUE(ConnectToTestServer(ssl_options));

  base::FilePath certs_dir = GetTestCertsDirectory();
  SSLConfig ssl_config = kDefaultSSLConfig;

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));

  EXPECT_EQ(ERR_SSL_CLIENT_AUTH_CERT_NEEDED, rv);
  EXPECT_FALSE(sock_->IsConnected());
}

// Connect to a server requesting client authentication, and send it
// an empty certificate. It should refuse the connection.
TEST_F(SSLClientSocketOpenSSLClientAuthTest, SendEmptyCert) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  ssl_options.client_authorities.push_back(
      GetTestClientCertsDirectory().AppendASCII("client_1_ca.pem"));

  ASSERT_TRUE(ConnectToTestServer(ssl_options));

  base::FilePath certs_dir = GetTestCertsDirectory();
  SSLConfig ssl_config = kDefaultSSLConfig;
  ssl_config.send_client_cert = true;
  ssl_config.client_cert = NULL;

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));

  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock_->IsConnected());
}

// Connect to a server requesting client authentication. Send it a
// matching certificate. It should allow the connection.
TEST_F(SSLClientSocketOpenSSLClientAuthTest, SendGoodCert) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  ssl_options.client_authorities.push_back(
      GetTestClientCertsDirectory().AppendASCII("client_1_ca.pem"));

  ASSERT_TRUE(ConnectToTestServer(ssl_options));

  base::FilePath certs_dir = GetTestCertsDirectory();
  SSLConfig ssl_config = kDefaultSSLConfig;
  ssl_config.send_client_cert = true;
  ssl_config.client_cert = ImportCertFromFile(certs_dir, "client_1.pem");

  // This is required to ensure that signing works with the client
  // certificate's private key.
  OpenSSLClientKeyStore::ScopedEVP_PKEY client_private_key;
  ASSERT_TRUE(LoadPrivateKeyOpenSSL(certs_dir.AppendASCII("client_1.key"),
                                    &client_private_key));
  EXPECT_TRUE(RecordPrivateKey(ssl_config, client_private_key.get()));

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));

  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock_->IsConnected());

  EXPECT_TRUE(CheckSSLClientSocketSentCert());

  sock_->Disconnect();
  EXPECT_FALSE(sock_->IsConnected());
}
#endif  // defined(USE_OPENSSL_CERTS)

}  // namespace
}  // namespace net
