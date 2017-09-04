// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the public base::FeatureList features for ARC.

#ifndef CHROMEOS_COMPONENTS_ARC_ARC_FEATURES_H_
#define CHROMEOS_COMPONENTS_ARC_ARC_FEATURES_H_

#include "base/feature_list.h"

namespace arc {

// Please keep alphabetized.
extern const base::Feature kArcUseAuthEndpointFeature;
extern const base::Feature kBootCompletedBroadcastFeature;

}  // namespace arc

#endif  // CHROMEOS_COMPONENTS_ARC_ARC_FEATURES_H_
