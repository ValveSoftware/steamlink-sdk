// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/source_address_token.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"

using std::string;
using std::vector;

namespace net {

SourceAddressToken::SourceAddressToken() {
}

SourceAddressToken::~SourceAddressToken() {
}

string SourceAddressToken::SerializeAsString() const {
  string out;
  out.push_back(ip_.size());
  out.append(ip_);
  string time_str = base::Int64ToString(timestamp_);
  out.push_back(time_str.size());
  out.append(time_str);
  return out;
}

bool SourceAddressToken::ParseFromArray(const char* plaintext,
                                        size_t plaintext_length) {
  if (plaintext_length == 0) {
    return false;
  }
  size_t ip_len = plaintext[0];
  if (plaintext_length <= 1 + ip_len) {
    return false;
  }
  size_t time_len = plaintext[1 + ip_len];
  if (plaintext_length != 1 + ip_len + 1 + time_len) {
    return false;
  }

  string time_str(&plaintext[1 + ip_len + 1], time_len);
  int64 timestamp;
  if (!base::StringToInt64(time_str, &timestamp)) {
    return false;
  }

  ip_.assign(&plaintext[1], ip_len);
  timestamp_ = timestamp;
  return true;
}

}  // namespace net
