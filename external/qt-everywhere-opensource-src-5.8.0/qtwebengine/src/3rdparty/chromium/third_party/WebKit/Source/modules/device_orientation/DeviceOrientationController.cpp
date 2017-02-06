// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/device_orientation/DeviceOrientationController.h"

#include "core/dom/Document.h"
#include "core/frame/Deprecation.h"
#include "core/frame/HostsUsingFeatures.h"
#include "core/frame/Settings.h"
#include "modules/EventModules.h"
#include "modules/device_orientation/DeviceOrientationData.h"
#include "modules/device_orientation/DeviceOrientationDispatcher.h"
#include "modules/device_orientation/DeviceOrientationEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"

namespace blink {

DeviceOrientationController::DeviceOrientationController(Document& document)
    : DeviceSingleWindowEventController(document)
{
}

DeviceOrientationController::~DeviceOrientationController()
{
}

void DeviceOrientationController::didUpdateData()
{
    if (m_overrideOrientationData)
        return;
    dispatchDeviceEvent(lastEvent());
}

const char* DeviceOrientationController::supplementName()
{
    return "DeviceOrientationController";
}

DeviceOrientationController& DeviceOrientationController::from(Document& document)
{
    DeviceOrientationController* controller = static_cast<DeviceOrientationController*>(Supplement<Document>::from(document, supplementName()));
    if (!controller) {
        controller = new DeviceOrientationController(document);
        Supplement<Document>::provideTo(document, supplementName(), controller);
    }
    return *controller;
}

void DeviceOrientationController::didAddEventListener(LocalDOMWindow* window, const AtomicString& eventType)
{
    if (eventType != eventTypeName())
        return;

    if (document().frame()) {
        String errorMessage;
        if (document().isSecureContext(errorMessage)) {
            UseCounter::count(document().frame(), UseCounter::DeviceOrientationSecureOrigin);
        } else {
            Deprecation::countDeprecation(document().frame(), UseCounter::DeviceOrientationInsecureOrigin);
            HostsUsingFeatures::countAnyWorld(document(), HostsUsingFeatures::Feature::DeviceOrientationInsecureHost);
            if (document().frame()->settings()->strictPowerfulFeatureRestrictions())
                return;
        }
    }

    if (!m_hasEventListener)
        Platform::current()->recordRapporURL("DeviceSensors.DeviceOrientation", WebURL(document().url()));

    DeviceSingleWindowEventController::didAddEventListener(window, eventType);
}

DeviceOrientationData* DeviceOrientationController::lastData() const
{
    return m_overrideOrientationData ? m_overrideOrientationData.get() : dispatcherInstance().latestDeviceOrientationData();
}

bool DeviceOrientationController::hasLastData()
{
    return lastData();
}

void DeviceOrientationController::registerWithDispatcher()
{
    dispatcherInstance().addController(this);
}

void DeviceOrientationController::unregisterWithDispatcher()
{
    dispatcherInstance().removeController(this);
}

Event* DeviceOrientationController::lastEvent() const
{
    return DeviceOrientationEvent::create(eventTypeName(), lastData());
}

bool DeviceOrientationController::isNullEvent(Event* event) const
{
    DeviceOrientationEvent* orientationEvent = toDeviceOrientationEvent(event);
    return !orientationEvent->orientation()->canProvideEventData();
}

const AtomicString& DeviceOrientationController::eventTypeName() const
{
    return EventTypeNames::deviceorientation;
}

void DeviceOrientationController::setOverride(DeviceOrientationData* deviceOrientationData)
{
    DCHECK(deviceOrientationData);
    m_overrideOrientationData = deviceOrientationData;
    dispatchDeviceEvent(lastEvent());
}

void DeviceOrientationController::clearOverride()
{
    if (!m_overrideOrientationData)
        return;
    m_overrideOrientationData.clear();
    if (lastData())
        didUpdateData();
}

DeviceOrientationDispatcher& DeviceOrientationController::dispatcherInstance() const
{
    return DeviceOrientationDispatcher::instance(false);
}

DEFINE_TRACE(DeviceOrientationController)
{
    visitor->trace(m_overrideOrientationData);
    DeviceSingleWindowEventController::trace(visitor);
    Supplement<Document>::trace(visitor);
}

} // namespace blink
