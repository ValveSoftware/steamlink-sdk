// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CLIENT_CERT_STORE_H_
#define NET_SSL_CLIENT_CERT_STORE_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "net/base/net_export.h"
#include "net/cert/x509_certificate.h"

namespace net {

class SSLCertRequestInfo;

// The caller is expected to keep the ClientCertStore alive until the callback
// supplied to GetClientCerts has been run.
class NET_EXPORT ClientCertStore {
 public:
  virtual ~ClientCertStore() {}

  // Get client certs matching the |cert_request_info|. On completion, the
  // results will be stored in |selected_certs| and the |callback| will be run.
  // The |callback| may be called sychronously. The caller must ensure the
  // ClientCertStore and the |selected_certs| object remain alive until the
  // callback has been run.
  virtual void GetClientCerts(const SSLCertRequestInfo& cert_request_info,
                              CertificateList* selected_certs,
                              const base::Closure& callback) = 0;
 protected:
  ClientCertStore() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientCertStore);
};

}  // namespace net

#endif  // NET_SSL_CLIENT_CERT_STORE_H_
