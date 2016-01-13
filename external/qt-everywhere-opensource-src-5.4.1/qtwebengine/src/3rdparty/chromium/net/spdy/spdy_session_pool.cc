// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session_pool.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/spdy/spdy_session.h"


namespace net {

namespace {

enum SpdySessionGetTypes {
  CREATED_NEW                 = 0,
  FOUND_EXISTING              = 1,
  FOUND_EXISTING_FROM_IP_POOL = 2,
  IMPORTED_FROM_SOCKET        = 3,
  SPDY_SESSION_GET_MAX        = 4
};

}  // namespace

SpdySessionPool::SpdySessionPool(
    HostResolver* resolver,
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
    const std::string& trusted_spdy_proxy)
    : http_server_properties_(http_server_properties),
      ssl_config_service_(ssl_config_service),
      resolver_(resolver),
      verify_domain_authentication_(true),
      enable_sending_initial_data_(true),
      force_single_domain_(force_single_domain),
      enable_compression_(enable_compression),
      enable_ping_based_connection_checking_(
          enable_ping_based_connection_checking),
      // TODO(akalin): Force callers to have a valid value of
      // |default_protocol_|.
      default_protocol_(
          (default_protocol == kProtoUnknown) ?
          kProtoSPDY3 : default_protocol),
      stream_initial_recv_window_size_(stream_initial_recv_window_size),
      initial_max_concurrent_streams_(initial_max_concurrent_streams),
      max_concurrent_streams_limit_(max_concurrent_streams_limit),
      time_func_(time_func),
      trusted_spdy_proxy_(
          HostPortPair::FromString(trusted_spdy_proxy)) {
  DCHECK(default_protocol_ >= kProtoSPDYMinimumVersion &&
         default_protocol_ <= kProtoSPDYMaximumVersion);
  NetworkChangeNotifier::AddIPAddressObserver(this);
  if (ssl_config_service_.get())
    ssl_config_service_->AddObserver(this);
  CertDatabase::GetInstance()->AddObserver(this);
}

SpdySessionPool::~SpdySessionPool() {
  CloseAllSessions();

  while (!sessions_.empty()) {
    // Destroy sessions to enforce that lifetime is scoped to SpdySessionPool.
    // Write callbacks queued upon session drain are not invoked.
    RemoveUnavailableSession((*sessions_.begin())->GetWeakPtr());
  }

  if (ssl_config_service_.get())
    ssl_config_service_->RemoveObserver(this);
  NetworkChangeNotifier::RemoveIPAddressObserver(this);
  CertDatabase::GetInstance()->RemoveObserver(this);
}

base::WeakPtr<SpdySession> SpdySessionPool::CreateAvailableSessionFromSocket(
    const SpdySessionKey& key,
    scoped_ptr<ClientSocketHandle> connection,
    const BoundNetLog& net_log,
    int certificate_error_code,
    bool is_secure) {
  DCHECK_GE(default_protocol_, kProtoSPDYMinimumVersion);
  DCHECK_LE(default_protocol_, kProtoSPDYMaximumVersion);

  UMA_HISTOGRAM_ENUMERATION(
      "Net.SpdySessionGet", IMPORTED_FROM_SOCKET, SPDY_SESSION_GET_MAX);

  scoped_ptr<SpdySession> new_session(
      new SpdySession(key,
                      http_server_properties_,
                      verify_domain_authentication_,
                      enable_sending_initial_data_,
                      enable_compression_,
                      enable_ping_based_connection_checking_,
                      default_protocol_,
                      stream_initial_recv_window_size_,
                      initial_max_concurrent_streams_,
                      max_concurrent_streams_limit_,
                      time_func_,
                      trusted_spdy_proxy_,
                      net_log.net_log()));

  new_session->InitializeWithSocket(
      connection.Pass(), this, is_secure, certificate_error_code);

  base::WeakPtr<SpdySession> available_session = new_session->GetWeakPtr();
  sessions_.insert(new_session.release());
  MapKeyToAvailableSession(key, available_session);

  net_log.AddEvent(
      NetLog::TYPE_SPDY_SESSION_POOL_IMPORTED_SESSION_FROM_SOCKET,
      available_session->net_log().source().ToEventParametersCallback());

  // Look up the IP address for this session so that we can match
  // future sessions (potentially to different domains) which can
  // potentially be pooled with this one. Because GetPeerAddress()
  // reports the proxy's address instead of the origin server, check
  // to see if this is a direct connection.
  if (key.proxy_server().is_direct()) {
    IPEndPoint address;
    if (available_session->GetPeerAddress(&address) == OK)
      aliases_[address] = key;
  }

  return available_session;
}

base::WeakPtr<SpdySession> SpdySessionPool::FindAvailableSession(
    const SpdySessionKey& key,
    const BoundNetLog& net_log) {
  AvailableSessionMap::iterator it = LookupAvailableSessionByKey(key);
  if (it != available_sessions_.end()) {
    UMA_HISTOGRAM_ENUMERATION(
        "Net.SpdySessionGet", FOUND_EXISTING, SPDY_SESSION_GET_MAX);
    net_log.AddEvent(
        NetLog::TYPE_SPDY_SESSION_POOL_FOUND_EXISTING_SESSION,
        it->second->net_log().source().ToEventParametersCallback());
    return it->second;
  }

  // Look up the key's from the resolver's cache.
  net::HostResolver::RequestInfo resolve_info(key.host_port_pair());
  AddressList addresses;
  int rv = resolver_->ResolveFromCache(resolve_info, &addresses, net_log);
  DCHECK_NE(rv, ERR_IO_PENDING);
  if (rv != OK)
    return base::WeakPtr<SpdySession>();

  // Check if we have a session through a domain alias.
  for (AddressList::const_iterator address_it = addresses.begin();
       address_it != addresses.end();
       ++address_it) {
    AliasMap::const_iterator alias_it = aliases_.find(*address_it);
    if (alias_it == aliases_.end())
      continue;

    // We found an alias.
    const SpdySessionKey& alias_key = alias_it->second;

    // We can reuse this session only if the proxy and privacy
    // settings match.
    if (!(alias_key.proxy_server() == key.proxy_server()) ||
        !(alias_key.privacy_mode() == key.privacy_mode()))
      continue;

    AvailableSessionMap::iterator available_session_it =
        LookupAvailableSessionByKey(alias_key);
    if (available_session_it == available_sessions_.end()) {
      NOTREACHED();  // It shouldn't be in the aliases table if we can't get it!
      continue;
    }

    const base::WeakPtr<SpdySession>& available_session =
        available_session_it->second;
    DCHECK(ContainsKey(sessions_, available_session.get()));
    // If the session is a secure one, we need to verify that the
    // server is authenticated to serve traffic for |host_port_proxy_pair| too.
    if (!available_session->VerifyDomainAuthentication(
            key.host_port_pair().host())) {
      UMA_HISTOGRAM_ENUMERATION("Net.SpdyIPPoolDomainMatch", 0, 2);
      continue;
    }

    UMA_HISTOGRAM_ENUMERATION("Net.SpdyIPPoolDomainMatch", 1, 2);
    UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionGet",
                              FOUND_EXISTING_FROM_IP_POOL,
                              SPDY_SESSION_GET_MAX);
    net_log.AddEvent(
        NetLog::TYPE_SPDY_SESSION_POOL_FOUND_EXISTING_SESSION_FROM_IP_POOL,
        available_session->net_log().source().ToEventParametersCallback());
    // Add this session to the map so that we can find it next time.
    MapKeyToAvailableSession(key, available_session);
    available_session->AddPooledAlias(key);
    return available_session;
  }

  return base::WeakPtr<SpdySession>();
}

void SpdySessionPool::MakeSessionUnavailable(
    const base::WeakPtr<SpdySession>& available_session) {
  UnmapKey(available_session->spdy_session_key());
  RemoveAliases(available_session->spdy_session_key());
  const std::set<SpdySessionKey>& aliases = available_session->pooled_aliases();
  for (std::set<SpdySessionKey>::const_iterator it = aliases.begin();
       it != aliases.end(); ++it) {
    UnmapKey(*it);
    RemoveAliases(*it);
  }
  DCHECK(!IsSessionAvailable(available_session));
}

void SpdySessionPool::RemoveUnavailableSession(
    const base::WeakPtr<SpdySession>& unavailable_session) {
  DCHECK(!IsSessionAvailable(unavailable_session));

  unavailable_session->net_log().AddEvent(
      NetLog::TYPE_SPDY_SESSION_POOL_REMOVE_SESSION,
      unavailable_session->net_log().source().ToEventParametersCallback());

  SessionSet::iterator it = sessions_.find(unavailable_session.get());
  CHECK(it != sessions_.end());
  scoped_ptr<SpdySession> owned_session(*it);
  sessions_.erase(it);
}

// Make a copy of |sessions_| in the Close* functions below to avoid
// reentrancy problems. Since arbitrary functions get called by close
// handlers, it doesn't suffice to simply increment the iterator
// before closing.

void SpdySessionPool::CloseCurrentSessions(net::Error error) {
  CloseCurrentSessionsHelper(error, "Closing current sessions.",
                             false /* idle_only */);
}

void SpdySessionPool::CloseCurrentIdleSessions() {
  CloseCurrentSessionsHelper(ERR_ABORTED, "Closing idle sessions.",
                             true /* idle_only */);
}

void SpdySessionPool::CloseAllSessions() {
  while (!available_sessions_.empty()) {
    CloseCurrentSessionsHelper(ERR_ABORTED, "Closing all sessions.",
                               false /* idle_only */);
  }
}

base::Value* SpdySessionPool::SpdySessionPoolInfoToValue() const {
  base::ListValue* list = new base::ListValue();

  for (AvailableSessionMap::const_iterator it = available_sessions_.begin();
       it != available_sessions_.end(); ++it) {
    // Only add the session if the key in the map matches the main
    // host_port_proxy_pair (not an alias).
    const SpdySessionKey& key = it->first;
    const SpdySessionKey& session_key = it->second->spdy_session_key();
    if (key.Equals(session_key))
      list->Append(it->second->GetInfoAsValue());
  }
  return list;
}

void SpdySessionPool::OnIPAddressChanged() {
  WeakSessionList current_sessions = GetCurrentSessions();
  for (WeakSessionList::const_iterator it = current_sessions.begin();
       it != current_sessions.end(); ++it) {
    if (!*it)
      continue;

// For OSs that terminate TCP connections upon relevant network changes,
// attempt to preserve active streams by marking all sessions as going
// away, rather than explicitly closing them. Streams may still fail due
// to a generated TCP reset.
#if defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_IOS)
    (*it)->MakeUnavailable();
    (*it)->StartGoingAway(kLastStreamId, ERR_NETWORK_CHANGED);
    (*it)->MaybeFinishGoingAway();
#else
    (*it)->CloseSessionOnError(ERR_NETWORK_CHANGED,
                               "Closing current sessions.");
    DCHECK((*it)->IsDraining());
#endif  // defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_IOS)
    DCHECK(!IsSessionAvailable(*it));
  }
  http_server_properties_->ClearAllSpdySettings();
}

void SpdySessionPool::OnSSLConfigChanged() {
  CloseCurrentSessions(ERR_NETWORK_CHANGED);
}

void SpdySessionPool::OnCertAdded(const X509Certificate* cert) {
  CloseCurrentSessions(ERR_CERT_DATABASE_CHANGED);
}

void SpdySessionPool::OnCACertChanged(const X509Certificate* cert) {
  // Per wtc, we actually only need to CloseCurrentSessions when trust is
  // reduced. CloseCurrentSessions now because OnCACertChanged does not
  // tell us this.
  // See comments in ClientSocketPoolManager::OnCACertChanged.
  CloseCurrentSessions(ERR_CERT_DATABASE_CHANGED);
}

bool SpdySessionPool::IsSessionAvailable(
    const base::WeakPtr<SpdySession>& session) const {
  for (AvailableSessionMap::const_iterator it = available_sessions_.begin();
       it != available_sessions_.end(); ++it) {
    if (it->second.get() == session.get())
      return true;
  }
  return false;
}

const SpdySessionKey& SpdySessionPool::NormalizeListKey(
    const SpdySessionKey& key) const {
  if (!force_single_domain_)
    return key;

  static SpdySessionKey* single_domain_key = NULL;
  if (!single_domain_key) {
    HostPortPair single_domain = HostPortPair("singledomain.com", 80);
    single_domain_key = new SpdySessionKey(single_domain,
                                           ProxyServer::Direct(),
                                           PRIVACY_MODE_DISABLED);
  }
  return *single_domain_key;
}

void SpdySessionPool::MapKeyToAvailableSession(
    const SpdySessionKey& key,
    const base::WeakPtr<SpdySession>& session) {
  DCHECK(ContainsKey(sessions_, session.get()));
  const SpdySessionKey& normalized_key = NormalizeListKey(key);
  std::pair<AvailableSessionMap::iterator, bool> result =
      available_sessions_.insert(std::make_pair(normalized_key, session));
  CHECK(result.second);
}

SpdySessionPool::AvailableSessionMap::iterator
SpdySessionPool::LookupAvailableSessionByKey(
    const SpdySessionKey& key) {
  const SpdySessionKey& normalized_key = NormalizeListKey(key);
  return available_sessions_.find(normalized_key);
}

void SpdySessionPool::UnmapKey(const SpdySessionKey& key) {
  AvailableSessionMap::iterator it = LookupAvailableSessionByKey(key);
  CHECK(it != available_sessions_.end());
  available_sessions_.erase(it);
}

void SpdySessionPool::RemoveAliases(const SpdySessionKey& key) {
  // Walk the aliases map, find references to this pair.
  // TODO(mbelshe):  Figure out if this is too expensive.
  for (AliasMap::iterator it = aliases_.begin(); it != aliases_.end(); ) {
    if (it->second.Equals(key)) {
      AliasMap::iterator old_it = it;
      ++it;
      aliases_.erase(old_it);
    } else {
      ++it;
    }
  }
}

SpdySessionPool::WeakSessionList SpdySessionPool::GetCurrentSessions() const {
  WeakSessionList current_sessions;
  for (SessionSet::const_iterator it = sessions_.begin();
       it != sessions_.end(); ++it) {
    current_sessions.push_back((*it)->GetWeakPtr());
  }
  return current_sessions;
}

void SpdySessionPool::CloseCurrentSessionsHelper(
    Error error,
    const std::string& description,
    bool idle_only) {
  WeakSessionList current_sessions = GetCurrentSessions();
  for (WeakSessionList::const_iterator it = current_sessions.begin();
       it != current_sessions.end(); ++it) {
    if (!*it)
      continue;

    if (idle_only && (*it)->is_active())
      continue;

    (*it)->CloseSessionOnError(error, description);
    DCHECK(!IsSessionAvailable(*it));
  }
}

}  // namespace net
