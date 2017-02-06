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

#include "core/editing/DragCaretController.h"

#include "core/editing/EditingUtilities.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/paint/PaintLayer.h"

namespace blink {

DragCaretController::DragCaretController()
    : m_caretBase(new CaretBase(CaretVisibility::Visible))
{
}

DragCaretController* DragCaretController::create()
{
    return new DragCaretController;
}

bool DragCaretController::isContentRichlyEditable() const
{
    return isRichlyEditablePosition(m_position.deepEquivalent());
}

void DragCaretController::setCaretPosition(const PositionWithAffinity& position)
{
    // for querying Layer::compositingState()
    // This code is probably correct, since it doesn't occur in a stack that
    // involves updating compositing state.
    DisableCompositingQueryAsserts disabler;

    if (Node* node = m_position.deepEquivalent().anchorNode())
        m_caretBase->invalidateCaretRect(node);
    m_position = createVisiblePosition(position);
    Document* document = nullptr;
    if (Node* node = m_position.deepEquivalent().anchorNode()) {
        m_caretBase->invalidateCaretRect(node);
        document = &node->document();
    }
    if (m_position.isNull() || m_position.isOrphan()) {
        m_caretBase->clearCaretRect();
    } else {
        document->updateStyleAndLayoutTree();
        m_caretBase->updateCaretRect(m_position);
    }
}

static bool removingNodeRemovesPosition(Node& node, const Position& position)
{
    if (!position.anchorNode())
        return false;

    if (position.anchorNode() == node)
        return true;

    if (!node.isElementNode())
        return false;

    Element& element = toElement(node);
    return element.isShadowIncludingInclusiveAncestorOf(position.anchorNode());
}

void DragCaretController::nodeWillBeRemoved(Node& node)
{
    if (!hasCaret() || !node.inActiveDocument())
        return;

    if (!removingNodeRemovesPosition(node, m_position.deepEquivalent()))
        return;

    m_position.deepEquivalent().document()->layoutViewItem().clearSelection();
    clear();
}

DEFINE_TRACE(DragCaretController)
{
    visitor->trace(m_position);
    visitor->trace(m_caretBase);
}

LayoutBlock* DragCaretController::caretLayoutObject() const
{
    return CaretBase::caretLayoutObject(m_position.deepEquivalent().anchorNode());
}

bool DragCaretController::isContentEditable() const
{
    return rootEditableElementOf(m_position);
}

void DragCaretController::paintDragCaret(LocalFrame* frame, GraphicsContext& context, const LayoutPoint& paintOffset) const
{
    if (m_position.deepEquivalent().anchorNode()->document().frame() == frame)
        m_caretBase->paintCaret(m_position.deepEquivalent().anchorNode(), context, paintOffset, DisplayItem::DragCaret);
}

} // namespace blink
