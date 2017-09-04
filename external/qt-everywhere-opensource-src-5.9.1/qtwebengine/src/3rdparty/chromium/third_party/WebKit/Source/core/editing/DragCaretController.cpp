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
#include "core/frame/Settings.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/paint/PaintLayer.h"

namespace blink {

DragCaretController::DragCaretController() : m_caretBase(new CaretBase()) {}

DragCaretController* DragCaretController::create() {
  return new DragCaretController;
}

bool DragCaretController::hasCaretIn(const LayoutBlock& layoutBlock) const {
  Node* node = m_position.anchorNode();
  if (!node)
    return false;
  if (layoutBlock != CaretBase::caretLayoutObject(node))
    return false;
  return rootEditableElementOf(m_position.position());
}

bool DragCaretController::isContentRichlyEditable() const {
  return isRichlyEditablePosition(m_position.position());
}

void DragCaretController::setCaretPosition(
    const PositionWithAffinity& position) {
  // for querying Layer::compositingState()
  // This code is probably correct, since it doesn't occur in a stack that
  // involves updating compositing state.
  DisableCompositingQueryAsserts disabler;

  if (Node* node = m_position.anchorNode())
    m_caretBase->invalidateCaretRect(node);
  m_position = createVisiblePosition(position).toPositionWithAffinity();
  Document* document = nullptr;
  if (Node* node = m_position.anchorNode()) {
    m_caretBase->invalidateCaretRect(node);
    document = &node->document();
  }
  if (m_position.isNull()) {
    m_caretBase->clearCaretRect();
  } else {
    DCHECK(!m_position.isOrphan());
    document->updateStyleAndLayoutTree();
    m_caretBase->updateCaretRect(m_position);
  }
}

void DragCaretController::nodeChildrenWillBeRemoved(ContainerNode& container) {
  if (!hasCaret() || !container.inActiveDocument())
    return;
  Node* const anchorNode = m_position.position().anchorNode();
  if (!anchorNode || anchorNode == container)
    return;
  if (!container.isShadowIncludingInclusiveAncestorOf(anchorNode))
    return;
  m_position.document()->layoutViewItem().clearSelection();
  clear();
}

void DragCaretController::nodeWillBeRemoved(Node& node) {
  if (!hasCaret() || !node.inActiveDocument())
    return;
  Node* const anchorNode = m_position.position().anchorNode();
  if (!anchorNode)
    return;
  if (!node.isShadowIncludingInclusiveAncestorOf(anchorNode))
    return;
  m_position.document()->layoutViewItem().clearSelection();
  clear();
}

DEFINE_TRACE(DragCaretController) {
  visitor->trace(m_position);
  visitor->trace(m_caretBase);
}

void DragCaretController::paintDragCaret(LocalFrame* frame,
                                         GraphicsContext& context,
                                         const LayoutPoint& paintOffset) const {
  if (m_position.anchorNode()->document().frame() == frame) {
    m_caretBase->paintCaret(m_position.anchorNode(), context, paintOffset,
                            DisplayItem::kDragCaret);
  }
}

}  // namespace blink
