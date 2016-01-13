// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Chrome-specific helper for QuicConnection which uses
// a TaskRunner for alarms, and uses a DatagramClientSocket for writing data.

#ifndef NET_QUIC_QUIC_CONNECTION_HELPER_H_
#define NET_QUIC_QUIC_CONNECTION_HELPER_H_

#include "net/quic/quic_connection.h"

#include <set>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"
#include "net/udp/datagram_client_socket.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace net {

class QuicClock;
class QuicRandom;

class NET_EXPORT_PRIVATE QuicConnectionHelper
    : public QuicConnectionHelperInterface {
 public:
  QuicConnectionHelper(base::TaskRunner* task_runner,
                       const QuicClock* clock,
                       QuicRandom* random_generator);
  virtual ~QuicConnectionHelper();

  // QuicConnectionHelperInterface
  virtual const QuicClock* GetClock() const OVERRIDE;
  virtual QuicRandom* GetRandomGenerator() OVERRIDE;
  virtual QuicAlarm* CreateAlarm(QuicAlarm::Delegate* delegate) OVERRIDE;

 private:
  base::WeakPtrFactory<QuicConnectionHelper> weak_factory_;
  base::TaskRunner* task_runner_;
  const QuicClock* clock_;
  QuicRandom* random_generator_;

  DISALLOW_COPY_AND_ASSIGN(QuicConnectionHelper);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CONNECTION_HELPER_H_
