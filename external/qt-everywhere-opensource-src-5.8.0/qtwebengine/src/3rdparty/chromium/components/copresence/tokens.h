// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_TOKENS_H_
#define COMPONENTS_COPRESENCE_TOKENS_H_

#include <string>

#include "base/time/time.h"
#include "components/copresence/proto/enums.pb.h"

namespace copresence {

// It's an error to define these constructors inline,
// so they're defined in tokens.cc.

struct TransmittedToken final {
  TransmittedToken();

  std::string id;
  TokenMedium medium;
  base::Time start_time;
  base::Time stop_time;
  bool broadcast_confirmed;
};

struct ReceivedToken final {
  enum Validity {
    UNKNOWN = 0,
    VALID = 1,
    INVALID = 2
  };

  ReceivedToken();
  ReceivedToken(const std::string& id,
                TokenMedium medium,
                base::Time last_time);

  std::string id;
  TokenMedium medium;
  base::Time start_time;
  base::Time last_time;
  Validity valid;
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_TOKENS_H_
