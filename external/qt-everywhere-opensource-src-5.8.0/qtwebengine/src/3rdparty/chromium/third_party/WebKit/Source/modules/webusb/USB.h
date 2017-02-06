// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USB_h
#define USB_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"
#include "device/usb/public/interfaces/chooser_service.mojom-blink.h"
#include "device/usb/public/interfaces/device_manager.mojom-blink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;
class ScopedScriptPromiseResolver;
class ScriptState;
class USBDevice;
class USBDeviceRequestOptions;

class USB final
    : public EventTargetWithInlineData
    , public ContextLifecycleObserver
    , public device::usb::blink::DeviceManagerClient {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(USB);
    USING_PRE_FINALIZER(USB, dispose);
public:
    static USB* create(LocalFrame& frame)
    {
        return new USB(frame);
    }

    virtual ~USB();

    void dispose();

    // USB.idl
    ScriptPromise getDevices(ScriptState*);
    ScriptPromise requestDevice(ScriptState*, const USBDeviceRequestOptions&);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(connect);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(disconnect);

    // EventTarget overrides.
    ExecutionContext* getExecutionContext() const override;
    const AtomicString& interfaceName() const override;

    // ContextLifecycleObserver overrides.
    void contextDestroyed() override;

    USBDevice* getOrCreateDevice(device::usb::blink::DeviceInfoPtr);

    device::usb::blink::DeviceManager* deviceManager() const { return m_deviceManager.get(); }

    void onGetDevices(ScriptPromiseResolver*, mojo::WTFArray<device::usb::blink::DeviceInfoPtr>);
    void onGetPermission(ScriptPromiseResolver*, device::usb::blink::DeviceInfoPtr);

    // DeviceManagerClient implementation.
    void OnDeviceAdded(device::usb::blink::DeviceInfoPtr);
    void OnDeviceRemoved(device::usb::blink::DeviceInfoPtr);

    void onDeviceManagerConnectionError();
    void onChooserServiceConnectionError();

    DECLARE_VIRTUAL_TRACE();

private:
    explicit USB(LocalFrame& frame);

    device::usb::blink::DeviceManagerPtr m_deviceManager;
    HeapHashSet<Member<ScriptPromiseResolver>> m_deviceManagerRequests;
    device::usb::blink::ChooserServicePtr m_chooserService;
    HeapHashSet<Member<ScriptPromiseResolver>> m_chooserServiceRequests;
    mojo::Binding<device::usb::blink::DeviceManagerClient> m_clientBinding;
    HeapHashMap<String, WeakMember<USBDevice>> m_deviceCache;
};

} // namespace blink

#endif // USB_h
