// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceLightDispatcher_h
#define DeviceLightDispatcher_h

#include "core/frame/DeviceEventDispatcherBase.h"
#include "public/platform/WebDeviceLightListener.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class DeviceLightController;

// This class listens to device light data and notifies all registered controllers.
class DeviceLightDispatcher FINAL : public DeviceEventDispatcherBase, public blink::WebDeviceLightListener {
public:
    static DeviceLightDispatcher& instance();

    double latestDeviceLightData() const;

    // Inherited from WebDeviceLightListener.
    virtual void didChangeDeviceLight(double) OVERRIDE;

private:
    DeviceLightDispatcher();
    virtual ~DeviceLightDispatcher();

    // Inherited from DeviceEventDispatcherBase.
    virtual void startListening() OVERRIDE;
    virtual void stopListening() OVERRIDE;

    double m_lastDeviceLightData;
};

} // namespace WebCore

#endif // DeviceLightDispatcher_h
