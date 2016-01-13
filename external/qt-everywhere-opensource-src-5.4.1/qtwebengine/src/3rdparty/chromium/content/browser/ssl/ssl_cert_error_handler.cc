// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_cert_error_handler.h"

#include "content/browser/ssl/ssl_manager.h"
#include "content/browser/ssl/ssl_policy.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"

namespace content {

SSLCertErrorHandler::SSLCertErrorHandler(
    const base::WeakPtr<Delegate>& delegate,
    const GlobalRequestID& id,
    ResourceType::Type resource_type,
    const GURL& url,
    int render_process_id,
    int render_frame_id,
    const net::SSLInfo& ssl_info,
    bool fatal)
    : SSLErrorHandler(delegate, id, resource_type, url, render_process_id,
                      render_frame_id),
      ssl_info_(ssl_info),
      cert_error_(net::MapCertStatusToNetError(ssl_info.cert_status)),
      fatal_(fatal) {
}

SSLCertErrorHandler* SSLCertErrorHandler::AsSSLCertErrorHandler() {
  return this;
}

void SSLCertErrorHandler::OnDispatchFailed() {
  // Requests can fail to dispatch because they don't have a WebContents. See
  // <http://crbug.com/86537>. In this case we have to make a decision in this
  // function, so we ignore revocation check failures.
  if (net::IsCertStatusMinorError(ssl_info().cert_status)) {
    ContinueRequest();
  } else {
    CancelRequest();
  }
}

void SSLCertErrorHandler::OnDispatched() {
  manager_->policy()->OnCertError(this);
}

SSLCertErrorHandler::~SSLCertErrorHandler() {}

}  // namespace content
