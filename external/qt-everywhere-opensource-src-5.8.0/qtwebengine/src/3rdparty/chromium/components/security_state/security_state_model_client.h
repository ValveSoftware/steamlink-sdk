// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_STATE_SECURITY_STATE_MODEL_CLIENT_H_
#define COMPONENTS_SECURITY_STATE_SECURITY_STATE_MODEL_CLIENT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/security_state/security_state_model.h"
#include "net/cert/cert_status_flags.h"

namespace net {
class X509Certificate;
}  // namespace net

namespace security_state {

// Provides embedder-specific information that a SecurityStateModel
// needs to determine the page's security status.
class SecurityStateModelClient {
 public:
  SecurityStateModelClient() {}
  virtual ~SecurityStateModelClient() {}

  // Retrieves the visible security state that is relevant to the
  // SecurityStateModel.
  virtual void GetVisibleSecurityState(
      SecurityStateModel::VisibleSecurityState* state) = 0;

  // Returns the certificate used to load the page or request.
  virtual bool RetrieveCert(scoped_refptr<net::X509Certificate>* cert) = 0;

  // Returns true if the page or request is known to be loaded with a
  // certificate installed by the system administrator.
  virtual bool UsedPolicyInstalledCertificate() = 0;

  // Returns true if the given |url|'s origin should be considered secure.
  virtual bool IsOriginSecure(const GURL& url) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityStateModelClient);
};

}  // namespace security_state

#endif  // COMPONENTS_SECURITY_STATE_SECURITY_STATE_MODEL_CLIENT_H_
