// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_P2P_QUIC_P2P_STREAM_H_
#define NET_QUIC_P2P_QUIC_P2P_STREAM_H_

#include "base/macros.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/quic/reliable_quic_stream.h"

namespace net {

// Streams created by QuicP2PSession.
class NET_EXPORT QuicP2PStream : public ReliableQuicStream {
 public:
  // Delegate handles protocol specific behavior of a quic stream.
  class NET_EXPORT Delegate {
   public:
    Delegate() {}

    // Called when data is received.
    virtual void OnDataReceived(const char* data, int length) = 0;

    // Called when the stream is closed by the peer.
    virtual void OnClose(QuicErrorCode error) = 0;

   protected:
    virtual ~Delegate() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  QuicP2PStream(QuicStreamId id, QuicSession* session);

  ~QuicP2PStream() override;

  // ReliableQuicStream overrides.
  void OnDataAvailable() override;
  void OnClose() override;
  void OnCanWrite() override;

  void WriteHeader(base::StringPiece data);

  int Write(base::StringPiece data, const CompletionCallback& callback);

  void SetDelegate(Delegate* delegate);
  Delegate* GetDelegate() { return delegate_; }

 private:
  Delegate* delegate_ = nullptr;

  CompletionCallback write_callback_;
  int last_write_size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(QuicP2PStream);
};

}  // namespace net

#endif  // NET_QUIC_P2P_QUIC_P2P_STREAM_H_
