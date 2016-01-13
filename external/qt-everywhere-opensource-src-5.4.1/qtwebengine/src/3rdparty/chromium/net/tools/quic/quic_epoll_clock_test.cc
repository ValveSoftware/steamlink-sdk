// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_epoll_clock.h"

#include "net/tools/quic/test_tools/mock_epoll_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {
namespace tools {
namespace test {

TEST(QuicEpollClockTest, ApproximateNowInUsec) {
  MockEpollServer epoll_server;
  QuicEpollClock clock(&epoll_server);

  epoll_server.set_now_in_usec(1000000);
  EXPECT_EQ(1000000,
            clock.ApproximateNow().Subtract(QuicTime::Zero()).ToMicroseconds());

  epoll_server.AdvanceBy(5);
  EXPECT_EQ(1000005,
            clock.ApproximateNow().Subtract(QuicTime::Zero()).ToMicroseconds());
}

TEST(QuicEpollClockTest, NowInUsec) {
  MockEpollServer epoll_server;
  QuicEpollClock clock(&epoll_server);

  epoll_server.set_now_in_usec(1000000);
  EXPECT_EQ(1000000,
            clock.Now().Subtract(QuicTime::Zero()).ToMicroseconds());

  epoll_server.AdvanceBy(5);
  EXPECT_EQ(1000005,
            clock.Now().Subtract(QuicTime::Zero()).ToMicroseconds());
}

TEST(QuicEpollClockTest, WallNow) {
  MockEpollServer epoll_server;
  QuicEpollClock clock(&epoll_server);

  base::Time start = base::Time::Now();
  QuicWallTime now = clock.WallNow();
  base::Time end = base::Time::Now();

  // If end > start, then we can check now is between start and end.
  if (end > start) {
    EXPECT_LE(static_cast<uint64>(start.ToTimeT()), now.ToUNIXSeconds());
    EXPECT_LE(now.ToUNIXSeconds(), static_cast<uint64>(end.ToTimeT()));
  }
}

}  // namespace test
}  // namespace tools
}  // namespace net
