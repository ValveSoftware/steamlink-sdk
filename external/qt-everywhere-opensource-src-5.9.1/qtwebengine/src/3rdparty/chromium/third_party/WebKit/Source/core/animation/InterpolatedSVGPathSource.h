// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolatedSVGPathSource_h
#define InterpolatedSVGPathSource_h

#include "core/animation/SVGPathSegInterpolationFunctions.h"
#include "core/svg/SVGPathData.h"
#include "wtf/Vector.h"

namespace blink {

class InterpolatedSVGPathSource {
  WTF_MAKE_NONCOPYABLE(InterpolatedSVGPathSource);
  STACK_ALLOCATED();

 public:
  InterpolatedSVGPathSource(const InterpolableList& listValue,
                            const Vector<SVGPathSegType>& pathSegTypes)
      : m_currentIndex(0),
        m_interpolablePathSegs(listValue),
        m_pathSegTypes(pathSegTypes) {
    DCHECK_EQ(m_interpolablePathSegs.length(), m_pathSegTypes.size());
  }

  bool hasMoreData() const;
  PathSegmentData parseSegment();

 private:
  PathCoordinates m_currentCoordinates;
  size_t m_currentIndex;
  const InterpolableList& m_interpolablePathSegs;
  const Vector<SVGPathSegType>& m_pathSegTypes;
};

bool InterpolatedSVGPathSource::hasMoreData() const {
  return m_currentIndex < m_interpolablePathSegs.length();
}

PathSegmentData InterpolatedSVGPathSource::parseSegment() {
  PathSegmentData segment =
      SVGPathSegInterpolationFunctions::consumeInterpolablePathSeg(
          *m_interpolablePathSegs.get(m_currentIndex),
          m_pathSegTypes.at(m_currentIndex), m_currentCoordinates);
  m_currentIndex++;
  return segment;
}

}  // namespace blink

#endif  // InterpolatedSVGPathSource_h
