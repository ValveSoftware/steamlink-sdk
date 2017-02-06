// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/security_state_model.h"

#include <stdint.h>

#include "components/security_state/security_state_model_client.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace security_state {

namespace {

const char kUrl[] = "https://foo.test";

class TestSecurityStateModelClient : public SecurityStateModelClient {
 public:
  TestSecurityStateModelClient()
      : initial_security_level_(SecurityStateModel::SECURE),
        connection_status_(net::SSL_CONNECTION_VERSION_TLS1_2
                           << net::SSL_CONNECTION_VERSION_SHIFT),
        cert_status_(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT),
        displayed_mixed_content_(false),
        ran_mixed_content_(false) {
    cert_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "sha1_2016.pem");
  }
  ~TestSecurityStateModelClient() override {}

  void set_connection_status(int connection_status) {
    connection_status_ = connection_status;
  }
  void SetCipherSuite(uint16_t ciphersuite) {
    net::SSLConnectionStatusSetCipherSuite(ciphersuite, &connection_status_);
  }
  void AddCertStatus(net::CertStatus cert_status) {
    cert_status_ |= cert_status;
  }
  void SetDisplayedMixedContent(bool displayed_mixed_content) {
    displayed_mixed_content_ = displayed_mixed_content;
  }
  void SetRanMixedContent(bool ran_mixed_content) {
    ran_mixed_content_ = ran_mixed_content;
  }
  void set_initial_security_level(
      SecurityStateModel::SecurityLevel security_level) {
    initial_security_level_ = security_level;
  }

  // SecurityStateModelClient:
  void GetVisibleSecurityState(
      SecurityStateModel::VisibleSecurityState* state) override {
    state->initialized = true;
    state->url = GURL(kUrl);
    state->initial_security_level = initial_security_level_;
    state->cert_id = 1;
    state->cert_status = cert_status_;
    state->connection_status = connection_status_;
    state->security_bits = 256;
    state->displayed_mixed_content = displayed_mixed_content_;
    state->ran_mixed_content = ran_mixed_content_;
  }

  bool RetrieveCert(scoped_refptr<net::X509Certificate>* cert) override {
    *cert = cert_;
    return true;
  }

  bool UsedPolicyInstalledCertificate() override { return false; }

  // Always returns true because all unit tests in this file test
  // scenarios in which the origin is secure.
  bool IsOriginSecure(const GURL& url) override { return true; }

 private:
  SecurityStateModel::SecurityLevel initial_security_level_;
  scoped_refptr<net::X509Certificate> cert_;
  int connection_status_;
  net::CertStatus cert_status_;
  bool displayed_mixed_content_;
  bool ran_mixed_content_;
};

// Tests that SHA1-signed certificates expiring in 2016 downgrade the
// security state of the page.
TEST(SecurityStateModelTest, SHA1Warning) {
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  const SecurityStateModel::SecurityInfo& security_info =
      model.GetSecurityInfo();
  EXPECT_EQ(SecurityStateModel::DEPRECATED_SHA1_MINOR,
            security_info.sha1_deprecation_status);
  EXPECT_EQ(SecurityStateModel::NONE, security_info.security_level);
}

// Tests that SHA1 warnings don't interfere with the handling of mixed
// content.
TEST(SecurityStateModelTest, SHA1WarningMixedContent) {
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  client.SetDisplayedMixedContent(true);
  const SecurityStateModel::SecurityInfo& security_info1 =
      model.GetSecurityInfo();
  EXPECT_EQ(SecurityStateModel::DEPRECATED_SHA1_MINOR,
            security_info1.sha1_deprecation_status);
  EXPECT_EQ(SecurityStateModel::DISPLAYED_MIXED_CONTENT,
            security_info1.mixed_content_status);
  EXPECT_EQ(SecurityStateModel::NONE, security_info1.security_level);

  client.set_initial_security_level(SecurityStateModel::SECURITY_ERROR);
  client.SetDisplayedMixedContent(false);
  client.SetRanMixedContent(true);
  const SecurityStateModel::SecurityInfo& security_info2 =
      model.GetSecurityInfo();
  EXPECT_EQ(SecurityStateModel::DEPRECATED_SHA1_MINOR,
            security_info2.sha1_deprecation_status);
  EXPECT_EQ(SecurityStateModel::RAN_MIXED_CONTENT,
            security_info2.mixed_content_status);
  EXPECT_EQ(SecurityStateModel::SECURITY_ERROR, security_info2.security_level);
}

// Tests that SHA1 warnings don't interfere with the handling of major
// cert errors.
TEST(SecurityStateModelTest, SHA1WarningBrokenHTTPS) {
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  client.set_initial_security_level(SecurityStateModel::SECURITY_ERROR);
  client.AddCertStatus(net::CERT_STATUS_DATE_INVALID);
  const SecurityStateModel::SecurityInfo& security_info =
      model.GetSecurityInfo();
  EXPECT_EQ(SecurityStateModel::DEPRECATED_SHA1_MINOR,
            security_info.sha1_deprecation_status);
  EXPECT_EQ(SecurityStateModel::SECURITY_ERROR, security_info.security_level);
}

// Tests that |security_info.is_secure_protocol_and_ciphersuite| is
// computed correctly.
TEST(SecurityStateModelTest, SecureProtocolAndCiphersuite) {
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc02f;
  client.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_2
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  client.SetCipherSuite(ciphersuite);
  const SecurityStateModel::SecurityInfo& security_info =
      model.GetSecurityInfo();
  EXPECT_TRUE(security_info.is_secure_protocol_and_ciphersuite);
}

TEST(SecurityStateModelTest, NonsecureProtocol) {
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc02f;
  client.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_1
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  client.SetCipherSuite(ciphersuite);
  const SecurityStateModel::SecurityInfo& security_info =
      model.GetSecurityInfo();
  EXPECT_FALSE(security_info.is_secure_protocol_and_ciphersuite);
}

TEST(SecurityStateModelTest, NonsecureCiphersuite) {
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  // TLS_RSA_WITH_AES_128_CCM_8 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc0a0;
  client.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_2
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  client.SetCipherSuite(ciphersuite);
  const SecurityStateModel::SecurityInfo& security_info =
      model.GetSecurityInfo();
  EXPECT_FALSE(security_info.is_secure_protocol_and_ciphersuite);
}

}  // namespace

}  // namespace security_state
