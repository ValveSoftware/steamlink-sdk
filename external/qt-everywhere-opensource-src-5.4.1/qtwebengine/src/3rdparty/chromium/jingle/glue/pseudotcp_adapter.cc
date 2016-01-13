// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/glue/pseudotcp_adapter.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"

using cricket::PseudoTcp;

namespace {
const int kReadBufferSize = 65536;  // Maximum size of a packet.
const uint16 kDefaultMtu = 1280;
}  // namespace

namespace jingle_glue {

class PseudoTcpAdapter::Core : public cricket::IPseudoTcpNotify,
                               public base::RefCounted<Core> {
 public:
  Core(net::Socket* socket);

  // Functions used to implement net::StreamSocket.
  int Read(net::IOBuffer* buffer, int buffer_size,
           const net::CompletionCallback& callback);
  int Write(net::IOBuffer* buffer, int buffer_size,
            const net::CompletionCallback& callback);
  int Connect(const net::CompletionCallback& callback);
  void Disconnect();
  bool IsConnected() const;

  // cricket::IPseudoTcpNotify interface.
  // These notifications are triggered from NotifyPacket.
  virtual void OnTcpOpen(cricket::PseudoTcp* tcp) OVERRIDE;
  virtual void OnTcpReadable(cricket::PseudoTcp* tcp) OVERRIDE;
  virtual void OnTcpWriteable(cricket::PseudoTcp* tcp) OVERRIDE;
  // This is triggered by NotifyClock or NotifyPacket.
  virtual void OnTcpClosed(cricket::PseudoTcp* tcp, uint32 error) OVERRIDE;
  // This is triggered by NotifyClock, NotifyPacket, Recv and Send.
  virtual WriteResult TcpWritePacket(cricket::PseudoTcp* tcp,
                                     const char* buffer, size_t len) OVERRIDE;

  void SetAckDelay(int delay_ms);
  void SetNoDelay(bool no_delay);
  void SetReceiveBufferSize(int32 size);
  void SetSendBufferSize(int32 size);
  void SetWriteWaitsForSend(bool write_waits_for_send);

  void DeleteSocket();

 private:
  friend class base::RefCounted<Core>;
  virtual ~Core();

  // These are invoked by the underlying Socket, and may trigger callbacks.
  // They hold a reference to |this| while running, to protect from deletion.
  void OnRead(int result);
  void OnWritten(int result);

  // These may trigger callbacks, so the holder must hold a reference on
  // the stack while calling them.
  void DoReadFromSocket();
  void HandleReadResults(int result);
  void HandleTcpClock();

  // Checks if current write has completed in the write-waits-for-send
  // mode.
  void CheckWriteComplete();

  // This re-sets |timer| without triggering callbacks.
  void AdjustClock();

  net::CompletionCallback connect_callback_;
  net::CompletionCallback read_callback_;
  net::CompletionCallback write_callback_;

  cricket::PseudoTcp pseudo_tcp_;
  scoped_ptr<net::Socket> socket_;

  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;
  scoped_refptr<net::IOBuffer> write_buffer_;
  int write_buffer_size_;

  // Whether we need to wait for data to be sent before completing write.
  bool write_waits_for_send_;

  // Set to true in the write-waits-for-send mode when we've
  // successfully writtend data to the send buffer and waiting for the
  // data to be sent to the remote end.
  bool waiting_write_position_;

  // Number of the bytes written by the last write stored while we wait
  // for the data to be sent (i.e. when waiting_write_position_ = true).
  int last_write_result_;

  bool socket_write_pending_;
  scoped_refptr<net::IOBuffer> socket_read_buffer_;

  base::OneShotTimer<Core> timer_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};


PseudoTcpAdapter::Core::Core(net::Socket* socket)
    : pseudo_tcp_(this, 0),
      socket_(socket),
      write_waits_for_send_(false),
      waiting_write_position_(false),
      socket_write_pending_(false) {
  // Doesn't trigger callbacks.
  pseudo_tcp_.NotifyMTU(kDefaultMtu);
}

PseudoTcpAdapter::Core::~Core() {
}

int PseudoTcpAdapter::Core::Read(net::IOBuffer* buffer, int buffer_size,
                                 const net::CompletionCallback& callback) {
  DCHECK(read_callback_.is_null());

  // Reference the Core in case a callback deletes the adapter.
  scoped_refptr<Core> core(this);

  int result = pseudo_tcp_.Recv(buffer->data(), buffer_size);
  if (result < 0) {
    result = net::MapSystemError(pseudo_tcp_.GetError());
    DCHECK(result < 0);
  }

  if (result == net::ERR_IO_PENDING) {
    read_buffer_ = buffer;
    read_buffer_size_ = buffer_size;
    read_callback_ = callback;
  }

  AdjustClock();

  return result;
}

int PseudoTcpAdapter::Core::Write(net::IOBuffer* buffer, int buffer_size,
                                  const net::CompletionCallback& callback) {
  DCHECK(write_callback_.is_null());

  // Reference the Core in case a callback deletes the adapter.
  scoped_refptr<Core> core(this);

  int result = pseudo_tcp_.Send(buffer->data(), buffer_size);
  if (result < 0) {
    result = net::MapSystemError(pseudo_tcp_.GetError());
    DCHECK(result < 0);
  }

  AdjustClock();

  if (result == net::ERR_IO_PENDING) {
    write_buffer_ = buffer;
    write_buffer_size_ = buffer_size;
    write_callback_ = callback;
    return result;
  }

  if (result < 0)
    return result;

  // Need to wait until the data is sent to the peer when
  // send-confirmation mode is enabled.
  if (write_waits_for_send_ && pseudo_tcp_.GetBytesBufferedNotSent() > 0) {
    DCHECK(!waiting_write_position_);
    waiting_write_position_ = true;
    last_write_result_ = result;
    write_buffer_ = buffer;
    write_buffer_size_ = buffer_size;
    write_callback_ = callback;
    return net::ERR_IO_PENDING;
  }

  return result;
}

int PseudoTcpAdapter::Core::Connect(const net::CompletionCallback& callback) {
  DCHECK_EQ(pseudo_tcp_.State(), cricket::PseudoTcp::TCP_LISTEN);

  // Reference the Core in case a callback deletes the adapter.
  scoped_refptr<Core> core(this);

  // Start the connection attempt.
  int result = pseudo_tcp_.Connect();
  if (result < 0)
    return net::ERR_FAILED;

  AdjustClock();

  connect_callback_ = callback;
  DoReadFromSocket();

  return net::ERR_IO_PENDING;
}

void PseudoTcpAdapter::Core::Disconnect() {
  // Don't dispatch outstanding callbacks, as mandated by net::StreamSocket.
  read_callback_.Reset();
  read_buffer_ = NULL;
  write_callback_.Reset();
  write_buffer_ = NULL;
  connect_callback_.Reset();

  // TODO(wez): Connect should succeed if called after Disconnect, which
  // PseudoTcp doesn't support, so we need to teardown the internal PseudoTcp
  // and create a new one in Connect.
  // TODO(wez): Close sets a shutdown flag inside PseudoTcp but has no other
  // effect.  This should be addressed in PseudoTcp, really.
  // In the meantime we can fake OnTcpClosed notification and tear down the
  // PseudoTcp.
  pseudo_tcp_.Close(true);
}

bool PseudoTcpAdapter::Core::IsConnected() const {
  return pseudo_tcp_.State() == PseudoTcp::TCP_ESTABLISHED;
}

void PseudoTcpAdapter::Core::OnTcpOpen(PseudoTcp* tcp) {
  DCHECK(tcp == &pseudo_tcp_);

  if (!connect_callback_.is_null()) {
    net::CompletionCallback callback = connect_callback_;
    connect_callback_.Reset();
    callback.Run(net::OK);
  }

  OnTcpReadable(tcp);
  OnTcpWriteable(tcp);
}

void PseudoTcpAdapter::Core::OnTcpReadable(PseudoTcp* tcp) {
  DCHECK_EQ(tcp, &pseudo_tcp_);
  if (read_callback_.is_null())
    return;

  int result = pseudo_tcp_.Recv(read_buffer_->data(), read_buffer_size_);
  if (result < 0) {
    result = net::MapSystemError(pseudo_tcp_.GetError());
    DCHECK(result < 0);
    if (result == net::ERR_IO_PENDING)
      return;
  }

  AdjustClock();

  net::CompletionCallback callback = read_callback_;
  read_callback_.Reset();
  read_buffer_ = NULL;
  callback.Run(result);
}

void PseudoTcpAdapter::Core::OnTcpWriteable(PseudoTcp* tcp) {
  DCHECK_EQ(tcp, &pseudo_tcp_);
  if (write_callback_.is_null())
    return;

  if (waiting_write_position_) {
    CheckWriteComplete();
    return;
  }

  int result = pseudo_tcp_.Send(write_buffer_->data(), write_buffer_size_);
  if (result < 0) {
    result = net::MapSystemError(pseudo_tcp_.GetError());
    DCHECK(result < 0);
    if (result == net::ERR_IO_PENDING)
      return;
  }

  AdjustClock();

  if (write_waits_for_send_ && pseudo_tcp_.GetBytesBufferedNotSent() > 0) {
    DCHECK(!waiting_write_position_);
    waiting_write_position_ = true;
    last_write_result_ = result;
    return;
  }

  net::CompletionCallback callback = write_callback_;
  write_callback_.Reset();
  write_buffer_ = NULL;
  callback.Run(result);
}

void PseudoTcpAdapter::Core::OnTcpClosed(PseudoTcp* tcp, uint32 error) {
  DCHECK_EQ(tcp, &pseudo_tcp_);

  if (!connect_callback_.is_null()) {
    net::CompletionCallback callback = connect_callback_;
    connect_callback_.Reset();
    callback.Run(net::MapSystemError(error));
  }

  if (!read_callback_.is_null()) {
    net::CompletionCallback callback = read_callback_;
    read_callback_.Reset();
    callback.Run(net::MapSystemError(error));
  }

  if (!write_callback_.is_null()) {
    net::CompletionCallback callback = write_callback_;
    write_callback_.Reset();
    callback.Run(net::MapSystemError(error));
  }
}

void PseudoTcpAdapter::Core::SetAckDelay(int delay_ms) {
  pseudo_tcp_.SetOption(cricket::PseudoTcp::OPT_ACKDELAY, delay_ms);
}

void PseudoTcpAdapter::Core::SetNoDelay(bool no_delay) {
  pseudo_tcp_.SetOption(cricket::PseudoTcp::OPT_NODELAY, no_delay ? 1 : 0);
}

void PseudoTcpAdapter::Core::SetReceiveBufferSize(int32 size) {
  pseudo_tcp_.SetOption(cricket::PseudoTcp::OPT_RCVBUF, size);
}

void PseudoTcpAdapter::Core::SetSendBufferSize(int32 size) {
  pseudo_tcp_.SetOption(cricket::PseudoTcp::OPT_SNDBUF, size);
}

void PseudoTcpAdapter::Core::SetWriteWaitsForSend(bool write_waits_for_send) {
  write_waits_for_send_ = write_waits_for_send;
}

void PseudoTcpAdapter::Core::DeleteSocket() {
  socket_.reset();
}

cricket::IPseudoTcpNotify::WriteResult PseudoTcpAdapter::Core::TcpWritePacket(
    PseudoTcp* tcp,
    const char* buffer,
    size_t len) {
  DCHECK_EQ(tcp, &pseudo_tcp_);

  // If we already have a write pending, we behave like a congested network,
  // returning success for the write, but dropping the packet.  PseudoTcp will
  // back-off and retransmit, adjusting for the perceived congestion.
  if (socket_write_pending_)
    return IPseudoTcpNotify::WR_SUCCESS;

  scoped_refptr<net::IOBuffer> write_buffer = new net::IOBuffer(len);
  memcpy(write_buffer->data(), buffer, len);

  // Our underlying socket is datagram-oriented, which means it should either
  // send exactly as many bytes as we requested, or fail.
  int result;
  if (socket_.get()) {
    result = socket_->Write(
        write_buffer.get(),
        len,
        base::Bind(&PseudoTcpAdapter::Core::OnWritten, base::Unretained(this)));
  } else {
    result = net::ERR_CONNECTION_CLOSED;
  }
  if (result == net::ERR_IO_PENDING) {
    socket_write_pending_ = true;
    return IPseudoTcpNotify::WR_SUCCESS;
  } else if (result == net::ERR_MSG_TOO_BIG) {
    return IPseudoTcpNotify::WR_TOO_LARGE;
  } else if (result < 0) {
    return IPseudoTcpNotify::WR_FAIL;
  } else {
    return IPseudoTcpNotify::WR_SUCCESS;
  }
}

void PseudoTcpAdapter::Core::DoReadFromSocket() {
  if (!socket_read_buffer_.get())
    socket_read_buffer_ = new net::IOBuffer(kReadBufferSize);

  int result = 1;
  while (socket_.get() && result > 0) {
    result = socket_->Read(
        socket_read_buffer_.get(),
        kReadBufferSize,
        base::Bind(&PseudoTcpAdapter::Core::OnRead, base::Unretained(this)));
    if (result != net::ERR_IO_PENDING)
      HandleReadResults(result);
  }
}

void PseudoTcpAdapter::Core::HandleReadResults(int result) {
  if (result <= 0) {
    LOG(ERROR) << "Read returned " << result;
    return;
  }

  // TODO(wez): Disconnect on failure of NotifyPacket?
  pseudo_tcp_.NotifyPacket(socket_read_buffer_->data(), result);
  AdjustClock();

  CheckWriteComplete();
}

void PseudoTcpAdapter::Core::OnRead(int result) {
  // Reference the Core in case a callback deletes the adapter.
  scoped_refptr<Core> core(this);

  HandleReadResults(result);
  if (result >= 0)
    DoReadFromSocket();
}

void PseudoTcpAdapter::Core::OnWritten(int result) {
  // Reference the Core in case a callback deletes the adapter.
  scoped_refptr<Core> core(this);

  socket_write_pending_ = false;
  if (result < 0) {
    LOG(WARNING) << "Write failed. Error code: " << result;
  }
}

void PseudoTcpAdapter::Core::AdjustClock() {
  long timeout = 0;
  if (pseudo_tcp_.GetNextClock(PseudoTcp::Now(), timeout)) {
    timer_.Stop();
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromMilliseconds(std::max(timeout, 0L)), this,
                 &PseudoTcpAdapter::Core::HandleTcpClock);
  }
}

void PseudoTcpAdapter::Core::HandleTcpClock() {
  // Reference the Core in case a callback deletes the adapter.
  scoped_refptr<Core> core(this);

  pseudo_tcp_.NotifyClock(PseudoTcp::Now());
  AdjustClock();

  CheckWriteComplete();
}

void PseudoTcpAdapter::Core::CheckWriteComplete() {
  if (!write_callback_.is_null() && waiting_write_position_) {
    if (pseudo_tcp_.GetBytesBufferedNotSent() == 0) {
      waiting_write_position_ = false;

      net::CompletionCallback callback = write_callback_;
      write_callback_.Reset();
      write_buffer_ = NULL;
      callback.Run(last_write_result_);
    }
  }
}

// Public interface implemention.

PseudoTcpAdapter::PseudoTcpAdapter(net::Socket* socket)
    : core_(new Core(socket)) {
}

PseudoTcpAdapter::~PseudoTcpAdapter() {
  Disconnect();

  // Make sure that the underlying socket is destroyed before PseudoTcp.
  core_->DeleteSocket();
}

int PseudoTcpAdapter::Read(net::IOBuffer* buffer, int buffer_size,
                           const net::CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  return core_->Read(buffer, buffer_size, callback);
}

int PseudoTcpAdapter::Write(net::IOBuffer* buffer, int buffer_size,
                            const net::CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  return core_->Write(buffer, buffer_size, callback);
}

int PseudoTcpAdapter::SetReceiveBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());

  core_->SetReceiveBufferSize(size);
  return net::OK;
}

int PseudoTcpAdapter::SetSendBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());

  core_->SetSendBufferSize(size);
  return net::OK;
}

int PseudoTcpAdapter::Connect(const net::CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());

  // net::StreamSocket requires that Connect return OK if already connected.
  if (IsConnected())
    return net::OK;

  return core_->Connect(callback);
}

void PseudoTcpAdapter::Disconnect() {
  DCHECK(CalledOnValidThread());
  core_->Disconnect();
}

bool PseudoTcpAdapter::IsConnected() const {
  return core_->IsConnected();
}

bool PseudoTcpAdapter::IsConnectedAndIdle() const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return false;
}

int PseudoTcpAdapter::GetPeerAddress(net::IPEndPoint* address) const {
  DCHECK(CalledOnValidThread());

  // We don't have a meaningful peer address, but we can't return an
  // error, so we return a INADDR_ANY instead.
  net::IPAddressNumber ip_address(net::kIPv4AddressSize);
  *address = net::IPEndPoint(ip_address, 0);
  return net::OK;
}

int PseudoTcpAdapter::GetLocalAddress(net::IPEndPoint* address) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return net::ERR_FAILED;
}

const net::BoundNetLog& PseudoTcpAdapter::NetLog() const {
  DCHECK(CalledOnValidThread());
  return net_log_;
}

void PseudoTcpAdapter::SetSubresourceSpeculation() {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void PseudoTcpAdapter::SetOmniboxSpeculation() {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

bool PseudoTcpAdapter::WasEverUsed() const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return true;
}

bool PseudoTcpAdapter::UsingTCPFastOpen() const {
  DCHECK(CalledOnValidThread());
  return false;
}

bool PseudoTcpAdapter::WasNpnNegotiated() const {
  DCHECK(CalledOnValidThread());
  return false;
}

net::NextProto PseudoTcpAdapter::GetNegotiatedProtocol() const {
  DCHECK(CalledOnValidThread());
  return net::kProtoUnknown;
}

bool PseudoTcpAdapter::GetSSLInfo(net::SSLInfo* ssl_info) {
  DCHECK(CalledOnValidThread());
  return false;
}

void PseudoTcpAdapter::SetAckDelay(int delay_ms) {
  DCHECK(CalledOnValidThread());
  core_->SetAckDelay(delay_ms);
}

void PseudoTcpAdapter::SetNoDelay(bool no_delay) {
  DCHECK(CalledOnValidThread());
  core_->SetNoDelay(no_delay);
}

void PseudoTcpAdapter::SetWriteWaitsForSend(bool write_waits_for_send) {
  DCHECK(CalledOnValidThread());
  core_->SetWriteWaitsForSend(write_waits_for_send);
}

}  // namespace jingle_glue
