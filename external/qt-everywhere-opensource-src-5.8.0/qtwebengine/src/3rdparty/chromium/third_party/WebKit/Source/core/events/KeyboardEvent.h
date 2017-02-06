/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef KeyboardEvent_h
#define KeyboardEvent_h

#include "core/CoreExport.h"
#include "core/events/KeyboardEventInit.h"
#include "core/events/UIEventWithKeyState.h"
#include <memory>

namespace blink {

class EventDispatcher;
class PlatformKeyboardEvent;

class CORE_EXPORT KeyboardEvent final : public UIEventWithKeyState {
    DEFINE_WRAPPERTYPEINFO();
public:
    enum KeyLocationCode {
        DOM_KEY_LOCATION_STANDARD   = 0x00,
        DOM_KEY_LOCATION_LEFT       = 0x01,
        DOM_KEY_LOCATION_RIGHT      = 0x02,
        DOM_KEY_LOCATION_NUMPAD     = 0x03
    };

    static KeyboardEvent* create()
    {
        return new KeyboardEvent;
    }

    static KeyboardEvent* create(const PlatformKeyboardEvent& platformEvent, AbstractView* view)
    {
        return new KeyboardEvent(platformEvent, view);
    }

    static KeyboardEvent* create(ScriptState*, const AtomicString& type, const KeyboardEventInit&);

    static KeyboardEvent* create(const AtomicString& type, bool canBubble, bool cancelable, AbstractView* view,
        const String& keyIdentifier, const String& code, const String& key, unsigned location,
        PlatformEvent::Modifiers modifiers, double platformTimeStamp)
    {
        return new KeyboardEvent(type, canBubble, cancelable, view, keyIdentifier, code, key, location,
            modifiers, platformTimeStamp);
    }

    ~KeyboardEvent() override;

    void initKeyboardEvent(ScriptState*, const AtomicString& type, bool canBubble, bool cancelable, AbstractView*,
        const String& keyIdentifier, unsigned location,
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey);

    const String& keyIdentifier() const { return m_keyIdentifier; }
    const String& code() const { return m_code; }
    const String& key() const { return m_key; }

    unsigned location() const { return m_location; }

    const PlatformKeyboardEvent* keyEvent() const { return m_keyEvent.get(); }

    int keyCode() const; // key code for keydown and keyup, character for keypress
    int charCode() const; // character code for keypress, 0 for keydown and keyup
    bool repeat() const { return modifiers() & PlatformEvent::IsAutoRepeat; }

    const AtomicString& interfaceName() const override;
    bool isKeyboardEvent() const override;
    int which() const override;

    DECLARE_VIRTUAL_TRACE();

private:
    KeyboardEvent();
    KeyboardEvent(const PlatformKeyboardEvent&, AbstractView*);
    KeyboardEvent(const AtomicString&, const KeyboardEventInit&);
    KeyboardEvent(const AtomicString& type, bool canBubble, bool cancelable, AbstractView*,
        const String& keyIdentifier, const String& code, const String& key, unsigned location,
        PlatformEvent::Modifiers, double platformTimeStamp);

    void initLocationModifiers(unsigned location);

    std::unique_ptr<PlatformKeyboardEvent> m_keyEvent;
    String m_keyIdentifier;
    String m_code;
    String m_key;
    unsigned m_location;
};

DEFINE_EVENT_TYPE_CASTS(KeyboardEvent);

} // namespace blink

#endif // KeyboardEvent_h
