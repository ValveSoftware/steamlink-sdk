/*
 * Copyright (C) 2006 Apple Computer, Inc.
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

#include "core/events/UIEventWithKeyState.h"

namespace blink {

UIEventWithKeyState::UIEventWithKeyState(const AtomicString& type, bool canBubble, bool cancelable, AbstractView* view,
    int detail, PlatformEvent::Modifiers modifiers, double platformTimeStamp, InputDeviceCapabilities* sourceCapabilities)
    : UIEvent(type, canBubble, cancelable, ComposedMode::Composed, platformTimeStamp, view, detail, sourceCapabilities)
    , m_modifiers(modifiers)
{
}

UIEventWithKeyState::UIEventWithKeyState(const AtomicString& type, const EventModifierInit& initializer)
    : UIEvent(type, initializer)
    , m_modifiers(0)
{
    if (initializer.ctrlKey())
        m_modifiers |= PlatformEvent::CtrlKey;
    if (initializer.shiftKey())
        m_modifiers |= PlatformEvent::ShiftKey;
    if (initializer.altKey())
        m_modifiers |= PlatformEvent::AltKey;
    if (initializer.metaKey())
        m_modifiers |= PlatformEvent::MetaKey;
    if (initializer.modifierAltGraph())
        m_modifiers |= PlatformEvent::AltGrKey;
    if (initializer.modifierFn())
        m_modifiers |= PlatformEvent::FnKey;
    if (initializer.modifierCapsLock())
        m_modifiers |= PlatformEvent::CapsLockOn;
    if (initializer.modifierScrollLock())
        m_modifiers |= PlatformEvent::ScrollLockOn;
    if (initializer.modifierNumLock())
        m_modifiers |= PlatformEvent::NumLockOn;
    if (initializer.modifierSymbol())
        m_modifiers |= PlatformEvent::SymbolKey;
}

bool UIEventWithKeyState::s_newTabModifierSetFromIsolatedWorld = false;

void UIEventWithKeyState::didCreateEventInIsolatedWorld(bool ctrlKey, bool shiftKey, bool altKey, bool metaKey)
{
#if OS(MACOSX)
    const bool newTabModifierSet = metaKey;
#else
    const bool newTabModifierSet = ctrlKey;
#endif
    s_newTabModifierSetFromIsolatedWorld |= newTabModifierSet;
}

void UIEventWithKeyState::setFromPlatformModifiers(EventModifierInit& initializer, const PlatformEvent::Modifiers modifiers)
{
    if (modifiers & PlatformEvent::CtrlKey)
        initializer.setCtrlKey(true);
    if (modifiers & PlatformEvent::ShiftKey)
        initializer.setShiftKey(true);
    if (modifiers & PlatformEvent::AltKey)
        initializer.setAltKey(true);
    if (modifiers & PlatformEvent::MetaKey)
        initializer.setMetaKey(true);
    if (modifiers & PlatformEvent::AltGrKey)
        initializer.setModifierAltGraph(true);
    if (modifiers & PlatformEvent::FnKey)
        initializer.setModifierFn(true);
    if (modifiers & PlatformEvent::CapsLockOn)
        initializer.setModifierCapsLock(true);
    if (modifiers & PlatformEvent::ScrollLockOn)
        initializer.setModifierScrollLock(true);
    if (modifiers & PlatformEvent::NumLockOn)
        initializer.setModifierNumLock(true);
    if (modifiers & PlatformEvent::SymbolKey)
        initializer.setModifierSymbol(true);
}

bool UIEventWithKeyState::getModifierState(const String& keyIdentifier) const
{
    struct Identifier {
        const char* identifier;
        PlatformEvent::Modifiers mask;
    };
    static const Identifier kIdentifiers[] = {
        { "Shift", PlatformEvent::ShiftKey },
        { "Control", PlatformEvent::CtrlKey },
        { "Alt", PlatformEvent::AltKey },
        { "Meta", PlatformEvent::MetaKey },
        { "AltGraph", PlatformEvent::AltGrKey },
        { "Accel",
#if OS(MACOSX)
            PlatformEvent::MetaKey
#else
            PlatformEvent::CtrlKey
#endif
        },
        { "Fn", PlatformEvent::FnKey },
        { "CapsLock", PlatformEvent::CapsLockOn },
        { "ScrollLock", PlatformEvent::ScrollLockOn },
        { "NumLock", PlatformEvent::NumLockOn },
        { "Symbol", PlatformEvent::SymbolKey },
    };
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(kIdentifiers); ++i) {
        if (keyIdentifier == kIdentifiers[i].identifier)
            return m_modifiers & kIdentifiers[i].mask;
    }
    return false;
}

void UIEventWithKeyState::initModifiers(bool ctrlKey, bool altKey, bool shiftKey, bool metaKey)
{
    m_modifiers = 0;
    if (ctrlKey)
        m_modifiers |= PlatformEvent::CtrlKey;
    if (altKey)
        m_modifiers |= PlatformEvent::AltKey;
    if (shiftKey)
        m_modifiers |= PlatformEvent::ShiftKey;
    if (metaKey)
        m_modifiers |= PlatformEvent::MetaKey;
}

UIEventWithKeyState* findEventWithKeyState(Event* event)
{
    for (Event* e = event; e; e = e->underlyingEvent())
        if (e->isKeyboardEvent() || e->isMouseEvent())
            return static_cast<UIEventWithKeyState*>(e);
    return nullptr;
}

} // namespace blink
