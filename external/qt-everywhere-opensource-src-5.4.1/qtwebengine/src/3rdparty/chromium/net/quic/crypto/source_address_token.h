// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_SOURCE_ADDRESS_TOKEN_H_
#define NET_QUIC_CRYPTO_SOURCE_ADDRESS_TOKEN_H_

#include <string>

#include "base/basictypes.h"
#include "base/strings/string_piece.h"

namespace net {

// TODO(rtenneti): sync with server more rationally.
class SourceAddressToken {
 public:
  SourceAddressToken();
  ~SourceAddressToken();

  std::string SerializeAsString() const;

  bool ParseFromArray(const char* plaintext, size_t plaintext_length);

  std::string ip() const {
    return ip_;
  }

  int64 timestamp() const {
    return timestamp_;
  }

  void set_ip(base::StringPiece ip) {
    ip_ = ip.as_string();
  }

  void set_timestamp(int64 timestamp) {
    timestamp_ = timestamp;
  }

 private:
  std::string ip_;
  int64 timestamp_;

  DISALLOW_COPY_AND_ASSIGN(SourceAddressToken);
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_SOURCE_ADDRESS_TOKEN_H_
