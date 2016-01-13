// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/DeviceEventControllerBase.h"

#include "core/page/Page.h"

namespace WebCore {

DeviceEventControllerBase::DeviceEventControllerBase(Page* page)
    : PageLifecycleObserver(page)
    , m_hasEventListener(false)
    , m_isActive(false)
    , m_timer(this, &DeviceEventControllerBase::oneShotCallback)
{
}

DeviceEventControllerBase::~DeviceEventControllerBase()
{
}

void DeviceEventControllerBase::oneShotCallback(Timer<DeviceEventControllerBase>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_timer);
    ASSERT(hasLastData());
    ASSERT(!m_timer.isActive());

    didUpdateData();
}

void DeviceEventControllerBase::startUpdating()
{
    if (m_isActive)
        return;

    if (hasLastData() && !m_timer.isActive()) {
        // Make sure to fire the data as soon as possible.
        m_timer.startOneShot(0, FROM_HERE);
    }

    registerWithDispatcher();
    m_isActive = true;
}

void DeviceEventControllerBase::stopUpdating()
{
    if (!m_isActive)
        return;

    if (m_timer.isActive())
        m_timer.stop();

    unregisterWithDispatcher();
    m_isActive = false;
}

void DeviceEventControllerBase::pageVisibilityChanged()
{
    if (!m_hasEventListener)
        return;

    if (page()->visibilityState() == PageVisibilityStateVisible)
        startUpdating();
    else
        stopUpdating();
}

} // namespace WebCore
