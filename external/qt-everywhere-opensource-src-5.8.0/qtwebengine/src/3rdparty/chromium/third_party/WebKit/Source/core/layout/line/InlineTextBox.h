/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2009, 2010, 2011 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef InlineTextBox_h
#define InlineTextBox_h

#include "core/CoreExport.h"
#include "core/layout/api/LineLayoutText.h"
#include "core/layout/api/SelectionState.h"
#include "core/layout/line/InlineBox.h"
#include "platform/text/TextRun.h"
#include "wtf/Forward.h"

namespace blink {

class DocumentMarker;
class GraphicsContext;

const unsigned short cNoTruncation = USHRT_MAX;
const unsigned short cFullTruncation = USHRT_MAX - 1;

class CORE_EXPORT InlineTextBox : public InlineBox {
public:
    InlineTextBox(LineLayoutItem item, int start, unsigned short length)
        : InlineBox(item)
        , m_prevTextBox(nullptr)
        , m_nextTextBox(nullptr)
        , m_start(start)
        , m_len(length)
        , m_truncation(cNoTruncation)
    {
        setIsText(true);
    }

    LineLayoutText getLineLayoutItem() const { return LineLayoutText(InlineBox::getLineLayoutItem()); }

    void destroy() final;

    InlineTextBox* prevTextBox() const { return m_prevTextBox; }
    InlineTextBox* nextTextBox() const { return m_nextTextBox; }
    void setNextTextBox(InlineTextBox* n) { m_nextTextBox = n; }
    void setPreviousTextBox(InlineTextBox* p) { m_prevTextBox = p; }

    // FIXME: These accessors should ASSERT(!isDirty()). See https://bugs.webkit.org/show_bug.cgi?id=97264
    unsigned start() const { return m_start; }
    unsigned end() const { return m_len ? m_start + m_len - 1 : m_start; }
    unsigned len() const { return m_len; }

    void offsetRun(int delta);

    unsigned short truncation() const { return m_truncation; }

    void markDirty() final;

    using InlineBox::hasHyphen;
    using InlineBox::setHasHyphen;
    using InlineBox::canHaveLeadingExpansion;
    using InlineBox::setCanHaveLeadingExpansion;

    static inline bool compareByStart(const InlineTextBox* first, const InlineTextBox* second) { return first->start() < second->start(); }

    int baselinePosition(FontBaseline) const final;
    LayoutUnit lineHeight() const final;

    bool getEmphasisMarkPosition(const ComputedStyle&, TextEmphasisPosition&) const;

    LayoutRect logicalOverflowRect() const;
    void setLogicalOverflowRect(const LayoutRect&);
    LayoutUnit logicalTopVisualOverflow() const { return logicalOverflowRect().y(); }
    LayoutUnit logicalBottomVisualOverflow() const { return logicalOverflowRect().maxY(); }

    // charactersWithHyphen, if provided, must not be destroyed before the TextRun.
    TextRun constructTextRun(const ComputedStyle&, const Font&, StringBuilder* charactersWithHyphen = nullptr) const;
    TextRun constructTextRun(const ComputedStyle&, const Font&, StringView, int maximumLength, StringBuilder* charactersWithHyphen = nullptr) const;

#ifndef NDEBUG
    void showBox(int = 0) const override;
#endif
    const char* boxName() const override;
    String debugName() const override;

    String text() const;

public:
    TextRun constructTextRunForInspector(const ComputedStyle&, const Font&) const;
    LayoutRect calculateBoundaries() const override { return LayoutRect(x(), y(), width(), height()); }

    virtual LayoutRect localSelectionRect(int startPos, int endPos) const;
    bool isSelected(int startPos, int endPos) const;
    void selectionStartEnd(int& sPos, int& ePos) const;

    // These functions both paint markers and update the DocumentMarker's renderedRect.
    virtual void paintDocumentMarker(GraphicsContext&, const LayoutPoint& boxOrigin, DocumentMarker*, const ComputedStyle&, const Font&, bool grammar) const;
    virtual void paintTextMatchMarkerForeground(const PaintInfo&, const LayoutPoint& boxOrigin, DocumentMarker*, const ComputedStyle&, const Font&) const;
    virtual void paintTextMatchMarkerBackground(const PaintInfo&, const LayoutPoint& boxOrigin, DocumentMarker*, const ComputedStyle&, const Font&) const;

    void move(const LayoutSize&) final;

protected:
    void paint(const PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom) const override;
    bool nodeAtPoint(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom) override;

private:
    void deleteLine() final;
    void extractLine() final;
    void attachLine() final;

public:
    SelectionState getSelectionState() const final;
    bool hasWrappedSelectionNewline() const;
    float newlineSpaceWidth() const;

private:
    void setTruncation(unsigned);

    void clearTruncation() final;
    LayoutUnit placeEllipsisBox(bool flowIsLTR, LayoutUnit visibleLeftEdge, LayoutUnit visibleRightEdge, LayoutUnit ellipsisWidth, LayoutUnit &truncatedWidth, bool& foundBox) final;

public:
    bool isLineBreak() const final;

    void setExpansion(int newExpansion)
    {
        m_logicalWidth -= expansion();
        InlineBox::setExpansion(newExpansion);
        m_logicalWidth += newExpansion;
    }

private:
    bool isInlineTextBox() const final { return true; }

public:
    int caretMinOffset() const final;
    int caretMaxOffset() const final;

    LayoutUnit textPos() const; // returns the x position relative to the left start of the text line.

public:
    virtual int offsetForPosition(LayoutUnit x, bool includePartialGlyphs = true) const;
    virtual LayoutUnit positionForOffset(int offset) const;

    bool containsCaretOffset(int offset) const; // false for offset after line break

    // Fills a vector with the pixel width of each character.
    void characterWidths(Vector<float>&) const;

private:
    InlineTextBox* m_prevTextBox; // The previous box that also uses our LayoutObject
    InlineTextBox* m_nextTextBox; // The next box that also uses our LayoutObject

    int m_start;
    unsigned short m_len;

    // Where to truncate when text overflow is applied.  We use special constants to denote
    // no truncation (the whole run paints) and full truncation (nothing paints at all).
    unsigned short m_truncation;

private:
    TextRun::ExpansionBehavior expansionBehavior() const
    {
        return (canHaveLeadingExpansion() ? TextRun::AllowLeadingExpansion : TextRun::ForbidLeadingExpansion)
            | (expansion() && nextLeafChild() ? TextRun::AllowTrailingExpansion : TextRun::ForbidTrailingExpansion);
    }
};

DEFINE_INLINE_BOX_TYPE_CASTS(InlineTextBox);

} // namespace blink

#endif // InlineTextBox_h
