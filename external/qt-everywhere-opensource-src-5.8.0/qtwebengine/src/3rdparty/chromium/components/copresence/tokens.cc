// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/tokens.h"

namespace copresence {

TransmittedToken::TransmittedToken()
    : medium(TOKEN_MEDIUM_UNKNOWN),
      broadcast_confirmed(false) {}

ReceivedToken::ReceivedToken()
    : medium(TOKEN_MEDIUM_UNKNOWN),
      valid(UNKNOWN) {}

ReceivedToken::ReceivedToken(const std::string& id,
                             TokenMedium medium,
                             base::Time last_time)
    : id(id),
      medium(medium),
      last_time(last_time),
      valid(UNKNOWN) {}

}  // namespace copresence
