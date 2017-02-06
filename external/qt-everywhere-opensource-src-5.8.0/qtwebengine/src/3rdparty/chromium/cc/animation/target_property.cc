// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/target_property.h"

#include "base/macros.h"

namespace cc {

namespace {

// This should match the TargetProperty enum.
static const char* const s_targetPropertyNames[] = {
    "TRANSFORM", "OPACITY", "FILTER", "SCROLL_OFFSET", "BACKGROUND_COLOR"};

static_assert(static_cast<int>(TargetProperty::LAST_TARGET_PROPERTY) + 1 ==
                  arraysize(s_targetPropertyNames),
              "TargetPropertyEnumSize should equal the number of elements in "
              "s_targetPropertyNames");

}  // namespace

const char* TargetProperty::GetName(TargetProperty::Type property) {
  return s_targetPropertyNames[property];
}

}  // namespace cc
