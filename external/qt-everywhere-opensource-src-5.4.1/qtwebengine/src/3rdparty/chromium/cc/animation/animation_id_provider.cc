// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_id_provider.h"

namespace cc {

int AnimationIdProvider::NextAnimationId() {
  static int next_animation_id = 1;
  return next_animation_id++;
}

int AnimationIdProvider::NextGroupId() {
  static int next_group_id = 1;
  return next_group_id++;
}

}  // namespace cc
