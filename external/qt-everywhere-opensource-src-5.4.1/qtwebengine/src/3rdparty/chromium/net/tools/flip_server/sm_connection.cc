// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/flip_server/sm_connection.h"

#include <errno.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <list>
#include <string>

#include "net/tools/flip_server/constants.h"
#include "net/tools/flip_server/flip_config.h"
#include "net/tools/flip_server/http_interface.h"
#include "net/tools/flip_server/spdy_interface.h"
#include "net/tools/flip_server/spdy_ssl.h"
#include "net/tools/flip_server/streamer_interface.h"

namespace net {

// static
bool SMConnection::force_spdy_ = false;

DataFrame::~DataFrame() {
  if (delete_when_done)
    delete[] data;
}

SMConnection::SMConnection(EpollServer* epoll_server,
                           SSLState* ssl_state,
                           MemoryCache* memory_cache,
                           FlipAcceptor* acceptor,
                           std::string log_prefix)
    : last_read_time_(0),
      fd_(-1),
      events_(0),
      registered_in_epoll_server_(false),
      initialized_(false),
      protocol_detected_(false),
      connection_complete_(false),
      connection_pool_(NULL),
      epoll_server_(epoll_server),
      ssl_state_(ssl_state),
      memory_cache_(memory_cache),
      acceptor_(acceptor),
      read_buffer_(kSpdySegmentSize * 40),
      sm_spdy_interface_(NULL),
      sm_http_interface_(NULL),
      sm_streamer_interface_(NULL),
      sm_interface_(NULL),
      log_prefix_(log_prefix),
      max_bytes_sent_per_dowrite_(4096),
      ssl_(NULL) {}

SMConnection::~SMConnection() {
  if (initialized())
    Reset();
}

EpollServer* SMConnection::epoll_server() { return epoll_server_; }

void SMConnection::ReadyToSend() {
  VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
          << "Setting ready to send: EPOLLIN | EPOLLOUT";
  epoll_server_->SetFDReady(fd_, EPOLLIN | EPOLLOUT);
}

void SMConnection::EnqueueDataFrame(DataFrame* df) {
  output_list_.push_back(df);
  VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT << "EnqueueDataFrame: "
          << "size = " << df->size << ": Setting FD ready.";
  ReadyToSend();
}

void SMConnection::InitSMConnection(SMConnectionPoolInterface* connection_pool,
                                    SMInterface* sm_interface,
                                    EpollServer* epoll_server,
                                    int fd,
                                    std::string server_ip,
                                    std::string server_port,
                                    std::string remote_ip,
                                    bool use_ssl) {
  if (initialized_) {
    LOG(FATAL) << "Attempted to initialize already initialized server";
    return;
  }

  client_ip_ = remote_ip;

  if (fd == -1) {
    // If fd == -1, then we are initializing a new connection that will
    // connect to the backend.
    //
    // ret:  -1 == error
    //        0 == connection in progress
    //        1 == connection complete
    // TODO(kelindsay): is_numeric_host_address value needs to be detected
    server_ip_ = server_ip;
    server_port_ = server_port;
    int ret = CreateConnectedSocket(
        &fd_, server_ip, server_port, true, acceptor_->disable_nagle_);

    if (ret < 0) {
      LOG(ERROR) << "-1 Could not create connected socket";
      return;
    } else if (ret == 1) {
      DCHECK_NE(-1, fd_);
      connection_complete_ = true;
      VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
              << "Connection complete to: " << server_ip_ << ":" << server_port_
              << " ";
    }
    VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
            << "Connecting to server: " << server_ip_ << ":" << server_port_
            << " ";
  } else {
    // If fd != -1 then we are initializing a connection that has just been
    // accepted from the listen socket.
    connection_complete_ = true;
    if (epoll_server_ && registered_in_epoll_server_ && fd_ != -1) {
      epoll_server_->UnregisterFD(fd_);
    }
    if (fd_ != -1) {
      VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
              << "Closing pre-existing fd";
      close(fd_);
      fd_ = -1;
    }

    fd_ = fd;
  }

  registered_in_epoll_server_ = false;
  // Set the last read time here as the idle checker will start from
  // now.
  last_read_time_ = time(NULL);
  initialized_ = true;

  connection_pool_ = connection_pool;
  epoll_server_ = epoll_server;

  if (sm_interface) {
    sm_interface_ = sm_interface;
    protocol_detected_ = true;
  }

  read_buffer_.Clear();

  epoll_server_->RegisterFD(fd_, this, EPOLLIN | EPOLLOUT | EPOLLET);

  if (use_ssl) {
    ssl_ = CreateSSLContext(ssl_state_->ssl_ctx);
    SSL_set_fd(ssl_, fd_);
    PrintSslError();
  }
}

void SMConnection::CorkSocket() {
  int state = 1;
  int rv = setsockopt(fd_, IPPROTO_TCP, TCP_CORK, &state, sizeof(state));
  if (rv < 0)
    VLOG(1) << "setsockopt(CORK): " << errno;
}

void SMConnection::UncorkSocket() {
  int state = 0;
  int rv = setsockopt(fd_, IPPROTO_TCP, TCP_CORK, &state, sizeof(state));
  if (rv < 0)
    VLOG(1) << "setsockopt(CORK): " << errno;
}

int SMConnection::Send(const char* data, int len, int flags) {
  int rv = 0;
  CorkSocket();
  if (ssl_) {
    ssize_t bytes_written = 0;
    // Write smallish chunks to SSL so that we don't have large
    // multi-packet TLS records to receive before being able to handle
    // the data.  We don't have to be too careful here, because our data
    // frames are already getting chunked appropriately, and those are
    // the most likely "big" frames.
    while (len > 0) {
      const int kMaxTLSRecordSize = 1500;
      const char* ptr = &(data[bytes_written]);
      int chunksize = std::min(len, kMaxTLSRecordSize);
      rv = SSL_write(ssl_, ptr, chunksize);
      VLOG(2) << "SSLWrite(" << chunksize << " bytes): " << rv;
      if (rv <= 0) {
        switch (SSL_get_error(ssl_, rv)) {
          case SSL_ERROR_WANT_READ:
          case SSL_ERROR_WANT_WRITE:
          case SSL_ERROR_WANT_ACCEPT:
          case SSL_ERROR_WANT_CONNECT:
            rv = -2;
            break;
          default:
            PrintSslError();
            break;
        }
        break;
      }
      bytes_written += rv;
      len -= rv;
      if (rv != chunksize)
        break;  // If we couldn't write everything, we're implicitly stalled
    }
    // If we wrote some data, return that count.  Otherwise
    // return the stall error.
    if (bytes_written > 0)
      rv = bytes_written;
  } else {
    rv = send(fd_, data, len, flags);
  }
  if (!(flags & MSG_MORE))
    UncorkSocket();
  return rv;
}

void SMConnection::OnRegistration(EpollServer* eps, int fd, int event_mask) {
  registered_in_epoll_server_ = true;
}

void SMConnection::OnEvent(int fd, EpollEvent* event) {
  events_ |= event->in_events;
  HandleEvents();
  if (events_) {
    event->out_ready_mask = events_;
    events_ = 0;
  }
}

void SMConnection::OnUnregistration(int fd, bool replaced) {
  registered_in_epoll_server_ = false;
}

void SMConnection::OnShutdown(EpollServer* eps, int fd) {
  Cleanup("OnShutdown");
  return;
}

void SMConnection::Cleanup(const char* cleanup) {
  VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT << "Cleanup: " << cleanup;
  if (!initialized_)
    return;
  Reset();
  if (connection_pool_)
    connection_pool_->SMConnectionDone(this);
  if (sm_interface_)
    sm_interface_->ResetForNewConnection();
  last_read_time_ = 0;
}

void SMConnection::HandleEvents() {
  VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
          << "Received: " << EpollServer::EventMaskToString(events_).c_str();

  if (events_ & EPOLLIN) {
    if (!DoRead())
      goto handle_close_or_error;
  }

  if (events_ & EPOLLOUT) {
    // Check if we have connected or not
    if (connection_complete_ == false) {
      int sock_error;
      socklen_t sock_error_len = sizeof(sock_error);
      int ret =
          getsockopt(fd_, SOL_SOCKET, SO_ERROR, &sock_error, &sock_error_len);
      if (ret != 0) {
        VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                << "getsockopt error: " << errno << ": " << strerror(errno);
        goto handle_close_or_error;
      }
      if (sock_error == 0) {
        connection_complete_ = true;
        VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                << "Connection complete to " << server_ip_ << ":"
                << server_port_ << " ";
      } else if (sock_error == EINPROGRESS) {
        return;
      } else {
        VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                << "error connecting to server";
        goto handle_close_or_error;
      }
    }
    if (!DoWrite())
      goto handle_close_or_error;
  }

  if (events_ & (EPOLLHUP | EPOLLERR)) {
    VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT << "!!! Got HUP or ERR";
    goto handle_close_or_error;
  }
  return;

 handle_close_or_error:
  Cleanup("HandleEvents");
}

// Decide if SPDY was negotiated.
bool SMConnection::WasSpdyNegotiated(SpdyMajorVersion* version_negotiated) {
  *version_negotiated = SPDY3;
  if (force_spdy())
    return true;

  // If this is an SSL connection, check if NPN specifies SPDY.
  if (ssl_) {
    const unsigned char* npn_proto;
    unsigned int npn_proto_len;
    SSL_get0_next_proto_negotiated(ssl_, &npn_proto, &npn_proto_len);
    if (npn_proto_len > 0) {
      std::string npn_proto_str((const char*)npn_proto, npn_proto_len);
      VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
              << "NPN protocol detected: " << npn_proto_str;
      if (!strncmp(reinterpret_cast<const char*>(npn_proto),
                   "spdy/2",
                   npn_proto_len)) {
        *version_negotiated = SPDY2;
        return true;
      }
      if (!strncmp(reinterpret_cast<const char*>(npn_proto),
                   "spdy/3",
                   npn_proto_len)) {
        *version_negotiated = SPDY3;
        return true;
      }
      if (!strncmp(reinterpret_cast<const char*>(npn_proto),
                   "spdy/4a2",
                   npn_proto_len)) {
        *version_negotiated = SPDY4;
        return true;
      }
    }
  }

  return false;
}

bool SMConnection::SetupProtocolInterfaces() {
  DCHECK(!protocol_detected_);
  protocol_detected_ = true;

  SpdyMajorVersion version;
  bool spdy_negotiated = WasSpdyNegotiated(&version);
  bool using_ssl = ssl_ != NULL;

  if (using_ssl)
    VLOG(1) << (SSL_session_reused(ssl_) ? "Resumed" : "Renegotiated")
            << " SSL Session.";

  if (acceptor_->spdy_only_ && !spdy_negotiated) {
    VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
            << "SPDY proxy only, closing HTTPS connection.";
    return false;
  }

  switch (acceptor_->flip_handler_type_) {
    case FLIP_HANDLER_HTTP_SERVER: {
      DCHECK(!spdy_negotiated);
      VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
              << (sm_http_interface_ ? "Creating" : "Reusing")
              << " HTTP interface.";
      if (!sm_http_interface_)
        sm_http_interface_ = new HttpSM(this, NULL, memory_cache_, acceptor_);
      sm_interface_ = sm_http_interface_;
      break;
    }
    case FLIP_HANDLER_PROXY: {
      VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
              << (sm_streamer_interface_ ? "Creating" : "Reusing")
              << " PROXY Streamer interface.";
      if (!sm_streamer_interface_) {
        sm_streamer_interface_ =
            new StreamerSM(this, NULL, epoll_server_, acceptor_);
        sm_streamer_interface_->set_is_request();
      }
      sm_interface_ = sm_streamer_interface_;
      // If spdy is not negotiated, the streamer interface will proxy all
      // data to the origin server.
      if (!spdy_negotiated)
        break;
    }
    // Otherwise fall through into the case below.
    case FLIP_HANDLER_SPDY_SERVER: {
      DCHECK(spdy_negotiated);
      VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
              << (sm_spdy_interface_ ? "Creating" : "Reusing")
              << " SPDY interface.";
      if (sm_spdy_interface_)
        sm_spdy_interface_->CreateFramer(version);
      else
        sm_spdy_interface_ = new SpdySM(
            this, NULL, epoll_server_, memory_cache_, acceptor_, version);
      sm_interface_ = sm_spdy_interface_;
      break;
    }
  }

  CorkSocket();
  if (!sm_interface_->PostAcceptHook())
    return false;

  return true;
}

bool SMConnection::DoRead() {
  VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT << "DoRead()";
  while (!read_buffer_.Full()) {
    char* bytes;
    int size;
    if (fd_ == -1) {
      VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
              << "DoRead(): fd_ == -1. Invalid FD. Returning false";
      return false;
    }
    read_buffer_.GetWritablePtr(&bytes, &size);
    ssize_t bytes_read = 0;
    if (ssl_) {
      bytes_read = SSL_read(ssl_, bytes, size);
      if (bytes_read < 0) {
        int err = SSL_get_error(ssl_, bytes_read);
        switch (err) {
          case SSL_ERROR_WANT_READ:
          case SSL_ERROR_WANT_WRITE:
          case SSL_ERROR_WANT_ACCEPT:
          case SSL_ERROR_WANT_CONNECT:
            events_ &= ~EPOLLIN;
            VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                    << "DoRead: SSL WANT_XXX: " << err;
            goto done;
          default:
            PrintSslError();
            goto error_or_close;
        }
      }
    } else {
      bytes_read = recv(fd_, bytes, size, MSG_DONTWAIT);
    }
    int stored_errno = errno;
    if (bytes_read == -1) {
      switch (stored_errno) {
        case EAGAIN:
          events_ &= ~EPOLLIN;
          VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                  << "Got EAGAIN while reading";
          goto done;
        case EINTR:
          VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                  << "Got EINTR while reading";
          continue;
        default:
          VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                  << "While calling recv, got error: "
                  << (ssl_ ? "(ssl error)" : strerror(stored_errno));
          goto error_or_close;
      }
    } else if (bytes_read > 0) {
      VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT << "read " << bytes_read
              << " bytes";
      last_read_time_ = time(NULL);
      // If the protocol hasn't been detected yet, set up the handlers
      // we'll need.
      if (!protocol_detected_) {
        if (!SetupProtocolInterfaces()) {
          LOG(ERROR) << "Error setting up protocol interfaces.";
          goto error_or_close;
        }
      }
      read_buffer_.AdvanceWritablePtr(bytes_read);
      if (!DoConsumeReadData())
        goto error_or_close;
      continue;
    } else {  // bytes_read == 0
      VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
              << "0 bytes read with recv call.";
    }
    goto error_or_close;
  }
 done:
  VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT << "DoRead done!";
  return true;

 error_or_close:
  VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
          << "DoRead(): error_or_close. "
          << "Cleaning up, then returning false";
  Cleanup("DoRead");
  return false;
}

bool SMConnection::DoConsumeReadData() {
  char* bytes;
  int size;
  read_buffer_.GetReadablePtr(&bytes, &size);
  while (size != 0) {
    size_t bytes_consumed = sm_interface_->ProcessReadInput(bytes, size);
    VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT << "consumed "
            << bytes_consumed << " bytes";
    if (bytes_consumed == 0) {
      break;
    }
    read_buffer_.AdvanceReadablePtr(bytes_consumed);
    if (sm_interface_->MessageFullyRead()) {
      VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
              << "HandleRequestFullyRead: Setting EPOLLOUT";
      HandleResponseFullyRead();
      events_ |= EPOLLOUT;
    } else if (sm_interface_->Error()) {
      LOG(ERROR) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                 << "Framer error detected: Setting EPOLLOUT: "
                 << sm_interface_->ErrorAsString();
      // this causes everything to be closed/cleaned up.
      events_ |= EPOLLOUT;
      return false;
    }
    read_buffer_.GetReadablePtr(&bytes, &size);
  }
  return true;
}

void SMConnection::HandleResponseFullyRead() { sm_interface_->Cleanup(); }

bool SMConnection::DoWrite() {
  size_t bytes_sent = 0;
  int flags = MSG_NOSIGNAL | MSG_DONTWAIT;
  if (fd_ == -1) {
    VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
            << "DoWrite: fd == -1. Returning false.";
    return false;
  }
  if (output_list_.empty()) {
    VLOG(2) << log_prefix_ << "DoWrite: Output list empty.";
    if (sm_interface_) {
      sm_interface_->GetOutput();
    }
    if (output_list_.empty()) {
      events_ &= ~EPOLLOUT;
    }
  }
  while (!output_list_.empty()) {
    VLOG(2) << log_prefix_
            << "DoWrite: Items in output list: " << output_list_.size();
    if (bytes_sent >= max_bytes_sent_per_dowrite_) {
      VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
              << " byte sent >= max bytes sent per write: Setting EPOLLOUT: "
              << bytes_sent;
      events_ |= EPOLLOUT;
      break;
    }
    if (sm_interface_ && output_list_.size() < 2) {
      sm_interface_->GetOutput();
    }
    DataFrame* data_frame = output_list_.front();
    const char* bytes = data_frame->data;
    int size = data_frame->size;
    bytes += data_frame->index;
    size -= data_frame->index;
    DCHECK_GE(size, 0);
    if (size <= 0) {
      output_list_.pop_front();
      delete data_frame;
      continue;
    }

    flags = MSG_NOSIGNAL | MSG_DONTWAIT;
    // Look for a queue size > 1 because |this| frame is remains on the list
    // until it has finished sending.
    if (output_list_.size() > 1) {
      VLOG(2) << log_prefix_ << "Outlist size: " << output_list_.size()
              << ": Adding MSG_MORE flag";
      flags |= MSG_MORE;
    }
    VLOG(2) << log_prefix_ << "Attempting to send " << size << " bytes.";
    ssize_t bytes_written = Send(bytes, size, flags);
    int stored_errno = errno;
    if (bytes_written == -1) {
      switch (stored_errno) {
        case EAGAIN:
          events_ &= ~EPOLLOUT;
          VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                  << "Got EAGAIN while writing";
          goto done;
        case EINTR:
          VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                  << "Got EINTR while writing";
          continue;
        default:
          VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                  << "While calling send, got error: " << stored_errno << ": "
                  << (ssl_ ? "" : strerror(stored_errno));
          goto error_or_close;
      }
    } else if (bytes_written > 0) {
      VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
              << "Wrote: " << bytes_written << " bytes";
      data_frame->index += bytes_written;
      bytes_sent += bytes_written;
      continue;
    } else if (bytes_written == -2) {
      // -2 handles SSL_ERROR_WANT_* errors
      events_ &= ~EPOLLOUT;
      goto done;
    }
    VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
            << "0 bytes written with send call.";
    goto error_or_close;
  }
 done:
  UncorkSocket();
  return true;

 error_or_close:
  VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
          << "DoWrite: error_or_close. Returning false "
          << "after cleaning up";
  Cleanup("DoWrite");
  UncorkSocket();
  return false;
}

void SMConnection::Reset() {
  VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT << "Resetting";
  if (ssl_) {
    SSL_shutdown(ssl_);
    PrintSslError();
    SSL_free(ssl_);
    PrintSslError();
    ssl_ = NULL;
  }
  if (registered_in_epoll_server_) {
    epoll_server_->UnregisterFD(fd_);
    registered_in_epoll_server_ = false;
  }
  if (fd_ >= 0) {
    VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT << "Closing connection";
    close(fd_);
    fd_ = -1;
  }
  read_buffer_.Clear();
  initialized_ = false;
  protocol_detected_ = false;
  events_ = 0;
  for (std::list<DataFrame*>::iterator i = output_list_.begin();
       i != output_list_.end();
       ++i) {
    delete *i;
  }
  output_list_.clear();
}

// static
SMConnection* SMConnection::NewSMConnection(EpollServer* epoll_server,
                                            SSLState* ssl_state,
                                            MemoryCache* memory_cache,
                                            FlipAcceptor* acceptor,
                                            std::string log_prefix) {
  return new SMConnection(
      epoll_server, ssl_state, memory_cache, acceptor, log_prefix);
}

}  // namespace net
