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
#include "platform/scroll/ScrollableArea.h"
#include "wtf/MathExtras.h"

namespace blink {

ScrollAnimatorBase::ScrollAnimatorBase(ScrollableArea* scrollableArea)
    : m_scrollableArea(scrollableArea) {}

ScrollAnimatorBase::~ScrollAnimatorBase() {}

ScrollOffset ScrollAnimatorBase::computeDeltaToConsume(
    const ScrollOffset& delta) const {
  ScrollOffset newPos =
      m_scrollableArea->clampScrollOffset(m_currentOffset + delta);
  return newPos - m_currentOffset;
}

ScrollResult ScrollAnimatorBase::userScroll(ScrollGranularity,
                                            const ScrollOffset& delta) {
  ScrollOffset consumedDelta = computeDeltaToConsume(delta);
  ScrollOffset newPos = m_currentOffset + consumedDelta;
  if (m_currentOffset == newPos)
    return ScrollResult(false, false, delta.width(), delta.height());

  m_currentOffset = newPos;

  notifyOffsetChanged();

  return ScrollResult(consumedDelta.width(), consumedDelta.height(),
                      delta.width() - consumedDelta.width(),
                      delta.height() - consumedDelta.height());
}

void ScrollAnimatorBase::scrollToOffsetWithoutAnimation(
    const ScrollOffset& offset) {
  m_currentOffset = offset;
  notifyOffsetChanged();
}

void ScrollAnimatorBase::setCurrentOffset(const ScrollOffset& offset) {
  m_currentOffset = offset;
}

ScrollOffset ScrollAnimatorBase::currentOffset() const {
  return m_currentOffset;
}

void ScrollAnimatorBase::notifyOffsetChanged() {
  scrollOffsetChanged(m_currentOffset, UserScroll);
}

DEFINE_TRACE(ScrollAnimatorBase) {
  visitor->trace(m_scrollableArea);
  ScrollAnimatorCompositorCoordinator::trace(visitor);
}

}  // namespace blink
