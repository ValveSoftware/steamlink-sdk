// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothAttributeInstanceMap_h
#define BluetoothAttributeInstanceMap_h

#include "modules/bluetooth/BluetoothRemoteGATTCharacteristic.h"
#include "modules/bluetooth/BluetoothRemoteGATTService.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include <memory>

namespace blink {

class BluetoothDevice;
class ExecutionContext;
class ScriptPromiseResolver;

struct WebBluetoothRemoteGATTCharacteristicInit;
struct WebBluetoothRemoteGATTService;

// Map that holds all GATT attributes, i.e. BluetoothRemoteGATTService,
// BluetoothRemoteGATTCharacteristic, BluetoothRemoteGATTDescriptor, for
// the BluetoothDevice passed in when constructing the object.
// Methods in this map are used to create or retrieve these attributes.
class BluetoothAttributeInstanceMap final
    : public GarbageCollected<BluetoothAttributeInstanceMap> {
 public:
  BluetoothAttributeInstanceMap(BluetoothDevice*);

  // Constructs a new BluetoothRemoteGATTService object if there was
  // no service with the same instance id and adds it to the map.
  // Otherwise returns the BluetoothRemoteGATTService object already
  // in the map.
  BluetoothRemoteGATTService* getOrCreateBluetoothRemoteGATTService(
      std::unique_ptr<WebBluetoothRemoteGATTService>);

  // Returns true if a BluetoothRemoteGATTService with |serviceInstanceId|
  // is in the map.
  bool containsService(const String& serviceInstanceId);

  // Constructs a new BluetoothRemoteGATTCharacteristic object if there was no
  // characteristic with the same instance id and adds it to the map.
  // Otherwise returns the BluetoothRemoteGATTCharacteristic object already in
  // the map.
  BluetoothRemoteGATTCharacteristic*
  getOrCreateBluetoothRemoteGATTCharacteristic(
      ExecutionContext*,
      std::unique_ptr<WebBluetoothRemoteGATTCharacteristicInit>,
      BluetoothRemoteGATTService*);

  // Returns true if a BluetoothRemoteGATTCharacteristic with
  // |characteristicInstanceId| is in the map.
  bool containsCharacteristic(const String& characteristicInstanceId);

  // Removes all Attributes from the map.
  // TODO(crbug.com/654950): Remove descriptors when implemented.
  void Clear();

  DECLARE_VIRTUAL_TRACE();

 private:
  // BluetoothDevice that owns this map.
  Member<BluetoothDevice> m_device;
  // Map of service instance ids to objects.
  HeapHashMap<String, Member<BluetoothRemoteGATTService>> m_serviceIdToObject;
  // Map of characteristic instance ids to objects.
  HeapHashMap<String, Member<BluetoothRemoteGATTCharacteristic>>
      m_characteristicIdToObject;
};

}  // namespace blink

#endif  // BluetoothAttributeInstanceMap_h
