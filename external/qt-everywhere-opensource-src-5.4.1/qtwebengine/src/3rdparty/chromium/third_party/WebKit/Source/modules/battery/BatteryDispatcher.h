// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BatteryDispatcher_h
#define BatteryDispatcher_h

#include "core/frame/DeviceEventDispatcherBase.h"
#include "modules/battery/BatteryManager.h"
#include "modules/battery/BatteryStatus.h"
#include "public/platform/WebBatteryStatusListener.h"

namespace blink {
class WebBatteryStatus;
}

namespace WebCore {

class BatteryDispatcher FINAL : public DeviceEventDispatcherBase, public blink::WebBatteryStatusListener {
public:
    static BatteryDispatcher& instance();
    virtual ~BatteryDispatcher();

    BatteryStatus* latestData();

    // Inherited from blink::WebBatteryStatusListener.
    virtual void updateBatteryStatus(const blink::WebBatteryStatus&) OVERRIDE;

private:
    BatteryDispatcher();

    // Inherited from DeviceEventDispatcherBase.
    virtual void startListening() OVERRIDE;
    virtual void stopListening() OVERRIDE;

    RefPtrWillBePersistent<BatteryStatus> m_batteryStatus;
};

}

#endif // BatteryDispatcher_h
