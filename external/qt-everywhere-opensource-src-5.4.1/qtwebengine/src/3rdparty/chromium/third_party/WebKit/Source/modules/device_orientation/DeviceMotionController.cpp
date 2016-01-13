// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/device_orientation/DeviceMotionController.h"

#include "core/dom/Document.h"
#include "modules/EventModules.h"
#include "modules/device_orientation/DeviceMotionData.h"
#include "modules/device_orientation/DeviceMotionDispatcher.h"
#include "modules/device_orientation/DeviceMotionEvent.h"

namespace WebCore {

DeviceMotionController::DeviceMotionController(Document& document)
    : DeviceSingleWindowEventController(document)
{
}

DeviceMotionController::~DeviceMotionController()
{
    stopUpdating();
}

const char* DeviceMotionController::supplementName()
{
    return "DeviceMotionController";
}

DeviceMotionController& DeviceMotionController::from(Document& document)
{
    DeviceMotionController* controller = static_cast<DeviceMotionController*>(DocumentSupplement::from(document, supplementName()));
    if (!controller) {
        controller = new DeviceMotionController(document);
        DocumentSupplement::provideTo(document, supplementName(), adoptPtrWillBeNoop(controller));
    }
    return *controller;
}

bool DeviceMotionController::hasLastData()
{
    return DeviceMotionDispatcher::instance().latestDeviceMotionData();
}

void DeviceMotionController::registerWithDispatcher()
{
    DeviceMotionDispatcher::instance().addController(this);
}

void DeviceMotionController::unregisterWithDispatcher()
{
    DeviceMotionDispatcher::instance().removeController(this);
}

PassRefPtrWillBeRawPtr<Event> DeviceMotionController::lastEvent() const
{
    return DeviceMotionEvent::create(EventTypeNames::devicemotion, DeviceMotionDispatcher::instance().latestDeviceMotionData());
}

bool DeviceMotionController::isNullEvent(Event* event) const
{
    DeviceMotionEvent* motionEvent = toDeviceMotionEvent(event);
    return !motionEvent->deviceMotionData()->canProvideEventData();
}

const AtomicString& DeviceMotionController::eventTypeName() const
{
    return EventTypeNames::devicemotion;
}

} // namespace WebCore
