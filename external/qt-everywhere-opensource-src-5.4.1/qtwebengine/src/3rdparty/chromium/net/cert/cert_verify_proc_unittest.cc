// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc.h"

#include <vector>

#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#elif defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mac_util.h"
#elif defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

using base::HexEncode;

namespace net {

namespace {

// A certificate for www.paypal.com with a NULL byte in the common name.
// From http://www.gossamer-threads.com/lists/fulldisc/full-disclosure/70363
unsigned char paypal_null_fingerprint[] = {
  0x4c, 0x88, 0x9e, 0x28, 0xd7, 0x7a, 0x44, 0x1e, 0x13, 0xf2, 0x6a, 0xba,
  0x1f, 0xe8, 0x1b, 0xd6, 0xab, 0x7b, 0xe8, 0xd7
};

// Mock CertVerifyProc that will set |verify_result->is_issued_by_known_root|
// for all certificates that are Verified.
class WellKnownCaCertVerifyProc : public CertVerifyProc {
 public:
  // Initialize a CertVerifyProc that will set
  // |verify_result->is_issued_by_known_root| to |is_well_known|.
  explicit WellKnownCaCertVerifyProc(bool is_well_known)
      : is_well_known_(is_well_known) {}

  // CertVerifyProc implementation:
  virtual bool SupportsAdditionalTrustAnchors() const OVERRIDE { return false; }

 protected:
  virtual ~WellKnownCaCertVerifyProc() {}

 private:
  virtual int VerifyInternal(X509Certificate* cert,
                             const std::string& hostname,
                             int flags,
                             CRLSet* crl_set,
                             const CertificateList& additional_trust_anchors,
                             CertVerifyResult* verify_result) OVERRIDE;

  const bool is_well_known_;

  DISALLOW_COPY_AND_ASSIGN(WellKnownCaCertVerifyProc);
};

int WellKnownCaCertVerifyProc::VerifyInternal(
    X509Certificate* cert,
    const std::string& hostname,
    int flags,
    CRLSet* crl_set,
    const CertificateList& additional_trust_anchors,
    CertVerifyResult* verify_result) {
  verify_result->is_issued_by_known_root = is_well_known_;
  return OK;
}

bool SupportsReturningVerifiedChain() {
#if defined(OS_ANDROID)
  // Before API level 17, Android does not expose the APIs necessary to get at
  // the verified certificate chain.
  if (base::android::BuildInfo::GetInstance()->sdk_int() < 17)
    return false;
#endif
  return true;
}

bool SupportsDetectingKnownRoots() {
#if defined(OS_ANDROID)
  // Before API level 17, Android does not expose the APIs necessary to get at
  // the verified certificate chain and detect known roots.
  if (base::android::BuildInfo::GetInstance()->sdk_int() < 17)
    return false;
#endif
  return true;
}

}  // namespace

class CertVerifyProcTest : public testing::Test {
 public:
  CertVerifyProcTest()
      : verify_proc_(CertVerifyProc::CreateDefault()) {
  }
  virtual ~CertVerifyProcTest() {}

 protected:
  bool SupportsAdditionalTrustAnchors() {
    return verify_proc_->SupportsAdditionalTrustAnchors();
  }

  int Verify(X509Certificate* cert,
             const std::string& hostname,
             int flags,
             CRLSet* crl_set,
             const CertificateList& additional_trust_anchors,
             CertVerifyResult* verify_result) {
    return verify_proc_->Verify(cert, hostname, flags, crl_set,
                                additional_trust_anchors, verify_result);
  }

  const CertificateList empty_cert_list_;
  scoped_refptr<CertVerifyProc> verify_proc_;
};

TEST_F(CertVerifyProcTest, DISABLED_WithoutRevocationChecking) {
  // Check that verification without revocation checking works.
  CertificateList certs = CreateCertificateListFromFile(
      GetTestCertsDirectory(),
      "googlenew.chain.pem",
      X509Certificate::FORMAT_PEM_CERT_SEQUENCE);

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(certs[1]->os_cert_handle());

  scoped_refptr<X509Certificate> google_full_chain =
      X509Certificate::CreateFromHandle(certs[0]->os_cert_handle(),
                                        intermediates);

  CertVerifyResult verify_result;
  EXPECT_EQ(OK,
            Verify(google_full_chain.get(),
                   "www.google.com",
                   0 /* flags */,
                   NULL,
                   empty_cert_list_,
                   &verify_result));
}

#if defined(OS_ANDROID) || defined(USE_OPENSSL_CERTS)
// TODO(jnd): http://crbug.com/117478 - EV verification is not yet supported.
#define MAYBE_EVVerification DISABLED_EVVerification
#else
#define MAYBE_EVVerification EVVerification
#endif
TEST_F(CertVerifyProcTest, MAYBE_EVVerification) {
  CertificateList certs = CreateCertificateListFromFile(
      GetTestCertsDirectory(),
      "comodo.chain.pem",
      X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_EQ(3U, certs.size());

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(certs[1]->os_cert_handle());
  intermediates.push_back(certs[2]->os_cert_handle());

  scoped_refptr<X509Certificate> comodo_chain =
      X509Certificate::CreateFromHandle(certs[0]->os_cert_handle(),
                                        intermediates);

  scoped_refptr<CRLSet> crl_set(CRLSet::ForTesting(false, NULL, ""));
  CertVerifyResult verify_result;
  int flags = CertVerifier::VERIFY_EV_CERT;
  int error = Verify(comodo_chain.get(),
                     "comodo.com",
                     flags,
                     crl_set.get(),
                     empty_cert_list_,
                     &verify_result);
  EXPECT_EQ(OK, error);
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_IS_EV);
}

TEST_F(CertVerifyProcTest, PaypalNullCertParsing) {
  scoped_refptr<X509Certificate> paypal_null_cert(
      X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(paypal_null_der),
          sizeof(paypal_null_der)));

  ASSERT_NE(static_cast<X509Certificate*>(NULL), paypal_null_cert);

  const SHA1HashValue& fingerprint =
      paypal_null_cert->fingerprint();
  for (size_t i = 0; i < 20; ++i)
    EXPECT_EQ(paypal_null_fingerprint[i], fingerprint.data[i]);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(paypal_null_cert.get(),
                     "www.paypal.com",
                     flags,
                     NULL,
                     empty_cert_list_,
                     &verify_result);
#if defined(USE_NSS) || defined(OS_IOS) || defined(OS_ANDROID)
  EXPECT_EQ(ERR_CERT_COMMON_NAME_INVALID, error);
#else
  // TOOD(bulach): investigate why macosx and win aren't returning
  // ERR_CERT_INVALID or ERR_CERT_COMMON_NAME_INVALID.
  EXPECT_EQ(ERR_CERT_AUTHORITY_INVALID, error);
#endif
  // Either the system crypto library should correctly report a certificate
  // name mismatch, or our certificate blacklist should cause us to report an
  // invalid certificate.
#if defined(USE_NSS) || defined(OS_WIN) || defined(OS_IOS)
  EXPECT_TRUE(verify_result.cert_status &
              (CERT_STATUS_COMMON_NAME_INVALID | CERT_STATUS_INVALID));
#endif
}

// A regression test for http://crbug.com/31497.
#if defined(OS_ANDROID)
// Disabled on Android, as the Android verification libraries require an
// explicit policy to be specified, even when anyPolicy is permitted.
#define MAYBE_IntermediateCARequireExplicitPolicy \
    DISABLED_IntermediateCARequireExplicitPolicy
#else
#define MAYBE_IntermediateCARequireExplicitPolicy \
    IntermediateCARequireExplicitPolicy
#endif
TEST_F(CertVerifyProcTest, MAYBE_IntermediateCARequireExplicitPolicy) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "explicit-policy-chain.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(certs[1]->os_cert_handle());

  scoped_refptr<X509Certificate> cert =
      X509Certificate::CreateFromHandle(certs[0]->os_cert_handle(),
                                        intermediates);
  ASSERT_TRUE(cert.get());

  ScopedTestRoot scoped_root(certs[2].get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(cert.get(),
                     "policy_test.example",
                     flags,
                     NULL,
                     empty_cert_list_,
                     &verify_result);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(0u, verify_result.cert_status);
}

// Test for bug 58437.
// This certificate will expire on 2011-12-21. The test will still
// pass if error == ERR_CERT_DATE_INVALID.
// This test is DISABLED because it appears that we cannot do
// certificate revocation checking when running all of the net unit tests.
// This test passes when run individually, but when run with all of the net
// unit tests, the call to PKIXVerifyCert returns the NSS error -8180, which is
// SEC_ERROR_REVOKED_CERTIFICATE. This indicates a lack of revocation
// status, i.e. that the revocation check is failing for some reason.
TEST_F(CertVerifyProcTest, DISABLED_GlobalSignR3EVTest) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "2029_globalsign_com_cert.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), server_cert);

  scoped_refptr<X509Certificate> intermediate_cert =
      ImportCertFromFile(certs_dir, "globalsign_ev_sha256_ca_cert.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), intermediate_cert);

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(intermediate_cert->os_cert_handle());
  scoped_refptr<X509Certificate> cert_chain =
      X509Certificate::CreateFromHandle(server_cert->os_cert_handle(),
                                        intermediates);

  CertVerifyResult verify_result;
  int flags = CertVerifier::VERIFY_REV_CHECKING_ENABLED |
              CertVerifier::VERIFY_EV_CERT;
  int error = Verify(cert_chain.get(),
                     "2029.globalsign.com",
                     flags,
                     NULL,
                     empty_cert_list_,
                     &verify_result);
  if (error == OK)
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_IS_EV);
  else
    EXPECT_EQ(ERR_CERT_DATE_INVALID, error);
}

// Test that verifying an ECDSA certificate doesn't crash on XP. (See
// crbug.com/144466).
TEST_F(CertVerifyProcTest, ECDSA_RSA) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir,
                         "prime256v1-ecdsa-ee-by-1024-rsa-intermediate.pem");

  CertVerifyResult verify_result;
  Verify(cert.get(), "127.0.0.1", 0, NULL, empty_cert_list_, &verify_result);

  // We don't check verify_result because the certificate is signed by an
  // unknown CA and will be considered invalid on XP because of the ECDSA
  // public key.
}

// Currently, only RSA and DSA keys are checked for weakness, and our example
// weak size is 768. These could change in the future.
//
// Note that this means there may be false negatives: keys for other
// algorithms and which are weak will pass this test.
static bool IsWeakKeyType(const std::string& key_type) {
  size_t pos = key_type.find("-");
  std::string size = key_type.substr(0, pos);
  std::string type = key_type.substr(pos + 1);

  if (type == "rsa" || type == "dsa")
    return size == "768";

  return false;
}

TEST_F(CertVerifyProcTest, RejectWeakKeys) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  typedef std::vector<std::string> Strings;
  Strings key_types;

  // generate-weak-test-chains.sh currently has:
  //     key_types="768-rsa 1024-rsa 2048-rsa prime256v1-ecdsa"
  // We must use the same key types here. The filenames generated look like:
  //     2048-rsa-ee-by-768-rsa-intermediate.pem
  key_types.push_back("768-rsa");
  key_types.push_back("1024-rsa");
  key_types.push_back("2048-rsa");

  bool use_ecdsa = true;
#if defined(OS_WIN)
  use_ecdsa = base::win::GetVersion() > base::win::VERSION_XP;
#endif

  if (use_ecdsa)
    key_types.push_back("prime256v1-ecdsa");

  // Add the root that signed the intermediates for this test.
  scoped_refptr<X509Certificate> root_cert =
      ImportCertFromFile(certs_dir, "2048-rsa-root.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), root_cert);
  ScopedTestRoot scoped_root(root_cert.get());

  // Now test each chain.
  for (Strings::const_iterator ee_type = key_types.begin();
       ee_type != key_types.end(); ++ee_type) {
    for (Strings::const_iterator signer_type = key_types.begin();
         signer_type != key_types.end(); ++signer_type) {
      std::string basename = *ee_type + "-ee-by-" + *signer_type +
          "-intermediate.pem";
      SCOPED_TRACE(basename);
      scoped_refptr<X509Certificate> ee_cert =
          ImportCertFromFile(certs_dir, basename);
      ASSERT_NE(static_cast<X509Certificate*>(NULL), ee_cert);

      basename = *signer_type + "-intermediate.pem";
      scoped_refptr<X509Certificate> intermediate =
          ImportCertFromFile(certs_dir, basename);
      ASSERT_NE(static_cast<X509Certificate*>(NULL), intermediate);

      X509Certificate::OSCertHandles intermediates;
      intermediates.push_back(intermediate->os_cert_handle());
      scoped_refptr<X509Certificate> cert_chain =
          X509Certificate::CreateFromHandle(ee_cert->os_cert_handle(),
                                            intermediates);

      CertVerifyResult verify_result;
      int error = Verify(cert_chain.get(),
                         "127.0.0.1",
                         0,
                         NULL,
                         empty_cert_list_,
                         &verify_result);

      if (IsWeakKeyType(*ee_type) || IsWeakKeyType(*signer_type)) {
        EXPECT_NE(OK, error);
        EXPECT_EQ(CERT_STATUS_WEAK_KEY,
                  verify_result.cert_status & CERT_STATUS_WEAK_KEY);
        EXPECT_NE(CERT_STATUS_INVALID,
                  verify_result.cert_status & CERT_STATUS_INVALID);
      } else {
        EXPECT_EQ(OK, error);
        EXPECT_EQ(0U, verify_result.cert_status & CERT_STATUS_WEAK_KEY);
      }
    }
  }
}

// Regression test for http://crbug.com/108514.
#if defined(OS_MACOSX) && !defined(OS_IOS)
// Disabled on OS X - Security.framework doesn't ignore superflous certificates
// provided by servers. See CertVerifyProcTest.CybertrustGTERoot for further
// details.
#define MAYBE_ExtraneousMD5RootCert DISABLED_ExtraneousMD5RootCert
#else
#define MAYBE_ExtraneousMD5RootCert ExtraneousMD5RootCert
#endif
TEST_F(CertVerifyProcTest, MAYBE_ExtraneousMD5RootCert) {
  if (!SupportsReturningVerifiedChain()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "cross-signed-leaf.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), server_cert.get());

  scoped_refptr<X509Certificate> extra_cert =
      ImportCertFromFile(certs_dir, "cross-signed-root-md5.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), extra_cert.get());

  scoped_refptr<X509Certificate> root_cert =
      ImportCertFromFile(certs_dir, "cross-signed-root-sha1.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), root_cert.get());

  ScopedTestRoot scoped_root(root_cert.get());

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(extra_cert->os_cert_handle());
  scoped_refptr<X509Certificate> cert_chain =
      X509Certificate::CreateFromHandle(server_cert->os_cert_handle(),
                                        intermediates);

  CertVerifyResult verify_result;
  int flags = 0;
  int error = Verify(cert_chain.get(),
                     "127.0.0.1",
                     flags,
                     NULL,
                     empty_cert_list_,
                     &verify_result);
  EXPECT_EQ(OK, error);

  // The extra MD5 root should be discarded
  ASSERT_TRUE(verify_result.verified_cert.get());
  ASSERT_EQ(1u,
            verify_result.verified_cert->GetIntermediateCertificates().size());
  EXPECT_TRUE(X509Certificate::IsSameOSCert(
        verify_result.verified_cert->GetIntermediateCertificates().front(),
        root_cert->os_cert_handle()));

  EXPECT_FALSE(verify_result.has_md5);
}

// Test for bug 94673.
TEST_F(CertVerifyProcTest, GoogleDigiNotarTest) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "google_diginotar.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), server_cert);

  scoped_refptr<X509Certificate> intermediate_cert =
      ImportCertFromFile(certs_dir, "diginotar_public_ca_2025.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), intermediate_cert);

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(intermediate_cert->os_cert_handle());
  scoped_refptr<X509Certificate> cert_chain =
      X509Certificate::CreateFromHandle(server_cert->os_cert_handle(),
                                        intermediates);

  CertVerifyResult verify_result;
  int flags = CertVerifier::VERIFY_REV_CHECKING_ENABLED;
  int error = Verify(cert_chain.get(),
                     "mail.google.com",
                     flags,
                     NULL,
                     empty_cert_list_,
                     &verify_result);
  EXPECT_NE(OK, error);

  // Now turn off revocation checking.  Certificate verification should still
  // fail.
  flags = 0;
  error = Verify(cert_chain.get(),
                 "mail.google.com",
                 flags,
                 NULL,
                 empty_cert_list_,
                 &verify_result);
  EXPECT_NE(OK, error);
}

TEST_F(CertVerifyProcTest, DigiNotarCerts) {
  static const char* const kDigiNotarFilenames[] = {
    "diginotar_root_ca.pem",
    "diginotar_cyber_ca.pem",
    "diginotar_services_1024_ca.pem",
    "diginotar_pkioverheid.pem",
    "diginotar_pkioverheid_g2.pem",
    NULL,
  };

  base::FilePath certs_dir = GetTestCertsDirectory();

  for (size_t i = 0; kDigiNotarFilenames[i]; i++) {
    scoped_refptr<X509Certificate> diginotar_cert =
        ImportCertFromFile(certs_dir, kDigiNotarFilenames[i]);
    std::string der_bytes;
    ASSERT_TRUE(X509Certificate::GetDEREncoded(
        diginotar_cert->os_cert_handle(), &der_bytes));

    base::StringPiece spki;
    ASSERT_TRUE(asn1::ExtractSPKIFromDERCert(der_bytes, &spki));

    std::string spki_sha1 = base::SHA1HashString(spki.as_string());

    HashValueVector public_keys;
    HashValue hash(HASH_VALUE_SHA1);
    ASSERT_EQ(hash.size(), spki_sha1.size());
    memcpy(hash.data(), spki_sha1.data(), spki_sha1.size());
    public_keys.push_back(hash);

    EXPECT_TRUE(CertVerifyProc::IsPublicKeyBlacklisted(public_keys)) <<
        "Public key not blocked for " << kDigiNotarFilenames[i];
  }
}

TEST_F(CertVerifyProcTest, NameConstraintsOk) {
  CertificateList ca_cert_list =
      CreateCertificateListFromFile(GetTestCertsDirectory(),
                                    "root_ca_cert.pem",
                                    X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, ca_cert_list.size());
  ScopedTestRoot test_root(ca_cert_list[0]);

  CertificateList cert_list = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "name_constraint_ok.crt",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, cert_list.size());

  X509Certificate::OSCertHandles intermediates;
  scoped_refptr<X509Certificate> leaf =
      X509Certificate::CreateFromHandle(cert_list[0]->os_cert_handle(),
                                        intermediates);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(leaf.get(),
                     "test.example.com",
                     flags,
                     NULL,
                     empty_cert_list_,
                     &verify_result);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(0U, verify_result.cert_status);
}

TEST_F(CertVerifyProcTest, NameConstraintsFailure) {
  if (!SupportsReturningVerifiedChain()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  CertificateList ca_cert_list =
      CreateCertificateListFromFile(GetTestCertsDirectory(),
                                    "root_ca_cert.pem",
                                    X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, ca_cert_list.size());
  ScopedTestRoot test_root(ca_cert_list[0]);

  CertificateList cert_list = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "name_constraint_bad.crt",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, cert_list.size());

  X509Certificate::OSCertHandles intermediates;
  scoped_refptr<X509Certificate> leaf =
      X509Certificate::CreateFromHandle(cert_list[0]->os_cert_handle(),
                                        intermediates);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(leaf.get(),
                     "test.example.com",
                     flags,
                     NULL,
                     empty_cert_list_,
                     &verify_result);
  EXPECT_EQ(ERR_CERT_NAME_CONSTRAINT_VIOLATION, error);
  EXPECT_EQ(CERT_STATUS_NAME_CONSTRAINT_VIOLATION,
            verify_result.cert_status & CERT_STATUS_NAME_CONSTRAINT_VIOLATION);
}

TEST_F(CertVerifyProcTest, TestKnownRoot) {
  if (!SupportsDetectingKnownRoots()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  base::FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "satveda.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(2U, certs.size());

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(certs[1]->os_cert_handle());

  scoped_refptr<X509Certificate> cert_chain =
      X509Certificate::CreateFromHandle(certs[0]->os_cert_handle(),
                                        intermediates);

  int flags = 0;
  CertVerifyResult verify_result;
  // This will blow up, May 24th, 2019. Sorry! Please disable and file a bug
  // against agl. See also PublicKeyHashes.
  int error = Verify(cert_chain.get(),
                     "satveda.com",
                     flags,
                     NULL,
                     empty_cert_list_,
                     &verify_result);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(0U, verify_result.cert_status);
  EXPECT_TRUE(verify_result.is_issued_by_known_root);
}

// The certse.pem certificate has been revoked. crbug.com/259723.
TEST_F(CertVerifyProcTest, PublicKeyHashes) {
  if (!SupportsReturningVerifiedChain()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  base::FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "satveda.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(2U, certs.size());

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(certs[1]->os_cert_handle());

  scoped_refptr<X509Certificate> cert_chain =
      X509Certificate::CreateFromHandle(certs[0]->os_cert_handle(),
                                        intermediates);
  int flags = 0;
  CertVerifyResult verify_result;

  // This will blow up, May 24th, 2019. Sorry! Please disable and file a bug
  // against agl. See also TestKnownRoot.
  int error = Verify(cert_chain.get(),
                     "satveda.com",
                     flags,
                     NULL,
                     empty_cert_list_,
                     &verify_result);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(0U, verify_result.cert_status);
  ASSERT_LE(2U, verify_result.public_key_hashes.size());

  HashValueVector sha1_hashes;
  for (size_t i = 0; i < verify_result.public_key_hashes.size(); ++i) {
    if (verify_result.public_key_hashes[i].tag != HASH_VALUE_SHA1)
      continue;
    sha1_hashes.push_back(verify_result.public_key_hashes[i]);
  }
  ASSERT_LE(2u, sha1_hashes.size());

  for (size_t i = 0; i < 2; ++i) {
    EXPECT_EQ(HexEncode(kSatvedaSPKIs[i], base::kSHA1Length),
              HexEncode(sha1_hashes[i].data(), base::kSHA1Length));
  }

  HashValueVector sha256_hashes;
  for (size_t i = 0; i < verify_result.public_key_hashes.size(); ++i) {
    if (verify_result.public_key_hashes[i].tag != HASH_VALUE_SHA256)
      continue;
    sha256_hashes.push_back(verify_result.public_key_hashes[i]);
  }
  ASSERT_LE(2u, sha256_hashes.size());

  for (size_t i = 0; i < 2; ++i) {
    EXPECT_EQ(HexEncode(kSatvedaSPKIsSHA256[i], crypto::kSHA256Length),
              HexEncode(sha256_hashes[i].data(), crypto::kSHA256Length));
  }
}

// A regression test for http://crbug.com/70293.
// The Key Usage extension in this RSA SSL server certificate does not have
// the keyEncipherment bit.
TEST_F(CertVerifyProcTest, InvalidKeyUsage) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "invalid_key_usage_cert.der");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), server_cert);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(server_cert.get(),
                     "jira.aquameta.com",
                     flags,
                     NULL,
                     empty_cert_list_,
                     &verify_result);
#if defined(USE_OPENSSL_CERTS) && !defined(OS_ANDROID)
  // This certificate has two errors: "invalid key usage" and "untrusted CA".
  // However, OpenSSL returns only one (the latter), and we can't detect
  // the other errors.
  EXPECT_EQ(ERR_CERT_AUTHORITY_INVALID, error);
#else
  EXPECT_EQ(ERR_CERT_INVALID, error);
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
#endif
  // TODO(wtc): fix http://crbug.com/75520 to get all the certificate errors
  // from NSS.
#if !defined(USE_NSS) && !defined(OS_IOS) && !defined(OS_ANDROID)
  // The certificate is issued by an unknown CA.
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);
#endif
}

// Basic test for returning the chain in CertVerifyResult. Note that the
// returned chain may just be a reflection of the originally supplied chain;
// that is, if any errors occur, the default chain returned is an exact copy
// of the certificate to be verified. The remaining VerifyReturn* tests are
// used to ensure that the actual, verified chain is being returned by
// Verify().
TEST_F(CertVerifyProcTest, VerifyReturnChainBasic) {
  if (!SupportsReturningVerifiedChain()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  base::FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "x509_verify_results.chain.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(certs[1]->os_cert_handle());
  intermediates.push_back(certs[2]->os_cert_handle());

  ScopedTestRoot scoped_root(certs[2].get());

  scoped_refptr<X509Certificate> google_full_chain =
      X509Certificate::CreateFromHandle(certs[0]->os_cert_handle(),
                                        intermediates);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), google_full_chain);
  ASSERT_EQ(2U, google_full_chain->GetIntermediateCertificates().size());

  CertVerifyResult verify_result;
  EXPECT_EQ(static_cast<X509Certificate*>(NULL), verify_result.verified_cert);
  int error = Verify(google_full_chain.get(),
                     "127.0.0.1",
                     0,
                     NULL,
                     empty_cert_list_,
                     &verify_result);
  EXPECT_EQ(OK, error);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), verify_result.verified_cert);

  EXPECT_NE(google_full_chain, verify_result.verified_cert);
  EXPECT_TRUE(X509Certificate::IsSameOSCert(
      google_full_chain->os_cert_handle(),
      verify_result.verified_cert->os_cert_handle()));
  const X509Certificate::OSCertHandles& return_intermediates =
      verify_result.verified_cert->GetIntermediateCertificates();
  ASSERT_EQ(2U, return_intermediates.size());
  EXPECT_TRUE(X509Certificate::IsSameOSCert(return_intermediates[0],
                                            certs[1]->os_cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(return_intermediates[1],
                                            certs[2]->os_cert_handle()));
}

// Test that certificates issued for 'intranet' names (that is, containing no
// known public registry controlled domain information) issued by well-known
// CAs are flagged appropriately, while certificates that are issued by
// internal CAs are not flagged.
TEST_F(CertVerifyProcTest, IntranetHostsRejected) {
  if (!SupportsDetectingKnownRoots()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  CertificateList cert_list = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "ok_cert.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, cert_list.size());
  scoped_refptr<X509Certificate> cert(cert_list[0]);

  CertVerifyResult verify_result;
  int error = 0;

  // Intranet names for public CAs should be flagged:
  verify_proc_ = new WellKnownCaCertVerifyProc(true);
  error =
      Verify(cert.get(), "intranet", 0, NULL, empty_cert_list_, &verify_result);
  EXPECT_EQ(OK, error);
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_NON_UNIQUE_NAME);

  // However, if the CA is not well known, these should not be flagged:
  verify_proc_ = new WellKnownCaCertVerifyProc(false);
  error =
      Verify(cert.get(), "intranet", 0, NULL, empty_cert_list_, &verify_result);
  EXPECT_EQ(OK, error);
  EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_NON_UNIQUE_NAME);
}

// Test that the certificate returned in CertVerifyResult is able to reorder
// certificates that are not ordered from end-entity to root. While this is
// a protocol violation if sent during a TLS handshake, if multiple sources
// of intermediate certificates are combined, it's possible that order may
// not be maintained.
TEST_F(CertVerifyProcTest, VerifyReturnChainProperlyOrdered) {
  if (!SupportsReturningVerifiedChain()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  base::FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "x509_verify_results.chain.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  // Construct the chain out of order.
  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(certs[2]->os_cert_handle());
  intermediates.push_back(certs[1]->os_cert_handle());

  ScopedTestRoot scoped_root(certs[2].get());

  scoped_refptr<X509Certificate> google_full_chain =
      X509Certificate::CreateFromHandle(certs[0]->os_cert_handle(),
                                        intermediates);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), google_full_chain);
  ASSERT_EQ(2U, google_full_chain->GetIntermediateCertificates().size());

  CertVerifyResult verify_result;
  EXPECT_EQ(static_cast<X509Certificate*>(NULL), verify_result.verified_cert);
  int error = Verify(google_full_chain.get(),
                     "127.0.0.1",
                     0,
                     NULL,
                     empty_cert_list_,
                     &verify_result);
  EXPECT_EQ(OK, error);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), verify_result.verified_cert);

  EXPECT_NE(google_full_chain, verify_result.verified_cert);
  EXPECT_TRUE(X509Certificate::IsSameOSCert(
      google_full_chain->os_cert_handle(),
      verify_result.verified_cert->os_cert_handle()));
  const X509Certificate::OSCertHandles& return_intermediates =
      verify_result.verified_cert->GetIntermediateCertificates();
  ASSERT_EQ(2U, return_intermediates.size());
  EXPECT_TRUE(X509Certificate::IsSameOSCert(return_intermediates[0],
                                            certs[1]->os_cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(return_intermediates[1],
                                            certs[2]->os_cert_handle()));
}

// Test that Verify() filters out certificates which are not related to
// or part of the certificate chain being verified.
TEST_F(CertVerifyProcTest, VerifyReturnChainFiltersUnrelatedCerts) {
  if (!SupportsReturningVerifiedChain()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  base::FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "x509_verify_results.chain.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());
  ScopedTestRoot scoped_root(certs[2].get());

  scoped_refptr<X509Certificate> unrelated_certificate =
      ImportCertFromFile(certs_dir, "duplicate_cn_1.pem");
  scoped_refptr<X509Certificate> unrelated_certificate2 =
      ImportCertFromFile(certs_dir, "aia-cert.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), unrelated_certificate);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), unrelated_certificate2);

  // Interject unrelated certificates into the list of intermediates.
  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(unrelated_certificate->os_cert_handle());
  intermediates.push_back(certs[1]->os_cert_handle());
  intermediates.push_back(unrelated_certificate2->os_cert_handle());
  intermediates.push_back(certs[2]->os_cert_handle());

  scoped_refptr<X509Certificate> google_full_chain =
      X509Certificate::CreateFromHandle(certs[0]->os_cert_handle(),
                                        intermediates);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), google_full_chain);
  ASSERT_EQ(4U, google_full_chain->GetIntermediateCertificates().size());

  CertVerifyResult verify_result;
  EXPECT_EQ(static_cast<X509Certificate*>(NULL), verify_result.verified_cert);
  int error = Verify(google_full_chain.get(),
                     "127.0.0.1",
                     0,
                     NULL,
                     empty_cert_list_,
                     &verify_result);
  EXPECT_EQ(OK, error);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), verify_result.verified_cert);

  EXPECT_NE(google_full_chain, verify_result.verified_cert);
  EXPECT_TRUE(X509Certificate::IsSameOSCert(
      google_full_chain->os_cert_handle(),
      verify_result.verified_cert->os_cert_handle()));
  const X509Certificate::OSCertHandles& return_intermediates =
      verify_result.verified_cert->GetIntermediateCertificates();
  ASSERT_EQ(2U, return_intermediates.size());
  EXPECT_TRUE(X509Certificate::IsSameOSCert(return_intermediates[0],
                                            certs[1]->os_cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(return_intermediates[1],
                                            certs[2]->os_cert_handle()));
}

TEST_F(CertVerifyProcTest, AdditionalTrustAnchors) {
  if (!SupportsAdditionalTrustAnchors()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  // |ca_cert| is the issuer of |cert|.
  CertificateList ca_cert_list = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "root_ca_cert.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, ca_cert_list.size());
  scoped_refptr<X509Certificate> ca_cert(ca_cert_list[0]);

  CertificateList cert_list = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "ok_cert.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, cert_list.size());
  scoped_refptr<X509Certificate> cert(cert_list[0]);

  // Verification of |cert| fails when |ca_cert| is not in the trust anchors
  // list.
  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(
      cert.get(), "127.0.0.1", flags, NULL, empty_cert_list_, &verify_result);
  EXPECT_EQ(ERR_CERT_AUTHORITY_INVALID, error);
  EXPECT_EQ(CERT_STATUS_AUTHORITY_INVALID, verify_result.cert_status);
  EXPECT_FALSE(verify_result.is_issued_by_additional_trust_anchor);

  // Now add the |ca_cert| to the |trust_anchors|, and verification should pass.
  CertificateList trust_anchors;
  trust_anchors.push_back(ca_cert);
  error = Verify(
      cert.get(), "127.0.0.1", flags, NULL, trust_anchors, &verify_result);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(0U, verify_result.cert_status);
  EXPECT_TRUE(verify_result.is_issued_by_additional_trust_anchor);

  // Clearing the |trust_anchors| makes verification fail again (the cache
  // should be skipped).
  error = Verify(
      cert.get(), "127.0.0.1", flags, NULL, empty_cert_list_, &verify_result);
  EXPECT_EQ(ERR_CERT_AUTHORITY_INVALID, error);
  EXPECT_EQ(CERT_STATUS_AUTHORITY_INVALID, verify_result.cert_status);
  EXPECT_FALSE(verify_result.is_issued_by_additional_trust_anchor);
}

// Tests that certificates issued by user-supplied roots are not flagged as
// issued by a known root. This should pass whether or not the platform supports
// detecting known roots.
TEST_F(CertVerifyProcTest, IsIssuedByKnownRootIgnoresTestRoots) {
  // Load root_ca_cert.pem into the test root store.
  TestRootCerts* root_certs = TestRootCerts::GetInstance();
  root_certs->AddFromFile(
      GetTestCertsDirectory().AppendASCII("root_ca_cert.pem"));

  CertificateList cert_list = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "ok_cert.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, cert_list.size());
  scoped_refptr<X509Certificate> cert(cert_list[0]);

  // Verification should pass.
  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(
      cert.get(), "127.0.0.1", flags, NULL, empty_cert_list_, &verify_result);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(0U, verify_result.cert_status);
  // But should not be marked as a known root.
  EXPECT_FALSE(verify_result.is_issued_by_known_root);

  root_certs->Clear();
  EXPECT_TRUE(root_certs->IsEmpty());
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
// Tests that, on OS X, issues with a cross-certified Baltimore CyberTrust
// Root can be successfully worked around once Apple completes removing the
// older GTE CyberTrust Root from its trusted root store.
//
// The issue is caused by servers supplying the cross-certified intermediate
// (necessary for certain mobile platforms), which OS X does not recognize
// as already existing within its trust store.
TEST_F(CertVerifyProcTest, CybertrustGTERoot) {
  CertificateList certs = CreateCertificateListFromFile(
      GetTestCertsDirectory(),
      "cybertrust_omniroot_chain.pem",
      X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_EQ(2U, certs.size());

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(certs[1]->os_cert_handle());

  scoped_refptr<X509Certificate> cybertrust_basic =
      X509Certificate::CreateFromHandle(certs[0]->os_cert_handle(),
                                        intermediates);
  ASSERT_TRUE(cybertrust_basic.get());

  scoped_refptr<X509Certificate> baltimore_root =
      ImportCertFromFile(GetTestCertsDirectory(),
                         "cybertrust_baltimore_root.pem");
  ASSERT_TRUE(baltimore_root.get());

  ScopedTestRoot scoped_root(baltimore_root.get());

  // Ensure that ONLY the Baltimore CyberTrust Root is trusted. This
  // simulates Keychain removing support for the GTE CyberTrust Root.
  TestRootCerts::GetInstance()->SetAllowSystemTrust(false);
  base::ScopedClosureRunner reset_system_trust(
      base::Bind(&TestRootCerts::SetAllowSystemTrust,
                 base::Unretained(TestRootCerts::GetInstance()),
                 true));

  // First, make sure a simple certificate chain from
  //   EE -> Public SureServer SV -> Baltimore CyberTrust
  // works. Only the first two certificates are included in the chain.
  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(cybertrust_basic.get(),
                     "cacert.omniroot.com",
                     flags,
                     NULL,
                     empty_cert_list_,
                     &verify_result);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(0U, verify_result.cert_status);

  // Attempt to verify with the first known cross-certified intermediate
  // provided.
  scoped_refptr<X509Certificate> baltimore_intermediate_1 =
      ImportCertFromFile(GetTestCertsDirectory(),
                         "cybertrust_baltimore_cross_certified_1.pem");
  ASSERT_TRUE(baltimore_intermediate_1.get());

  X509Certificate::OSCertHandles intermediate_chain_1 =
      cybertrust_basic->GetIntermediateCertificates();
  intermediate_chain_1.push_back(baltimore_intermediate_1->os_cert_handle());

  scoped_refptr<X509Certificate> baltimore_chain_1 =
      X509Certificate::CreateFromHandle(cybertrust_basic->os_cert_handle(),
                                        intermediate_chain_1);
  error = Verify(baltimore_chain_1.get(),
                 "cacert.omniroot.com",
                 flags,
                 NULL,
                 empty_cert_list_,
                 &verify_result);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(0U, verify_result.cert_status);

  // Attempt to verify with the second known cross-certified intermediate
  // provided.
  scoped_refptr<X509Certificate> baltimore_intermediate_2 =
      ImportCertFromFile(GetTestCertsDirectory(),
                         "cybertrust_baltimore_cross_certified_2.pem");
  ASSERT_TRUE(baltimore_intermediate_2.get());

  X509Certificate::OSCertHandles intermediate_chain_2 =
      cybertrust_basic->GetIntermediateCertificates();
  intermediate_chain_2.push_back(baltimore_intermediate_2->os_cert_handle());

  scoped_refptr<X509Certificate> baltimore_chain_2 =
      X509Certificate::CreateFromHandle(cybertrust_basic->os_cert_handle(),
                                        intermediate_chain_2);
  error = Verify(baltimore_chain_2.get(),
                 "cacert.omniroot.com",
                 flags,
                 NULL,
                 empty_cert_list_,
                 &verify_result);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(0U, verify_result.cert_status);

  // Attempt to verify when both a cross-certified intermediate AND
  // the legacy GTE root are provided.
  scoped_refptr<X509Certificate> cybertrust_root =
      ImportCertFromFile(GetTestCertsDirectory(),
                         "cybertrust_gte_root.pem");
  ASSERT_TRUE(cybertrust_root.get());

  intermediate_chain_2.push_back(cybertrust_root->os_cert_handle());
  scoped_refptr<X509Certificate> baltimore_chain_with_root =
      X509Certificate::CreateFromHandle(cybertrust_basic->os_cert_handle(),
                                        intermediate_chain_2);
  error = Verify(baltimore_chain_with_root.get(),
                 "cacert.omniroot.com",
                 flags,
                 NULL,
                 empty_cert_list_,
                 &verify_result);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(0U, verify_result.cert_status);

  TestRootCerts::GetInstance()->Clear();
  EXPECT_TRUE(TestRootCerts::GetInstance()->IsEmpty());
}
#endif

#if defined(USE_NSS) || defined(OS_IOS) || defined(OS_WIN) || defined(OS_MACOSX)
static const uint8 kCRLSetLeafSPKIBlocked[] = {
  0x8e, 0x00, 0x7b, 0x22, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x22, 0x3a,
  0x30, 0x2c, 0x22, 0x43, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x54, 0x79, 0x70,
  0x65, 0x22, 0x3a, 0x22, 0x43, 0x52, 0x4c, 0x53, 0x65, 0x74, 0x22, 0x2c, 0x22,
  0x53, 0x65, 0x71, 0x75, 0x65, 0x6e, 0x63, 0x65, 0x22, 0x3a, 0x30, 0x2c, 0x22,
  0x44, 0x65, 0x6c, 0x74, 0x61, 0x46, 0x72, 0x6f, 0x6d, 0x22, 0x3a, 0x30, 0x2c,
  0x22, 0x4e, 0x75, 0x6d, 0x50, 0x61, 0x72, 0x65, 0x6e, 0x74, 0x73, 0x22, 0x3a,
  0x30, 0x2c, 0x22, 0x42, 0x6c, 0x6f, 0x63, 0x6b, 0x65, 0x64, 0x53, 0x50, 0x4b,
  0x49, 0x73, 0x22, 0x3a, 0x5b, 0x22, 0x43, 0x38, 0x4d, 0x4a, 0x46, 0x55, 0x55,
  0x5a, 0x38, 0x43, 0x79, 0x54, 0x2b, 0x4e, 0x57, 0x64, 0x68, 0x69, 0x7a, 0x51,
  0x68, 0x54, 0x49, 0x65, 0x46, 0x49, 0x37, 0x76, 0x41, 0x77, 0x7a, 0x64, 0x54,
  0x79, 0x52, 0x59, 0x45, 0x6e, 0x78, 0x6c, 0x33, 0x62, 0x67, 0x3d, 0x22, 0x5d,
  0x7d,
};

static const uint8 kCRLSetLeafSerialBlocked[] = {
  0x60, 0x00, 0x7b, 0x22, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x22, 0x3a,
  0x30, 0x2c, 0x22, 0x43, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x54, 0x79, 0x70,
  0x65, 0x22, 0x3a, 0x22, 0x43, 0x52, 0x4c, 0x53, 0x65, 0x74, 0x22, 0x2c, 0x22,
  0x53, 0x65, 0x71, 0x75, 0x65, 0x6e, 0x63, 0x65, 0x22, 0x3a, 0x30, 0x2c, 0x22,
  0x44, 0x65, 0x6c, 0x74, 0x61, 0x46, 0x72, 0x6f, 0x6d, 0x22, 0x3a, 0x30, 0x2c,
  0x22, 0x4e, 0x75, 0x6d, 0x50, 0x61, 0x72, 0x65, 0x6e, 0x74, 0x73, 0x22, 0x3a,
  0x31, 0x2c, 0x22, 0x42, 0x6c, 0x6f, 0x63, 0x6b, 0x65, 0x64, 0x53, 0x50, 0x4b,
  0x49, 0x73, 0x22, 0x3a, 0x5b, 0x5d, 0x7d, 0x0f, 0x87, 0xe4, 0xc7, 0x75, 0xea,
  0x46, 0x7e, 0xf3, 0xfd, 0x82, 0xb7, 0x46, 0x7b, 0x10, 0xda, 0xc5, 0xbf, 0xd8,
  0xd1, 0x29, 0xb2, 0xc6, 0xac, 0x7f, 0x51, 0x42, 0x15, 0x28, 0x51, 0x06, 0x7f,
  0x01, 0x00, 0x00, 0x00,  // number of serials
  0x01, 0xed,  // serial 0xed
};

static const uint8 kCRLSetQUICSerialBlocked[] = {
  0x60, 0x00, 0x7b, 0x22, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x22, 0x3a,
  0x30, 0x2c, 0x22, 0x43, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x54, 0x79, 0x70,
  0x65, 0x22, 0x3a, 0x22, 0x43, 0x52, 0x4c, 0x53, 0x65, 0x74, 0x22, 0x2c, 0x22,
  0x53, 0x65, 0x71, 0x75, 0x65, 0x6e, 0x63, 0x65, 0x22, 0x3a, 0x30, 0x2c, 0x22,
  0x44, 0x65, 0x6c, 0x74, 0x61, 0x46, 0x72, 0x6f, 0x6d, 0x22, 0x3a, 0x30, 0x2c,
  0x22, 0x4e, 0x75, 0x6d, 0x50, 0x61, 0x72, 0x65, 0x6e, 0x74, 0x73, 0x22, 0x3a,
  0x31, 0x2c, 0x22, 0x42, 0x6c, 0x6f, 0x63, 0x6b, 0x65, 0x64, 0x53, 0x50, 0x4b,
  0x49, 0x73, 0x22, 0x3a, 0x5b, 0x5d, 0x7d,
  // Issuer SPKI SHA-256 hash:
  0xe4, 0x3a, 0xa3, 0xdb, 0x98, 0x31, 0x61, 0x05, 0xdd, 0x57, 0x6d, 0xc6, 0x2f,
  0x71, 0x26, 0xba, 0xdd, 0xf4, 0x98, 0x3e, 0x62, 0x22, 0xf8, 0xf9, 0xe4, 0x18,
  0x62, 0x77, 0x79, 0xdb, 0x9b, 0x31,
  0x01, 0x00, 0x00, 0x00,  // number of serials
  0x01, 0x03,  // serial 3
};

// Test that CRLSets are effective in making a certificate appear to be
// revoked.
TEST_F(CertVerifyProcTest, CRLSet) {
  CertificateList ca_cert_list =
      CreateCertificateListFromFile(GetTestCertsDirectory(),
                                    "root_ca_cert.pem",
                                    X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, ca_cert_list.size());
  ScopedTestRoot test_root(ca_cert_list[0]);

  CertificateList cert_list = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "ok_cert.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, cert_list.size());
  scoped_refptr<X509Certificate> cert(cert_list[0]);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(
      cert.get(), "127.0.0.1", flags, NULL, empty_cert_list_, &verify_result);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(0U, verify_result.cert_status);

  // First test blocking by SPKI.
  base::StringPiece crl_set_bytes(
      reinterpret_cast<const char*>(kCRLSetLeafSPKIBlocked),
      sizeof(kCRLSetLeafSPKIBlocked));
  scoped_refptr<CRLSet> crl_set;
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  error = Verify(cert.get(),
                 "127.0.0.1",
                 flags,
                 crl_set.get(),
                 empty_cert_list_,
                 &verify_result);
  EXPECT_EQ(ERR_CERT_REVOKED, error);

  // Second, test revocation by serial number of a cert directly under the
  // root.
  crl_set_bytes =
      base::StringPiece(reinterpret_cast<const char*>(kCRLSetLeafSerialBlocked),
                        sizeof(kCRLSetLeafSerialBlocked));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  error = Verify(cert.get(),
                 "127.0.0.1",
                 flags,
                 crl_set.get(),
                 empty_cert_list_,
                 &verify_result);
  EXPECT_EQ(ERR_CERT_REVOKED, error);
}

TEST_F(CertVerifyProcTest, CRLSetLeafSerial) {
  CertificateList ca_cert_list =
      CreateCertificateListFromFile(GetTestCertsDirectory(),
                                    "quic_root.crt",
                                    X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, ca_cert_list.size());
  ScopedTestRoot test_root(ca_cert_list[0]);

  CertificateList intermediate_cert_list =
      CreateCertificateListFromFile(GetTestCertsDirectory(),
                                    "quic_intermediate.crt",
                                    X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, intermediate_cert_list.size());
  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(intermediate_cert_list[0]->os_cert_handle());

  CertificateList cert_list = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "quic_test.example.com.crt",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, cert_list.size());

  scoped_refptr<X509Certificate> leaf =
      X509Certificate::CreateFromHandle(cert_list[0]->os_cert_handle(),
                                        intermediates);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(leaf.get(),
                     "test.example.com",
                     flags,
                     NULL,
                     empty_cert_list_,
                     &verify_result);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(0U, verify_result.cert_status);

  // Test revocation by serial number of a certificate not under the root.
  scoped_refptr<CRLSet> crl_set;
  base::StringPiece crl_set_bytes =
      base::StringPiece(reinterpret_cast<const char*>(kCRLSetQUICSerialBlocked),
                        sizeof(kCRLSetQUICSerialBlocked));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  error = Verify(leaf.get(),
                 "test.example.com",
                 flags,
                 crl_set.get(),
                 empty_cert_list_,
                 &verify_result);
  EXPECT_EQ(ERR_CERT_REVOKED, error);
}
#endif

struct WeakDigestTestData {
  const char* root_cert_filename;
  const char* intermediate_cert_filename;
  const char* ee_cert_filename;
  bool expected_has_md5;
  bool expected_has_md4;
  bool expected_has_md2;
};

// GTest 'magic' pretty-printer, so that if/when a test fails, it knows how
// to output the parameter that was passed. Without this, it will simply
// attempt to print out the first twenty bytes of the object, which depending
// on platform and alignment, may result in an invalid read.
void PrintTo(const WeakDigestTestData& data, std::ostream* os) {
  *os << "root: "
      << (data.root_cert_filename ? data.root_cert_filename : "none")
      << "; intermediate: " << data.intermediate_cert_filename
      << "; end-entity: " << data.ee_cert_filename;
}

class CertVerifyProcWeakDigestTest
    : public CertVerifyProcTest,
      public testing::WithParamInterface<WeakDigestTestData> {
 public:
  CertVerifyProcWeakDigestTest() {}
  virtual ~CertVerifyProcWeakDigestTest() {}
};

TEST_P(CertVerifyProcWeakDigestTest, Verify) {
  WeakDigestTestData data = GetParam();
  base::FilePath certs_dir = GetTestCertsDirectory();

  ScopedTestRoot test_root;
  if (data.root_cert_filename) {
     scoped_refptr<X509Certificate> root_cert =
         ImportCertFromFile(certs_dir, data.root_cert_filename);
     ASSERT_NE(static_cast<X509Certificate*>(NULL), root_cert);
     test_root.Reset(root_cert.get());
  }

  scoped_refptr<X509Certificate> intermediate_cert =
      ImportCertFromFile(certs_dir, data.intermediate_cert_filename);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), intermediate_cert);
  scoped_refptr<X509Certificate> ee_cert =
      ImportCertFromFile(certs_dir, data.ee_cert_filename);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), ee_cert);

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(intermediate_cert->os_cert_handle());

  scoped_refptr<X509Certificate> ee_chain =
      X509Certificate::CreateFromHandle(ee_cert->os_cert_handle(),
                                        intermediates);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), ee_chain);

  int flags = 0;
  CertVerifyResult verify_result;
  int rv = Verify(ee_chain.get(),
                  "127.0.0.1",
                  flags,
                  NULL,
                  empty_cert_list_,
                  &verify_result);
  EXPECT_EQ(data.expected_has_md5, verify_result.has_md5);
  EXPECT_EQ(data.expected_has_md4, verify_result.has_md4);
  EXPECT_EQ(data.expected_has_md2, verify_result.has_md2);
  EXPECT_FALSE(verify_result.is_issued_by_additional_trust_anchor);

  // Ensure that MD4 and MD2 are tagged as invalid.
  if (data.expected_has_md4 || data.expected_has_md2) {
    EXPECT_EQ(CERT_STATUS_INVALID,
              verify_result.cert_status & CERT_STATUS_INVALID);
  }

  // Ensure that MD5 is flagged as weak.
  if (data.expected_has_md5) {
    EXPECT_EQ(
        CERT_STATUS_WEAK_SIGNATURE_ALGORITHM,
        verify_result.cert_status & CERT_STATUS_WEAK_SIGNATURE_ALGORITHM);
  }

  // If a root cert is present, then check that the chain was rejected if any
  // weak algorithms are present. This is only checked when a root cert is
  // present because the error reported for incomplete chains with weak
  // algorithms depends on which implementation was used to validate (NSS,
  // OpenSSL, CryptoAPI, Security.framework) and upon which weak algorithm
  // present (MD2, MD4, MD5).
  if (data.root_cert_filename) {
    if (data.expected_has_md4 || data.expected_has_md2) {
      EXPECT_EQ(ERR_CERT_INVALID, rv);
    } else if (data.expected_has_md5) {
      EXPECT_EQ(ERR_CERT_WEAK_SIGNATURE_ALGORITHM, rv);
    } else {
      EXPECT_EQ(OK, rv);
    }
  }
}

// Unlike TEST/TEST_F, which are macros that expand to further macros,
// INSTANTIATE_TEST_CASE_P is a macro that expands directly to code that
// stringizes the arguments. As a result, macros passed as parameters (such as
// prefix or test_case_name) will not be expanded by the preprocessor. To work
// around this, indirect the macro for INSTANTIATE_TEST_CASE_P, so that the
// pre-processor will expand macros such as MAYBE_test_name before
// instantiating the test.
#define WRAPPED_INSTANTIATE_TEST_CASE_P(prefix, test_case_name, generator) \
    INSTANTIATE_TEST_CASE_P(prefix, test_case_name, generator)

// The signature algorithm of the root CA should not matter.
const WeakDigestTestData kVerifyRootCATestData[] = {
  { "weak_digest_md5_root.pem", "weak_digest_sha1_intermediate.pem",
    "weak_digest_sha1_ee.pem", false, false, false },
#if defined(USE_OPENSSL_CERTS) || defined(OS_WIN)
  // MD4 is not supported by OS X / NSS
  { "weak_digest_md4_root.pem", "weak_digest_sha1_intermediate.pem",
    "weak_digest_sha1_ee.pem", false, false, false },
#endif
  { "weak_digest_md2_root.pem", "weak_digest_sha1_intermediate.pem",
    "weak_digest_sha1_ee.pem", false, false, false },
};
INSTANTIATE_TEST_CASE_P(VerifyRoot, CertVerifyProcWeakDigestTest,
                        testing::ValuesIn(kVerifyRootCATestData));

// The signature algorithm of intermediates should be properly detected.
const WeakDigestTestData kVerifyIntermediateCATestData[] = {
  { "weak_digest_sha1_root.pem", "weak_digest_md5_intermediate.pem",
    "weak_digest_sha1_ee.pem", true, false, false },
#if defined(USE_OPENSSL_CERTS) || defined(OS_WIN)
  // MD4 is not supported by OS X / NSS
  { "weak_digest_sha1_root.pem", "weak_digest_md4_intermediate.pem",
    "weak_digest_sha1_ee.pem", false, true, false },
#endif
  { "weak_digest_sha1_root.pem", "weak_digest_md2_intermediate.pem",
    "weak_digest_sha1_ee.pem", false, false, true },
};
// Disabled on NSS - MD4 is not supported, and MD2 and MD5 are disabled.
#if defined(USE_NSS) || defined(OS_IOS)
#define MAYBE_VerifyIntermediate DISABLED_VerifyIntermediate
#else
#define MAYBE_VerifyIntermediate VerifyIntermediate
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_VerifyIntermediate,
    CertVerifyProcWeakDigestTest,
    testing::ValuesIn(kVerifyIntermediateCATestData));

// The signature algorithm of end-entity should be properly detected.
const WeakDigestTestData kVerifyEndEntityTestData[] = {
  { "weak_digest_sha1_root.pem", "weak_digest_sha1_intermediate.pem",
    "weak_digest_md5_ee.pem", true, false, false },
#if defined(USE_OPENSSL_CERTS) || defined(OS_WIN)
  // MD4 is not supported by OS X / NSS
  { "weak_digest_sha1_root.pem", "weak_digest_sha1_intermediate.pem",
    "weak_digest_md4_ee.pem", false, true, false },
#endif
  { "weak_digest_sha1_root.pem", "weak_digest_sha1_intermediate.pem",
    "weak_digest_md2_ee.pem", false, false, true },
};
// Disabled on NSS - NSS caches chains/signatures in such a way that cannot
// be cleared until NSS is cleanly shutdown, which is not presently supported
// in Chromium.
#if defined(USE_NSS) || defined(OS_IOS)
#define MAYBE_VerifyEndEntity DISABLED_VerifyEndEntity
#else
#define MAYBE_VerifyEndEntity VerifyEndEntity
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(MAYBE_VerifyEndEntity,
                                CertVerifyProcWeakDigestTest,
                                testing::ValuesIn(kVerifyEndEntityTestData));

// Incomplete chains should still report the status of the intermediate.
const WeakDigestTestData kVerifyIncompleteIntermediateTestData[] = {
  { NULL, "weak_digest_md5_intermediate.pem", "weak_digest_sha1_ee.pem",
    true, false, false },
#if defined(USE_OPENSSL_CERTS) || defined(OS_WIN)
  // MD4 is not supported by OS X / NSS
  { NULL, "weak_digest_md4_intermediate.pem", "weak_digest_sha1_ee.pem",
    false, true, false },
#endif
  { NULL, "weak_digest_md2_intermediate.pem", "weak_digest_sha1_ee.pem",
    false, false, true },
};
// Disabled on NSS - libpkix does not return constructed chains on error,
// preventing us from detecting/inspecting the verified chain.
#if defined(USE_NSS) || defined(OS_IOS)
#define MAYBE_VerifyIncompleteIntermediate \
    DISABLED_VerifyIncompleteIntermediate
#else
#define MAYBE_VerifyIncompleteIntermediate VerifyIncompleteIntermediate
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_VerifyIncompleteIntermediate,
    CertVerifyProcWeakDigestTest,
    testing::ValuesIn(kVerifyIncompleteIntermediateTestData));

// Incomplete chains should still report the status of the end-entity.
const WeakDigestTestData kVerifyIncompleteEETestData[] = {
  { NULL, "weak_digest_sha1_intermediate.pem", "weak_digest_md5_ee.pem",
    true, false, false },
#if defined(USE_OPENSSL_CERTS) || defined(OS_WIN)
  // MD4 is not supported by OS X / NSS
  { NULL, "weak_digest_sha1_intermediate.pem", "weak_digest_md4_ee.pem",
    false, true, false },
#endif
  { NULL, "weak_digest_sha1_intermediate.pem", "weak_digest_md2_ee.pem",
    false, false, true },
};
// Disabled on NSS - libpkix does not return constructed chains on error,
// preventing us from detecting/inspecting the verified chain.
#if defined(USE_NSS) || defined(OS_IOS)
#define MAYBE_VerifyIncompleteEndEntity DISABLED_VerifyIncompleteEndEntity
#else
#define MAYBE_VerifyIncompleteEndEntity VerifyIncompleteEndEntity
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_VerifyIncompleteEndEntity,
    CertVerifyProcWeakDigestTest,
    testing::ValuesIn(kVerifyIncompleteEETestData));

// Differing algorithms between the intermediate and the EE should still be
// reported.
const WeakDigestTestData kVerifyMixedTestData[] = {
  { "weak_digest_sha1_root.pem", "weak_digest_md5_intermediate.pem",
    "weak_digest_md2_ee.pem", true, false, true },
  { "weak_digest_sha1_root.pem", "weak_digest_md2_intermediate.pem",
    "weak_digest_md5_ee.pem", true, false, true },
#if defined(USE_OPENSSL_CERTS) || defined(OS_WIN)
  // MD4 is not supported by OS X / NSS
  { "weak_digest_sha1_root.pem", "weak_digest_md4_intermediate.pem",
    "weak_digest_md2_ee.pem", false, true, true },
#endif
};
// NSS does not support MD4 and does not enable MD2 by default, making all
// permutations invalid.
#if defined(USE_NSS) || defined(OS_IOS)
#define MAYBE_VerifyMixed DISABLED_VerifyMixed
#else
#define MAYBE_VerifyMixed VerifyMixed
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_VerifyMixed,
    CertVerifyProcWeakDigestTest,
    testing::ValuesIn(kVerifyMixedTestData));

// For the list of valid hostnames, see
// net/cert/data/ssl/certificates/subjectAltName_sanity_check.pem
static const struct CertVerifyProcNameData {
  const char* hostname;
  bool valid;  // Whether or not |hostname| matches a subjectAltName.
} kVerifyNameData[] = {
  { "127.0.0.1", false },  // Don't match the common name
  { "127.0.0.2", true },  // Matches the iPAddress SAN (IPv4)
  { "FE80:0:0:0:0:0:0:1", true },  // Matches the iPAddress SAN (IPv6)
  { "[FE80:0:0:0:0:0:0:1]", false },  // Should not match the iPAddress SAN
  { "FE80::1", true },  // Compressed form matches the iPAddress SAN (IPv6)
  { "::127.0.0.2", false },  // IPv6 mapped form should NOT match iPAddress SAN
  { "test.example", true },  // Matches the dNSName SAN
  { "test.example.", true },  // Matches the dNSName SAN (trailing . ignored)
  { "www.test.example", false },  // Should not match the dNSName SAN
  { "test..example", false },  // Should not match the dNSName SAN
  { "test.example..", false },  // Should not match the dNSName SAN
  { ".test.example.", false },  // Should not match the dNSName SAN
  { ".test.example", false },  // Should not match the dNSName SAN
};

// GTest 'magic' pretty-printer, so that if/when a test fails, it knows how
// to output the parameter that was passed. Without this, it will simply
// attempt to print out the first twenty bytes of the object, which depending
// on platform and alignment, may result in an invalid read.
void PrintTo(const CertVerifyProcNameData& data, std::ostream* os) {
  *os << "Hostname: " << data.hostname << "; valid=" << data.valid;
}

class CertVerifyProcNameTest
    : public CertVerifyProcTest,
      public testing::WithParamInterface<CertVerifyProcNameData> {
 public:
  CertVerifyProcNameTest() {}
  virtual ~CertVerifyProcNameTest() {}
};

TEST_P(CertVerifyProcNameTest, VerifyCertName) {
  CertVerifyProcNameData data = GetParam();

  CertificateList cert_list = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "subjectAltName_sanity_check.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, cert_list.size());
  scoped_refptr<X509Certificate> cert(cert_list[0]);

  ScopedTestRoot scoped_root(cert.get());

  CertVerifyResult verify_result;
  int error = Verify(cert.get(), data.hostname, 0, NULL, empty_cert_list_,
                     &verify_result);
  if (data.valid) {
    EXPECT_EQ(OK, error);
    EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_COMMON_NAME_INVALID);
  } else {
    EXPECT_EQ(ERR_CERT_COMMON_NAME_INVALID, error);
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_COMMON_NAME_INVALID);
  }
}

WRAPPED_INSTANTIATE_TEST_CASE_P(
    VerifyName,
    CertVerifyProcNameTest,
    testing::ValuesIn(kVerifyNameData));

}  // namespace net
