// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CLIENT_CERT_STORE_WIN_H_
#define NET_SSL_CLIENT_CERT_STORE_WIN_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "net/base/net_export.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"

namespace net {

class NET_EXPORT ClientCertStoreWin : public ClientCertStore {
 public:
  ClientCertStoreWin();
  virtual ~ClientCertStoreWin();

  // ClientCertStore:
  virtual void GetClientCerts(const SSLCertRequestInfo& cert_request_info,
                              CertificateList* selected_certs,
                              const base::Closure& callback) OVERRIDE;

 private:
  friend class ClientCertStoreWinTestDelegate;

  // A hook for testing. Filters |input_certs| using the logic being used to
  // filter the system store when GetClientCerts() is called.
  // Implemented by creating a temporary in-memory store and filtering it
  // using the common logic.
  bool SelectClientCertsForTesting(const CertificateList& input_certs,
                                   const SSLCertRequestInfo& cert_request_info,
                                   CertificateList* selected_certs);

  DISALLOW_COPY_AND_ASSIGN(ClientCertStoreWin);
};

}  // namespace net

#endif  // NET_SSL_CLIENT_CERT_STORE_WIN_H_
