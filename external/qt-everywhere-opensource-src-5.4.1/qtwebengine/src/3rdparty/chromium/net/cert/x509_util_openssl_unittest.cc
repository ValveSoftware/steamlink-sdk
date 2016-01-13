// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "crypto/ec_private_key.h"
#include "crypto/openssl_util.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_openssl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Verify that a given certificate was signed with the private key corresponding
// to a given public key.
// |der_cert| is the DER-encoded X.509 certificate.
// |der_spki| is the DER-encoded public key of the signer.
void VerifyCertificateSignature(const std::string& der_cert,
                                const std::vector<uint8>& der_spki) {
  const unsigned char* cert_data =
      reinterpret_cast<const unsigned char*>(der_cert.data());
  int cert_data_len = static_cast<int>(der_cert.size());
  crypto::ScopedOpenSSL<X509, X509_free> cert(
      d2i_X509(NULL, &cert_data, cert_data_len));
  ASSERT_TRUE(cert.get());

  // NOTE: SignatureVerifier wants the DER-encoded ASN.1 AlgorithmIdentifier
  // but there is no OpenSSL API to extract it from an X509 object (!?)
  // Use X509_verify() directly instead, which takes an EVP_PKEY.
  const unsigned char* pub_key_data = &der_spki.front();
  int pub_key_len = static_cast<int>(der_spki.size());
  crypto::ScopedOpenSSL<EVP_PKEY, EVP_PKEY_free> pub_key(
      d2i_PUBKEY(NULL, &pub_key_data, pub_key_len));
  ASSERT_TRUE(pub_key.get());

  // NOTE: X509_verify() returns 1 in case of succes, 0 or -1 on error.
  EXPECT_EQ(1, X509_verify(cert.get(), pub_key.get()));
}

// Verify the attributes of a domain-bound certificate.
// |domain| is the bound domain name.
// |der_cert| is the DER-encoded X.509 certificate.
void VerifyDomainBoundCert(const std::string& domain,
                           const std::string& der_cert) {
  // Origin Bound Cert OID.
  static const char oid_string[] = "1.3.6.1.4.1.11129.2.1.6";
  crypto::ScopedOpenSSL<ASN1_OBJECT, ASN1_OBJECT_free> oid_obj(
      OBJ_txt2obj(oid_string, 0));
  ASSERT_TRUE(oid_obj.get());

  const unsigned char* cert_data =
      reinterpret_cast<const unsigned char*>(der_cert.data());
  int cert_data_len = static_cast<int>(der_cert.size());
  crypto::ScopedOpenSSL<X509, X509_free> cert(
      d2i_X509(NULL, &cert_data, cert_data_len));
  ASSERT_TRUE(cert.get());

  // Find the extension.
  int ext_pos = X509_get_ext_by_OBJ(cert.get(), oid_obj.get(), -1);
  ASSERT_NE(-1, ext_pos);
  X509_EXTENSION* ext = X509_get_ext(cert.get(), ext_pos);
  ASSERT_TRUE(ext);

  // Check its value, it must be an ASN.1 IA5STRING
  // Which means <tag> <length> <domain>, with:
  // <tag> == 22
  // <length> is the domain length, a single byte for short forms.
  // <domain> are the domain characters.
  // See http://en.wikipedia.org/wiki/X.690
  ASN1_STRING* value_asn1 = X509_EXTENSION_get_data(ext);
  ASSERT_TRUE(value_asn1);
  std::string value_str(reinterpret_cast<const char*>(value_asn1->data),
                        value_asn1->length);

  // Check that the domain size is small enough for short form.
  ASSERT_LE(domain.size(), 127U) << "Domain is too long!";
  std::string value_expected;
  value_expected.resize(2);
  value_expected[0] = 22;
  value_expected[1] = static_cast<char>(domain.size());
  value_expected += domain;

  EXPECT_EQ(value_expected, value_str);
}

}  // namespace

TEST(X509UtilOpenSSLTest, IsSupportedValidityRange) {
  base::Time now = base::Time::Now();
  EXPECT_TRUE(x509_util::IsSupportedValidityRange(now, now));
  EXPECT_FALSE(x509_util::IsSupportedValidityRange(
      now, now - base::TimeDelta::FromSeconds(1)));

  // See x509_util_openssl.cc to see how these were computed.
  const int64 kDaysFromYear0001ToUnixEpoch = 719162;
  const int64 kDaysFromUnixEpochToYear10000 = 2932896 + 1;

  // When computing too_old / too_late, add one day to account for
  // possible leap seconds.
  base::Time too_old = base::Time::UnixEpoch() -
      base::TimeDelta::FromDays(kDaysFromYear0001ToUnixEpoch + 1);

  base::Time too_late = base::Time::UnixEpoch() +
      base::TimeDelta::FromDays(kDaysFromUnixEpochToYear10000 + 1);

  EXPECT_FALSE(x509_util::IsSupportedValidityRange(too_old, too_old));
  EXPECT_FALSE(x509_util::IsSupportedValidityRange(too_old, now));

  EXPECT_FALSE(x509_util::IsSupportedValidityRange(now, too_late));
  EXPECT_FALSE(x509_util::IsSupportedValidityRange(too_late, too_late));
}

TEST(X509UtilOpenSSLTest, CreateDomainBoundCertEC) {
  // Create a sample ASCII weborigin.
  std::string domain = "weborigin.com";
  base::Time now = base::Time::Now();

  scoped_ptr<crypto::ECPrivateKey> private_key(
      crypto::ECPrivateKey::Create());
  std::string der_cert;
  ASSERT_TRUE(
      x509_util::CreateDomainBoundCertEC(private_key.get(),
                                         x509_util::DIGEST_SHA1,
                                         domain,
                                         1,
                                         now,
                                         now + base::TimeDelta::FromDays(1),
                                         &der_cert));

  VerifyDomainBoundCert(domain, der_cert);

  // signature_verifier_win and signature_verifier_mac can't handle EC certs.
  std::vector<uint8> spki;
  ASSERT_TRUE(private_key->ExportPublicKey(&spki));
  VerifyCertificateSignature(der_cert, spki);
}

}  // namespace net
