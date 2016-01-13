// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorBattery_h
#define NavigatorBattery_h

#include "bindings/v8/ScriptPromise.h"
#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace WebCore {

class BatteryManager;
class Navigator;

class NavigatorBattery FINAL : public NoBaseWillBeGarbageCollectedFinalized<NavigatorBattery>, public WillBeHeapSupplement<Navigator> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NavigatorBattery);
public:
    virtual ~NavigatorBattery();

    static NavigatorBattery& from(Navigator&);

    static ScriptPromise getBattery(ScriptState*, Navigator&);
    ScriptPromise getBattery(ScriptState*);

    void trace(Visitor*);

private:
    NavigatorBattery();
    static const char* supplementName();

    RefPtrWillBeMember<BatteryManager> m_batteryManager;
};

} // namespace WebCore

#endif // NavigatorBattery_h
