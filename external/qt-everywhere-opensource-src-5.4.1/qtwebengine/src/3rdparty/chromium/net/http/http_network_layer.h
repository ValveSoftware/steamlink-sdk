// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_LAYER_H_
#define NET_HTTP_HTTP_NETWORK_LAYER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/power_monitor/power_observer.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/net_export.h"
#include "net/http/http_transaction_factory.h"

namespace net {

class HttpNetworkSession;

class NET_EXPORT HttpNetworkLayer
    : public HttpTransactionFactory,
      public base::PowerObserver,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  // Construct a HttpNetworkLayer with an existing HttpNetworkSession which
  // contains a valid ProxyService.
  explicit HttpNetworkLayer(HttpNetworkSession* session);
  virtual ~HttpNetworkLayer();

  // Create a transaction factory that instantiate a network layer over an
  // existing network session. Network session contains some valuable
  // information (e.g. authentication data) that we want to share across
  // multiple network layers. This method exposes the implementation details
  // of a network layer, use this method with an existing network layer only
  // when network session is shared.
  static HttpTransactionFactory* CreateFactory(HttpNetworkSession* session);

  // Forces an alternate protocol of SPDY/3 on port 443.
  // TODO(rch): eliminate this method.
  static void ForceAlternateProtocol();

  // HttpTransactionFactory methods:
  virtual int CreateTransaction(RequestPriority priority,
                                scoped_ptr<HttpTransaction>* trans) OVERRIDE;
  virtual HttpCache* GetCache() OVERRIDE;
  virtual HttpNetworkSession* GetSession() OVERRIDE;

  // base::PowerObserver methods:
  virtual void OnSuspend() OVERRIDE;
  virtual void OnResume() OVERRIDE;

 private:
  const scoped_refptr<HttpNetworkSession> session_;
  bool suspended_;

  DISALLOW_COPY_AND_ASSIGN(HttpNetworkLayer);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_LAYER_H_
