// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleMotionData_h
#define StyleMotionData_h

#include "core/style/StyleOffsetRotation.h"
#include "core/style/StylePath.h"
#include "platform/Length.h"
#include "platform/LengthPoint.h"
#include "wtf/Allocator.h"

namespace blink {

class StyleMotionData {
  DISALLOW_NEW();

 public:
  StyleMotionData(const LengthPoint& anchor,
                  const LengthPoint& position,
                  StylePath* path,
                  const Length& distance,
                  StyleOffsetRotation rotation)
      : m_anchor(anchor),
        m_position(position),
        m_path(path),
        m_distance(distance),
        m_rotation(rotation) {}

  bool operator==(const StyleMotionData&) const;

  bool operator!=(const StyleMotionData& o) const { return !(*this == o); }

  // Must be public for SET_VAR in ComputedStyle.h
  LengthPoint m_anchor;
  LengthPoint m_position;
  RefPtr<StylePath> m_path;  // nullptr indicates path is 'none'
  Length m_distance;
  StyleOffsetRotation m_rotation;
};

}  // namespace blink

#endif  // StyleMotionData_h
