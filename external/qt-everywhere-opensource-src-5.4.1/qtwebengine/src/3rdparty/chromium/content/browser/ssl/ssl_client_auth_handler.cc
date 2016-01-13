// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_client_auth_handler.h"

#include "base/bind.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_transaction_factory.h"
#include "net/ssl/client_cert_store.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace content {

SSLClientAuthHandler::SSLClientAuthHandler(
    scoped_ptr<net::ClientCertStore> client_cert_store,
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info)
    : request_(request),
      http_network_session_(
          request_->context()->http_transaction_factory()->GetSession()),
      cert_request_info_(cert_request_info),
      client_cert_store_(client_cert_store.Pass()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

SSLClientAuthHandler::~SSLClientAuthHandler() {
  // If we were simply dropped, then act as if we selected no certificate.
  DoCertificateSelected(NULL);
}

void SSLClientAuthHandler::OnRequestCancelled() {
  request_ = NULL;
}

void SSLClientAuthHandler::SelectCertificate() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(request_);

  if (client_cert_store_) {
    client_cert_store_->GetClientCerts(
        *cert_request_info_,
        &cert_request_info_->client_certs,
        base::Bind(&SSLClientAuthHandler::DidGetClientCerts, this));
  } else {
    DidGetClientCerts();
  }
}

void SSLClientAuthHandler::CertificateSelected(net::X509Certificate* cert) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(1) << this << " CertificateSelected " << cert;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &SSLClientAuthHandler::DoCertificateSelected, this,
          make_scoped_refptr(cert)));
}

void SSLClientAuthHandler::DidGetClientCerts() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Request may have cancelled while we were getting client certs.
  if (!request_)
    return;

  // Note that if |client_cert_store_| is NULL, we intentionally fall through to
  // DoCertificateSelected. This is for platforms where the client cert matching
  // is not performed by Chrome, the platform can handle the cert matching
  // before showing the dialog.
  if (client_cert_store_ && cert_request_info_->client_certs.empty()) {
    // No need to query the user if there are no certs to choose from.
    DoCertificateSelected(NULL);
    return;
  }

  int render_process_host_id;
  int render_frame_host_id;
  if (!ResourceRequestInfo::ForRequest(request_)->GetAssociatedRenderFrame(
          &render_process_host_id,
          &render_frame_host_id))
    NOTREACHED();

  // If the RVH does not exist by the time this task gets run, then the task
  // will be dropped and the scoped_refptr to SSLClientAuthHandler will go
  // away, so we do not leak anything. The destructor takes care of ensuring
  // the net::URLRequest always gets a response.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &SSLClientAuthHandler::DoSelectCertificate, this,
          render_process_host_id, render_frame_host_id));
}

void SSLClientAuthHandler::DoCertificateSelected(net::X509Certificate* cert) {
  VLOG(1) << this << " DoCertificateSelected " << cert;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // request_ could have been NULLed if the request was cancelled while the
  // user was choosing a cert, or because we have already responded to the
  // certificate.
  if (request_) {
    request_->ContinueWithCertificate(cert);

    ResourceDispatcherHostImpl::Get()->
        ClearSSLClientAuthHandlerForRequest(request_);
    request_ = NULL;
  }
}

void SSLClientAuthHandler::DoSelectCertificate(
    int render_process_host_id, int render_frame_host_id) {
  GetContentClient()->browser()->SelectClientCertificate(
      render_process_host_id,
      render_frame_host_id,
      http_network_session_,
      cert_request_info_.get(),
      base::Bind(&SSLClientAuthHandler::CertificateSelected, this));
}

}  // namespace content
