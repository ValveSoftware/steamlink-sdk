/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/scroll/ScrollAnimatorBase.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/scroll/ScrollableArea.h"
#include "wtf/MathExtras.h"

namespace blink {

ScrollAnimatorBase::ScrollAnimatorBase(ScrollableArea* scrollableArea)
    : m_scrollableArea(scrollableArea)
{
}

ScrollAnimatorBase::~ScrollAnimatorBase()
{
}

FloatSize ScrollAnimatorBase::computeDeltaToConsume(const FloatSize& delta) const
{
    FloatPoint newPos = toFloatPoint(m_scrollableArea->clampScrollPosition(m_currentPos + delta));
    return newPos - m_currentPos;
}

ScrollResult ScrollAnimatorBase::userScroll(ScrollGranularity, const FloatSize& delta)
{
    FloatSize consumedDelta = computeDeltaToConsume(delta);
    FloatPoint newPos = m_currentPos + consumedDelta;
    if (m_currentPos == newPos)
        return ScrollResult(false, false, delta.width(), delta.height());

    m_currentPos = newPos;

    notifyPositionChanged();

    return ScrollResult(
        consumedDelta.width(),
        consumedDelta.height(),
        delta.width() - consumedDelta.width(),
        delta.height() - consumedDelta.height());
}

void ScrollAnimatorBase::scrollToOffsetWithoutAnimation(const FloatPoint& offset)
{
    m_currentPos = offset;
    notifyPositionChanged();
}

void ScrollAnimatorBase::setCurrentPosition(const FloatPoint& position)
{
    m_currentPos = position;
}

FloatPoint ScrollAnimatorBase::currentPosition() const
{
    return m_currentPos;
}

void ScrollAnimatorBase::notifyPositionChanged()
{
    m_scrollableArea->scrollPositionChanged(m_currentPos, UserScroll);
}

DEFINE_TRACE(ScrollAnimatorBase)
{
    visitor->trace(m_scrollableArea);
    ScrollAnimatorCompositorCoordinator::trace(visitor);
}

} // namespace blink
