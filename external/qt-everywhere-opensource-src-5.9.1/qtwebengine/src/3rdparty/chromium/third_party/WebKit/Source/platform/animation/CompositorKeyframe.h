// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorKeyframe_h
#define CompositorKeyframe_h

#include "platform/PlatformExport.h"
#include "wtf/PassRefPtr.h"

namespace cc {
class TimingFunction;
}

namespace blink {

class TimingFunction;

class PLATFORM_EXPORT CompositorKeyframe {
 public:
  virtual ~CompositorKeyframe() {}

  virtual double time() const = 0;

  PassRefPtr<TimingFunction> getTimingFunctionForTesting() const;

 private:
  virtual const cc::TimingFunction* ccTimingFunction() const = 0;
};

}  // namespace blink

#endif  // CompositorKeyframe_h
