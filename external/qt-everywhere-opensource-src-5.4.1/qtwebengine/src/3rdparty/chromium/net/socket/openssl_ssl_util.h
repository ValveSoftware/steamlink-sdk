// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_OPENSSL_SSL_UTIL_H_
#define NET_SOCKET_OPENSSL_SSL_UTIL_H_

namespace crypto {
class OpenSSLErrStackTracer;
}

namespace net {

// Utility to construct the appropriate set & clear masks for use the OpenSSL
// options and mode configuration functions. (SSL_set_options etc)
struct SslSetClearMask {
  SslSetClearMask();
  void ConfigureFlag(long flag, bool state);

  long set_mask;
  long clear_mask;
};

// Converts an OpenSSL error code into a net error code, walking the OpenSSL
// error stack if needed. Note that |tracer| is not currently used in the
// implementation, but is passed in anyway as this ensures the caller will clear
// any residual codes left on the error stack.
int MapOpenSSLError(int err, const crypto::OpenSSLErrStackTracer& tracer);

}  // namespace net

#endif  // NET_SOCKET_OPENSSL_SSL_UTIL_H_
