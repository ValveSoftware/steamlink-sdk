// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_P2P_QUIC_P2P_CRYPTO_STREAM_H_
#define NET_QUIC_P2P_QUIC_P2P_CRYPTO_STREAM_H_

#include "base/macros.h"
#include "net/quic/p2p/quic_p2p_crypto_config.h"
#include "net/quic/quic_crypto_stream.h"

namespace net {

class QuicSession;

class NET_EXPORT_PRIVATE QuicP2PCryptoStream : public QuicCryptoStream {
 public:
  QuicP2PCryptoStream(QuicSession* session, const QuicP2PCryptoConfig& config);
  ~QuicP2PCryptoStream() override;

  bool Connect();

 private:
  QuicP2PCryptoConfig config_;

  DISALLOW_COPY_AND_ASSIGN(QuicP2PCryptoStream);
};

}  // namespace net

#endif  // NET_QUIC_P2P_QUIC_P2P_CRYPTO_STREAM_H_
