// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/frame_connected_bluetooth_devices.h"

#include "base/strings/string_util.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"

namespace content {

FrameConnectedBluetoothDevices::FrameConnectedBluetoothDevices(
    RenderFrameHost* rfh)
    : web_contents_impl_(static_cast<WebContentsImpl*>(
          WebContents::FromRenderFrameHost(rfh))) {}

FrameConnectedBluetoothDevices::~FrameConnectedBluetoothDevices() {
  for (size_t i = 0; i < device_id_to_connection_map_.size(); i++) {
    DecrementDevicesConnectedCount();
  }
}

bool FrameConnectedBluetoothDevices::IsConnectedToDeviceWithId(
    const std::string& device_id) {
  auto connection_iter = device_id_to_connection_map_.find(device_id);
  if (connection_iter == device_id_to_connection_map_.end()) {
    return false;
  }
  // Owners of FrameConnectedBluetoothDevices should notify it when a device
  // disconnects but currently Android and Mac don't notify of disconnection,
  // so the map could get into a state where it's holding a stale connection.
  // For this reason we return the value of IsConnected for the connection.
  // TODO(ortuno): Always return true once Android and Mac notify of
  // disconnection.
  // http://crbug.com/607273
  return connection_iter->second->IsConnected();
}

void FrameConnectedBluetoothDevices::Insert(
    const std::string& device_id,
    std::unique_ptr<device::BluetoothGattConnection> connection) {
  auto connection_iter = device_id_to_connection_map_.find(device_id);
  if (connection_iter != device_id_to_connection_map_.end()) {
    // Owners of FrameConnectedBluetoothDevices should notify it when a device
    // disconnects but currently Android and Mac don't notify of disconnection,
    // so the map could get into a state where it's holding a stale connection.
    // For this reason we check if the current connection is active and if
    // not we remove it.
    // TODO(ortuno): Remove once Android and Mac notify of disconnection.
    // http://crbug.com/607273
    if (!connection_iter->second->IsConnected()) {
      device_address_to_id_map_.erase(
          connection_iter->second->GetDeviceAddress());
      device_id_to_connection_map_.erase(connection_iter);
      DecrementDevicesConnectedCount();
    } else {
      // It's possible for WebBluetoothServiceImpl to issue two successive
      // connection requests for which it would get two successive responses
      // and consequently try to insert two BluetoothGattConnections for the
      // same device. WebBluetoothServiceImpl should reject or queue connection
      // requests if there is a pending connection already, but the platform
      // abstraction doesn't currently support checking for pending connections.
      // TODO(ortuno): CHECK that this never happens once the platform
      // abstraction allows to check for pending connections.
      // http://crbug.com/583544
      return;
    }
  }
  device_address_to_id_map_[connection->GetDeviceAddress()] = device_id;
  device_id_to_connection_map_[device_id] = std::move(connection);
  IncrementDevicesConnectedCount();
}

void FrameConnectedBluetoothDevices::CloseConnectionToDeviceWithId(
    const std::string& device_id) {
  auto connection_iter = device_id_to_connection_map_.find(device_id);
  if (connection_iter == device_id_to_connection_map_.end()) {
    return;
  }
  CHECK(device_address_to_id_map_.erase(
      connection_iter->second->GetDeviceAddress()));
  device_id_to_connection_map_.erase(connection_iter);
  DecrementDevicesConnectedCount();
}

std::string FrameConnectedBluetoothDevices::CloseConnectionToDeviceWithAddress(
    const std::string& device_address) {
  auto device_address_iter = device_address_to_id_map_.find(device_address);
  if (device_address_iter == device_address_to_id_map_.end()) {
    return std::string();
  }
  std::string device_id = device_address_iter->second;
  CHECK(device_address_to_id_map_.erase(device_address));
  CHECK(device_id_to_connection_map_.erase(device_id));
  DecrementDevicesConnectedCount();
  return device_id;
}

void FrameConnectedBluetoothDevices::IncrementDevicesConnectedCount() {
  web_contents_impl_->IncrementBluetoothConnectedDeviceCount();
}

void FrameConnectedBluetoothDevices::DecrementDevicesConnectedCount() {
  web_contents_impl_->DecrementBluetoothConnectedDeviceCount();
}

}  // namespace content
