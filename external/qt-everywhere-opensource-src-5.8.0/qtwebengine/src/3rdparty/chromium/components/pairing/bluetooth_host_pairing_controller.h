// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAIRING_BLUETOOTH_HOST_PAIRING_CONTROLLER_H_
#define COMPONENTS_PAIRING_BLUETOOTH_HOST_PAIRING_CONTROLLER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/pairing/host_pairing_controller.h"
#include "components/pairing/proto_decoder.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/hid/input_service_linux.h"

namespace device {
class BluetoothAdapter;
}

namespace net {
class IOBuffer;
}

namespace pairing_chromeos {

class BluetoothHostPairingController
    : public HostPairingController,
      public ProtoDecoder::Observer,
      public device::BluetoothAdapter::Observer,
      public device::BluetoothDevice::PairingDelegate {
 public:
  using Observer = HostPairingController::Observer;
  using InputDeviceInfo = device::InputServiceLinux::InputDeviceInfo;

  // An interface that is used for testing purpose.
  class TestDelegate {
   public:
    virtual void OnAdapterReset() = 0;

   protected:
    TestDelegate() {}
    virtual ~TestDelegate() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(TestDelegate);
  };

  explicit BluetoothHostPairingController(
      const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner);
  ~BluetoothHostPairingController() override;

  // These functions should be only used in tests.
  void SetDelegateForTesting(TestDelegate* delegate);
  scoped_refptr<device::BluetoothAdapter> GetAdapterForTesting();

 private:
  void ChangeStage(Stage new_stage);
  void SendHostStatus();

  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);
  void SetName();
  void OnSetName();
  void OnSetPowered();
  void OnCreateService(scoped_refptr<device::BluetoothSocket> socket);
  void OnSetDiscoverable(bool change_stage);
  void OnAccept(const device::BluetoothDevice* device,
                scoped_refptr<device::BluetoothSocket> socket);
  void OnSendComplete(int bytes_sent);
  void OnReceiveComplete(int bytes, scoped_refptr<net::IOBuffer> io_buffer);

  void OnCreateServiceError(const std::string& message);
  void OnSetError();
  void OnAcceptError(const std::string& error_message);
  void OnSendError(const std::string& error_message);
  void OnReceiveError(device::BluetoothSocket::ErrorReason reason,
                      const std::string& error_message);
  void PowerOffAdapterIfApplicable(const std::vector<InputDeviceInfo>& devices);
  void ResetAdapter();

  // HostPairingController:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  Stage GetCurrentStage() override;
  void StartPairing() override;
  std::string GetDeviceName() override;
  std::string GetConfirmationCode() override;
  std::string GetEnrollmentDomain() override;
  void OnNetworkConnectivityChanged(Connectivity connectivity_status) override;
  void OnUpdateStatusChanged(UpdateStatus update_status) override;
  void OnEnrollmentStatusChanged(EnrollmentStatus enrollment_status) override;
  void SetPermanentId(const std::string& permanent_id) override;
  void Reset() override;

  // ProtoDecoder::Observer:
  void OnHostStatusMessage(const pairing_api::HostStatus& message) override;
  void OnConfigureHostMessage(
      const pairing_api::ConfigureHost& message) override;
  void OnPairDevicesMessage(const pairing_api::PairDevices& message) override;
  void OnCompleteSetupMessage(
      const pairing_api::CompleteSetup& message) override;
  void OnErrorMessage(const pairing_api::Error& message) override;
  void OnAddNetworkMessage(const pairing_api::AddNetwork& message) override;

  // BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;

  // device::BluetoothDevice::PairingDelegate:
  void RequestPinCode(device::BluetoothDevice* device) override;
  void RequestPasskey(device::BluetoothDevice* device) override;
  void DisplayPinCode(device::BluetoothDevice* device,
                      const std::string& pincode) override;
  void DisplayPasskey(device::BluetoothDevice* device,
                      uint32_t passkey) override;
  void KeysEntered(device::BluetoothDevice* device, uint32_t entered) override;
  void ConfirmPasskey(device::BluetoothDevice* device,
                      uint32_t passkey) override;
  void AuthorizePairing(device::BluetoothDevice* device) override;

  Stage current_stage_;
  std::string device_name_;
  std::string confirmation_code_;
  std::string enrollment_domain_;
  Connectivity connectivity_status_;
  UpdateStatus update_status_;
  EnrollmentStatus enrollment_status_;
  std::string permanent_id_;
  bool was_powered_ = false;

  scoped_refptr<device::BluetoothAdapter> adapter_;
  scoped_refptr<device::BluetoothSocket> service_socket_;
  scoped_refptr<device::BluetoothSocket> controller_socket_;
  std::unique_ptr<ProtoDecoder> proto_decoder_;
  TestDelegate* delegate_ = nullptr;

  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  base::ThreadChecker thread_checker_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<BluetoothHostPairingController> ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothHostPairingController);
};

}  // namespace pairing_chromeos

#endif  // COMPONENTS_PAIRING_BLUETOOTH_HOST_PAIRING_CONTROLLER_H_
