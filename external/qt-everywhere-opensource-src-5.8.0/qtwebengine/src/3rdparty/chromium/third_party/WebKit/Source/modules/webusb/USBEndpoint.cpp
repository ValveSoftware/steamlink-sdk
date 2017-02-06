// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webusb/USBEndpoint.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/DOMException.h"
#include "device/usb/public/interfaces/device.mojom-blink.h"
#include "modules/webusb/USBAlternateInterface.h"

using device::usb::blink::EndpointType;
using device::usb::blink::TransferDirection;

namespace blink {

namespace {

String convertDirectionToEnum(const TransferDirection& direction)
{
    switch (direction) {
    case TransferDirection::INBOUND:
        return "in";
    case TransferDirection::OUTBOUND:
        return "out";
    default:
        ASSERT_NOT_REACHED();
        return "";
    }
}

String convertTypeToEnum(const EndpointType& type)
{
    switch (type) {
    case EndpointType::BULK:
        return "bulk";
    case EndpointType::INTERRUPT:
        return "interrupt";
    case EndpointType::ISOCHRONOUS:
        return "isochronous";
    default:
        ASSERT_NOT_REACHED();
        return "";
    }
}

} // namespace

USBEndpoint* USBEndpoint::create(const USBAlternateInterface* alternate, size_t endpointIndex)
{
    return new USBEndpoint(alternate, endpointIndex);
}

USBEndpoint* USBEndpoint::create(const USBAlternateInterface* alternate, size_t endpointNumber, const String& direction, ExceptionState& exceptionState)
{
    TransferDirection mojoDirection = direction == "in" ? TransferDirection::INBOUND : TransferDirection::OUTBOUND;
    const auto& endpoints = alternate->info().endpoints;
    for (size_t i = 0; i < endpoints.size(); ++i) {
        const auto& endpoint = endpoints[i];
        if (endpoint->endpoint_number == endpointNumber && endpoint->direction == mojoDirection)
            return USBEndpoint::create(alternate, i);
    }
    exceptionState.throwRangeError("No such endpoint exists in the given alternate interface.");
    return nullptr;
}

USBEndpoint::USBEndpoint(const USBAlternateInterface* alternate, size_t endpointIndex)
    : m_alternate(alternate)
    , m_endpointIndex(endpointIndex)
{
    ASSERT(m_alternate);
    ASSERT(m_endpointIndex < m_alternate->info().endpoints.size());
}

const device::usb::blink::EndpointInfo& USBEndpoint::info() const
{
    const device::usb::blink::AlternateInterfaceInfo& alternateInfo = m_alternate->info();
    ASSERT(m_endpointIndex < alternateInfo.endpoints.size());
    return *alternateInfo.endpoints[m_endpointIndex];
}

String USBEndpoint::direction() const
{
    return convertDirectionToEnum(info().direction);
}

String USBEndpoint::type() const
{
    return convertTypeToEnum(info().type);
}

DEFINE_TRACE(USBEndpoint)
{
    visitor->trace(m_alternate);
}

} // namespace blink
