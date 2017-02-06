// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/RunSegmenter.h"

#include "platform/fonts/ScriptRunIterator.h"
#include "platform/fonts/SmallCapsIterator.h"
#include "platform/fonts/SymbolsIterator.h"
#include "platform/fonts/UTF16TextIterator.h"
#include "platform/text/Character.h"
#include "wtf/Assertions.h"
#include "wtf/PtrUtil.h"

namespace blink {

RunSegmenter::RunSegmenter(const UChar* buffer, unsigned bufferSize, FontOrientation runOrientation)
    : m_bufferSize(bufferSize)
    , m_candidateRange({ 0, 0, USCRIPT_INVALID_CODE, OrientationIterator::OrientationKeep, FontFallbackPriority::Text })
    , m_scriptRunIterator(wrapUnique(new ScriptRunIterator(buffer, bufferSize)))
    , m_orientationIterator(runOrientation == FontOrientation::VerticalMixed ? wrapUnique(new OrientationIterator(buffer, bufferSize, runOrientation)) : nullptr)
    , m_symbolsIterator(wrapUnique(new SymbolsIterator(buffer, bufferSize)))
    , m_lastSplit(0)
    , m_scriptRunIteratorPosition(0)
    , m_orientationIteratorPosition(runOrientation == FontOrientation::VerticalMixed ? 0 : m_bufferSize)
    , m_symbolsIteratorPosition(0)
    , m_atEnd(false)
{
}

void RunSegmenter::consumeScriptIteratorPastLastSplit()
{
    ASSERT(m_scriptRunIterator);
    if (m_scriptRunIteratorPosition <= m_lastSplit && m_scriptRunIteratorPosition < m_bufferSize) {
        while (m_scriptRunIterator->consume(m_scriptRunIteratorPosition, m_candidateRange.script)) {
            if (m_scriptRunIteratorPosition > m_lastSplit)
                return;
        }
    }
}

void RunSegmenter::consumeOrientationIteratorPastLastSplit()
{
    if (m_orientationIterator && m_orientationIteratorPosition <= m_lastSplit && m_orientationIteratorPosition < m_bufferSize) {
        while (m_orientationIterator->consume(&m_orientationIteratorPosition, &m_candidateRange.renderOrientation)) {
            if (m_orientationIteratorPosition > m_lastSplit)
                return;
        }
    }
}

void RunSegmenter::consumeSymbolsIteratorPastLastSplit()
{
    ASSERT(m_symbolsIterator);
    if (m_symbolsIteratorPosition <= m_lastSplit && m_symbolsIteratorPosition < m_bufferSize) {
        while (m_symbolsIterator->consume(&m_symbolsIteratorPosition, &m_candidateRange.fontFallbackPriority)) {
            if (m_symbolsIteratorPosition > m_lastSplit)
                return;
        }
    }
}

bool RunSegmenter::consume(RunSegmenterRange* nextRange)
{
    if (m_atEnd || !m_bufferSize)
        return false;

    consumeScriptIteratorPastLastSplit();
    consumeOrientationIteratorPastLastSplit();
    consumeSymbolsIteratorPastLastSplit();

    if (m_scriptRunIteratorPosition <= m_orientationIteratorPosition
        && m_scriptRunIteratorPosition <= m_symbolsIteratorPosition) {
        m_lastSplit = m_scriptRunIteratorPosition;
    }

    if (m_orientationIteratorPosition <= m_scriptRunIteratorPosition
        && m_orientationIteratorPosition <= m_symbolsIteratorPosition) {
        m_lastSplit = m_orientationIteratorPosition;
    }

    if (m_symbolsIteratorPosition <= m_scriptRunIteratorPosition
        && m_symbolsIteratorPosition <= m_orientationIteratorPosition) {
        m_lastSplit = m_symbolsIteratorPosition;
    }

    m_candidateRange.start = m_candidateRange.end;
    m_candidateRange.end = m_lastSplit;
    *nextRange = m_candidateRange;

    m_atEnd = m_lastSplit == m_bufferSize;
    return true;
}


} // namespace blink
