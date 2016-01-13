// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_NETWORK_CONTEXT_H_
#define MOJO_SERVICES_NETWORK_NETWORK_CONTEXT_H_

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"

namespace net {
class NetLog;
class URLRequestContext;
class URLRequestContextStorage;
class TransportSecurityPersister;
}

namespace mojo {

class NetworkContext {
 public:
  NetworkContext();
  ~NetworkContext();

  net::URLRequestContext* url_request_context() {
    return url_request_context_.get();
  }

 private:
  base::Thread file_thread_;
  base::Thread cache_thread_;
  scoped_ptr<net::NetLog> net_log_;
  scoped_ptr<net::URLRequestContextStorage> storage_;
  scoped_ptr<net::URLRequestContext> url_request_context_;
  scoped_ptr<net::TransportSecurityPersister> transport_security_persister_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContext);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_NETWORK_CONTEXT_H_
