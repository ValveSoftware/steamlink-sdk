// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/content/content_utils.h"

#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/security_style_explanation.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "ui/base/l10n/l10n_util.h"

namespace security_state {

namespace {

// Note: This is a lossy operation. Not all of the policies that can be
// expressed by a SecurityLevel can be expressed by a blink::WebSecurityStyle.
blink::WebSecurityStyle SecurityLevelToSecurityStyle(
    security_state::SecurityLevel security_level) {
  switch (security_level) {
    case security_state::NONE:
    case security_state::HTTP_SHOW_WARNING:
      return blink::WebSecurityStyleUnauthenticated;
    case security_state::SECURITY_WARNING:
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
      return blink::WebSecurityStyleWarning;
    case security_state::EV_SECURE:
    case security_state::SECURE:
      return blink::WebSecurityStyleAuthenticated;
    case security_state::DANGEROUS:
      return blink::WebSecurityStyleAuthenticationBroken;
  }

  NOTREACHED();
  return blink::WebSecurityStyleUnknown;
}

void AddConnectionExplanation(
    const security_state::SecurityInfo& security_info,
    content::SecurityStyleExplanations* security_style_explanations) {
  // Avoid showing TLS details when we couldn't even establish a TLS connection
  // (e.g. for net errors) or if there was no real connection (some tests). We
  // check the |connection_status| to see if there was a connection.
  if (security_info.connection_status == 0) {
    return;
  }

  int ssl_version =
      net::SSLConnectionStatusToVersion(security_info.connection_status);
  const char* protocol;
  net::SSLVersionToString(&protocol, ssl_version);
  const char* key_exchange;
  const char* cipher;
  const char* mac;
  bool is_aead;
  bool is_tls13;
  uint16_t cipher_suite =
      net::SSLConnectionStatusToCipherSuite(security_info.connection_status);
  net::SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead,
                               &is_tls13, cipher_suite);
  base::string16 protocol_name = base::ASCIIToUTF16(protocol);
  const base::string16 cipher_name =
      (mac == NULL) ? base::ASCIIToUTF16(cipher)
                    : l10n_util::GetStringFUTF16(IDS_CIPHER_WITH_MAC,
                                                 base::ASCIIToUTF16(cipher),
                                                 base::ASCIIToUTF16(mac));

  // Include the key exchange group (previously known as curve) if specified.
  base::string16 key_exchange_name;
  if (is_tls13) {
    key_exchange_name = base::ASCIIToUTF16(
        SSL_get_curve_name(security_info.key_exchange_group));
  } else if (security_info.key_exchange_group != 0) {
    key_exchange_name = l10n_util::GetStringFUTF16(
        IDS_SSL_KEY_EXCHANGE_WITH_GROUP, base::ASCIIToUTF16(key_exchange),
        base::ASCIIToUTF16(
            SSL_get_curve_name(security_info.key_exchange_group)));
  } else {
    key_exchange_name = base::ASCIIToUTF16(key_exchange);
  }

  if (security_info.obsolete_ssl_status == net::OBSOLETE_SSL_NONE) {
    security_style_explanations->secure_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_STRONG_SSL_SUMMARY),
            l10n_util::GetStringFUTF8(IDS_STRONG_SSL_DESCRIPTION, protocol_name,
                                      key_exchange_name, cipher_name)));
    return;
  }

  std::vector<base::string16> description_replacements;
  int status = security_info.obsolete_ssl_status;
  int str_id;

  str_id = (status & net::OBSOLETE_SSL_MASK_PROTOCOL)
               ? IDS_SSL_AN_OBSOLETE_PROTOCOL
               : IDS_SSL_A_STRONG_PROTOCOL;
  description_replacements.push_back(l10n_util::GetStringUTF16(str_id));
  description_replacements.push_back(protocol_name);

  str_id = (status & net::OBSOLETE_SSL_MASK_KEY_EXCHANGE)
               ? IDS_SSL_AN_OBSOLETE_KEY_EXCHANGE
               : IDS_SSL_A_STRONG_KEY_EXCHANGE;
  description_replacements.push_back(l10n_util::GetStringUTF16(str_id));
  description_replacements.push_back(key_exchange_name);

  str_id = (status & net::OBSOLETE_SSL_MASK_CIPHER) ? IDS_SSL_AN_OBSOLETE_CIPHER
                                                    : IDS_SSL_A_STRONG_CIPHER;
  description_replacements.push_back(l10n_util::GetStringUTF16(str_id));
  description_replacements.push_back(cipher_name);

  security_style_explanations->info_explanations.push_back(
      content::SecurityStyleExplanation(
          l10n_util::GetStringUTF8(IDS_OBSOLETE_SSL_SUMMARY),
          base::UTF16ToUTF8(
              l10n_util::GetStringFUTF16(IDS_OBSOLETE_SSL_DESCRIPTION,
                                         description_replacements, nullptr))));
}

}  // namespace

std::unique_ptr<security_state::VisibleSecurityState> GetVisibleSecurityState(
    content::WebContents* web_contents) {
  auto state = base::MakeUnique<security_state::VisibleSecurityState>();

  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  if (!entry || !entry->GetSSL().initialized)
    return state;

  state->connection_info_initialized = true;
  state->url = entry->GetURL();
  const content::SSLStatus& ssl = entry->GetSSL();
  state->certificate = ssl.certificate;
  state->cert_status = ssl.cert_status;
  state->connection_status = ssl.connection_status;
  state->key_exchange_group = ssl.key_exchange_group;
  state->security_bits = ssl.security_bits;
  state->pkp_bypassed = ssl.pkp_bypassed;
  state->sct_verify_statuses.clear();
  state->sct_verify_statuses.insert(state->sct_verify_statuses.begin(),
                                    ssl.sct_statuses.begin(),
                                    ssl.sct_statuses.end());
  state->displayed_mixed_content =
      !!(ssl.content_status & content::SSLStatus::DISPLAYED_INSECURE_CONTENT);
  state->ran_mixed_content =
      !!(ssl.content_status & content::SSLStatus::RAN_INSECURE_CONTENT);
  state->displayed_content_with_cert_errors =
      !!(ssl.content_status &
         content::SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS);
  state->ran_content_with_cert_errors =
      !!(ssl.content_status & content::SSLStatus::RAN_CONTENT_WITH_CERT_ERRORS);
  state->displayed_password_field_on_http =
      !!(ssl.content_status &
         content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
  state->displayed_credit_card_field_on_http =
      !!(ssl.content_status &
         content::SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP);

  return state;
}

blink::WebSecurityStyle GetSecurityStyle(
    const security_state::SecurityInfo& security_info,
    content::SecurityStyleExplanations* security_style_explanations) {
  const blink::WebSecurityStyle security_style =
      SecurityLevelToSecurityStyle(security_info.security_level);

  if (security_info.security_level == security_state::HTTP_SHOW_WARNING) {
    // If the HTTP_SHOW_WARNING field trial is in use, display an
    // unauthenticated explanation explaining why the omnibox warning is
    // present.
    security_style_explanations->unauthenticated_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_PRIVATE_USER_DATA_INPUT),
            l10n_util::GetStringUTF8(IDS_PRIVATE_USER_DATA_INPUT_DESCRIPTION)));
  } else if (security_info.security_level == security_state::NONE &&
             (security_info.displayed_password_field_on_http ||
              security_info.displayed_credit_card_field_on_http)) {
    // If the HTTP_SHOW_WARNING field trial isn't in use yet, display an
    // informational note that the omnibox will contain a warning for
    // this site in a future version of Chrome.
    security_style_explanations->info_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_PRIVATE_USER_DATA_INPUT),
            l10n_util::GetStringUTF8(
                IDS_PRIVATE_USER_DATA_INPUT_FUTURE_DESCRIPTION)));
  }

  security_style_explanations->ran_insecure_content_style =
      SecurityLevelToSecurityStyle(security_state::kRanInsecureContentLevel);
  security_style_explanations->displayed_insecure_content_style =
      SecurityLevelToSecurityStyle(
          security_state::kDisplayedInsecureContentLevel);

  // Check if the page is HTTP; if so, no more explanations are needed. Note
  // that SecurityStyleUnauthenticated does not necessarily mean that
  // the page is loaded over HTTP, because the security style merely
  // represents how the embedder wishes to display the security state of
  // the page, and the embedder can choose to display HTTPS page as HTTP
  // if it wants to (for example, displaying deprecated crypto
  // algorithms with the same UI treatment as HTTP pages).
  security_style_explanations->scheme_is_cryptographic =
      security_info.scheme_is_cryptographic;
  if (!security_info.scheme_is_cryptographic) {
    return security_style;
  }

  if (security_info.sha1_deprecation_status ==
      security_state::DEPRECATED_SHA1_MAJOR) {
    security_style_explanations->broken_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_MAJOR_SHA1),
            l10n_util::GetStringUTF8(IDS_MAJOR_SHA1_DESCRIPTION),
            !!security_info.certificate));
  } else if (security_info.sha1_deprecation_status ==
             security_state::DEPRECATED_SHA1_MINOR) {
    security_style_explanations->unauthenticated_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_MINOR_SHA1),
            l10n_util::GetStringUTF8(IDS_MINOR_SHA1_DESCRIPTION),
            !!security_info.certificate));
  }

  // Record the presence of mixed content (HTTP subresources on an HTTPS
  // page).
  security_style_explanations->ran_mixed_content =
      security_info.mixed_content_status ==
          security_state::CONTENT_STATUS_RAN ||
      security_info.mixed_content_status ==
          security_state::CONTENT_STATUS_DISPLAYED_AND_RAN;
  security_style_explanations->displayed_mixed_content =
      security_info.mixed_content_status ==
          security_state::CONTENT_STATUS_DISPLAYED ||
      security_info.mixed_content_status ==
          security_state::CONTENT_STATUS_DISPLAYED_AND_RAN;

  bool is_cert_status_error = net::IsCertStatusError(security_info.cert_status);
  bool is_cert_status_minor_error =
      net::IsCertStatusMinorError(security_info.cert_status);

  // If the main resource was loaded no certificate errors or only minor
  // certificate errors, then record the presence of subresources with
  // certificate errors. Subresource certificate errors aren't recorded
  // when the main resource was loaded with major certificate errors
  // because, in the common case, these subresource certificate errors
  // would be duplicative with the main resource's error.
  if (!is_cert_status_error || is_cert_status_minor_error) {
    security_style_explanations->ran_content_with_cert_errors =
        security_info.content_with_cert_errors_status ==
            security_state::CONTENT_STATUS_RAN ||
        security_info.content_with_cert_errors_status ==
            security_state::CONTENT_STATUS_DISPLAYED_AND_RAN;
    security_style_explanations->displayed_content_with_cert_errors =
        security_info.content_with_cert_errors_status ==
            security_state::CONTENT_STATUS_DISPLAYED ||
        security_info.content_with_cert_errors_status ==
            security_state::CONTENT_STATUS_DISPLAYED_AND_RAN;
  }

  if (is_cert_status_error) {
    base::string16 error_string = base::UTF8ToUTF16(net::ErrorToString(
        net::MapCertStatusToNetError(security_info.cert_status)));

    content::SecurityStyleExplanation explanation(
        l10n_util::GetStringUTF8(IDS_CERTIFICATE_CHAIN_ERROR),
        l10n_util::GetStringFUTF8(
            IDS_CERTIFICATE_CHAIN_ERROR_DESCRIPTION_FORMAT, error_string),
        !!security_info.certificate);

    if (is_cert_status_minor_error) {
      security_style_explanations->unauthenticated_explanations.push_back(
          explanation);
    } else {
      security_style_explanations->broken_explanations.push_back(explanation);
    }
  } else {
    // If the certificate does not have errors and is not using
    // deprecated SHA1, then add an explanation that the certificate is
    // valid.
    if (security_info.sha1_deprecation_status ==
        security_state::NO_DEPRECATED_SHA1) {
      security_style_explanations->secure_explanations.push_back(
          content::SecurityStyleExplanation(
              l10n_util::GetStringUTF8(IDS_VALID_SERVER_CERTIFICATE),
              l10n_util::GetStringUTF8(
                  IDS_VALID_SERVER_CERTIFICATE_DESCRIPTION),
              !!security_info.certificate));
    }
  }

  AddConnectionExplanation(security_info, security_style_explanations);

  security_style_explanations->pkp_bypassed = security_info.pkp_bypassed;
  if (security_info.pkp_bypassed) {
    security_style_explanations->info_explanations.push_back(
        content::SecurityStyleExplanation(
            "Public-Key Pinning Bypassed",
            "Public-key pinning was bypassed by a local root certificate."));
  }

  return security_style;
}

}  // namespace security_state
