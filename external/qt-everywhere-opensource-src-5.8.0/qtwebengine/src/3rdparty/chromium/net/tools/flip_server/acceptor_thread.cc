// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/flip_server/acceptor_thread.h"

#include <errno.h>
#include <netinet/in.h>
#include <string.h>       // For strerror
#include <sys/socket.h>
#include <sys/types.h>

#include <string>

#include "net/socket/tcp_socket.h"
#include "net/tools/flip_server/constants.h"
#include "net/tools/flip_server/flip_config.h"
#include "net/tools/flip_server/sm_connection.h"
#include "net/tools/flip_server/spdy_ssl.h"
#include "openssl/err.h"
#include "openssl/ssl.h"

namespace net {

SMAcceptorThread::SMAcceptorThread(FlipAcceptor* acceptor,
                                   MemoryCache* memory_cache)
    : SimpleThread("SMAcceptorThread"),
      acceptor_(acceptor),
      ssl_state_(NULL),
      use_ssl_(false),
      idle_socket_timeout_s_(acceptor->idle_socket_timeout_s_),
      quitting_(false),
      memory_cache_(memory_cache) {
  if (!acceptor->ssl_cert_filename_.empty() &&
      !acceptor->ssl_key_filename_.empty()) {
    ssl_state_ = new SSLState;
    bool use_npn = true;
    if (acceptor_->flip_handler_type_ == FLIP_HANDLER_HTTP_SERVER) {
      use_npn = false;
    }
    InitSSL(ssl_state_,
            acceptor_->ssl_cert_filename_,
            acceptor_->ssl_key_filename_,
            use_npn,
            acceptor_->ssl_session_expiry_,
            acceptor_->ssl_disable_compression_);
    use_ssl_ = true;
  }
}

SMAcceptorThread::~SMAcceptorThread() {
  for (std::vector<SMConnection*>::iterator i =
           allocated_server_connections_.begin();
       i != allocated_server_connections_.end();
       ++i) {
    delete *i;
  }
  delete ssl_state_;
}

SMConnection* SMAcceptorThread::NewConnection() {
  SMConnection* server = SMConnection::NewSMConnection(
      &epoll_server_, ssl_state_, memory_cache_, acceptor_, "client_conn: ");
  allocated_server_connections_.push_back(server);
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "Acceptor: Making new server.";
  return server;
}

SMConnection* SMAcceptorThread::FindOrMakeNewSMConnection() {
  if (unused_server_connections_.empty()) {
    return NewConnection();
  }
  SMConnection* server = unused_server_connections_.back();
  unused_server_connections_.pop_back();
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "Acceptor: Reusing server.";
  return server;
}

void SMAcceptorThread::InitWorker() {
  epoll_server_.RegisterFD(acceptor_->listen_fd_, this, EPOLLIN | EPOLLET);
}

void SMAcceptorThread::HandleConnection(int server_fd,
                                        struct sockaddr_in* remote_addr) {
  if (acceptor_->disable_nagle_) {
    if (!SetTCPNoDelay(server_fd, /*no_delay=*/true)) {
      close(server_fd);
      LOG(FATAL) << "SetTCPNoDelay() failed on fd: " << server_fd;
      return;
    }
  }

  SMConnection* server_connection = FindOrMakeNewSMConnection();
  if (server_connection == NULL) {
    VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Acceptor: Closing fd " << server_fd;
    close(server_fd);
    return;
  }
  std::string remote_ip = inet_ntoa(remote_addr->sin_addr);
  server_connection->InitSMConnection(this,
                                      NULL,
                                      &epoll_server_,
                                      server_fd,
                                      std::string(),
                                      std::string(),
                                      remote_ip,
                                      use_ssl_);
  if (server_connection->initialized())
    active_server_connections_.push_back(server_connection);
}

void SMAcceptorThread::AcceptFromListenFD() {
  if (acceptor_->accepts_per_wake_ > 0) {
    for (int i = 0; i < acceptor_->accepts_per_wake_; ++i) {
      struct sockaddr address;
      socklen_t socklen = sizeof(address);
      int fd = accept(acceptor_->listen_fd_, &address, &socklen);
      if (fd == -1) {
        if (errno != 11) {
          VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Acceptor: accept fail("
                  << acceptor_->listen_fd_ << "): " << errno << ": "
                  << strerror(errno);
        }
        break;
      }
      VLOG(1) << ACCEPTOR_CLIENT_IDENT << " Accepted connection";
      HandleConnection(fd, (struct sockaddr_in*)&address);
    }
  } else {
    while (true) {
      struct sockaddr address;
      socklen_t socklen = sizeof(address);
      int fd = accept(acceptor_->listen_fd_, &address, &socklen);
      if (fd == -1) {
        if (errno != 11) {
          VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Acceptor: accept fail("
                  << acceptor_->listen_fd_ << "): " << errno << ": "
                  << strerror(errno);
        }
        break;
      }
      VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Accepted connection";
      HandleConnection(fd, (struct sockaddr_in*)&address);
    }
  }
}

void SMAcceptorThread::HandleConnectionIdleTimeout() {
  static time_t oldest_time = time(NULL);

  int cur_time = time(NULL);
  // Only iterate the list if we speculate that a connection is ready to be
  // expired
  if ((cur_time - oldest_time) < idle_socket_timeout_s_)
    return;

  // TODO(mbelshe): This code could be optimized, active_server_connections_
  //                is already in-order.
  std::list<SMConnection*>::iterator iter = active_server_connections_.begin();
  while (iter != active_server_connections_.end()) {
    SMConnection* conn = *iter;
    int elapsed_time = (cur_time - conn->last_read_time_);
    if (elapsed_time > idle_socket_timeout_s_) {
      conn->Cleanup("Connection idle timeout reached.");
      iter = active_server_connections_.erase(iter);
      continue;
    }
    if (conn->last_read_time_ < oldest_time)
      oldest_time = conn->last_read_time_;
    iter++;
  }
  if ((cur_time - oldest_time) >= idle_socket_timeout_s_)
    oldest_time = cur_time;
}

void SMAcceptorThread::Run() {
  while (!quitting_.HasBeenNotified()) {
    epoll_server_.set_timeout_in_us(10 * 1000);  // 10 ms
    epoll_server_.WaitForEventsAndExecuteCallbacks();
    if (tmp_unused_server_connections_.size()) {
      VLOG(2) << "have " << tmp_unused_server_connections_.size()
              << " additional unused connections.  Total = "
              << unused_server_connections_.size();
      unused_server_connections_.insert(unused_server_connections_.end(),
                                        tmp_unused_server_connections_.begin(),
                                        tmp_unused_server_connections_.end());
      tmp_unused_server_connections_.clear();
    }
    HandleConnectionIdleTimeout();
  }
}

void SMAcceptorThread::OnEvent(int fd, EpollEvent* event) {
  if (event->in_events | EPOLLIN) {
    VLOG(2) << ACCEPTOR_CLIENT_IDENT
            << "Acceptor: Accepting based upon epoll events";
    AcceptFromListenFD();
  }
}

void SMAcceptorThread::SMConnectionDone(SMConnection* sc) {
  VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Done with connection.";
  tmp_unused_server_connections_.push_back(sc);
}

}  // namespace net
