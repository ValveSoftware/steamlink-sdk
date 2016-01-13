// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_H_

#include <queue>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_agent_service_provider.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_input_client.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {
class BluetoothSocketThread;
}  // namespace device

namespace chromeos {

class BluetoothChromeOSTest;
class BluetoothDeviceChromeOS;
class BluetoothPairingChromeOS;

// The BluetoothAdapterChromeOS class implements BluetoothAdapter for the
// Chrome OS platform.
class BluetoothAdapterChromeOS
    : public device::BluetoothAdapter,
      public chromeos::BluetoothAdapterClient::Observer,
      public chromeos::BluetoothDeviceClient::Observer,
      public chromeos::BluetoothInputClient::Observer,
      public chromeos::BluetoothAgentServiceProvider::Delegate {
 public:
  static base::WeakPtr<BluetoothAdapter> CreateAdapter();

  // BluetoothAdapter:
  virtual void AddObserver(
      device::BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(
      device::BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual std::string GetAddress() const OVERRIDE;
  virtual std::string GetName() const OVERRIDE;
  virtual void SetName(const std::string& name,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) OVERRIDE;
  virtual bool IsInitialized() const OVERRIDE;
  virtual bool IsPresent() const OVERRIDE;
  virtual bool IsPowered() const OVERRIDE;
  virtual void SetPowered(
      bool powered,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual bool IsDiscoverable() const OVERRIDE;
  virtual void SetDiscoverable(
      bool discoverable,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual bool IsDiscovering() const OVERRIDE;
  virtual void CreateRfcommService(
      const device::BluetoothUUID& uuid,
      int channel,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) OVERRIDE;
  virtual void CreateL2capService(
      const device::BluetoothUUID& uuid,
      int psm,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) OVERRIDE;

  // Locates the device object by object path (the devices map and
  // BluetoothDevice methods are by address).
  BluetoothDeviceChromeOS* GetDeviceWithPath(
      const dbus::ObjectPath& object_path);

  // Announce to observers a change in device state that is not reflected by
  // its D-Bus properties.
  void NotifyDeviceChanged(BluetoothDeviceChromeOS* device);

  // Returns the object path of the adapter.
  const dbus::ObjectPath& object_path() const { return object_path_; }

 protected:
  // BluetoothAdapter:
  virtual void RemovePairingDelegateInternal(
      device::BluetoothDevice::PairingDelegate* pairing_delegate) OVERRIDE;

 private:
  friend class BluetoothChromeOSTest;

  // typedef for callback parameters that are passed to AddDiscoverySession
  // and RemoveDiscoverySession. This is used to queue incoming requests while
  // a call to BlueZ is pending.
  typedef std::pair<base::Closure, ErrorCallback> DiscoveryCallbackPair;
  typedef std::queue<DiscoveryCallbackPair> DiscoveryCallbackQueue;

  BluetoothAdapterChromeOS();
  virtual ~BluetoothAdapterChromeOS();

  // BluetoothAdapterClient::Observer override.
  virtual void AdapterAdded(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void AdapterRemoved(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void AdapterPropertyChanged(
      const dbus::ObjectPath& object_path,
      const std::string& property_name) OVERRIDE;

  // BluetoothDeviceClient::Observer override.
  virtual void DeviceAdded(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void DeviceRemoved(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void DevicePropertyChanged(const dbus::ObjectPath& object_path,
                                     const std::string& property_name) OVERRIDE;

  // BluetoothInputClient::Observer override.
  virtual void InputPropertyChanged(const dbus::ObjectPath& object_path,
                                    const std::string& property_name) OVERRIDE;

  // BluetoothAgentServiceProvider::Delegate override.
  virtual void Released() OVERRIDE;
  virtual void RequestPinCode(const dbus::ObjectPath& device_path,
                              const PinCodeCallback& callback) OVERRIDE;
  virtual void DisplayPinCode(const dbus::ObjectPath& device_path,
                              const std::string& pincode) OVERRIDE;
  virtual void RequestPasskey(const dbus::ObjectPath& device_path,
                              const PasskeyCallback& callback) OVERRIDE;
  virtual void DisplayPasskey(const dbus::ObjectPath& device_path,
                              uint32 passkey, uint16 entered) OVERRIDE;
  virtual void RequestConfirmation(const dbus::ObjectPath& device_path,
                                   uint32 passkey,
                                   const ConfirmationCallback& callback)
      OVERRIDE;
  virtual void RequestAuthorization(const dbus::ObjectPath& device_path,
                                    const ConfirmationCallback& callback)
      OVERRIDE;
  virtual void AuthorizeService(const dbus::ObjectPath& device_path,
                                const std::string& uuid,
                                const ConfirmationCallback& callback) OVERRIDE;
  virtual void Cancel() OVERRIDE;

  // Called by dbus:: on completion of the D-Bus method call to register the
  // pairing agent.
  void OnRegisterAgent();
  void OnRegisterAgentError(const std::string& error_name,
                            const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method call to request that
  // the pairing agent be made the default.
  void OnRequestDefaultAgent();
  void OnRequestDefaultAgentError(const std::string& error_name,
                                  const std::string& error_message);

  // Internal method to obtain a BluetoothPairingChromeOS object for the device
  // with path |object_path|. Returns the existing pairing object if the device
  // already has one (usually an outgoing connection in progress) or a new
  // pairing object with the default pairing delegate if not. If no default
  // pairing object exists, NULL will be returned.
  BluetoothPairingChromeOS* GetPairing(const dbus::ObjectPath& object_path);

  // Set the tracked adapter to the one in |object_path|, this object will
  // subsequently operate on that adapter until it is removed.
  void SetAdapter(const dbus::ObjectPath& object_path);

  // Set the adapter name to one chosen from the system information.
  void SetDefaultAdapterName();

  // Remove the currently tracked adapter. IsPresent() will return false after
  // this is called.
  void RemoveAdapter();

  // Announce to observers a change in the adapter state.
  void PoweredChanged(bool powered);
  void DiscoverableChanged(bool discoverable);
  void DiscoveringChanged(bool discovering);
  void PresentChanged(bool present);

  // Called by dbus:: on completion of the discoverable property change.
  void OnSetDiscoverable(const base::Closure& callback,
                         const ErrorCallback& error_callback,
                         bool success);

  // Called by dbus:: on completion of an adapter property change.
  void OnPropertyChangeCompleted(const base::Closure& callback,
                                 const ErrorCallback& error_callback,
                                 bool success);

  // BluetoothAdapter:
  virtual void AddDiscoverySession(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void RemoveDiscoverySession(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;

  // Called by dbus:: on completion of the D-Bus method call to start discovery.
  void OnStartDiscovery(const base::Closure& callback);
  void OnStartDiscoveryError(const base::Closure& callback,
                             const ErrorCallback& error_callback,
                             const std::string& error_name,
                             const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method call to stop discovery.
  void OnStopDiscovery(const base::Closure& callback);
  void OnStopDiscoveryError(const ErrorCallback& error_callback,
                            const std::string& error_name,
                            const std::string& error_message);

  // Processes the queued discovery requests. For each DiscoveryCallbackPair in
  // the queue, this method will try to add a new discovery session. This method
  // is called whenever a pending D-Bus call to start or stop discovery has
  // ended (with either success or failure).
  void ProcessQueuedDiscoveryRequests();

  // Number of discovery sessions that have been added.
  int num_discovery_sessions_;

  // True, if there is a pending request to start or stop discovery.
  bool discovery_request_pending_;

  // List of queued requests to add new discovery sessions. While there is a
  // pending request to BlueZ to start or stop discovery, many requests from
  // within Chrome to start or stop discovery sessions may occur. We only
  // queue requests to add new sessions to be processed later. All requests to
  // remove a session while a call is pending immediately return failure. Note
  // that since BlueZ keeps its own reference count of applications that have
  // requested discovery, dropping our count to 0 won't necessarily result in
  // the controller actually stopping discovery if, for example, an application
  // other than Chrome, such as bt_console, was also used to start discovery.
  DiscoveryCallbackQueue discovery_request_queue_;

  // Object path of the adapter we track.
  dbus::ObjectPath object_path_;

  // List of observers interested in event notifications from us.
  ObserverList<device::BluetoothAdapter::Observer> observers_;

  // Instance of the D-Bus agent object used for pairing, initialized with
  // our own class as its delegate.
  scoped_ptr<BluetoothAgentServiceProvider> agent_;

  // UI thread task runner and socket thread object used to create sockets.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<device::BluetoothSocketThread> socket_thread_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_H_
