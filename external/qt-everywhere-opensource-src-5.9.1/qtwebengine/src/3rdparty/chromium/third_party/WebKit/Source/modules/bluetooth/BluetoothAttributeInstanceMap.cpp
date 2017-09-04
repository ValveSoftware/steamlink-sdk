// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothAttributeInstanceMap.h"

#include "modules/bluetooth/BluetoothDevice.h"
#include "modules/bluetooth/BluetoothRemoteGATTService.h"
#include "public/platform/modules/bluetooth/WebBluetoothRemoteGATTCharacteristicInit.h"
#include "public/platform/modules/bluetooth/WebBluetoothRemoteGATTService.h"
#include <memory>
#include <utility>

namespace blink {

BluetoothAttributeInstanceMap::BluetoothAttributeInstanceMap(
    BluetoothDevice* device)
    : m_device(device) {}

BluetoothRemoteGATTService*
BluetoothAttributeInstanceMap::getOrCreateBluetoothRemoteGATTService(
    std::unique_ptr<WebBluetoothRemoteGATTService> webService) {
  String serviceInstanceId = webService->serviceInstanceID;

  BluetoothRemoteGATTService* service =
      m_serviceIdToObject.get(serviceInstanceId);

  if (!service) {
    service =
        new BluetoothRemoteGATTService(std::move(webService), m_device.get());
    m_serviceIdToObject.add(serviceInstanceId, service);
  }

  return service;
}

bool BluetoothAttributeInstanceMap::containsService(
    const String& serviceInstanceId) {
  return m_serviceIdToObject.contains(serviceInstanceId);
}

BluetoothRemoteGATTCharacteristic*
BluetoothAttributeInstanceMap::getOrCreateBluetoothRemoteGATTCharacteristic(
    ExecutionContext* context,
    std::unique_ptr<WebBluetoothRemoteGATTCharacteristicInit> webCharacteristic,
    BluetoothRemoteGATTService* service) {
  String characteristicInstanceId = webCharacteristic->characteristicInstanceID;

  BluetoothRemoteGATTCharacteristic* characteristic =
      m_characteristicIdToObject.get(characteristicInstanceId);

  if (!characteristic) {
    characteristic = BluetoothRemoteGATTCharacteristic::create(
        context, std::move(webCharacteristic), service);
    m_characteristicIdToObject.add(characteristicInstanceId, characteristic);
  }

  return characteristic;
}

bool BluetoothAttributeInstanceMap::containsCharacteristic(
    const String& characteristicInstanceId) {
  return m_characteristicIdToObject.contains(characteristicInstanceId);
}

void BluetoothAttributeInstanceMap::Clear() {
  m_serviceIdToObject.clear();
  m_characteristicIdToObject.clear();
}

DEFINE_TRACE(BluetoothAttributeInstanceMap) {
  visitor->trace(m_device);
  visitor->trace(m_serviceIdToObject);
  visitor->trace(m_characteristicIdToObject);
}

}  // namespace blink
