// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGPathSegInterpolationFunctions_h
#define SVGPathSegInterpolationFunctions_h

#include "core/animation/InterpolableValue.h"
#include "core/svg/SVGPathData.h"
#include <memory>

namespace blink {

struct PathCoordinates {
  double initialX = 0;
  double initialY = 0;
  double currentX = 0;
  double currentY = 0;
};

class SVGPathSegInterpolationFunctions {
  STATIC_ONLY(SVGPathSegInterpolationFunctions);

 public:
  static std::unique_ptr<InterpolableValue> consumePathSeg(
      const PathSegmentData&,
      PathCoordinates& currentCoordinates);
  static PathSegmentData consumeInterpolablePathSeg(
      const InterpolableValue&,
      SVGPathSegType,
      PathCoordinates& currentCoordinates);
};

}  // namespace blink

#endif  // SVGPathSegInterpolationFunctions_h
