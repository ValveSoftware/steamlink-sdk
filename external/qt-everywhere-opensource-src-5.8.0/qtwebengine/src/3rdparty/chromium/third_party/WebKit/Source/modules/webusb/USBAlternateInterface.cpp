// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webusb/USBAlternateInterface.h"

#include "bindings/core/v8/ExceptionState.h"
#include "modules/webusb/USBEndpoint.h"
#include "modules/webusb/USBInterface.h"

namespace blink {

USBAlternateInterface* USBAlternateInterface::create(const USBInterface* interface, size_t alternateIndex)
{
    return new USBAlternateInterface(interface, alternateIndex);
}

USBAlternateInterface* USBAlternateInterface::create(const USBInterface* interface, size_t alternateSetting, ExceptionState& exceptionState)
{
    const auto& alternates = interface->info().alternates;
    for (size_t i = 0; i < alternates.size(); ++i) {
        if (alternates[i]->alternate_setting == alternateSetting)
            return USBAlternateInterface::create(interface, i);
    }
    exceptionState.throwRangeError("Invalid alternate setting.");
    return nullptr;
}

USBAlternateInterface::USBAlternateInterface(const USBInterface* interface, size_t alternateIndex)
    : m_interface(interface)
    , m_alternateIndex(alternateIndex)
{
    ASSERT(m_interface);
    ASSERT(m_alternateIndex < m_interface->info().alternates.size());
}

const device::usb::blink::AlternateInterfaceInfo& USBAlternateInterface::info() const
{
    const device::usb::blink::InterfaceInfo& interfaceInfo = m_interface->info();
    ASSERT(m_alternateIndex < interfaceInfo.alternates.size());
    return *interfaceInfo.alternates[m_alternateIndex];
}

HeapVector<Member<USBEndpoint>> USBAlternateInterface::endpoints() const
{
    HeapVector<Member<USBEndpoint>> endpoints;
    for (size_t i = 0; i < info().endpoints.size(); ++i)
        endpoints.append(USBEndpoint::create(this, i));
    return endpoints;
}

DEFINE_TRACE(USBAlternateInterface)
{
    visitor->trace(m_interface);
}

} // namespace blink
