// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/content/content_utils.h"

#include "base/command_line.h"
#include "base/test/histogram_tester.h"
#include "components/security_state/core/security_state.h"
#include "components/security_state/core/switches.h"
#include "content/public/browser/security_style_explanation.h"
#include "content/public/browser/security_style_explanations.h"
#include "net/cert/cert_status_flags.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using security_state::GetSecurityStyle;

// Tests that SecurityInfo flags for subresources with certificate
// errors are reflected in the SecurityStyleExplanations produced by
// GetSecurityStyle.
TEST(SecurityStateContentUtilsTest, GetSecurityStyleForContentWithCertErrors) {
  content::SecurityStyleExplanations explanations;
  security_state::SecurityInfo security_info;
  security_info.cert_status = 0;
  security_info.scheme_is_cryptographic = true;

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_DISPLAYED_AND_RAN;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_TRUE(explanations.ran_content_with_cert_errors);
  EXPECT_TRUE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_RAN;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_TRUE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_DISPLAYED;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_TRUE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_NONE;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);
}

// Tests that SecurityStyleExplanations for subresources with cert
// errors are *not* set when the main resource has major certificate
// errors. If the main resource has certificate errors, it would be
// duplicative/confusing to also report subresources with cert errors.
TEST(SecurityStateContentUtilsTest,
     SubresourcesAndMainResourceWithMajorCertErrors) {
  content::SecurityStyleExplanations explanations;
  security_state::SecurityInfo security_info;
  security_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  security_info.scheme_is_cryptographic = true;

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_DISPLAYED_AND_RAN;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_RAN;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_DISPLAYED;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_NONE;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);
}

// Tests that SecurityStyleExplanations for subresources with cert
// errors are set when the main resource has only minor certificate
// errors. Minor errors on the main resource should not hide major
// errors on subresources.
TEST(SecurityStateContentUtilsTest,
     SubresourcesAndMainResourceWithMinorCertErrors) {
  content::SecurityStyleExplanations explanations;
  security_state::SecurityInfo security_info;
  security_info.cert_status = net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  security_info.scheme_is_cryptographic = true;

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_DISPLAYED_AND_RAN;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_TRUE(explanations.ran_content_with_cert_errors);
  EXPECT_TRUE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_RAN;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_TRUE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_DISPLAYED;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_TRUE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_NONE;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);
}

bool FindSecurityStyleExplanation(
    const std::vector<content::SecurityStyleExplanation>& explanations,
    const char* summary,
    content::SecurityStyleExplanation* explanation) {
  for (const auto& entry : explanations) {
    if (entry.summary == summary) {
      *explanation = entry;
      return true;
    }
  }

  return false;
}

// Test that connection explanations are formated as expected. Note the strings
// are not translated and so will be the same in any locale.
TEST(SecurityStateContentUtilsTest, ConnectionExplanation) {
  // Test a modern configuration with a key exchange group.
  security_state::SecurityInfo security_info;
  security_info.cert_status = net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  security_info.scheme_is_cryptographic = true;
  net::SSLConnectionStatusSetCipherSuite(
      0xcca8 /* TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 */,
      &security_info.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &security_info.connection_status);
  security_info.key_exchange_group = 29;  // X25519

  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
    content::SecurityStyleExplanation explanation;
    ASSERT_TRUE(FindSecurityStyleExplanation(
        explanations.secure_explanations, "Secure Connection", &explanation));
    EXPECT_EQ(
        "The connection to this site is encrypted and authenticated using a "
        "strong protocol (TLS 1.2), a strong key exchange (ECDHE_RSA with "
        "X25519), and a strong cipher (CHACHA20_POLY1305).",
        explanation.description);
  }

  // Some older cache entries may be missing the key exchange group, despite
  // having a cipher which should supply one.
  security_info.key_exchange_group = 0;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
    content::SecurityStyleExplanation explanation;
    ASSERT_TRUE(FindSecurityStyleExplanation(
        explanations.secure_explanations, "Secure Connection", &explanation));
    EXPECT_EQ(
        "The connection to this site is encrypted and authenticated using a "
        "strong protocol (TLS 1.2), a strong key exchange (ECDHE_RSA), and a "
        "strong cipher (CHACHA20_POLY1305).",
        explanation.description);
  }

  // TLS 1.3 ciphers use the key exchange group exclusively.
  net::SSLConnectionStatusSetCipherSuite(0x1301 /* TLS_AES_128_GCM_SHA256 */,
                                         &security_info.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_3,
                                     &security_info.connection_status);
  security_info.key_exchange_group = 29;  // X25519
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
    content::SecurityStyleExplanation explanation;
    ASSERT_TRUE(FindSecurityStyleExplanation(
        explanations.secure_explanations, "Secure Connection", &explanation));
    EXPECT_EQ(
        "The connection to this site is encrypted and authenticated using a "
        "strong protocol (TLS 1.3), a strong key exchange (X25519), and a "
        "strong cipher (AES_128_GCM).",
        explanation.description);
  }
}

// Tests that a security level of HTTP_SHOW_WARNING produces a
// content::SecurityStyle of UNAUTHENTICATED, with an explanation.
TEST(SecurityStateContentUtilsTest, HTTPWarning) {
  security_state::SecurityInfo security_info;
  content::SecurityStyleExplanations explanations;
  security_info.security_level = security_state::HTTP_SHOW_WARNING;
  blink::WebSecurityStyle security_style =
      GetSecurityStyle(security_info, &explanations);
  EXPECT_EQ(blink::WebSecurityStyleUnauthenticated, security_style);
  EXPECT_EQ(1u, explanations.unauthenticated_explanations.size());
}

// Tests that a security level of NONE when there is a password or
// credit card field on HTTP produces a content::SecurityStyle of
// UNAUTHENTICATED, with an info explanation for each.
TEST(SecurityStateContentUtilsTest, HTTPWarningInFuture) {
  security_state::SecurityInfo security_info;
  content::SecurityStyleExplanations explanations;
  security_info.security_level = security_state::NONE;
  security_info.displayed_password_field_on_http = true;
  blink::WebSecurityStyle security_style =
      GetSecurityStyle(security_info, &explanations);
  EXPECT_EQ(blink::WebSecurityStyleUnauthenticated, security_style);
  EXPECT_EQ(1u, explanations.info_explanations.size());

  explanations.info_explanations.clear();
  security_info.displayed_credit_card_field_on_http = true;
  security_style = GetSecurityStyle(security_info, &explanations);
  EXPECT_EQ(blink::WebSecurityStyleUnauthenticated, security_style);
  EXPECT_EQ(1u, explanations.info_explanations.size());

  // Check that when both password and credit card fields get displayed, only
  // one explanation is added.
  explanations.info_explanations.clear();
  security_info.displayed_credit_card_field_on_http = true;
  security_info.displayed_password_field_on_http = true;
  security_style = GetSecurityStyle(security_info, &explanations);
  EXPECT_EQ(blink::WebSecurityStyleUnauthenticated, security_style);
  EXPECT_EQ(1u, explanations.info_explanations.size());
}

}  // namespace
