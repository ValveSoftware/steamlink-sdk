// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_MOCK_RANDOM_H_
#define NET_QUIC_TEST_TOOLS_MOCK_RANDOM_H_

#include "base/compiler_specific.h"
#include "net/quic/crypto/quic_random.h"

namespace net {

class MockRandom : public QuicRandom {
 public:
  // Initializes base_ to 0xDEADBEEF.
  MockRandom();
  explicit MockRandom(uint32 base);

  // QuicRandom:
  // Fills the |data| buffer with a repeating byte, initially 'r'.
  virtual void RandBytes(void* data, size_t len) OVERRIDE;
  // Returns base + the current increment.
  virtual uint64 RandUint64() OVERRIDE;
  // Does nothing.
  virtual void Reseed(const void* additional_entropy,
                      size_t entropy_len) OVERRIDE;

  // ChangeValue increments |increment_|. This causes the value returned by
  // |RandUint64| and the byte that |RandBytes| fills with, to change.
  void ChangeValue();

 private:
  uint32 base_;
  uint8 increment_;

  DISALLOW_COPY_AND_ASSIGN(MockRandom);
};

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_MOCK_RANDOM_H_
