// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// NOTE: This code is not shared between Google and Chrome.

#ifndef NET_QUIC_QUIC_RELIABLE_CLIENT_STREAM_H_
#define NET_QUIC_QUIC_RELIABLE_CLIENT_STREAM_H_

#include "net/base/ip_endpoint.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/http/http_stream.h"
#include "net/quic/quic_data_stream.h"

namespace net {

class QuicClientSession;

// A client-initiated ReliableQuicStream.  Instances of this class
// are owned by the QuicClientSession which created them.
class NET_EXPORT_PRIVATE QuicReliableClientStream : public QuicDataStream {
 public:
  // Delegate handles protocol specific behavior of a quic stream.
  class NET_EXPORT_PRIVATE Delegate {
   public:
    Delegate() {}

    // Called when data is received.
    // Returns network error code. OK when it successfully receives data.
    virtual int OnDataReceived(const char* data, int length) = 0;

    // Called when the stream is closed by the peer.
    virtual void OnClose(QuicErrorCode error) = 0;

    // Called when the stream is closed because of an error.
    virtual void OnError(int error) = 0;

    // Returns true if sending of headers has completed.
    virtual bool HasSendHeadersComplete() = 0;

   protected:
    virtual ~Delegate() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  QuicReliableClientStream(QuicStreamId id,
                           QuicSession* session,
                           const BoundNetLog& net_log);

  virtual ~QuicReliableClientStream();

  // QuicDataStream
  virtual uint32 ProcessData(const char* data, uint32 data_len) OVERRIDE;
  virtual void OnFinRead() OVERRIDE;
  virtual void OnCanWrite() OVERRIDE;
  virtual QuicPriority EffectivePriority() const OVERRIDE;

  // While the server's set_priority shouldn't be called externally, the creator
  // of client-side streams should be able to set the priority.
  using QuicDataStream::set_priority;

  int WriteStreamData(base::StringPiece data,
                      bool fin,
                      const CompletionCallback& callback);
  // Set new |delegate|. |delegate| must not be NULL.
  // If this stream has already received data, OnDataReceived() will be
  // called on the delegate.
  void SetDelegate(Delegate* delegate);
  Delegate* GetDelegate() { return delegate_; }
  void OnError(int error);

  // Returns true if the stream can possible write data.  (The socket may
  // turn out to be write blocked, of course).  If the stream can not write,
  // this method returns false, and |callback| will be invoked when
  // it becomes writable.
  bool CanWrite(const CompletionCallback& callback);

  const BoundNetLog& net_log() const { return net_log_; }

  using QuicDataStream::HasBufferedData;

 private:
  BoundNetLog net_log_;
  Delegate* delegate_;

  CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(QuicReliableClientStream);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_RELIABLE_CLIENT_STREAM_H_
