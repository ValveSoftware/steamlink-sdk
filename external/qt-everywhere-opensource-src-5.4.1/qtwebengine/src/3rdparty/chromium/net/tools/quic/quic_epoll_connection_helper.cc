// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_epoll_connection_helper.h"

#include <errno.h>
#include <sys/socket.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/crypto/quic_random.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/quic_socket_utils.h"

namespace net {
namespace tools {

namespace {

class QuicEpollAlarm : public QuicAlarm {
 public:
  QuicEpollAlarm(EpollServer* epoll_server,
                 QuicAlarm::Delegate* delegate)
      : QuicAlarm(delegate),
        epoll_server_(epoll_server),
        epoll_alarm_impl_(this) {}

 protected:
  virtual void SetImpl() OVERRIDE {
    DCHECK(deadline().IsInitialized());
    epoll_server_->RegisterAlarm(
        deadline().Subtract(QuicTime::Zero()).ToMicroseconds(),
        &epoll_alarm_impl_);
  }

  virtual void CancelImpl() OVERRIDE {
    DCHECK(!deadline().IsInitialized());
    epoll_alarm_impl_.UnregisterIfRegistered();
  }

 private:
  class EpollAlarmImpl : public EpollAlarm {
   public:
    explicit EpollAlarmImpl(QuicEpollAlarm* alarm) : alarm_(alarm) {}

    virtual int64 OnAlarm() OVERRIDE {
      EpollAlarm::OnAlarm();
      alarm_->Fire();
      // Fire will take care of registering the alarm, if needed.
      return 0;
    }

   private:
    QuicEpollAlarm* alarm_;
  };

  EpollServer* epoll_server_;
  EpollAlarmImpl epoll_alarm_impl_;
};

}  // namespace

QuicEpollConnectionHelper::QuicEpollConnectionHelper(EpollServer* epoll_server)
    : epoll_server_(epoll_server),
      clock_(epoll_server),
      random_generator_(QuicRandom::GetInstance()) {
}

QuicEpollConnectionHelper::~QuicEpollConnectionHelper() {
}

const QuicClock* QuicEpollConnectionHelper::GetClock() const {
  return &clock_;
}

QuicRandom* QuicEpollConnectionHelper::GetRandomGenerator() {
  return random_generator_;
}

QuicAlarm* QuicEpollConnectionHelper::CreateAlarm(
    QuicAlarm::Delegate* delegate) {
  return new QuicEpollAlarm(epoll_server_, delegate);
}

}  // namespace tools
}  // namespace net
