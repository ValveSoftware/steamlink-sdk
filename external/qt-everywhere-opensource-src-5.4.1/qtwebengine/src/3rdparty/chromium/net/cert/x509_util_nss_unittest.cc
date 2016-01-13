// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"

#include <cert.h>
#include <secoid.h>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "crypto/ec_private_key.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/signature_verifier.h"
#include "net/cert/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

CERTCertificate* CreateNSSCertHandleFromBytes(const char* data, size_t length) {
  SECItem der_cert;
  der_cert.data = reinterpret_cast<unsigned char*>(const_cast<char*>(data));
  der_cert.len  = length;
  der_cert.type = siDERCertBuffer;

  // Parse into a certificate structure.
  return CERT_NewTempCertificate(CERT_GetDefaultCertDB(), &der_cert, NULL,
                                 PR_FALSE, PR_TRUE);
}

#if !defined(OS_WIN) && !defined(OS_MACOSX)
void VerifyCertificateSignature(const std::string& der_cert,
                                const std::vector<uint8>& der_spki) {
  crypto::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));

  CERTSignedData sd;
  memset(&sd, 0, sizeof(sd));

  SECItem der_cert_item = {
    siDERCertBuffer,
    reinterpret_cast<unsigned char*>(const_cast<char*>(der_cert.data())),
    static_cast<unsigned int>(der_cert.size())
  };
  SECStatus rv = SEC_ASN1DecodeItem(arena.get(), &sd,
                                    SEC_ASN1_GET(CERT_SignedDataTemplate),
                                    &der_cert_item);
  ASSERT_EQ(SECSuccess, rv);

  // The CERTSignedData.signatureAlgorithm is decoded, but SignatureVerifier
  // wants the DER encoded form, so re-encode it again.
  SECItem* signature_algorithm = SEC_ASN1EncodeItem(
      arena.get(),
      NULL,
      &sd.signatureAlgorithm,
      SEC_ASN1_GET(SECOID_AlgorithmIDTemplate));
  ASSERT_TRUE(signature_algorithm);

  crypto::SignatureVerifier verifier;
  bool ok = verifier.VerifyInit(
      signature_algorithm->data,
      signature_algorithm->len,
      sd.signature.data,
      sd.signature.len / 8,  // Signature is a BIT STRING, convert to bytes.
      &der_spki[0],
      der_spki.size());

  ASSERT_TRUE(ok);
  verifier.VerifyUpdate(sd.data.data,
                        sd.data.len);

  ok = verifier.VerifyFinal();
  EXPECT_TRUE(ok);
}
#endif  // !defined(OS_WIN) && !defined(OS_MACOSX)

void VerifyDomainBoundCert(const std::string& domain,
                           const std::string& der_cert) {
  // Origin Bound Cert OID.
  static const char oid_string[] = "1.3.6.1.4.1.11129.2.1.6";

  // Create object neccessary for extension lookup call.
  SECItem extension_object = {
    siAsciiString,
    (unsigned char*)domain.data(),
    static_cast<unsigned int>(domain.size())
  };

  // IA5Encode and arena allocate SECItem.
  PLArenaPool* arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  SECItem* expected = SEC_ASN1EncodeItem(arena,
                                         NULL,
                                         &extension_object,
                                         SEC_ASN1_GET(SEC_IA5StringTemplate));

  ASSERT_NE(static_cast<SECItem*>(NULL), expected);

  // Create OID SECItem.
  SECItem ob_cert_oid = { siDEROID, NULL, 0 };
  SECStatus ok = SEC_StringToOID(arena, &ob_cert_oid,
                                 oid_string, 0);

  ASSERT_EQ(SECSuccess, ok);

  SECOidTag ob_cert_oid_tag = SECOID_FindOIDTag(&ob_cert_oid);

  ASSERT_NE(SEC_OID_UNKNOWN, ob_cert_oid_tag);

  // This test is run on Mac and Win where X509Certificate::os_cert_handle isn't
  // an NSS type, so we have to manually create a NSS certificate object so we
  // can use CERT_FindCertExtension.  We also check the subject and validity
  // times using NSS since X509Certificate will fail with EC certs on OSX 10.5
  // (http://crbug.com/101231).
  CERTCertificate* nss_cert = CreateNSSCertHandleFromBytes(
      der_cert.data(), der_cert.size());

  char* common_name = CERT_GetCommonName(&nss_cert->subject);
  ASSERT_TRUE(common_name);
  EXPECT_STREQ("anonymous.invalid", common_name);
  PORT_Free(common_name);
  EXPECT_EQ(SECSuccess, CERT_CertTimesValid(nss_cert));

  // Lookup Origin Bound Cert extension in generated cert.
  SECItem actual = { siBuffer, NULL, 0 };
  ok = CERT_FindCertExtension(nss_cert,
                              ob_cert_oid_tag,
                              &actual);
  CERT_DestroyCertificate(nss_cert);
  ASSERT_EQ(SECSuccess, ok);

  // Compare expected and actual extension values.
  PRBool result = SECITEM_ItemsAreEqual(expected, &actual);
  ASSERT_TRUE(result);

  // Do Cleanup.
  SECITEM_FreeItem(&actual, PR_FALSE);
  PORT_FreeArena(arena, PR_FALSE);
}

}  // namespace

// This test creates a domain-bound cert and an EC private key and
// then verifies the content of the certificate.
TEST(X509UtilNSSTest, CreateKeyAndDomainBoundCertEC) {
  // Create a sample ASCII weborigin.
  std::string domain = "weborigin.com";
  base::Time now = base::Time::Now();

  scoped_ptr<crypto::ECPrivateKey> private_key;
  std::string der_cert;
  ASSERT_TRUE(x509_util::CreateKeyAndDomainBoundCertEC(
      domain, 1,
      now,
      now + base::TimeDelta::FromDays(1),
      &private_key,
      &der_cert));

  VerifyDomainBoundCert(domain, der_cert);

#if !defined(OS_WIN) && !defined(OS_MACOSX)
  // signature_verifier_win and signature_verifier_mac can't handle EC certs.
  std::vector<uint8> spki;
  ASSERT_TRUE(private_key->ExportPublicKey(&spki));
  VerifyCertificateSignature(der_cert, spki);
#endif
}

}  // namespace net
