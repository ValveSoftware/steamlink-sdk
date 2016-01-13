// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_IMPL_H_
#define NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_IMPL_H_

#include <map>
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/template_util.h"
#include "base/threading/non_thread_safe.h"
#include "net/cert/cert_database.h"
#include "net/http/http_network_session.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/client_socket_pool_manager.h"

namespace net {

class CertVerifier;
class ClientSocketFactory;
class ClientSocketPoolHistograms;
class CTVerifier;
class HttpProxyClientSocketPool;
class HostResolver;
class NetLog;
class ServerBoundCertService;
class ProxyService;
class SOCKSClientSocketPool;
class SSLClientSocketPool;
class SSLConfigService;
class TransportClientSocketPool;
class TransportSecurityState;

namespace internal {

// A helper class for auto-deleting Values in the destructor.
template <typename Key, typename Value>
class OwnedPoolMap : public std::map<Key, Value> {
 public:
  OwnedPoolMap() {
    COMPILE_ASSERT(base::is_pointer<Value>::value,
                   value_must_be_a_pointer);
  }

  ~OwnedPoolMap() {
    STLDeleteValues(this);
  }
};

}  // namespace internal

class ClientSocketPoolManagerImpl : public base::NonThreadSafe,
                                    public ClientSocketPoolManager,
                                    public CertDatabase::Observer {
 public:
  ClientSocketPoolManagerImpl(NetLog* net_log,
                              ClientSocketFactory* socket_factory,
                              HostResolver* host_resolver,
                              CertVerifier* cert_verifier,
                              ServerBoundCertService* server_bound_cert_service,
                              TransportSecurityState* transport_security_state,
                              CTVerifier* cert_transparency_verifier,
                              const std::string& ssl_session_cache_shard,
                              ProxyService* proxy_service,
                              SSLConfigService* ssl_config_service,
                              HttpNetworkSession::SocketPoolType pool_type);
  virtual ~ClientSocketPoolManagerImpl();

  virtual void FlushSocketPoolsWithError(int error) OVERRIDE;
  virtual void CloseIdleSockets() OVERRIDE;

  virtual TransportClientSocketPool* GetTransportSocketPool() OVERRIDE;

  virtual SSLClientSocketPool* GetSSLSocketPool() OVERRIDE;

  virtual SOCKSClientSocketPool* GetSocketPoolForSOCKSProxy(
      const HostPortPair& socks_proxy) OVERRIDE;

  virtual HttpProxyClientSocketPool* GetSocketPoolForHTTPProxy(
      const HostPortPair& http_proxy) OVERRIDE;

  virtual SSLClientSocketPool* GetSocketPoolForSSLWithProxy(
      const HostPortPair& proxy_server) OVERRIDE;

  // Creates a Value summary of the state of the socket pools. The caller is
  // responsible for deleting the returned value.
  virtual base::Value* SocketPoolInfoToValue() const OVERRIDE;

  // CertDatabase::Observer methods:
  virtual void OnCertAdded(const X509Certificate* cert) OVERRIDE;
  virtual void OnCACertChanged(const X509Certificate* cert) OVERRIDE;

 private:
  typedef internal::OwnedPoolMap<HostPortPair, TransportClientSocketPool*>
      TransportSocketPoolMap;
  typedef internal::OwnedPoolMap<HostPortPair, SOCKSClientSocketPool*>
      SOCKSSocketPoolMap;
  typedef internal::OwnedPoolMap<HostPortPair, HttpProxyClientSocketPool*>
      HTTPProxySocketPoolMap;
  typedef internal::OwnedPoolMap<HostPortPair, SSLClientSocketPool*>
      SSLSocketPoolMap;

  NetLog* const net_log_;
  ClientSocketFactory* const socket_factory_;
  HostResolver* const host_resolver_;
  CertVerifier* const cert_verifier_;
  ServerBoundCertService* const server_bound_cert_service_;
  TransportSecurityState* const transport_security_state_;
  CTVerifier* const cert_transparency_verifier_;
  const std::string ssl_session_cache_shard_;
  ProxyService* const proxy_service_;
  const scoped_refptr<SSLConfigService> ssl_config_service_;
  const HttpNetworkSession::SocketPoolType pool_type_;

  // Note: this ordering is important.

  ClientSocketPoolHistograms transport_pool_histograms_;
  scoped_ptr<TransportClientSocketPool> transport_socket_pool_;

  ClientSocketPoolHistograms ssl_pool_histograms_;
  scoped_ptr<SSLClientSocketPool> ssl_socket_pool_;

  ClientSocketPoolHistograms transport_for_socks_pool_histograms_;
  TransportSocketPoolMap transport_socket_pools_for_socks_proxies_;

  ClientSocketPoolHistograms socks_pool_histograms_;
  SOCKSSocketPoolMap socks_socket_pools_;

  ClientSocketPoolHistograms transport_for_http_proxy_pool_histograms_;
  TransportSocketPoolMap transport_socket_pools_for_http_proxies_;

  ClientSocketPoolHistograms transport_for_https_proxy_pool_histograms_;
  TransportSocketPoolMap transport_socket_pools_for_https_proxies_;

  ClientSocketPoolHistograms ssl_for_https_proxy_pool_histograms_;
  SSLSocketPoolMap ssl_socket_pools_for_https_proxies_;

  ClientSocketPoolHistograms http_proxy_pool_histograms_;
  HTTPProxySocketPoolMap http_proxy_socket_pools_;

  ClientSocketPoolHistograms ssl_socket_pool_for_proxies_histograms_;
  SSLSocketPoolMap ssl_socket_pools_for_proxies_;

  DISALLOW_COPY_AND_ASSIGN(ClientSocketPoolManagerImpl);
};

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_IMPL_H_
