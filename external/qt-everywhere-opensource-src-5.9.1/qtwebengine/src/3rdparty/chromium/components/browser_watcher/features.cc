// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/features.h"

namespace browser_watcher {

// Additional stability instrumentation.
const base::Feature kStabilityDebuggingFeature{
    "StabilityDebugging", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace browser_watcher
