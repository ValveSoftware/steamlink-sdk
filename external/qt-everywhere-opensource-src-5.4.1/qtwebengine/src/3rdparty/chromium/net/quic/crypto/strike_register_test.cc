// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/strike_register.h"

#include <set>
#include <string>

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using net::StrikeRegister;
using std::set;
using std::string;

const uint8 kOrbit[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

// StrikeRegisterTests don't look at the random bytes so this function can
// simply set the random bytes to 0.
void SetNonce(uint8 nonce[32], unsigned time, const uint8 orbit[8]) {
  nonce[0] = time >> 24;
  nonce[1] = time >> 16;
  nonce[2] = time >> 8;
  nonce[3] = time;
  memcpy(nonce + 4, orbit, 8);
  memset(nonce + 12, 0, 20);
}

TEST(StrikeRegisterTest, SimpleHorizon) {
  // The set must reject values created on or before its own creation time.
  StrikeRegister set(10 /* max size */, 1000 /* current time */,
                     100 /* window secs */, kOrbit,
                     StrikeRegister::DENY_REQUESTS_AT_STARTUP);
  uint8 nonce[32];
  SetNonce(nonce, 999, kOrbit);
  ASSERT_FALSE(set.Insert(nonce, 1000));
  SetNonce(nonce, 1000, kOrbit);
  ASSERT_FALSE(set.Insert(nonce, 1000));
}

TEST(StrikeRegisterTest, NoStartupMode) {
  // Check that a strike register works immediately if NO_STARTUP_PERIOD_NEEDED
  // is specified.
  StrikeRegister set(10 /* max size */, 0 /* current time */,
                     100 /* window secs */, kOrbit,
                     StrikeRegister::NO_STARTUP_PERIOD_NEEDED);
  uint8 nonce[32];
  SetNonce(nonce, 0, kOrbit);
  ASSERT_TRUE(set.Insert(nonce, 0));
  ASSERT_FALSE(set.Insert(nonce, 0));
}

TEST(StrikeRegisterTest, WindowFuture) {
  // The set must reject values outside the window.
  StrikeRegister set(10 /* max size */, 1000 /* current time */,
                     100 /* window secs */, kOrbit,
                     StrikeRegister::DENY_REQUESTS_AT_STARTUP);
  uint8 nonce[32];
  SetNonce(nonce, 1101, kOrbit);
  ASSERT_FALSE(set.Insert(nonce, 1000));
  SetNonce(nonce, 999, kOrbit);
  ASSERT_FALSE(set.Insert(nonce, 1100));
}

TEST(StrikeRegisterTest, BadOrbit) {
  // The set must reject values with the wrong orbit
  StrikeRegister set(10 /* max size */, 1000 /* current time */,
                     100 /* window secs */, kOrbit,
                     StrikeRegister::DENY_REQUESTS_AT_STARTUP);
  uint8 nonce[32];
  static const uint8 kBadOrbit[8] = { 0, 0, 0, 0, 1, 1, 1, 1 };
  SetNonce(nonce, 1101, kBadOrbit);
  ASSERT_FALSE(set.Insert(nonce, 1100));
}

TEST(StrikeRegisterTest, OneValue) {
  StrikeRegister set(10 /* max size */, 1000 /* current time */,
                     100 /* window secs */, kOrbit,
                     StrikeRegister::DENY_REQUESTS_AT_STARTUP);
  uint8 nonce[32];
  SetNonce(nonce, 1101, kOrbit);
  ASSERT_TRUE(set.Insert(nonce, 1100));
}

TEST(StrikeRegisterTest, RejectDuplicate) {
  // The set must reject values with the wrong orbit
  StrikeRegister set(10 /* max size */, 1000 /* current time */,
                     100 /* window secs */, kOrbit,
                     StrikeRegister::DENY_REQUESTS_AT_STARTUP);
  uint8 nonce[32];
  SetNonce(nonce, 1101, kOrbit);
  ASSERT_TRUE(set.Insert(nonce, 1100));
  ASSERT_FALSE(set.Insert(nonce, 1100));
}

TEST(StrikeRegisterTest, HorizonUpdating) {
  StrikeRegister set(5 /* max size */, 1000 /* current time */,
                     100 /* window secs */, kOrbit,
                     StrikeRegister::DENY_REQUESTS_AT_STARTUP);
  uint8 nonce[6][32];
  for (unsigned i = 0; i < 5; i++) {
    SetNonce(nonce[i], 1101, kOrbit);
    nonce[i][31] = i;
    ASSERT_TRUE(set.Insert(nonce[i], 1100));
  }

  // This should push the oldest value out and force the horizon to be updated.
  SetNonce(nonce[5], 1102, kOrbit);
  ASSERT_TRUE(set.Insert(nonce[5], 1100));

  // This should be behind the horizon now:
  SetNonce(nonce[5], 1101, kOrbit);
  nonce[5][31] = 10;
  ASSERT_FALSE(set.Insert(nonce[5], 1100));
}

TEST(StrikeRegisterTest, InsertMany) {
  StrikeRegister set(5000 /* max size */, 1000 /* current time */,
                     500 /* window secs */, kOrbit,
                     StrikeRegister::DENY_REQUESTS_AT_STARTUP);

  uint8 nonce[32];
  SetNonce(nonce, 1101, kOrbit);
  for (unsigned i = 0; i < 100000; i++) {
    SetNonce(nonce, 1101 + i/500, kOrbit);
    memcpy(nonce + 12, &i, sizeof(i));
    set.Insert(nonce, 1100);
  }
}


// For the following test we create a slow, but simple, version of a
// StrikeRegister. The behaviour of this object is much easier to understand
// than the fully fledged version. We then create a test to show, empirically,
// that the two objects have identical behaviour.

// A SlowStrikeRegister has the same public interface as a StrikeRegister, but
// is much slower. Hopefully it is also more obviously correct and we can
// empirically test that their behaviours are identical.
class SlowStrikeRegister {
 public:
  SlowStrikeRegister(unsigned max_entries, uint32 current_time,
                     uint32 window_secs, const uint8 orbit[8])
      : max_entries_(max_entries),
        window_secs_(window_secs),
        creation_time_(current_time),
        horizon_(ExternalTimeToInternal(current_time + window_secs)) {
    memcpy(orbit_, orbit, sizeof(orbit_));
  }

  bool Insert(const uint8 nonce_bytes[32], const uint32 current_time_external) {
    const uint32 current_time = ExternalTimeToInternal(current_time_external);

    // Check to see if the orbit is correct.
    if (memcmp(nonce_bytes + 4, orbit_, sizeof(orbit_))) {
      return false;
    }
    const uint32 nonce_time =
        ExternalTimeToInternal(TimeFromBytes(nonce_bytes));
    // We have dropped one or more nonces with a time value of |horizon_|, so
    // we have to reject anything with a timestamp less than or equal to that.
    if (nonce_time <= horizon_) {
      return false;
    }

    // Check that the timestamp is in the current window.
    if ((current_time > window_secs_ &&
         nonce_time < (current_time - window_secs_)) ||
        nonce_time > (current_time + window_secs_)) {
      return false;
    }

    string nonce;
    nonce.reserve(32);
    nonce +=
        string(reinterpret_cast<const char*>(&nonce_time), sizeof(nonce_time));
    nonce +=
        string(reinterpret_cast<const char*>(nonce_bytes) + sizeof(nonce_time),
               32 - sizeof(nonce_time));

    set<string>::const_iterator it = nonces_.find(nonce);
    if (it != nonces_.end()) {
      return false;
    }

    if (nonces_.size() == max_entries_) {
      DropOldestEntry();
    }

    nonces_.insert(nonce);
    return true;
  }

 private:
  // TimeFromBytes returns a big-endian uint32 from |d|.
  static uint32 TimeFromBytes(const uint8 d[4]) {
    return static_cast<uint32>(d[0]) << 24 |
           static_cast<uint32>(d[1]) << 16 |
           static_cast<uint32>(d[2]) << 8 |
           static_cast<uint32>(d[3]);
  }

  uint32 ExternalTimeToInternal(uint32 external_time) {
    static const uint32 kCreationTimeFromInternalEpoch = 63115200.0;
    uint32 internal_epoch = 0;
    if (creation_time_ > kCreationTimeFromInternalEpoch) {
      internal_epoch = creation_time_ - kCreationTimeFromInternalEpoch;
    }

    return external_time - internal_epoch;
  }

  void DropOldestEntry() {
    set<string>::iterator oldest = nonces_.begin(), it;
    uint32 oldest_time =
        TimeFromBytes(reinterpret_cast<const uint8*>(oldest->data()));

    for (it = oldest; it != nonces_.end(); it++) {
      uint32 t = TimeFromBytes(reinterpret_cast<const uint8*>(it->data()));
      if (t < oldest_time ||
          (t == oldest_time && memcmp(it->data(), oldest->data(), 32) < 0)) {
        oldest_time = t;
        oldest = it;
      }
    }

    nonces_.erase(oldest);
    horizon_ = oldest_time;
  }

  const unsigned max_entries_;
  const unsigned window_secs_;
  const uint32 creation_time_;
  uint8 orbit_[8];
  uint32 horizon_;

  set<string> nonces_;
};

TEST(StrikeRegisterStressTest, Stress) {
  // Fixed seed gives reproducibility for this test.
  srand(42);
  unsigned max_entries = 64;
  uint32 current_time = 10000, window = 200;
  scoped_ptr<StrikeRegister> s1(
      new StrikeRegister(max_entries, current_time, window, kOrbit,
                         StrikeRegister::DENY_REQUESTS_AT_STARTUP));
  scoped_ptr<SlowStrikeRegister> s2(
      new SlowStrikeRegister(max_entries, current_time, window, kOrbit));
  uint64 i;

  // When making changes it's worth removing the limit on this test and running
  // it for a while. For the initial development an opt binary was left running
  // for 10 minutes.
  const uint64 kMaxIterations = 10000;
  for (i = 0; i < kMaxIterations; i++) {
    if (rand() % 1000 == 0) {
      // 0.1% chance of resetting the sets.
      max_entries = rand() % 300 + 2;
      current_time = rand() % 10000;
      window = rand() % 500;
      s1.reset(new StrikeRegister(max_entries, current_time, window, kOrbit,
                                  StrikeRegister::DENY_REQUESTS_AT_STARTUP));
      s2.reset(
          new SlowStrikeRegister(max_entries, current_time, window, kOrbit));
    }

    int32 time_delta = rand() % (window * 4);
    time_delta -= window * 2;
    const uint32 time = current_time + time_delta;
    if (time_delta < 0 && time > current_time) {
      continue;  // overflow
    }

    uint8 nonce[32];
    SetNonce(nonce, time, kOrbit);

    // There are 2048 possible nonce values:
    const uint32 v = rand() % 2048;
    nonce[30] = v >> 8;
    nonce[31] = v;

    const bool r2 = s2->Insert(nonce, time);
    const bool r1 = s1->Insert(nonce, time);

    if (r1 != r2) {
      break;
    }

    if (i % 10 == 0) {
      s1->Validate();
    }
  }

  if (i != kMaxIterations) {
    FAIL() << "Failed after " << i << " iterations";
  }
}

}  // anonymous namespace
