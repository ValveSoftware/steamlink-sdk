/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/shadow/SliderThumbElement.h"

#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/Event.h"
#include "core/events/MouseEvent.h"
#include "core/events/TouchEvent.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/forms/StepRange.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutSliderContainer.h"
#include "core/layout/LayoutSliderThumb.h"
#include "core/layout/LayoutTheme.h"

namespace blink {

using namespace HTMLNames;

inline static bool hasVerticalAppearance(HTMLInputElement* input) {
  DCHECK(input->layoutObject());
  const ComputedStyle& sliderStyle = input->layoutObject()->styleRef();

  return sliderStyle.appearance() == SliderVerticalPart;
}

inline SliderThumbElement::SliderThumbElement(Document& document)
    : HTMLDivElement(document), m_inDragMode(false) {}

SliderThumbElement* SliderThumbElement::create(Document& document) {
  SliderThumbElement* element = new SliderThumbElement(document);
  element->setAttribute(idAttr, ShadowElementNames::sliderThumb());
  return element;
}

void SliderThumbElement::setPositionFromValue() {
  // Since the code to calculate position is in the LayoutSliderThumb layout
  // path, we don't actually update the value here. Instead, we poke at the
  // layoutObject directly to trigger layout.
  if (layoutObject())
    layoutObject()->setNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::SliderValueChanged);
}

LayoutObject* SliderThumbElement::createLayoutObject(const ComputedStyle&) {
  return new LayoutSliderThumb(this);
}

bool SliderThumbElement::isDisabledFormControl() const {
  return hostInput() && hostInput()->isDisabledFormControl();
}

bool SliderThumbElement::matchesReadOnlyPseudoClass() const {
  return hostInput() && hostInput()->matchesReadOnlyPseudoClass();
}

bool SliderThumbElement::matchesReadWritePseudoClass() const {
  return hostInput() && hostInput()->matchesReadWritePseudoClass();
}

Node* SliderThumbElement::focusDelegate() {
  return hostInput();
}

void SliderThumbElement::dragFrom(const LayoutPoint& point) {
  startDragging();
  setPositionFromPoint(point);
}

void SliderThumbElement::setPositionFromPoint(const LayoutPoint& point) {
  HTMLInputElement* input(hostInput());
  Element* trackElement = input->userAgentShadowRoot()->getElementById(
      ShadowElementNames::sliderTrack());

  if (!input->layoutObject() || !layoutBox() || !trackElement->layoutBox())
    return;

  LayoutPoint offset = LayoutPoint(
      input->layoutObject()->absoluteToLocal(FloatPoint(point), UseTransforms));
  bool isVertical = hasVerticalAppearance(input);
  bool isLeftToRightDirection = layoutBox()->style()->isLeftToRightDirection();
  LayoutUnit trackSize;
  LayoutUnit position;
  LayoutUnit currentPosition;
  // We need to calculate currentPosition from absolute points becaue the
  // layoutObject for this node is usually on a layer and layoutBox()->x() and
  // y() are unusable.
  // FIXME: This should probably respect transforms.
  LayoutPoint absoluteThumbOrigin =
      layoutBox()->absoluteBoundingBoxRectIgnoringTransforms().location();
  LayoutPoint absoluteSliderContentOrigin =
      LayoutPoint(input->layoutObject()->localToAbsolute());
  IntRect trackBoundingBox =
      trackElement->layoutObject()->absoluteBoundingBoxRectIgnoringTransforms();
  IntRect inputBoundingBox =
      input->layoutObject()->absoluteBoundingBoxRectIgnoringTransforms();
  if (isVertical) {
    trackSize = trackElement->layoutBox()->contentHeight() -
                layoutBox()->size().height();
    position = offset.y() - layoutBox()->size().height() / 2 -
               trackBoundingBox.y() + inputBoundingBox.y() -
               layoutBox()->marginBottom();
    currentPosition = absoluteThumbOrigin.y() - absoluteSliderContentOrigin.y();
  } else {
    trackSize =
        trackElement->layoutBox()->contentWidth() - layoutBox()->size().width();
    position = offset.x() - layoutBox()->size().width() / 2 -
               trackBoundingBox.x() + inputBoundingBox.x();
    position -= isLeftToRightDirection ? layoutBox()->marginLeft()
                                       : layoutBox()->marginRight();
    currentPosition = absoluteThumbOrigin.x() - absoluteSliderContentOrigin.x();
  }
  position = std::min(position, trackSize).clampNegativeToZero();
  const Decimal ratio =
      Decimal::fromDouble(static_cast<double>(position) / trackSize);
  const Decimal fraction =
      isVertical || !isLeftToRightDirection ? Decimal(1) - ratio : ratio;
  StepRange stepRange(input->createStepRange(RejectAny));
  Decimal value = stepRange.clampValue(stepRange.valueFromProportion(fraction));

  Decimal closest = input->findClosestTickMarkValue(value);
  if (closest.isFinite()) {
    double closestFraction = stepRange.proportionFromValue(closest).toDouble();
    double closestRatio = isVertical || !isLeftToRightDirection
                              ? 1.0 - closestFraction
                              : closestFraction;
    LayoutUnit closestPosition(trackSize * closestRatio);
    const LayoutUnit snappingThreshold(5);
    if ((closestPosition - position).abs() <= snappingThreshold)
      value = closest;
  }

  String valueString = serializeForNumberType(value);
  if (valueString == input->value())
    return;

  // FIXME: This is no longer being set from renderer. Consider updating the
  // method name.
  input->setValueFromRenderer(valueString);
  if (layoutObject())
    layoutObject()->setNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::SliderValueChanged);
}

void SliderThumbElement::startDragging() {
  if (LocalFrame* frame = document().frame()) {
    frame->eventHandler().setCapturingMouseEventsNode(this);
    m_inDragMode = true;
  }
}

void SliderThumbElement::stopDragging() {
  if (!m_inDragMode)
    return;

  if (LocalFrame* frame = document().frame())
    frame->eventHandler().setCapturingMouseEventsNode(nullptr);
  m_inDragMode = false;
  if (layoutObject())
    layoutObject()->setNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::SliderValueChanged);
  if (hostInput())
    hostInput()->dispatchFormControlChangeEvent();
}

void SliderThumbElement::defaultEventHandler(Event* event) {
  if (!event->isMouseEvent()) {
    HTMLDivElement::defaultEventHandler(event);
    return;
  }

  // FIXME: Should handle this readonly/disabled check in more general way.
  // Missing this kind of check is likely to occur elsewhere if adding it in
  // each shadow element.
  HTMLInputElement* input = hostInput();
  if (!input || input->isDisabledOrReadOnly()) {
    stopDragging();
    HTMLDivElement::defaultEventHandler(event);
    return;
  }

  MouseEvent* mouseEvent = toMouseEvent(event);
  bool isLeftButton = mouseEvent->button() ==
                      static_cast<short>(WebPointerProperties::Button::Left);
  const AtomicString& eventType = event->type();

  // We intentionally do not call event->setDefaultHandled() here because
  // MediaControlTimelineElement::defaultEventHandler() wants to handle these
  // mouse events.
  if (eventType == EventTypeNames::mousedown && isLeftButton) {
    startDragging();
    return;
  }
  if (eventType == EventTypeNames::mouseup && isLeftButton) {
    stopDragging();
    return;
  }
  if (eventType == EventTypeNames::mousemove) {
    if (m_inDragMode)
      setPositionFromPoint(LayoutPoint(mouseEvent->absoluteLocation()));
    return;
  }

  HTMLDivElement::defaultEventHandler(event);
}

bool SliderThumbElement::willRespondToMouseMoveEvents() {
  const HTMLInputElement* input = hostInput();
  if (input && !input->isDisabledOrReadOnly() && m_inDragMode)
    return true;

  return HTMLDivElement::willRespondToMouseMoveEvents();
}

bool SliderThumbElement::willRespondToMouseClickEvents() {
  const HTMLInputElement* input = hostInput();
  if (input && !input->isDisabledOrReadOnly())
    return true;

  return HTMLDivElement::willRespondToMouseClickEvents();
}

void SliderThumbElement::detachLayoutTree(const AttachContext& context) {
  if (m_inDragMode) {
    if (LocalFrame* frame = document().frame())
      frame->eventHandler().setCapturingMouseEventsNode(nullptr);
  }
  HTMLDivElement::detachLayoutTree(context);
}

HTMLInputElement* SliderThumbElement::hostInput() const {
  // Only HTMLInputElement creates SliderThumbElement instances as its shadow
  // nodes.  So, ownerShadowHost() must be an HTMLInputElement.
  return toHTMLInputElement(ownerShadowHost());
}

static const AtomicString& sliderThumbShadowPartId() {
  DEFINE_STATIC_LOCAL(const AtomicString, sliderThumb,
                      ("-webkit-slider-thumb"));
  return sliderThumb;
}

static const AtomicString& mediaSliderThumbShadowPartId() {
  DEFINE_STATIC_LOCAL(const AtomicString, mediaSliderThumb,
                      ("-webkit-media-slider-thumb"));
  return mediaSliderThumb;
}

const AtomicString& SliderThumbElement::shadowPseudoId() const {
  HTMLInputElement* input = hostInput();
  if (!input || !input->layoutObject())
    return sliderThumbShadowPartId();

  const ComputedStyle& sliderStyle = input->layoutObject()->styleRef();
  switch (sliderStyle.appearance()) {
    case MediaSliderPart:
    case MediaSliderThumbPart:
    case MediaVolumeSliderPart:
    case MediaVolumeSliderThumbPart:
    case MediaFullscreenVolumeSliderPart:
    case MediaFullscreenVolumeSliderThumbPart:
      return mediaSliderThumbShadowPartId();
    default:
      return sliderThumbShadowPartId();
  }
}

// --------------------------------

inline SliderContainerElement::SliderContainerElement(Document& document)
    : HTMLDivElement(document),
      m_hasTouchEventHandler(false),
      m_touchStarted(false),
      m_slidingDirection(NoMove) {
  updateTouchEventHandlerRegistry();
}

DEFINE_NODE_FACTORY(SliderContainerElement)

HTMLInputElement* SliderContainerElement::hostInput() const {
  return toHTMLInputElement(ownerShadowHost());
}

LayoutObject* SliderContainerElement::createLayoutObject(const ComputedStyle&) {
  return new LayoutSliderContainer(this);
}

void SliderContainerElement::defaultEventHandler(Event* event) {
  if (event->isTouchEvent()) {
    handleTouchEvent(toTouchEvent(event));
    return;
  }
}

void SliderContainerElement::handleTouchEvent(TouchEvent* event) {
  HTMLInputElement* input = hostInput();
  if (input->isDisabledOrReadOnly())
    return;

  if (event->type() == EventTypeNames::touchend) {
    input->dispatchFormControlChangeEvent();
    event->setDefaultHandled();
    m_slidingDirection = NoMove;
    m_touchStarted = false;
    return;
  }

  // The direction of this series of touch actions has been determined, which is
  // perpendicular to the slider, so no need to adjust the value.
  if (!canSlide()) {
    return;
  }

  TouchList* touches = event->targetTouches();
  SliderThumbElement* thumb = toSliderThumbElement(
      treeScope().getElementById(ShadowElementNames::sliderThumb()));
  if (touches->length() == 1) {
    if (event->type() == EventTypeNames::touchstart) {
      m_startPoint = touches->item(0)->absoluteLocation();
      m_slidingDirection = NoMove;
      m_touchStarted = true;
      thumb->setPositionFromPoint(touches->item(0)->absoluteLocation());
    } else if (m_touchStarted) {
      LayoutPoint currentPoint = touches->item(0)->absoluteLocation();
      if (m_slidingDirection ==
          NoMove) {  // Still needs to update the direction.
        m_slidingDirection = getDirection(currentPoint, m_startPoint);
      }

      // m_slidingDirection has been updated, so check whether it's okay to
      // slide again.
      if (canSlide()) {
        thumb->setPositionFromPoint(touches->item(0)->absoluteLocation());
        event->setDefaultHandled();
      }
    }
  }
}

SliderContainerElement::Direction SliderContainerElement::getDirection(
    LayoutPoint& point1,
    LayoutPoint& point2) {
  if (point1 == point2) {
    return NoMove;
  }
  if ((point1.x() - point2.x()).abs() >= (point1.y() - point2.y()).abs()) {
    return Horizontal;
  }
  return Vertical;
}

bool SliderContainerElement::canSlide() {
  if (!hostInput() || !hostInput()->layoutObject() ||
      !hostInput()->layoutObject()->style()) {
    return false;
  }
  const ComputedStyle* sliderStyle = hostInput()->layoutObject()->style();
  const TransformOperations& transforms = sliderStyle->transform();
  int transformSize = transforms.size();
  if (transformSize > 0) {
    for (int i = 0; i < transformSize; ++i) {
      if (transforms.at(i)->type() == TransformOperation::Rotate) {
        return true;
      }
    }
  }
  if ((m_slidingDirection == Vertical &&
       sliderStyle->appearance() == SliderHorizontalPart) ||
      (m_slidingDirection == Horizontal &&
       sliderStyle->appearance() == SliderVerticalPart)) {
    return false;
  }
  return true;
}

const AtomicString& SliderContainerElement::shadowPseudoId() const {
  DEFINE_STATIC_LOCAL(const AtomicString, mediaSliderContainer,
                      ("-webkit-media-slider-container"));
  DEFINE_STATIC_LOCAL(const AtomicString, sliderContainer,
                      ("-webkit-slider-container"));

  if (!ownerShadowHost() || !ownerShadowHost()->layoutObject())
    return sliderContainer;

  const ComputedStyle& sliderStyle =
      ownerShadowHost()->layoutObject()->styleRef();
  switch (sliderStyle.appearance()) {
    case MediaSliderPart:
    case MediaSliderThumbPart:
    case MediaVolumeSliderPart:
    case MediaVolumeSliderThumbPart:
    case MediaFullscreenVolumeSliderPart:
    case MediaFullscreenVolumeSliderThumbPart:
      return mediaSliderContainer;
    default:
      return sliderContainer;
  }
}

void SliderContainerElement::updateTouchEventHandlerRegistry() {
  if (m_hasTouchEventHandler) {
    return;
  }
  if (document().frameHost() &&
      document().lifecycle().state() < DocumentLifecycle::Stopping) {
    EventHandlerRegistry& registry =
        document().frameHost()->eventHandlerRegistry();
    registry.didAddEventHandler(
        *this, EventHandlerRegistry::TouchStartOrMoveEventPassive);
    m_hasTouchEventHandler = true;
  }
}

void SliderContainerElement::didMoveToNewDocument(Document& oldDocument) {
  updateTouchEventHandlerRegistry();
  HTMLElement::didMoveToNewDocument(oldDocument);
}

void SliderContainerElement::removeAllEventListeners() {
  Node::removeAllEventListeners();
  m_hasTouchEventHandler = false;
}

}  // namespace blink
