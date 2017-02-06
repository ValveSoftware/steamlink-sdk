// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webusb/USB.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "device/usb/public/interfaces/device.mojom-blink.h"
#include "modules/EventTargetModules.h"
#include "modules/webusb/USBConnectionEvent.h"
#include "modules/webusb/USBDevice.h"
#include "modules/webusb/USBDeviceFilter.h"
#include "modules/webusb/USBDeviceRequestOptions.h"
#include "platform/UserGestureIndicator.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/ServiceRegistry.h"
#include "wtf/Functional.h"

namespace usb = device::usb::blink;

namespace blink {
namespace {

const char kNoServiceError[] = "USB service unavailable.";

usb::DeviceFilterPtr convertDeviceFilter(const USBDeviceFilter& filter)
{
    auto mojoFilter = usb::DeviceFilter::New();
    mojoFilter->has_vendor_id = filter.hasVendorId();
    if (mojoFilter->has_vendor_id)
        mojoFilter->vendor_id = filter.vendorId();
    mojoFilter->has_product_id = filter.hasProductId();
    if (mojoFilter->has_product_id)
        mojoFilter->product_id = filter.productId();
    mojoFilter->has_class_code = filter.hasClassCode();
    if (mojoFilter->has_class_code)
        mojoFilter->class_code = filter.classCode();
    mojoFilter->has_subclass_code = filter.hasSubclassCode();
    if (mojoFilter->has_subclass_code)
        mojoFilter->subclass_code = filter.subclassCode();
    mojoFilter->has_protocol_code = filter.hasProtocolCode();
    if (mojoFilter->has_protocol_code)
        mojoFilter->protocol_code = filter.protocolCode();
    return mojoFilter;
}

} // namespace

USB::USB(LocalFrame& frame)
    : ContextLifecycleObserver(frame.document())
    , m_clientBinding(this)
{
    ThreadState::current()->registerPreFinalizer(this);
    frame.serviceRegistry()->connectToRemoteService(mojo::GetProxy(&m_deviceManager));
    m_deviceManager.set_connection_error_handler(createBaseCallback(WTF::bind(&USB::onDeviceManagerConnectionError, wrapWeakPersistent(this))));
    m_deviceManager->SetClient(m_clientBinding.CreateInterfacePtrAndBind());
}

USB::~USB()
{
    // |m_deviceManager| and |m_chooserService| may still be valid but there
    // should be no more outstanding requests to them because each holds a
    // persistent handle to this object.
    DCHECK(m_deviceManagerRequests.isEmpty());
    DCHECK(m_chooserServiceRequests.isEmpty());
}

void USB::dispose()
{
    // The pipe to this object must be closed when is marked unreachable to
    // prevent messages from being dispatched before lazy sweeping.
    m_clientBinding.Close();
}

ScriptPromise USB::getDevices(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (!m_deviceManager) {
        resolver->reject(DOMException::create(NotSupportedError));
    } else {
        String errorMessage;
        if (!scriptState->getExecutionContext()->isSecureContext(errorMessage)) {
            resolver->reject(DOMException::create(SecurityError, errorMessage));
        } else {
            m_deviceManagerRequests.add(resolver);
            m_deviceManager->GetDevices(nullptr, createBaseCallback(WTF::bind(&USB::onGetDevices, wrapPersistent(this), wrapPersistent(resolver))));
        }
    }
    return promise;
}

ScriptPromise USB::requestDevice(ScriptState* scriptState, const USBDeviceRequestOptions& options)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (!m_chooserService) {
        LocalFrame* frame = getExecutionContext() ? toDocument(getExecutionContext())->frame() : nullptr;
        if (!frame) {
            resolver->reject(DOMException::create(NotSupportedError));
            return promise;
        }
        frame->serviceRegistry()->connectToRemoteService(mojo::GetProxy(&m_chooserService));
        m_chooserService.set_connection_error_handler(createBaseCallback(WTF::bind(&USB::onChooserServiceConnectionError, wrapWeakPersistent(this))));
    }

    String errorMessage;
    if (!scriptState->getExecutionContext()->isSecureContext(errorMessage)) {
        resolver->reject(DOMException::create(SecurityError, errorMessage));
    } else if (!UserGestureIndicator::consumeUserGesture()) {
        resolver->reject(DOMException::create(SecurityError, "Must be handling a user gesture to show a permission request."));
    } else {
        Vector<usb::DeviceFilterPtr> filters;
        if (options.hasFilters()) {
            filters.reserveCapacity(options.filters().size());
            for (const auto& filter : options.filters())
                filters.append(convertDeviceFilter(filter));
        }
        m_chooserServiceRequests.add(resolver);
        m_chooserService->GetPermission(std::move(filters), createBaseCallback(WTF::bind(&USB::onGetPermission, wrapPersistent(this), wrapPersistent(resolver))));
    }
    return promise;
}

ExecutionContext* USB::getExecutionContext() const
{
    return ContextLifecycleObserver::getExecutionContext();
}

const AtomicString& USB::interfaceName() const
{
    return EventTargetNames::USB;
}

void USB::contextDestroyed()
{
    m_deviceManager.reset();
    m_deviceManagerRequests.clear();
    m_chooserService.reset();
    m_chooserServiceRequests.clear();
}

USBDevice* USB::getOrCreateDevice(usb::DeviceInfoPtr deviceInfo)
{
    USBDevice* device = m_deviceCache.get(deviceInfo->guid);
    if (!device) {
        String guid = deviceInfo->guid;
        usb::DevicePtr pipe;
        m_deviceManager->GetDevice(guid, mojo::GetProxy(&pipe));
        device = USBDevice::create(std::move(deviceInfo), std::move(pipe), getExecutionContext());
        m_deviceCache.add(guid, device);
    }
    return device;
}

void USB::onGetDevices(ScriptPromiseResolver* resolver, mojo::WTFArray<usb::DeviceInfoPtr> deviceInfos)
{
    auto requestEntry = m_deviceManagerRequests.find(resolver);
    if (requestEntry == m_deviceManagerRequests.end())
        return;
    m_deviceManagerRequests.remove(requestEntry);

    HeapVector<Member<USBDevice>> devices;
    for (auto& deviceInfo : deviceInfos.PassStorage())
        devices.append(getOrCreateDevice(std::move(deviceInfo)));
    resolver->resolve(devices);
    m_deviceManagerRequests.remove(resolver);
}

void USB::onGetPermission(ScriptPromiseResolver* resolver, usb::DeviceInfoPtr deviceInfo)
{
    auto requestEntry = m_chooserServiceRequests.find(resolver);
    if (requestEntry == m_chooserServiceRequests.end())
        return;
    m_chooserServiceRequests.remove(requestEntry);

    if (!m_deviceManager) {
        resolver->reject(DOMException::create(NotFoundError, kNoServiceError));
        return;
    }

    if (deviceInfo)
        resolver->resolve(getOrCreateDevice(std::move(deviceInfo)));
    else
        resolver->reject(DOMException::create(NotFoundError, "No device selected."));
}

void USB::OnDeviceAdded(usb::DeviceInfoPtr deviceInfo)
{
    if (!m_deviceManager)
        return;

    dispatchEvent(USBConnectionEvent::create(EventTypeNames::connect, getOrCreateDevice(std::move(deviceInfo))));
}

void USB::OnDeviceRemoved(usb::DeviceInfoPtr deviceInfo)
{
    String guid = deviceInfo->guid;
    USBDevice* device = m_deviceCache.get(guid);
    if (!device)
        device = USBDevice::create(std::move(deviceInfo), nullptr, getExecutionContext());
    dispatchEvent(USBConnectionEvent::create(EventTypeNames::disconnect, device));
    m_deviceCache.remove(guid);
}

void USB::onDeviceManagerConnectionError()
{
    m_deviceManager.reset();
    for (ScriptPromiseResolver* resolver : m_deviceManagerRequests)
        resolver->reject(DOMException::create(NotFoundError, kNoServiceError));
    m_deviceManagerRequests.clear();
}

void USB::onChooserServiceConnectionError()
{
    m_chooserService.reset();
    for (ScriptPromiseResolver* resolver : m_chooserServiceRequests)
        resolver->reject(DOMException::create(NotFoundError, kNoServiceError));
    m_chooserServiceRequests.clear();
}

DEFINE_TRACE(USB)
{
    EventTargetWithInlineData::trace(visitor);
    ContextLifecycleObserver::trace(visitor);
    visitor->trace(m_deviceManagerRequests);
    visitor->trace(m_chooserServiceRequests);
    visitor->trace(m_deviceCache);
}

} // namespace blink
