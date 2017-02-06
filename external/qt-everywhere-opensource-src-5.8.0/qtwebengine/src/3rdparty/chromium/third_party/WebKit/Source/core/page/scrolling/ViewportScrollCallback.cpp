// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/ViewportScrollCallback.h"

#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/TopControls.h"
#include "core/page/scrolling/OverscrollController.h"
#include "core/page/scrolling/ScrollState.h"
#include "platform/geometry/FloatSize.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

ViewportScrollCallback::ViewportScrollCallback(
    TopControls* topControls, OverscrollController* overscrollController)
    : m_topControls(topControls)
    , m_overscrollController(overscrollController)
{
}

ViewportScrollCallback::~ViewportScrollCallback()
{
}

DEFINE_TRACE(ViewportScrollCallback)
{
    visitor->trace(m_topControls);
    visitor->trace(m_overscrollController);
    visitor->trace(m_scroller);
    ScrollStateCallback::trace(visitor);
}

bool ViewportScrollCallback::shouldScrollTopControls(const FloatSize& delta,
    ScrollGranularity granularity) const
{
    if (granularity != ScrollByPixel && granularity != ScrollByPrecisePixel)
        return false;

    if (!m_scroller)
        return false;

    DoublePoint maxScroll = m_scroller->maximumScrollPositionDouble();
    DoublePoint scrollPosition = m_scroller->scrollPositionDouble();

    // Always give the delta to the top controls if the scroll is in
    // the direction to show the top controls. If it's in the
    // direction to hide the top controls, only give the delta to the
    // top controls when the frame can scroll.
    return delta.height() < 0 || scrollPosition.y() < maxScroll.y();
}

void ViewportScrollCallback::handleEvent(ScrollState* state)
{
    FloatSize delta(state->deltaX(), state->deltaY());
    ScrollGranularity granularity =
        ScrollGranularity(static_cast<int>(state->deltaGranularity()));
    FloatSize remainingDelta = delta;

    // Scroll top controls.
    if (m_topControls) {
        if (state->isBeginning())
            m_topControls->scrollBegin();

        if (shouldScrollTopControls(delta, granularity))
            remainingDelta = m_topControls->scrollBy(delta);
    }

    bool topControlsConsumedScroll = remainingDelta.height() != delta.height();

    // Scroll the element's scrollable area.
    if (!m_scroller)
        return;

    ScrollResult result = m_scroller->userScroll(granularity, remainingDelta);

    // We consider top controls movement to be scrolling.
    result.didScrollY |= topControlsConsumedScroll;

    // Handle Overscroll.
    if (m_overscrollController) {
        FloatPoint position(state->positionX(), state->positionY());
        FloatSize velocity(state->velocityX(), state->velocityY());
        m_overscrollController->handleOverscroll(result, position, velocity);
    }

    // The viewport consumes everything.
    // TODO(bokan): This isn't actually consuming everything but doing so breaks
    // the main thread pull-to-refresh action. I need to figure out where that
    // gets activated. crbug.com/607210.
    state->consumeDeltaNative(
        state->deltaX() - result.unusedScrollDeltaX,
        state->deltaY() - result.unusedScrollDeltaY);
}

void ViewportScrollCallback::setScroller(ScrollableArea* scroller)
{
    m_scroller = scroller;
}

} // namespace blink
