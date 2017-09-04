// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_UTIL_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_UTIL_H_

#include <string>

namespace variations {

// Get the current set of chosen FieldTrial groups (aka variations) and send
// them to the logging module so it can save it for crash dumps.
void SetVariationListCrashKeys();

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_UTIL_H_
