// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_SSL_POLICY_BACKEND_H_
#define CONTENT_BROWSER_SSL_SSL_POLICY_BACKEND_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"

namespace content {
class NavigationControllerImpl;

class CONTENT_EXPORT SSLPolicyBackend {
 public:
  explicit SSLPolicyBackend(NavigationControllerImpl* controller);

  // Records that a host has run insecure content.
  void HostRanInsecureContent(const std::string& host, int pid);

  // Returns whether the specified host ran insecure content.
  bool DidHostRunInsecureContent(const std::string& host, int pid) const;

  // Revokes all allow exceptions by the user for |host|.
  void RevokeUserAllowExceptions(const std::string& host);

  // Returns true if and only if a user exception has previously been made for
  // |host|.
  bool HasAllowException(const std::string& host);

  // Records that |cert| is permitted to be used for |host| in the future, for
  // a specific error type.
  void AllowCertForHost(const net::X509Certificate& cert,
                        const std::string& host,
                        net::CertStatus error);

  // Queries whether |cert| is allowed for |host|. Returns true in
  // |expired_previous_decision| if a user decision had been made previously but
  // that decision has expired, otherwise false.
  SSLHostStateDelegate::CertJudgment QueryPolicy(
      const net::X509Certificate& cert,
      const std::string& host,
      net::CertStatus error,
      bool* expired_previous_decision);

 private:
  // SSL state delegate specific for each host.
  SSLHostStateDelegate* ssl_host_state_delegate_;

  NavigationControllerImpl* controller_;

  DISALLOW_COPY_AND_ASSIGN(SSLPolicyBackend);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SSL_SSL_POLICY_BACKEND_H_
