// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/socket_host_tcp.h"

#include "base/sys_byteorder.h"
#include "content/common/p2p_messages.h"
#include "ipc/ipc_sender.h"
#include "jingle/glue/fake_ssl_client_socket.h"
#include "jingle/glue/proxy_resolving_client_socket.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/libjingle/source/talk/base/asyncpacketsocket.h"

namespace {

typedef uint16 PacketLength;
const int kPacketHeaderSize = sizeof(PacketLength);
const int kReadBufferSize = 4096;
const int kPacketLengthOffset = 2;
const int kTurnChannelDataHeaderSize = 4;
const int kRecvSocketBufferSize = 128 * 1024;
const int kSendSocketBufferSize = 128 * 1024;

bool IsTlsClientSocket(content::P2PSocketType type) {
  return (type == content::P2P_SOCKET_STUN_TLS_CLIENT ||
          type == content::P2P_SOCKET_TLS_CLIENT);
}

bool IsPseudoTlsClientSocket(content::P2PSocketType type) {
  return (type == content::P2P_SOCKET_SSLTCP_CLIENT ||
          type == content::P2P_SOCKET_STUN_SSLTCP_CLIENT);
}

}  // namespace

namespace content {

P2PSocketHostTcpBase::P2PSocketHostTcpBase(
    IPC::Sender* message_sender,
    int socket_id,
    P2PSocketType type,
    net::URLRequestContextGetter* url_context)
    : P2PSocketHost(message_sender, socket_id),
      write_pending_(false),
      connected_(false),
      type_(type),
      url_context_(url_context) {
}

P2PSocketHostTcpBase::~P2PSocketHostTcpBase() {
  if (state_ == STATE_OPEN) {
    DCHECK(socket_.get());
    socket_.reset();
  }
}

bool P2PSocketHostTcpBase::InitAccepted(const net::IPEndPoint& remote_address,
                                        net::StreamSocket* socket) {
  DCHECK(socket);
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  remote_address_.ip_address = remote_address;
  // TODO(ronghuawu): Add FakeSSLServerSocket.
  socket_.reset(socket);
  state_ = STATE_OPEN;
  DoRead();
  return state_ != STATE_ERROR;
}

bool P2PSocketHostTcpBase::Init(const net::IPEndPoint& local_address,
                                const P2PHostAndIPEndPoint& remote_address) {
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  remote_address_ = remote_address;
  state_ = STATE_CONNECTING;

  net::HostPortPair dest_host_port_pair =
      net::HostPortPair::FromIPEndPoint(remote_address.ip_address);
  // TODO(mallinath) - We are ignoring local_address altogether. We should
  // find a way to inject this into ProxyResolvingClientSocket. This could be
  // a problem on multi-homed host.

  // The default SSLConfig is good enough for us for now.
  const net::SSLConfig ssl_config;
  socket_.reset(new jingle_glue::ProxyResolvingClientSocket(
                    NULL,     // Default socket pool provided by the net::Proxy.
                    url_context_,
                    ssl_config,
                    dest_host_port_pair));

  int status = socket_->Connect(
      base::Bind(&P2PSocketHostTcpBase::OnConnected,
                 base::Unretained(this)));
  if (status != net::ERR_IO_PENDING) {
    // We defer execution of ProcessConnectDone instead of calling it
    // directly here as the caller may not expect an error/close to
    // happen here.  This is okay, as from the caller's point of view,
    // the connect always happens asynchronously.
    base::MessageLoop* message_loop = base::MessageLoop::current();
    CHECK(message_loop);
    message_loop->PostTask(
        FROM_HERE,
        base::Bind(&P2PSocketHostTcpBase::OnConnected,
                   base::Unretained(this), status));
  }

  return state_ != STATE_ERROR;
}

void P2PSocketHostTcpBase::OnError() {
  socket_.reset();

  if (state_ == STATE_UNINITIALIZED || state_ == STATE_CONNECTING ||
      state_ == STATE_TLS_CONNECTING || state_ == STATE_OPEN) {
    message_sender_->Send(new P2PMsg_OnError(id_));
  }

  state_ = STATE_ERROR;
}

void P2PSocketHostTcpBase::OnConnected(int result) {
  DCHECK_EQ(state_, STATE_CONNECTING);
  DCHECK_NE(result, net::ERR_IO_PENDING);

  if (result != net::OK) {
    OnError();
    return;
  }

  if (IsTlsClientSocket(type_)) {
    state_ = STATE_TLS_CONNECTING;
    StartTls();
  } else if (IsPseudoTlsClientSocket(type_)) {
    scoped_ptr<net::StreamSocket> transport_socket = socket_.Pass();
    socket_.reset(
        new jingle_glue::FakeSSLClientSocket(transport_socket.Pass()));
    state_ = STATE_TLS_CONNECTING;
    int status = socket_->Connect(
        base::Bind(&P2PSocketHostTcpBase::ProcessTlsSslConnectDone,
                   base::Unretained(this)));
    if (status != net::ERR_IO_PENDING) {
      ProcessTlsSslConnectDone(status);
    }
  } else {
    // If we are not doing TLS, we are ready to send data now.
    // In case of TLS, SignalConnect will be sent only after TLS handshake is
    // successfull. So no buffering will be done at socket handlers if any
    // packets sent before that by the application.
    OnOpen();
  }
}

void P2PSocketHostTcpBase::StartTls() {
  DCHECK_EQ(state_, STATE_TLS_CONNECTING);
  DCHECK(socket_.get());

  scoped_ptr<net::ClientSocketHandle> socket_handle(
      new net::ClientSocketHandle());
  socket_handle->SetSocket(socket_.Pass());

  net::SSLClientSocketContext context;
  context.cert_verifier = url_context_->GetURLRequestContext()->cert_verifier();
  context.transport_security_state =
      url_context_->GetURLRequestContext()->transport_security_state();
  DCHECK(context.transport_security_state);

  // Default ssl config.
  const net::SSLConfig ssl_config;
  net::HostPortPair dest_host_port_pair =
      net::HostPortPair::FromIPEndPoint(remote_address_.ip_address);
  if (!remote_address_.hostname.empty())
    dest_host_port_pair.set_host(remote_address_.hostname);

  net::ClientSocketFactory* socket_factory =
      net::ClientSocketFactory::GetDefaultFactory();
  DCHECK(socket_factory);

  socket_ = socket_factory->CreateSSLClientSocket(
      socket_handle.Pass(), dest_host_port_pair, ssl_config, context);
  int status = socket_->Connect(
      base::Bind(&P2PSocketHostTcpBase::ProcessTlsSslConnectDone,
                 base::Unretained(this)));
  if (status != net::ERR_IO_PENDING) {
    ProcessTlsSslConnectDone(status);
  }
}

void P2PSocketHostTcpBase::ProcessTlsSslConnectDone(int status) {
  DCHECK_NE(status, net::ERR_IO_PENDING);
  DCHECK_EQ(state_, STATE_TLS_CONNECTING);
  if (status != net::OK) {
    OnError();
    return;
  }
  OnOpen();
}

void P2PSocketHostTcpBase::OnOpen() {
  state_ = STATE_OPEN;
  // Setting socket send and receive buffer size.
  if (net::OK != socket_->SetReceiveBufferSize(kRecvSocketBufferSize)) {
    LOG(WARNING) << "Failed to set socket receive buffer size to "
                 << kRecvSocketBufferSize;
  }

  if (net::OK != socket_->SetSendBufferSize(kSendSocketBufferSize)) {
    LOG(WARNING) << "Failed to set socket send buffer size to "
                 << kSendSocketBufferSize;
  }

  DoSendSocketCreateMsg();
  DoRead();
}

void P2PSocketHostTcpBase::DoSendSocketCreateMsg() {
  DCHECK(socket_.get());

  net::IPEndPoint address;
  int result = socket_->GetLocalAddress(&address);
  if (result < 0) {
    LOG(ERROR) << "P2PSocketHostTcpBase::OnConnected: unable to get local"
               << " address: " << result;
    OnError();
    return;
  }

  VLOG(1) << "Local address: " << address.ToString();

  // If we are not doing TLS, we are ready to send data now.
  // In case of TLS SignalConnect will be sent only after TLS handshake is
  // successfull. So no buffering will be done at socket handlers if any
  // packets sent before that by the application.
  message_sender_->Send(new P2PMsg_OnSocketCreated(id_, address));
}

void P2PSocketHostTcpBase::DoRead() {
  int result;
  do {
    if (!read_buffer_.get()) {
      read_buffer_ = new net::GrowableIOBuffer();
      read_buffer_->SetCapacity(kReadBufferSize);
    } else if (read_buffer_->RemainingCapacity() < kReadBufferSize) {
      // Make sure that we always have at least kReadBufferSize of
      // remaining capacity in the read buffer. Normally all packets
      // are smaller than kReadBufferSize, so this is not really
      // required.
      read_buffer_->SetCapacity(read_buffer_->capacity() + kReadBufferSize -
                                read_buffer_->RemainingCapacity());
    }
    result = socket_->Read(
        read_buffer_.get(),
        read_buffer_->RemainingCapacity(),
        base::Bind(&P2PSocketHostTcp::OnRead, base::Unretained(this)));
    DidCompleteRead(result);
  } while (result > 0);
}

void P2PSocketHostTcpBase::OnRead(int result) {
  DidCompleteRead(result);
  if (state_ == STATE_OPEN) {
    DoRead();
  }
}

void P2PSocketHostTcpBase::OnPacket(const std::vector<char>& data) {
  if (!connected_) {
    P2PSocketHost::StunMessageType type;
    bool stun = GetStunPacketType(&*data.begin(), data.size(), &type);
    if (stun && IsRequestOrResponse(type)) {
      connected_ = true;
    } else if (!stun || type == STUN_DATA_INDICATION) {
      LOG(ERROR) << "Received unexpected data packet from "
                 << remote_address_.ip_address.ToString()
                 << " before STUN binding is finished. "
                 << "Terminating connection.";
      OnError();
      return;
    }
  }

  message_sender_->Send(new P2PMsg_OnDataReceived(
      id_, remote_address_.ip_address, data, base::TimeTicks::Now()));

  if (dump_incoming_rtp_packet_)
    DumpRtpPacket(&data[0], data.size(), true);
}

// Note: dscp is not actually used on TCP sockets as this point,
// but may be honored in the future.
void P2PSocketHostTcpBase::Send(const net::IPEndPoint& to,
                                const std::vector<char>& data,
                                const talk_base::PacketOptions& options,
                                uint64 packet_id) {
  if (!socket_) {
    // The Send message may be sent after the an OnError message was
    // sent by hasn't been processed the renderer.
    return;
  }

  if (!(to == remote_address_.ip_address)) {
    // Renderer should use this socket only to send data to |remote_address_|.
    NOTREACHED();
    OnError();
    return;
  }

  if (!connected_) {
    P2PSocketHost::StunMessageType type = P2PSocketHost::StunMessageType();
    bool stun = GetStunPacketType(&*data.begin(), data.size(), &type);
    if (!stun || type == STUN_DATA_INDICATION) {
      LOG(ERROR) << "Page tried to send a data packet to " << to.ToString()
                 << " before STUN binding is finished.";
      OnError();
      return;
    }
  }

  DoSend(to, data, options);
}

void P2PSocketHostTcpBase::WriteOrQueue(
    scoped_refptr<net::DrainableIOBuffer>& buffer) {
  if (write_buffer_.get()) {
    write_queue_.push(buffer);
    return;
  }

  write_buffer_ = buffer;
  DoWrite();
}

void P2PSocketHostTcpBase::DoWrite() {
  while (write_buffer_.get() && state_ == STATE_OPEN && !write_pending_) {
    int result = socket_->Write(
        write_buffer_.get(),
        write_buffer_->BytesRemaining(),
        base::Bind(&P2PSocketHostTcp::OnWritten, base::Unretained(this)));
    HandleWriteResult(result);
  }
}

void P2PSocketHostTcpBase::OnWritten(int result) {
  DCHECK(write_pending_);
  DCHECK_NE(result, net::ERR_IO_PENDING);

  write_pending_ = false;
  HandleWriteResult(result);
  DoWrite();
}

void P2PSocketHostTcpBase::HandleWriteResult(int result) {
  DCHECK(write_buffer_.get());
  if (result >= 0) {
    write_buffer_->DidConsume(result);
    if (write_buffer_->BytesRemaining() == 0) {
      message_sender_->Send(new P2PMsg_OnSendComplete(id_));
      if (write_queue_.empty()) {
        write_buffer_ = NULL;
      } else {
        write_buffer_ = write_queue_.front();
        write_queue_.pop();
      }
    }
  } else if (result == net::ERR_IO_PENDING) {
    write_pending_ = true;
  } else {
    LOG(ERROR) << "Error when sending data in TCP socket: " << result;
    OnError();
  }
}

P2PSocketHost* P2PSocketHostTcpBase::AcceptIncomingTcpConnection(
    const net::IPEndPoint& remote_address, int id) {
  NOTREACHED();
  OnError();
  return NULL;
}

void P2PSocketHostTcpBase::DidCompleteRead(int result) {
  DCHECK_EQ(state_, STATE_OPEN);

  if (result == net::ERR_IO_PENDING) {
    return;
  } else if (result < 0) {
    LOG(ERROR) << "Error when reading from TCP socket: " << result;
    OnError();
    return;
  }

  read_buffer_->set_offset(read_buffer_->offset() + result);
  char* head = read_buffer_->StartOfBuffer();  // Purely a convenience.
  int pos = 0;
  while (pos <= read_buffer_->offset() && state_ == STATE_OPEN) {
    int consumed = ProcessInput(head + pos, read_buffer_->offset() - pos);
    if (!consumed)
      break;
    pos += consumed;
  }
  // We've consumed all complete packets from the buffer; now move any remaining
  // bytes to the head of the buffer and set offset to reflect this.
  if (pos && pos <= read_buffer_->offset()) {
    memmove(head, head + pos, read_buffer_->offset() - pos);
    read_buffer_->set_offset(read_buffer_->offset() - pos);
  }
}

bool P2PSocketHostTcpBase::SetOption(P2PSocketOption option, int value) {
  DCHECK_EQ(STATE_OPEN, state_);
  switch (option) {
    case P2P_SOCKET_OPT_RCVBUF:
      return socket_->SetReceiveBufferSize(value) == net::OK;
    case P2P_SOCKET_OPT_SNDBUF:
      return socket_->SetSendBufferSize(value) == net::OK;
    case P2P_SOCKET_OPT_DSCP:
      return false;  // For TCP sockets DSCP setting is not available.
    default:
      NOTREACHED();
      return false;
  }
}

P2PSocketHostTcp::P2PSocketHostTcp(IPC::Sender* message_sender,
                                   int socket_id,
                                   P2PSocketType type,
                                   net::URLRequestContextGetter* url_context)
    : P2PSocketHostTcpBase(message_sender, socket_id, type, url_context) {
  DCHECK(type == P2P_SOCKET_TCP_CLIENT ||
         type == P2P_SOCKET_SSLTCP_CLIENT ||
         type == P2P_SOCKET_TLS_CLIENT);
}

P2PSocketHostTcp::~P2PSocketHostTcp() {
}

int P2PSocketHostTcp::ProcessInput(char* input, int input_len) {
  if (input_len < kPacketHeaderSize)
    return 0;
  int packet_size = base::NetToHost16(*reinterpret_cast<uint16*>(input));
  if (input_len < packet_size + kPacketHeaderSize)
    return 0;

  int consumed = kPacketHeaderSize;
  char* cur = input + consumed;
  std::vector<char> data(cur, cur + packet_size);
  OnPacket(data);
  consumed += packet_size;
  return consumed;
}

void P2PSocketHostTcp::DoSend(const net::IPEndPoint& to,
                              const std::vector<char>& data,
                              const talk_base::PacketOptions& options) {
  int size = kPacketHeaderSize + data.size();
  scoped_refptr<net::DrainableIOBuffer> buffer =
      new net::DrainableIOBuffer(new net::IOBuffer(size), size);
  *reinterpret_cast<uint16*>(buffer->data()) = base::HostToNet16(data.size());
  memcpy(buffer->data() + kPacketHeaderSize, &data[0], data.size());

  packet_processing_helpers::ApplyPacketOptions(
      buffer->data() + kPacketHeaderSize,
      buffer->BytesRemaining() - kPacketHeaderSize,
      options, 0);

  WriteOrQueue(buffer);
}

// P2PSocketHostStunTcp
P2PSocketHostStunTcp::P2PSocketHostStunTcp(
    IPC::Sender* message_sender,
    int socket_id,
    P2PSocketType type,
    net::URLRequestContextGetter* url_context)
    : P2PSocketHostTcpBase(message_sender, socket_id, type, url_context) {
  DCHECK(type == P2P_SOCKET_STUN_TCP_CLIENT ||
         type == P2P_SOCKET_STUN_SSLTCP_CLIENT ||
         type == P2P_SOCKET_STUN_TLS_CLIENT);
}

P2PSocketHostStunTcp::~P2PSocketHostStunTcp() {
}

int P2PSocketHostStunTcp::ProcessInput(char* input, int input_len) {
  if (input_len < kPacketHeaderSize + kPacketLengthOffset)
    return 0;

  int pad_bytes;
  int packet_size = GetExpectedPacketSize(
      input, input_len, &pad_bytes);

  if (input_len < packet_size + pad_bytes)
    return 0;

  // We have a complete packet. Read through it.
  int consumed = 0;
  char* cur = input;
  std::vector<char> data(cur, cur + packet_size);
  OnPacket(data);
  consumed += packet_size;
  consumed += pad_bytes;
  return consumed;
}

void P2PSocketHostStunTcp::DoSend(const net::IPEndPoint& to,
                                  const std::vector<char>& data,
                                  const talk_base::PacketOptions& options) {
  // Each packet is expected to have header (STUN/TURN ChannelData), where
  // header contains message type and and length of message.
  if (data.size() < kPacketHeaderSize + kPacketLengthOffset) {
    NOTREACHED();
    OnError();
    return;
  }

  int pad_bytes;
  size_t expected_len = GetExpectedPacketSize(
      &data[0], data.size(), &pad_bytes);

  // Accepts only complete STUN/TURN packets.
  if (data.size() != expected_len) {
    NOTREACHED();
    OnError();
    return;
  }

  // Add any pad bytes to the total size.
  int size = data.size() + pad_bytes;

  scoped_refptr<net::DrainableIOBuffer> buffer =
      new net::DrainableIOBuffer(new net::IOBuffer(size), size);
  memcpy(buffer->data(), &data[0], data.size());

  packet_processing_helpers::ApplyPacketOptions(
      buffer->data(), data.size(), options, 0);

  if (pad_bytes) {
    char padding[4] = {0};
    DCHECK_LE(pad_bytes, 4);
    memcpy(buffer->data() + data.size(), padding, pad_bytes);
  }
  WriteOrQueue(buffer);

  if (dump_outgoing_rtp_packet_)
    DumpRtpPacket(buffer->data(), data.size(), false);
}

int P2PSocketHostStunTcp::GetExpectedPacketSize(
    const char* data, int len, int* pad_bytes) {
  DCHECK_LE(kTurnChannelDataHeaderSize, len);
  // Both stun and turn had length at offset 2.
  int packet_size = base::NetToHost16(*reinterpret_cast<const uint16*>(
      data + kPacketLengthOffset));

  // Get packet type (STUN or TURN).
  uint16 msg_type = base::NetToHost16(*reinterpret_cast<const uint16*>(data));

  *pad_bytes = 0;
  // Add heder length to packet length.
  if ((msg_type & 0xC000) == 0) {
    packet_size += kStunHeaderSize;
  } else {
    packet_size += kTurnChannelDataHeaderSize;
    // Calculate any padding if present.
    if (packet_size % 4)
      *pad_bytes = 4 - packet_size % 4;
  }
  return packet_size;
}

}  // namespace content
