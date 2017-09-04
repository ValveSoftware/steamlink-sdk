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
#include "core/editing/SelectionEditor.h"
#include "core/editing/commands/CompositeEditCommand.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/TextControlElement.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/page/Page.h"
#include "core/paint/PaintLayer.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

FrameCaret::FrameCaret(LocalFrame* frame,
                       const SelectionEditor& selectionEditor)
    : m_selectionEditor(&selectionEditor),
      m_frame(frame),
      m_caretVisibility(CaretVisibility::Hidden),
      m_previousCaretVisibility(CaretVisibility::Hidden),
      m_caretBlinkTimer(this, &FrameCaret::caretBlinkTimerFired),
      m_caretRectDirty(true),
      m_shouldPaintCaret(true),
      m_isCaretBlinkingSuspended(false),
      m_shouldShowBlockCursor(false) {
  DCHECK(frame);
}

FrameCaret::~FrameCaret() = default;

DEFINE_TRACE(FrameCaret) {
  visitor->trace(m_selectionEditor);
  visitor->trace(m_frame);
  visitor->trace(m_previousCaretNode);
  visitor->trace(m_previousCaretAnchorNode);
  CaretBase::trace(visitor);
}

const PositionWithAffinity FrameCaret::caretPosition() const {
  const VisibleSelection& selection =
      m_selectionEditor->visibleSelection<EditingStrategy>();
  if (!selection.isCaret())
    return PositionWithAffinity();
  return PositionWithAffinity(selection.start(), selection.affinity());
}

inline static bool shouldStopBlinkingDueToTypingCommand(LocalFrame* frame) {
  return frame->editor().lastEditCommand() &&
         frame->editor().lastEditCommand()->shouldStopCaretBlinking();
}

void FrameCaret::updateAppearance() {
  // Paint a block cursor instead of a caret in overtype mode unless the caret
  // is at the end of a line (in this case the FrameSelection will paint a
  // blinking caret as usual).
  bool paintBlockCursor = m_shouldShowBlockCursor && isActive();
  if (paintBlockCursor) {
    // TODO(editing-dev): Use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited.  see http://crbug.com/590369 for more details.
    // In the long term, we should defer the update of the caret's appearance
    // to prevent synchronous layout.
    m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

    if (isLogicalEndOfLine(createVisiblePosition(caretPosition())))
      paintBlockCursor = false;
  }

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

void FrameCaret::stopCaretBlinkTimer() {
  if (m_caretBlinkTimer.isActive() || m_shouldPaintCaret)
    setCaretRectNeedsUpdate();
  m_shouldPaintCaret = false;
  m_caretBlinkTimer.stop();
}

void FrameCaret::startBlinkCaret() {
  // Start blinking with a black caret. Be sure not to restart if we're
  // already blinking in the right location.
  if (m_caretBlinkTimer.isActive())
    return;

  if (double blinkInterval = LayoutTheme::theme().caretBlinkInterval())
    m_caretBlinkTimer.startRepeating(blinkInterval, BLINK_FROM_HERE);

  m_shouldPaintCaret = true;
  setCaretRectNeedsUpdate();
}

void FrameCaret::setCaretVisibility(CaretVisibility visibility) {
  if (m_caretVisibility == visibility)
    return;

  m_caretVisibility = visibility;

  updateAppearance();
}

void FrameCaret::setCaretRectNeedsUpdate() {
  if (m_caretRectDirty)
    return;
  m_caretRectDirty = true;

  if (Page* page = m_frame->page())
    page->animator().scheduleVisualUpdate(m_frame->localFrameRoot());
}

bool FrameCaret::caretPositionIsValidForDocument(
    const Document& document) const {
  if (!isActive())
    return true;

  return caretPosition().document() == document && !caretPosition().isOrphan();
}

void FrameCaret::invalidateCaretRect(bool forceInvalidation) {
  if (!m_caretRectDirty)
    return;
  m_caretRectDirty = false;

  DCHECK(caretPositionIsValidForDocument(*m_frame->document()));
  LayoutObject* layoutObject = nullptr;
  LayoutRect newRect;
  PositionWithAffinity currentCaretPosition = caretPosition();
  if (isActive())
    newRect = localCaretRectOfPosition(currentCaretPosition, layoutObject);
  Node* newNode = layoutObject ? layoutObject->node() : nullptr;
  // The current selected node |newNode| could be a child multiple levels below
  // its associated "anchor node" ancestor, so we reference and keep around the
  // anchor node for checking editability.
  // TODO(wkorman): Consider storing previous Position, rather than Node, and
  // making use of EditingUtilies::isEditablePosition() directly.
  Node* newAnchorNode =
      currentCaretPosition.position().parentAnchoredEquivalent().anchorNode();
  if (newNode && newAnchorNode && newNode != newAnchorNode &&
      newAnchorNode->layoutObject() && newAnchorNode->layoutObject()->isBox()) {
    newNode->layoutObject()->mapToVisualRectInAncestorSpace(
        toLayoutBoxModelObject(newAnchorNode->layoutObject()), newRect);
  }
  // It's possible for the timer to be inactive even though we want to
  // invalidate the caret. For example, when running as a layout test the
  // caret blink interval could be zero and thus |m_caretBlinkTimer| will
  // never be started. We provide |forceInvalidation| for use by paint
  // invalidation internals where we need to invalidate the caret regardless
  // of timer state.
  if (!forceInvalidation && !m_caretBlinkTimer.isActive() &&
      newNode == m_previousCaretNode && newRect == m_previousCaretRect &&
      m_caretVisibility == m_previousCaretVisibility)
    return;

  if (m_previousCaretNode && shouldRepaintCaret(*m_previousCaretAnchorNode)) {
    invalidateLocalCaretRect(m_previousCaretAnchorNode.get(),
                             m_previousCaretRect);
  }
  if (newNode && shouldRepaintCaret(*newAnchorNode))
    invalidateLocalCaretRect(newAnchorNode, newRect);
  m_previousCaretNode = newNode;
  m_previousCaretAnchorNode = newAnchorNode;
  m_previousCaretRect = newRect;
  m_previousCaretVisibility = m_caretVisibility;
}

IntRect FrameCaret::absoluteCaretBounds() {
  DCHECK_NE(m_frame->document()->lifecycle().state(),
            DocumentLifecycle::InPaintInvalidation);
  DCHECK(!m_frame->document()->needsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      m_frame->document()->lifecycle());

  if (!isActive()) {
    clearCaretRect();
  } else {
    if (enclosingTextControl(caretPosition().position())) {
      if (isVisuallyEquivalentCandidate(caretPosition().position()))
        updateCaretRect(caretPosition());
      else
        updateCaretRect(createVisiblePosition(caretPosition()));
    } else {
      updateCaretRect(createVisiblePosition(caretPosition()));
    }
  }
  return absoluteBoundsForLocalRect(caretPosition().anchorNode(),
                                    localCaretRectWithoutUpdate());
}

void FrameCaret::setShouldShowBlockCursor(bool shouldShowBlockCursor) {
  m_shouldShowBlockCursor = shouldShowBlockCursor;

  m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  updateAppearance();
}

void FrameCaret::paintCaret(GraphicsContext& context,
                            const LayoutPoint& paintOffset) {
  if (m_caretVisibility == CaretVisibility::Hidden)
    return;

  if (!(isActive() && m_shouldPaintCaret))
    return;

  updateCaretRect(caretPosition());
  CaretBase::paintCaret(caretPosition().anchorNode(), context, paintOffset,
                        DisplayItem::kCaret);
}

void FrameCaret::dataWillChange(const CharacterData& node) {
  if (node == m_previousCaretNode) {
    // This invalidation is eager, and intentionally uses stale state.
    DisableCompositingQueryAsserts disabler;
    invalidateLocalCaretRect(m_previousCaretAnchorNode.get(),
                             m_previousCaretRect);
  }
}

void FrameCaret::nodeWillBeRemoved(Node& node) {
  if (node != m_previousCaretNode && node != m_previousCaretAnchorNode)
    return;
  // Hits in ManualTests/caret-paint-after-last-text-is-removed.html
  DisableCompositingQueryAsserts disabler;
  invalidateLocalCaretRect(m_previousCaretAnchorNode.get(),
                           m_previousCaretRect);
  m_previousCaretNode = nullptr;
  m_previousCaretAnchorNode = nullptr;
  m_previousCaretRect = LayoutRect();
  m_previousCaretVisibility = CaretVisibility::Hidden;
}

void FrameCaret::documentDetached() {
  m_caretBlinkTimer.stop();
  m_previousCaretNode.clear();
  m_previousCaretAnchorNode.clear();
}

bool FrameCaret::shouldBlinkCaret() const {
  if (m_caretVisibility != CaretVisibility::Visible || !isActive())
    return false;

  Element* root = rootEditableElementOf(caretPosition().position());
  if (!root)
    return false;

  Element* focusedElement = root->document().focusedElement();
  if (!focusedElement)
    return false;

  return focusedElement->isShadowIncludingInclusiveAncestorOf(
      caretPosition().anchorNode());
}

void FrameCaret::caretBlinkTimerFired(TimerBase*) {
  DCHECK_EQ(m_caretVisibility, CaretVisibility::Visible);
  if (isCaretBlinkingSuspended() && m_shouldPaintCaret)
    return;
  m_shouldPaintCaret = !m_shouldPaintCaret;
  setCaretRectNeedsUpdate();
}

}  // namespace blink
