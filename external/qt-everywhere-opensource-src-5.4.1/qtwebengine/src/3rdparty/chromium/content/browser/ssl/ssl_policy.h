// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_SSL_POLICY_H_
#define CONTENT_BROWSER_SSL_SSL_POLICY_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "webkit/common/resource_type.h"

namespace content {
class NavigationEntryImpl;
class SSLCertErrorHandler;
class SSLPolicyBackend;
class SSLRequestInfo;
class WebContentsImpl;

// SSLPolicy
//
// This class is responsible for making the security decisions that concern the
// SSL trust indicators.  It relies on the SSLPolicyBackend to actually enact
// the decisions it reaches.
//
class SSLPolicy {
 public:
  explicit SSLPolicy(SSLPolicyBackend* backend);

  // An error occurred with the certificate in an SSL connection.
  void OnCertError(SSLCertErrorHandler* handler);

  void DidRunInsecureContent(NavigationEntryImpl* entry,
                             const std::string& security_origin);

  // We have started a resource request with the given info.
  void OnRequestStarted(SSLRequestInfo* info);

  // Update the SSL information in |entry| to match the current state.
  // |web_contents| is the WebContentsImpl associated with this entry.
  void UpdateEntry(NavigationEntryImpl* entry,
                   WebContentsImpl* web_contents);

  SSLPolicyBackend* backend() const { return backend_; }

 private:
  // Callback that the user chose to accept or deny the certificate.
  void OnAllowCertificate(scoped_refptr<SSLCertErrorHandler> handler,
                          bool allow);

  // Helper method for derived classes handling certificate errors.
  //
  // |overridable| indicates whether or not the user could (assuming perfect
  // knowledge) successfully override the error and still get the security
  // guarantees of TLS. |strict_enforcement| indicates whether or not the
  // site the user is trying to connect to has requested strict enforcement
  // of certificate validation (e.g. with HTTP Strict-Transport-Security).
  void OnCertErrorInternal(SSLCertErrorHandler* handler,
                           bool overridable,
                           bool strict_enforcement);

  // If the security style of |entry| has not been initialized, then initialize
  // it with the default style for its URL.
  void InitializeEntryIfNeeded(NavigationEntryImpl* entry);

  // Mark |origin| as having run insecure content in the process with ID |pid|.
  void OriginRanInsecureContent(const std::string& origin, int pid);

  // The backend we use to enact our decisions.
  SSLPolicyBackend* backend_;

  DISALLOW_COPY_AND_ASSIGN(SSLPolicy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SSL_SSL_POLICY_H_
