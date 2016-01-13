// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_MOCK_CRYPTO_CLIENT_STREAM_FACTORY_H_
#define NET_QUIC_TEST_TOOLS_MOCK_CRYPTO_CLIENT_STREAM_FACTORY_H_

#include <string>

#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_crypto_client_stream_factory.h"
#include "net/quic/test_tools/mock_crypto_client_stream.h"

namespace net {

class QuicServerId;

class MockCryptoClientStreamFactory : public QuicCryptoClientStreamFactory  {
 public:
  MockCryptoClientStreamFactory();
  virtual ~MockCryptoClientStreamFactory() {}

  virtual QuicCryptoClientStream* CreateQuicCryptoClientStream(
      const QuicServerId& server_id,
      QuicClientSession* session,
      QuicCryptoClientConfig* crypto_config) OVERRIDE;

  void set_handshake_mode(
      MockCryptoClientStream::HandshakeMode handshake_mode) {
    handshake_mode_ = handshake_mode;
  }

  void set_proof_verify_details(
      const ProofVerifyDetails* proof_verify_details) {
    proof_verify_details_ = proof_verify_details;
  }

  MockCryptoClientStream* last_stream() const {
    return last_stream_;
  }

 private:
  MockCryptoClientStream::HandshakeMode handshake_mode_;
  MockCryptoClientStream* last_stream_;
  const ProofVerifyDetails* proof_verify_details_;

  DISALLOW_COPY_AND_ASSIGN(MockCryptoClientStreamFactory);
};

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_MOCK_CRYPTO_CLIENT_STREAM_FACTORY_H_
