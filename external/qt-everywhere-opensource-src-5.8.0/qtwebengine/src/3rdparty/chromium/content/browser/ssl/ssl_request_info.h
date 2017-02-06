// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_SSL_REQUEST_INFO_H_
#define CONTENT_BROWSER_SSL_SSL_REQUEST_INFO_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/common/resource_type.h"
#include "net/cert/cert_status_flags.h"
#include "url/gurl.h"

namespace content {

// SSLRequestInfo wraps up the information SSLPolicy needs about a request in
// order to update our security UI.  SSLRequestInfo is RefCounted in case we
// need to deal with the request asynchronously.
class SSLRequestInfo : public base::RefCounted<SSLRequestInfo> {
 public:
  SSLRequestInfo(const GURL& url,
                 ResourceType resource_type,
                 int ssl_cert_id,
                 net::CertStatus ssl_cert_status);

  const GURL& url() const { return url_; }
  ResourceType resource_type() const { return resource_type_; }
  int ssl_cert_id() const { return ssl_cert_id_; }
  net::CertStatus ssl_cert_status() const { return ssl_cert_status_; }

 private:
  friend class base::RefCounted<SSLRequestInfo>;

  virtual ~SSLRequestInfo();

  GURL url_;
  ResourceType resource_type_;
  int ssl_cert_id_;
  net::CertStatus ssl_cert_status_;

  DISALLOW_COPY_AND_ASSIGN(SSLRequestInfo);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SSL_SSL_REQUEST_INFO_H_
