// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_SSL_CERT_ERROR_HANDLER_H_
#define CONTENT_BROWSER_SSL_SSL_CERT_ERROR_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "content/browser/ssl/ssl_error_handler.h"
#include "net/ssl/ssl_info.h"

namespace content {

// A CertError represents an error that occurred with the certificate in an
// SSL session.  A CertError object exists both on the IO thread and on the UI
// thread and allows us to cancel/continue a request it is associated with.
class SSLCertErrorHandler : public SSLErrorHandler {
 public:
  // Construct on the IO thread.
  SSLCertErrorHandler(const base::WeakPtr<Delegate>& delegate,
                      const GlobalRequestID& id,
                      ResourceType::Type resource_type,
                      const GURL& url,
                      int render_process_id,
                      int render_frame_id,
                      const net::SSLInfo& ssl_info,
                      bool fatal);

  virtual SSLCertErrorHandler* AsSSLCertErrorHandler() OVERRIDE;

  // These accessors are available on either thread
  const net::SSLInfo& ssl_info() const { return ssl_info_; }
  int cert_error() const { return cert_error_; }
  bool fatal() const { return fatal_; }

 protected:
  // SSLErrorHandler methods
  virtual void OnDispatchFailed() OVERRIDE;
  virtual void OnDispatched() OVERRIDE;

 private:
  virtual ~SSLCertErrorHandler();

  // These read-only members may be accessed on any thread.
  const net::SSLInfo ssl_info_;
  const int cert_error_;  // The error we represent.
  const bool fatal_;  // True if the error is from a host requiring
                      // certificate errors to be fatal.

  DISALLOW_COPY_AND_ASSIGN(SSLCertErrorHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SSL_SSL_CERT_ERROR_HANDLER_H_
