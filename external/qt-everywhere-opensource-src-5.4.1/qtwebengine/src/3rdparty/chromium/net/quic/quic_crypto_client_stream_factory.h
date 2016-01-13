// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_FACTORY_H_
#define NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_FACTORY_H_

#include <string>

#include "net/base/net_export.h"

namespace net {

class QuicClientSession;
class QuicCryptoClientStream;
class QuicServerId;

// An interface used to instantiate QuicCryptoClientStream objects. Used to
// facilitate testing code with mock implementations.
class NET_EXPORT QuicCryptoClientStreamFactory {
 public:
  virtual ~QuicCryptoClientStreamFactory() {}

  virtual QuicCryptoClientStream* CreateQuicCryptoClientStream(
      const QuicServerId& server_id,
      QuicClientSession* session,
      QuicCryptoClientConfig* crypto_config) = 0;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_FACTORY_H_
