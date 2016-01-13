// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool_manager.h"

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "net/base/load_flags.h"
#include "net/http/http_proxy_client_socket_pool.h"
#include "net/http/http_request_info.h"
#include "net/http/http_stream_factory.h"
#include "net/proxy/proxy_info.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/ssl_client_socket_pool.h"
#include "net/socket/transport_client_socket_pool.h"

namespace net {

namespace {

// Limit of sockets of each socket pool.
int g_max_sockets_per_pool[] = {
  256,  // NORMAL_SOCKET_POOL
  256   // WEBSOCKET_SOCKET_POOL
};

COMPILE_ASSERT(arraysize(g_max_sockets_per_pool) ==
                   HttpNetworkSession::NUM_SOCKET_POOL_TYPES,
               max_sockets_per_pool_length_mismatch);

// Default to allow up to 6 connections per host. Experiment and tuning may
// try other values (greater than 0).  Too large may cause many problems, such
// as home routers blocking the connections!?!?  See http://crbug.com/12066.
//
// WebSocket connections are long-lived, and should be treated differently
// than normal other connections. 6 connections per group sounded too small
// for such use, thus we use a larger limit which was determined somewhat
// arbitrarily.
// TODO(yutak): Look at the usage and determine the right value after
// WebSocket protocol stack starts to work.
int g_max_sockets_per_group[] = {
  6,  // NORMAL_SOCKET_POOL
  30  // WEBSOCKET_SOCKET_POOL
};

COMPILE_ASSERT(arraysize(g_max_sockets_per_group) ==
                   HttpNetworkSession::NUM_SOCKET_POOL_TYPES,
               max_sockets_per_group_length_mismatch);

// The max number of sockets to allow per proxy server.  This applies both to
// http and SOCKS proxies.  See http://crbug.com/12066 and
// http://crbug.com/44501 for details about proxy server connection limits.
int g_max_sockets_per_proxy_server[] = {
  kDefaultMaxSocketsPerProxyServer,  // NORMAL_SOCKET_POOL
  kDefaultMaxSocketsPerProxyServer   // WEBSOCKET_SOCKET_POOL
};

COMPILE_ASSERT(arraysize(g_max_sockets_per_proxy_server) ==
                   HttpNetworkSession::NUM_SOCKET_POOL_TYPES,
               max_sockets_per_proxy_server_length_mismatch);

// The meat of the implementation for the InitSocketHandleForHttpRequest,
// InitSocketHandleForRawConnect and PreconnectSocketsForHttpRequest methods.
int InitSocketPoolHelper(const GURL& request_url,
                         const HttpRequestHeaders& request_extra_headers,
                         int request_load_flags,
                         RequestPriority request_priority,
                         HttpNetworkSession* session,
                         const ProxyInfo& proxy_info,
                         bool force_spdy_over_ssl,
                         bool want_spdy_over_npn,
                         const SSLConfig& ssl_config_for_origin,
                         const SSLConfig& ssl_config_for_proxy,
                         bool force_tunnel,
                         PrivacyMode privacy_mode,
                         const BoundNetLog& net_log,
                         int num_preconnect_streams,
                         ClientSocketHandle* socket_handle,
                         HttpNetworkSession::SocketPoolType socket_pool_type,
                         const OnHostResolutionCallback& resolution_callback,
                         const CompletionCallback& callback) {
  scoped_refptr<TransportSocketParams> tcp_params;
  scoped_refptr<HttpProxySocketParams> http_proxy_params;
  scoped_refptr<SOCKSSocketParams> socks_params;
  scoped_ptr<HostPortPair> proxy_host_port;

  bool using_ssl = request_url.SchemeIs("https") ||
      request_url.SchemeIs("wss") || force_spdy_over_ssl;

  HostPortPair origin_host_port =
      HostPortPair(request_url.HostNoBrackets(),
                   request_url.EffectiveIntPort());

  if (!using_ssl && session->params().testing_fixed_http_port != 0) {
    origin_host_port.set_port(session->params().testing_fixed_http_port);
  } else if (using_ssl && session->params().testing_fixed_https_port != 0) {
    origin_host_port.set_port(session->params().testing_fixed_https_port);
  }

  bool disable_resolver_cache =
      request_load_flags & LOAD_BYPASS_CACHE ||
      request_load_flags & LOAD_VALIDATE_CACHE ||
      request_load_flags & LOAD_DISABLE_CACHE;

  int load_flags = request_load_flags;
  if (session->params().ignore_certificate_errors)
    load_flags |= LOAD_IGNORE_ALL_CERT_ERRORS;

  // Build the string used to uniquely identify connections of this type.
  // Determine the host and port to connect to.
  std::string connection_group = origin_host_port.ToString();
  DCHECK(!connection_group.empty());
  if (request_url.SchemeIs("ftp")) {
    // Combining FTP with forced SPDY over SSL would be a "path to madness".
    // Make sure we never do that.
    DCHECK(!using_ssl);
    connection_group = "ftp/" + connection_group;
  }
  if (using_ssl) {
    // All connections in a group should use the same SSLConfig settings.
    // Encode version_max in the connection group's name, unless it's the
    // default version_max. (We want the common case to use the shortest
    // encoding). A version_max of TLS 1.1 is encoded as "ssl(max:3.2)/"
    // rather than "tlsv1.1/" because the actual protocol version, which
    // is selected by the server, may not be TLS 1.1. Do not encode
    // version_min in the connection group's name because version_min
    // should be the same for all connections, whereas version_max may
    // change for version fallbacks.
    std::string prefix = "ssl/";
    if (ssl_config_for_origin.version_max != net::kDefaultSSLVersionMax) {
      switch (ssl_config_for_origin.version_max) {
        case SSL_PROTOCOL_VERSION_TLS1_2:
          prefix = "ssl(max:3.3)/";
          break;
        case SSL_PROTOCOL_VERSION_TLS1_1:
          prefix = "ssl(max:3.2)/";
          break;
        case SSL_PROTOCOL_VERSION_TLS1:
          prefix = "ssl(max:3.1)/";
          break;
        case SSL_PROTOCOL_VERSION_SSL3:
          prefix = "sslv3/";
          break;
        default:
          CHECK(false);
          break;
      }
    }
    connection_group = prefix + connection_group;
  }

  bool ignore_limits = (request_load_flags & LOAD_IGNORE_LIMITS) != 0;
  if (proxy_info.is_direct()) {
    tcp_params = new TransportSocketParams(origin_host_port,
                                           disable_resolver_cache,
                                           ignore_limits,
                                           resolution_callback);
  } else {
    ProxyServer proxy_server = proxy_info.proxy_server();
    proxy_host_port.reset(new HostPortPair(proxy_server.host_port_pair()));
    scoped_refptr<TransportSocketParams> proxy_tcp_params(
        new TransportSocketParams(*proxy_host_port,
                                  disable_resolver_cache,
                                  ignore_limits,
                                  resolution_callback));

    if (proxy_info.is_http() || proxy_info.is_https()) {
      std::string user_agent;
      request_extra_headers.GetHeader(HttpRequestHeaders::kUserAgent,
                                      &user_agent);
      scoped_refptr<SSLSocketParams> ssl_params;
      if (proxy_info.is_https()) {
        // Set ssl_params, and unset proxy_tcp_params
        ssl_params = new SSLSocketParams(proxy_tcp_params,
                                         NULL,
                                         NULL,
                                         *proxy_host_port.get(),
                                         ssl_config_for_proxy,
                                         PRIVACY_MODE_DISABLED,
                                         load_flags,
                                         force_spdy_over_ssl,
                                         want_spdy_over_npn);
        proxy_tcp_params = NULL;
      }

      http_proxy_params =
          new HttpProxySocketParams(proxy_tcp_params,
                                    ssl_params,
                                    request_url,
                                    user_agent,
                                    origin_host_port,
                                    session->http_auth_cache(),
                                    session->http_auth_handler_factory(),
                                    session->spdy_session_pool(),
                                    force_tunnel || using_ssl);
    } else {
      DCHECK(proxy_info.is_socks());
      char socks_version;
      if (proxy_server.scheme() == ProxyServer::SCHEME_SOCKS5)
        socks_version = '5';
      else
        socks_version = '4';
      connection_group = base::StringPrintf(
          "socks%c/%s", socks_version, connection_group.c_str());

      socks_params = new SOCKSSocketParams(proxy_tcp_params,
                                           socks_version == '5',
                                           origin_host_port);
    }
  }

  // Change group name if privacy mode is enabled.
  if (privacy_mode == PRIVACY_MODE_ENABLED)
    connection_group = "pm/" + connection_group;

  // Deal with SSL - which layers on top of any given proxy.
  if (using_ssl) {
    scoped_refptr<SSLSocketParams> ssl_params =
        new SSLSocketParams(tcp_params,
                            socks_params,
                            http_proxy_params,
                            origin_host_port,
                            ssl_config_for_origin,
                            privacy_mode,
                            load_flags,
                            force_spdy_over_ssl,
                            want_spdy_over_npn);
    SSLClientSocketPool* ssl_pool = NULL;
    if (proxy_info.is_direct()) {
      ssl_pool = session->GetSSLSocketPool(socket_pool_type);
    } else {
      ssl_pool = session->GetSocketPoolForSSLWithProxy(socket_pool_type,
                                                       *proxy_host_port);
    }

    if (num_preconnect_streams) {
      RequestSocketsForPool(ssl_pool, connection_group, ssl_params,
                            num_preconnect_streams, net_log);
      return OK;
    }

    return socket_handle->Init(connection_group, ssl_params,
                               request_priority, callback, ssl_pool,
                               net_log);
  }

  // Finally, get the connection started.

  if (proxy_info.is_http() || proxy_info.is_https()) {
    HttpProxyClientSocketPool* pool =
        session->GetSocketPoolForHTTPProxy(socket_pool_type, *proxy_host_port);
    if (num_preconnect_streams) {
      RequestSocketsForPool(pool, connection_group, http_proxy_params,
                            num_preconnect_streams, net_log);
      return OK;
    }

    return socket_handle->Init(connection_group, http_proxy_params,
                               request_priority, callback,
                               pool, net_log);
  }

  if (proxy_info.is_socks()) {
    SOCKSClientSocketPool* pool =
        session->GetSocketPoolForSOCKSProxy(socket_pool_type, *proxy_host_port);
    if (num_preconnect_streams) {
      RequestSocketsForPool(pool, connection_group, socks_params,
                            num_preconnect_streams, net_log);
      return OK;
    }

    return socket_handle->Init(connection_group, socks_params,
                               request_priority, callback, pool,
                               net_log);
  }

  DCHECK(proxy_info.is_direct());

  TransportClientSocketPool* pool =
      session->GetTransportSocketPool(socket_pool_type);
  if (num_preconnect_streams) {
    RequestSocketsForPool(pool, connection_group, tcp_params,
                          num_preconnect_streams, net_log);
    return OK;
  }

  return socket_handle->Init(connection_group, tcp_params,
                             request_priority, callback,
                             pool, net_log);
}

}  // namespace

ClientSocketPoolManager::ClientSocketPoolManager() {}
ClientSocketPoolManager::~ClientSocketPoolManager() {}

// static
int ClientSocketPoolManager::max_sockets_per_pool(
    HttpNetworkSession::SocketPoolType pool_type) {
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  return g_max_sockets_per_pool[pool_type];
}

// static
void ClientSocketPoolManager::set_max_sockets_per_pool(
    HttpNetworkSession::SocketPoolType pool_type,
    int socket_count) {
  DCHECK_LT(0, socket_count);
  DCHECK_GT(1000, socket_count);  // Sanity check.
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  g_max_sockets_per_pool[pool_type] = socket_count;
  DCHECK_GE(g_max_sockets_per_pool[pool_type],
            g_max_sockets_per_group[pool_type]);
}

// static
int ClientSocketPoolManager::max_sockets_per_group(
    HttpNetworkSession::SocketPoolType pool_type) {
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  return g_max_sockets_per_group[pool_type];
}

// static
void ClientSocketPoolManager::set_max_sockets_per_group(
    HttpNetworkSession::SocketPoolType pool_type,
    int socket_count) {
  DCHECK_LT(0, socket_count);
  // The following is a sanity check... but we should NEVER be near this value.
  DCHECK_GT(100, socket_count);
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  g_max_sockets_per_group[pool_type] = socket_count;

  DCHECK_GE(g_max_sockets_per_pool[pool_type],
            g_max_sockets_per_group[pool_type]);
  DCHECK_GE(g_max_sockets_per_proxy_server[pool_type],
            g_max_sockets_per_group[pool_type]);
}

// static
int ClientSocketPoolManager::max_sockets_per_proxy_server(
    HttpNetworkSession::SocketPoolType pool_type) {
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  return g_max_sockets_per_proxy_server[pool_type];
}

// static
void ClientSocketPoolManager::set_max_sockets_per_proxy_server(
    HttpNetworkSession::SocketPoolType pool_type,
    int socket_count) {
  DCHECK_LT(0, socket_count);
  DCHECK_GT(100, socket_count);  // Sanity check.
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  // Assert this case early on. The max number of sockets per group cannot
  // exceed the max number of sockets per proxy server.
  DCHECK_LE(g_max_sockets_per_group[pool_type], socket_count);
  g_max_sockets_per_proxy_server[pool_type] = socket_count;
}

int InitSocketHandleForHttpRequest(
    const GURL& request_url,
    const HttpRequestHeaders& request_extra_headers,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    bool force_spdy_over_ssl,
    bool want_spdy_over_npn,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    PrivacyMode privacy_mode,
    const BoundNetLog& net_log,
    ClientSocketHandle* socket_handle,
    const OnHostResolutionCallback& resolution_callback,
    const CompletionCallback& callback) {
  DCHECK(socket_handle);
  return InitSocketPoolHelper(
      request_url, request_extra_headers, request_load_flags, request_priority,
      session, proxy_info, force_spdy_over_ssl, want_spdy_over_npn,
      ssl_config_for_origin, ssl_config_for_proxy, false, privacy_mode, net_log,
      0, socket_handle, HttpNetworkSession::NORMAL_SOCKET_POOL,
      resolution_callback, callback);
}

int InitSocketHandleForWebSocketRequest(
    const GURL& request_url,
    const HttpRequestHeaders& request_extra_headers,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    bool force_spdy_over_ssl,
    bool want_spdy_over_npn,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    PrivacyMode privacy_mode,
    const BoundNetLog& net_log,
    ClientSocketHandle* socket_handle,
    const OnHostResolutionCallback& resolution_callback,
    const CompletionCallback& callback) {
  DCHECK(socket_handle);
  return InitSocketPoolHelper(
      request_url, request_extra_headers, request_load_flags, request_priority,
      session, proxy_info, force_spdy_over_ssl, want_spdy_over_npn,
      ssl_config_for_origin, ssl_config_for_proxy, true, privacy_mode, net_log,
      0, socket_handle, HttpNetworkSession::WEBSOCKET_SOCKET_POOL,
      resolution_callback, callback);
}

int InitSocketHandleForRawConnect(
    const HostPortPair& host_port_pair,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    PrivacyMode privacy_mode,
    const BoundNetLog& net_log,
    ClientSocketHandle* socket_handle,
    const CompletionCallback& callback) {
  DCHECK(socket_handle);
  // Synthesize an HttpRequestInfo.
  GURL request_url = GURL("http://" + host_port_pair.ToString());
  HttpRequestHeaders request_extra_headers;
  int request_load_flags = 0;
  RequestPriority request_priority = MEDIUM;

  return InitSocketPoolHelper(
      request_url, request_extra_headers, request_load_flags, request_priority,
      session, proxy_info, false, false, ssl_config_for_origin,
      ssl_config_for_proxy, true, privacy_mode, net_log, 0, socket_handle,
      HttpNetworkSession::NORMAL_SOCKET_POOL, OnHostResolutionCallback(),
      callback);
}

int InitSocketHandleForTlsConnect(
    const HostPortPair& host_port_pair,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    PrivacyMode privacy_mode,
    const BoundNetLog& net_log,
    ClientSocketHandle* socket_handle,
    const CompletionCallback& callback) {
  DCHECK(socket_handle);
  // Synthesize an HttpRequestInfo.
  GURL request_url = GURL("https://" + host_port_pair.ToString());
  HttpRequestHeaders request_extra_headers;
  int request_load_flags = 0;
  RequestPriority request_priority = MEDIUM;

  return InitSocketPoolHelper(
      request_url, request_extra_headers, request_load_flags, request_priority,
      session, proxy_info, false, false, ssl_config_for_origin,
      ssl_config_for_proxy, true, privacy_mode, net_log, 0, socket_handle,
      HttpNetworkSession::NORMAL_SOCKET_POOL, OnHostResolutionCallback(),
      callback);
}

int PreconnectSocketsForHttpRequest(
    const GURL& request_url,
    const HttpRequestHeaders& request_extra_headers,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    bool force_spdy_over_ssl,
    bool want_spdy_over_npn,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    PrivacyMode privacy_mode,
    const BoundNetLog& net_log,
    int num_preconnect_streams) {
  return InitSocketPoolHelper(
      request_url, request_extra_headers, request_load_flags, request_priority,
      session, proxy_info, force_spdy_over_ssl, want_spdy_over_npn,
      ssl_config_for_origin, ssl_config_for_proxy, false, privacy_mode, net_log,
      num_preconnect_streams, NULL, HttpNetworkSession::NORMAL_SOCKET_POOL,
      OnHostResolutionCallback(), CompletionCallback());
}

}  // namespace net
