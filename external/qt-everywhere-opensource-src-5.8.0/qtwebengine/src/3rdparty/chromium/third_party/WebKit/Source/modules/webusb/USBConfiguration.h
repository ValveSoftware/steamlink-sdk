// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBConfiguration_h
#define USBConfiguration_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "device/usb/public/interfaces/device.mojom-blink.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;
class USBDevice;
class USBInterface;

class USBConfiguration
    : public GarbageCollected<USBConfiguration>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static USBConfiguration* create(const USBDevice*, size_t configurationIndex);
    static USBConfiguration* create(const USBDevice*, size_t configurationValue, ExceptionState&);

    USBConfiguration(const USBDevice*, size_t configurationIndex);

    const USBDevice* device() const;
    size_t index() const;
    const device::usb::blink::ConfigurationInfo& info() const;

    uint8_t configurationValue() const { return info().configuration_value; }
    String configurationName() const { return info().configuration_name; }
    HeapVector<Member<USBInterface>> interfaces() const;

    DECLARE_TRACE();

private:
    Member<const USBDevice> m_device;
    const size_t m_configurationIndex;
};

} // namespace blink

#endif // USBConfiguration_h
