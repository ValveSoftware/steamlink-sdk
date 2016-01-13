// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GamepadDispatcher_h
#define GamepadDispatcher_h

#include "core/frame/DeviceEventDispatcherBase.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebGamepad.h"
#include "public/platform/WebGamepadListener.h"

namespace blink {
class WebGamepads;
}

namespace WebCore {

class NavigatorGamepad;

class GamepadDispatcher : public DeviceEventDispatcherBase, public blink::WebGamepadListener {
public:
    static GamepadDispatcher& instance();

    void sampleGamepads(blink::WebGamepads&);

    struct ConnectionChange {
        blink::WebGamepad pad;
        unsigned index;
    };

    const ConnectionChange& latestConnectionChange() const { return m_latestChange; }

private:
    GamepadDispatcher();
    virtual ~GamepadDispatcher();

    // WebGamepadListener
    virtual void didConnectGamepad(unsigned index, const blink::WebGamepad&) OVERRIDE;
    virtual void didDisconnectGamepad(unsigned index, const blink::WebGamepad&) OVERRIDE;

    // DeviceEventDispatcherBase
    virtual void startListening() OVERRIDE;
    virtual void stopListening() OVERRIDE;

    void dispatchDidConnectOrDisconnectGamepad(unsigned index, const blink::WebGamepad&, bool connected);

    ConnectionChange m_latestChange;
};

} // namespace WebCore

#endif
