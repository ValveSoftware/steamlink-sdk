// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/ssl_context_helper.h"

#include "net/cert/cert_verifier.h"
#include "net/http/transport_security_state.h"

namespace content {

SSLContextHelper::SSLContextHelper() {}

SSLContextHelper::~SSLContextHelper() {}

net::CertVerifier* SSLContextHelper::GetCertVerifier() {
  if (!cert_verifier_)
    cert_verifier_.reset(net::CertVerifier::CreateDefault());
  return cert_verifier_.get();
}

net::TransportSecurityState* SSLContextHelper::GetTransportSecurityState() {
  if (!transport_security_state_)
    transport_security_state_.reset(new net::TransportSecurityState());
  return transport_security_state_.get();
}

}  // namespace content
