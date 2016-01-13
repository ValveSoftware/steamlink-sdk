// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H_
#define CONTENT_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "net/ssl/ssl_cert_request_info.h"

namespace net {
class ClientCertStore;
class HttpNetworkSession;
class URLRequest;
class X509Certificate;
}  // namespace net

namespace content {

class ResourceContext;

// This class handles the approval and selection of a certificate for SSL client
// authentication by the user.
// It is self-owned and deletes itself when the UI reports the user selection or
// when the net::URLRequest is cancelled.
class CONTENT_EXPORT SSLClientAuthHandler
    : public base::RefCountedThreadSafe<
          SSLClientAuthHandler, BrowserThread::DeleteOnIOThread> {
 public:
  SSLClientAuthHandler(scoped_ptr<net::ClientCertStore> client_cert_store,
                       net::URLRequest* request,
                       net::SSLCertRequestInfo* cert_request_info);

  // Selects a certificate and resumes the URL request with that certificate.
  // Should only be called on the IO thread.
  void SelectCertificate();

  // Invoked when the request associated with this handler is cancelled.
  // Should only be called on the IO thread.
  void OnRequestCancelled();

  // Calls DoCertificateSelected on the I/O thread.
  // Called on the UI thread after the user has made a selection (which may
  // be long after DoSelectCertificate returns, if the UI is modeless/async.)
  void CertificateSelected(net::X509Certificate* cert);

 protected:
  virtual ~SSLClientAuthHandler();

 private:
  friend class base::RefCountedThreadSafe<
      SSLClientAuthHandler, BrowserThread::DeleteOnIOThread>;
  friend class BrowserThread;
  friend class base::DeleteHelper<SSLClientAuthHandler>;

  // Called when ClientCertStore is done retrieving the cert list.
  void DidGetClientCerts();

  // Notifies that the user has selected a cert.
  // Called on the IO thread.
  void DoCertificateSelected(net::X509Certificate* cert);

  // Selects a client certificate on the UI thread.
  void DoSelectCertificate(int render_process_host_id,
                           int render_frame_host_id);

  // The net::URLRequest that triggered this client auth.
  net::URLRequest* request_;

  // The HttpNetworkSession |request_| is associated with.
  const net::HttpNetworkSession* http_network_session_;

  // The certs to choose from.
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;

  scoped_ptr<net::ClientCertStore> client_cert_store_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientAuthHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H_
