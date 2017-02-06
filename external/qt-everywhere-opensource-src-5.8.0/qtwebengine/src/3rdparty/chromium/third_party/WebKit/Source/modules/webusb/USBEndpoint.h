// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBEndpoint_h
#define USBEndpoint_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "device/usb/public/interfaces/device.mojom-blink.h"
#include "platform/heap/Heap.h"

namespace blink {

class ExceptionState;
class USBAlternateInterface;

class USBEndpoint
    : public GarbageCollected<USBEndpoint>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static USBEndpoint* create(const USBAlternateInterface*, size_t endpointIndex);
    static USBEndpoint* create(const USBAlternateInterface*, size_t endpointNumber, const String& direction, ExceptionState&);

    USBEndpoint(const USBAlternateInterface*, size_t endpointIndex);

    const device::usb::blink::EndpointInfo& info() const;

    uint8_t endpointNumber() const { return info().endpoint_number; }
    String direction() const;
    String type() const;
    unsigned packetSize() const { return info().packet_size; }

    DECLARE_TRACE();

private:
    Member<const USBAlternateInterface> m_alternate;
    const size_t m_endpointIndex;
};

} // namespace blink

#endif // USBEndpoint_h
