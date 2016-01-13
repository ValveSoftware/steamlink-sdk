// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Google-specific helper for QuicConnection which uses
// EpollAlarm for alarms, and used an int fd_ for writing data.

#ifndef NET_TOOLS_QUIC_QUIC_EPOLL_CONNECTION_HELPER_H_
#define NET_TOOLS_QUIC_QUIC_EPOLL_CONNECTION_HELPER_H_

#include <sys/types.h>
#include <set>

#include "net/quic/quic_connection.h"
#include "net/quic/quic_packet_writer.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"
#include "net/tools/quic/quic_default_packet_writer.h"
#include "net/tools/quic/quic_epoll_clock.h"

namespace net {

class EpollServer;
class QuicRandom;

namespace tools {

class AckAlarm;
class RetransmissionAlarm;
class SendAlarm;
class TimeoutAlarm;

class QuicEpollConnectionHelper : public QuicConnectionHelperInterface {
 public:
  explicit QuicEpollConnectionHelper(EpollServer* eps);
  virtual ~QuicEpollConnectionHelper();

  // QuicEpollConnectionHelperInterface
  virtual const QuicClock* GetClock() const OVERRIDE;
  virtual QuicRandom* GetRandomGenerator() OVERRIDE;
  virtual QuicAlarm* CreateAlarm(QuicAlarm::Delegate* delegate) OVERRIDE;

  EpollServer* epoll_server() { return epoll_server_; }

 private:
  friend class QuicConnectionPeer;

  EpollServer* epoll_server_;  // Not owned.

  const QuicEpollClock clock_;
  QuicRandom* random_generator_;

  DISALLOW_COPY_AND_ASSIGN(QuicEpollConnectionHelper);
};

}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_EPOLL_CONNECTION_HELPER_H_
