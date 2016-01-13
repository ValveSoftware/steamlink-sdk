// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_client_session.h"

#include "base/logging.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_server_id.h"
#include "net/tools/quic/quic_spdy_client_stream.h"

using std::string;

namespace net {
namespace tools {

QuicClientSession::QuicClientSession(
    const QuicServerId& server_id,
    const QuicConfig& config,
    QuicConnection* connection,
    QuicCryptoClientConfig* crypto_config)
    : QuicClientSessionBase(connection, config),
      crypto_stream_(server_id, this, NULL, crypto_config) {
}

QuicClientSession::~QuicClientSession() {
}

void QuicClientSession::OnProofValid(
    const QuicCryptoClientConfig::CachedState& /*cached*/) {
}

void QuicClientSession::OnProofVerifyDetailsAvailable(
    const ProofVerifyDetails& /*verify_details*/) {
}

QuicSpdyClientStream* QuicClientSession::CreateOutgoingDataStream() {
  if (!crypto_stream_.encryption_established()) {
    DVLOG(1) << "Encryption not active so no outgoing stream created.";
    return NULL;
  }
  if (GetNumOpenStreams() >= get_max_open_streams()) {
    DVLOG(1) << "Failed to create a new outgoing stream. "
             << "Already " << GetNumOpenStreams() << " open.";
    return NULL;
  }
  if (goaway_received()) {
    DVLOG(1) << "Failed to create a new outgoing stream. "
             << "Already received goaway.";
    return NULL;
  }
  QuicSpdyClientStream* stream
      = new QuicSpdyClientStream(GetNextStreamId(), this);
  ActivateStream(stream);
  return stream;
}

QuicCryptoClientStream* QuicClientSession::GetCryptoStream() {
  return &crypto_stream_;
}

bool QuicClientSession::CryptoConnect() {
  return crypto_stream_.CryptoConnect();
}

int QuicClientSession::GetNumSentClientHellos() const {
  return crypto_stream_.num_sent_client_hellos();
}

QuicDataStream* QuicClientSession::CreateIncomingDataStream(
    QuicStreamId id) {
  DLOG(ERROR) << "Server push not supported";
  return NULL;
}

}  // namespace tools
}  // namespace net
