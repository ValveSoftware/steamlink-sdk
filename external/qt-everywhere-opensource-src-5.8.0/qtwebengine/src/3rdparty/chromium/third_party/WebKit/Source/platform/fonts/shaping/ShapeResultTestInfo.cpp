// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/ShapeResultTestInfo.h"

#include "platform/fonts/Font.h"
#include "platform/fonts/shaping/ShapeResultInlineHeaders.h"

namespace blink {

unsigned ShapeResultTestInfo::numberOfRunsForTesting() const
{
    return m_runs.size();
}

bool ShapeResultTestInfo::runInfoForTesting(unsigned runIndex,
    unsigned& startIndex, unsigned& numGlyphs, hb_script_t& script) const
{
    if (runIndex < m_runs.size() && m_runs[runIndex]) {
        startIndex = m_runs[runIndex]->m_startIndex;
        numGlyphs = m_runs[runIndex]->m_glyphData.size();
        script = m_runs[runIndex]->m_script;
        return true;
    }
    return false;
}

uint16_t ShapeResultTestInfo::glyphForTesting(unsigned runIndex,
    size_t glyphIndex) const
{
    return m_runs[runIndex]->m_glyphData[glyphIndex].glyph;
}

float ShapeResultTestInfo::advanceForTesting(unsigned runIndex,
    size_t glyphIndex) const
{
    return m_runs[runIndex]->m_glyphData[glyphIndex].advance;
}

} // namespace blink
