// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/StyleMotionData.h"

#include "core/style/DataEquivalency.h"

namespace blink {

bool StyleMotionData::operator==(const StyleMotionData& o) const {
  if (m_anchor != o.m_anchor || m_position != o.m_position ||
      m_distance != o.m_distance || m_rotation != o.m_rotation)
    return false;

  return dataEquivalent(m_path, o.m_path);
}

}  // namespace blink
