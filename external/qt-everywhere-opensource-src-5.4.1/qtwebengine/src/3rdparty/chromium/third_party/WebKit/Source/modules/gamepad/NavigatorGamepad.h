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

#ifndef NavigatorGamepad_h
#define NavigatorGamepad_h

#include "core/frame/DOMWindowLifecycleObserver.h"
#include "core/frame/DOMWindowProperty.h"
#include "core/frame/DeviceEventControllerBase.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebGamepads.h"

namespace blink {
class WebGamepad;
class WebGamepads;
}

namespace WebCore {

class Document;
class GamepadList;
class Navigator;
class WebKitGamepadList;

class NavigatorGamepad FINAL : public NoBaseWillBeGarbageCollectedFinalized<NavigatorGamepad>, public WillBeHeapSupplement<Navigator>, public DOMWindowProperty, public DeviceEventControllerBase, public DOMWindowLifecycleObserver {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NavigatorGamepad);
public:
    static NavigatorGamepad* from(Document&);
    static NavigatorGamepad& from(Navigator&);
    virtual ~NavigatorGamepad();

    static WebKitGamepadList* webkitGetGamepads(Navigator&);
    static GamepadList* getGamepads(Navigator&);

    WebKitGamepadList* webkitGamepads();
    GamepadList* gamepads();

    virtual void trace(Visitor*);

    void didConnectOrDisconnectGamepad(unsigned index, const blink::WebGamepad&, bool connected);

private:
    explicit NavigatorGamepad(LocalFrame*);

    static const char* supplementName();

    // DOMWindowProperty
    virtual void willDestroyGlobalObjectInFrame() OVERRIDE;
    virtual void willDetachGlobalObjectFromFrame() OVERRIDE;

    // DeviceEventControllerBase
    virtual void registerWithDispatcher() OVERRIDE;
    virtual void unregisterWithDispatcher() OVERRIDE;
    virtual bool hasLastData() OVERRIDE;
    virtual void didUpdateData() OVERRIDE;

    // DOMWindowLifecycleObserver
    virtual void didAddEventListener(LocalDOMWindow*, const AtomicString&) OVERRIDE;
    virtual void didRemoveEventListener(LocalDOMWindow*, const AtomicString&) OVERRIDE;
    virtual void didRemoveAllEventListeners(LocalDOMWindow*) OVERRIDE;

    PersistentWillBeMember<GamepadList> m_gamepads;
    PersistentWillBeMember<WebKitGamepadList> m_webkitGamepads;
};

} // namespace WebCore

#endif // NavigatorGamepad_h
