// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_MOCK_CLOCK_H_
#define NET_QUIC_TEST_TOOLS_MOCK_CLOCK_H_

#include "net/quic/quic_clock.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/time/time.h"

namespace net {

class MockClock : public QuicClock {
 public:
  MockClock();
  virtual ~MockClock();

  void AdvanceTime(QuicTime::Delta delta);

  virtual QuicTime Now() const OVERRIDE;

  virtual QuicTime ApproximateNow() const OVERRIDE;

  virtual QuicWallTime WallNow() const OVERRIDE;

  base::TimeTicks NowInTicks() const;

 private:
  QuicTime now_;

  DISALLOW_COPY_AND_ASSIGN(MockClock);
};

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_MOCK_CLOCK_H_
