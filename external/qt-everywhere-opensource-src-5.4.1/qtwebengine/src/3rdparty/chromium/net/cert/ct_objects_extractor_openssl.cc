// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_objects_extractor.h"

#include "base/logging.h"

namespace net {

namespace ct {

bool ExtractEmbeddedSCTList(X509Certificate::OSCertHandle cert,
                            std::string* sct_list) {
  NOTIMPLEMENTED();
  return false;
}

bool GetPrecertLogEntry(X509Certificate::OSCertHandle leaf,
                        X509Certificate::OSCertHandle issuer,
                        LogEntry* result) {
  NOTIMPLEMENTED();
  return false;
}

bool GetX509LogEntry(X509Certificate::OSCertHandle leaf, LogEntry* result) {
  NOTIMPLEMENTED();
  return false;
}

bool ExtractSCTListFromOCSPResponse(X509Certificate::OSCertHandle issuer,
                                    const std::string& cert_serial_number,
                                    const std::string& ocsp_response,
                                    std::string* sct_list) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace ct

}  // namespace net
