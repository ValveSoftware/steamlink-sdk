// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_NETWORK_LAYER_H_
#define NET_FTP_FTP_NETWORK_LAYER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "net/ftp/ftp_transaction_factory.h"

namespace net {

class FtpNetworkSession;
class HostResolver;

class NET_EXPORT FtpNetworkLayer : public FtpTransactionFactory {
 public:
  explicit FtpNetworkLayer(HostResolver* host_resolver);
  virtual ~FtpNetworkLayer();

  static FtpTransactionFactory* CreateFactory(HostResolver* host_resolver);

  // FtpTransactionFactory methods:
  virtual FtpTransaction* CreateTransaction() OVERRIDE;
  virtual void Suspend(bool suspend) OVERRIDE;

 private:
  scoped_refptr<FtpNetworkSession> session_;
  bool suspended_;
  DISALLOW_COPY_AND_ASSIGN(FtpNetworkLayer);
};

}  // namespace net

#endif  // NET_FTP_FTP_NETWORK_LAYER_H_
