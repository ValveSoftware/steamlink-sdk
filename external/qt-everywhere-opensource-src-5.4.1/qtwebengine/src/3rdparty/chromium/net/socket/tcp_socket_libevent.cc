// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_socket.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/socket/socket_net_log_params.h"

// If we don't have a definition for TCPI_OPT_SYN_DATA, create one.
#ifndef TCPI_OPT_SYN_DATA
#define TCPI_OPT_SYN_DATA 32
#endif

namespace net {

namespace {

// SetTCPNoDelay turns on/off buffering in the kernel. By default, TCP sockets
// will wait up to 200ms for more data to complete a packet before transmitting.
// After calling this function, the kernel will not wait. See TCP_NODELAY in
// `man 7 tcp`.
bool SetTCPNoDelay(int fd, bool no_delay) {
  int on = no_delay ? 1 : 0;
  int error = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
  return error == 0;
}

// SetTCPKeepAlive sets SO_KEEPALIVE.
bool SetTCPKeepAlive(int fd, bool enable, int delay) {
  int on = enable ? 1 : 0;
  if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on))) {
    PLOG(ERROR) << "Failed to set SO_KEEPALIVE on fd: " << fd;
    return false;
  }

  // If we disabled TCP keep alive, our work is done here.
  if (!enable)
    return true;

#if defined(OS_LINUX) || defined(OS_ANDROID)
  // Set seconds until first TCP keep alive.
  if (setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &delay, sizeof(delay))) {
    PLOG(ERROR) << "Failed to set TCP_KEEPIDLE on fd: " << fd;
    return false;
  }
  // Set seconds between TCP keep alives.
  if (setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &delay, sizeof(delay))) {
    PLOG(ERROR) << "Failed to set TCP_KEEPINTVL on fd: " << fd;
    return false;
  }
#endif
  return true;
}

int MapAcceptError(int os_error) {
  switch (os_error) {
    // If the client aborts the connection before the server calls accept,
    // POSIX specifies accept should fail with ECONNABORTED. The server can
    // ignore the error and just call accept again, so we map the error to
    // ERR_IO_PENDING. See UNIX Network Programming, Vol. 1, 3rd Ed., Sec.
    // 5.11, "Connection Abort before accept Returns".
    case ECONNABORTED:
      return ERR_IO_PENDING;
    default:
      return MapSystemError(os_error);
  }
}

int MapConnectError(int os_error) {
  switch (os_error) {
    case EACCES:
      return ERR_NETWORK_ACCESS_DENIED;
    case ETIMEDOUT:
      return ERR_CONNECTION_TIMED_OUT;
    default: {
      int net_error = MapSystemError(os_error);
      if (net_error == ERR_FAILED)
        return ERR_CONNECTION_FAILED;  // More specific than ERR_FAILED.

      // Give a more specific error when the user is offline.
      if (net_error == ERR_ADDRESS_UNREACHABLE &&
          NetworkChangeNotifier::IsOffline()) {
        return ERR_INTERNET_DISCONNECTED;
      }
      return net_error;
    }
  }
}

}  // namespace

//-----------------------------------------------------------------------------

TCPSocketLibevent::Watcher::Watcher(
    const base::Closure& read_ready_callback,
    const base::Closure& write_ready_callback)
    : read_ready_callback_(read_ready_callback),
      write_ready_callback_(write_ready_callback) {
}

TCPSocketLibevent::Watcher::~Watcher() {
}

void TCPSocketLibevent::Watcher::OnFileCanReadWithoutBlocking(int /* fd */) {
  if (!read_ready_callback_.is_null())
    read_ready_callback_.Run();
  else
    NOTREACHED();
}

void TCPSocketLibevent::Watcher::OnFileCanWriteWithoutBlocking(int /* fd */) {
  if (!write_ready_callback_.is_null())
    write_ready_callback_.Run();
  else
    NOTREACHED();
}

TCPSocketLibevent::TCPSocketLibevent(NetLog* net_log,
                                     const NetLog::Source& source)
    : socket_(kInvalidSocket),
      accept_watcher_(base::Bind(&TCPSocketLibevent::DidCompleteAccept,
                                 base::Unretained(this)),
                      base::Closure()),
      accept_socket_(NULL),
      accept_address_(NULL),
      read_watcher_(base::Bind(&TCPSocketLibevent::DidCompleteRead,
                               base::Unretained(this)),
                    base::Closure()),
      write_watcher_(base::Closure(),
                     base::Bind(&TCPSocketLibevent::DidCompleteConnectOrWrite,
                                base::Unretained(this))),
      read_buf_len_(0),
      write_buf_len_(0),
      use_tcp_fastopen_(IsTCPFastOpenEnabled()),
      tcp_fastopen_connected_(false),
      fast_open_status_(FAST_OPEN_STATUS_UNKNOWN),
      waiting_connect_(false),
      connect_os_error_(0),
      logging_multiple_connect_attempts_(false),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)) {
  net_log_.BeginEvent(NetLog::TYPE_SOCKET_ALIVE,
                      source.ToEventParametersCallback());
}

TCPSocketLibevent::~TCPSocketLibevent() {
  net_log_.EndEvent(NetLog::TYPE_SOCKET_ALIVE);
  if (tcp_fastopen_connected_) {
    UMA_HISTOGRAM_ENUMERATION("Net.TcpFastOpenSocketConnection",
                              fast_open_status_, FAST_OPEN_MAX_VALUE);
  }
  Close();
}

int TCPSocketLibevent::Open(AddressFamily family) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(socket_, kInvalidSocket);

  socket_ = CreatePlatformSocket(ConvertAddressFamily(family), SOCK_STREAM,
                                 IPPROTO_TCP);
  if (socket_ < 0) {
    PLOG(ERROR) << "CreatePlatformSocket() returned an error";
    return MapSystemError(errno);
  }

  if (SetNonBlocking(socket_)) {
    int result = MapSystemError(errno);
    Close();
    return result;
  }

  return OK;
}

int TCPSocketLibevent::AdoptConnectedSocket(int socket,
                                            const IPEndPoint& peer_address) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(socket_, kInvalidSocket);

  socket_ = socket;

  if (SetNonBlocking(socket_)) {
    int result = MapSystemError(errno);
    Close();
    return result;
  }

  peer_address_.reset(new IPEndPoint(peer_address));

  return OK;
}

int TCPSocketLibevent::Bind(const IPEndPoint& address) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(socket_, kInvalidSocket);

  SockaddrStorage storage;
  if (!address.ToSockAddr(storage.addr, &storage.addr_len))
    return ERR_ADDRESS_INVALID;

  int result = bind(socket_, storage.addr, storage.addr_len);
  if (result < 0) {
    PLOG(ERROR) << "bind() returned an error";
    return MapSystemError(errno);
  }

  return OK;
}

int TCPSocketLibevent::Listen(int backlog) {
  DCHECK(CalledOnValidThread());
  DCHECK_GT(backlog, 0);
  DCHECK_NE(socket_, kInvalidSocket);

  int result = listen(socket_, backlog);
  if (result < 0) {
    PLOG(ERROR) << "listen() returned an error";
    return MapSystemError(errno);
  }

  return OK;
}

int TCPSocketLibevent::Accept(scoped_ptr<TCPSocketLibevent>* socket,
                              IPEndPoint* address,
                              const CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(socket);
  DCHECK(address);
  DCHECK(!callback.is_null());
  DCHECK(accept_callback_.is_null());

  net_log_.BeginEvent(NetLog::TYPE_TCP_ACCEPT);

  int result = AcceptInternal(socket, address);

  if (result == ERR_IO_PENDING) {
    if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
            socket_, true, base::MessageLoopForIO::WATCH_READ,
            &accept_socket_watcher_, &accept_watcher_)) {
      PLOG(ERROR) << "WatchFileDescriptor failed on read";
      return MapSystemError(errno);
    }

    accept_socket_ = socket;
    accept_address_ = address;
    accept_callback_ = callback;
  }

  return result;
}

int TCPSocketLibevent::Connect(const IPEndPoint& address,
                               const CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(socket_, kInvalidSocket);
  DCHECK(!waiting_connect_);

  // |peer_address_| will be non-NULL if Connect() has been called. Unless
  // Close() is called to reset the internal state, a second call to Connect()
  // is not allowed.
  // Please note that we don't allow a second Connect() even if the previous
  // Connect() has failed. Connecting the same |socket_| again after a
  // connection attempt failed results in unspecified behavior according to
  // POSIX.
  DCHECK(!peer_address_);

  if (!logging_multiple_connect_attempts_)
    LogConnectBegin(AddressList(address));

  peer_address_.reset(new IPEndPoint(address));

  int rv = DoConnect();
  if (rv == ERR_IO_PENDING) {
    // Synchronous operation not supported.
    DCHECK(!callback.is_null());
    write_callback_ = callback;
    waiting_connect_ = true;
  } else {
    DoConnectComplete(rv);
  }

  return rv;
}

bool TCPSocketLibevent::IsConnected() const {
  DCHECK(CalledOnValidThread());

  if (socket_ == kInvalidSocket || waiting_connect_)
    return false;

  if (use_tcp_fastopen_ && !tcp_fastopen_connected_ && peer_address_) {
    // With TCP FastOpen, we pretend that the socket is connected.
    // This allows GetPeerAddress() to return peer_address_.
    return true;
  }

  // Check if connection is alive.
  char c;
  int rv = HANDLE_EINTR(recv(socket_, &c, 1, MSG_PEEK));
  if (rv == 0)
    return false;
  if (rv == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
    return false;

  return true;
}

bool TCPSocketLibevent::IsConnectedAndIdle() const {
  DCHECK(CalledOnValidThread());

  if (socket_ == kInvalidSocket || waiting_connect_)
    return false;

  // TODO(wtc): should we also handle the TCP FastOpen case here,
  // as we do in IsConnected()?

  // Check if connection is alive and we haven't received any data
  // unexpectedly.
  char c;
  int rv = HANDLE_EINTR(recv(socket_, &c, 1, MSG_PEEK));
  if (rv >= 0)
    return false;
  if (errno != EAGAIN && errno != EWOULDBLOCK)
    return false;

  return true;
}

int TCPSocketLibevent::Read(IOBuffer* buf,
                            int buf_len,
                            const CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_);
  DCHECK(!waiting_connect_);
  DCHECK(read_callback_.is_null());
  // Synchronous operation not supported
  DCHECK(!callback.is_null());
  DCHECK_GT(buf_len, 0);

  int nread = HANDLE_EINTR(read(socket_, buf->data(), buf_len));
  if (nread >= 0) {
    base::StatsCounter read_bytes("tcp.read_bytes");
    read_bytes.Add(nread);
    net_log_.AddByteTransferEvent(NetLog::TYPE_SOCKET_BYTES_RECEIVED, nread,
                                  buf->data());
    RecordFastOpenStatus();
    return nread;
  }
  if (errno != EAGAIN && errno != EWOULDBLOCK) {
    int net_error = MapSystemError(errno);
    net_log_.AddEvent(NetLog::TYPE_SOCKET_READ_ERROR,
                      CreateNetLogSocketErrorCallback(net_error, errno));
    return net_error;
  }

  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
          socket_, true, base::MessageLoopForIO::WATCH_READ,
          &read_socket_watcher_, &read_watcher_)) {
    DVLOG(1) << "WatchFileDescriptor failed on read, errno " << errno;
    return MapSystemError(errno);
  }

  read_buf_ = buf;
  read_buf_len_ = buf_len;
  read_callback_ = callback;
  return ERR_IO_PENDING;
}

int TCPSocketLibevent::Write(IOBuffer* buf,
                             int buf_len,
                             const CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_);
  DCHECK(!waiting_connect_);
  DCHECK(write_callback_.is_null());
  // Synchronous operation not supported
  DCHECK(!callback.is_null());
  DCHECK_GT(buf_len, 0);

  int nwrite = InternalWrite(buf, buf_len);
  if (nwrite >= 0) {
    base::StatsCounter write_bytes("tcp.write_bytes");
    write_bytes.Add(nwrite);
    net_log_.AddByteTransferEvent(NetLog::TYPE_SOCKET_BYTES_SENT, nwrite,
                                  buf->data());
    return nwrite;
  }
  if (errno != EAGAIN && errno != EWOULDBLOCK) {
    int net_error = MapSystemError(errno);
    net_log_.AddEvent(NetLog::TYPE_SOCKET_WRITE_ERROR,
                      CreateNetLogSocketErrorCallback(net_error, errno));
    return net_error;
  }

  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
          socket_, true, base::MessageLoopForIO::WATCH_WRITE,
          &write_socket_watcher_, &write_watcher_)) {
    DVLOG(1) << "WatchFileDescriptor failed on write, errno " << errno;
    return MapSystemError(errno);
  }

  write_buf_ = buf;
  write_buf_len_ = buf_len;
  write_callback_ = callback;
  return ERR_IO_PENDING;
}

int TCPSocketLibevent::GetLocalAddress(IPEndPoint* address) const {
  DCHECK(CalledOnValidThread());
  DCHECK(address);

  SockaddrStorage storage;
  if (getsockname(socket_, storage.addr, &storage.addr_len) < 0)
    return MapSystemError(errno);
  if (!address->FromSockAddr(storage.addr, storage.addr_len))
    return ERR_ADDRESS_INVALID;

  return OK;
}

int TCPSocketLibevent::GetPeerAddress(IPEndPoint* address) const {
  DCHECK(CalledOnValidThread());
  DCHECK(address);
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;
  *address = *peer_address_;
  return OK;
}

int TCPSocketLibevent::SetDefaultOptionsForServer() {
  DCHECK(CalledOnValidThread());
  return SetAddressReuse(true);
}

void TCPSocketLibevent::SetDefaultOptionsForClient() {
  DCHECK(CalledOnValidThread());

  // This mirrors the behaviour on Windows. See the comment in
  // tcp_socket_win.cc after searching for "NODELAY".
  SetTCPNoDelay(socket_, true);  // If SetTCPNoDelay fails, we don't care.

  // TCP keep alive wakes up the radio, which is expensive on mobile. Do not
  // enable it there. It's useful to prevent TCP middleboxes from timing out
  // connection mappings. Packets for timed out connection mappings at
  // middleboxes will either lead to:
  // a) Middleboxes sending TCP RSTs. It's up to higher layers to check for this
  // and retry. The HTTP network transaction code does this.
  // b) Middleboxes just drop the unrecognized TCP packet. This leads to the TCP
  // stack retransmitting packets per TCP stack retransmission timeouts, which
  // are very high (on the order of seconds). Given the number of
  // retransmissions required before killing the connection, this can lead to
  // tens of seconds or even minutes of delay, depending on OS.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  const int kTCPKeepAliveSeconds = 45;

  SetTCPKeepAlive(socket_, true, kTCPKeepAliveSeconds);
#endif
}

int TCPSocketLibevent::SetAddressReuse(bool allow) {
  DCHECK(CalledOnValidThread());

  // SO_REUSEADDR is useful for server sockets to bind to a recently unbound
  // port. When a socket is closed, the end point changes its state to TIME_WAIT
  // and wait for 2 MSL (maximum segment lifetime) to ensure the remote peer
  // acknowledges its closure. For server sockets, it is usually safe to
  // bind to a TIME_WAIT end point immediately, which is a widely adopted
  // behavior.
  //
  // Note that on *nix, SO_REUSEADDR does not enable the TCP socket to bind to
  // an end point that is already bound by another socket. To do that one must
  // set SO_REUSEPORT instead. This option is not provided on Linux prior
  // to 3.9.
  //
  // SO_REUSEPORT is provided in MacOS X and iOS.
  int boolean_value = allow ? 1 : 0;
  int rv = setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &boolean_value,
                      sizeof(boolean_value));
  if (rv < 0)
    return MapSystemError(errno);
  return OK;
}

int TCPSocketLibevent::SetReceiveBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());
  int rv = setsockopt(socket_, SOL_SOCKET, SO_RCVBUF,
                      reinterpret_cast<const char*>(&size), sizeof(size));
  return (rv == 0) ? OK : MapSystemError(errno);
}

int TCPSocketLibevent::SetSendBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());
  int rv = setsockopt(socket_, SOL_SOCKET, SO_SNDBUF,
                      reinterpret_cast<const char*>(&size), sizeof(size));
  return (rv == 0) ? OK : MapSystemError(errno);
}

bool TCPSocketLibevent::SetKeepAlive(bool enable, int delay) {
  DCHECK(CalledOnValidThread());
  return SetTCPKeepAlive(socket_, enable, delay);
}

bool TCPSocketLibevent::SetNoDelay(bool no_delay) {
  DCHECK(CalledOnValidThread());
  return SetTCPNoDelay(socket_, no_delay);
}

void TCPSocketLibevent::Close() {
  DCHECK(CalledOnValidThread());

  bool ok = accept_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  ok = read_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  ok = write_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);

  if (socket_ != kInvalidSocket) {
    if (IGNORE_EINTR(close(socket_)) < 0)
      PLOG(ERROR) << "close";
    socket_ = kInvalidSocket;
  }

  if (!accept_callback_.is_null()) {
    accept_socket_ = NULL;
    accept_address_ = NULL;
    accept_callback_.Reset();
  }

  if (!read_callback_.is_null()) {
    read_buf_ = NULL;
    read_buf_len_ = 0;
    read_callback_.Reset();
  }

  if (!write_callback_.is_null()) {
    write_buf_ = NULL;
    write_buf_len_ = 0;
    write_callback_.Reset();
  }

  tcp_fastopen_connected_ = false;
  fast_open_status_ = FAST_OPEN_STATUS_UNKNOWN;
  waiting_connect_ = false;
  peer_address_.reset();
  connect_os_error_ = 0;
}

bool TCPSocketLibevent::UsingTCPFastOpen() const {
  return use_tcp_fastopen_;
}

void TCPSocketLibevent::StartLoggingMultipleConnectAttempts(
    const AddressList& addresses) {
  if (!logging_multiple_connect_attempts_) {
    logging_multiple_connect_attempts_ = true;
    LogConnectBegin(addresses);
  } else {
    NOTREACHED();
  }
}

void TCPSocketLibevent::EndLoggingMultipleConnectAttempts(int net_error) {
  if (logging_multiple_connect_attempts_) {
    LogConnectEnd(net_error);
    logging_multiple_connect_attempts_ = false;
  } else {
    NOTREACHED();
  }
}

int TCPSocketLibevent::AcceptInternal(scoped_ptr<TCPSocketLibevent>* socket,
                                      IPEndPoint* address) {
  SockaddrStorage storage;
  int new_socket = HANDLE_EINTR(accept(socket_,
                                       storage.addr,
                                       &storage.addr_len));
  if (new_socket < 0) {
    int net_error = MapAcceptError(errno);
    if (net_error != ERR_IO_PENDING)
      net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_ACCEPT, net_error);
    return net_error;
  }

  IPEndPoint ip_end_point;
  if (!ip_end_point.FromSockAddr(storage.addr, storage.addr_len)) {
    NOTREACHED();
    if (IGNORE_EINTR(close(new_socket)) < 0)
      PLOG(ERROR) << "close";
    int net_error = ERR_ADDRESS_INVALID;
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_ACCEPT, net_error);
    return net_error;
  }
  scoped_ptr<TCPSocketLibevent> tcp_socket(new TCPSocketLibevent(
      net_log_.net_log(), net_log_.source()));
  int adopt_result = tcp_socket->AdoptConnectedSocket(new_socket, ip_end_point);
  if (adopt_result != OK) {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_ACCEPT, adopt_result);
    return adopt_result;
  }
  *socket = tcp_socket.Pass();
  *address = ip_end_point;
  net_log_.EndEvent(NetLog::TYPE_TCP_ACCEPT,
                    CreateNetLogIPEndPointCallback(&ip_end_point));
  return OK;
}

int TCPSocketLibevent::DoConnect() {
  DCHECK_EQ(0, connect_os_error_);

  net_log_.BeginEvent(NetLog::TYPE_TCP_CONNECT_ATTEMPT,
                      CreateNetLogIPEndPointCallback(peer_address_.get()));

  // Connect the socket.
  if (!use_tcp_fastopen_) {
    SockaddrStorage storage;
    if (!peer_address_->ToSockAddr(storage.addr, &storage.addr_len))
      return ERR_ADDRESS_INVALID;

    if (!HANDLE_EINTR(connect(socket_, storage.addr, storage.addr_len))) {
      // Connected without waiting!
      return OK;
    }
  } else {
    // With TCP FastOpen, we pretend that the socket is connected.
    DCHECK(!tcp_fastopen_connected_);
    return OK;
  }

  // Check if the connect() failed synchronously.
  connect_os_error_ = errno;
  if (connect_os_error_ != EINPROGRESS)
    return MapConnectError(connect_os_error_);

  // Otherwise the connect() is going to complete asynchronously, so watch
  // for its completion.
  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
          socket_, true, base::MessageLoopForIO::WATCH_WRITE,
          &write_socket_watcher_, &write_watcher_)) {
    connect_os_error_ = errno;
    DVLOG(1) << "WatchFileDescriptor failed: " << connect_os_error_;
    return MapSystemError(connect_os_error_);
  }

  return ERR_IO_PENDING;
}

void TCPSocketLibevent::DoConnectComplete(int result) {
  // Log the end of this attempt (and any OS error it threw).
  int os_error = connect_os_error_;
  connect_os_error_ = 0;
  if (result != OK) {
    net_log_.EndEvent(NetLog::TYPE_TCP_CONNECT_ATTEMPT,
                      NetLog::IntegerCallback("os_error", os_error));
  } else {
    net_log_.EndEvent(NetLog::TYPE_TCP_CONNECT_ATTEMPT);
  }

  if (!logging_multiple_connect_attempts_)
    LogConnectEnd(result);
}

void TCPSocketLibevent::LogConnectBegin(const AddressList& addresses) {
  base::StatsCounter connects("tcp.connect");
  connects.Increment();

  net_log_.BeginEvent(NetLog::TYPE_TCP_CONNECT,
                      addresses.CreateNetLogCallback());
}

void TCPSocketLibevent::LogConnectEnd(int net_error) {
  if (net_error == OK)
    UpdateConnectionTypeHistograms(CONNECTION_ANY);

  if (net_error != OK) {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_CONNECT, net_error);
    return;
  }

  SockaddrStorage storage;
  int rv = getsockname(socket_, storage.addr, &storage.addr_len);
  if (rv != 0) {
    PLOG(ERROR) << "getsockname() [rv: " << rv << "] error: ";
    NOTREACHED();
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_CONNECT, rv);
    return;
  }

  net_log_.EndEvent(NetLog::TYPE_TCP_CONNECT,
                    CreateNetLogSourceAddressCallback(storage.addr,
                                                      storage.addr_len));
}

void TCPSocketLibevent::DidCompleteRead() {
  RecordFastOpenStatus();
  if (read_callback_.is_null())
    return;

  int bytes_transferred;
  bytes_transferred = HANDLE_EINTR(read(socket_, read_buf_->data(),
                                        read_buf_len_));

  int result;
  if (bytes_transferred >= 0) {
    result = bytes_transferred;
    base::StatsCounter read_bytes("tcp.read_bytes");
    read_bytes.Add(bytes_transferred);
    net_log_.AddByteTransferEvent(NetLog::TYPE_SOCKET_BYTES_RECEIVED, result,
                                  read_buf_->data());
  } else {
    result = MapSystemError(errno);
    if (result != ERR_IO_PENDING) {
      net_log_.AddEvent(NetLog::TYPE_SOCKET_READ_ERROR,
                        CreateNetLogSocketErrorCallback(result, errno));
    }
  }

  if (result != ERR_IO_PENDING) {
    read_buf_ = NULL;
    read_buf_len_ = 0;
    bool ok = read_socket_watcher_.StopWatchingFileDescriptor();
    DCHECK(ok);
    base::ResetAndReturn(&read_callback_).Run(result);
  }
}

void TCPSocketLibevent::DidCompleteWrite() {
  if (write_callback_.is_null())
    return;

  int bytes_transferred;
  bytes_transferred = HANDLE_EINTR(write(socket_, write_buf_->data(),
                                         write_buf_len_));

  int result;
  if (bytes_transferred >= 0) {
    result = bytes_transferred;
    base::StatsCounter write_bytes("tcp.write_bytes");
    write_bytes.Add(bytes_transferred);
    net_log_.AddByteTransferEvent(NetLog::TYPE_SOCKET_BYTES_SENT, result,
                                  write_buf_->data());
  } else {
    result = MapSystemError(errno);
    if (result != ERR_IO_PENDING) {
      net_log_.AddEvent(NetLog::TYPE_SOCKET_WRITE_ERROR,
                        CreateNetLogSocketErrorCallback(result, errno));
    }
  }

  if (result != ERR_IO_PENDING) {
    write_buf_ = NULL;
    write_buf_len_ = 0;
    write_socket_watcher_.StopWatchingFileDescriptor();
    base::ResetAndReturn(&write_callback_).Run(result);
  }
}

void TCPSocketLibevent::DidCompleteConnect() {
  DCHECK(waiting_connect_);

  // Get the error that connect() completed with.
  int os_error = 0;
  socklen_t len = sizeof(os_error);
  if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, &os_error, &len) < 0)
    os_error = errno;

  int result = MapConnectError(os_error);
  connect_os_error_ = os_error;
  if (result != ERR_IO_PENDING) {
    DoConnectComplete(result);
    waiting_connect_ = false;
    write_socket_watcher_.StopWatchingFileDescriptor();
    base::ResetAndReturn(&write_callback_).Run(result);
  }
}

void TCPSocketLibevent::DidCompleteConnectOrWrite() {
  if (waiting_connect_)
    DidCompleteConnect();
  else
    DidCompleteWrite();
}

void TCPSocketLibevent::DidCompleteAccept() {
  DCHECK(CalledOnValidThread());

  int result = AcceptInternal(accept_socket_, accept_address_);
  if (result != ERR_IO_PENDING) {
    accept_socket_ = NULL;
    accept_address_ = NULL;
    bool ok = accept_socket_watcher_.StopWatchingFileDescriptor();
    DCHECK(ok);
    CompletionCallback callback = accept_callback_;
    accept_callback_.Reset();
    callback.Run(result);
  }
}

int TCPSocketLibevent::InternalWrite(IOBuffer* buf, int buf_len) {
  int nwrite;
  if (use_tcp_fastopen_ && !tcp_fastopen_connected_) {
    SockaddrStorage storage;
    if (!peer_address_->ToSockAddr(storage.addr, &storage.addr_len)) {
      // Set errno to EADDRNOTAVAIL so that MapSystemError will map it to
      // ERR_ADDRESS_INVALID later.
      errno = EADDRNOTAVAIL;
      return -1;
    }

    int flags = 0x20000000; // Magic flag to enable TCP_FASTOPEN.
#if defined(OS_LINUX)
    // sendto() will fail with EPIPE when the system doesn't support TCP Fast
    // Open. Theoretically that shouldn't happen since the caller should check
    // for system support on startup, but users may dynamically disable TCP Fast
    // Open via sysctl.
    flags |= MSG_NOSIGNAL;
#endif // defined(OS_LINUX)
    nwrite = HANDLE_EINTR(sendto(socket_,
                                 buf->data(),
                                 buf_len,
                                 flags,
                                 storage.addr,
                                 storage.addr_len));
    tcp_fastopen_connected_ = true;

    if (nwrite < 0) {
      DCHECK_NE(EPIPE, errno);

      // If errno == EINPROGRESS, that means the kernel didn't have a cookie
      // and would block. The kernel is internally doing a connect() though.
      // Remap EINPROGRESS to EAGAIN so we treat this the same as our other
      // asynchronous cases. Note that the user buffer has not been copied to
      // kernel space.
      if (errno == EINPROGRESS) {
        errno = EAGAIN;
        fast_open_status_ = FAST_OPEN_SLOW_CONNECT_RETURN;
      } else {
        fast_open_status_ = FAST_OPEN_ERROR;
      }
    } else {
      fast_open_status_ = FAST_OPEN_FAST_CONNECT_RETURN;
    }
  } else {
    nwrite = HANDLE_EINTR(write(socket_, buf->data(), buf_len));
  }
  return nwrite;
}

void TCPSocketLibevent::RecordFastOpenStatus() {
  if (use_tcp_fastopen_ &&
      (fast_open_status_ == FAST_OPEN_FAST_CONNECT_RETURN ||
       fast_open_status_ == FAST_OPEN_SLOW_CONNECT_RETURN)) {
    DCHECK_NE(FAST_OPEN_STATUS_UNKNOWN, fast_open_status_);
    bool getsockopt_success(false);
    bool server_acked_data(false);
#if defined(TCP_INFO)
    // Probe to see the if the socket used TCP Fast Open.
    tcp_info info;
    socklen_t info_len = sizeof(tcp_info);
    getsockopt_success =
        getsockopt(socket_, IPPROTO_TCP, TCP_INFO, &info, &info_len) == 0 &&
        info_len == sizeof(tcp_info);
    server_acked_data = getsockopt_success &&
        (info.tcpi_options & TCPI_OPT_SYN_DATA);
#endif
    if (getsockopt_success) {
      if (fast_open_status_ == FAST_OPEN_FAST_CONNECT_RETURN) {
        fast_open_status_ = (server_acked_data ? FAST_OPEN_SYN_DATA_ACK :
                             FAST_OPEN_SYN_DATA_NACK);
      } else {
        fast_open_status_ = (server_acked_data ? FAST_OPEN_NO_SYN_DATA_ACK :
                             FAST_OPEN_NO_SYN_DATA_NACK);
      }
    } else {
      fast_open_status_ = (fast_open_status_ == FAST_OPEN_FAST_CONNECT_RETURN ?
                           FAST_OPEN_SYN_DATA_FAILED :
                           FAST_OPEN_NO_SYN_DATA_FAILED);
    }
  }
}

}  // namespace net
