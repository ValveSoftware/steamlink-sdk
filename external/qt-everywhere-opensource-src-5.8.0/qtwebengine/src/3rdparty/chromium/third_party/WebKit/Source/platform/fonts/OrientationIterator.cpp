// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "OrientationIterator.h"

#include "wtf/PtrUtil.h"

namespace blink {

OrientationIterator::OrientationIterator(const UChar* buffer, unsigned bufferSize, FontOrientation runOrientation)
    : m_utf16Iterator(wrapUnique(new UTF16TextIterator(buffer, bufferSize)))
    , m_bufferSize(bufferSize)
    , m_atEnd(bufferSize == 0)
{
    // There's not much point in segmenting by isUprightInVertical if the text
    // orientation is not "mixed".
    ASSERT(runOrientation == FontOrientation::VerticalMixed);
}

bool OrientationIterator::consume(unsigned *orientationLimit, RenderOrientation* renderOrientation)
{
    if (m_atEnd)
        return false;

    RenderOrientation currentRenderOrientation = OrientationInvalid;
    UChar32 nextUChar32;
    while (m_utf16Iterator->consume(nextUChar32)) {
        if (currentRenderOrientation == OrientationInvalid
            || !Character::isGraphemeExtended(nextUChar32)) {

            RenderOrientation previousRenderOrientation = currentRenderOrientation;
            currentRenderOrientation =
                Character::isUprightInMixedVertical(nextUChar32)
                ? OrientationKeep : OrientationRotateSideways;
            if (previousRenderOrientation != currentRenderOrientation
                && previousRenderOrientation != OrientationInvalid) {
                *orientationLimit = m_utf16Iterator->offset();
                *renderOrientation = previousRenderOrientation;
                return true;
            }
        }
        m_utf16Iterator->advance();
    }
    *orientationLimit = m_bufferSize;
    *renderOrientation = currentRenderOrientation;
    m_atEnd = true;
    return true;
}

} // namespace blink
