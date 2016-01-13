// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_DELAYED_VERIFY_STRIKE_REGISTER_CLIENT_H_
#define NET_QUIC_TEST_TOOLS_DELAYED_VERIFY_STRIKE_REGISTER_CLIENT_H_

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "net/quic/crypto/local_strike_register_client.h"

namespace net {
namespace test {

// Test helper that allows delaying execution of nonce verification
// callbacks until a later time.
class DelayedVerifyStrikeRegisterClient : public LocalStrikeRegisterClient {
 public:
  DelayedVerifyStrikeRegisterClient(unsigned max_entries,
                                    uint32 current_time_external,
                                    uint32 window_secs,
                                    const uint8 orbit[8],
                                    StrikeRegister::StartupType startup);
  virtual ~DelayedVerifyStrikeRegisterClient();

  virtual void VerifyNonceIsValidAndUnique(base::StringPiece nonce,
                                           QuicWallTime now,
                                           ResultCallback* cb) OVERRIDE;

  // Start queueing verifications instead of executing them immediately.
  void StartDelayingVerification() {
    delay_verifications_ = true;
  }
  // Number of verifications that are queued.
  int PendingVerifications() const;
  // Run all pending verifications.
  void RunPendingVerifications();

 private:
  struct VerifyArgs {
    VerifyArgs(base::StringPiece in_nonce,
               QuicWallTime in_now,
               ResultCallback* in_cb)
        : nonce(in_nonce.as_string()),
          now(in_now),
          cb(in_cb) {
    }

    std::string nonce;
    QuicWallTime now;
    ResultCallback* cb;
  };

  bool delay_verifications_;
  std::vector<VerifyArgs> pending_verifications_;

  DISALLOW_COPY_AND_ASSIGN(DelayedVerifyStrikeRegisterClient);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_DELAYED_VERIFY_STRIKE_REGISTER_CLIENT_H_
