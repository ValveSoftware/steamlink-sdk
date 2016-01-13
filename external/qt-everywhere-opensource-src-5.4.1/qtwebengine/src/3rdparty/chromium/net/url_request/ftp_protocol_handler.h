// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_FTP_PROTOCOL_HANDLER_H_
#define NET_URL_REQUEST_FTP_PROTOCOL_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request_job_factory.h"

namespace net {

class FtpAuthCache;
class FtpTransactionFactory;
class NetworkDelegate;
class URLRequestJob;

// Implements a ProtocolHandler for FTP.
class NET_EXPORT FtpProtocolHandler :
    public URLRequestJobFactory::ProtocolHandler {
 public:
  explicit FtpProtocolHandler(FtpTransactionFactory* ftp_transaction_factory);
  virtual ~FtpProtocolHandler();
  virtual URLRequestJob* MaybeCreateJob(
      URLRequest* request, NetworkDelegate* network_delegate) const OVERRIDE;

 private:
  friend class FtpTestURLRequestContext;

  FtpTransactionFactory* ftp_transaction_factory_;
  scoped_ptr<FtpAuthCache> ftp_auth_cache_;

  DISALLOW_COPY_AND_ASSIGN(FtpProtocolHandler);
};

}  // namespace net

#endif  // NET_URL_REQUEST_FTP_PROTOCOL_HANDLER_H_
