// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothDevice_h
#define BluetoothDevice_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "modules/EventTargetModules.h"
#include "modules/bluetooth/BluetoothRemoteGATTServer.h"
#include "platform/heap/Heap.h"
#include "public/platform/modules/bluetooth/WebBluetoothDevice.h"
#include "public/platform/modules/bluetooth/WebBluetoothDeviceInit.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class BluetoothAttributeInstanceMap;
class BluetoothRemoteGATTCharacteristic;
class BluetoothRemoteGATTServer;
class BluetoothRemoteGATTService;
class ScriptPromise;
class ScriptPromiseResolver;

struct WebBluetoothRemoteGATTCharacteristicInit;
struct WebBluetoothRemoteGATTService;

// BluetoothDevice represents a physical bluetooth device in the DOM. See IDL.
//
// Callbacks providing WebBluetoothDevice objects are handled by
// CallbackPromiseAdapter templatized with this class. See this class's
// "Interface required by CallbackPromiseAdapter" section and the
// CallbackPromiseAdapter class comments.
class BluetoothDevice final : public EventTargetWithInlineData,
                              public ContextLifecycleObserver,
                              public WebBluetoothDevice {
  USING_PRE_FINALIZER(BluetoothDevice, dispose);
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(BluetoothDevice);

 public:
  BluetoothDevice(ExecutionContext*, std::unique_ptr<WebBluetoothDeviceInit>);

  // Interface required by CallbackPromiseAdapter:
  using WebType = std::unique_ptr<WebBluetoothDeviceInit>;
  static BluetoothDevice* take(ScriptPromiseResolver*,
                               std::unique_ptr<WebBluetoothDeviceInit>);

  BluetoothRemoteGATTService* getOrCreateBluetoothRemoteGATTService(
      std::unique_ptr<WebBluetoothRemoteGATTService>);
  bool isValidService(const String& serviceInstanceId);

  BluetoothRemoteGATTCharacteristic*
  getOrCreateBluetoothRemoteGATTCharacteristic(
      ExecutionContext*,
      std::unique_ptr<WebBluetoothRemoteGATTCharacteristicInit>,
      BluetoothRemoteGATTService*);
  bool isValidCharacteristic(const String& characteristicInstanceId);

  // We should disconnect from the device in all of the following cases:
  // 1. When the object gets GarbageCollected e.g. it went out of scope.
  // dispose() is called in this case.
  // 2. When the parent document gets detached e.g. reloading a page.
  // stop() is called in this case.
  // TODO(ortuno): Users should be able to turn on notifications for
  // events on navigator.bluetooth and still remain connected even if the
  // BluetoothDevice object is garbage collected.

  // USING_PRE_FINALIZER interface.
  // Called before the object gets garbage collected.
  void dispose();

  // ContextLifecycleObserver interface.
  void contextDestroyed() override;

  // If gatt is connected then sets gatt.connected to false and disconnects.
  // This function only performs the necessary steps to ensure a device
  // disconnects therefore it should only be used when the object is being
  // garbage collected or the context is being destroyed.
  void disconnectGATTIfConnected();

  // Performs necessary cleanup when a device disconnects and fires
  // gattserverdisconnected event.
  void cleanupDisconnectedDeviceAndFireEvent();

  // EventTarget methods:
  const AtomicString& interfaceName() const override;
  ExecutionContext* getExecutionContext() const override;

  // WebBluetoothDevice interface:
  void dispatchGattServerDisconnected() override;

  // Interface required by Garbage Collection:
  DECLARE_VIRTUAL_TRACE();

  // IDL exposed interface:
  String id() { return m_webDevice->id; }
  String name() { return m_webDevice->name; }
  BluetoothRemoteGATTServer* gatt() { return m_gatt; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(gattserverdisconnected);

 private:
  // Holds all GATT Attributes associated with this BluetoothDevice.
  Member<BluetoothAttributeInstanceMap> m_attributeInstanceMap;

  std::unique_ptr<WebBluetoothDeviceInit> m_webDevice;
  Member<BluetoothRemoteGATTServer> m_gatt;
};

}  // namespace blink

#endif  // BluetoothDevice_h
