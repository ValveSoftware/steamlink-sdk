// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/multi_threaded_cert_verifier.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_data_directory.h"
#include "net/cert/cert_trust_anchor_provider.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::ReturnRef;

namespace net {

namespace {

void FailTest(int /* result */) {
  FAIL();
}

class MockCertVerifyProc : public CertVerifyProc {
 public:
  MockCertVerifyProc() {}

 private:
  virtual ~MockCertVerifyProc() {}

  // CertVerifyProc implementation
  virtual bool SupportsAdditionalTrustAnchors() const OVERRIDE {
    return false;
  }

  virtual int VerifyInternal(X509Certificate* cert,
                             const std::string& hostname,
                             int flags,
                             CRLSet* crl_set,
                             const CertificateList& additional_trust_anchors,
                             CertVerifyResult* verify_result) OVERRIDE {
    verify_result->Reset();
    verify_result->verified_cert = cert;
    verify_result->cert_status = CERT_STATUS_COMMON_NAME_INVALID;
    return ERR_CERT_COMMON_NAME_INVALID;
  }
};

class MockCertTrustAnchorProvider : public CertTrustAnchorProvider {
 public:
  MockCertTrustAnchorProvider() {}
  virtual ~MockCertTrustAnchorProvider() {}

  MOCK_METHOD0(GetAdditionalTrustAnchors, const CertificateList&());
};

}  // namespace

class MultiThreadedCertVerifierTest : public ::testing::Test {
 public:
  MultiThreadedCertVerifierTest() : verifier_(new MockCertVerifyProc()) {}
  virtual ~MultiThreadedCertVerifierTest() {}

 protected:
  MultiThreadedCertVerifier verifier_;
};

TEST_F(MultiThreadedCertVerifierTest, CacheHit) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), test_cert);

  int error;
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  CertVerifier::RequestHandle request_handle;

  error = verifier_.Verify(test_cert.get(),
                           "www.example.com",
                           0,
                           NULL,
                           &verify_result,
                           callback.callback(),
                           &request_handle,
                           BoundNetLog());
  ASSERT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle);
  error = callback.WaitForResult();
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_EQ(1u, verifier_.requests());
  ASSERT_EQ(0u, verifier_.cache_hits());
  ASSERT_EQ(0u, verifier_.inflight_joins());
  ASSERT_EQ(1u, verifier_.GetCacheSize());

  error = verifier_.Verify(test_cert.get(),
                           "www.example.com",
                           0,
                           NULL,
                           &verify_result,
                           callback.callback(),
                           &request_handle,
                           BoundNetLog());
  // Synchronous completion.
  ASSERT_NE(ERR_IO_PENDING, error);
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_TRUE(request_handle == NULL);
  ASSERT_EQ(2u, verifier_.requests());
  ASSERT_EQ(1u, verifier_.cache_hits());
  ASSERT_EQ(0u, verifier_.inflight_joins());
  ASSERT_EQ(1u, verifier_.GetCacheSize());
}

// Tests the same server certificate with different intermediate CA
// certificates.  These should be treated as different certificate chains even
// though the two X509Certificate objects contain the same server certificate.
TEST_F(MultiThreadedCertVerifierTest, DifferentCACerts) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "salesforce_com_test.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), server_cert);

  scoped_refptr<X509Certificate> intermediate_cert1 =
      ImportCertFromFile(certs_dir, "verisign_intermediate_ca_2011.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), intermediate_cert1);

  scoped_refptr<X509Certificate> intermediate_cert2 =
      ImportCertFromFile(certs_dir, "verisign_intermediate_ca_2016.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), intermediate_cert2);

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(intermediate_cert1->os_cert_handle());
  scoped_refptr<X509Certificate> cert_chain1 =
      X509Certificate::CreateFromHandle(server_cert->os_cert_handle(),
                                        intermediates);

  intermediates.clear();
  intermediates.push_back(intermediate_cert2->os_cert_handle());
  scoped_refptr<X509Certificate> cert_chain2 =
      X509Certificate::CreateFromHandle(server_cert->os_cert_handle(),
                                        intermediates);

  int error;
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  CertVerifier::RequestHandle request_handle;

  error = verifier_.Verify(cert_chain1.get(),
                           "www.example.com",
                           0,
                           NULL,
                           &verify_result,
                           callback.callback(),
                           &request_handle,
                           BoundNetLog());
  ASSERT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle);
  error = callback.WaitForResult();
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_EQ(1u, verifier_.requests());
  ASSERT_EQ(0u, verifier_.cache_hits());
  ASSERT_EQ(0u, verifier_.inflight_joins());
  ASSERT_EQ(1u, verifier_.GetCacheSize());

  error = verifier_.Verify(cert_chain2.get(),
                           "www.example.com",
                           0,
                           NULL,
                           &verify_result,
                           callback.callback(),
                           &request_handle,
                           BoundNetLog());
  ASSERT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle);
  error = callback.WaitForResult();
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_EQ(2u, verifier_.requests());
  ASSERT_EQ(0u, verifier_.cache_hits());
  ASSERT_EQ(0u, verifier_.inflight_joins());
  ASSERT_EQ(2u, verifier_.GetCacheSize());
}

// Tests an inflight join.
TEST_F(MultiThreadedCertVerifierTest, InflightJoin) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), test_cert);

  int error;
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  CertVerifier::RequestHandle request_handle;
  CertVerifyResult verify_result2;
  TestCompletionCallback callback2;
  CertVerifier::RequestHandle request_handle2;

  error = verifier_.Verify(test_cert.get(),
                           "www.example.com",
                           0,
                           NULL,
                           &verify_result,
                           callback.callback(),
                           &request_handle,
                           BoundNetLog());
  ASSERT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle);
  error = verifier_.Verify(test_cert.get(),
                           "www.example.com",
                           0,
                           NULL,
                           &verify_result2,
                           callback2.callback(),
                           &request_handle2,
                           BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle2 != NULL);
  error = callback.WaitForResult();
  EXPECT_TRUE(IsCertificateError(error));
  error = callback2.WaitForResult();
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_EQ(2u, verifier_.requests());
  ASSERT_EQ(0u, verifier_.cache_hits());
  ASSERT_EQ(1u, verifier_.inflight_joins());
}

// Tests that the callback of a canceled request is never made.
TEST_F(MultiThreadedCertVerifierTest, CancelRequest) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), test_cert);

  int error;
  CertVerifyResult verify_result;
  CertVerifier::RequestHandle request_handle;

  error = verifier_.Verify(test_cert.get(),
                           "www.example.com",
                           0,
                           NULL,
                           &verify_result,
                           base::Bind(&FailTest),
                           &request_handle,
                           BoundNetLog());
  ASSERT_EQ(ERR_IO_PENDING, error);
  ASSERT_TRUE(request_handle != NULL);
  verifier_.CancelRequest(request_handle);

  // Issue a few more requests to the worker pool and wait for their
  // completion, so that the task of the canceled request (which runs on a
  // worker thread) is likely to complete by the end of this test.
  TestCompletionCallback callback;
  for (int i = 0; i < 5; ++i) {
    error = verifier_.Verify(test_cert.get(),
                             "www2.example.com",
                             0,
                             NULL,
                             &verify_result,
                             callback.callback(),
                             &request_handle,
                             BoundNetLog());
    ASSERT_EQ(ERR_IO_PENDING, error);
    EXPECT_TRUE(request_handle);
    error = callback.WaitForResult();
    verifier_.ClearCache();
  }
}

// Tests that a canceled request is not leaked.
#if !defined(LEAK_SANITIZER)
#define MAYBE_CancelRequestThenQuit CancelRequestThenQuit
#else
// See PR303886. LeakSanitizer flags a leak here.
#define MAYBE_CancelRequestThenQuit DISABLED_CancelRequestThenQuit
#endif
TEST_F(MultiThreadedCertVerifierTest, MAYBE_CancelRequestThenQuit) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), test_cert);

  int error;
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  CertVerifier::RequestHandle request_handle;

  error = verifier_.Verify(test_cert.get(),
                           "www.example.com",
                           0,
                           NULL,
                           &verify_result,
                           callback.callback(),
                           &request_handle,
                           BoundNetLog());
  ASSERT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle);
  verifier_.CancelRequest(request_handle);
  // Destroy |verifier| by going out of scope.
}

TEST_F(MultiThreadedCertVerifierTest, RequestParamsComparators) {
  SHA1HashValue a_key;
  memset(a_key.data, 'a', sizeof(a_key.data));

  SHA1HashValue z_key;
  memset(z_key.data, 'z', sizeof(z_key.data));

  const CertificateList empty_list;
  CertificateList test_list;
  test_list.push_back(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));

  struct {
    // Keys to test
    MultiThreadedCertVerifier::RequestParams key1;
    MultiThreadedCertVerifier::RequestParams key2;

    // Expectation:
    // -1 means key1 is less than key2
    //  0 means key1 equals key2
    //  1 means key1 is greater than key2
    int expected_result;
  } tests[] = {
    {  // Test for basic equivalence.
      MultiThreadedCertVerifier::RequestParams(a_key, a_key, "www.example.test",
                                               0, test_list),
      MultiThreadedCertVerifier::RequestParams(a_key, a_key, "www.example.test",
                                               0, test_list),
      0,
    },
    {  // Test that different certificates but with the same CA and for
       // the same host are different validation keys.
      MultiThreadedCertVerifier::RequestParams(a_key, a_key, "www.example.test",
                                               0, test_list),
      MultiThreadedCertVerifier::RequestParams(z_key, a_key, "www.example.test",
                                               0, test_list),
      -1,
    },
    {  // Test that the same EE certificate for the same host, but with
       // different chains are different validation keys.
      MultiThreadedCertVerifier::RequestParams(a_key, z_key, "www.example.test",
                                               0, test_list),
      MultiThreadedCertVerifier::RequestParams(a_key, a_key, "www.example.test",
                                               0, test_list),
      1,
    },
    {  // The same certificate, with the same chain, but for different
       // hosts are different validation keys.
      MultiThreadedCertVerifier::RequestParams(a_key, a_key,
                                               "www1.example.test", 0,
                                               test_list),
      MultiThreadedCertVerifier::RequestParams(a_key, a_key,
                                               "www2.example.test", 0,
                                               test_list),
      -1,
    },
    {  // The same certificate, chain, and host, but with different flags
       // are different validation keys.
      MultiThreadedCertVerifier::RequestParams(a_key, a_key, "www.example.test",
                                               CertVerifier::VERIFY_EV_CERT,
                                               test_list),
      MultiThreadedCertVerifier::RequestParams(a_key, a_key, "www.example.test",
                                               0, test_list),
      1,
    },
    {  // Different additional_trust_anchors.
      MultiThreadedCertVerifier::RequestParams(a_key, a_key, "www.example.test",
                                               0, empty_list),
      MultiThreadedCertVerifier::RequestParams(a_key, a_key, "www.example.test",
                                               0, test_list),
      -1,
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]", i));

    const MultiThreadedCertVerifier::RequestParams& key1 = tests[i].key1;
    const MultiThreadedCertVerifier::RequestParams& key2 = tests[i].key2;

    switch (tests[i].expected_result) {
      case -1:
        EXPECT_TRUE(key1 < key2);
        EXPECT_FALSE(key2 < key1);
        break;
      case 0:
        EXPECT_FALSE(key1 < key2);
        EXPECT_FALSE(key2 < key1);
        break;
      case 1:
        EXPECT_FALSE(key1 < key2);
        EXPECT_TRUE(key2 < key1);
        break;
      default:
        FAIL() << "Invalid expectation. Can be only -1, 0, 1";
    }
  }
}

TEST_F(MultiThreadedCertVerifierTest, CertTrustAnchorProvider) {
  MockCertTrustAnchorProvider trust_provider;
  verifier_.SetCertTrustAnchorProvider(&trust_provider);

  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(test_cert.get());

  const CertificateList empty_cert_list;
  CertificateList cert_list;
  cert_list.push_back(test_cert);

  // Check that Verify() asks the |trust_provider| for the current list of
  // additional trust anchors.
  int error;
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  CertVerifier::RequestHandle request_handle;
  EXPECT_CALL(trust_provider, GetAdditionalTrustAnchors())
      .WillOnce(ReturnRef(empty_cert_list));
  error = verifier_.Verify(test_cert.get(),
                           "www.example.com",
                           0,
                           NULL,
                           &verify_result,
                           callback.callback(),
                           &request_handle,
                           BoundNetLog());
  Mock::VerifyAndClearExpectations(&trust_provider);
  ASSERT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle);
  error = callback.WaitForResult();
  EXPECT_EQ(ERR_CERT_COMMON_NAME_INVALID, error);
  ASSERT_EQ(1u, verifier_.requests());
  ASSERT_EQ(0u, verifier_.cache_hits());

  // The next Verify() uses the cached result.
  EXPECT_CALL(trust_provider, GetAdditionalTrustAnchors())
      .WillOnce(ReturnRef(empty_cert_list));
  error = verifier_.Verify(test_cert.get(),
                           "www.example.com",
                           0,
                           NULL,
                           &verify_result,
                           callback.callback(),
                           &request_handle,
                           BoundNetLog());
  Mock::VerifyAndClearExpectations(&trust_provider);
  EXPECT_EQ(ERR_CERT_COMMON_NAME_INVALID, error);
  EXPECT_FALSE(request_handle);
  ASSERT_EQ(2u, verifier_.requests());
  ASSERT_EQ(1u, verifier_.cache_hits());

  // Another Verify() for the same certificate but with a different list of
  // trust anchors will not reuse the cache.
  EXPECT_CALL(trust_provider, GetAdditionalTrustAnchors())
      .WillOnce(ReturnRef(cert_list));
  error = verifier_.Verify(test_cert.get(),
                           "www.example.com",
                           0,
                           NULL,
                           &verify_result,
                           callback.callback(),
                           &request_handle,
                           BoundNetLog());
  Mock::VerifyAndClearExpectations(&trust_provider);
  ASSERT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle);
  error = callback.WaitForResult();
  EXPECT_EQ(ERR_CERT_COMMON_NAME_INVALID, error);
  ASSERT_EQ(3u, verifier_.requests());
  ASSERT_EQ(1u, verifier_.cache_hits());
}

}  // namespace net
