// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/battery/BatteryDispatcher.h"

#include "platform/mojo/MojoHelper.h"
#include "public/platform/Platform.h"
#include "public/platform/ServiceRegistry.h"
#include "wtf/Assertions.h"

namespace blink {

BatteryDispatcher& BatteryDispatcher::instance()
{
    DEFINE_STATIC_LOCAL(BatteryDispatcher, batteryDispatcher, (new BatteryDispatcher));
    return batteryDispatcher;
}

BatteryDispatcher::BatteryDispatcher()
    : m_hasLatestData(false)
{
}

void BatteryDispatcher::queryNextStatus()
{
    m_monitor->QueryNextStatus(createBaseCallback(WTF::bind(&BatteryDispatcher::onDidChange, wrapPersistent(this))));
}

void BatteryDispatcher::onDidChange(device::blink::BatteryStatusPtr batteryStatus)
{
    queryNextStatus();

    DCHECK(batteryStatus);

    updateBatteryStatus(BatteryStatus(
        batteryStatus->charging,
        batteryStatus->charging_time,
        batteryStatus->discharging_time,
        batteryStatus->level));
}

void BatteryDispatcher::updateBatteryStatus(const BatteryStatus& batteryStatus)
{
    m_batteryStatus = batteryStatus;
    m_hasLatestData = true;
    notifyControllers();
}

void BatteryDispatcher::startListening()
{
    DCHECK(!m_monitor.is_bound());
    Platform::current()->serviceRegistry()->connectToRemoteService(
        mojo::GetProxy(&m_monitor));
    queryNextStatus();
}

void BatteryDispatcher::stopListening()
{
    m_monitor.reset();
    m_hasLatestData = false;
}

} // namespace blink
