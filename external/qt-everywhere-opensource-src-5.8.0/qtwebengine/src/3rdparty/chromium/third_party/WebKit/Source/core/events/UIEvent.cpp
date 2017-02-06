/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
 */

#include "core/events/UIEvent.h"


namespace blink {

UIEvent::UIEvent()
    : m_detail(0)
    , m_sourceCapabilities(nullptr)
{
}

// TODO(lanwei): Will add sourceCapabilities to all the subclass of UIEvent later, see https://crbug.com/476530.
UIEvent::UIEvent(const AtomicString& eventType, bool canBubbleArg, bool cancelableArg, ComposedMode composedMode, AbstractView* viewArg, int detailArg, InputDeviceCapabilities* sourceCapabilitiesArg)
    : Event(eventType, canBubbleArg, cancelableArg, composedMode)
    , m_view(viewArg)
    , m_detail(detailArg)
    , m_sourceCapabilities(sourceCapabilitiesArg)
{
}

UIEvent::UIEvent(const AtomicString& eventType, bool canBubbleArg, bool cancelableArg, ComposedMode composedMode, double platformTimeStamp, AbstractView* viewArg, int detailArg, InputDeviceCapabilities* sourceCapabilitiesArg)
    : Event(eventType, canBubbleArg, cancelableArg, composedMode, platformTimeStamp)
    , m_view(viewArg)
    , m_detail(detailArg)
    , m_sourceCapabilities(sourceCapabilitiesArg)
{
}

UIEvent::UIEvent(const AtomicString& eventType, const UIEventInit& initializer)
    : Event(eventType, initializer)
    , m_view(initializer.view())
    , m_detail(initializer.detail())
    , m_sourceCapabilities(initializer.sourceCapabilities())
{
}

UIEvent::~UIEvent()
{
}

void UIEvent::initUIEvent(const AtomicString& typeArg, bool canBubbleArg, bool cancelableArg, AbstractView* viewArg, int detailArg)
{
    initUIEventInternal(typeArg, canBubbleArg, cancelableArg, nullptr, viewArg, detailArg, nullptr);
}

void UIEvent::initUIEventInternal(const AtomicString& typeArg, bool canBubbleArg, bool cancelableArg, EventTarget* relatedTarget, AbstractView* viewArg, int detailArg, InputDeviceCapabilities* sourceCapabilitiesArg)
{
    if (isBeingDispatched())
        return;

    initEvent(typeArg, canBubbleArg, cancelableArg, relatedTarget);

    m_view = viewArg;
    m_detail = detailArg;
    m_sourceCapabilities = sourceCapabilitiesArg;
}

bool UIEvent::isUIEvent() const
{
    return true;
}

const AtomicString& UIEvent::interfaceName() const
{
    return EventNames::UIEvent;
}

int UIEvent::which() const
{
    return 0;
}

DEFINE_TRACE(UIEvent)
{
    visitor->trace(m_view);
    visitor->trace(m_sourceCapabilities);
    Event::trace(visitor);
}

} // namespace blink
