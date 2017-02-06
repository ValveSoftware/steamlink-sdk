// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webusb/USBInterface.h"

#include "bindings/core/v8/ExceptionState.h"
#include "device/usb/public/interfaces/device.mojom-blink.h"
#include "modules/webusb/USBAlternateInterface.h"
#include "modules/webusb/USBConfiguration.h"
#include "modules/webusb/USBDevice.h"

namespace blink {

USBInterface* USBInterface::create(const USBConfiguration* configuration, size_t interfaceIndex)
{
    return new USBInterface(configuration->device(), configuration->index(), interfaceIndex);
}

USBInterface* USBInterface::create(const USBConfiguration* configuration, size_t interfaceNumber, ExceptionState& exceptionState)
{
    const auto& interfaces = configuration->info().interfaces;
    for (size_t i = 0; i < interfaces.size(); ++i) {
        if (interfaces[i]->interface_number == interfaceNumber)
            return new USBInterface(configuration->device(), configuration->index(), i);
    }
    exceptionState.throwRangeError("Invalid interface index.");
    return nullptr;
}

USBInterface::USBInterface(const USBDevice* device, size_t configurationIndex, size_t interfaceIndex)
    : m_device(device)
    , m_configurationIndex(configurationIndex)
    , m_interfaceIndex(interfaceIndex)
{
    ASSERT(m_configurationIndex < m_device->info().configurations.size());
    ASSERT(m_interfaceIndex < m_device->info().configurations[m_configurationIndex]->interfaces.size());
}

const device::usb::blink::InterfaceInfo& USBInterface::info() const
{
    return *m_device->info().configurations[m_configurationIndex]->interfaces[m_interfaceIndex];
}

USBAlternateInterface* USBInterface::alternate() const
{
    if (m_device->isInterfaceClaimed(m_configurationIndex, m_interfaceIndex))
        return USBAlternateInterface::create(this, m_device->selectedAlternateInterface(m_interfaceIndex));
    return nullptr;
}

HeapVector<Member<USBAlternateInterface>> USBInterface::alternates() const
{
    HeapVector<Member<USBAlternateInterface>> alternates;
    for (size_t i = 0; i < info().alternates.size(); ++i)
        alternates.append(USBAlternateInterface::create(this, i));
    return alternates;
}

bool USBInterface::claimed() const
{
    return m_device->isInterfaceClaimed(m_configurationIndex, m_interfaceIndex);
}

DEFINE_TRACE(USBInterface)
{
    visitor->trace(m_device);
}

} // namespace blink
