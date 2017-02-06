// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_REPORTING_CERTIFICATE_ERROR_REPORT_H_
#define COMPONENTS_CERTIFICATE_REPORTING_CERTIFICATE_ERROR_REPORT_H_

#include <memory>
#include <string>


namespace net {
class SSLInfo;
}  // namespace net

namespace certificate_reporting {

class CertLoggerRequest;

// This class builds and serializes reports for invalid SSL certificate
// chains, intended to be sent with ErrorReporter.
class ErrorReport {
 public:
  // Describes the type of interstitial that the user was shown for the
  // error that this report represents. Gets mapped to
  // |CertLoggerInterstitialInfo::InterstitialReason|.
  enum InterstitialReason {
    INTERSTITIAL_SSL,
    INTERSTITIAL_CAPTIVE_PORTAL,
    INTERSTITIAL_CLOCK
  };

  // Whether the user clicked through the interstitial or not.
  enum ProceedDecision { USER_PROCEEDED, USER_DID_NOT_PROCEED };

  // Whether the user was shown an option to click through the
  // interstitial.
  enum Overridable { INTERSTITIAL_OVERRIDABLE, INTERSTITIAL_NOT_OVERRIDABLE };

  // Constructs an empty report.
  ErrorReport();

  // Constructs a report for the given |hostname| using the SSL
  // properties in |ssl_info|.
  ErrorReport(const std::string& hostname, const net::SSLInfo& ssl_info);

  ~ErrorReport();

  // Initializes an empty report by parsing the given serialized
  // report. |serialized_report| should be a serialized
  // CertLoggerRequest protobuf. Returns true if the report could be
  // successfully parsed and false otherwise.
  bool InitializeFromString(const std::string& serialized_report);

  // Serializes the report. The output will be a serialized
  // CertLoggerRequest protobuf. Returns true if the serialization was
  // successful and false otherwise.
  bool Serialize(std::string* output) const;

  void SetInterstitialInfo(const InterstitialReason& interstitial_reason,
                           const ProceedDecision& proceed_decision,
                           const Overridable& overridable);

  // Gets the hostname to which this report corresponds.
  const std::string& hostname() const;

 private:
  std::unique_ptr<CertLoggerRequest> cert_report_;
};

}  // namespace certificate_reporting

#endif  // COMPONENTS_CERTIFICATE_REPORTING_CERTIFICATE_ERROR_REPORT_H_
