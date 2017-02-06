/*
 * Copyright (C) 2004, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/editing/FrameCaret.h"

#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/commands/CompositeEditCommand.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLTextFormControlElement.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/page/Page.h"
#include "core/paint/PaintLayer.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

FrameCaret::FrameCaret(LocalFrame* frame)
    : m_frame(frame)
    , m_previousCaretVisibility(CaretVisibility::Hidden)
    , m_caretBlinkTimer(this, &FrameCaret::caretBlinkTimerFired)
    , m_caretRectDirty(true)
    , m_shouldPaintCaret(true)
    , m_isCaretBlinkingSuspended(false)
    , m_shouldShowBlockCursor(false)
{
    DCHECK(frame);
}

FrameCaret::~FrameCaret() = default;

DEFINE_TRACE(FrameCaret)
{
    visitor->trace(m_frame);
    visitor->trace(m_previousCaretNode);
    visitor->trace(m_caretPosition);
    CaretBase::trace(visitor);
}

void FrameCaret::setCaretPosition(const PositionWithAffinity& position)
{
    m_caretPosition = position;
    setCaretRectNeedsUpdate();
}

void FrameCaret::clear()
{
    m_caretPosition = PositionWithAffinity();
    setCaretRectNeedsUpdate();
}

inline static bool shouldStopBlinkingDueToTypingCommand(LocalFrame* frame)
{
    return frame->editor().lastEditCommand() && frame->editor().lastEditCommand()->shouldStopCaretBlinking();
}

void FrameCaret::updateAppearance()
{
    // Paint a block cursor instead of a caret in overtype mode unless the caret is at the end of a line (in this case
    // the FrameSelection will paint a blinking caret as usual).
    bool paintBlockCursor = m_shouldShowBlockCursor && isActive() && !isLogicalEndOfLine(createVisiblePosition(m_caretPosition));

    bool shouldBlink = !paintBlockCursor && shouldBlinkCaret();

    // If the caret moved, stop the blink timer so we can restart with a
    // black caret in the new location.
    if (!shouldBlink || shouldStopBlinkingDueToTypingCommand(m_frame))
        stopCaretBlinkTimer();

    // Start blinking with a black caret. Be sure not to restart if we're
    // already blinking in the right location.
    if (shouldBlink)
        startBlinkCaret();
}

void FrameCaret::stopCaretBlinkTimer()
{
    if (m_caretBlinkTimer.isActive() || m_shouldPaintCaret)
        setCaretRectNeedsUpdate();
    m_shouldPaintCaret = false;
    m_caretBlinkTimer.stop();
}

void FrameCaret::startBlinkCaret()
{
    // Start blinking with a black caret. Be sure not to restart if we're
    // already blinking in the right location.
    if (m_caretBlinkTimer.isActive())
        return;

    if (double blinkInterval = LayoutTheme::theme().caretBlinkInterval())
        m_caretBlinkTimer.startRepeating(blinkInterval, BLINK_FROM_HERE);

    m_shouldPaintCaret = true;
    setCaretRectNeedsUpdate();

}

void FrameCaret::setCaretVisibility(CaretVisibility visibility)
{
    if (getCaretVisibility() == visibility)
        return;

    CaretBase::setCaretVisibility(visibility);

    updateAppearance();
}

void FrameCaret::setCaretRectNeedsUpdate()
{
    if (m_caretRectDirty)
        return;
    m_caretRectDirty = true;

    if (Page* page = m_frame->page())
        page->animator().scheduleVisualUpdate(m_frame->localFrameRoot());
}

bool FrameCaret::caretPositionIsValidForDocument(const Document& document) const
{
    if (!isActive())
        return true;

    return m_caretPosition.position().document() == document
        && !m_caretPosition.position().isOrphan();
}

void FrameCaret::invalidateCaretRect()
{
    if (!m_caretRectDirty)
        return;
    m_caretRectDirty = false;

    DCHECK(caretPositionIsValidForDocument(*m_frame->document()));
    LayoutObject* layoutObject = nullptr;
    LayoutRect newRect;
    if (isActive())
        newRect = localCaretRectOfPosition(m_caretPosition, layoutObject);
    Node* newNode = layoutObject ? layoutObject->node() : nullptr;

    if (!m_caretBlinkTimer.isActive()
        && newNode == m_previousCaretNode
        && newRect == m_previousCaretRect
        && getCaretVisibility() == m_previousCaretVisibility)
        return;

    LayoutViewItem view = m_frame->document()->layoutViewItem();
    if (m_previousCaretNode && (shouldRepaintCaret(*m_previousCaretNode) || shouldRepaintCaret(view)))
        invalidateLocalCaretRect(m_previousCaretNode.get(), m_previousCaretRect);
    if (newNode && (shouldRepaintCaret(*newNode) || shouldRepaintCaret(view)))
        invalidateLocalCaretRect(newNode, newRect);
    m_previousCaretNode = newNode;
    m_previousCaretRect = newRect;
    m_previousCaretVisibility = getCaretVisibility();
}

IntRect FrameCaret::absoluteCaretBounds()
{
    DCHECK_NE(m_frame->document()->lifecycle().state(), DocumentLifecycle::InPaintInvalidation);
    m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();
    if (!isActive()) {
        clearCaretRect();
    }  else {
        if (enclosingTextFormControl(m_caretPosition.position()))
            updateCaretRect(PositionWithAffinity(isVisuallyEquivalentCandidate(m_caretPosition.position()) ? m_caretPosition.position() : Position(), m_caretPosition.affinity()));
        else
            updateCaretRect(createVisiblePosition(m_caretPosition));
    }
    return absoluteBoundsForLocalRect(m_caretPosition.position().anchorNode(), localCaretRectWithoutUpdate());
}

void FrameCaret::setShouldShowBlockCursor(bool shouldShowBlockCursor)
{
    m_shouldShowBlockCursor = shouldShowBlockCursor;

    m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

    updateAppearance();
}

void FrameCaret::paintCaret(GraphicsContext& context, const LayoutPoint& paintOffset)
{
    if (!(isActive() && m_shouldPaintCaret))
        return;

    updateCaretRect(m_caretPosition);
    CaretBase::paintCaret(m_caretPosition.position().anchorNode(), context, paintOffset, DisplayItem::Caret);
}

void FrameCaret::dataWillChange(const CharacterData& node)
{
    if (node == m_previousCaretNode) {
        // This invalidation is eager, and intentionally uses stale state.
        DisableCompositingQueryAsserts disabler;
        invalidateLocalCaretRect(m_previousCaretNode.get(), m_previousCaretRect);
    }
}

void FrameCaret::nodeWillBeRemoved(Node& node)
{
    if (node != m_previousCaretNode)
        return;
    // Hits in ManualTests/caret-paint-after-last-text-is-removed.html
    DisableCompositingQueryAsserts disabler;
    invalidateLocalCaretRect(m_previousCaretNode.get(), m_previousCaretRect);
    m_previousCaretNode = nullptr;
    m_previousCaretRect = LayoutRect();
    m_previousCaretVisibility = CaretVisibility::Hidden;
}

void FrameCaret::documentDetached()
{
    m_caretPosition = PositionWithAffinity();
    m_caretBlinkTimer.stop();
    m_previousCaretNode.clear();
}

bool FrameCaret::shouldBlinkCaret() const
{
    if (!caretIsVisible() || !isActive())
        return false;

    if (m_frame->settings() && m_frame->settings()->caretBrowsingEnabled())
        return false;

    Element* root = rootEditableElementOf(m_caretPosition.position());
    if (!root)
        return false;

    Element* focusedElement = root->document().focusedElement();
    if (!focusedElement)
        return false;

    return focusedElement->isShadowIncludingInclusiveAncestorOf(m_caretPosition.position().anchorNode());
}

void FrameCaret::caretBlinkTimerFired(Timer<FrameCaret>*)
{
    DCHECK(caretIsVisible());
    if (isCaretBlinkingSuspended() && m_shouldPaintCaret)
        return;
    m_shouldPaintCaret = !m_shouldPaintCaret;
    setCaretRectNeedsUpdate();
}

} // nemaspace blink
