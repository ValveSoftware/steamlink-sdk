/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LineWidth_h
#define LineWidth_h

#include "core/layout/api/LineLayoutBlockFlow.h"
#include "platform/LayoutUnit.h"
#include "wtf/Allocator.h"

namespace blink {

class FloatingObject;
class LineLayoutRubyRun;

enum WhitespaceTreatment { ExcludeWhitespace, IncludeWhitespace };

class LineWidth {
    STACK_ALLOCATED();
public:
    LineWidth(LineLayoutBlockFlow, bool isFirstLine, IndentTextOrNot);

    bool fitsOnLine() const
    {
        return LayoutUnit::fromFloatFloor(currentWidth()) <= m_availableWidth + LayoutUnit::epsilon();
    }
    bool fitsOnLine(float extra) const
    {
        float totalWidth = currentWidth() + extra;
        return LayoutUnit::fromFloatFloor(totalWidth) <= m_availableWidth + LayoutUnit::epsilon();
    }
    bool fitsOnLine(float extra, WhitespaceTreatment whitespaceTreatment) const
    {
        LayoutUnit w = LayoutUnit::fromFloatFloor(currentWidth() + extra);
        if (whitespaceTreatment == ExcludeWhitespace)
            w -= LayoutUnit::fromFloatCeil(trailingWhitespaceWidth());
        return w <= m_availableWidth;
    }

    // Note that m_uncommittedWidth may not be LayoutUnit-snapped at this point.  Because
    // currentWidth() is used by the code that lays out words in a single LayoutText, it's
    // expected that offsets will not be snapped until an InlineBox boundary is reached.
    float currentWidth() const { return m_committedWidth + m_uncommittedWidth; }

    // FIXME: We should eventually replace these three functions by ones that work on a higher abstraction.
    float uncommittedWidth() const { return m_uncommittedWidth; }
    float committedWidth() const { return m_committedWidth; }
    float availableWidth() const { return m_availableWidth; }
    float trailingWhitespaceWidth() const { return m_trailingWhitespaceWidth; }

    void updateAvailableWidth(LayoutUnit minimumHeight = LayoutUnit());
    void shrinkAvailableWidthForNewFloatIfNeeded(const FloatingObject&);
    void addUncommittedWidth(float delta) { m_uncommittedWidth += delta; }
    void commit();
    void applyOverhang(LineLayoutRubyRun, LineLayoutItem startLayoutItem, LineLayoutItem endLayoutItem);
    void fitBelowFloats(bool isFirstLine = false);
    void setTrailingWhitespaceWidth(float width) { m_trailingWhitespaceWidth = width; }
    void snapUncommittedWidth() { m_uncommittedWidth = LayoutUnit::fromFloatCeil(m_uncommittedWidth).toFloat(); }

    IndentTextOrNot indentText() const { return m_indentText; }

private:
    void computeAvailableWidthFromLeftAndRight();
    void updateLineDimension(LayoutUnit newLineTop, LayoutUnit newLineWidth, const LayoutUnit& newLineLeft, const LayoutUnit& newLineRight);
    void wrapNextToShapeOutside(bool isFirstLine);

    LineLayoutBlockFlow m_block;
    float m_uncommittedWidth;
    float m_committedWidth;
    float m_overhangWidth; // The amount by which |m_availableWidth| has been inflated to account for possible contraction due to ruby overhang.
    float m_trailingWhitespaceWidth;
    LayoutUnit m_left;
    LayoutUnit m_right;
    LayoutUnit m_availableWidth;
    bool m_isFirstLine;
    IndentTextOrNot m_indentText;
};

} // namespace blink

#endif // LineWidth_h
