// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_GLUE_PSEUDOTCP_ADAPTER_H_
#define JINGLE_GLUE_PSEUDOTCP_ADAPTER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer/timer.h"
#include "net/base/net_log.h"
#include "net/socket/stream_socket.h"
#include "third_party/libjingle/source/talk/p2p/base/pseudotcp.h"

namespace jingle_glue {

// PseudoTcpAdapter adapts a connectionless net::Socket to a connection-
// oriented net::StreamSocket using PseudoTcp.  Because net::StreamSockets
// can be deleted during callbacks, while PseudoTcp cannot, the core of the
// PseudoTcpAdapter is reference counted, with a reference held by the
// adapter, and an additional reference held on the stack during callbacks.
class PseudoTcpAdapter : public net::StreamSocket, base::NonThreadSafe {
 public:
  // Creates an adapter for the supplied Socket.  |socket| should already
  // be ready for use, and ownership of it will be assumed by the adapter.
  PseudoTcpAdapter(net::Socket* socket);
  virtual ~PseudoTcpAdapter();

  // net::Socket implementation.
  virtual int Read(net::IOBuffer* buffer, int buffer_size,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual int Write(net::IOBuffer* buffer, int buffer_size,
                    const net::CompletionCallback& callback) OVERRIDE;
  virtual int SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual int SetSendBufferSize(int32 size) OVERRIDE;

  // net::StreamSocket implementation.
  virtual int Connect(const net::CompletionCallback& callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectedAndIdle() const OVERRIDE;
  virtual int GetPeerAddress(net::IPEndPoint* address) const OVERRIDE;
  virtual int GetLocalAddress(net::IPEndPoint* address) const OVERRIDE;
  virtual const net::BoundNetLog& NetLog() const OVERRIDE;
  virtual void SetSubresourceSpeculation() OVERRIDE;
  virtual void SetOmniboxSpeculation() OVERRIDE;
  virtual bool WasEverUsed() const OVERRIDE;
  virtual bool UsingTCPFastOpen() const OVERRIDE;
  virtual bool WasNpnNegotiated() const OVERRIDE;
  virtual net::NextProto GetNegotiatedProtocol() const OVERRIDE;
  virtual bool GetSSLInfo(net::SSLInfo* ssl_info) OVERRIDE;

  // Set the delay for sending ACK.
  void SetAckDelay(int delay_ms);

  // Set whether Nagle's algorithm is enabled.
  void SetNoDelay(bool no_delay);

  // When write_waits_for_send flag is set to true the Write() method
  // will wait until the data is sent to the remote end before the
  // write completes (it still doesn't wait until the data is received
  // and acknowledged by the remote end). Otherwise write completes
  // after the data has been copied to the send buffer.
  //
  // This flag is useful in cases when the sender needs to get
  // feedback from the connection when it is congested. E.g. remoting
  // host uses this feature to adjust screen capturing rate according
  // to the available bandwidth. In the same time it may negatively
  // impact performance in some cases. E.g. when the sender writes one
  // byte at a time then each byte will always be sent in a separate
  // packet.
  //
  // TODO(sergeyu): Remove this flag once remoting has a better
  // flow-control solution.
  void SetWriteWaitsForSend(bool write_waits_for_send);

 private:
  class Core;

  scoped_refptr<Core> core_;

  net::BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(PseudoTcpAdapter);
};

}  // namespace jingle_glue

#endif  // JINGLE_GLUE_STREAM_SOCKET_ADAPTER_H_
