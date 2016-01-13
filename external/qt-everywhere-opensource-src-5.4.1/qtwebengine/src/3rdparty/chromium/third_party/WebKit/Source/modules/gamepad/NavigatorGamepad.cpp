/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config.h"
#include "modules/gamepad/NavigatorGamepad.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "core/page/Page.h"
#include "modules/gamepad/GamepadDispatcher.h"
#include "modules/gamepad/GamepadEvent.h"
#include "modules/gamepad/GamepadList.h"
#include "modules/gamepad/WebKitGamepadList.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace WebCore {

template<typename T>
static void sampleGamepad(unsigned index, T& gamepad, const blink::WebGamepad& webGamepad)
{
    gamepad.setId(webGamepad.id);
    gamepad.setIndex(index);
    gamepad.setConnected(webGamepad.connected);
    gamepad.setTimestamp(webGamepad.timestamp);
    gamepad.setMapping(webGamepad.mapping);
    gamepad.setAxes(webGamepad.axesLength, webGamepad.axes);
    gamepad.setButtons(webGamepad.buttonsLength, webGamepad.buttons);
}

template<typename GamepadType, typename ListType>
static void sampleGamepads(ListType* into)
{
    blink::WebGamepads gamepads;

    GamepadDispatcher::instance().sampleGamepads(gamepads);

    for (unsigned i = 0; i < blink::WebGamepads::itemsLengthCap; ++i) {
        blink::WebGamepad& webGamepad = gamepads.items[i];
        if (i < gamepads.length && webGamepad.connected) {
            GamepadType* gamepad = into->item(i);
            if (!gamepad)
                gamepad = GamepadType::create();
            sampleGamepad(i, *gamepad, webGamepad);
            into->set(i, gamepad);
        } else {
            into->set(i, 0);
        }
    }
}

NavigatorGamepad* NavigatorGamepad::from(Document& document)
{
    if (!document.frame() || !document.frame()->domWindow())
        return 0;
    Navigator& navigator = document.frame()->domWindow()->navigator();
    return &from(navigator);
}

NavigatorGamepad& NavigatorGamepad::from(Navigator& navigator)
{
    NavigatorGamepad* supplement = static_cast<NavigatorGamepad*>(WillBeHeapSupplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        supplement = new NavigatorGamepad(navigator.frame());
        provideTo(navigator, supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
}

WebKitGamepadList* NavigatorGamepad::webkitGetGamepads(Navigator& navigator)
{
    return NavigatorGamepad::from(navigator).webkitGamepads();
}

GamepadList* NavigatorGamepad::getGamepads(Navigator& navigator)
{
    return NavigatorGamepad::from(navigator).gamepads();
}

WebKitGamepadList* NavigatorGamepad::webkitGamepads()
{
    if (!m_webkitGamepads)
        m_webkitGamepads = WebKitGamepadList::create();
    if (window()) {
        startUpdating();
        sampleGamepads<WebKitGamepad>(m_webkitGamepads.get());
    }
    return m_webkitGamepads.get();
}

GamepadList* NavigatorGamepad::gamepads()
{
    if (!m_gamepads)
        m_gamepads = GamepadList::create();
    if (window()) {
        startUpdating();
        sampleGamepads<Gamepad>(m_gamepads.get());
    }
    return m_gamepads.get();
}

void NavigatorGamepad::trace(Visitor* visitor)
{
    visitor->trace(m_gamepads);
    visitor->trace(m_webkitGamepads);
    WillBeHeapSupplement<Navigator>::trace(visitor);
}

void NavigatorGamepad::didUpdateData()
{
    // We should stop listening once we detached.
    ASSERT(window());

    // We register to the dispatcher before sampling gamepads so we need to check if we actually have an event listener.
    if (!m_hasEventListener)
        return;

    if (window()->document()->activeDOMObjectsAreStopped() || window()->document()->activeDOMObjectsAreSuspended())
        return;

    const GamepadDispatcher::ConnectionChange& change = GamepadDispatcher::instance().latestConnectionChange();

    if (!m_gamepads)
        m_gamepads = GamepadList::create();

    Gamepad* gamepad = m_gamepads->item(change.index);
    if (!gamepad)
        gamepad = Gamepad::create();
    sampleGamepad(change.index, *gamepad, change.pad);
    m_gamepads->set(change.index, gamepad);

    const AtomicString& eventName = change.pad.connected ? EventTypeNames::gamepadconnected : EventTypeNames::gamepaddisconnected;
    window()->dispatchEvent(GamepadEvent::create(eventName, false, true, gamepad));
}

NavigatorGamepad::NavigatorGamepad(LocalFrame* frame)
    : DOMWindowProperty(frame)
    , DeviceEventControllerBase(frame ? frame->page() : 0)
    , DOMWindowLifecycleObserver(frame ? frame->domWindow() : 0)
{
}

NavigatorGamepad::~NavigatorGamepad()
{
}

const char* NavigatorGamepad::supplementName()
{
    return "NavigatorGamepad";
}

void NavigatorGamepad::willDestroyGlobalObjectInFrame()
{
    stopUpdating();
    DOMWindowProperty::willDestroyGlobalObjectInFrame();
}

void NavigatorGamepad::willDetachGlobalObjectFromFrame()
{
    stopUpdating();
    DOMWindowProperty::willDetachGlobalObjectFromFrame();
}

void NavigatorGamepad::registerWithDispatcher()
{
    GamepadDispatcher::instance().addController(this);
}

void NavigatorGamepad::unregisterWithDispatcher()
{
    GamepadDispatcher::instance().removeController(this);
}

bool NavigatorGamepad::hasLastData()
{
    // Gamepad data is polled instead of pushed.
    return false;
}

static bool isGamepadEvent(const AtomicString& eventType)
{
    return eventType == EventTypeNames::gamepadconnected || eventType == EventTypeNames::gamepaddisconnected;
}

void NavigatorGamepad::didAddEventListener(LocalDOMWindow*, const AtomicString& eventType)
{
    if (RuntimeEnabledFeatures::gamepadEnabled() && isGamepadEvent(eventType)) {
        if (page() && page()->visibilityState() == PageVisibilityStateVisible)
            startUpdating();
        m_hasEventListener = true;
    }
}

void NavigatorGamepad::didRemoveEventListener(LocalDOMWindow* window, const AtomicString& eventType)
{
    if (isGamepadEvent(eventType)
        && !window->hasEventListeners(EventTypeNames::gamepadconnected)
        && !window->hasEventListeners(EventTypeNames::gamepaddisconnected)) {
        m_hasEventListener = false;
    }
}

void NavigatorGamepad::didRemoveAllEventListeners(LocalDOMWindow*)
{
    m_hasEventListener = false;
}

} // namespace WebCore
