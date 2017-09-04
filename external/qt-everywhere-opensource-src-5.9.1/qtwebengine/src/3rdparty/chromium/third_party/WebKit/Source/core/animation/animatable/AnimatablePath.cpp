// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/animatable/AnimatablePath.h"

#include "core/style/DataEquivalency.h"
#include "core/svg/SVGPathBlender.h"
#include "core/svg/SVGPathByteStreamBuilder.h"
#include "core/svg/SVGPathByteStreamSource.h"
#include <memory>

namespace blink {

bool AnimatablePath::usesDefaultInterpolationWith(
    const AnimatableValue* value) const {
  // Default interpolation is used if the paths have different lengths,
  // or the paths have a segment with different types (ignoring "relativeness").

  const StylePath* toPath = toAnimatablePath(value)->path();
  if (!m_path || !toPath)
    return true;
  SVGPathByteStreamSource fromSource(path()->byteStream());
  SVGPathByteStreamSource toSource(toPath->byteStream());

  while (fromSource.hasMoreData()) {
    if (!toSource.hasMoreData())
      return true;

    PathSegmentData fromSeg = fromSource.parseSegment();
    PathSegmentData toSeg = toSource.parseSegment();
    DCHECK_NE(fromSeg.command, PathSegUnknown);
    DCHECK_NE(toSeg.command, PathSegUnknown);

    if (toAbsolutePathSegType(fromSeg.command) !=
        toAbsolutePathSegType(toSeg.command))
      return true;
  }

  return toSource.hasMoreData();
}

PassRefPtr<AnimatableValue> AnimatablePath::interpolateTo(
    const AnimatableValue* value,
    double fraction) const {
  if (usesDefaultInterpolationWith(value))
    return defaultInterpolateTo(this, value, fraction);

  std::unique_ptr<SVGPathByteStream> byteStream = SVGPathByteStream::create();
  SVGPathByteStreamBuilder builder(*byteStream);

  SVGPathByteStreamSource fromSource(path()->byteStream());
  SVGPathByteStreamSource toSource(
      toAnimatablePath(value)->path()->byteStream());

  SVGPathBlender blender(&fromSource, &toSource, &builder);
  bool ok = blender.blendAnimatedPath(fraction);
  ALLOW_UNUSED_LOCAL(ok);
  return AnimatablePath::create(StylePath::create(std::move(byteStream)));
}

bool AnimatablePath::equalTo(const AnimatableValue* value) const {
  return dataEquivalent(m_path.get(), toAnimatablePath(value)->path());
}

}  // namespace blink
