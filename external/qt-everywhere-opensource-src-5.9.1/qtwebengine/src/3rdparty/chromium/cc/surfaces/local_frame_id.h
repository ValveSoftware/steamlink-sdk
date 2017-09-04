// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_LOCAL_FRAME_ID_H_
#define CC_SURFACES_LOCAL_FRAME_ID_H_

#include <inttypes.h>
#include <tuple>

#include "base/hash.h"
#include "base/strings/stringprintf.h"
#include "base/unguessable_token.h"

namespace cc {

class LocalFrameId {
 public:
  constexpr LocalFrameId() : local_id_(0) {}

  constexpr LocalFrameId(const LocalFrameId& other)
      : local_id_(other.local_id_), nonce_(other.nonce_) {}

  constexpr LocalFrameId(uint32_t local_id, const base::UnguessableToken& nonce)
      : local_id_(local_id), nonce_(nonce) {}

  constexpr bool is_valid() const {
    return local_id_ != 0 && !nonce_.is_empty();
  }

  constexpr uint32_t local_id() const { return local_id_; }

  constexpr const base::UnguessableToken& nonce() const { return nonce_; }

  bool operator==(const LocalFrameId& other) const {
    return local_id_ == other.local_id_ && nonce_ == other.nonce_;
  }

  bool operator!=(const LocalFrameId& other) const { return !(*this == other); }

  bool operator<(const LocalFrameId& other) const {
    return std::tie(local_id_, nonce_) <
           std::tie(other.local_id_, other.nonce_);
  }

  size_t hash() const {
    return base::HashInts(
        local_id_, static_cast<uint64_t>(base::UnguessableTokenHash()(nonce_)));
  }

  std::string ToString() const {
    return base::StringPrintf("LocalFrameId(%d, %s" PRIu64 ")", local_id_,
                              nonce_.ToString().c_str());
  }

 private:
  uint32_t local_id_;
  base::UnguessableToken nonce_;
};

struct LocalFrameIdHash {
  size_t operator()(const LocalFrameId& key) const { return key.hash(); }
};

}  // namespace cc

#endif  // CC_SURFACES_LOCAL_FRAME_ID_H_
