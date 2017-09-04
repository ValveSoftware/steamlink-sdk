// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/state_machines/TextSegmentationMachineState.h"

#include "wtf/Assertions.h"
#include <ostream>  // NOLINT

namespace blink {

std::ostream& operator<<(std::ostream& os, TextSegmentationMachineState state) {
  static const char* const texts[] = {
      "Invalid", "NeedMoreCodeUnit", "NeedFollowingCodeUnit", "Finished",
  };

  const auto& it = std::begin(texts) + static_cast<size_t>(state);
  DCHECK_GE(it, std::begin(texts)) << "Unknown state value";
  DCHECK_LT(it, std::end(texts)) << "Unknown state value";
  return os << *it;
}

}  // namespace blink
