/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef UIEventWithKeyState_h
#define UIEventWithKeyState_h

#include "core/CoreExport.h"
#include "core/events/EventModifierInit.h"
#include "core/events/UIEvent.h"

namespace blink {

class CORE_EXPORT UIEventWithKeyState : public UIEvent {
public:
    bool ctrlKey() const { return m_modifiers & PlatformEvent::CtrlKey; }
    bool shiftKey() const { return m_modifiers & PlatformEvent::ShiftKey; }
    bool altKey() const { return m_modifiers & PlatformEvent::AltKey; }
    bool metaKey() const { return m_modifiers & PlatformEvent::MetaKey; }

    // We ignore the new tab modifiers (ctrl or meta, depending on OS) set by JavaScript when processing events.
    // However, scripts running in isolated worlds (aka content scripts) are not subject to this restriction. Since it is possible that an event created by a content script is caught and recreated by the web page's script, we resort to a global flag.
    static bool newTabModifierSetFromIsolatedWorld() { return s_newTabModifierSetFromIsolatedWorld; }
    static void clearNewTabModifierSetFromIsolatedWorld() { s_newTabModifierSetFromIsolatedWorld = false; }
    static void didCreateEventInIsolatedWorld(bool ctrlKey, bool shiftKey, bool altKey, bool metaKey);

    static void setFromPlatformModifiers(EventModifierInit&, const PlatformEvent::Modifiers);

    bool getModifierState(const String& keyIdentifier) const;

    PlatformEvent::Modifiers modifiers() const { return static_cast<PlatformEvent::Modifiers>(m_modifiers); }

protected:
    UIEventWithKeyState()
        : m_modifiers(0)
    {
    }

    UIEventWithKeyState(const AtomicString& type, bool canBubble, bool cancelable, AbstractView*,
        int detail, PlatformEvent::Modifiers, double platformTimeStamp, InputDeviceCapabilities* sourceCapabilities = nullptr);
    UIEventWithKeyState(const AtomicString& type, const EventModifierInit& initializer);
    void initModifiers(bool ctrlKey, bool altKey, bool shiftKey, bool metaKey);

    unsigned m_modifiers;

private:
    static bool s_newTabModifierSetFromIsolatedWorld;
};

UIEventWithKeyState* findEventWithKeyState(Event*);

} // namespace blink

#endif // UIEventWithKeyState_h
