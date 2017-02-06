// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/tethering_handler.h"

#include "base/stl_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/socket/server_socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_server_socket.h"

namespace content {
namespace devtools {
namespace tethering {

namespace {

const int kListenBacklog = 5;
const int kBufferSize = 16 * 1024;

const int kMinTetheringPort = 1024;
const int kMaxTetheringPort = 32767;

using Response = DevToolsProtocolClient::Response;
using CreateServerSocketCallback =
    base::Callback<std::unique_ptr<net::ServerSocket>(std::string*)>;

class SocketPump {
 public:
  SocketPump(net::StreamSocket* client_socket)
      : client_socket_(client_socket),
        pending_writes_(0),
        pending_destruction_(false) {
  }

  std::string Init(const CreateServerSocketCallback& socket_callback) {
    std::string channel_name;
    server_socket_ = socket_callback.Run(&channel_name);
    if (!server_socket_.get() || channel_name.empty())
      SelfDestruct();

    int result = server_socket_->Accept(
        &accepted_socket_,
        base::Bind(&SocketPump::OnAccepted, base::Unretained(this)));
    if (result != net::ERR_IO_PENDING)
      OnAccepted(result);
    return channel_name;
  }

 private:
  void OnAccepted(int result) {
    if (result < 0) {
      SelfDestruct();
      return;
    }

    ++pending_writes_; // avoid SelfDestruct in first Pump
    Pump(client_socket_.get(), accepted_socket_.get());
    --pending_writes_;
    if (pending_destruction_) {
      SelfDestruct();
    } else {
      Pump(accepted_socket_.get(), client_socket_.get());
    }
  }

  void Pump(net::StreamSocket* from, net::StreamSocket* to) {
    scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(kBufferSize);
    int result = from->Read(
        buffer.get(),
        kBufferSize,
        base::Bind(
            &SocketPump::OnRead, base::Unretained(this), from, to, buffer));
    if (result != net::ERR_IO_PENDING)
      OnRead(from, to, buffer, result);
  }

  void OnRead(net::StreamSocket* from,
              net::StreamSocket* to,
              scoped_refptr<net::IOBuffer> buffer,
              int result) {
    if (result <= 0) {
      SelfDestruct();
      return;
    }

    int total = result;
    scoped_refptr<net::DrainableIOBuffer> drainable =
        new net::DrainableIOBuffer(buffer.get(), total);

    ++pending_writes_;
    result = to->Write(drainable.get(),
                       total,
                       base::Bind(&SocketPump::OnWritten,
                                  base::Unretained(this),
                                  drainable,
                                  from,
                                  to));
    if (result != net::ERR_IO_PENDING)
      OnWritten(drainable, from, to, result);
  }

  void OnWritten(scoped_refptr<net::DrainableIOBuffer> drainable,
                 net::StreamSocket* from,
                 net::StreamSocket* to,
                 int result) {
    --pending_writes_;
    if (result < 0) {
      SelfDestruct();
      return;
    }

    drainable->DidConsume(result);
    if (drainable->BytesRemaining() > 0) {
      ++pending_writes_;
      result = to->Write(drainable.get(),
                         drainable->BytesRemaining(),
                         base::Bind(&SocketPump::OnWritten,
                                    base::Unretained(this),
                                    drainable,
                                    from,
                                    to));
      if (result != net::ERR_IO_PENDING)
        OnWritten(drainable, from, to, result);
      return;
    }

    if (pending_destruction_) {
      SelfDestruct();
      return;
    }
    Pump(from, to);
  }

  void SelfDestruct() {
    if (pending_writes_ > 0) {
      pending_destruction_ = true;
      return;
    }
    delete this;
  }


 private:
  std::unique_ptr<net::StreamSocket> client_socket_;
  std::unique_ptr<net::ServerSocket> server_socket_;
  std::unique_ptr<net::StreamSocket> accepted_socket_;
  int pending_writes_;
  bool pending_destruction_;
};

class BoundSocket {
 public:
  typedef base::Callback<void(uint16_t, const std::string&)> AcceptedCallback;

  BoundSocket(AcceptedCallback accepted_callback,
              const CreateServerSocketCallback& socket_callback)
      : accepted_callback_(accepted_callback),
        socket_callback_(socket_callback),
        socket_(new net::TCPServerSocket(NULL, net::NetLog::Source())),
        port_(0) {
  }

  virtual ~BoundSocket() {
  }

  bool Listen(uint16_t port) {
    port_ = port;
    net::IPEndPoint end_point(net::IPAddress::IPv4Localhost(), port);
    int result = socket_->Listen(end_point, kListenBacklog);
    if (result < 0)
      return false;

    net::IPEndPoint local_address;
    result = socket_->GetLocalAddress(&local_address);
    if (result < 0)
      return false;

    DoAccept();
    return true;
  }

 private:
  typedef std::map<net::IPEndPoint, net::StreamSocket*> AcceptedSocketsMap;

  void DoAccept() {
    while (true) {
      int result = socket_->Accept(
          &accept_socket_,
          base::Bind(&BoundSocket::OnAccepted, base::Unretained(this)));
      if (result == net::ERR_IO_PENDING)
        break;
      else
        HandleAcceptResult(result);
    }
  }

  void OnAccepted(int result) {
    HandleAcceptResult(result);
    if (result == net::OK)
      DoAccept();
  }

  void HandleAcceptResult(int result) {
    if (result != net::OK)
      return;

    SocketPump* pump = new SocketPump(accept_socket_.release());
    std::string name = pump->Init(socket_callback_);
    if (!name.empty())
      accepted_callback_.Run(port_, name);
  }

  AcceptedCallback accepted_callback_;
  CreateServerSocketCallback socket_callback_;
  std::unique_ptr<net::ServerSocket> socket_;
  std::unique_ptr<net::StreamSocket> accept_socket_;
  uint16_t port_;
};

}  // namespace

// TetheringHandler::TetheringImpl -------------------------------------------

class TetheringHandler::TetheringImpl {
 public:
  TetheringImpl(
      base::WeakPtr<TetheringHandler> handler,
      const CreateServerSocketCallback& socket_callback);
  ~TetheringImpl();

  void Bind(DevToolsCommandId command_id, uint16_t port);
  void Unbind(DevToolsCommandId command_id, uint16_t port);
  void Accepted(uint16_t port, const std::string& name);

 private:
  void SendInternalError(DevToolsCommandId command_id,
                         const std::string& message);

  base::WeakPtr<TetheringHandler> handler_;
  CreateServerSocketCallback socket_callback_;

  typedef std::map<uint16_t, BoundSocket*> BoundSockets;
  BoundSockets bound_sockets_;
};

TetheringHandler::TetheringImpl::TetheringImpl(
    base::WeakPtr<TetheringHandler> handler,
    const CreateServerSocketCallback& socket_callback)
    : handler_(handler),
      socket_callback_(socket_callback) {
}

TetheringHandler::TetheringImpl::~TetheringImpl() {
  STLDeleteValues(&bound_sockets_);
}

void TetheringHandler::TetheringImpl::Bind(DevToolsCommandId command_id,
                                           uint16_t port) {
  if (bound_sockets_.find(port) != bound_sockets_.end()) {
    SendInternalError(command_id, "Port already bound");
    return;
  }

  BoundSocket::AcceptedCallback callback = base::Bind(
      &TetheringHandler::TetheringImpl::Accepted, base::Unretained(this));
  std::unique_ptr<BoundSocket> bound_socket(
      new BoundSocket(callback, socket_callback_));
  if (!bound_socket->Listen(port)) {
    SendInternalError(command_id, "Could not bind port");
    return;
  }

  bound_sockets_[port] = bound_socket.release();
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&TetheringHandler::SendBindSuccess, handler_, command_id));
}

void TetheringHandler::TetheringImpl::Unbind(DevToolsCommandId command_id,
                                             uint16_t port) {
  BoundSockets::iterator it = bound_sockets_.find(port);
  if (it == bound_sockets_.end()) {
    SendInternalError(command_id, "Port is not bound");
    return;
  }

  delete it->second;
  bound_sockets_.erase(it);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&TetheringHandler::SendUnbindSuccess, handler_, command_id));
}

void TetheringHandler::TetheringImpl::Accepted(uint16_t port,
                                               const std::string& name) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&TetheringHandler::Accepted, handler_, port, name));
}

void TetheringHandler::TetheringImpl::SendInternalError(
    DevToolsCommandId command_id,
    const std::string& message) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&TetheringHandler::SendInternalError, handler_,
                 command_id, message));
}


// TetheringHandler ----------------------------------------------------------

// static
TetheringHandler::TetheringImpl* TetheringHandler::impl_ = nullptr;

TetheringHandler::TetheringHandler(
    const CreateServerSocketCallback& socket_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : socket_callback_(socket_callback),
      task_runner_(task_runner),
      is_active_(false),
      weak_factory_(this) {
}

TetheringHandler::~TetheringHandler() {
  if (is_active_) {
    task_runner_->DeleteSoon(FROM_HERE, impl_);
    impl_ = nullptr;
  }
}

void TetheringHandler::SetClient(std::unique_ptr<Client> client) {
  client_.swap(client);
}

void TetheringHandler::Accepted(uint16_t port, const std::string& name) {
  client_->Accepted(AcceptedParams::Create()->set_port(port)
                                            ->set_connection_id(name));
}

bool TetheringHandler::Activate() {
  if (is_active_)
    return true;
  if (impl_)
    return false;
  is_active_ = true;
  impl_ = new TetheringImpl(weak_factory_.GetWeakPtr(), socket_callback_);
  return true;
}

Response TetheringHandler::Bind(DevToolsCommandId command_id, int port) {
  if (port < kMinTetheringPort || port > kMaxTetheringPort)
    return Response::InvalidParams("port");

  if (!Activate())
    return Response::ServerError("Tethering is used by another connection");

  DCHECK(impl_);
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&TetheringImpl::Bind, base::Unretained(impl_),
                            command_id, port));
  return Response::OK();
}

Response TetheringHandler::Unbind(DevToolsCommandId command_id, int port) {
  if (!Activate())
    return Response::ServerError("Tethering is used by another connection");

  DCHECK(impl_);
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&TetheringImpl::Unbind, base::Unretained(impl_),
                            command_id, port));
  return Response::OK();
}

void TetheringHandler::SendBindSuccess(DevToolsCommandId command_id) {
  client_->SendBindResponse(command_id, BindResponse::Create());
}

void TetheringHandler::SendUnbindSuccess(DevToolsCommandId command_id) {
  client_->SendUnbindResponse(command_id, UnbindResponse::Create());
}

void TetheringHandler::SendInternalError(DevToolsCommandId command_id,
                                         const std::string& message) {
  client_->SendError(command_id, Response::InternalError(message));
}

}  // namespace tethering
}  // namespace devtools
}  // namespace content
