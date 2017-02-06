// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webusb/USBDevice.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ToV8.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webusb/USBConfiguration.h"
#include "modules/webusb/USBControlTransferParameters.h"
#include "modules/webusb/USBInTransferResult.h"
#include "modules/webusb/USBIsochronousInTransferResult.h"
#include "modules/webusb/USBIsochronousOutTransferResult.h"
#include "modules/webusb/USBOutTransferResult.h"
#include "platform/mojo/MojoHelper.h"
#include "wtf/Assertions.h"

namespace usb = device::usb::blink;

namespace blink {

namespace {

const char kDeviceStateChangeInProgress[] = "An operation that changes the device state is in progress.";
const char kDeviceUnavailable[] = "Device unavailable.";
const char kInterfaceNotFound[] = "The interface number provided is not supported by the device in its current configuration.";
const char kInterfaceStateChangeInProgress[] = "An operation that changes interface state is in progress.";
const char kOpenRequired[] = "The device must be opened first.";

DOMException* convertFatalTransferStatus(const usb::TransferStatus& status)
{
    switch (status) {
    case usb::TransferStatus::TRANSFER_ERROR:
        return DOMException::create(NetworkError, "A transfer error has occured.");
    case usb::TransferStatus::PERMISSION_DENIED:
        return DOMException::create(SecurityError, "The transfer was not allowed.");
    case usb::TransferStatus::TIMEOUT:
        return DOMException::create(TimeoutError, "The transfer timed out.");
    case usb::TransferStatus::CANCELLED:
        return DOMException::create(AbortError, "The transfer was cancelled.");
    case usb::TransferStatus::DISCONNECT:
        return DOMException::create(NotFoundError, kDeviceUnavailable);
    case usb::TransferStatus::COMPLETED:
    case usb::TransferStatus::STALLED:
    case usb::TransferStatus::BABBLE:
    case usb::TransferStatus::SHORT_PACKET:
        return nullptr;
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

String convertTransferStatus(const usb::TransferStatus& status)
{
    switch (status) {
    case usb::TransferStatus::COMPLETED:
    case usb::TransferStatus::SHORT_PACKET:
        return "ok";
    case usb::TransferStatus::STALLED:
        return "stall";
    case usb::TransferStatus::BABBLE:
        return "babble";
    default:
        ASSERT_NOT_REACHED();
        return "";
    }
}

mojo::WTFArray<uint8_t> convertBufferSource(const ArrayBufferOrArrayBufferView& buffer)
{
    ASSERT(!buffer.isNull());
    Vector<uint8_t> vector;
    if (buffer.isArrayBuffer())
        vector.append(static_cast<uint8_t*>(buffer.getAsArrayBuffer()->data()), buffer.getAsArrayBuffer()->byteLength());
    else
        vector.append(static_cast<uint8_t*>(buffer.getAsArrayBufferView()->baseAddress()), buffer.getAsArrayBufferView()->byteLength());
    return mojo::WTFArray<uint8_t>(std::move(vector));
}

} // namespace

USBDevice::USBDevice(usb::DeviceInfoPtr deviceInfo, usb::DevicePtr device, ExecutionContext* context)
    : ContextLifecycleObserver(context)
    , m_deviceInfo(std::move(deviceInfo))
    , m_device(std::move(device))
    , m_opened(false)
    , m_deviceStateChangeInProgress(false)
    , m_configurationIndex(-1)
{
    if (m_device)
        m_device.set_connection_error_handler(createBaseCallback(WTF::bind(&USBDevice::onConnectionError, wrapWeakPersistent(this))));
    int configurationIndex = findConfigurationIndex(info().active_configuration);
    if (configurationIndex != -1)
        onConfigurationSelected(true /* success */, configurationIndex);
}

USBDevice::~USBDevice()
{
    // |m_device| may still be valid but there should be no more outstanding
    // requests because each holds a persistent handle to this object.
    DCHECK(m_deviceRequests.isEmpty());
}

bool USBDevice::isInterfaceClaimed(size_t configurationIndex, size_t interfaceIndex) const
{
    return m_configurationIndex != -1 && static_cast<size_t>(m_configurationIndex) == configurationIndex && m_claimedInterfaces.get(interfaceIndex);
}

size_t USBDevice::selectedAlternateInterface(size_t interfaceIndex) const
{
    return m_selectedAlternates[interfaceIndex];
}

USBConfiguration* USBDevice::configuration() const
{
    if (m_configurationIndex != -1)
        return USBConfiguration::create(this, m_configurationIndex);
    return nullptr;
}

HeapVector<Member<USBConfiguration>> USBDevice::configurations() const
{
    size_t numConfigurations = info().configurations.size();
    HeapVector<Member<USBConfiguration>> configurations(numConfigurations);
    for (size_t i = 0; i < numConfigurations; ++i)
        configurations[i] = USBConfiguration::create(this, i);
    return configurations;
}

ScriptPromise USBDevice::open(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureNoDeviceOrInterfaceChangeInProgress(resolver)) {
        if (m_opened) {
            resolver->resolve();
        } else {
            m_deviceStateChangeInProgress = true;
            m_deviceRequests.add(resolver);
            m_device->Open(createBaseCallback(WTF::bind(&USBDevice::asyncOpen, wrapPersistent(this), wrapPersistent(resolver))));
        }
    }
    return promise;
}

ScriptPromise USBDevice::close(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureNoDeviceOrInterfaceChangeInProgress(resolver)) {
        if (!m_opened) {
            resolver->resolve();
        } else {
            m_deviceStateChangeInProgress = true;
            m_deviceRequests.add(resolver);
            m_device->Close(createBaseCallback(WTF::bind(&USBDevice::asyncClose, wrapPersistent(this), wrapPersistent(resolver))));
        }
    }
    return promise;
}

ScriptPromise USBDevice::selectConfiguration(ScriptState* scriptState, uint8_t configurationValue)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureNoDeviceOrInterfaceChangeInProgress(resolver)) {
        if (!m_opened) {
            resolver->reject(DOMException::create(InvalidStateError, kOpenRequired));
        } else {
            int configurationIndex = findConfigurationIndex(configurationValue);
            if (configurationIndex == -1) {
                resolver->reject(DOMException::create(NotFoundError, "The configuration value provided is not supported by the device."));
            } else if (m_configurationIndex == configurationIndex) {
                resolver->resolve();
            } else {
                m_deviceStateChangeInProgress = true;
                m_deviceRequests.add(resolver);
                m_device->SetConfiguration(configurationValue, createBaseCallback(WTF::bind(&USBDevice::asyncSelectConfiguration, wrapPersistent(this), configurationIndex, wrapPersistent(resolver))));
            }
        }
    }
    return promise;
}

ScriptPromise USBDevice::claimInterface(ScriptState* scriptState, uint8_t interfaceNumber)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureDeviceConfigured(resolver)) {
        int interfaceIndex = findInterfaceIndex(interfaceNumber);
        if (interfaceIndex == -1) {
            resolver->reject(DOMException::create(NotFoundError, kInterfaceNotFound));
        } else if (m_interfaceStateChangeInProgress.get(interfaceIndex)) {
            resolver->reject(DOMException::create(InvalidStateError, kInterfaceStateChangeInProgress));
        } else if (m_claimedInterfaces.get(interfaceIndex)) {
            resolver->resolve();
        } else {
            m_interfaceStateChangeInProgress.set(interfaceIndex);
            m_deviceRequests.add(resolver);
            m_device->ClaimInterface(interfaceNumber, createBaseCallback(WTF::bind(&USBDevice::asyncClaimInterface, wrapPersistent(this), interfaceIndex, wrapPersistent(resolver))));
        }
    }
    return promise;
}

ScriptPromise USBDevice::releaseInterface(ScriptState* scriptState, uint8_t interfaceNumber)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureDeviceConfigured(resolver)) {
        int interfaceIndex = findInterfaceIndex(interfaceNumber);
        if (interfaceIndex == -1) {
            resolver->reject(DOMException::create(NotFoundError, "The interface number provided is not supported by the device in its current configuration."));
        } else if (m_interfaceStateChangeInProgress.get(interfaceIndex)) {
            resolver->reject(DOMException::create(InvalidStateError, kInterfaceStateChangeInProgress));
        } else if (!m_claimedInterfaces.get(interfaceIndex)) {
            resolver->resolve();
        } else {
            // Mark this interface's endpoints unavailable while its state is
            // changing.
            setEndpointsForInterface(interfaceIndex, false);
            m_interfaceStateChangeInProgress.set(interfaceIndex);
            m_deviceRequests.add(resolver);
            m_device->ReleaseInterface(interfaceNumber, createBaseCallback(WTF::bind(&USBDevice::asyncReleaseInterface, wrapPersistent(this), interfaceIndex, wrapPersistent(resolver))));
        }
    }
    return promise;
}

ScriptPromise USBDevice::selectAlternateInterface(ScriptState* scriptState, uint8_t interfaceNumber, uint8_t alternateSetting)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureInterfaceClaimed(interfaceNumber, resolver)) {
        // TODO(reillyg): This is duplicated work.
        int interfaceIndex = findInterfaceIndex(interfaceNumber);
        ASSERT(interfaceIndex != -1);
        int alternateIndex = findAlternateIndex(interfaceIndex, alternateSetting);
        if (alternateIndex == -1) {
            resolver->reject(DOMException::create(NotFoundError, "The alternate setting provided is not supported by the device in its current configuration."));
        } else {
            // Mark this old alternate interface's endpoints unavailable while
            // the change is in progress.
            setEndpointsForInterface(interfaceIndex, false);
            m_interfaceStateChangeInProgress.set(interfaceIndex);
            m_deviceRequests.add(resolver);
            m_device->SetInterfaceAlternateSetting(interfaceNumber, alternateSetting, createBaseCallback(WTF::bind(&USBDevice::asyncSelectAlternateInterface, wrapPersistent(this), interfaceNumber, alternateSetting, wrapPersistent(resolver))));
        }
    }
    return promise;
}

ScriptPromise USBDevice::controlTransferIn(ScriptState* scriptState, const USBControlTransferParameters& setup, unsigned length)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureDeviceConfigured(resolver)) {
        auto parameters = convertControlTransferParameters(setup, resolver);
        if (parameters) {
            m_deviceRequests.add(resolver);
            m_device->ControlTransferIn(std::move(parameters), length, 0, createBaseCallback(WTF::bind(&USBDevice::asyncControlTransferIn, wrapPersistent(this), wrapPersistent(resolver))));
        }
    }
    return promise;
}

ScriptPromise USBDevice::controlTransferOut(ScriptState* scriptState, const USBControlTransferParameters& setup)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureDeviceConfigured(resolver)) {
        auto parameters = convertControlTransferParameters(setup, resolver);
        if (parameters) {
            m_deviceRequests.add(resolver);
            m_device->ControlTransferOut(std::move(parameters), mojo::WTFArray<uint8_t>(), 0, createBaseCallback(WTF::bind(&USBDevice::asyncControlTransferOut, wrapPersistent(this), 0, wrapPersistent(resolver))));
        }
    }
    return promise;
}

ScriptPromise USBDevice::controlTransferOut(ScriptState* scriptState, const USBControlTransferParameters& setup, const ArrayBufferOrArrayBufferView& data)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureDeviceConfigured(resolver)) {
        auto parameters = convertControlTransferParameters(setup, resolver);
        if (parameters) {
            mojo::WTFArray<uint8_t> buffer = convertBufferSource(data);
            unsigned transferLength = buffer.size();
            m_deviceRequests.add(resolver);
            m_device->ControlTransferOut(std::move(parameters), std::move(buffer), 0, createBaseCallback(WTF::bind(&USBDevice::asyncControlTransferOut, wrapPersistent(this), transferLength, wrapPersistent(resolver))));
        }
    }
    return promise;
}

ScriptPromise USBDevice::clearHalt(ScriptState* scriptState, String direction, uint8_t endpointNumber)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureEndpointAvailable(direction == "in", endpointNumber, resolver)) {
        m_deviceRequests.add(resolver);
        m_device->ClearHalt(endpointNumber, createBaseCallback(WTF::bind(&USBDevice::asyncClearHalt, wrapPersistent(this), wrapPersistent(resolver))));
    }
    return promise;
}

ScriptPromise USBDevice::transferIn(ScriptState* scriptState, uint8_t endpointNumber, unsigned length)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureEndpointAvailable(true /* in */, endpointNumber, resolver)) {
        m_deviceRequests.add(resolver);
        m_device->GenericTransferIn(endpointNumber, length, 0, createBaseCallback(WTF::bind(&USBDevice::asyncTransferIn, wrapPersistent(this), wrapPersistent(resolver))));
    }
    return promise;
}

ScriptPromise USBDevice::transferOut(ScriptState* scriptState, uint8_t endpointNumber, const ArrayBufferOrArrayBufferView& data)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureEndpointAvailable(false /* out */, endpointNumber, resolver)) {
        mojo::WTFArray<uint8_t> buffer = convertBufferSource(data);
        unsigned transferLength = buffer.size();
        m_deviceRequests.add(resolver);
        m_device->GenericTransferOut(endpointNumber, std::move(buffer), 0, createBaseCallback(WTF::bind(&USBDevice::asyncTransferOut, wrapPersistent(this), transferLength, wrapPersistent(resolver))));
    }
    return promise;
}

ScriptPromise USBDevice::isochronousTransferIn(ScriptState* scriptState, uint8_t endpointNumber, Vector<unsigned> packetLengths)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureEndpointAvailable(true /* in */, endpointNumber, resolver)) {
        m_deviceRequests.add(resolver);
        m_device->IsochronousTransferIn(endpointNumber, mojo::WTFArray<uint32_t>(std::move(packetLengths)), 0, createBaseCallback(WTF::bind(&USBDevice::asyncIsochronousTransferIn, wrapPersistent(this), wrapPersistent(resolver))));
    }
    return promise;
}

ScriptPromise USBDevice::isochronousTransferOut(ScriptState* scriptState, uint8_t endpointNumber, const ArrayBufferOrArrayBufferView& data, Vector<unsigned> packetLengths)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureEndpointAvailable(false /* out */, endpointNumber, resolver)) {
        m_deviceRequests.add(resolver);
        m_device->IsochronousTransferOut(endpointNumber, convertBufferSource(data), mojo::WTFArray<uint32_t>(std::move(packetLengths)), 0, createBaseCallback(WTF::bind(&USBDevice::asyncIsochronousTransferOut, wrapPersistent(this), wrapPersistent(resolver))));
    }
    return promise;
}

ScriptPromise USBDevice::reset(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureNoDeviceOrInterfaceChangeInProgress(resolver)) {
        if (!m_opened) {
            resolver->reject(DOMException::create(InvalidStateError, kOpenRequired));
        } else {
            m_deviceRequests.add(resolver);
            m_device->Reset(createBaseCallback(WTF::bind(&USBDevice::asyncReset, wrapPersistent(this), wrapPersistent(resolver))));
        }
    }
    return promise;
}

void USBDevice::contextDestroyed()
{
    m_device.reset();
    m_deviceRequests.clear();
}

DEFINE_TRACE(USBDevice)
{
    ContextLifecycleObserver::trace(visitor);
    visitor->trace(m_deviceRequests);
}

int USBDevice::findConfigurationIndex(uint8_t configurationValue) const
{
    const auto& configurations = info().configurations;
    for (size_t i = 0; i < configurations.size(); ++i) {
        if (configurations[i]->configuration_value == configurationValue)
            return i;
    }
    return -1;
}

int USBDevice::findInterfaceIndex(uint8_t interfaceNumber) const
{
    ASSERT(m_configurationIndex != -1);
    const auto& interfaces = info().configurations[m_configurationIndex]->interfaces;
    for (size_t i = 0; i < interfaces.size(); ++i) {
        if (interfaces[i]->interface_number == interfaceNumber)
            return i;
    }
    return -1;
}

int USBDevice::findAlternateIndex(size_t interfaceIndex, uint8_t alternateSetting) const
{
    ASSERT(m_configurationIndex != -1);
    const auto& alternates = info().configurations[m_configurationIndex]->interfaces[interfaceIndex]->alternates;
    for (size_t i = 0; i < alternates.size(); ++i) {
        if (alternates[i]->alternate_setting == alternateSetting)
            return i;
    }
    return -1;
}

bool USBDevice::ensureNoDeviceOrInterfaceChangeInProgress(ScriptPromiseResolver* resolver) const
{
    if (!m_device)
        resolver->reject(DOMException::create(NotFoundError, kDeviceUnavailable));
    else if (m_deviceStateChangeInProgress)
        resolver->reject(DOMException::create(InvalidStateError, kDeviceStateChangeInProgress));
    else if (anyInterfaceChangeInProgress())
        resolver->reject(DOMException::create(InvalidStateError, kInterfaceStateChangeInProgress));
    else
        return true;
    return false;
}

bool USBDevice::ensureDeviceConfigured(ScriptPromiseResolver* resolver) const
{
    if (!m_device)
        resolver->reject(DOMException::create(NotFoundError, kDeviceUnavailable));
    else if (m_deviceStateChangeInProgress)
        resolver->reject(DOMException::create(InvalidStateError, kDeviceStateChangeInProgress));
    else if (!m_opened)
        resolver->reject(DOMException::create(InvalidStateError, kOpenRequired));
    else if (m_configurationIndex == -1)
        resolver->reject(DOMException::create(InvalidStateError, "The device must have a configuration selected."));
    else
        return true;
    return false;
}

bool USBDevice::ensureInterfaceClaimed(uint8_t interfaceNumber, ScriptPromiseResolver* resolver) const
{
    if (!ensureDeviceConfigured(resolver))
        return false;
    int interfaceIndex = findInterfaceIndex(interfaceNumber);
    if (interfaceIndex == -1)
        resolver->reject(DOMException::create(NotFoundError, kInterfaceNotFound));
    else if (m_interfaceStateChangeInProgress.get(interfaceIndex))
        resolver->reject(DOMException::create(InvalidStateError, kInterfaceStateChangeInProgress));
    else if (!m_claimedInterfaces.get(interfaceIndex))
        resolver->reject(DOMException::create(InvalidStateError, "The specified interface has not been claimed."));
    else
        return true;
    return false;
}

bool USBDevice::ensureEndpointAvailable(bool inTransfer, uint8_t endpointNumber, ScriptPromiseResolver* resolver) const
{
    if (!ensureDeviceConfigured(resolver))
        return false;
    if (endpointNumber == 0 || endpointNumber >= 16) {
        resolver->reject(DOMException::create(IndexSizeError, "The specified endpoint number is out of range."));
        return false;
    }
    auto& bitVector = inTransfer ? m_inEndpoints : m_outEndpoints;
    if (!bitVector.get(endpointNumber - 1)) {
        resolver->reject(DOMException::create(NotFoundError, "The specified endpoint is not part of a claimed and selected alternate interface."));
        return false;
    }
    return true;
}

bool USBDevice::anyInterfaceChangeInProgress() const
{
    for (size_t i = 0; i < m_interfaceStateChangeInProgress.size(); ++i) {
        if (m_interfaceStateChangeInProgress.quickGet(i))
            return true;
    }
    return false;
}

usb::ControlTransferParamsPtr USBDevice::convertControlTransferParameters(
    const USBControlTransferParameters& parameters,
    ScriptPromiseResolver* resolver) const
{
    auto mojoParameters = usb::ControlTransferParams::New();

    if (parameters.requestType() == "standard") {
        mojoParameters->type = usb::ControlTransferType::STANDARD;
    } else if (parameters.requestType() == "class") {
        mojoParameters->type = usb::ControlTransferType::CLASS;
    } else if (parameters.requestType() == "vendor") {
        mojoParameters->type = usb::ControlTransferType::VENDOR;
    } else {
        resolver->reject(DOMException::create(TypeMismatchError, "The control transfer requestType parameter is invalid."));
        return nullptr;
    }

    if (parameters.recipient() == "device") {
        mojoParameters->recipient = usb::ControlTransferRecipient::DEVICE;
    } else if (parameters.recipient() == "interface") {
        size_t interfaceNumber = parameters.index() & 0xff;
        if (!ensureInterfaceClaimed(interfaceNumber, resolver))
            return nullptr;
        mojoParameters->recipient = usb::ControlTransferRecipient::INTERFACE;
    } else if (parameters.recipient() == "endpoint") {
        bool inTransfer = parameters.index() & 0x80;
        size_t endpointNumber = parameters.index() & 0x0f;
        if (!ensureEndpointAvailable(inTransfer, endpointNumber, resolver))
            return nullptr;
        mojoParameters->recipient = usb::ControlTransferRecipient::ENDPOINT;
    } else if (parameters.recipient() == "other") {
        mojoParameters->recipient = usb::ControlTransferRecipient::OTHER;
    } else {
        resolver->reject(DOMException::create(TypeMismatchError, "The control transfer recipient parameter is invalid."));
        return nullptr;
    }

    mojoParameters->request = parameters.request();
    mojoParameters->value = parameters.value();
    mojoParameters->index = parameters.index();
    return mojoParameters;
}

void USBDevice::setEndpointsForInterface(size_t interfaceIndex, bool set)
{
    const auto& configuration = *info().configurations[m_configurationIndex];
    const auto& interface = *configuration.interfaces[interfaceIndex];
    const auto& alternate = *interface.alternates[m_selectedAlternates[interfaceIndex]];
    for (const auto& endpoint : alternate.endpoints.storage()) {
        uint8_t endpointNumber = endpoint->endpoint_number;
        if (endpointNumber == 0 || endpointNumber >= 16)
            continue; // Ignore endpoints with invalid indices.
        auto& bitVector = endpoint->direction == usb::TransferDirection::INBOUND ? m_inEndpoints : m_outEndpoints;
        if (set)
            bitVector.set(endpointNumber - 1);
        else
            bitVector.clear(endpointNumber - 1);
    }
}

void USBDevice::asyncOpen(ScriptPromiseResolver* resolver, usb::OpenDeviceError error)
{
    if (!markRequestComplete(resolver))
        return;

    switch (error) {
    case usb::OpenDeviceError::ALREADY_OPEN:
        ASSERT_NOT_REACHED();
        // fall through
    case usb::OpenDeviceError::OK:
        onDeviceOpenedOrClosed(true /* opened */);
        resolver->resolve();
        return;
    case usb::OpenDeviceError::ACCESS_DENIED:
        onDeviceOpenedOrClosed(false /* not opened */);
        resolver->reject(DOMException::create(SecurityError, "Access denied."));
        return;
    }
}

void USBDevice::asyncClose(ScriptPromiseResolver* resolver)
{
    if (!markRequestComplete(resolver))
        return;

    onDeviceOpenedOrClosed(false /* closed */);
    resolver->resolve();
}

void USBDevice::onDeviceOpenedOrClosed(bool opened)
{
    m_opened = opened;
    m_deviceStateChangeInProgress = false;
}

void USBDevice::asyncSelectConfiguration(size_t configurationIndex, ScriptPromiseResolver* resolver, bool success)
{
    if (!markRequestComplete(resolver))
        return;

    onConfigurationSelected(success, configurationIndex);
    if (success)
        resolver->resolve();
    else
        resolver->reject(DOMException::create(NetworkError, "Unable to set device configuration."));
}

void USBDevice::onConfigurationSelected(bool success, size_t configurationIndex)
{
    if (success) {
        m_configurationIndex = configurationIndex;
        size_t numInterfaces = info().configurations[m_configurationIndex]->interfaces.size();
        m_claimedInterfaces.clearAll();
        m_claimedInterfaces.resize(numInterfaces);
        m_interfaceStateChangeInProgress.clearAll();
        m_interfaceStateChangeInProgress.resize(numInterfaces);
        m_selectedAlternates.resize(numInterfaces);
        m_selectedAlternates.fill(0);
        m_inEndpoints.clearAll();
        m_outEndpoints.clearAll();
    }
    m_deviceStateChangeInProgress = false;
}

void USBDevice::asyncClaimInterface(size_t interfaceIndex, ScriptPromiseResolver* resolver, bool success)
{
    if (!markRequestComplete(resolver))
        return;

    onInterfaceClaimedOrUnclaimed(success, interfaceIndex);
    if (success)
        resolver->resolve();
    else
        resolver->reject(DOMException::create(NetworkError, "Unable to claim interface."));
}

void USBDevice::asyncReleaseInterface(size_t interfaceIndex, ScriptPromiseResolver* resolver, bool success)
{
    if (!markRequestComplete(resolver))
        return;

    onInterfaceClaimedOrUnclaimed(!success, interfaceIndex);
    if (success)
        resolver->resolve();
    else
        resolver->reject(DOMException::create(NetworkError, "Unable to release interface."));
}

void USBDevice::onInterfaceClaimedOrUnclaimed(bool claimed, size_t interfaceIndex)
{
    if (claimed) {
        m_claimedInterfaces.set(interfaceIndex);
    } else {
        m_claimedInterfaces.clear(interfaceIndex);
        m_selectedAlternates[interfaceIndex] = 0;
    }
    setEndpointsForInterface(interfaceIndex, claimed);
    m_interfaceStateChangeInProgress.clear(interfaceIndex);
}

void USBDevice::asyncSelectAlternateInterface(size_t interfaceIndex, size_t alternateIndex, ScriptPromiseResolver* resolver, bool success)
{
    if (!markRequestComplete(resolver))
        return;

    if (success)
        m_selectedAlternates[interfaceIndex] = alternateIndex;
    setEndpointsForInterface(interfaceIndex, success);
    m_interfaceStateChangeInProgress.clear(interfaceIndex);

    if (success)
        resolver->resolve();
    else
        resolver->reject(DOMException::create(NetworkError, "Unable to set device interface."));
}

void USBDevice::asyncControlTransferIn(ScriptPromiseResolver* resolver, usb::TransferStatus status, mojo::WTFArray<uint8_t> data)
{
    if (!markRequestComplete(resolver))
        return;

    DOMException* error = convertFatalTransferStatus(status);
    if (error)
        resolver->reject(error);
    else
        resolver->resolve(USBInTransferResult::create(convertTransferStatus(status), data.PassStorage()));
}

void USBDevice::asyncControlTransferOut(unsigned transferLength, ScriptPromiseResolver* resolver, usb::TransferStatus status)
{
    if (!markRequestComplete(resolver))
        return;

    DOMException* error = convertFatalTransferStatus(status);
    if (error)
        resolver->reject(error);
    else
        resolver->resolve(USBOutTransferResult::create(convertTransferStatus(status), transferLength));
}

void USBDevice::asyncClearHalt(ScriptPromiseResolver* resolver, bool success)
{
    if (!markRequestComplete(resolver))
        return;

    if (success)
        resolver->resolve();
    else
        resolver->reject(DOMException::create(NetworkError, "Unable to clear endpoint."));
}

void USBDevice::asyncTransferIn(ScriptPromiseResolver* resolver, usb::TransferStatus status, mojo::WTFArray<uint8_t> data)
{
    if (!markRequestComplete(resolver))
        return;

    DOMException* error = convertFatalTransferStatus(status);
    if (error)
        resolver->reject(error);
    else
        resolver->resolve(USBInTransferResult::create(convertTransferStatus(status), data.PassStorage()));
}

void USBDevice::asyncTransferOut(unsigned transferLength, ScriptPromiseResolver* resolver, usb::TransferStatus status)
{
    if (!markRequestComplete(resolver))
        return;

    DOMException* error = convertFatalTransferStatus(status);
    if (error)
        resolver->reject(error);
    else
        resolver->resolve(USBOutTransferResult::create(convertTransferStatus(status), transferLength));
}

void USBDevice::asyncIsochronousTransferIn(ScriptPromiseResolver* resolver, mojo::WTFArray<uint8_t> data, mojo::WTFArray<usb::IsochronousPacketPtr> mojoPackets)
{
    if (!markRequestComplete(resolver))
        return;

    DOMArrayBuffer* buffer = DOMArrayBuffer::create(data.storage().data(), data.storage().size());
    HeapVector<Member<USBIsochronousInTransferPacket>> packets;
    packets.reserveCapacity(mojoPackets.size());
    size_t byteOffset = 0;
    for (const auto& packet : mojoPackets.storage()) {
        DOMException* error = convertFatalTransferStatus(packet->status);
        if (error) {
            resolver->reject(error);
            return;
        }
        packets.append(USBIsochronousInTransferPacket::create(convertTransferStatus(packet->status), DOMDataView::create(buffer, byteOffset, packet->transferred_length)));
        byteOffset += packet->length;
    }
    resolver->resolve(USBIsochronousInTransferResult::create(buffer, packets));
}

void USBDevice::asyncIsochronousTransferOut(ScriptPromiseResolver* resolver, mojo::WTFArray<usb::IsochronousPacketPtr> mojoPackets)
{
    if (!markRequestComplete(resolver))
        return;

    HeapVector<Member<USBIsochronousOutTransferPacket>> packets;
    packets.reserveCapacity(mojoPackets.size());
    for (const auto& packet : mojoPackets.storage()) {
        DOMException* error = convertFatalTransferStatus(packet->status);
        if (error) {
            resolver->reject(error);
            return;
        }
        packets.append(USBIsochronousOutTransferPacket::create(convertTransferStatus(packet->status), packet->transferred_length));
    }
    resolver->resolve(USBIsochronousOutTransferResult::create(packets));
}

void USBDevice::asyncReset(ScriptPromiseResolver* resolver, bool success)
{
    if (!markRequestComplete(resolver))
        return;

    if (success)
        resolver->resolve();
    else
        resolver->reject(DOMException::create(NetworkError, "Unable to reset the device."));
}

void USBDevice::onConnectionError()
{
    m_device.reset();
    m_opened = false;
    for (ScriptPromiseResolver* resolver : m_deviceRequests)
        resolver->reject(DOMException::create(NotFoundError, kDeviceUnavailable));
    m_deviceRequests.clear();
}

bool USBDevice::markRequestComplete(ScriptPromiseResolver* resolver)
{
    auto requestEntry = m_deviceRequests.find(resolver);
    if (requestEntry == m_deviceRequests.end())
        return false;
    m_deviceRequests.remove(requestEntry);
    return true;
}

} // namespace blink
