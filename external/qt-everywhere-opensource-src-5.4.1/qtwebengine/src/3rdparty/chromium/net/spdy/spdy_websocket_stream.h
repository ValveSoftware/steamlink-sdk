// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_WEBSOCKET_STREAM_H_
#define NET_SPDY_SPDY_WEBSOCKET_STREAM_H_

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/completion_callback.h"
#include "net/base/request_priority.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_header_block.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_stream.h"

namespace net {

// The SpdyWebSocketStream is a WebSocket-specific type of stream known to a
// SpdySession. WebSocket's opening handshake is converted to SPDY's
// SYN_STREAM/SYN_REPLY. WebSocket frames are encapsulated as SPDY data frames.
class NET_EXPORT_PRIVATE SpdyWebSocketStream
    : public SpdyStream::Delegate {
 public:
  // Delegate handles asynchronous events.
  class NET_EXPORT_PRIVATE Delegate {
   public:
    // Called when InitializeStream() finishes asynchronously. This delegate is
    // called if InitializeStream() returns ERR_IO_PENDING. |status| indicates
    // network error.
    virtual void OnCreatedSpdyStream(int status) = 0;

    // Called on corresponding to OnSendHeadersComplete() or SPDY's SYN frame
    // has been sent.
    virtual void OnSentSpdyHeaders() = 0;

    // Called on corresponding to OnResponseHeadersUpdated() or
    // SPDY's SYN_STREAM, SYN_REPLY, or HEADERS frames are
    // received. This callback may be called multiple times as SPDY's
    // delegate does.
    virtual void OnSpdyResponseHeadersUpdated(
        const SpdyHeaderBlock& response_headers) = 0;

    // Called when data is sent.
    virtual void OnSentSpdyData(size_t bytes_sent) = 0;

    // Called when data is received.
    virtual void OnReceivedSpdyData(scoped_ptr<SpdyBuffer> buffer) = 0;

    // Called when SpdyStream is closed.
    virtual void OnCloseSpdyStream() = 0;

   protected:
    virtual ~Delegate() {}
  };

  SpdyWebSocketStream(const base::WeakPtr<SpdySession>& spdy_session,
                      Delegate* delegate);
  virtual ~SpdyWebSocketStream();

  // Initializes SPDY stream for the WebSocket.
  // It might create SPDY stream asynchronously.  In this case, this method
  // returns ERR_IO_PENDING and call OnCreatedSpdyStream delegate with result
  // after completion. In other cases, delegate does not be called.
  int InitializeStream(const GURL& url,
                       RequestPriority request_priority,
                       const BoundNetLog& stream_net_log);

  int SendRequest(scoped_ptr<SpdyHeaderBlock> headers);
  int SendData(const char* data, int length);
  void Close();

  // SpdyStream::Delegate
  virtual void OnRequestHeadersSent() OVERRIDE;
  virtual SpdyResponseHeadersStatus OnResponseHeadersUpdated(
      const SpdyHeaderBlock& response_headers) OVERRIDE;
  virtual void OnDataReceived(scoped_ptr<SpdyBuffer> buffer) OVERRIDE;
  virtual void OnDataSent() OVERRIDE;
  virtual void OnClose(int status) OVERRIDE;

 private:
  friend class SpdyWebSocketStreamTest;
  FRIEND_TEST_ALL_PREFIXES(SpdyWebSocketStreamTest, Basic);

  void OnSpdyStreamCreated(int status);

  SpdyStreamRequest stream_request_;
  base::WeakPtr<SpdyStream> stream_;
  const base::WeakPtr<SpdySession> spdy_session_;
  size_t pending_send_data_length_;
  Delegate* delegate_;

  base::WeakPtrFactory<SpdyWebSocketStream> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SpdyWebSocketStream);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_WEBSOCKET_STREAM_H_
