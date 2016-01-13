// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/single_request_cert_verifier.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"

namespace net {

SingleRequestCertVerifier::SingleRequestCertVerifier(
    CertVerifier* cert_verifier)
    : cert_verifier_(cert_verifier),
      cur_request_(NULL) {
  DCHECK(cert_verifier_ != NULL);
}

SingleRequestCertVerifier::~SingleRequestCertVerifier() {
  if (cur_request_) {
    cert_verifier_->CancelRequest(cur_request_);
    cur_request_ = NULL;
  }
}

int SingleRequestCertVerifier::Verify(X509Certificate* cert,
                                      const std::string& hostname,
                                      int flags,
                                      CRLSet* crl_set,
                                      CertVerifyResult* verify_result,
                                      const CompletionCallback& callback,
                                      const BoundNetLog& net_log) {
  // Should not be already in use.
  DCHECK(!cur_request_ && cur_request_callback_.is_null());

  CertVerifier::RequestHandle request = NULL;

  // We need to be notified of completion before |callback| is called, so that
  // we can clear out |cur_request_*|.
  int rv = cert_verifier_->Verify(
      cert, hostname, flags, crl_set, verify_result,
      base::Bind(&SingleRequestCertVerifier::OnVerifyCompletion,
                 base::Unretained(this)),
      &request, net_log);

  if (rv == ERR_IO_PENDING) {
    // Cleared in OnVerifyCompletion().
    cur_request_ = request;
    cur_request_callback_ = callback;
  }

  return rv;
}

void SingleRequestCertVerifier::OnVerifyCompletion(int result) {
  DCHECK(cur_request_ && !cur_request_callback_.is_null());

  CompletionCallback callback = cur_request_callback_;

  // Clear the outstanding request information.
  cur_request_ = NULL;
  cur_request_callback_.Reset();

  // Call the user's original callback.
  callback.Run(result);
}

}  // namespace net
