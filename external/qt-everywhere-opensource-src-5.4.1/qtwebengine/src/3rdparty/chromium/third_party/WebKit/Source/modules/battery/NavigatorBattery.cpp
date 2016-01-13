// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/battery/NavigatorBattery.h"

#include "core/frame/LocalFrame.h"
#include "modules/battery/BatteryManager.h"

namespace WebCore {

NavigatorBattery::NavigatorBattery()
{
}

NavigatorBattery::~NavigatorBattery()
{
}

ScriptPromise NavigatorBattery::getBattery(ScriptState* scriptState, Navigator& navigator)
{
    return NavigatorBattery::from(navigator).getBattery(scriptState);
}

ScriptPromise NavigatorBattery::getBattery(ScriptState* scriptState)
{
    if (!m_batteryManager)
        m_batteryManager = BatteryManager::create(scriptState->executionContext());

    return m_batteryManager->startRequest(scriptState);
}

const char* NavigatorBattery::supplementName()
{
    return "NavigatorBattery";
}

NavigatorBattery& NavigatorBattery::from(Navigator& navigator)
{
    NavigatorBattery* supplement = static_cast<NavigatorBattery*>(WillBeHeapSupplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        supplement = new NavigatorBattery();
        provideTo(navigator, supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
}

void NavigatorBattery::trace(Visitor* visitor)
{
    visitor->trace(m_batteryManager);
    WillBeHeapSupplement<Navigator>::trace(visitor);
}

} // namespace WebCore
