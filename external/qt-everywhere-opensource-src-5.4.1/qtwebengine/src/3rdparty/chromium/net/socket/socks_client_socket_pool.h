// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKS_CLIENT_SOCKET_POOL_H_
#define NET_SOCKET_SOCKS_CLIENT_SOCKET_POOL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "net/base/host_port_pair.h"
#include "net/dns/host_resolver.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/client_socket_pool_histograms.h"

namespace net {

class ConnectJobFactory;
class TransportClientSocketPool;
class TransportSocketParams;

class NET_EXPORT_PRIVATE SOCKSSocketParams
    : public base::RefCounted<SOCKSSocketParams> {
 public:
  SOCKSSocketParams(const scoped_refptr<TransportSocketParams>& proxy_server,
                    bool socks_v5, const HostPortPair& host_port_pair);

  const scoped_refptr<TransportSocketParams>& transport_params() const {
    return transport_params_;
  }
  const HostResolver::RequestInfo& destination() const { return destination_; }
  bool is_socks_v5() const { return socks_v5_; }
  bool ignore_limits() const { return ignore_limits_; }

 private:
  friend class base::RefCounted<SOCKSSocketParams>;
  ~SOCKSSocketParams();

  // The transport (likely TCP) connection must point toward the proxy server.
  const scoped_refptr<TransportSocketParams> transport_params_;
  // This is the HTTP destination.
  HostResolver::RequestInfo destination_;
  const bool socks_v5_;
  bool ignore_limits_;

  DISALLOW_COPY_AND_ASSIGN(SOCKSSocketParams);
};

// SOCKSConnectJob handles the handshake to a socks server after setting up
// an underlying transport socket.
class SOCKSConnectJob : public ConnectJob {
 public:
  SOCKSConnectJob(const std::string& group_name,
                  RequestPriority priority,
                  const scoped_refptr<SOCKSSocketParams>& params,
                  const base::TimeDelta& timeout_duration,
                  TransportClientSocketPool* transport_pool,
                  HostResolver* host_resolver,
                  Delegate* delegate,
                  NetLog* net_log);
  virtual ~SOCKSConnectJob();

  // ConnectJob methods.
  virtual LoadState GetLoadState() const OVERRIDE;

 private:
  enum State {
    STATE_TRANSPORT_CONNECT,
    STATE_TRANSPORT_CONNECT_COMPLETE,
    STATE_SOCKS_CONNECT,
    STATE_SOCKS_CONNECT_COMPLETE,
    STATE_NONE,
  };

  void OnIOComplete(int result);

  // Runs the state transition loop.
  int DoLoop(int result);

  int DoTransportConnect();
  int DoTransportConnectComplete(int result);
  int DoSOCKSConnect();
  int DoSOCKSConnectComplete(int result);

  // Begins the transport connection and the SOCKS handshake.  Returns OK on
  // success and ERR_IO_PENDING if it cannot immediately service the request.
  // Otherwise, it returns a net error code.
  virtual int ConnectInternal() OVERRIDE;

  scoped_refptr<SOCKSSocketParams> socks_params_;
  TransportClientSocketPool* const transport_pool_;
  HostResolver* const resolver_;

  State next_state_;
  CompletionCallback callback_;
  scoped_ptr<ClientSocketHandle> transport_socket_handle_;
  scoped_ptr<StreamSocket> socket_;

  DISALLOW_COPY_AND_ASSIGN(SOCKSConnectJob);
};

class NET_EXPORT_PRIVATE SOCKSClientSocketPool
    : public ClientSocketPool, public HigherLayeredPool {
 public:
  typedef SOCKSSocketParams SocketParams;

  SOCKSClientSocketPool(
      int max_sockets,
      int max_sockets_per_group,
      ClientSocketPoolHistograms* histograms,
      HostResolver* host_resolver,
      TransportClientSocketPool* transport_pool,
      NetLog* net_log);

  virtual ~SOCKSClientSocketPool();

  // ClientSocketPool implementation.
  virtual int RequestSocket(const std::string& group_name,
                            const void* connect_params,
                            RequestPriority priority,
                            ClientSocketHandle* handle,
                            const CompletionCallback& callback,
                            const BoundNetLog& net_log) OVERRIDE;

  virtual void RequestSockets(const std::string& group_name,
                              const void* params,
                              int num_sockets,
                              const BoundNetLog& net_log) OVERRIDE;

  virtual void CancelRequest(const std::string& group_name,
                             ClientSocketHandle* handle) OVERRIDE;

  virtual void ReleaseSocket(const std::string& group_name,
                             scoped_ptr<StreamSocket> socket,
                             int id) OVERRIDE;

  virtual void FlushWithError(int error) OVERRIDE;

  virtual void CloseIdleSockets() OVERRIDE;

  virtual int IdleSocketCount() const OVERRIDE;

  virtual int IdleSocketCountInGroup(
      const std::string& group_name) const OVERRIDE;

  virtual LoadState GetLoadState(
      const std::string& group_name,
      const ClientSocketHandle* handle) const OVERRIDE;

  virtual base::DictionaryValue* GetInfoAsValue(
      const std::string& name,
      const std::string& type,
      bool include_nested_pools) const OVERRIDE;

  virtual base::TimeDelta ConnectionTimeout() const OVERRIDE;

  virtual ClientSocketPoolHistograms* histograms() const OVERRIDE;

  // LowerLayeredPool implementation.
  virtual bool IsStalled() const OVERRIDE;

  virtual void AddHigherLayeredPool(HigherLayeredPool* higher_pool) OVERRIDE;

  virtual void RemoveHigherLayeredPool(HigherLayeredPool* higher_pool) OVERRIDE;

  // HigherLayeredPool implementation.
  virtual bool CloseOneIdleConnection() OVERRIDE;

 private:
  typedef ClientSocketPoolBase<SOCKSSocketParams> PoolBase;

  class SOCKSConnectJobFactory : public PoolBase::ConnectJobFactory {
   public:
    SOCKSConnectJobFactory(TransportClientSocketPool* transport_pool,
                           HostResolver* host_resolver,
                           NetLog* net_log)
        : transport_pool_(transport_pool),
          host_resolver_(host_resolver),
          net_log_(net_log) {}

    virtual ~SOCKSConnectJobFactory() {}

    // ClientSocketPoolBase::ConnectJobFactory methods.
    virtual scoped_ptr<ConnectJob> NewConnectJob(
        const std::string& group_name,
        const PoolBase::Request& request,
        ConnectJob::Delegate* delegate) const OVERRIDE;

    virtual base::TimeDelta ConnectionTimeout() const OVERRIDE;

   private:
    TransportClientSocketPool* const transport_pool_;
    HostResolver* const host_resolver_;
    NetLog* net_log_;

    DISALLOW_COPY_AND_ASSIGN(SOCKSConnectJobFactory);
  };

  TransportClientSocketPool* const transport_pool_;
  PoolBase base_;

  DISALLOW_COPY_AND_ASSIGN(SOCKSClientSocketPool);
};

}  // namespace net

#endif  // NET_SOCKET_SOCKS_CLIENT_SOCKET_POOL_H_
