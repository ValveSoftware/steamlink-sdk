// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_SM_CONNECTION_H_
#define NET_TOOLS_FLIP_SERVER_SM_CONNECTION_H_

#include <arpa/inet.h>  // in_addr_t
#include <time.h>

#include <list>
#include <string>

#include "base/compiler_specific.h"
#include "net/spdy/spdy_protocol.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/flip_server/create_listener.h"
#include "net/tools/flip_server/mem_cache.h"
#include "net/tools/flip_server/ring_buffer.h"
#include "net/tools/flip_server/sm_interface.h"
#include "openssl/ssl.h"

namespace net {

class FlipAcceptor;
class MemoryCache;
struct SSLState;
class SpdySM;

// A frame of data to be sent.
class DataFrame {
 public:
  const char* data;
  size_t size;
  bool delete_when_done;
  size_t index;
  DataFrame() : data(NULL), size(0), delete_when_done(false), index(0) {}
  virtual ~DataFrame();
};

typedef std::list<DataFrame*> OutputList;

class SMConnection : public SMConnectionInterface,
                     public EpollCallbackInterface,
                     public NotifierInterface {
 public:
  virtual ~SMConnection();

  static SMConnection* NewSMConnection(EpollServer* epoll_server,
                                       SSLState* ssl_state,
                                       MemoryCache* memory_cache,
                                       FlipAcceptor* acceptor,
                                       std::string log_prefix);

  // TODO(mbelshe): Make these private.
  time_t last_read_time_;
  std::string server_ip_;
  std::string server_port_;

  virtual EpollServer* epoll_server() OVERRIDE;
  OutputList* output_list() { return &output_list_; }
  MemoryCache* memory_cache() { return memory_cache_; }
  virtual void ReadyToSend() OVERRIDE;
  void EnqueueDataFrame(DataFrame* df);

  int fd() const { return fd_; }
  bool initialized() const { return initialized_; }
  std::string client_ip() const { return client_ip_; }

  virtual void InitSMConnection(SMConnectionPoolInterface* connection_pool,
                                SMInterface* sm_interface,
                                EpollServer* epoll_server,
                                int fd,
                                std::string server_ip,
                                std::string server_port,
                                std::string remote_ip,
                                bool use_ssl);

  void CorkSocket();
  void UncorkSocket();

  int Send(const char* data, int len, int flags);

  // EpollCallbackInterface interface.
  virtual void OnRegistration(EpollServer* eps,
                              int fd,
                              int event_mask) OVERRIDE;
  virtual void OnModification(int fd, int event_mask) OVERRIDE {}
  virtual void OnEvent(int fd, EpollEvent* event) OVERRIDE;
  virtual void OnUnregistration(int fd, bool replaced) OVERRIDE;
  virtual void OnShutdown(EpollServer* eps, int fd) OVERRIDE;

  // NotifierInterface interface.
  virtual void Notify() OVERRIDE {}

  void Cleanup(const char* cleanup);

  // Flag indicating if we should force spdy on all connections.
  static bool force_spdy() { return force_spdy_; }
  static void set_force_spdy(bool value) { force_spdy_ = value; }

 private:
  // Decide if SPDY was negotiated.
  bool WasSpdyNegotiated(SpdyMajorVersion* version_negotiated);

  // Initialize the protocol interfaces we'll need for this connection.
  // Returns true if successful, false otherwise.
  bool SetupProtocolInterfaces();

  bool DoRead();
  bool DoWrite();
  bool DoConsumeReadData();
  void Reset();

  void HandleEvents();
  void HandleResponseFullyRead();

 protected:
  friend std::ostream& operator<<(std::ostream& os, const SMConnection& c) {
    os << &c << "\n";
    return os;
  }

  SMConnection(EpollServer* epoll_server,
               SSLState* ssl_state,
               MemoryCache* memory_cache,
               FlipAcceptor* acceptor,
               std::string log_prefix);

 private:
  int fd_;
  int events_;

  bool registered_in_epoll_server_;
  bool initialized_;
  bool protocol_detected_;
  bool connection_complete_;

  SMConnectionPoolInterface* connection_pool_;

  EpollServer* epoll_server_;
  SSLState* ssl_state_;
  MemoryCache* memory_cache_;
  FlipAcceptor* acceptor_;
  std::string client_ip_;

  RingBuffer read_buffer_;

  OutputList output_list_;
  SpdySM* sm_spdy_interface_;
  SMInterface* sm_http_interface_;
  SMInterface* sm_streamer_interface_;
  SMInterface* sm_interface_;
  std::string log_prefix_;

  size_t max_bytes_sent_per_dowrite_;

  SSL* ssl_;

  static bool force_spdy_;
};

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_SM_CONNECTION_H_
