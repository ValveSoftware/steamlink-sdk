// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_SPDY_SSL_H_
#define NET_TOOLS_FLIP_SERVER_SPDY_SSL_H_

#include <string>

#include "openssl/ssl.h"

namespace net {

struct SSLState {
  const SSL_METHOD* ssl_method;
  SSL_CTX* ssl_ctx;
};

void InitSSL(SSLState* state,
             std::string ssl_cert_name,
             std::string ssl_key_name,
             bool use_npn,
             int session_expiration_time,
             bool disable_ssl_compression);
SSL* CreateSSLContext(SSL_CTX* ssl_ctx);
void PrintSslError();

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_SPDY_SSL_H_
