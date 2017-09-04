// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_API_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_API_FEATURE_H_

#include "extensions/common/features/simple_feature.h"

namespace extensions {

// TODO(devlin): APIFeatures are just SimpleFeatures now. However, there's a
// bunch of stuff (including the feature compiler) that relies on the presence
// of an APIFeature class. For now, just typedef this, but eventually we should
// remove it.
using APIFeature = SimpleFeature;

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_API_FEATURE_H_
