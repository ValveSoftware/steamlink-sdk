// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_SSL_CONTEXT_HELPER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_SSL_CONTEXT_HELPER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/ssl/ssl_config_service.h"

namespace net {
class CertVerifier;
class TransportSecurityState;
}

namespace content {

class SSLContextHelper : public base::RefCounted<SSLContextHelper> {
 public:
  SSLContextHelper();

  net::CertVerifier* GetCertVerifier();
  net::TransportSecurityState* GetTransportSecurityState();
  const net::SSLConfig& ssl_config() { return ssl_config_; }

 private:
  friend class base::RefCounted<SSLContextHelper>;

  ~SSLContextHelper();

  // This is lazily created. Users should use GetCertVerifier to retrieve it.
  scoped_ptr<net::CertVerifier> cert_verifier_;
  // This is lazily created. Users should use GetTransportSecurityState to
  // retrieve it.
  scoped_ptr<net::TransportSecurityState> transport_security_state_;

  // The default SSL configuration settings are used, as opposed to Chrome's SSL
  // settings.
  net::SSLConfig ssl_config_;

  DISALLOW_COPY_AND_ASSIGN(SSLContextHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_SSL_CONTEXT_HELPER_H_
