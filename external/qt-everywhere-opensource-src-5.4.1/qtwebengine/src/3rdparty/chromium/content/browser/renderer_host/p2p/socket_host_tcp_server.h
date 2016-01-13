// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TCP_SERVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TCP_SERVER_H_

#include <map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/renderer_host/p2p/socket_host.h"
#include "content/common/content_export.h"
#include "content/common/p2p_socket_type.h"
#include "ipc/ipc_sender.h"
#include "net/base/completion_callback.h"
#include "net/socket/tcp_server_socket.h"

namespace net {
class StreamSocket;
}  // namespace net

namespace content {

class CONTENT_EXPORT P2PSocketHostTcpServer : public P2PSocketHost {
 public:
  typedef std::map<net::IPEndPoint, net::StreamSocket*> AcceptedSocketsMap;

  P2PSocketHostTcpServer(IPC::Sender* message_sender,
                         int socket_id,
                         P2PSocketType client_type);
  virtual ~P2PSocketHostTcpServer();

  // P2PSocketHost overrides.
  virtual bool Init(const net::IPEndPoint& local_address,
                    const P2PHostAndIPEndPoint& remote_address) OVERRIDE;
  virtual void Send(const net::IPEndPoint& to,
                    const std::vector<char>& data,
                    const talk_base::PacketOptions& options,
                    uint64 packet_id) OVERRIDE;
  virtual P2PSocketHost* AcceptIncomingTcpConnection(
      const net::IPEndPoint& remote_address, int id) OVERRIDE;
  virtual bool SetOption(P2PSocketOption option, int value) OVERRIDE;


 private:
  friend class P2PSocketHostTcpServerTest;

  void OnError();

  void DoAccept();
  void HandleAcceptResult(int result);

  // Callback for Accept().
  void OnAccepted(int result);

  const P2PSocketType client_type_;
  scoped_ptr<net::ServerSocket> socket_;
  net::IPEndPoint local_address_;

  scoped_ptr<net::StreamSocket> accept_socket_;
  AcceptedSocketsMap accepted_sockets_;

  net::CompletionCallback accept_callback_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketHostTcpServer);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TCP_SERVER_H_
