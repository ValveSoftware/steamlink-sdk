// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/TopControls.h"

#include "core/frame/FrameHost.h"
#include "core/frame/VisualViewport.h"
#include "core/page/ChromeClient.h"
#include "platform/geometry/FloatSize.h"
#include <algorithm> // for std::min and std::max

namespace blink {

TopControls::TopControls(const FrameHost& frameHost)
    : m_frameHost(&frameHost)
    , m_height(0)
    , m_shownRatio(0)
    , m_baselineContentOffset(0)
    , m_accumulatedScrollDelta(0)
    , m_shrinkViewport(false)
    , m_permittedState(WebTopControlsBoth)
{
}

DEFINE_TRACE(TopControls)
{
    visitor->trace(m_frameHost);
}

void TopControls::scrollBegin()
{
    resetBaseline();
}

FloatSize TopControls::scrollBy(FloatSize pendingDelta)
{
    if ((m_permittedState == WebTopControlsShown && pendingDelta.height() > 0) || (m_permittedState == WebTopControlsHidden && pendingDelta.height() < 0))
        return pendingDelta;

    if (m_height == 0)
        return pendingDelta;

    float oldOffset = contentOffset();
    float pageScale = m_frameHost->visualViewport().scale();

    // Update accumulated vertical scroll and apply it to top controls
    // Compute scroll delta in viewport space by applying page scale
    m_accumulatedScrollDelta += pendingDelta.height() * pageScale;

    float newContentOffset = m_baselineContentOffset - m_accumulatedScrollDelta;

    setShownRatio(newContentOffset / m_height);

    // Reset baseline when controls are fully visible
    if (m_shownRatio == 1)
        resetBaseline();

    // Clamp and use the expected content offset so that we don't return
    // spurrious remaining scrolls due to the imprecision of the shownRatio.
    newContentOffset = std::min(newContentOffset, m_height);
    newContentOffset = std::max(newContentOffset, 0.f);

    // We negate the difference because scrolling down (positive delta) causes
    // top controls to hide (negative offset difference).
    FloatSize appliedDelta(0, (oldOffset - newContentOffset) / pageScale);
    return pendingDelta - appliedDelta;
}

void TopControls::resetBaseline()
{
    m_accumulatedScrollDelta = 0;
    m_baselineContentOffset = contentOffset();
}

float TopControls::layoutHeight()
{
    return m_shrinkViewport ? m_height : 0;
}

float TopControls::contentOffset()
{
    return m_shownRatio * m_height;
}

void TopControls::setShownRatio(float shownRatio)
{
    shownRatio = std::min(shownRatio, 1.f);
    shownRatio = std::max(shownRatio, 0.f);

    if (m_shownRatio == shownRatio)
        return;

    m_shownRatio = shownRatio;
    m_frameHost->chromeClient().didUpdateTopControls();
}

void TopControls::updateConstraintsAndState(
    WebTopControlsState constraints,
    WebTopControlsState current,
    bool animate)
{
    m_permittedState = constraints;

    DCHECK(!(constraints == WebTopControlsShown && current == WebTopControlsHidden));
    DCHECK(!(constraints == WebTopControlsHidden && current == WebTopControlsShown));

    // If the change should be animated, let the impl thread drive the change.
    // Otherwise, immediately set the shown ratio so we don't have to wait for
    // a commit from the impl thread.
    if (animate)
        return;

    if (constraints == WebTopControlsBoth && current == WebTopControlsBoth)
        return;

    if (constraints == WebTopControlsHidden || current == WebTopControlsHidden)
        setShownRatio(0.f);
    else
        setShownRatio(1.f);
}

void TopControls::setHeight(float height, bool shrinkViewport)
{
    if (m_height == height && m_shrinkViewport == shrinkViewport)
        return;

    m_height = height;
    m_shrinkViewport = shrinkViewport;
    m_frameHost->chromeClient().didUpdateTopControls();
}

} // namespace blink
