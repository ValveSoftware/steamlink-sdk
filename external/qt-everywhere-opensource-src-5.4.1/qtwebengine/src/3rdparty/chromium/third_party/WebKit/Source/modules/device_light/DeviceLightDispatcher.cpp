// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/device_light/DeviceLightDispatcher.h"

#include "modules/device_light/DeviceLightController.h"
#include "public/platform/Platform.h"

namespace WebCore {

DeviceLightDispatcher& DeviceLightDispatcher::instance()
{
    DEFINE_STATIC_LOCAL(DeviceLightDispatcher, deviceLightDispatcher, ());
    return deviceLightDispatcher;
}

DeviceLightDispatcher::DeviceLightDispatcher()
    : m_lastDeviceLightData(-1)
{
}

DeviceLightDispatcher::~DeviceLightDispatcher()
{
}

void DeviceLightDispatcher::startListening()
{
    blink::Platform::current()->setDeviceLightListener(this);
}

void DeviceLightDispatcher::stopListening()
{
    blink::Platform::current()->setDeviceLightListener(0);
    m_lastDeviceLightData = -1;
}

void DeviceLightDispatcher::didChangeDeviceLight(double value)
{
    m_lastDeviceLightData = value;
    notifyControllers();
}

double DeviceLightDispatcher::latestDeviceLightData() const
{
    return m_lastDeviceLightData;
}

} // namespace WebCore
