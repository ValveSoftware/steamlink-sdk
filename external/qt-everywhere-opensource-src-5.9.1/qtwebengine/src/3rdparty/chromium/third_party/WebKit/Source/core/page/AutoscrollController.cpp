/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2012 Digia Plc. and/or its subsidiary(-ies)
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

#include "core/page/AutoscrollController.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutListBox.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "wtf/CurrentTime.h"

namespace blink {

// Delay time in second for start autoscroll if pointer is in border edge of
// scrollable element.
static double autoscrollDelay = 0.2;

AutoscrollController* AutoscrollController::create(Page& page) {
  return new AutoscrollController(page);
}

AutoscrollController::AutoscrollController(Page& page)
    : m_page(&page),
      m_autoscrollLayoutObject(nullptr),
      m_pressedLayoutObject(nullptr),
      m_autoscrollType(NoAutoscroll),
      m_dragAndDropAutoscrollStartTime(0),
      m_didLatchForMiddleClickAutoscroll(false) {}

DEFINE_TRACE(AutoscrollController) {
  visitor->trace(m_page);
}

bool AutoscrollController::autoscrollInProgress() const {
  return m_autoscrollType == AutoscrollForSelection;
}

bool AutoscrollController::autoscrollInProgress(
    const LayoutBox* layoutObject) const {
  return m_autoscrollLayoutObject == layoutObject;
}

void AutoscrollController::startAutoscrollForSelection(
    LayoutObject* layoutObject) {
  // We don't want to trigger the autoscroll or the middleClickAutoscroll if
  // it's already active.
  if (m_autoscrollType != NoAutoscroll)
    return;
  LayoutBox* scrollable = LayoutBox::findAutoscrollable(layoutObject);
  if (!scrollable)
    scrollable =
        layoutObject->isListBox() ? toLayoutListBox(layoutObject) : nullptr;
  if (!scrollable)
    return;

  m_pressedLayoutObject = layoutObject && layoutObject->isBox()
                              ? toLayoutBox(layoutObject)
                              : nullptr;
  m_autoscrollType = AutoscrollForSelection;
  m_autoscrollLayoutObject = scrollable;
  startAutoscroll();
}

void AutoscrollController::stopAutoscroll() {
  if (m_pressedLayoutObject) {
    m_pressedLayoutObject->stopAutoscroll();
    m_pressedLayoutObject = nullptr;
  }
  LayoutBox* scrollable = m_autoscrollLayoutObject;
  m_autoscrollLayoutObject = nullptr;

  if (!scrollable)
    return;

  if (RuntimeEnabledFeatures::middleClickAutoscrollEnabled() &&
      middleClickAutoscrollInProgress()) {
    if (FrameView* view = scrollable->frame()->view()) {
      view->setCursor(pointerCursor());
    }
  }
  m_autoscrollType = NoAutoscroll;
}

void AutoscrollController::stopAutoscrollIfNeeded(LayoutObject* layoutObject) {
  if (m_pressedLayoutObject == layoutObject)
    m_pressedLayoutObject = nullptr;

  if (m_autoscrollLayoutObject != layoutObject)
    return;
  m_autoscrollLayoutObject = nullptr;
  m_autoscrollType = NoAutoscroll;
}

void AutoscrollController::updateAutoscrollLayoutObject() {
  if (!m_autoscrollLayoutObject)
    return;

  LayoutObject* layoutObject = m_autoscrollLayoutObject;

  if (RuntimeEnabledFeatures::middleClickAutoscrollEnabled()) {
    HitTestResult hitTest =
        layoutObject->frame()->eventHandler().hitTestResultAtPoint(
            m_middleClickAutoscrollStartPos,
            HitTestRequest::ReadOnly | HitTestRequest::Active);

    if (Node* nodeAtPoint = hitTest.innerNode())
      layoutObject = nodeAtPoint->layoutObject();
  }

  while (layoutObject &&
         !(layoutObject->isBox() && toLayoutBox(layoutObject)->canAutoscroll()))
    layoutObject = layoutObject->parent();

  m_autoscrollLayoutObject = layoutObject && layoutObject->isBox()
                                 ? toLayoutBox(layoutObject)
                                 : nullptr;

  if (m_autoscrollType != NoAutoscroll && !m_autoscrollLayoutObject)
    m_autoscrollType = NoAutoscroll;
}

void AutoscrollController::updateDragAndDrop(Node* dropTargetNode,
                                             const IntPoint& eventPosition,
                                             double eventTime) {
  if (!dropTargetNode || !dropTargetNode->layoutObject()) {
    stopAutoscroll();
    return;
  }

  if (m_autoscrollLayoutObject &&
      m_autoscrollLayoutObject->frame() !=
          dropTargetNode->layoutObject()->frame())
    return;

  LayoutBox* scrollable =
      LayoutBox::findAutoscrollable(dropTargetNode->layoutObject());
  if (!scrollable) {
    stopAutoscroll();
    return;
  }

  Page* page = scrollable->frame() ? scrollable->frame()->page() : nullptr;
  if (!page) {
    stopAutoscroll();
    return;
  }

  IntSize offset = scrollable->calculateAutoscrollDirection(eventPosition);
  if (offset.isZero()) {
    stopAutoscroll();
    return;
  }

  m_dragAndDropAutoscrollReferencePosition = eventPosition + offset;

  if (m_autoscrollType == NoAutoscroll) {
    m_autoscrollType = AutoscrollForDragAndDrop;
    m_autoscrollLayoutObject = scrollable;
    m_dragAndDropAutoscrollStartTime = eventTime;
    UseCounter::count(m_page->mainFrame(), UseCounter::DragAndDropScrollStart);
    startAutoscroll();
  } else if (m_autoscrollLayoutObject != scrollable) {
    m_dragAndDropAutoscrollStartTime = eventTime;
    m_autoscrollLayoutObject = scrollable;
  }
}

void AutoscrollController::handleMouseReleaseForMiddleClickAutoscroll(
    LocalFrame* frame,
    const PlatformMouseEvent& mouseEvent) {
  DCHECK(RuntimeEnabledFeatures::middleClickAutoscrollEnabled());
  if (!frame->isMainFrame())
    return;
  switch (m_autoscrollType) {
    case AutoscrollForMiddleClick:
      if (mouseEvent.pointerProperties().button ==
          WebPointerProperties::Button::Middle)
        m_autoscrollType = AutoscrollForMiddleClickCanStop;
      break;
    case AutoscrollForMiddleClickCanStop:
      stopAutoscroll();
      break;
    case AutoscrollForDragAndDrop:
    case AutoscrollForSelection:
    case NoAutoscroll:
      // Nothing to do.
      break;
  }
}

bool AutoscrollController::middleClickAutoscrollInProgress() const {
  return m_autoscrollType == AutoscrollForMiddleClickCanStop ||
         m_autoscrollType == AutoscrollForMiddleClick;
}

void AutoscrollController::startMiddleClickAutoscroll(
    LayoutBox* scrollable,
    const IntPoint& lastKnownMousePosition) {
  DCHECK(RuntimeEnabledFeatures::middleClickAutoscrollEnabled());
  // We don't want to trigger the autoscroll or the middleClickAutoscroll if
  // it's already active.
  if (m_autoscrollType != NoAutoscroll)
    return;

  m_autoscrollType = AutoscrollForMiddleClick;
  m_autoscrollLayoutObject = scrollable;
  m_middleClickAutoscrollStartPos = lastKnownMousePosition;
  m_didLatchForMiddleClickAutoscroll = false;

  UseCounter::count(m_page->mainFrame(),
                    UseCounter::MiddleClickAutoscrollStart);
  startAutoscroll();
}

static inline int adjustedScrollDelta(int beginningDelta) {
  // This implemention matches Firefox's.
  // http://mxr.mozilla.org/firefox/source/toolkit/content/widgets/browser.xml#856.
  const int speedReducer = 12;

  int adjustedDelta = beginningDelta / speedReducer;
  if (adjustedDelta > 1) {
    adjustedDelta = static_cast<int>(adjustedDelta *
                                     sqrt(static_cast<double>(adjustedDelta))) -
                    1;
  } else if (adjustedDelta < -1) {
    adjustedDelta =
        static_cast<int>(adjustedDelta *
                         sqrt(static_cast<double>(-adjustedDelta))) +
        1;
  }

  return adjustedDelta;
}

static inline IntSize adjustedScrollDelta(const IntSize& delta) {
  return IntSize(adjustedScrollDelta(delta.width()),
                 adjustedScrollDelta(delta.height()));
}

FloatSize AutoscrollController::calculateAutoscrollDelta() {
  LocalFrame* frame = m_autoscrollLayoutObject->frame();
  if (!frame)
    return FloatSize();

  IntPoint lastKnownMousePosition =
      frame->eventHandler().lastKnownMousePosition();

  // We need to check if the last known mouse position is out of the window.
  // When the mouse is out of the window, the position is incoherent
  static IntPoint previousMousePosition;
  if (lastKnownMousePosition.x() < 0 || lastKnownMousePosition.y() < 0)
    lastKnownMousePosition = previousMousePosition;
  else
    previousMousePosition = lastKnownMousePosition;

  IntSize delta = lastKnownMousePosition - m_middleClickAutoscrollStartPos;

  // at the center we let the space for the icon.
  if (abs(delta.width()) <= noMiddleClickAutoscrollRadius)
    delta.setWidth(0);
  if (abs(delta.height()) <= noMiddleClickAutoscrollRadius)
    delta.setHeight(0);
  return FloatSize(adjustedScrollDelta(delta));
}

void AutoscrollController::animate(double) {
  if (!m_autoscrollLayoutObject || !m_autoscrollLayoutObject->frame()) {
    stopAutoscroll();
    return;
  }

  EventHandler& eventHandler =
      m_autoscrollLayoutObject->frame()->eventHandler();
  IntSize offset = m_autoscrollLayoutObject->calculateAutoscrollDirection(
      eventHandler.lastKnownMousePosition());
  IntPoint selectionPoint = eventHandler.lastKnownMousePosition() + offset;
  switch (m_autoscrollType) {
    case AutoscrollForDragAndDrop:
      if (WTF::monotonicallyIncreasingTime() -
              m_dragAndDropAutoscrollStartTime >
          autoscrollDelay)
        m_autoscrollLayoutObject->autoscroll(
            m_dragAndDropAutoscrollReferencePosition);
      break;
    case AutoscrollForSelection:
      if (!eventHandler.mousePressed()) {
        stopAutoscroll();
        return;
      }
      eventHandler.updateSelectionForMouseDrag();
      m_autoscrollLayoutObject->autoscroll(selectionPoint);
      break;
    case NoAutoscroll:
      break;
    case AutoscrollForMiddleClickCanStop:
    case AutoscrollForMiddleClick:
      DCHECK(RuntimeEnabledFeatures::middleClickAutoscrollEnabled());
      if (!middleClickAutoscrollInProgress()) {
        stopAutoscroll();
        return;
      }
      if (FrameView* view = m_autoscrollLayoutObject->frame()->view())
        updateMiddleClickAutoscrollState(view,
                                         eventHandler.lastKnownMousePosition());
      FloatSize delta = calculateAutoscrollDelta();
      if (delta.isZero())
        break;
      ScrollResult result =
          m_autoscrollLayoutObject->scroll(ScrollByPixel, delta);
      LayoutObject* layoutObject = m_autoscrollLayoutObject;
      while (!m_didLatchForMiddleClickAutoscroll && !result.didScroll()) {
        if (layoutObject->node() && layoutObject->node()->isDocumentNode()) {
          Element* owner = toDocument(layoutObject->node())->localOwner();
          layoutObject = owner ? owner->layoutObject() : nullptr;
        } else {
          layoutObject = layoutObject->parent();
        }
        if (!layoutObject) {
          break;
        }
        if (layoutObject && layoutObject->isBox() &&
            toLayoutBox(layoutObject)->canBeScrolledAndHasScrollableArea())
          result = toLayoutBox(layoutObject)->scroll(ScrollByPixel, delta);
      }
      if (result.didScroll()) {
        m_didLatchForMiddleClickAutoscroll = true;
        m_autoscrollLayoutObject = toLayoutBox(layoutObject);
      }
      break;
  }
  if (m_autoscrollType != NoAutoscroll && m_autoscrollLayoutObject)
    m_page->chromeClient().scheduleAnimation(
        m_autoscrollLayoutObject->frame()->view());
}

void AutoscrollController::startAutoscroll() {
  m_page->chromeClient().scheduleAnimation(
      m_autoscrollLayoutObject->frame()->view());
}

void AutoscrollController::updateMiddleClickAutoscrollState(
    FrameView* view,
    const IntPoint& lastKnownMousePosition) {
  DCHECK(RuntimeEnabledFeatures::middleClickAutoscrollEnabled());
  // At the original click location we draw a 4 arrowed icon. Over this icon
  // there won't be any scroll, So don't change the cursor over this area.
  bool east = m_middleClickAutoscrollStartPos.x() <
              (lastKnownMousePosition.x() - noMiddleClickAutoscrollRadius);
  bool west = m_middleClickAutoscrollStartPos.x() >
              (lastKnownMousePosition.x() + noMiddleClickAutoscrollRadius);
  bool north = m_middleClickAutoscrollStartPos.y() >
               (lastKnownMousePosition.y() + noMiddleClickAutoscrollRadius);
  bool south = m_middleClickAutoscrollStartPos.y() <
               (lastKnownMousePosition.y() - noMiddleClickAutoscrollRadius);

  if (m_autoscrollType == AutoscrollForMiddleClick &&
      (east || west || north || south))
    m_autoscrollType = AutoscrollForMiddleClickCanStop;

  if (north) {
    if (east)
      view->setCursor(northEastPanningCursor());
    else if (west)
      view->setCursor(northWestPanningCursor());
    else
      view->setCursor(northPanningCursor());
  } else if (south) {
    if (east)
      view->setCursor(southEastPanningCursor());
    else if (west)
      view->setCursor(southWestPanningCursor());
    else
      view->setCursor(southPanningCursor());
  } else if (east) {
    view->setCursor(eastPanningCursor());
  } else if (west) {
    view->setCursor(westPanningCursor());
  } else {
    view->setCursor(middlePanningCursor());
  }
}

}  // namespace blink
