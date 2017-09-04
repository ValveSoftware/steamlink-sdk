// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/udp_transport.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/rand_callback.h"
#include "net/log/net_log_source.h"

namespace media {
namespace cast {

namespace {

const char kOptionDscp[] = "DSCP";
#if defined(OS_WIN)
const char kOptionDisableNonBlockingIO[] = "disable_non_blocking_io";
#endif
const char kOptionSendBufferMinSize[] = "send_buffer_min_size";
const char kOptionPacerMaxBurstSize[] = "pacer_max_burst_size";

bool IsEmpty(const net::IPEndPoint& addr) {
  return (addr.address().empty() || addr.address().IsZero()) && !addr.port();
}

int LookupOptionWithDefault(const base::DictionaryValue& options,
                            const std::string& path,
                            int default_value) {
  int ret;
  if (options.GetInteger(path, &ret)) {
    return ret;
  } else {
    return default_value;
  }
}

int32_t GetTransportSendBufferSize(const base::DictionaryValue& options) {
  // Socket send buffer size needs to be at least greater than one burst
  // size.
  int32_t max_burst_size =
      LookupOptionWithDefault(options, kOptionPacerMaxBurstSize,
                              media::cast::kMaxBurstSize) *
      media::cast::kMaxIpPacketSize;
  int32_t min_send_buffer_size =
      LookupOptionWithDefault(options, kOptionSendBufferMinSize, 0);
  return std::max(max_burst_size, min_send_buffer_size);
}

}  // namespace

UdpTransport::UdpTransport(
    net::NetLog* net_log,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_thread_proxy,
    const net::IPEndPoint& local_end_point,
    const net::IPEndPoint& remote_end_point,
    const CastTransportStatusCallback& status_callback)
    : io_thread_proxy_(io_thread_proxy),
      local_addr_(local_end_point),
      remote_addr_(remote_end_point),
      udp_socket_(new net::UDPSocket(net::DatagramSocket::DEFAULT_BIND,
                                     net::RandIntCallback(),
                                     net_log,
                                     net::NetLogSource())),
      send_pending_(false),
      receive_pending_(false),
      client_connected_(false),
      next_dscp_value_(net::DSCP_NO_CHANGE),
      send_buffer_size_(media::cast::kMaxBurstSize *
                        media::cast::kMaxIpPacketSize),
      status_callback_(status_callback),
      bytes_sent_(0),
      weak_factory_(this) {
  DCHECK(!IsEmpty(local_end_point) || !IsEmpty(remote_end_point));
}

UdpTransport::~UdpTransport() {}

void UdpTransport::StartReceiving(
    const PacketReceiverCallbackWithStatus& packet_receiver) {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());

  if (!udp_socket_) {
    status_callback_.Run(TRANSPORT_SOCKET_ERROR);
    return;
  }

  packet_receiver_ = packet_receiver;
  udp_socket_->SetMulticastLoopbackMode(true);
  if (!IsEmpty(local_addr_)) {
    if (udp_socket_->Open(local_addr_.GetFamily()) < 0 ||
        udp_socket_->AllowAddressReuse() < 0 ||
        udp_socket_->Bind(local_addr_) < 0) {
      udp_socket_->Close();
      udp_socket_.reset();
      status_callback_.Run(TRANSPORT_SOCKET_ERROR);
      LOG(ERROR) << "Failed to bind local address.";
      return;
    }
  } else if (!IsEmpty(remote_addr_)) {
    if (udp_socket_->Open(remote_addr_.GetFamily()) < 0 ||
        udp_socket_->AllowAddressReuse() < 0 ||
        udp_socket_->Connect(remote_addr_) < 0) {
      udp_socket_->Close();
      udp_socket_.reset();
      status_callback_.Run(TRANSPORT_SOCKET_ERROR);
      LOG(ERROR) << "Failed to connect to remote address.";
      return;
    }
    client_connected_ = true;
  } else {
    NOTREACHED() << "Either local or remote address has to be defined.";
  }
  if (udp_socket_->SetSendBufferSize(send_buffer_size_) != net::OK) {
    LOG(WARNING) << "Failed to set socket send buffer size.";
  }

  ScheduleReceiveNextPacket();
}

void UdpTransport::StopReceiving() {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());
  packet_receiver_ = PacketReceiverCallbackWithStatus();
}


void UdpTransport::SetDscp(net::DiffServCodePoint dscp) {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());
  next_dscp_value_ = dscp;
}

#if defined(OS_WIN)
void UdpTransport::UseNonBlockingIO() {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());
  if (!udp_socket_)
    return;
  udp_socket_->UseNonBlockingIO();
}
#endif

void UdpTransport::ScheduleReceiveNextPacket() {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());
  if (!packet_receiver_.is_null() && !receive_pending_) {
    receive_pending_ = true;
    io_thread_proxy_->PostTask(FROM_HERE,
                               base::Bind(&UdpTransport::ReceiveNextPacket,
                                          weak_factory_.GetWeakPtr(),
                                          net::ERR_IO_PENDING));
  }
}

void UdpTransport::ReceiveNextPacket(int length_or_status) {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());

  if (packet_receiver_.is_null())
    return;
  if (!udp_socket_)
    return;

  // Loop while UdpSocket is delivering data synchronously.  When it responds
  // with a "pending" status, break and expect this method to be called back in
  // the future when a packet is ready.
  while (true) {
    if (length_or_status == net::ERR_IO_PENDING) {
      next_packet_.reset(new Packet(media::cast::kMaxIpPacketSize));
      recv_buf_ = new net::WrappedIOBuffer(
          reinterpret_cast<char*>(&next_packet_->front()));
      length_or_status = udp_socket_->RecvFrom(
          recv_buf_.get(), media::cast::kMaxIpPacketSize, &recv_addr_,
          base::Bind(&UdpTransport::ReceiveNextPacket,
                     weak_factory_.GetWeakPtr()));
      if (length_or_status == net::ERR_IO_PENDING) {
        receive_pending_ = true;
        return;
      }
    }

    // Note: At this point, either a packet is ready or an error has occurred.
    if (length_or_status < 0) {
      VLOG(1) << "Failed to receive packet: Status code is "
              << length_or_status;
      receive_pending_ = false;
      return;
    }

    // Confirm the packet has come from the expected remote address; otherwise,
    // ignore it.  If this is the first packet being received and no remote
    // address has been set, set the remote address and expect all future
    // packets to come from the same one.
    // TODO(hubbe): We should only do this if the caller used a valid ssrc.
    if (IsEmpty(remote_addr_)) {
      remote_addr_ = recv_addr_;
      VLOG(1) << "Setting remote address from first received packet: "
              << remote_addr_.ToString();
      next_packet_->resize(length_or_status);
      if (!packet_receiver_.Run(std::move(next_packet_))) {
        VLOG(1) << "Packet was not valid, resetting remote address.";
        remote_addr_ = net::IPEndPoint();
      }
    } else if (!(remote_addr_ == recv_addr_)) {
      VLOG(1) << "Ignoring packet received from an unrecognized address: "
              << recv_addr_.ToString() << ".";
    } else {
      next_packet_->resize(length_or_status);
      packet_receiver_.Run(std::move(next_packet_));
    }
    length_or_status = net::ERR_IO_PENDING;
  }
}

bool UdpTransport::SendPacket(PacketRef packet, const base::Closure& cb) {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());
  if (!udp_socket_)
    return true;

  // Increase byte count no matter the packet was sent or dropped.
  bytes_sent_ += packet->data.size();

  DCHECK(!send_pending_);
  if (send_pending_) {
    VLOG(1) << "Cannot send because of pending IO.";
    return true;
  }

  if (next_dscp_value_ != net::DSCP_NO_CHANGE) {
    int result = udp_socket_->SetDiffServCodePoint(next_dscp_value_);
    if (result != net::OK) {
      VLOG(1) << "Unable to set DSCP: " << next_dscp_value_
              << " to socket; Error: " << result;
    }

    if (result != net::ERR_SOCKET_NOT_CONNECTED) {
      // Don't change DSCP in next send.
      next_dscp_value_ = net::DSCP_NO_CHANGE;
    }
  }

  scoped_refptr<net::IOBuffer> buf =
      new net::WrappedIOBuffer(reinterpret_cast<char*>(&packet->data.front()));

  int result;
  base::Callback<void(int)> callback = base::Bind(&UdpTransport::OnSent,
                                                  weak_factory_.GetWeakPtr(),
                                                  buf,
                                                  packet,
                                                  cb);
  if (client_connected_) {
    // If we called Connect() before we must call Write() instead of
    // SendTo(). Otherwise on some platforms we might get
    // ERR_SOCKET_IS_CONNECTED.
    result = udp_socket_->Write(
        buf.get(), static_cast<int>(packet->data.size()), callback);
  } else if (!IsEmpty(remote_addr_)) {
    result = udp_socket_->SendTo(buf.get(),
                                 static_cast<int>(packet->data.size()),
                                 remote_addr_,
                                 callback);
  } else {
    VLOG(1) << "Failed to send packet; socket is neither bound nor "
            << "connected.";
    return true;
  }

  if (result == net::ERR_IO_PENDING) {
    send_pending_ = true;
    return false;
  }
  OnSent(buf, packet, base::Closure(), result);
  return true;
}

int64_t UdpTransport::GetBytesSent() {
  return bytes_sent_;
}

void UdpTransport::OnSent(const scoped_refptr<net::IOBuffer>& buf,
                          PacketRef packet,
                          const base::Closure& cb,
                          int result) {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());

  send_pending_ = false;
  if (result < 0) {
    VLOG(1) << "Failed to send packet: " << result << ".";
  }
  ScheduleReceiveNextPacket();

  if (!cb.is_null()) {
    cb.Run();
  }
}

void UdpTransport::SetUdpOptions(const base::DictionaryValue& options) {
  SetSendBufferSize(GetTransportSendBufferSize(options));
  if (options.HasKey(kOptionDscp)) {
    // The default DSCP value for cast is AF41. Which gives it a higher
    // priority over other traffic.
    SetDscp(net::DSCP_AF41);
  }
#if defined(OS_WIN)
  if (!options.HasKey(kOptionDisableNonBlockingIO)) {
    UseNonBlockingIO();
  }
#endif
}

void UdpTransport::SetSendBufferSize(int32_t send_buffer_size) {
  send_buffer_size_ = send_buffer_size;
}

}  // namespace cast
}  // namespace media
