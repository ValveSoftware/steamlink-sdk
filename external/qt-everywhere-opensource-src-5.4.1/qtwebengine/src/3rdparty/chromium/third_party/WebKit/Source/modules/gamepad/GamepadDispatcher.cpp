// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/gamepad/GamepadDispatcher.h"

#include "modules/gamepad/NavigatorGamepad.h"
#include "public/platform/Platform.h"
#include "wtf/TemporaryChange.h"

namespace WebCore {

GamepadDispatcher& GamepadDispatcher::instance()
{
    DEFINE_STATIC_LOCAL(GamepadDispatcher, gamepadDispatcher, ());
    return gamepadDispatcher;
}

void GamepadDispatcher::sampleGamepads(blink::WebGamepads& gamepads)
{
    blink::Platform::current()->sampleGamepads(gamepads);
}

GamepadDispatcher::GamepadDispatcher()
{
}

GamepadDispatcher::~GamepadDispatcher()
{
}

void GamepadDispatcher::didConnectGamepad(unsigned index, const blink::WebGamepad& gamepad)
{
    dispatchDidConnectOrDisconnectGamepad(index, gamepad, true);
}

void GamepadDispatcher::didDisconnectGamepad(unsigned index, const blink::WebGamepad& gamepad)
{
    dispatchDidConnectOrDisconnectGamepad(index, gamepad, false);
}

void GamepadDispatcher::dispatchDidConnectOrDisconnectGamepad(unsigned index, const blink::WebGamepad& gamepad, bool connected)
{
    ASSERT(index < blink::WebGamepads::itemsLengthCap);
    ASSERT(connected == gamepad.connected);

    m_latestChange.pad = gamepad;
    m_latestChange.index = index;
    notifyControllers();
}

void GamepadDispatcher::startListening()
{
    blink::Platform::current()->setGamepadListener(this);
}

void GamepadDispatcher::stopListening()
{
    blink::Platform::current()->setGamepadListener(0);
}

} // namespace WebCore
