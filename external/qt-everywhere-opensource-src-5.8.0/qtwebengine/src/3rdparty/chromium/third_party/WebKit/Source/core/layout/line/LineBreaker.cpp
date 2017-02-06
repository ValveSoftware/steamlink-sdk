/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "core/layout/line/LineBreaker.h"

#include "core/layout/line/BreakingContextInlineHeaders.h"

namespace blink {

void LineBreaker::skipLeadingWhitespace(InlineBidiResolver& resolver, LineInfo& lineInfo, LineWidth& width)
{
    while (!resolver.position().atEnd() && !requiresLineBox(resolver.position(), lineInfo, LeadingWhitespace)) {
        LineLayoutItem lineLayoutItem = resolver.position().getLineLayoutItem();
        if (lineLayoutItem.isOutOfFlowPositioned()) {
            setStaticPositions(m_block, LineLayoutBox(lineLayoutItem), width.indentText());
            if (lineLayoutItem.style()->isOriginalDisplayInlineType()) {
                resolver.runs().addRun(createRun(0, 1, LineLayoutItem(lineLayoutItem), resolver));
                lineInfo.incrementRunsFromLeadingWhitespace();
            }
        } else if (lineLayoutItem.isFloating()) {
            m_block.insertFloatingObject(LineLayoutBox(lineLayoutItem));
            m_block.positionNewFloats(&width);
        }
        resolver.position().increment(&resolver);
    }
    resolver.commitExplicitEmbedding(resolver.runs());
}

void LineBreaker::reset()
{
    m_positionedObjects.clear();
    m_hyphenated = false;
    m_clear = ClearNone;
}

InlineIterator LineBreaker::nextLineBreak(InlineBidiResolver& resolver, LineInfo& lineInfo,
    LayoutTextInfo& layoutTextInfo, WordMeasurements& wordMeasurements)
{
    reset();

    ASSERT(resolver.position().root() == m_block);

    bool appliedStartWidth = resolver.position().offset() > 0;

    LineWidth width(m_block, lineInfo.isFirstLine(), requiresIndent(lineInfo.isFirstLine(), lineInfo.previousLineBrokeCleanly(), m_block.styleRef()));

    skipLeadingWhitespace(resolver, lineInfo, width);

    if (resolver.position().atEnd())
        return resolver.position();

    BreakingContext context(resolver, lineInfo, width, layoutTextInfo, appliedStartWidth, m_block);

    while (context.currentItem()) {
        context.initializeForCurrentObject();
        if (context.currentItem().isBR()) {
            context.handleBR(m_clear);
        } else if (context.currentItem().isOutOfFlowPositioned()) {
            context.handleOutOfFlowPositioned(m_positionedObjects);
        } else if (context.currentItem().isFloating()) {
            context.handleFloat();
        } else if (context.currentItem().isLayoutInline()) {
            context.handleEmptyInline();
        } else if (context.currentItem().isAtomicInlineLevel()) {
            context.handleReplaced();
        } else if (context.currentItem().isText()) {
            if (context.handleText(wordMeasurements, m_hyphenated)) {
                // We've hit a hard text line break. Our line break iterator is updated, so go ahead and early return.
                return context.lineBreak();
            }
        } else {
            ASSERT_NOT_REACHED();
        }

        if (context.atEnd())
            return context.handleEndOfLine();

        context.commitAndUpdateLineBreakIfNeeded();

        if (context.atEnd())
            return context.handleEndOfLine();

        context.increment();
    }

    context.clearLineBreakIfFitsOnLine();

    return context.handleEndOfLine();
}

} // namespace blink
