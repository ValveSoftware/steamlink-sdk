// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_THROTTLE_H_
#define NET_WEBSOCKETS_WEBSOCKET_THROTTLE_H_

#include <deque>
#include <map>
#include <set>
#include <string>

#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"

template <typename T> struct DefaultSingletonTraits;

namespace net {

class SocketStream;
class WebSocketJob;

// SocketStreamThrottle for WebSocket protocol.
// Implements the client-side requirements in the spec.
// http://tools.ietf.org/html/draft-hixie-thewebsocketprotocol
// 4.1 Handshake
//   1.   If the user agent already has a Web Socket connection to the
//        remote host (IP address) identified by /host/, even if known by
//        another name, wait until that connection has been established or
//        for that connection to have failed.
class NET_EXPORT_PRIVATE WebSocketThrottle {
 public:
  // Returns the singleton instance.
  static WebSocketThrottle* GetInstance();

  // Puts |job| in |queue_| and queues for the destination addresses
  // of |job|.
  // If other job is using the same destination address, set |job| waiting.
  //
  // Returns true if successful. If the number of pending jobs will exceed
  // the limit, does nothing and returns false.
  bool PutInQueue(WebSocketJob* job);

  // Removes |job| from |queue_| and queues for the destination addresses
  // of |job|, and then wakes up jobs that can now resume establishing a
  // connection.
  void RemoveFromQueue(WebSocketJob* job);

 private:
  typedef std::deque<WebSocketJob*> ConnectingQueue;
  typedef std::map<IPEndPoint, ConnectingQueue> ConnectingAddressMap;

  WebSocketThrottle();
  ~WebSocketThrottle();
  friend struct DefaultSingletonTraits<WebSocketThrottle>;

  // Examines if any of the given jobs can resume establishing a connection. If
  // for all per-address queues for each resolved addresses
  // (job->address_list()) of a job, the job is at the front of the queues, the
  // job can resume establishing a connection, so wakes up the job.
  void WakeupSocketIfNecessary(
      const std::set<WebSocketJob*>& wakeup_candidates);

  // Key: string of host's address.  Value: queue of sockets for the address.
  ConnectingAddressMap addr_map_;

  // Queue of sockets for websockets in opening state.
  ConnectingQueue queue_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketThrottle);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_THROTTLE_H_
