// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_FLIP_CONFIG_H_
#define NET_TOOLS_FLIP_SERVER_FLIP_CONFIG_H_

#include <arpa/inet.h>

#include <string>
#include <vector>

#include "base/logging.h"

namespace net {

enum FlipHandlerType {
  FLIP_HANDLER_PROXY,
  FLIP_HANDLER_SPDY_SERVER,
  FLIP_HANDLER_HTTP_SERVER
};

class FlipAcceptor {
 public:
  FlipAcceptor(enum FlipHandlerType flip_handler_type,
               std::string listen_ip,
               std::string listen_port,
               std::string ssl_cert_filename,
               std::string ssl_key_filename,
               std::string http_server_ip,
               std::string http_server_port,
               std::string https_server_ip,
               std::string https_server_port,
               int spdy_only,
               int accept_backlog_size,
               bool disable_nagle,
               int accepts_per_wake,
               bool reuseport,
               bool wait_for_iface,
               void* memory_cache);
  ~FlipAcceptor();

  enum FlipHandlerType flip_handler_type_;
  std::string listen_ip_;
  std::string listen_port_;
  std::string ssl_cert_filename_;
  std::string ssl_key_filename_;
  std::string http_server_ip_;
  std::string http_server_port_;
  std::string https_server_ip_;
  std::string https_server_port_;
  int spdy_only_;
  int accept_backlog_size_;
  bool disable_nagle_;
  int accepts_per_wake_;
  int listen_fd_;
  void* memory_cache_;
  int ssl_session_expiry_;
  bool ssl_disable_compression_;
  int idle_socket_timeout_s_;
};

class FlipConfig {
 public:
  FlipConfig();
  ~FlipConfig();

  void AddAcceptor(enum FlipHandlerType flip_handler_type,
                   std::string listen_ip,
                   std::string listen_port,
                   std::string ssl_cert_filename,
                   std::string ssl_key_filename,
                   std::string http_server_ip,
                   std::string http_server_port,
                   std::string https_server_ip,
                   std::string https_server_port,
                   int spdy_only,
                   int accept_backlog_size,
                   bool disable_nagle,
                   int accepts_per_wake,
                   bool reuseport,
                   bool wait_for_iface,
                   void* memory_cache);

  std::vector<FlipAcceptor*> acceptors_;
  double server_think_time_in_s_;
  enum logging::LoggingDestination log_destination_;
  std::string log_filename_;
  bool wait_for_iface_;
  int ssl_session_expiry_;
  bool ssl_disable_compression_;
  int idle_socket_timeout_s_;
};

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_FLIP_CONFIG_H_
