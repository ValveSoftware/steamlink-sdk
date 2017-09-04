// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/OverscrollController.h"

#include "core/frame/VisualViewport.h"
#include "core/page/ChromeClient.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatSize.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

namespace {

// Report Overscroll if OverscrollDelta is greater than minimumOverscrollDelta
// to maintain consistency as done in the compositor.
const float minimumOverscrollDelta = 0.1;

void adjustOverscroll(FloatSize* unusedDelta) {
  DCHECK(unusedDelta);
  if (std::abs(unusedDelta->width()) < minimumOverscrollDelta)
    unusedDelta->setWidth(0);
  if (std::abs(unusedDelta->height()) < minimumOverscrollDelta)
    unusedDelta->setHeight(0);
}

}  // namespace

OverscrollController::OverscrollController(const VisualViewport& visualViewport,
                                           ChromeClient& chromeClient)
    : m_visualViewport(&visualViewport), m_chromeClient(&chromeClient) {}

DEFINE_TRACE(OverscrollController) {
  visitor->trace(m_visualViewport);
  visitor->trace(m_chromeClient);
}

void OverscrollController::resetAccumulated(bool resetX, bool resetY) {
  if (resetX)
    m_accumulatedRootOverscroll.setWidth(0);
  if (resetY)
    m_accumulatedRootOverscroll.setHeight(0);
}

void OverscrollController::handleOverscroll(
    const ScrollResult& scrollResult,
    const FloatPoint& positionInRootFrame,
    const FloatSize& velocityInRootFrame) {
  DCHECK(m_visualViewport);
  DCHECK(m_chromeClient);

  FloatSize unusedDelta(scrollResult.unusedScrollDeltaX,
                        scrollResult.unusedScrollDeltaY);
  adjustOverscroll(&unusedDelta);

  FloatSize deltaInViewport = unusedDelta.scaledBy(m_visualViewport->scale());
  FloatSize velocityInViewport =
      velocityInRootFrame.scaledBy(m_visualViewport->scale());
  FloatPoint positionInViewport =
      m_visualViewport->rootFrameToViewport(positionInRootFrame);

  resetAccumulated(scrollResult.didScrollX, scrollResult.didScrollY);

  if (deltaInViewport != FloatSize()) {
    m_accumulatedRootOverscroll += deltaInViewport;
    m_chromeClient->didOverscroll(deltaInViewport, m_accumulatedRootOverscroll,
                                  positionInViewport, velocityInViewport);
  }
}

}  // namespace blink
