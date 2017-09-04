// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OverscrollController_h
#define OverscrollController_h

#include "platform/geometry/FloatSize.h"
#include "platform/heap/Handle.h"

namespace blink {

class ChromeClient;
class FloatPoint;
struct ScrollResult;
class VisualViewport;

// Handles overscroll logic that occurs when the user scolls past the end of the
// document's scroll extents. Currently, this applies only to document scrolling
// and only on the root frame. We "accumulate" overscroll deltas from separate
// scroll events if the user continuously scrolls past the extent but reset it
// as soon as a gesture ends.
class OverscrollController : public GarbageCollected<OverscrollController> {
 public:
  static OverscrollController* create(const VisualViewport& visualViewport,
                                      ChromeClient& chromeClient) {
    return new OverscrollController(visualViewport, chromeClient);
  }

  void resetAccumulated(bool resetX, bool resetY);

  // Reports unused scroll as overscroll to the content layer. The position
  // argument is the most recent location of the gesture, the finger position
  // for touch scrolling and the cursor position for wheel. Velocity is used
  // in the case of a fling gesture where we want the overscroll to feel like
  // it has momentum.
  void handleOverscroll(const ScrollResult&,
                        const FloatPoint& positionInRootFrame,
                        const FloatSize& velocityInRootFrame);

  DECLARE_TRACE();

 private:
  OverscrollController(const VisualViewport&, ChromeClient&);

  WeakMember<const VisualViewport> m_visualViewport;
  WeakMember<ChromeClient> m_chromeClient;

  FloatSize m_accumulatedRootOverscroll;
};

}  // namespace blink

#endif  // OverscrollController_h
