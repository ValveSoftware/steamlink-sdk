// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_win.h"

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "device/bluetooth/bluetooth_service_record_win.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/bluetooth_socket_win.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace {

const int kSdpBytesBufferSize = 1024;

}  // namespace

namespace device {

BluetoothDeviceWin::BluetoothDeviceWin(
    const BluetoothTaskManagerWin::DeviceState& state,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<BluetoothSocketThread> socket_thread,
    net::NetLog* net_log,
    const net::NetLog::Source& net_log_source)
    : BluetoothDevice(),
      ui_task_runner_(ui_task_runner),
      socket_thread_(socket_thread),
      net_log_(net_log),
      net_log_source_(net_log_source) {
  name_ = state.name;
  address_ = CanonicalizeAddress(state.address);
  bluetooth_class_ = state.bluetooth_class;
  visible_ = state.visible;
  connected_ = state.connected;
  paired_ = state.authenticated;

  for (ScopedVector<BluetoothTaskManagerWin::ServiceRecordState>::const_iterator
       iter = state.service_record_states.begin();
       iter != state.service_record_states.end();
       ++iter) {
    uint8 sdp_bytes_buffer[kSdpBytesBufferSize];
    std::copy((*iter)->sdp_bytes.begin(),
              (*iter)->sdp_bytes.end(),
              sdp_bytes_buffer);
    BluetoothServiceRecordWin* service_record = new BluetoothServiceRecordWin(
        (*iter)->name,
        (*iter)->address,
        (*iter)->sdp_bytes.size(),
        sdp_bytes_buffer);
    service_record_list_.push_back(service_record);
    uuids_.push_back(service_record->uuid());
  }
}

BluetoothDeviceWin::~BluetoothDeviceWin() {
}

void BluetoothDeviceWin::SetVisible(bool visible) {
  visible_ = visible;
}

void BluetoothDeviceWin::AddObserver(
    device::BluetoothDevice::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void BluetoothDeviceWin::RemoveObserver(
    device::BluetoothDevice::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}


uint32 BluetoothDeviceWin::GetBluetoothClass() const {
  return bluetooth_class_;
}

std::string BluetoothDeviceWin::GetDeviceName() const {
  return name_;
}

std::string BluetoothDeviceWin::GetAddress() const {
  return address_;
}

BluetoothDevice::VendorIDSource
BluetoothDeviceWin::GetVendorIDSource() const {
  return VENDOR_ID_UNKNOWN;
}

uint16 BluetoothDeviceWin::GetVendorID() const {
  return 0;
}

uint16 BluetoothDeviceWin::GetProductID() const {
  return 0;
}

uint16 BluetoothDeviceWin::GetDeviceID() const {
  return 0;
}

int BluetoothDeviceWin::GetRSSI() const {
  NOTIMPLEMENTED();
  return kUnknownPower;
}

int BluetoothDeviceWin::GetCurrentHostTransmitPower() const {
  NOTIMPLEMENTED();
  return kUnknownPower;
}

int BluetoothDeviceWin::GetMaximumHostTransmitPower() const {
  NOTIMPLEMENTED();
  return kUnknownPower;
}

bool BluetoothDeviceWin::IsPaired() const {
  return paired_;
}

bool BluetoothDeviceWin::IsConnected() const {
  return connected_;
}

bool BluetoothDeviceWin::IsConnectable() const {
  return false;
}

bool BluetoothDeviceWin::IsConnecting() const {
  return false;
}

BluetoothDevice::UUIDList BluetoothDeviceWin::GetUUIDs() const {
  return uuids_;
}

bool BluetoothDeviceWin::ExpectingPinCode() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceWin::ExpectingPasskey() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceWin::ExpectingConfirmation() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothDeviceWin::Connect(
    PairingDelegate* pairing_delegate,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::SetPinCode(const std::string& pincode) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::SetPasskey(uint32 passkey) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::ConfirmPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::RejectPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::CancelPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::Disconnect(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::Forget(const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::ConnectToService(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  scoped_refptr<BluetoothSocketWin> socket(
      BluetoothSocketWin::CreateBluetoothSocket(
          ui_task_runner_, socket_thread_, NULL, net::NetLog::Source()));
  socket->Connect(this, uuid, base::Bind(callback, socket), error_callback);
}

void BluetoothDeviceWin::CreateGattConnection(
      const GattConnectionCallback& callback,
      const ConnectErrorCallback& error_callback) {
  // TODO(armansito): Implement.
  error_callback.Run(ERROR_UNSUPPORTED_DEVICE);
}

void BluetoothDeviceWin::StartConnectionMonitor(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

const BluetoothServiceRecordWin* BluetoothDeviceWin::GetServiceRecord(
    const device::BluetoothUUID& uuid) const {
  for (ServiceRecordList::const_iterator iter = service_record_list_.begin();
       iter != service_record_list_.end();
       ++iter) {
    if ((*iter)->uuid() == uuid)
      return *iter;
  }
  return NULL;
}

}  // namespace device
