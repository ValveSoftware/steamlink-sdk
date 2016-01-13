// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SOCKET_STREAM_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_SOCKET_STREAM_HOST_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "net/socket_stream/socket_stream.h"

class GURL;

namespace net {
class SocketStreamJob;
class URLRequestContext;
class SSLInfo;
}  // namespace net

namespace content {

// Host of SocketStreamHandle.  Each SocketStreamHandle will have an unique
// socket_id assigned by SocketStreamHost constructor.  If socket id is
// kNoSocketId, there is no SocketStreamHost.  Each SocketStreamHost has
// SocketStream to manage bi-directional communication over socket stream.  The
// lifetime of an instance of this class is completely controlled by the
// SocketStreamDispatcherHost.
class SocketStreamHost {
 public:
  SocketStreamHost(net::SocketStream::Delegate* delegate,
                   int child_id,
                   int render_frame_id,
                   int socket_id);
  ~SocketStreamHost();

  // Gets socket_id associated with |socket|.
  static int SocketIdFromSocketStream(const net::SocketStream* socket);

  int render_frame_id() const { return render_frame_id_; }
  int socket_id() const { return socket_id_; }

  // Starts to open connection to |url|.
  void Connect(const GURL& url, net::URLRequestContext* request_context);

  // Sends |data| over the socket stream.
  // socket stream must be open to send data.
  // Returns true if the data is put in transmit buffer in socket stream.
  // Returns false otherwise (transmit buffer exceeds limit, or socket
  // stream is closed).
  bool SendData(const std::vector<char>& data);

  // Closes the socket stream.
  void Close();

  // Following CancelWithError, CancelWithSSLError, and ContinueDespiteError
  // will be called by net::SocketStream::Delegate in OnSSLCertificateError.
  // CancelWithError Cancels the connection because of an error.
  // |error| is net::Error which represents the error.
  void CancelWithError(int error);

  // Cancels the connection because of receiving a certificate with an error.
  void CancelWithSSLError(const net::SSLInfo& ssl_info);

  // Continue to establish the connection in spite of an error.
  void ContinueDespiteError();

 private:
  net::SocketStream::Delegate* delegate_;
  int child_id_;
  int render_frame_id_;
  int socket_id_;

  scoped_refptr<net::SocketStreamJob> job_;

  DISALLOW_COPY_AND_ASSIGN(SocketStreamHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SOCKET_STREAM_HOST_H_
