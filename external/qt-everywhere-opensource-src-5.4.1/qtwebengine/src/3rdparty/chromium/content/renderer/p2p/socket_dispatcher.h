// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// P2PSocketDispatcher is a per-renderer object that dispatchers all
// P2P messages received from the browser and relays all P2P messages
// sent to the browser. P2PSocketClient instances register themselves
// with the dispatcher using RegisterClient() and UnregisterClient().
//
// Relationship of classes.
//
//       P2PSocketHost                     P2PSocketClient
//            ^                                   ^
//            |                                   |
//            v                  IPC              v
//  P2PSocketDispatcherHost  <--------->  P2PSocketDispatcher
//
// P2PSocketDispatcher receives and dispatches messages on the
// IO thread.

#ifndef CONTENT_RENDERER_P2P_SOCKET_DISPATCHER_H_
#define CONTENT_RENDERER_P2P_SOCKET_DISPATCHER_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/common/p2p_socket_type.h"
#include "ipc/message_filter.h"
#include "net/base/net_util.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace net {
class IPEndPoint;
}  // namespace net

namespace content {

class NetworkListObserver;
class P2PAsyncAddressResolver;
class P2PSocketClientImpl;
class RenderViewImpl;

class CONTENT_EXPORT P2PSocketDispatcher : public IPC::MessageFilter {
 public:
  explicit P2PSocketDispatcher(base::MessageLoopProxy* ipc_message_loop);

  // Add a new network list observer. Each observer is called
  // immidiately after it is registered and then later whenever
  // network configuration changes. Can be called on any thread. The
  // observer is always called on the thread it was added.
  void AddNetworkListObserver(NetworkListObserver* network_list_observer);

  // Removes network list observer. Must be called on the thread on
  // which the observer was added.
  void RemoveNetworkListObserver(NetworkListObserver* network_list_observer);

 protected:
  virtual ~P2PSocketDispatcher();

 private:
  friend class P2PAsyncAddressResolver;
  friend class P2PSocketClientImpl;

  // Send a message asynchronously.
  virtual void Send(IPC::Message* message);

  // IPC::MessageFilter override. Called on IO thread.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnFilterAdded(IPC::Sender* sender) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

  // Returns the IO message loop.
  base::MessageLoopProxy* message_loop();

  // Called by P2PSocketClient.
  int RegisterClient(P2PSocketClientImpl* client);
  void UnregisterClient(int id);
  void SendP2PMessage(IPC::Message* msg);

  // Called by DnsRequest.
  int RegisterHostAddressRequest(P2PAsyncAddressResolver* request);
  void UnregisterHostAddressRequest(int id);

  // Incoming message handlers.
  void OnNetworkListChanged(const net::NetworkInterfaceList& networks);
  void OnGetHostAddressResult(int32 request_id,
                              const net::IPAddressList& addresses);
  void OnSocketCreated(int socket_id, const net::IPEndPoint& address);
  void OnIncomingTcpConnection(int socket_id, const net::IPEndPoint& address);
  void OnSendComplete(int socket_id);
  void OnError(int socket_id);
  void OnDataReceived(int socket_id, const net::IPEndPoint& address,
                      const std::vector<char>& data,
                      const base::TimeTicks& timestamp);

  P2PSocketClientImpl* GetClient(int socket_id);

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  IDMap<P2PSocketClientImpl> clients_;

  IDMap<P2PAsyncAddressResolver> host_address_requests_;

  bool network_notifications_started_;
  scoped_refptr<ObserverListThreadSafe<NetworkListObserver> >
      network_list_observers_;

  IPC::Sender* sender_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_P2P_SOCKET_DISPATCHER_H_
