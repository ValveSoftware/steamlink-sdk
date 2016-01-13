// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/multi_log_ct_verifier.h"

#include <string>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/values.h"
#include "net/base/capturing_net_log.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/test_data_directory.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/ct_verify_result.h"
#include "net/cert/pem_tokenizer.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/ct_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const char kLogDescription[] = "somelog";
const char kSCTCountHistogram[] =
    "Net.CertificateTransparency.SCTsPerConnection";

class MultiLogCTVerifierTest : public ::testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    scoped_ptr<CTLogVerifier> log(
        CTLogVerifier::Create(ct::GetTestPublicKey(), kLogDescription));
    ASSERT_TRUE(log);

    verifier_.reset(new MultiLogCTVerifier());
    verifier_->AddLog(log.Pass());
    std::string der_test_cert(ct::GetDerEncodedX509Cert());
    chain_ = X509Certificate::CreateFromBytes(
        der_test_cert.data(),
        der_test_cert.length());
    ASSERT_TRUE(chain_);

    embedded_sct_chain_ =
        CreateCertificateChainFromFile(GetTestCertsDirectory(),
                                       "ct-test-embedded-cert.pem",
                                       X509Certificate::FORMAT_AUTO);
    ASSERT_TRUE(embedded_sct_chain_);
  }

  bool CheckForSingleVerifiedSCTInResult(const ct::CTVerifyResult& result) {
    return (result.verified_scts.size() == 1U) &&
        result.invalid_scts.empty() &&
        result.unknown_logs_scts.empty() &&
        result.verified_scts[0]->log_description == kLogDescription;
  }

  bool CheckForSCTOrigin(
      const ct::CTVerifyResult& result,
      ct::SignedCertificateTimestamp::Origin origin) {
    return (result.verified_scts.size() > 0) &&
        (result.verified_scts[0]->origin == origin);
  }

  bool CheckForEmbeddedSCTInNetLog(CapturingNetLog& net_log) {
    CapturingNetLog::CapturedEntryList entries;
    net_log.GetEntries(&entries);
    if (entries.size() != 2)
      return false;

    const CapturingNetLog::CapturedEntry& received = entries[0];
    std::string embedded_scts;
    if (!received.GetStringValue("embedded_scts", &embedded_scts))
      return false;
    if (embedded_scts.empty())
      return false;

    const CapturingNetLog::CapturedEntry& parsed = entries[1];
    base::ListValue* verified_scts;
    if (!parsed.GetListValue("verified_scts", &verified_scts) ||
        verified_scts->GetSize() != 1) {
      return false;
    }

    base::DictionaryValue* the_sct;
    if (!verified_scts->GetDictionary(0, &the_sct))
      return false;

    std::string origin;
    if (!the_sct->GetString("origin", &origin))
      return false;
    if (origin != "embedded_in_certificate")
      return false;

    base::ListValue* other_scts;
    if (!parsed.GetListValue("invalid_scts", &other_scts) ||
        !other_scts->empty()) {
      return false;
    }

    if (!parsed.GetListValue("unknown_logs_scts", &other_scts) ||
        !other_scts->empty()) {
      return false;
    }

    return true;
  }

  std::string GetSCTListWithInvalidSCT() {
    std::string sct(ct::GetTestSignedCertificateTimestamp());

    // Change a byte inside the Log ID part of the SCT so it does
    // not match the log used in the tests
    sct[15] = 't';

    std::string sct_list;
    ct::EncodeSCTListForTesting(sct, &sct_list);
    return sct_list;
  }

  bool VerifySinglePrecertificateChain(scoped_refptr<X509Certificate> chain,
                                       const BoundNetLog& bound_net_log,
                                       ct::CTVerifyResult* result) {
    return verifier_->Verify(
               chain, std::string(), std::string(), result, bound_net_log) ==
           OK;
  }

  bool VerifySinglePrecertificateChain(scoped_refptr<X509Certificate> chain) {
    ct::CTVerifyResult result;
    CapturingNetLog net_log;
    BoundNetLog bound_net_log =
        BoundNetLog::Make(&net_log, NetLog::SOURCE_CONNECT_JOB);

    return verifier_->Verify(
               chain, std::string(), std::string(), &result, bound_net_log) ==
           OK;
  }

  bool CheckPrecertificateVerification(scoped_refptr<X509Certificate> chain) {
    ct::CTVerifyResult result;
    CapturingNetLog net_log;
    BoundNetLog bound_net_log =
      BoundNetLog::Make(&net_log, NetLog::SOURCE_CONNECT_JOB);
    return (VerifySinglePrecertificateChain(chain, bound_net_log, &result) &&
            CheckForSingleVerifiedSCTInResult(result) &&
            CheckForSCTOrigin(result,
                              ct::SignedCertificateTimestamp::SCT_EMBEDDED) &&
            CheckForEmbeddedSCTInNetLog(net_log));
  }

  // Histogram-related helper methods
  int GetValueFromHistogram(std::string histogram_name, int sample_index) {
    base::Histogram* histogram = static_cast<base::Histogram*>(
        base::StatisticsRecorder::FindHistogram(histogram_name));

    if (histogram == NULL)
      return 0;

    scoped_ptr<base::HistogramSamples> samples = histogram->SnapshotSamples();
    return samples->GetCount(sample_index);
  }

  int NumConnectionsWithSingleSCT() {
    return GetValueFromHistogram(kSCTCountHistogram, 1);
  }

  int NumEmbeddedSCTsInHistogram() {
    return GetValueFromHistogram("Net.CertificateTransparency.SCTOrigin",
                                 ct::SignedCertificateTimestamp::SCT_EMBEDDED);
  }

  int NumValidSCTsInStatusHistogram() {
    return GetValueFromHistogram("Net.CertificateTransparency.SCTStatus",
                                 ct::SCT_STATUS_OK);
  }

 protected:
  scoped_ptr<MultiLogCTVerifier> verifier_;
  scoped_refptr<X509Certificate> chain_;
  scoped_refptr<X509Certificate> embedded_sct_chain_;
};

TEST_F(MultiLogCTVerifierTest, VerifiesEmbeddedSCT) {
  ASSERT_TRUE(CheckPrecertificateVerification(embedded_sct_chain_));
}

TEST_F(MultiLogCTVerifierTest, VerifiesEmbeddedSCTWithPreCA) {
  scoped_refptr<X509Certificate> chain(
      CreateCertificateChainFromFile(GetTestCertsDirectory(),
                                     "ct-test-embedded-with-preca-chain.pem",
                                     X509Certificate::FORMAT_AUTO));
  ASSERT_TRUE(chain);
  ASSERT_TRUE(CheckPrecertificateVerification(chain));
}

TEST_F(MultiLogCTVerifierTest, VerifiesEmbeddedSCTWithIntermediate) {
  scoped_refptr<X509Certificate> chain(CreateCertificateChainFromFile(
      GetTestCertsDirectory(),
      "ct-test-embedded-with-intermediate-chain.pem",
      X509Certificate::FORMAT_AUTO));
  ASSERT_TRUE(chain);
  ASSERT_TRUE(CheckPrecertificateVerification(chain));
}

TEST_F(MultiLogCTVerifierTest,
       VerifiesEmbeddedSCTWithIntermediateAndPreCA) {
  scoped_refptr<X509Certificate> chain(CreateCertificateChainFromFile(
      GetTestCertsDirectory(),
      "ct-test-embedded-with-intermediate-preca-chain.pem",
      X509Certificate::FORMAT_AUTO));
  ASSERT_TRUE(chain);
  ASSERT_TRUE(CheckPrecertificateVerification(chain));
}

TEST_F(MultiLogCTVerifierTest,
       VerifiesSCTOverX509Cert) {
  std::string sct(ct::GetTestSignedCertificateTimestamp());

  std::string sct_list;
  ASSERT_TRUE(ct::EncodeSCTListForTesting(sct, &sct_list));

  ct::CTVerifyResult result;
  EXPECT_EQ(OK,
            verifier_->Verify(chain_, std::string(), sct_list, &result,
                              BoundNetLog()));
  ASSERT_TRUE(CheckForSingleVerifiedSCTInResult(result));
  ASSERT_TRUE(CheckForSCTOrigin(
      result, ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION));
}

TEST_F(MultiLogCTVerifierTest,
       IdentifiesSCTFromUnknownLog) {
  std::string sct_list = GetSCTListWithInvalidSCT();
  ct::CTVerifyResult result;

  EXPECT_NE(OK,
            verifier_->Verify(
                chain_, std::string(), sct_list, &result, BoundNetLog()));
  EXPECT_EQ(1U, result.unknown_logs_scts.size());
  EXPECT_EQ("", result.unknown_logs_scts[0]->log_description);
}

TEST_F(MultiLogCTVerifierTest, CountsValidSCTsInStatusHistogram) {
  int num_valid_scts = NumValidSCTsInStatusHistogram();

  ASSERT_TRUE(VerifySinglePrecertificateChain(embedded_sct_chain_));

  EXPECT_EQ(num_valid_scts + 1, NumValidSCTsInStatusHistogram());
}

TEST_F(MultiLogCTVerifierTest, CountsInvalidSCTsInStatusHistogram) {
  std::string sct_list = GetSCTListWithInvalidSCT();
  ct::CTVerifyResult result;
  int num_valid_scts = NumValidSCTsInStatusHistogram();
  int num_invalid_scts = GetValueFromHistogram(
      "Net.CertificateTransparency.SCTStatus", ct::SCT_STATUS_LOG_UNKNOWN);

  EXPECT_NE(OK,
            verifier_->Verify(chain_, std::string(), sct_list, &result,
                              BoundNetLog()));

  ASSERT_EQ(num_valid_scts, NumValidSCTsInStatusHistogram());
  ASSERT_EQ(num_invalid_scts + 1,
            GetValueFromHistogram("Net.CertificateTransparency.SCTStatus",
                                  ct::SCT_STATUS_LOG_UNKNOWN));
}

TEST_F(MultiLogCTVerifierTest, CountsSingleEmbeddedSCTInConnectionsHistogram) {
  int old_sct_count = NumConnectionsWithSingleSCT();
  ASSERT_TRUE(CheckPrecertificateVerification(embedded_sct_chain_));
  EXPECT_EQ(old_sct_count + 1, NumConnectionsWithSingleSCT());
}

TEST_F(MultiLogCTVerifierTest, CountsSingleEmbeddedSCTInOriginsHistogram) {
  int old_embedded_count = NumEmbeddedSCTsInHistogram();
  ASSERT_TRUE(CheckPrecertificateVerification(embedded_sct_chain_));
  EXPECT_EQ(old_embedded_count + 1, NumEmbeddedSCTsInHistogram());
}

TEST_F(MultiLogCTVerifierTest, CountsZeroSCTsCorrectly) {
  int connections_without_scts = GetValueFromHistogram(kSCTCountHistogram, 0);
  EXPECT_FALSE(VerifySinglePrecertificateChain(chain_));
  ASSERT_EQ(connections_without_scts + 1,
            GetValueFromHistogram(kSCTCountHistogram, 0));
}

}  // namespace

}  // namespace net
