// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBInterface_h
#define USBInterface_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "device/usb/public/interfaces/device.mojom-blink.h"
#include "platform/heap/Heap.h"

namespace blink {

class ExceptionState;
class USBAlternateInterface;
class USBConfiguration;
class USBDevice;

class USBInterface
    : public GarbageCollected<USBInterface>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static USBInterface* create(const USBConfiguration*, size_t interfaceIndex);
    static USBInterface* create(const USBConfiguration*, size_t interfaceNumber, ExceptionState&);

    USBInterface(const USBDevice*, size_t configurationIndex, size_t interfaceIndex);

    const device::usb::blink::InterfaceInfo& info() const;

    uint8_t interfaceNumber() const { return info().interface_number; }
    USBAlternateInterface* alternate() const;
    HeapVector<Member<USBAlternateInterface>> alternates() const;
    bool claimed() const;

    DECLARE_TRACE();

private:
    Member<const USBDevice> m_device;
    const size_t m_configurationIndex;
    const size_t m_interfaceIndex;
};

} // namespace blink

#endif // USBInterface_h
