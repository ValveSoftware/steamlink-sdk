// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/activation_state.h"

#include <ostream>

#include "base/logging.h"

namespace subresource_filter {

std::ostream& operator<<(std::ostream& os, const ActivationState& state) {
  switch (state) {
    case ActivationState::DISABLED:
      os << "DISABLED";
      break;
    case ActivationState::DRYRUN:
      os << "DRYRUN";
      break;
    case ActivationState::ENABLED:
      os << "ENABLED";
      break;
    default:
      NOTREACHED();
      break;
  }
  return os;
}

}  // namespace subresource_filter
