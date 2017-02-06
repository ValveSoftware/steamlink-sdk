// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_reporting/error_report.h"

#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "components/certificate_reporting/cert_logger.pb.h"
#include "net/cert/cert_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::SSLInfo;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

namespace certificate_reporting {

namespace {

const char kDummyHostname[] = "dummy.hostname.com";
const char kDummyFailureLog[] = "dummy failure log";
const char kTestCertFilename[] = "test_mail_google_com.pem";

const net::CertStatus kCertStatus =
    net::CERT_STATUS_COMMON_NAME_INVALID | net::CERT_STATUS_REVOKED;

const CertLoggerRequest::CertError kFirstReportedCertError =
    CertLoggerRequest::ERR_CERT_COMMON_NAME_INVALID;
const CertLoggerRequest::CertError kSecondReportedCertError =
    CertLoggerRequest::ERR_CERT_REVOKED;

// Whether to include an unverified certificate chain in the test
// SSLInfo. In production code, an unverified cert chain will not be
// present if the resource was loaded from cache.
enum UnverifiedCertChainStatus {
  INCLUDE_UNVERIFIED_CERT_CHAIN,
  EXCLUDE_UNVERIFIED_CERT_CHAIN
};

void GetTestSSLInfo(UnverifiedCertChainStatus unverified_cert_chain_status,
                    SSLInfo* info,
                    net::CertStatus cert_status) {
  info->cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), kTestCertFilename);
  ASSERT_TRUE(info->cert);
  if (unverified_cert_chain_status == INCLUDE_UNVERIFIED_CERT_CHAIN) {
    info->unverified_cert = net::ImportCertFromFile(
        net::GetTestCertsDirectory(), kTestCertFilename);
    ASSERT_TRUE(info->unverified_cert);
  }
  info->is_issued_by_known_root = true;
  info->cert_status = cert_status;
  info->pinning_failure_log = kDummyFailureLog;
}

std::string GetPEMEncodedChain() {
  base::FilePath cert_path =
      net::GetTestCertsDirectory().AppendASCII(kTestCertFilename);
  std::string cert_data;
  EXPECT_TRUE(base::ReadFileToString(cert_path, &cert_data));
  return cert_data;
}

void VerifyErrorReportSerialization(
    const ErrorReport& report,
    const SSLInfo& ssl_info,
    std::vector<CertLoggerRequest::CertError> cert_errors) {
  std::string serialized_report;
  ASSERT_TRUE(report.Serialize(&serialized_report));

  CertLoggerRequest deserialized_report;
  ASSERT_TRUE(deserialized_report.ParseFromString(serialized_report));
  EXPECT_EQ(kDummyHostname, deserialized_report.hostname());
  EXPECT_EQ(GetPEMEncodedChain(), deserialized_report.cert_chain());
  EXPECT_EQ(GetPEMEncodedChain(), deserialized_report.unverified_cert_chain());
  EXPECT_EQ(1, deserialized_report.pin().size());
  EXPECT_EQ(kDummyFailureLog, deserialized_report.pin().Get(0));
  EXPECT_EQ(
      ssl_info.is_issued_by_known_root,
      deserialized_report.is_issued_by_known_root());
  EXPECT_THAT(deserialized_report.cert_error(),
              UnorderedElementsAreArray(cert_errors));
}

// Test that a serialized ErrorReport can be deserialized as
// a CertLoggerRequest protobuf (which is the format that the receiving
// server expects it in) with the right data in it.
TEST(ErrorReportTest, SerializedReportAsProtobuf) {
  SSLInfo ssl_info;
  ASSERT_NO_FATAL_FAILURE(
      GetTestSSLInfo(INCLUDE_UNVERIFIED_CERT_CHAIN, &ssl_info, kCertStatus));
  ErrorReport report_known(kDummyHostname, ssl_info);
  std::vector<CertLoggerRequest::CertError> cert_errors;
  cert_errors.push_back(kFirstReportedCertError);
  cert_errors.push_back(kSecondReportedCertError);
  ASSERT_NO_FATAL_FAILURE(
      VerifyErrorReportSerialization(report_known, ssl_info, cert_errors));
  // Test that both values for |is_issued_by_known_root| are serialized
  // correctly.
  ssl_info.is_issued_by_known_root = false;
  ErrorReport report_unknown(kDummyHostname, ssl_info);
  ASSERT_NO_FATAL_FAILURE(
      VerifyErrorReportSerialization(report_unknown, ssl_info, cert_errors));
}

TEST(ErrorReportTest, SerializedReportAsProtobufWithInterstitialInfo) {
  std::string serialized_report;
  SSLInfo ssl_info;
  // Use EXCLUDE_UNVERIFIED_CERT_CHAIN here to exercise the code path
  // where SSLInfo does not contain the unverified cert chain. (The test
  // above exercises the path where it does.)
  ASSERT_NO_FATAL_FAILURE(
      GetTestSSLInfo(EXCLUDE_UNVERIFIED_CERT_CHAIN, &ssl_info, kCertStatus));
  ErrorReport report(kDummyHostname, ssl_info);

  report.SetInterstitialInfo(ErrorReport::INTERSTITIAL_CLOCK,
                             ErrorReport::USER_PROCEEDED,
                             ErrorReport::INTERSTITIAL_OVERRIDABLE);

  ASSERT_TRUE(report.Serialize(&serialized_report));

  CertLoggerRequest deserialized_report;
  ASSERT_TRUE(deserialized_report.ParseFromString(serialized_report));
  EXPECT_EQ(kDummyHostname, deserialized_report.hostname());
  EXPECT_EQ(GetPEMEncodedChain(), deserialized_report.cert_chain());
  EXPECT_EQ(std::string(), deserialized_report.unverified_cert_chain());
  EXPECT_EQ(1, deserialized_report.pin().size());
  EXPECT_EQ(kDummyFailureLog, deserialized_report.pin().Get(0));

  EXPECT_EQ(CertLoggerInterstitialInfo::INTERSTITIAL_CLOCK,
            deserialized_report.interstitial_info().interstitial_reason());
  EXPECT_EQ(true, deserialized_report.interstitial_info().user_proceeded());
  EXPECT_EQ(true, deserialized_report.interstitial_info().overridable());
  EXPECT_EQ(
      ssl_info.is_issued_by_known_root,
      deserialized_report.is_issued_by_known_root());

  EXPECT_THAT(
      deserialized_report.cert_error(),
      UnorderedElementsAre(kFirstReportedCertError, kSecondReportedCertError));
}

// Test that a serialized report can be parsed.
TEST(ErrorReportTest, ParseSerializedReport) {
  std::string serialized_report;
  SSLInfo ssl_info;
  ASSERT_NO_FATAL_FAILURE(
      GetTestSSLInfo(INCLUDE_UNVERIFIED_CERT_CHAIN, &ssl_info, kCertStatus));
  ErrorReport report(kDummyHostname, ssl_info);
  EXPECT_EQ(kDummyHostname, report.hostname());
  ASSERT_TRUE(report.Serialize(&serialized_report));

  ErrorReport parsed;
  ASSERT_TRUE(parsed.InitializeFromString(serialized_report));
  EXPECT_EQ(report.hostname(), parsed.hostname());
}

// Check that CT errors are handled correctly.
TEST(ErrorReportTest, CertificateTransparencyError) {
  SSLInfo ssl_info;
  ASSERT_NO_FATAL_FAILURE(
      GetTestSSLInfo(INCLUDE_UNVERIFIED_CERT_CHAIN, &ssl_info,
                     net::CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED));
  ErrorReport report_known(kDummyHostname, ssl_info);
  std::vector<CertLoggerRequest::CertError> cert_errors;
  cert_errors.push_back(
      CertLoggerRequest::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED);
  ASSERT_NO_FATAL_FAILURE(
      VerifyErrorReportSerialization(report_known, ssl_info, cert_errors));
}

}  // namespace

}  // namespace certificate_reporting
