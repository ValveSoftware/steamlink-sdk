// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/socket_host_udp.h"

#include "base/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/p2p/socket_host_throttler.h"
#include "content/common/p2p_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "ipc/ipc_sender.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "third_party/webrtc/media/base/rtputils.h"

namespace {

// UDP packets cannot be bigger than 64k.
const int kReadBufferSize = 65536;
// Socket receive buffer size.
const int kRecvSocketBufferSize = 65536;  // 64K

// Defines set of transient errors. These errors are ignored when we get them
// from sendto() or recvfrom() calls.
//
// net::ERR_OUT_OF_MEMORY
//
// This is caused by ENOBUFS which means the buffer of the network interface
// is full.
//
// net::ERR_CONNECTION_RESET
//
// This is caused by WSAENETRESET or WSAECONNRESET which means the
// last send resulted in an "ICMP Port Unreachable" message.
struct {
  int code;
  const char* name;
} static const kTransientErrors[] {
  {net::ERR_ADDRESS_UNREACHABLE, "net::ERR_ADDRESS_UNREACHABLE"},
  {net::ERR_ADDRESS_INVALID, "net::ERR_ADDRESS_INVALID"},
  {net::ERR_ACCESS_DENIED, "net::ERR_ACCESS_DENIED"},
  {net::ERR_CONNECTION_RESET, "net::ERR_CONNECTION_RESET"},
  {net::ERR_OUT_OF_MEMORY, "net::ERR_OUT_OF_MEMORY"},
  {net::ERR_INTERNET_DISCONNECTED, "net::ERR_INTERNET_DISCONNECTED"}
};

bool IsTransientError(int error) {
  for (const auto& transient_error : kTransientErrors) {
    if (transient_error.code == error)
      return true;
  }
  return false;
}

const char* GetTransientErrorName(int error) {
  for (const auto& transient_error : kTransientErrors) {
    if (transient_error.code == error)
      return transient_error.name;
  }
  return "";
}

}  // namespace

namespace content {

P2PSocketHostUdp::PendingPacket::PendingPacket(
    const net::IPEndPoint& to,
    const std::vector<char>& content,
    const rtc::PacketOptions& options,
    uint64_t id)
    : to(to),
      data(new net::IOBuffer(content.size())),
      size(content.size()),
      packet_options(options),
      id(id) {
  memcpy(data->data(), &content[0], size);
}

P2PSocketHostUdp::PendingPacket::PendingPacket(const PendingPacket& other) =
    default;

P2PSocketHostUdp::PendingPacket::~PendingPacket() {
}

P2PSocketHostUdp::P2PSocketHostUdp(IPC::Sender* message_sender,
                                   int socket_id,
                                   P2PMessageThrottler* throttler)
    : P2PSocketHost(message_sender, socket_id, P2PSocketHost::UDP),
      send_pending_(false),
      last_dscp_(net::DSCP_CS0),
      throttler_(throttler),
      send_buffer_size_(0) {
  net::UDPServerSocket* socket = new net::UDPServerSocket(
      GetContentClient()->browser()->GetNetLog(), net::NetLog::Source());
#if defined(OS_WIN)
  socket->UseNonBlockingIO();
#endif
  socket_.reset(socket);
}

P2PSocketHostUdp::~P2PSocketHostUdp() {
  if (state_ == STATE_OPEN) {
    DCHECK(socket_.get());
    socket_.reset();
  }
}

void P2PSocketHostUdp::SetSendBufferSize() {
  unsigned int send_buffer_size = 0;

  base::StringToUint(
      base::FieldTrialList::FindFullName("WebRTC-SystemUDPSendSocketSize"),
      &send_buffer_size);

  if (send_buffer_size > 0) {
    if (!SetOption(P2P_SOCKET_OPT_SNDBUF, send_buffer_size)) {
      LOG(WARNING) << "Failed to set socket send buffer size to "
                   << send_buffer_size;
    } else {
      send_buffer_size_ = send_buffer_size;
    }
  }
}

bool P2PSocketHostUdp::Init(const net::IPEndPoint& local_address,
                            const P2PHostAndIPEndPoint& remote_address) {
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  int result = socket_->Listen(local_address);
  if (result < 0) {
    LOG(ERROR) << "bind() to " << local_address.ToString()
               << " failed: " << result;
    OnError();
    return false;
  }

  // Setting recv socket buffer size.
  if (socket_->SetReceiveBufferSize(kRecvSocketBufferSize) != net::OK) {
    LOG(WARNING) << "Failed to set socket receive buffer size to "
                 << kRecvSocketBufferSize;
  }

  net::IPEndPoint address;
  result = socket_->GetLocalAddress(&address);
  if (result < 0) {
    LOG(ERROR) << "P2PSocketHostUdp::Init(): unable to get local address: "
               << result;
    OnError();
    return false;
  }
  VLOG(1) << "Local address: " << address.ToString();

  state_ = STATE_OPEN;

  SetSendBufferSize();

  // NOTE: Remote address will be same as what renderer provided.
  message_sender_->Send(new P2PMsg_OnSocketCreated(
      id_, address, remote_address.ip_address));

  recv_buffer_ = new net::IOBuffer(kReadBufferSize);
  DoRead();

  return true;
}

void P2PSocketHostUdp::OnError() {
  socket_.reset();
  send_queue_.clear();

  if (state_ == STATE_UNINITIALIZED || state_ == STATE_OPEN)
    message_sender_->Send(new P2PMsg_OnError(id_));

  state_ = STATE_ERROR;
}

void P2PSocketHostUdp::DoRead() {
  int result;
  do {
    result = socket_->RecvFrom(
        recv_buffer_.get(),
        kReadBufferSize,
        &recv_address_,
        base::Bind(&P2PSocketHostUdp::OnRecv, base::Unretained(this)));
    if (result == net::ERR_IO_PENDING)
      return;
    HandleReadResult(result);
  } while (state_ == STATE_OPEN);
}

void P2PSocketHostUdp::OnRecv(int result) {
  HandleReadResult(result);
  if (state_ == STATE_OPEN) {
    DoRead();
  }
}

void P2PSocketHostUdp::HandleReadResult(int result) {
  DCHECK_EQ(STATE_OPEN, state_);

  if (result > 0) {
    std::vector<char> data(recv_buffer_->data(), recv_buffer_->data() + result);

    if (!ContainsKey(connected_peers_, recv_address_)) {
      P2PSocketHost::StunMessageType type;
      bool stun = GetStunPacketType(&*data.begin(), data.size(), &type);
      if ((stun && IsRequestOrResponse(type))) {
        connected_peers_.insert(recv_address_);
      } else if (!stun || type == STUN_DATA_INDICATION) {
        LOG(ERROR) << "Received unexpected data packet from "
                   << recv_address_.ToString()
                   << " before STUN binding is finished.";
        return;
      }
    }

    message_sender_->Send(new P2PMsg_OnDataReceived(
        id_, recv_address_, data, base::TimeTicks::Now()));

    if (dump_incoming_rtp_packet_)
      DumpRtpPacket(&data[0], data.size(), true);
  } else if (result < 0 && !IsTransientError(result)) {
    LOG(ERROR) << "Error when reading from UDP socket: " << result;
    OnError();
  }
}

void P2PSocketHostUdp::Send(const net::IPEndPoint& to,
                            const std::vector<char>& data,
                            const rtc::PacketOptions& options,
                            uint64_t packet_id) {
  if (!socket_) {
    // The Send message may be sent after the an OnError message was
    // sent by hasn't been processed the renderer.
    return;
  }

  if (!ContainsKey(connected_peers_, to)) {
    P2PSocketHost::StunMessageType type = P2PSocketHost::StunMessageType();
    bool stun = GetStunPacketType(&*data.begin(), data.size(), &type);
    if (!stun || type == STUN_DATA_INDICATION) {
      LOG(ERROR) << "Page tried to send a data packet to " << to.ToString()
                 << " before STUN binding is finished.";
      OnError();
      return;
    }

    if (throttler_->DropNextPacket(data.size())) {
      VLOG(0) << "STUN message is dropped due to high volume.";
      // Do not reset socket.
      return;
    }
  }

  IncrementTotalSentPackets();

  if (send_pending_) {
    send_queue_.push_back(PendingPacket(to, data, options, packet_id));
    IncrementDelayedBytes(data.size());
    IncrementDelayedPackets();
  } else {
    // TODO(mallinath: Remove unnecessary memcpy in this case.
    PendingPacket packet(to, data, options, packet_id);
    DoSend(packet);
  }
}

void P2PSocketHostUdp::DoSend(const PendingPacket& packet) {
  TRACE_EVENT_ASYNC_STEP_INTO1("p2p", "Send", packet.id, "UdpAsyncSendTo",
                               "size", packet.size);
  // Don't try to set DSCP in following conditions,
  // 1. If the outgoing packet is set to DSCP_NO_CHANGE
  // 2. If no change in DSCP value from last packet
  // 3. If there is any error in setting DSCP on socket.
  net::DiffServCodePoint dscp =
      static_cast<net::DiffServCodePoint>(packet.packet_options.dscp);
  if (dscp != net::DSCP_NO_CHANGE && last_dscp_ != dscp &&
      last_dscp_ != net::DSCP_NO_CHANGE) {
    int result = socket_->SetDiffServCodePoint(dscp);
    if (result == net::OK) {
      last_dscp_ = dscp;
    } else if (!IsTransientError(result) && last_dscp_ != net::DSCP_CS0) {
      // We receieved a non-transient error, and it seems we have
      // not changed the DSCP in the past, disable DSCP as it unlikely
      // to work in the future.
      last_dscp_ = net::DSCP_NO_CHANGE;
    }
  }

  base::TimeTicks send_time = base::TimeTicks::Now();
  cricket::ApplyPacketOptions(reinterpret_cast<uint8_t*>(packet.data->data()),
                              packet.size,
                              packet.packet_options.packet_time_params,
                              (send_time - base::TimeTicks()).InMicroseconds());
  auto callback_binding =
      base::Bind(&P2PSocketHostUdp::OnSend, base::Unretained(this), packet.id,
                 packet.packet_options.packet_id, send_time);
  int result = socket_->SendTo(packet.data.get(), packet.size, packet.to,
                               callback_binding);

  // sendto() may return an error, e.g. if we've received an ICMP Destination
  // Unreachable message. When this happens try sending the same packet again,
  // and just drop it if it fails again.
  if (IsTransientError(result)) {
    result = socket_->SendTo(packet.data.get(), packet.size, packet.to,
                             callback_binding);
  }

  if (result == net::ERR_IO_PENDING) {
    send_pending_ = true;
  } else {
    HandleSendResult(packet.id, packet.packet_options.packet_id, send_time,
                     result);
  }

  if (dump_outgoing_rtp_packet_)
    DumpRtpPacket(packet.data->data(), packet.size, false);
}

void P2PSocketHostUdp::OnSend(uint64_t packet_id,
                              int32_t transport_sequence_number,
                              base::TimeTicks send_time,
                              int result) {
  DCHECK(send_pending_);
  DCHECK_NE(result, net::ERR_IO_PENDING);

  send_pending_ = false;

  HandleSendResult(packet_id, transport_sequence_number, send_time, result);

  // Send next packets if we have them waiting in the buffer.
  while (state_ == STATE_OPEN && !send_queue_.empty() && !send_pending_) {
    PendingPacket packet = send_queue_.front();
    DoSend(packet);
    send_queue_.pop_front();
    DecrementDelayedBytes(packet.size);
  }
}

void P2PSocketHostUdp::HandleSendResult(uint64_t packet_id,
                                        int32_t transport_sequence_number,
                                        base::TimeTicks send_time,
                                        int result) {
  TRACE_EVENT_ASYNC_END1("p2p", "Send", packet_id,
                         "result", result);
  if (result < 0) {
    if (!IsTransientError(result)) {
      LOG(ERROR) << "Error when sending data in UDP socket: " << result;
      OnError();
      return;
    }
    VLOG(0) << "sendto() has failed twice returning a "
               " transient error " << GetTransientErrorName(result)
            << ". Dropping the packet.";
  }

  // UMA to track the histograms from 1ms to 1 sec for how long a packet spends
  // in the browser process.
  UMA_HISTOGRAM_TIMES("WebRTC.SystemSendPacketDuration_UDP" /* name */,
                      base::TimeTicks::Now() - send_time /* sample */);

  message_sender_->Send(new P2PMsg_OnSendComplete(
      id_,
      P2PSendPacketMetrics(packet_id, transport_sequence_number, send_time)));
}

P2PSocketHost* P2PSocketHostUdp::AcceptIncomingTcpConnection(
    const net::IPEndPoint& remote_address, int id) {
  NOTREACHED();
  OnError();
  return NULL;
}

bool P2PSocketHostUdp::SetOption(P2PSocketOption option, int value) {
  DCHECK_EQ(STATE_OPEN, state_);
  switch (option) {
    case P2P_SOCKET_OPT_RCVBUF:
      return socket_->SetReceiveBufferSize(value) == net::OK;
    case P2P_SOCKET_OPT_SNDBUF:
      // Ignore any following call to set the send buffer size if we're under
      // experiment.
      if (send_buffer_size_ > 0) {
        return true;
      }
      return socket_->SetSendBufferSize(value) == net::OK;
    case P2P_SOCKET_OPT_DSCP:
      return (net::OK == socket_->SetDiffServCodePoint(
          static_cast<net::DiffServCodePoint>(value))) ? true : false;
    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace content
