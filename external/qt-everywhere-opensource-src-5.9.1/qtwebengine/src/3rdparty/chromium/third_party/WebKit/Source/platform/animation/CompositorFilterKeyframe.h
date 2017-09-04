// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorFilterKeyframe_h
#define CompositorFilterKeyframe_h

#include "cc/animation/keyframed_animation_curve.h"
#include "platform/PlatformExport.h"
#include "platform/animation/CompositorKeyframe.h"
#include "platform/graphics/CompositorFilterOperations.h"
#include "wtf/Noncopyable.h"

namespace blink {

class TimingFunction;

class PLATFORM_EXPORT CompositorFilterKeyframe : public CompositorKeyframe {
  WTF_MAKE_NONCOPYABLE(CompositorFilterKeyframe);

 public:
  CompositorFilterKeyframe(double time,
                           CompositorFilterOperations value,
                           const TimingFunction&);
  ~CompositorFilterKeyframe();

  std::unique_ptr<cc::FilterKeyframe> cloneToCC() const;

  // CompositorKeyframe implementation.
  double time() const override;
  const cc::TimingFunction* ccTimingFunction() const override;

 private:
  std::unique_ptr<cc::FilterKeyframe> m_filterKeyframe;
};

}  // namespace blink

#endif  // CompositorFilterKeyframe_h
