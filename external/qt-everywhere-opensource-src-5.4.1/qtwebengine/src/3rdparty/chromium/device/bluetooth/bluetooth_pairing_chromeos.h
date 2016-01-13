// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_PAIRING_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_PAIRING_CHROMEOS_H_

#include "chromeos/dbus/bluetooth_agent_service_provider.h"
#include "device/bluetooth/bluetooth_device.h"

namespace chromeos {

class BluetoothDeviceChromeOS;

// The BluetoothPairingChromeOS class encapsulates the logic for an individual
// device pairing, acting as a bridge between BluetoothAdapterChromeOS which
// communicates with the underlying Controller and Host Subsystem, and
// BluetoothDeviceChromeOS which presents the pairing logic to the application.
class BluetoothPairingChromeOS {
 public:
  BluetoothPairingChromeOS(
      BluetoothDeviceChromeOS* device,
      device::BluetoothDevice::PairingDelegate* pairing_delegate);
  ~BluetoothPairingChromeOS();

  // Indicates whether the device is currently pairing and expecting a
  // Passkey to be returned.
  bool ExpectingPasskey() const;

  // Indicates whether the device is currently pairing and expecting
  // confirmation of a displayed passkey.
  bool ExpectingConfirmation() const;

  // Requests a PIN code for the current device from the current pairing
  // delegate, the SetPinCode(), RejectPairing() and CancelPairing() method
  // calls on this object are translated into the appropriate response to
  // |callback|.
  void RequestPinCode(
      const BluetoothAgentServiceProvider::Delegate::PinCodeCallback& callback);

  // Indicates whether the device is currently pairing and expecting a
  // PIN Code to be returned.
  bool ExpectingPinCode() const;

  // Sends the PIN code |pincode| to the remote device during pairing.
  //
  // PIN Codes are generally required for Bluetooth 2.0 and earlier devices
  // for which there is no automatic pairing or special handling.
  void SetPinCode(const std::string& pincode);

  // Requests a PIN code for the current device be displayed by the current
  // pairing delegate. No response is expected from the delegate.
  void DisplayPinCode(const std::string& pincode);

  // Requests a Passkey for the current device from the current pairing
  // delegate, the SetPasskey(), RejectPairing() and CancelPairing() method
  // calls on this object are translated into the appropriate response to
  // |callback|.
  void RequestPasskey(
      const BluetoothAgentServiceProvider::Delegate::PasskeyCallback& callback);

  // Sends the Passkey |passkey| to the remote device during pairing.
  //
  // Passkeys are generally required for Bluetooth 2.1 and later devices
  // which cannot provide input or display on their own, and don't accept
  // passkey-less pairing, and are a numeric in the range 0-999999.
  void SetPasskey(uint32 passkey);

  // Requests a Passkey for the current device be displayed by the current
  // pairing delegate. No response is expected from the delegate.
  void DisplayPasskey(uint32 passkey);

  // Informs the current pairing delegate that |entered| keys have been
  // provided to the remote device since the DisplayPasskey() call. No
  // response is expected from the delegate.
  void KeysEntered(uint16 entered);

  // Requests confirmation that |passkey| is displayed on the current device
  // from the current pairing delegate. The ConfirmPairing(), RejectPairing()
  // and CancelPairing() method calls on this object are translated into the
  // appropriate response to |callback|.
  void RequestConfirmation(
      uint32 passkey,
      const BluetoothAgentServiceProvider::Delegate::ConfirmationCallback&
          callback);

  // Requests authorization that the current device be allowed to pair with
  // this device from the current pairing delegate. The ConfirmPairing(),
  // RejectPairing() and CancelPairing() method calls on this object are
  // translated into the appropriate response to |callback|.
  void RequestAuthorization(
      const BluetoothAgentServiceProvider::Delegate::ConfirmationCallback&
          callback);

  // Confirms to the remote device during pairing that a passkey provided by
  // the ConfirmPasskey() delegate call is displayed on both devices.
  void ConfirmPairing();

  // Rejects a pairing or connection request from a remote device, returns
  // false if there was no way to reject the pairing.
  bool RejectPairing();

  // Cancels a pairing or connection attempt to a remote device, returns
  // false if there was no way to cancel the pairing.
  bool CancelPairing();

  // Returns the pairing delegate being used by this pairing object.
  device::BluetoothDevice::PairingDelegate* GetPairingDelegate() const;

 private:
  // Internal method to reset the current set of callbacks because a new
  // request has arrived that supercedes them.
  void ResetCallbacks();

  // Internal method to respond to the relevant callback for a RejectPairing
  // or CancelPairing call.
  bool RunPairingCallbacks(
      BluetoothAgentServiceProvider::Delegate::Status status);

  // The underlying BluetoothDeviceChromeOS that owns this pairing context.
  BluetoothDeviceChromeOS* device_;

  // UI Pairing Delegate to make method calls on, this must live as long as
  // the object capturing the PairingContext.
  device::BluetoothDevice::PairingDelegate* pairing_delegate_;

  // Flag to indicate whether any pairing delegate method has been called
  // during pairing. Used to determine whether we need to log the
  // "no pairing interaction" metric.
  bool pairing_delegate_used_;

  // During pairing these callbacks are set to those provided by method calls
  // made on the BluetoothAdapterChromeOS instance by its respective
  // BluetoothAgentServiceProvider instance, and are called by our own
  // method calls such as SetPinCode() and SetPasskey().
  BluetoothAgentServiceProvider::Delegate::PinCodeCallback pincode_callback_;
  BluetoothAgentServiceProvider::Delegate::PasskeyCallback passkey_callback_;
  BluetoothAgentServiceProvider::Delegate::ConfirmationCallback
      confirmation_callback_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothPairingChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_PAIRING_CHROMEOS_H_
