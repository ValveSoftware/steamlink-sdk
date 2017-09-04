// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_COMMON_LOGGING_H_
#define BLIMP_COMMON_LOGGING_H_

#include <ostream>

#include "base/macros.h"
#include "blimp/common/blimp_common_export.h"

namespace blimp {

class BlimpMessage;

// Serializes a BlimpMessage in a human-readable format for logging.
// An example message would look like:
// "<type=TAB_CONTROL subtype=SIZE size=640x480:1.000000 size=19>"
BLIMP_COMMON_EXPORT std::ostream& operator<<(std::ostream& out,
                                             const BlimpMessage& message);

}  // namespace blimp

#endif  // BLIMP_COMMON_LOGGING_H_
