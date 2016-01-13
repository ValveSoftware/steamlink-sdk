// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/gamepad/GamepadEvent.h"

namespace WebCore {

GamepadEventInit::GamepadEventInit()
{
}

GamepadEvent::GamepadEvent()
{
    ScriptWrappable::init(this);
}

GamepadEvent::GamepadEvent(const AtomicString& type, bool canBubble, bool cancelable, Gamepad* gamepad)
    : Event(type, canBubble, cancelable)
    , m_gamepad(gamepad)
{
    ScriptWrappable::init(this);
}

GamepadEvent::GamepadEvent(const AtomicString& type, const GamepadEventInit& initializer)
    : Event(type, initializer)
    , m_gamepad(initializer.gamepad)
{
    ScriptWrappable::init(this);
}

GamepadEvent::~GamepadEvent()
{
}

const AtomicString& GamepadEvent::interfaceName() const
{
    return EventNames::GamepadEvent;
}

void GamepadEvent::trace(Visitor* visitor)
{
    visitor->trace(m_gamepad);
    Event::trace(visitor);
}

} // namespace WebCore
