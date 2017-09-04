// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/PointerEventFactory.h"

#include "core/frame/FrameView.h"
#include "platform/geometry/FloatSize.h"

namespace blink {

namespace {

inline int toInt(WebPointerProperties::PointerType t) {
  return static_cast<int>(t);
}

const char* pointerTypeNameForWebPointPointerType(
    WebPointerProperties::PointerType type) {
  switch (type) {
    case WebPointerProperties::PointerType::Unknown:
      return "";
    case WebPointerProperties::PointerType::Touch:
      return "touch";
    case WebPointerProperties::PointerType::Pen:
    case WebPointerProperties::PointerType::Eraser:
      // TODO(mustaq): Fix when the spec starts supporting hovering erasers.
      return "pen";
    case WebPointerProperties::PointerType::Mouse:
      return "mouse";
  }
  NOTREACHED();
  return "";
}

const AtomicString& pointerEventNameForMouseEventName(
    const AtomicString& mouseEventName) {
#define RETURN_CORRESPONDING_PE_NAME(eventSuffix)             \
  if (mouseEventName == EventTypeNames::mouse##eventSuffix) { \
    return EventTypeNames::pointer##eventSuffix;              \
  }

  RETURN_CORRESPONDING_PE_NAME(down);
  RETURN_CORRESPONDING_PE_NAME(enter);
  RETURN_CORRESPONDING_PE_NAME(leave);
  RETURN_CORRESPONDING_PE_NAME(move);
  RETURN_CORRESPONDING_PE_NAME(out);
  RETURN_CORRESPONDING_PE_NAME(over);
  RETURN_CORRESPONDING_PE_NAME(up);

#undef RETURN_CORRESPONDING_PE_NAME

  NOTREACHED();
  return emptyAtom;
}

unsigned short buttonToButtonsBitfield(WebPointerProperties::Button button) {
#define CASE_BUTTON_TO_BUTTONS(enumLabel)       \
  case WebPointerProperties::Button::enumLabel: \
    return static_cast<unsigned short>(WebPointerProperties::Buttons::enumLabel)

  switch (button) {
    CASE_BUTTON_TO_BUTTONS(NoButton);
    CASE_BUTTON_TO_BUTTONS(Left);
    CASE_BUTTON_TO_BUTTONS(Right);
    CASE_BUTTON_TO_BUTTONS(Middle);
    CASE_BUTTON_TO_BUTTONS(X1);
    CASE_BUTTON_TO_BUTTONS(X2);
    CASE_BUTTON_TO_BUTTONS(Eraser);
  }

#undef CASE_BUTTON_TO_BUTTONS

  NOTREACHED();
  return 0;
}

}  // namespace

const int PointerEventFactory::s_invalidId = 0;

// Mouse id is 1 to behave the same as MS Edge for compatibility reasons.
const int PointerEventFactory::s_mouseId = 1;

float getPointerEventPressure(float force, int buttons) {
  if (std::isnan(force))
    return buttons ? 0.5 : 0;
  return force;
}

void PointerEventFactory::setIdTypeButtons(
    PointerEventInit& pointerEventInit,
    const WebPointerProperties& pointerProperties,
    unsigned buttons) {
  const WebPointerProperties::PointerType pointerType =
      pointerProperties.pointerType;
  const IncomingId incomingId(pointerType, pointerProperties.id);
  int pointerId = addIdAndActiveButtons(incomingId, buttons != 0);

  // Tweak the |buttons| to reflect pen eraser mode only if the pen is in
  // active buttons state w/o even considering the eraser button.
  // TODO(mustaq): Fix when the spec starts supporting hovering erasers.
  if (pointerType == WebPointerProperties::PointerType::Eraser &&
      buttons != 0) {
    buttons |= static_cast<unsigned>(WebPointerProperties::Buttons::Eraser);
    buttons &= ~static_cast<unsigned>(WebPointerProperties::Buttons::Left);
  }
  pointerEventInit.setButtons(buttons);

  pointerEventInit.setPointerId(pointerId);
  pointerEventInit.setPointerType(
      pointerTypeNameForWebPointPointerType(pointerType));
  pointerEventInit.setIsPrimary(isPrimary(pointerId));
}

void PointerEventFactory::setEventSpecificFields(
    PointerEventInit& pointerEventInit,
    const AtomicString& type) {
  pointerEventInit.setBubbles(type != EventTypeNames::pointerenter &&
                              type != EventTypeNames::pointerleave);
  pointerEventInit.setCancelable(type != EventTypeNames::pointerenter &&
                                 type != EventTypeNames::pointerleave &&
                                 type != EventTypeNames::pointercancel &&
                                 type != EventTypeNames::gotpointercapture &&
                                 type != EventTypeNames::lostpointercapture);

  pointerEventInit.setComposed(true);
  pointerEventInit.setDetail(0);
}

PointerEvent* PointerEventFactory::create(const AtomicString& mouseEventName,
                                          const PlatformMouseEvent& mouseEvent,
                                          LocalDOMWindow* view) {
  DCHECK(mouseEventName == EventTypeNames::mousemove ||
         mouseEventName == EventTypeNames::mousedown ||
         mouseEventName == EventTypeNames::mouseup);

  AtomicString pointerEventName =
      pointerEventNameForMouseEventName(mouseEventName);
  unsigned buttons =
      MouseEvent::platformModifiersToButtons(mouseEvent.getModifiers());
  PointerEventInit pointerEventInit;

  setIdTypeButtons(pointerEventInit, mouseEvent.pointerProperties(), buttons);
  setEventSpecificFields(pointerEventInit, pointerEventName);

  pointerEventInit.setScreenX(mouseEvent.globalPosition().x());
  pointerEventInit.setScreenY(mouseEvent.globalPosition().y());

  IntPoint locationInFrameZoomed;
  if (view && view->frame() && view->frame()->view()) {
    LocalFrame* frame = view->frame();
    FrameView* frameView = frame->view();
    IntPoint locationInContents =
        frameView->rootFrameToContents(mouseEvent.position());
    locationInFrameZoomed = frameView->contentsToFrame(locationInContents);
    float scaleFactor = 1 / frame->pageZoomFactor();
    locationInFrameZoomed.scale(scaleFactor, scaleFactor);
  }

  // Set up initial values for coordinates.
  pointerEventInit.setClientX(locationInFrameZoomed.x());
  pointerEventInit.setClientY(locationInFrameZoomed.y());

  if (pointerEventName == EventTypeNames::pointerdown ||
      pointerEventName == EventTypeNames::pointerup) {
    WebPointerProperties::Button button = mouseEvent.pointerProperties().button;
    // TODO(mustaq): Fix when the spec starts supporting hovering erasers.
    if (mouseEvent.pointerProperties().pointerType ==
            WebPointerProperties::PointerType::Eraser &&
        button == WebPointerProperties::Button::Left)
      button = WebPointerProperties::Button::Eraser;
    pointerEventInit.setButton(static_cast<int>(button));
  } else {
    DCHECK(pointerEventName == EventTypeNames::pointermove);
    pointerEventInit.setButton(
        static_cast<int>(WebPointerProperties::Button::NoButton));
  }
  pointerEventInit.setPressure(getPointerEventPressure(
      mouseEvent.pointerProperties().force, pointerEventInit.buttons()));
  pointerEventInit.setTiltX(mouseEvent.pointerProperties().tiltX);
  pointerEventInit.setTiltY(mouseEvent.pointerProperties().tiltY);

  UIEventWithKeyState::setFromPlatformModifiers(pointerEventInit,
                                                mouseEvent.getModifiers());

  // Make sure chorded buttons fire pointermove instead of pointerup/down.
  if ((pointerEventName == EventTypeNames::pointerdown &&
       (buttons &
        ~buttonToButtonsBitfield(mouseEvent.pointerProperties().button)) !=
           0) ||
      (pointerEventName == EventTypeNames::pointerup && buttons != 0))
    pointerEventName = EventTypeNames::pointermove;

  pointerEventInit.setView(view);

  return PointerEvent::create(pointerEventName, pointerEventInit);
}

PointerEvent* PointerEventFactory::create(const AtomicString& type,
                                          const PlatformTouchPoint& touchPoint,
                                          PlatformEvent::Modifiers modifiers,
                                          const FloatSize& pointRadius,
                                          const FloatPoint& clientPoint,
                                          DOMWindow* view) {
  const PlatformTouchPoint::TouchState pointState = touchPoint.state();

  bool pointerReleasedOrCancelled =
      pointState == PlatformTouchPoint::TouchReleased ||
      pointState == PlatformTouchPoint::TouchCancelled;
  bool pointerPressedOrReleased =
      pointState == PlatformTouchPoint::TouchPressed ||
      pointState == PlatformTouchPoint::TouchReleased;

  PointerEventInit pointerEventInit;

  setIdTypeButtons(pointerEventInit, touchPoint.pointerProperties(),
                   pointerReleasedOrCancelled ? 0 : 1);

  pointerEventInit.setWidth(pointRadius.width());
  pointerEventInit.setHeight(pointRadius.height());
  pointerEventInit.setScreenX(touchPoint.screenPos().x());
  pointerEventInit.setScreenY(touchPoint.screenPos().y());
  pointerEventInit.setClientX(clientPoint.x());
  pointerEventInit.setClientY(clientPoint.y());
  pointerEventInit.setButton(static_cast<int>(
      pointerPressedOrReleased ? WebPointerProperties::Button::Left
                               : WebPointerProperties::Button::NoButton));
  pointerEventInit.setPressure(
      getPointerEventPressure(touchPoint.force(), pointerEventInit.buttons()));
  pointerEventInit.setTiltX(touchPoint.pointerProperties().tiltX);
  pointerEventInit.setTiltY(touchPoint.pointerProperties().tiltY);
  pointerEventInit.setView(view);

  UIEventWithKeyState::setFromPlatformModifiers(pointerEventInit, modifiers);

  setEventSpecificFields(pointerEventInit, type);

  return PointerEvent::create(type, pointerEventInit);
}

PointerEvent* PointerEventFactory::createPointerCancelEvent(
    const int pointerId,
    const WebPointerProperties::PointerType pointerType) {
  DCHECK(m_pointerIdMapping.contains(pointerId));
  m_pointerIdMapping.set(
      pointerId,
      PointerAttributes(m_pointerIdMapping.get(pointerId).incomingId, false));

  PointerEventInit pointerEventInit;

  pointerEventInit.setPointerId(pointerId);
  pointerEventInit.setPointerType(
      pointerTypeNameForWebPointPointerType(pointerType));
  pointerEventInit.setIsPrimary(isPrimary(pointerId));

  setEventSpecificFields(pointerEventInit, EventTypeNames::pointercancel);

  return PointerEvent::create(EventTypeNames::pointercancel, pointerEventInit);
}

PointerEvent* PointerEventFactory::createPointerEventFrom(
    PointerEvent* pointerEvent,
    const AtomicString& type,
    EventTarget* relatedTarget) {
  PointerEventInit pointerEventInit;

  pointerEventInit.setPointerId(pointerEvent->pointerId());
  pointerEventInit.setPointerType(pointerEvent->pointerType());
  pointerEventInit.setIsPrimary(pointerEvent->isPrimary());
  pointerEventInit.setWidth(pointerEvent->width());
  pointerEventInit.setHeight(pointerEvent->height());
  pointerEventInit.setScreenX(pointerEvent->screenX());
  pointerEventInit.setScreenY(pointerEvent->screenY());
  pointerEventInit.setClientX(pointerEvent->clientX());
  pointerEventInit.setClientY(pointerEvent->clientY());
  pointerEventInit.setButton(pointerEvent->button());
  pointerEventInit.setButtons(pointerEvent->buttons());
  pointerEventInit.setPressure(pointerEvent->pressure());
  pointerEventInit.setTiltX(pointerEvent->tiltX());
  pointerEventInit.setTiltY(pointerEvent->tiltY());
  pointerEventInit.setView(pointerEvent->view());

  setEventSpecificFields(pointerEventInit, type);

  if (relatedTarget)
    pointerEventInit.setRelatedTarget(relatedTarget);

  return PointerEvent::create(type, pointerEventInit);
}

PointerEvent* PointerEventFactory::createPointerCaptureEvent(
    PointerEvent* pointerEvent,
    const AtomicString& type) {
  DCHECK(type == EventTypeNames::gotpointercapture ||
         type == EventTypeNames::lostpointercapture);

  return createPointerEventFrom(pointerEvent, type,
                                pointerEvent->relatedTarget());
}

PointerEvent* PointerEventFactory::createPointerBoundaryEvent(
    PointerEvent* pointerEvent,
    const AtomicString& type,
    EventTarget* relatedTarget) {
  DCHECK(type == EventTypeNames::pointerout ||
         type == EventTypeNames::pointerleave ||
         type == EventTypeNames::pointerover ||
         type == EventTypeNames::pointerenter);

  return createPointerEventFrom(pointerEvent, type, relatedTarget);
}

PointerEventFactory::PointerEventFactory() {
  clear();
}

PointerEventFactory::~PointerEventFactory() {
  clear();
}

void PointerEventFactory::clear() {
  for (int type = 0;
       type <= toInt(WebPointerProperties::PointerType::LastEntry); type++) {
    m_primaryId[type] = PointerEventFactory::s_invalidId;
    m_idCount[type] = 0;
  }
  m_pointerIncomingIdMapping.clear();
  m_pointerIdMapping.clear();

  // Always add mouse pointer in initialization and never remove it.
  // No need to add it to m_pointerIncomingIdMapping as it is not going to be
  // used with the existing APIs
  m_primaryId[toInt(WebPointerProperties::PointerType::Mouse)] = s_mouseId;
  m_pointerIdMapping.add(
      s_mouseId,
      PointerAttributes(IncomingId(WebPointerProperties::PointerType::Mouse, 0),
                        0));

  m_currentId = PointerEventFactory::s_mouseId + 1;
}

int PointerEventFactory::addIdAndActiveButtons(const IncomingId p,
                                               bool isActiveButtons) {
  // Do not add extra mouse pointer as it was added in initialization
  if (p.pointerType() == WebPointerProperties::PointerType::Mouse) {
    m_pointerIdMapping.set(s_mouseId, PointerAttributes(p, isActiveButtons));
    return s_mouseId;
  }

  if (m_pointerIncomingIdMapping.contains(p)) {
    int mappedId = m_pointerIncomingIdMapping.get(p);
    m_pointerIdMapping.set(mappedId, PointerAttributes(p, isActiveButtons));
    return mappedId;
  }
  int typeInt = p.pointerTypeInt();
  // We do not handle the overflow of m_currentId as it should be very rare
  int mappedId = m_currentId++;
  if (!m_idCount[typeInt])
    m_primaryId[typeInt] = mappedId;
  m_idCount[typeInt]++;
  m_pointerIncomingIdMapping.add(p, mappedId);
  m_pointerIdMapping.add(mappedId, PointerAttributes(p, isActiveButtons));
  return mappedId;
}

bool PointerEventFactory::remove(const int mappedId) {
  // Do not remove mouse pointer id as it should always be there
  if (mappedId == s_mouseId || !m_pointerIdMapping.contains(mappedId))
    return false;

  IncomingId p = m_pointerIdMapping.get(mappedId).incomingId;
  int typeInt = p.pointerTypeInt();
  m_pointerIdMapping.remove(mappedId);
  m_pointerIncomingIdMapping.remove(p);
  if (m_primaryId[typeInt] == mappedId)
    m_primaryId[typeInt] = PointerEventFactory::s_invalidId;
  m_idCount[typeInt]--;
  return true;
}

Vector<int> PointerEventFactory::getPointerIdsOfType(
    WebPointerProperties::PointerType pointerType) const {
  Vector<int> mappedIds;

  for (auto iter = m_pointerIdMapping.begin(); iter != m_pointerIdMapping.end();
       ++iter) {
    int mappedId = iter->key;
    if (iter->value.incomingId.pointerType() == pointerType)
      mappedIds.append(mappedId);
  }

  // Sorting for a predictable ordering.
  std::sort(mappedIds.begin(), mappedIds.end());
  return mappedIds;
}

bool PointerEventFactory::isPrimary(int mappedId) const {
  if (!m_pointerIdMapping.contains(mappedId))
    return false;

  IncomingId p = m_pointerIdMapping.get(mappedId).incomingId;
  return m_primaryId[p.pointerTypeInt()] == mappedId;
}

bool PointerEventFactory::isActive(const int pointerId) const {
  return m_pointerIdMapping.contains(pointerId);
}

bool PointerEventFactory::isActiveButtonsState(const int pointerId) const {
  return m_pointerIdMapping.contains(pointerId) &&
         m_pointerIdMapping.get(pointerId).isActiveButtons;
}

WebPointerProperties::PointerType PointerEventFactory::getPointerType(
    int pointerId) const {
  if (!isActive(pointerId))
    return WebPointerProperties::PointerType::Unknown;
  return m_pointerIdMapping.get(pointerId).incomingId.pointerType();
}

int PointerEventFactory::getPointerEventId(
    const WebPointerProperties& properties) const {
  if (properties.pointerType == WebPointerProperties::PointerType::Mouse)
    return PointerEventFactory::s_mouseId;
  IncomingId id(properties.pointerType, properties.id);
  if (m_pointerIncomingIdMapping.contains(id))
    return m_pointerIncomingIdMapping.get(id);
  return PointerEventFactory::s_invalidId;
}

}  // namespace blink
