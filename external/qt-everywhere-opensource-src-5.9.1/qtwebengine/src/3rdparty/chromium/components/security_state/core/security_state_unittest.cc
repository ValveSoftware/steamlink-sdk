// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/core/security_state.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/test/histogram_tester.h"
#include "components/security_state/core/switches.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace security_state {

namespace {

const char kHttpsUrl[] = "https://foo.test/";
const char kHttpUrl[] = "http://foo.test/";

bool IsOriginSecure(const GURL& url) {
  return url == kHttpsUrl;
}

class TestSecurityStateHelper {
 public:
  TestSecurityStateHelper()
      : url_(kHttpsUrl),
        cert_(net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "sha1_2016.pem")),
        connection_status_(net::SSL_CONNECTION_VERSION_TLS1_2
                           << net::SSL_CONNECTION_VERSION_SHIFT),
        cert_status_(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT),
        displayed_mixed_content_(false),
        ran_mixed_content_(false),
        malicious_content_status_(MALICIOUS_CONTENT_STATUS_NONE),
        displayed_password_field_on_http_(false),
        displayed_credit_card_field_on_http_(false) {}
  virtual ~TestSecurityStateHelper() {}

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
  void set_malicious_content_status(
      MaliciousContentStatus malicious_content_status) {
    malicious_content_status_ = malicious_content_status;
  }
  void set_displayed_password_field_on_http(
      bool displayed_password_field_on_http) {
    displayed_password_field_on_http_ = displayed_password_field_on_http;
  }
  void set_displayed_credit_card_field_on_http(
      bool displayed_credit_card_field_on_http) {
    displayed_credit_card_field_on_http_ = displayed_credit_card_field_on_http;
  }

  void SetUrl(const GURL& url) { url_ = url; }

  std::unique_ptr<VisibleSecurityState> GetVisibleSecurityState() const {
    auto state = base::MakeUnique<VisibleSecurityState>();
    state->connection_info_initialized = true;
    state->url = url_;
    state->certificate = cert_;
    state->cert_status = cert_status_;
    state->connection_status = connection_status_;
    state->security_bits = 256;
    state->displayed_mixed_content = displayed_mixed_content_;
    state->ran_mixed_content = ran_mixed_content_;
    state->malicious_content_status = malicious_content_status_;
    state->displayed_password_field_on_http = displayed_password_field_on_http_;
    state->displayed_credit_card_field_on_http =
        displayed_credit_card_field_on_http_;
    return state;
  }

  void GetSecurityInfo(SecurityInfo* security_info) const {
    security_state::GetSecurityInfo(
        GetVisibleSecurityState(),
        false /* used policy installed certificate */,
        base::Bind(&IsOriginSecure), security_info);
  }

 private:
  GURL url_;
  const scoped_refptr<net::X509Certificate> cert_;
  int connection_status_;
  net::CertStatus cert_status_;
  bool displayed_mixed_content_;
  bool ran_mixed_content_;
  MaliciousContentStatus malicious_content_status_;
  bool displayed_password_field_on_http_;
  bool displayed_credit_card_field_on_http_;
};

}  // namespace

// Tests that SHA1-signed certificates expiring in 2016 downgrade the
// security state of the page.
TEST(SecurityStateTest, SHA1Warning) {
  TestSecurityStateHelper helper;
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(DEPRECATED_SHA1_MINOR, security_info.sha1_deprecation_status);
  EXPECT_EQ(DANGEROUS, security_info.security_level);
}

// Tests that SHA1 warnings don't interfere with the handling of mixed
// content.
TEST(SecurityStateTest, SHA1WarningMixedContent) {
  TestSecurityStateHelper helper;
  helper.SetDisplayedMixedContent(true);
  SecurityInfo security_info1;
  helper.GetSecurityInfo(&security_info1);
  EXPECT_EQ(DEPRECATED_SHA1_MINOR, security_info1.sha1_deprecation_status);
  EXPECT_EQ(CONTENT_STATUS_DISPLAYED, security_info1.mixed_content_status);
  EXPECT_EQ(DANGEROUS, security_info1.security_level);

  helper.SetDisplayedMixedContent(false);
  helper.SetRanMixedContent(true);
  SecurityInfo security_info2;
  helper.GetSecurityInfo(&security_info2);
  EXPECT_EQ(DEPRECATED_SHA1_MINOR, security_info2.sha1_deprecation_status);
  EXPECT_EQ(CONTENT_STATUS_RAN, security_info2.mixed_content_status);
  EXPECT_EQ(DANGEROUS, security_info2.security_level);
}

// Tests that SHA1 warnings don't interfere with the handling of major
// cert errors.
TEST(SecurityStateTest, SHA1WarningBrokenHTTPS) {
  TestSecurityStateHelper helper;
  helper.AddCertStatus(net::CERT_STATUS_DATE_INVALID);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(DEPRECATED_SHA1_MINOR, security_info.sha1_deprecation_status);
  EXPECT_EQ(DANGEROUS, security_info.security_level);
}

// Tests that |security_info.is_secure_protocol_and_ciphersuite| is
// computed correctly.
TEST(SecurityStateTest, SecureProtocolAndCiphersuite) {
  TestSecurityStateHelper helper;
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc02f;
  helper.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_2
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  helper.SetCipherSuite(ciphersuite);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(net::OBSOLETE_SSL_NONE, security_info.obsolete_ssl_status);
}

TEST(SecurityStateTest, NonsecureProtocol) {
  TestSecurityStateHelper helper;
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc02f;
  helper.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_1
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  helper.SetCipherSuite(ciphersuite);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(net::OBSOLETE_SSL_MASK_PROTOCOL, security_info.obsolete_ssl_status);
}

TEST(SecurityStateTest, NonsecureCiphersuite) {
  TestSecurityStateHelper helper;
  // TLS_RSA_WITH_AES_128_CCM_8 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc0a0;
  helper.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_2
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  helper.SetCipherSuite(ciphersuite);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(net::OBSOLETE_SSL_MASK_KEY_EXCHANGE | net::OBSOLETE_SSL_MASK_CIPHER,
            security_info.obsolete_ssl_status);
}

// Tests that the malware/phishing status is set, and it overrides valid HTTPS.
TEST(SecurityStateTest, MalwareOverride) {
  TestSecurityStateHelper helper;
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc02f;
  helper.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_2
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  helper.SetCipherSuite(ciphersuite);

  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(MALICIOUS_CONTENT_STATUS_NONE,
            security_info.malicious_content_status);

  helper.set_malicious_content_status(MALICIOUS_CONTENT_STATUS_MALWARE);
  helper.GetSecurityInfo(&security_info);

  EXPECT_EQ(MALICIOUS_CONTENT_STATUS_MALWARE,
            security_info.malicious_content_status);
  EXPECT_EQ(DANGEROUS, security_info.security_level);
}

// Tests that the malware/phishing status is set, even if other connection info
// is not available.
TEST(SecurityStateTest, MalwareWithoutConnectionState) {
  TestSecurityStateHelper helper;
  helper.set_malicious_content_status(
      MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING,
            security_info.malicious_content_status);
  EXPECT_EQ(DANGEROUS, security_info.security_level);
}

// Tests that pseudo URLs always cause an HTTP_SHOW_WARNING to be shown,
// regardless of whether a password or credit card field was displayed.
TEST(SecurityStateTest, AlwaysWarnOnDataUrls) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL("data:text/html,<html>test</html>"));
  helper.set_displayed_password_field_on_http(false);
  helper.set_displayed_credit_card_field_on_http(false);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_FALSE(security_info.displayed_password_field_on_http);
  EXPECT_FALSE(security_info.displayed_credit_card_field_on_http);
  EXPECT_EQ(HTTP_SHOW_WARNING, security_info.security_level);
}

// Tests that password fields cause the security level to be downgraded
// to HTTP_SHOW_WARNING when the command-line switch is set.
TEST(SecurityStateTest, PasswordFieldWarning) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kMarkHttpAs, switches::kMarkHttpWithPasswordsOrCcWithChip);
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));
  helper.set_displayed_password_field_on_http(true);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.displayed_password_field_on_http);
  EXPECT_EQ(HTTP_SHOW_WARNING, security_info.security_level);
}

// Tests that credit card fields cause the security level to be downgraded
// to HTTP_SHOW_WARNING when the command-line switch is set.
TEST(SecurityStateTest, CreditCardFieldWarning) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kMarkHttpAs, switches::kMarkHttpWithPasswordsOrCcWithChip);
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));
  helper.set_displayed_credit_card_field_on_http(true);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.displayed_credit_card_field_on_http);
  EXPECT_EQ(HTTP_SHOW_WARNING, security_info.security_level);
}

// Tests that neither password nor credit fields cause the security
// level to be downgraded to HTTP_SHOW_WARNING when the command-line switch
// is NOT set.
TEST(SecurityStateTest, HttpWarningNotSetWithoutSwitch) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));
  helper.set_displayed_password_field_on_http(true);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.displayed_password_field_on_http);
  EXPECT_EQ(NONE, security_info.security_level);

  helper.set_displayed_credit_card_field_on_http(true);
  helper.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.displayed_credit_card_field_on_http);
  EXPECT_EQ(NONE, security_info.security_level);
}

// Tests that neither |displayed_password_field_on_http| nor
// |displayed_credit_card_field_on_http| is set when the corresponding
// VisibleSecurityState flags are not set.
TEST(SecurityStateTest, PrivateUserDataNotSet) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_FALSE(security_info.displayed_password_field_on_http);
  EXPECT_FALSE(security_info.displayed_credit_card_field_on_http);
  EXPECT_EQ(NONE, security_info.security_level);
}

// Tests that SSL.MarkHttpAsStatus histogram is updated when security state is
// computed for a page.
TEST(SecurityStateTest, MarkHttpAsStatusHistogram) {
  const char* kHistogramName = "SSL.MarkHttpAsStatus";
  base::HistogramTester histograms;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kMarkHttpAs, switches::kMarkHttpWithPasswordsOrCcWithChip);
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));

  // Ensure histogram recorded correctly when a non-secure password input is
  // found on the page.
  helper.set_displayed_password_field_on_http(true);
  SecurityInfo security_info;
  histograms.ExpectTotalCount(kHistogramName, 0);
  helper.GetSecurityInfo(&security_info);
  histograms.ExpectUniqueSample(kHistogramName, 2 /* HTTP_SHOW_WARNING */, 1);

  // Ensure histogram recorded correctly even without a password input.
  helper.set_displayed_password_field_on_http(false);
  helper.GetSecurityInfo(&security_info);
  histograms.ExpectUniqueSample(kHistogramName, 2 /* HTTP_SHOW_WARNING */, 2);
}

}  // namespace security_state
