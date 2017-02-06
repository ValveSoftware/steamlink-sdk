// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_P2P_QUIC_P2P_SESSION_H_
#define NET_QUIC_P2P_QUIC_P2P_SESSION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "net/quic/p2p/quic_p2p_stream.h"
#include "net/quic/quic_client_session_base.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_protocol.h"

namespace net {

class QuicConfig;
class QuicConnection;
class QuicP2PCryptoStream;
class QuicP2PCryptoConfig;
class Socket;
class IOBuffer;

// QuicP2PSession represents a QUIC session over peer-to-peer transport
// specified by |socket|. There is no handshake performed by this class, i.e.
// the handshake is expected to happen out-of-bound before QuicP2PSession is
// created. The out-of-bound handshake should generate the following parameters
// passed to the constructor:
//   1. Negotiated QuicConfig.
//   2. Secret key exchanged with the peer on the other end of the |socket|.
//      This key is passed to the constructor as part of the |crypto_config|.
class NET_EXPORT QuicP2PSession : public QuicSession {
 public:
  class Delegate {
   public:
    virtual void OnIncomingStream(QuicP2PStream* stream) = 0;

    virtual void OnConnectionClosed(QuicErrorCode error) = 0;
  };

  // |config| must be already negotiated. |crypto_config| contains a secret key
  // shared with the peer.
  QuicP2PSession(const QuicConfig& config,
                 const QuicP2PCryptoConfig& crypto_config,
                 std::unique_ptr<QuicConnection> connection,
                 std::unique_ptr<Socket> socket);
  ~QuicP2PSession() override;

  // QuicSession overrides.
  void Initialize() override;
  QuicP2PStream* CreateOutgoingDynamicStream(
      net::SpdyPriority priority) override;

  // QuicConnectionVisitorInterface overrides.
  void OnConnectionClosed(QuicErrorCode error,
                          const std::string& error_details,
                          ConnectionCloseSource source) override;

  void SetDelegate(Delegate* delegate);

 protected:
  // QuicSession overrides.
  QuicCryptoStream* GetCryptoStream() override;
  QuicP2PStream* CreateIncomingDynamicStream(QuicStreamId id) override;

  QuicP2PSession* CreateDataStream(QuicStreamId id);

 private:
  enum ReadState {
    READ_STATE_DO_READ,
    READ_STATE_DO_READ_COMPLETE,
  };

  void DoReadLoop(int result);
  int DoRead();
  int DoReadComplete(int result);

  std::unique_ptr<Socket> socket_;
  std::unique_ptr<QuicP2PCryptoStream> crypto_stream_;

  Delegate* delegate_ = nullptr;

  ReadState read_state_ = READ_STATE_DO_READ;
  scoped_refptr<IOBuffer> read_buffer_;

  // For recording receipt time
  QuicClock clock_;

  DISALLOW_COPY_AND_ASSIGN(QuicP2PSession);
};

}  // namespace net

#endif  // NET_QUIC_P2P_QUIC_P2P_SESSION_H_
