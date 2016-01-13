// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_SESSION_POOL_H_
#define NET_SPDY_SPDY_SESSION_POOL_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/cert/cert_database.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_server.h"
#include "net/socket/next_proto.h"
#include "net/spdy/spdy_session_key.h"
#include "net/ssl/ssl_config_service.h"

namespace net {

class AddressList;
class BoundNetLog;
class ClientSocketHandle;
class HostResolver;
class HttpServerProperties;
class SpdySession;

// This is a very simple pool for open SpdySessions.
class NET_EXPORT SpdySessionPool
    : public NetworkChangeNotifier::IPAddressObserver,
      public SSLConfigService::Observer,
      public CertDatabase::Observer {
 public:
  typedef base::TimeTicks (*TimeFunc)(void);

  // |default_protocol| may be kProtoUnknown (e.g., if SPDY is
  // disabled), in which case it's set to a default value. Otherwise,
  // it must be a SPDY protocol.
  SpdySessionPool(
      HostResolver* host_resolver,
      SSLConfigService* ssl_config_service,
      const base::WeakPtr<HttpServerProperties>& http_server_properties,
      bool force_single_domain,
      bool enable_compression,
      bool enable_ping_based_connection_checking,
      NextProto default_protocol,
      size_t stream_initial_recv_window_size,
      size_t initial_max_concurrent_streams,
      size_t max_concurrent_streams_limit,
      SpdySessionPool::TimeFunc time_func,
      const std::string& trusted_spdy_proxy);
  virtual ~SpdySessionPool();

  // In the functions below, a session is "available" if this pool has
  // a reference to it and there is some SpdySessionKey for which
  // FindAvailableSession() will return it. A session is "unavailable"
  // if this pool has a reference to it but it won't be returned by
  // FindAvailableSession() for any SpdySessionKey; for example, this
  // can happen when a session receives a GOAWAY frame and is still
  // processing existing streams.

  // Create a new SPDY session from an existing socket.  There must
  // not already be a session for the given key. This pool must have
  // been constructed with a valid |default_protocol| value.
  //
  // |is_secure| can be false for testing or when SPDY is configured
  // to work with non-secure sockets. If |is_secure| is true,
  // |certificate_error_code| indicates that the certificate error
  // encountered when connecting the SSL socket, with OK meaning there
  // was no error.
  //
  // Returns the new SpdySession. Note that the SpdySession begins reading from
  // |connection| on a subsequent event loop iteration, so it may be closed
  // immediately afterwards if the first read of |connection| fails.
  base::WeakPtr<SpdySession> CreateAvailableSessionFromSocket(
      const SpdySessionKey& key,
      scoped_ptr<ClientSocketHandle> connection,
      const BoundNetLog& net_log,
      int certificate_error_code,
      bool is_secure);

  // Find an available session for the given key, or NULL if there isn't one.
  base::WeakPtr<SpdySession> FindAvailableSession(const SpdySessionKey& key,
                                                  const BoundNetLog& net_log);

  // Remove all mappings and aliases for the given session, which must
  // still be available. Except for in tests, this must be called by
  // the given session itself.
  void MakeSessionUnavailable(
      const base::WeakPtr<SpdySession>& available_session);

  // Removes an unavailable session from the pool.  Except for in
  // tests, this must be called by the given session itself.
  void RemoveUnavailableSession(
      const base::WeakPtr<SpdySession>& unavailable_session);

  // Close only the currently existing SpdySessions with |error|.
  // Let any new ones created while this method is running continue to
  // live.
  void CloseCurrentSessions(net::Error error);

  // Close only the currently existing SpdySessions that are idle.
  // Let any new ones created while this method is running continue to
  // live.
  void CloseCurrentIdleSessions();

  // Close all SpdySessions, including any new ones created in the process of
  // closing the current ones.
  void CloseAllSessions();

  // Creates a Value summary of the state of the spdy session pool. The caller
  // responsible for deleting the returned value.
  base::Value* SpdySessionPoolInfoToValue() const;

  base::WeakPtr<HttpServerProperties> http_server_properties() {
    return http_server_properties_;
  }

  // NetworkChangeNotifier::IPAddressObserver methods:

  // We flush all idle sessions and release references to the active ones so
  // they won't get re-used.  The active ones will either complete successfully
  // or error out due to the IP address change.
  virtual void OnIPAddressChanged() OVERRIDE;

  // SSLConfigService::Observer methods:

  // We perform the same flushing as described above when SSL settings change.
  virtual void OnSSLConfigChanged() OVERRIDE;

  // CertDatabase::Observer methods:

  // We perform the same flushing as described above when certificate database
  // is changed.
  virtual void OnCertAdded(const X509Certificate* cert) OVERRIDE;
  virtual void OnCACertChanged(const X509Certificate* cert) OVERRIDE;

 private:
  friend class SpdySessionPoolPeer;  // For testing.

  typedef std::set<SpdySession*> SessionSet;
  typedef std::vector<base::WeakPtr<SpdySession> > WeakSessionList;
  typedef std::map<SpdySessionKey, base::WeakPtr<SpdySession> >
      AvailableSessionMap;
  typedef std::map<IPEndPoint, SpdySessionKey> AliasMap;

  // Returns true iff |session| is in |available_sessions_|.
  bool IsSessionAvailable(const base::WeakPtr<SpdySession>& session) const;

  // Returns a normalized version of the given key suitable for lookup
  // into |available_sessions_|.
  const SpdySessionKey& NormalizeListKey(const SpdySessionKey& key) const;

  // Map the given key to the given session. There must not already be
  // a mapping for |key|.
  void MapKeyToAvailableSession(const SpdySessionKey& key,
                                const base::WeakPtr<SpdySession>& session);

  // Returns an iterator into |available_sessions_| for the given key,
  // which may be equal to |available_sessions_.end()|.
  AvailableSessionMap::iterator LookupAvailableSessionByKey(
      const SpdySessionKey& key);

  // Remove the mapping of the given key, which must exist.
  void UnmapKey(const SpdySessionKey& key);

  // Remove all aliases for |key| from the aliases table.
  void RemoveAliases(const SpdySessionKey& key);

  // Get a copy of the current sessions as a list of WeakPtrs. Used by
  // CloseCurrentSessionsHelper() below.
  WeakSessionList GetCurrentSessions() const;

  // Close only the currently existing SpdySessions with |error|.  Let
  // any new ones created while this method is running continue to
  // live. If |idle_only| is true only idle sessions are closed.
  void CloseCurrentSessionsHelper(
      Error error,
      const std::string& description,
      bool idle_only);

  const base::WeakPtr<HttpServerProperties> http_server_properties_;

  // The set of all sessions. This is a superset of the sessions in
  // |available_sessions_|.
  //
  // |sessions_| owns all its SpdySession objects.
  SessionSet sessions_;

  // This is a map of available sessions by key. A session may appear
  // more than once in this map if it has aliases.
  AvailableSessionMap available_sessions_;

  // A map of IPEndPoint aliases for sessions.
  AliasMap aliases_;

  static bool g_force_single_domain;

  const scoped_refptr<SSLConfigService> ssl_config_service_;
  HostResolver* const resolver_;

  // Defaults to true. May be controlled via SpdySessionPoolPeer for tests.
  bool verify_domain_authentication_;
  bool enable_sending_initial_data_;
  bool force_single_domain_;
  bool enable_compression_;
  bool enable_ping_based_connection_checking_;
  const NextProto default_protocol_;
  size_t stream_initial_recv_window_size_;
  size_t initial_max_concurrent_streams_;
  size_t max_concurrent_streams_limit_;
  TimeFunc time_func_;

  // This SPDY proxy is allowed to push resources from origins that are
  // different from those of their associated streams.
  HostPortPair trusted_spdy_proxy_;

  DISALLOW_COPY_AND_ASSIGN(SpdySessionPool);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_SESSION_POOL_H_
