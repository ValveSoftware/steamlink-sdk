// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_store_nss.h"

#include "net/ssl/client_cert_store_unittest-inl.h"

namespace net {

class ClientCertStoreNSSTestDelegate {
 public:
  ClientCertStoreNSSTestDelegate() {}

  bool SelectClientCerts(const CertificateList& input_certs,
                         const SSLCertRequestInfo& cert_request_info,
                         CertificateList* selected_certs) {
    // Filters |input_certs| using the logic being used to filter the system
    // store when GetClientCerts() is called.
    ClientCertStoreNSS::FilterCertsOnWorkerThread(
        input_certs, cert_request_info, selected_certs);
    return true;
  }
};

INSTANTIATE_TYPED_TEST_CASE_P(NSS,
                              ClientCertStoreTest,
                              ClientCertStoreNSSTestDelegate);

}  // namespace net
