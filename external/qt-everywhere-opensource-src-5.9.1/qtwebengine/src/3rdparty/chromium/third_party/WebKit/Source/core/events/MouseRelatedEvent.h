/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef MouseRelatedEvent_h
#define MouseRelatedEvent_h

#include "core/CoreExport.h"
#include "core/events/MouseEventInit.h"
#include "core/events/UIEventWithKeyState.h"
#include "platform/geometry/LayoutPoint.h"

namespace blink {

// Internal only: Helper class for what's common between mouse and wheel events.
class CORE_EXPORT MouseRelatedEvent : public UIEventWithKeyState {
 public:
  enum class PositionType {
    Position,
    // Positionless mouse events are used, for example, for 'click' events from
    // keyboard input.  It's kind of surprising for a mouse event not to have a
    // position.
    Positionless
  };

  // Note that these values are adjusted to counter the effects of zoom, so that
  // values exposed via DOM APIs are invariant under zooming.
  // TODO(mustaq): Remove the PointerEvent specific code when mouse has
  // fractional coordinates. See crbug.com/655786.
  double screenX() const {
    return isPointerEvent() ? m_screenLocation.x()
                            : static_cast<int>(m_screenLocation.x());
  }
  double screenY() const {
    return isPointerEvent() ? m_screenLocation.y()
                            : static_cast<int>(m_screenLocation.y());
  }

  double clientX() const {
    return isPointerEvent() ? m_clientLocation.x()
                            : static_cast<int>(m_clientLocation.x());
  }
  double clientY() const {
    return isPointerEvent() ? m_clientLocation.y()
                            : static_cast<int>(m_clientLocation.y());
  }

  int movementX() const { return m_movementDelta.x(); }
  int movementY() const { return m_movementDelta.y(); }

  int layerX();
  int layerY();

  int offsetX();
  int offsetY();

  double pageX() const {
    return isPointerEvent() ? m_pageLocation.x()
                            : static_cast<int>(m_pageLocation.x());
  }
  double pageY() const {
    return isPointerEvent() ? m_pageLocation.y()
                            : static_cast<int>(m_pageLocation.y());
  }

  double x() const { return clientX(); }
  double y() const { return clientY(); }

  bool hasPosition() const { return m_positionType == PositionType::Position; }

  // Page point in "absolute" coordinates (i.e. post-zoomed, page-relative
  // coords, usable with LayoutObject::absoluteToLocal) relative to view(), i.e.
  // the local frame.
  const DoublePoint& absoluteLocation() const { return m_absoluteLocation; }

  DECLARE_VIRTUAL_TRACE();

 protected:
  MouseRelatedEvent();
  // TODO(lanwei): Will make this argument non-optional and all the callers need
  // to provide sourceCapabilities even when it is null, see
  // https://crbug.com/476530.
  MouseRelatedEvent(const AtomicString& type,
                    bool canBubble,
                    bool cancelable,
                    AbstractView*,
                    int detail,
                    const IntPoint& screenLocation,
                    const IntPoint& rootFrameLocation,
                    const IntPoint& movementDelta,
                    PlatformEvent::Modifiers,
                    double platformTimeStamp,
                    PositionType,
                    InputDeviceCapabilities* sourceCapabilities = nullptr);

  MouseRelatedEvent(const AtomicString& type,
                    const MouseEventInit& initializer);

  void initCoordinates(const double clientX, const double clientY);
  void receivedTarget() final;

  void computePageLocation();
  void computeRelativePosition();

  // Expose these so MouseEvent::initMouseEvent can set them.
  DoublePoint m_screenLocation;
  DoublePoint m_clientLocation;
  DoublePoint m_movementDelta;

 private:
  DoublePoint m_pageLocation;
  DoublePoint m_layerLocation;
  DoublePoint m_offsetLocation;
  DoublePoint m_absoluteLocation;
  PositionType m_positionType;
  bool m_hasCachedRelativePosition;
};

}  // namespace blink

#endif  // MouseRelatedEvent_h
