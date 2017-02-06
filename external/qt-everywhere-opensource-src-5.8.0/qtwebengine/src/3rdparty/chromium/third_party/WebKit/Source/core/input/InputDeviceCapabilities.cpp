// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/InputDeviceCapabilities.h"

namespace blink {

InputDeviceCapabilities::InputDeviceCapabilities(bool firesTouchEvents)
{
    m_firesTouchEvents = firesTouchEvents;
}

InputDeviceCapabilities::InputDeviceCapabilities(const InputDeviceCapabilitiesInit& initializer)
{
    m_firesTouchEvents = initializer.firesTouchEvents();
}

InputDeviceCapabilities* InputDeviceCapabilities::firesTouchEventsSourceCapabilities()
{
    DEFINE_STATIC_LOCAL(InputDeviceCapabilities, instance, (InputDeviceCapabilities::create(true)));
    return &instance;
}

InputDeviceCapabilities* InputDeviceCapabilities::doesntFireTouchEventsSourceCapabilities()
{
    DEFINE_STATIC_LOCAL(InputDeviceCapabilities, instance, (InputDeviceCapabilities::create(false)));
    return &instance;
}

} // namespace blink
