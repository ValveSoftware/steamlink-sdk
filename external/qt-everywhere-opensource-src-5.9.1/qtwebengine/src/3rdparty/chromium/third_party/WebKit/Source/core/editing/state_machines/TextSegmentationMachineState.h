// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextSegmentationMachineState_h
#define TextSegmentationMachineState_h

#include "core/CoreExport.h"
#include <ostream>  // NOLINT

namespace blink {

enum class TextSegmentationMachineState {
  // Indicates the state machine is in invalid state.
  Invalid,
  // Indicates the state machine needs more code units to transit the state.
  NeedMoreCodeUnit,
  // Indicates the state machine needs following code units to transit the
  // state.
  NeedFollowingCodeUnit,
  // Indicates the state machine found a boundary.
  Finished,
};

CORE_EXPORT std::ostream& operator<<(std::ostream&,
                                     TextSegmentationMachineState);

}  // namespace blink

#endif  // TextSegmentationMachineState_h
