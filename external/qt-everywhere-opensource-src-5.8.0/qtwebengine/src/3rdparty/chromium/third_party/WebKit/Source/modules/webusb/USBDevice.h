// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBDevice_h
#define USBDevice_h

#include "bindings/core/v8/ArrayBufferOrArrayBufferView.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "device/usb/public/interfaces/device.mojom-blink.h"
#include "platform/heap/Handle.h"
#include "wtf/BitVector.h"
#include "wtf/Vector.h"

namespace blink {

class ScriptPromiseResolver;
class ScriptState;
class USBConfiguration;
class USBControlTransferParameters;

class USBDevice
    : public GarbageCollectedFinalized<USBDevice>
    , public ContextLifecycleObserver
    , public ScriptWrappable {
    USING_GARBAGE_COLLECTED_MIXIN(USBDevice);
    DEFINE_WRAPPERTYPEINFO();
public:
    static USBDevice* create(device::usb::blink::DeviceInfoPtr deviceInfo, device::usb::blink::DevicePtr device, ExecutionContext* context)
    {
        return new USBDevice(std::move(deviceInfo), std::move(device), context);
    }

    explicit USBDevice(device::usb::blink::DeviceInfoPtr, device::usb::blink::DevicePtr, ExecutionContext*);
    virtual ~USBDevice();

    const device::usb::blink::DeviceInfo& info() const { return *m_deviceInfo; }
    bool isInterfaceClaimed(size_t configurationIndex, size_t interfaceIndex) const;
    size_t selectedAlternateInterface(size_t interfaceIndex) const;

    // USBDevice.idl
    uint8_t usbVersionMajor() const { return info().usb_version_major; }
    uint8_t usbVersionMinor() const { return info().usb_version_minor; }
    uint8_t usbVersionSubminor() const { return info().usb_version_subminor; }
    uint8_t deviceClass() const { return info().class_code; }
    uint8_t deviceSubclass() const { return info().subclass_code; }
    uint8_t deviceProtocol() const { return info().protocol_code; }
    uint16_t vendorId() const { return info().vendor_id; }
    uint16_t productId() const { return info().product_id; }
    uint8_t deviceVersionMajor() const { return info().device_version_major; }
    uint8_t deviceVersionMinor() const { return info().device_version_minor; }
    uint8_t deviceVersionSubminor() const { return info().device_version_subminor; }
    String manufacturerName() const { return info().manufacturer_name; }
    String productName() const { return info().product_name; }
    String serialNumber() const { return info().serial_number; }
    USBConfiguration* configuration() const;
    HeapVector<Member<USBConfiguration>> configurations() const;
    bool opened() const { return m_opened; }

    ScriptPromise open(ScriptState*);
    ScriptPromise close(ScriptState*);
    ScriptPromise selectConfiguration(ScriptState*, uint8_t configurationValue);
    ScriptPromise claimInterface(ScriptState*, uint8_t interfaceNumber);
    ScriptPromise releaseInterface(ScriptState*, uint8_t interfaceNumber);
    ScriptPromise selectAlternateInterface(ScriptState*, uint8_t interfaceNumber, uint8_t alternateSetting);
    ScriptPromise controlTransferIn(ScriptState*, const USBControlTransferParameters& setup, unsigned length);
    ScriptPromise controlTransferOut(ScriptState*, const USBControlTransferParameters& setup);
    ScriptPromise controlTransferOut(ScriptState*, const USBControlTransferParameters& setup, const ArrayBufferOrArrayBufferView& data);
    ScriptPromise clearHalt(ScriptState*, String direction, uint8_t endpointNumber);
    ScriptPromise transferIn(ScriptState*, uint8_t endpointNumber, unsigned length);
    ScriptPromise transferOut(ScriptState*, uint8_t endpointNumber, const ArrayBufferOrArrayBufferView& data);
    ScriptPromise isochronousTransferIn(ScriptState*, uint8_t endpointNumber, Vector<unsigned> packetLengths);
    ScriptPromise isochronousTransferOut(ScriptState*, uint8_t endpointNumber, const ArrayBufferOrArrayBufferView& data, Vector<unsigned> packetLengths);
    ScriptPromise reset(ScriptState*);

    // ContextLifecycleObserver interface.
    void contextDestroyed() override;

    DECLARE_TRACE();

private:
    int findConfigurationIndex(uint8_t configurationValue) const;
    int findInterfaceIndex(uint8_t interfaceNumber) const;
    int findAlternateIndex(size_t interfaceIndex, uint8_t alternateSetting) const;
    bool ensureNoDeviceOrInterfaceChangeInProgress(ScriptPromiseResolver*) const;
    bool ensureDeviceConfigured(ScriptPromiseResolver*) const;
    bool ensureInterfaceClaimed(uint8_t interfaceNumber, ScriptPromiseResolver*) const;
    bool ensureEndpointAvailable(bool inTransfer, uint8_t endpointNumber, ScriptPromiseResolver*) const;
    bool anyInterfaceChangeInProgress() const;
    device::usb::blink::ControlTransferParamsPtr convertControlTransferParameters(const USBControlTransferParameters&, ScriptPromiseResolver*) const;
    void setEndpointsForInterface(size_t interfaceIndex, bool set);

    void asyncOpen(ScriptPromiseResolver*, device::usb::blink::OpenDeviceError);
    void asyncClose(ScriptPromiseResolver*);
    void onDeviceOpenedOrClosed(bool);
    void asyncSelectConfiguration(size_t configurationIndex, ScriptPromiseResolver*, bool success);
    void onConfigurationSelected(bool success, size_t configurationIndex);
    void asyncClaimInterface(size_t interfaceIndex, ScriptPromiseResolver*, bool success);
    void asyncReleaseInterface(size_t interfaceIndex, ScriptPromiseResolver*, bool success);
    void onInterfaceClaimedOrUnclaimed(bool claimed, size_t interfaceIndex);
    void asyncSelectAlternateInterface(size_t interfaceIndex, size_t alternateIndex, ScriptPromiseResolver*, bool success);
    void asyncControlTransferIn(ScriptPromiseResolver*, device::usb::blink::TransferStatus, mojo::WTFArray<uint8_t>);
    void asyncControlTransferOut(unsigned, ScriptPromiseResolver*, device::usb::blink::TransferStatus);
    void asyncClearHalt(ScriptPromiseResolver*, bool success);
    void asyncTransferIn(ScriptPromiseResolver*, device::usb::blink::TransferStatus, mojo::WTFArray<uint8_t>);
    void asyncTransferOut(unsigned, ScriptPromiseResolver*, device::usb::blink::TransferStatus);
    void asyncIsochronousTransferIn(ScriptPromiseResolver*, mojo::WTFArray<uint8_t>, mojo::WTFArray<device::usb::blink::IsochronousPacketPtr>);
    void asyncIsochronousTransferOut(ScriptPromiseResolver*, mojo::WTFArray<device::usb::blink::IsochronousPacketPtr>);
    void asyncReset(ScriptPromiseResolver*, bool success);

    void onConnectionError();
    bool markRequestComplete(ScriptPromiseResolver*);

    device::usb::blink::DeviceInfoPtr m_deviceInfo;
    device::usb::blink::DevicePtr m_device;
    HeapHashSet<Member<ScriptPromiseResolver>> m_deviceRequests;
    bool m_opened;
    bool m_deviceStateChangeInProgress;
    int m_configurationIndex;
    WTF::BitVector m_claimedInterfaces;
    WTF::BitVector m_interfaceStateChangeInProgress;
    WTF::Vector<size_t> m_selectedAlternates;
    WTF::BitVector m_inEndpoints;
    WTF::BitVector m_outEndpoints;
};

} // namespace blink

#endif // USBDevice_h
