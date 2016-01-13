// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_FRAUDULENT_CERTIFICATE_REPORTER_H_
#define NET_URL_REQUEST_FRAUDULENT_CERTIFICATE_REPORTER_H_

#include <string>

namespace net {

class SSLInfo;

// FraudulentCertificateReporter is an interface for asynchronously
// reporting certificate chains that fail the certificate pinning
// check.
class FraudulentCertificateReporter {
 public:
  virtual ~FraudulentCertificateReporter() {}

  // Sends a report to the report collection server containing the |ssl_info|
  // associated with a connection to |hostname|. If |sni_available| is true,
  // searches the SNI transport security metadata as well as the usual
  // transport security metadata when determining policy for sending the report.
  virtual void SendReport(const std::string& hostname,
                          const SSLInfo& ssl_info,
                          bool sni_available) = 0;
};

}  // namespace net

#endif  // NET_URL_REQUEST_FRAUDULENT_CERTIFICATE_REPORTER_H_

