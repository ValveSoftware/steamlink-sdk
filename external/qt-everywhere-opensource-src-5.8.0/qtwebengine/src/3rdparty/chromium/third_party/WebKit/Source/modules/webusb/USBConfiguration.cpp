// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webusb/USBConfiguration.h"

#include "bindings/core/v8/ExceptionState.h"
#include "device/usb/public/interfaces/device.mojom-blink.h"
#include "modules/webusb/USBDevice.h"
#include "modules/webusb/USBInterface.h"

namespace blink {

USBConfiguration* USBConfiguration::create(const USBDevice* device, size_t configurationIndex)
{
    return new USBConfiguration(device, configurationIndex);
}

USBConfiguration* USBConfiguration::create(const USBDevice* device, size_t configurationValue, ExceptionState& exceptionState)
{
    const auto& configurations = device->info().configurations;
    for (size_t i = 0; i < configurations.size(); ++i) {
        if (configurations[i]->configuration_value == configurationValue)
            return new USBConfiguration(device, i);
    }
    exceptionState.throwRangeError("Invalid configuration value.");
    return nullptr;
}

USBConfiguration::USBConfiguration(const USBDevice* device, size_t configurationIndex)
    : m_device(device)
    , m_configurationIndex(configurationIndex)
{
    ASSERT(m_device);
    ASSERT(m_configurationIndex < m_device->info().configurations.size());
}

const USBDevice* USBConfiguration::device() const
{
    return m_device;
}

size_t USBConfiguration::index() const
{
    return m_configurationIndex;
}

const device::usb::blink::ConfigurationInfo& USBConfiguration::info() const
{
    return *m_device->info().configurations[m_configurationIndex];
}

HeapVector<Member<USBInterface>> USBConfiguration::interfaces() const
{
    HeapVector<Member<USBInterface>> interfaces;
    for (size_t i = 0; i < info().interfaces.size(); ++i)
        interfaces.append(USBInterface::create(this, i));
    return interfaces;
}

DEFINE_TRACE(USBConfiguration)
{
    visitor->trace(m_device);
}

} // namespace blink
