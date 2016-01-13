// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_CONNECTION_FACTORY_IMPL_H_
#define GOOGLE_APIS_GCM_ENGINE_CONNECTION_FACTORY_IMPL_H_

#include "google_apis/gcm/engine/connection_factory.h"

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/base/backoff_entry.h"
#include "net/base/network_change_notifier.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_handle.h"
#include "url/gurl.h"

namespace net {
class HttpNetworkSession;
class NetLog;
}

namespace gcm {

class ConnectionHandlerImpl;
class GCMStatsRecorder;

class GCM_EXPORT ConnectionFactoryImpl :
    public ConnectionFactory,
    public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  ConnectionFactoryImpl(
      const std::vector<GURL>& mcs_endpoints,
      const net::BackoffEntry::Policy& backoff_policy,
      scoped_refptr<net::HttpNetworkSession> network_session,
      net::NetLog* net_log,
      GCMStatsRecorder* recorder);
  virtual ~ConnectionFactoryImpl();

  // ConnectionFactory implementation.
  virtual void Initialize(
      const BuildLoginRequestCallback& request_builder,
      const ConnectionHandler::ProtoReceivedCallback& read_callback,
      const ConnectionHandler::ProtoSentCallback& write_callback) OVERRIDE;
  virtual ConnectionHandler* GetConnectionHandler() const OVERRIDE;
  virtual void Connect() OVERRIDE;
  virtual bool IsEndpointReachable() const OVERRIDE;
  virtual std::string GetConnectionStateString() const OVERRIDE;
  virtual base::TimeTicks NextRetryAttempt() const OVERRIDE;
  virtual void SignalConnectionReset(ConnectionResetReason reason) OVERRIDE;
  virtual void SetConnectionListener(ConnectionListener* listener) OVERRIDE;

  // NetworkChangeObserver implementation.
  virtual void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  // Returns the server to which the factory is currently connected, or if
  // a connection is currently pending, the server to which the next connection
  // attempt will be made.
  GURL GetCurrentEndpoint() const;

  // Returns the IPEndpoint to which the factory is currently connected. If no
  // connection is active, returns an empty IPEndpoint.
  net::IPEndPoint GetPeerIP();

 protected:
  // Implementation of Connect(..). If not in backoff, uses |login_request_|
  // in attempting a connection/handshake. On connection/handshake failure, goes
  // into backoff.
  // Virtual for testing.
  virtual void ConnectImpl();

  // Helper method for initalizing the connection hander.
  // Virtual for testing.
  virtual void InitHandler();

  // Helper method for creating a backoff entry.
  // Virtual for testing.
  virtual scoped_ptr<net::BackoffEntry> CreateBackoffEntry(
      const net::BackoffEntry::Policy* const policy);

  // Helper method for creating the connection handler.
  // Virtual for testing.
  virtual scoped_ptr<ConnectionHandler> CreateConnectionHandler(
      base::TimeDelta read_timeout,
      const ConnectionHandler::ProtoReceivedCallback& read_callback,
      const ConnectionHandler::ProtoSentCallback& write_callback,
      const ConnectionHandler::ConnectionChangedCallback& connection_callback);

  // Returns the current time in Ticks.
  // Virtual for testing.
  virtual base::TimeTicks NowTicks();

  // Callback for Socket connection completion.
  void OnConnectDone(int result);

  // ConnectionHandler callback for connection issues.
  void ConnectionHandlerCallback(int result);

 private:
  // Helper method for checking backoff and triggering a connection as
  // necessary.
  void ConnectWithBackoff();

  // Proxy resolution and connection functions.
  void OnProxyResolveDone(int status);
  void OnProxyConnectDone(int status);
  int ReconsiderProxyAfterError(int error);
  void ReportSuccessfulProxyConnection();

  void CloseSocket();

  // The MCS endpoints to make connections to, sorted in order of priority.
  const std::vector<GURL> mcs_endpoints_;
  // Index to the endpoint for which a connection should be attempted next.
  size_t next_endpoint_;
  // Index to the endpoint that was last successfully connected.
  size_t last_successful_endpoint_;

  // The backoff policy to use.
  const net::BackoffEntry::Policy backoff_policy_;

  // ---- net:: components for establishing connections. ----
  // Network session for creating new connections.
  const scoped_refptr<net::HttpNetworkSession> network_session_;
  // Net log to use in connection attempts.
  net::BoundNetLog bound_net_log_;
  // The current PAC request, if one exists. Owned by the proxy service.
  net::ProxyService::PacRequest* pac_request_;
  // The current proxy info.
  net::ProxyInfo proxy_info_;
  // The handle to the socket for the current connection, if one exists.
  net::ClientSocketHandle socket_handle_;
  // Current backoff entry.
  scoped_ptr<net::BackoffEntry> backoff_entry_;
  // Backoff entry from previous connection attempt. Updated on each login
  // completion.
  scoped_ptr<net::BackoffEntry> previous_backoff_;

  // Whether a connection attempt is currently actively in progress.
  bool connecting_;

  // Whether the client is waiting for backoff to finish before attempting to
  // connect. Canary jobs are able to preempt connections pending backoff
  // expiration.
  bool waiting_for_backoff_;

  // Whether the NetworkChangeNotifier has informed the client that there is
  // no current connection. No connection attempts will be made until the
  // client is informed of a valid connection type.
  bool waiting_for_network_online_;

  // Whether login successfully completed after the connection was established.
  // If a connection reset happens while attempting to log in, the current
  // backoff entry is reused (after incrementing with a new failure).
  bool logging_in_;

  // The time of the last login completion. Used for calculating whether to
  // restore a previous backoff entry and for measuring uptime.
  base::TimeTicks last_login_time_;

  // The current connection handler, if one exists.
  scoped_ptr<ConnectionHandler> connection_handler_;

  // Builder for generating new login requests.
  BuildLoginRequestCallback request_builder_;

  // Recorder that records GCM activities for debugging purpose. Not owned.
  GCMStatsRecorder* recorder_;

  // Listener for connection change events.
  ConnectionListener* listener_;

  base::WeakPtrFactory<ConnectionFactoryImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionFactoryImpl);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_CONNECTION_FACTORY_IMPL_H_
