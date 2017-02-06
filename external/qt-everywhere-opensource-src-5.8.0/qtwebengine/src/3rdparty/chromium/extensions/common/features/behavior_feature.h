// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_BEHAVIOR_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_BEHAVIOR_FEATURE_H_

#include <string>

#include "extensions/common/features/simple_feature.h"

namespace extensions {

// Implementation of the features in _behavior_features.json.
//
// For now, this is just constants + a vacuous implementation of SimpleFeature,
// for consistency with the other Feature types. One day we may add some
// additional functionality. One day we may also generate the feature names.
class BehaviorFeature : public SimpleFeature {
 public:
  // Constants corresponding to keys in _behavior_features.json.
  static const char kWhitelistedForIncognito[];
  static const char kDoNotSync[];
  static const char kZoomWithoutBubble[];
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_BEHAVIOR_FEATURE_H_
