// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "net/log/net_log.h"

namespace base {
class BinaryValue;
}

namespace device {

class BluetoothAdapter;
class BluetoothGattConnection;
class BluetoothRemoteGattService;
class BluetoothSocket;
class BluetoothUUID;

// BluetoothDevice represents a remote Bluetooth device, both its properties and
// capabilities as discovered by a local adapter and actions that may be
// performed on the remove device such as pairing, connection and disconnection.
//
// The class is instantiated and managed by the BluetoothAdapter class
// and pointers should only be obtained from that class and not cached,
// instead use the GetAddress() method as a unique key for a device.
//
// Since the lifecycle of BluetoothDevice instances is managed by
// BluetoothAdapter, that class rather than this provides observer methods
// for devices coming and going, as well as properties being updated.
class DEVICE_BLUETOOTH_EXPORT BluetoothDevice {
 public:
  // Possible values that may be returned by GetVendorIDSource(),
  // indicating different organisations that allocate the identifiers returned
  // by GetVendorID().
  enum VendorIDSource {
    VENDOR_ID_UNKNOWN,
    VENDOR_ID_BLUETOOTH,
    VENDOR_ID_USB,
    VENDOR_ID_MAX_VALUE = VENDOR_ID_USB
  };

  // Possible values that may be returned by GetDeviceType(), representing
  // different types of bluetooth device that we support or are aware of
  // decoded from the bluetooth class information.
  enum DeviceType {
    DEVICE_UNKNOWN,
    DEVICE_COMPUTER,
    DEVICE_PHONE,
    DEVICE_MODEM,
    DEVICE_AUDIO,
    DEVICE_CAR_AUDIO,
    DEVICE_VIDEO,
    DEVICE_PERIPHERAL,
    DEVICE_JOYSTICK,
    DEVICE_GAMEPAD,
    DEVICE_KEYBOARD,
    DEVICE_MOUSE,
    DEVICE_TABLET,
    DEVICE_KEYBOARD_MOUSE_COMBO
  };

  // The value returned if the RSSI or transmit power cannot be read.
  static const int kUnknownPower = 127;
  // The value returned if the appearance is not present.
  static const uint16_t kAppearanceNotPresent = 0xffc0;

  struct DEVICE_BLUETOOTH_EXPORT ConnectionInfo {
    int rssi;
    int transmit_power;
    int max_transmit_power;

    ConnectionInfo();
    ConnectionInfo(int rssi, int transmit_power, int max_transmit_power);
    ~ConnectionInfo();
  };

  // Possible errors passed back to an error callback function in case of a
  // failed call to Connect().
  enum ConnectErrorCode {
    ERROR_ATTRIBUTE_LENGTH_INVALID,
    ERROR_AUTH_CANCELED,
    ERROR_AUTH_FAILED,
    ERROR_AUTH_REJECTED,
    ERROR_AUTH_TIMEOUT,
    ERROR_CONNECTION_CONGESTED,
    ERROR_FAILED,
    ERROR_INPROGRESS,
    ERROR_INSUFFICIENT_ENCRYPTION,
    ERROR_OFFSET_INVALID,
    ERROR_READ_NOT_PERMITTED,
    ERROR_REQUEST_NOT_SUPPORTED,
    ERROR_UNKNOWN,
    ERROR_UNSUPPORTED_DEVICE,
    ERROR_WRITE_NOT_PERMITTED,
    NUM_CONNECT_ERROR_CODES  // Keep as last enum.
  };

  typedef std::vector<BluetoothUUID> UUIDList;

  // Interface for negotiating pairing of bluetooth devices.
  class PairingDelegate {
   public:
    virtual ~PairingDelegate() {}

    // This method will be called when the Bluetooth daemon requires a
    // PIN Code for authentication of the device |device|, the delegate should
    // obtain the code from the user and call SetPinCode() on the device to
    // provide it, or RejectPairing() or CancelPairing() to reject or cancel
    // the request.
    //
    // PIN Codes are generally required for Bluetooth 2.0 and earlier devices
    // for which there is no automatic pairing or special handling.
    virtual void RequestPinCode(BluetoothDevice* device) = 0;

    // This method will be called when the Bluetooth daemon requires a
    // Passkey for authentication of the device |device|, the delegate should
    // obtain the passkey from the user (a numeric in the range 0-999999) and
    // call SetPasskey() on the device to provide it, or RejectPairing() or
    // CancelPairing() to reject or cancel the request.
    //
    // Passkeys are generally required for Bluetooth 2.1 and later devices
    // which cannot provide input or display on their own, and don't accept
    // passkey-less pairing.
    virtual void RequestPasskey(BluetoothDevice* device) = 0;

    // This method will be called when the Bluetooth daemon requires that the
    // user enter the PIN code |pincode| into the device |device| so that it
    // may be authenticated.
    //
    // This is used for Bluetooth 2.0 and earlier keyboard devices, the
    // |pincode| will always be a six-digit numeric in the range 000000-999999
    // for compatibility with later specifications.
    virtual void DisplayPinCode(BluetoothDevice* device,
                                const std::string& pincode) = 0;

    // This method will be called when the Bluetooth daemon requires that the
    // user enter the Passkey |passkey| into the device |device| so that it
    // may be authenticated.
    //
    // This is used for Bluetooth 2.1 and later devices that support input
    // but not display, such as keyboards. The Passkey is a numeric in the
    // range 0-999999 and should be always presented zero-padded to six
    // digits.
    virtual void DisplayPasskey(BluetoothDevice* device, uint32_t passkey) = 0;

    // This method will be called when the Bluetooth daemon gets a notification
    // of a key entered on the device |device| while pairing with the device
    // using a PIN code or a Passkey.
    //
    // This method will be called only after DisplayPinCode() or
    // DisplayPasskey() method is called, but is not warranted to be called
    // on every pairing process that requires a PIN code or a Passkey because
    // some device may not support this feature.
    //
    // The |entered| value describes the number of keys entered so far,
    // including the last [enter] key. A first call to KeysEntered() with
    // |entered| as 0 will be sent when the device supports this feature.
    virtual void KeysEntered(BluetoothDevice* device, uint32_t entered) = 0;

    // This method will be called when the Bluetooth daemon requires that the
    // user confirm that the Passkey |passkey| is displayed on the screen
    // of the device |device| so that it may be authenticated. The delegate
    // should display to the user and ask for confirmation, then call
    // ConfirmPairing() on the device to confirm, RejectPairing() on the device
    // to reject or CancelPairing() on the device to cancel authentication
    // for any other reason.
    //
    // This is used for Bluetooth 2.1 and later devices that support display,
    // such as other computers or phones. The Passkey is a numeric in the
    // range 0-999999 and should be always present zero-padded to six
    // digits.
    virtual void ConfirmPasskey(BluetoothDevice* device, uint32_t passkey) = 0;

    // This method will be called when the Bluetooth daemon requires that a
    // pairing request, usually only incoming, using the just-works model is
    // authorized. The delegate should decide whether the user should confirm
    // or not, then call ConfirmPairing() on the device to confirm the pairing
    // (whether by user action or by default), RejectPairing() on the device to
    // reject or CancelPairing() on the device to cancel authorization for
    // any other reason.
    virtual void AuthorizePairing(BluetoothDevice* device) = 0;
  };

  virtual ~BluetoothDevice();

  // Returns the Bluetooth class of the device, used by GetDeviceType()
  // and metrics logging,
  virtual uint32_t GetBluetoothClass() const = 0;

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
  // Returns the transport type of the device. Some devices only support one
  // of BR/EDR or LE, and some support both.
  virtual BluetoothTransport GetType() const = 0;
#endif

  // Returns the identifier of the bluetooth device.
  virtual std::string GetIdentifier() const;

  // Returns the Bluetooth of address the device. This should be used as
  // a unique key to identify the device and copied where needed.
  virtual std::string GetAddress() const = 0;

  // Returns the allocation source of the identifier returned by GetVendorID(),
  // where available, or VENDOR_ID_UNKNOWN where not.
  virtual VendorIDSource GetVendorIDSource() const = 0;

  // Returns the Vendor ID of the device, where available.
  virtual uint16_t GetVendorID() const = 0;

  // Returns the Product ID of the device, where available.
  virtual uint16_t GetProductID() const = 0;

  // Returns the Device ID of the device, typically the release or version
  // number in BCD format, where available.
  virtual uint16_t GetDeviceID() const = 0;

  // Returns the appearance of the device.
  virtual uint16_t GetAppearance() const = 0;

  // Returns the name of the device suitable for displaying, this may
  // be a synthesized string containing the address and localized type name
  // if the device has no obtained name.
  virtual base::string16 GetNameForDisplay() const;

  // Returns the type of the device, limited to those we support or are
  // aware of, by decoding the bluetooth class information. The returned
  // values are unique, and do not overlap, so DEVICE_KEYBOARD is not also
  // DEVICE_PERIPHERAL.
  //
  // Returns the type of the device, limited to those we support or are aware
  // of, by decoding the bluetooth class information for Classic devices or
  // by decoding the device's appearance for LE devices. For example,
  // Microsoft Universal Foldable Keyboard only advertises the appearance.
  DeviceType GetDeviceType() const;

  // Indicates whether the device is known to support pairing based on its
  // device class and address.
  bool IsPairable() const;

  // Indicates whether the device is paired with the adapter.
  virtual bool IsPaired() const = 0;

  // Indicates whether the device is currently connected to the adapter.
  // Note that if IsConnected() is true, does not imply that the device is
  // connected to any application or service. If the device is not paired, it
  // could be still connected to the adapter for other reason, for example, to
  // request the adapter's SDP records. The same holds for paired devices, since
  // they could be connected to the adapter but not to an application.
  virtual bool IsConnected() const = 0;

  // Indicates whether an active GATT connection exists to the device.
  virtual bool IsGattConnected() const = 0;

  // Indicates whether the paired device accepts connections initiated from the
  // adapter. This value is undefined for unpaired devices.
  virtual bool IsConnectable() const = 0;

  // Indicates whether there is a call to Connect() ongoing. For this attribute,
  // we consider a call is ongoing if none of the callbacks passed to Connect()
  // were called after the corresponding call to Connect().
  virtual bool IsConnecting() const = 0;

  // Indicates whether the device can be trusted, based on device properties,
  // such as vendor and product id.
  bool IsTrustable() const;

  // Returns the set of UUIDs that this device supports. For classic Bluetooth
  // devices this data is collected from both the EIR data and SDP tables,
  // for Low Energy devices this data is collected from AD and GATT primary
  // services, for dual mode devices this may be collected from both./
  virtual UUIDList GetUUIDs() const = 0;

  // The received signal strength, in dBm. This field is avaliable and valid
  // only during discovery. If not during discovery, or RSSI wasn't reported,
  // this method will return |kUnknownPower|.
  virtual int16_t GetInquiryRSSI() const = 0;

  // The transmitted power level. This field is avaliable only for LE devices
  // that include this field in AD. It is avaliable and valid only during
  // discovery. If not during discovery, or TxPower wasn't reported, this
  // method will return |kUnknownPower|.
  virtual int16_t GetInquiryTxPower() const = 0;

  // The ErrorCallback is used for methods that can fail in which case it
  // is called, in the success case the callback is simply not called.
  typedef base::Callback<void()> ErrorCallback;

  // The ConnectErrorCallback is used for methods that can fail with an error,
  // passed back as an error code argument to this callback.
  // In the success case this callback is not called.
  typedef base::Callback<void(enum ConnectErrorCode)> ConnectErrorCallback;

  typedef base::Callback<void(const ConnectionInfo&)> ConnectionInfoCallback;

  // Indicates whether the device is currently pairing and expecting a
  // PIN Code to be returned.
  virtual bool ExpectingPinCode() const = 0;

  // Indicates whether the device is currently pairing and expecting a
  // Passkey to be returned.
  virtual bool ExpectingPasskey() const = 0;

  // Indicates whether the device is currently pairing and expecting
  // confirmation of a displayed passkey.
  virtual bool ExpectingConfirmation() const = 0;

  // Returns the RSSI and TX power of the active connection to the device:
  //
  // The RSSI indicates the power present in the received radio signal, measured
  // in dBm, to a resolution of 1dBm. Larger (typically, less negative) values
  // indicate a stronger signal.
  //
  // The transmit power indicates the strength of the signal broadcast from the
  // host's Bluetooth antenna when communicating with the device, measured in
  // dBm, to a resolution of 1dBm. Larger (typically, less negative) values
  // indicate a stronger signal.
  //
  // If the device isn't connected, then the ConnectionInfo struct passed into
  // the callback will be populated with |kUnknownPower|.
  virtual void GetConnectionInfo(const ConnectionInfoCallback& callback) = 0;

  // Initiates a connection to the device, pairing first if necessary.
  //
  // Method calls will be made on the supplied object |pairing_delegate|
  // to indicate what display, and in response should make method calls
  // back to the device object. Not all devices require user responses
  // during pairing, so it is normal for |pairing_delegate| to receive no
  // calls. To explicitly force a low-security connection without bonding,
  // pass NULL, though this is ignored if the device is already paired.
  //
  // If the request fails, |error_callback| will be called; otherwise,
  // |callback| is called when the request is complete.
  // After calling Connect, CancelPairing should be called to cancel the pairing
  // process and release the pairing delegate if user cancels the pairing and
  // closes the pairing UI.
  virtual void Connect(PairingDelegate* pairing_delegate,
                       const base::Closure& callback,
                       const ConnectErrorCallback& error_callback) = 0;

  // Pairs the device. This method triggers pairing unconditially, i.e. it
  // ignores the |IsPaired()| value.
  //
  // In most cases |Connect()| should be preferred. This method is only
  // implemented on ChromeOS and Linux.
  virtual void Pair(PairingDelegate* pairing_delegate,
                    const base::Closure& callback,
                    const ConnectErrorCallback& error_callback);

  // Sends the PIN code |pincode| to the remote device during pairing.
  //
  // PIN Codes are generally required for Bluetooth 2.0 and earlier devices
  // for which there is no automatic pairing or special handling.
  virtual void SetPinCode(const std::string& pincode) = 0;

  // Sends the Passkey |passkey| to the remote device during pairing.
  //
  // Passkeys are generally required for Bluetooth 2.1 and later devices
  // which cannot provide input or display on their own, and don't accept
  // passkey-less pairing, and are a numeric in the range 0-999999.
  virtual void SetPasskey(uint32_t passkey) = 0;

  // Confirms to the remote device during pairing that a passkey provided by
  // the ConfirmPasskey() delegate call is displayed on both devices.
  virtual void ConfirmPairing() = 0;

  // Rejects a pairing or connection request from a remote device.
  virtual void RejectPairing() = 0;

  // Cancels a pairing or connection attempt to a remote device, releasing
  // the pairing delegate.
  virtual void CancelPairing() = 0;

  // Disconnects the device, terminating the low-level ACL connection
  // and any application connections using it. Link keys and other pairing
  // information are not discarded, and the device object is not deleted.
  // If the request fails, |error_callback| will be called; otherwise,
  // |callback| is called when the request is complete.
  virtual void Disconnect(const base::Closure& callback,
                          const ErrorCallback& error_callback) = 0;

  // Disconnects the device, terminating the low-level ACL connection
  // and any application connections using it, and then discards link keys
  // and other pairing information. The device object remains valid until
  // returning from the calling function, after which it should be assumed to
  // have been deleted. If the request fails, |error_callback| will be called.
  // On success |callback| will be invoked, but note that the BluetoothDevice
  // object will have been deleted at that point.
  virtual void Forget(const base::Closure& callback,
                      const ErrorCallback& error_callback) = 0;

  // Attempts to initiate an outgoing L2CAP or RFCOMM connection to the
  // advertised service on this device matching |uuid|, performing an SDP lookup
  // if necessary to determine the correct protocol and channel for the
  // connection. |callback| will be called on a successful connection with a
  // BluetoothSocket instance that is to be owned by the receiver.
  // |error_callback| will be called on failure with a message indicating the
  // cause.
  typedef base::Callback<void(scoped_refptr<BluetoothSocket>)>
      ConnectToServiceCallback;
  typedef base::Callback<void(const std::string& message)>
      ConnectToServiceErrorCallback;
  virtual void ConnectToService(
      const BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) = 0;

  // Attempts to initiate an insecure outgoing L2CAP or RFCOMM connection to the
  // advertised service on this device matching |uuid|, performing an SDP lookup
  // if necessary to determine the correct protocol and channel for the
  // connection. Unlike ConnectToService, the outgoing connection will request
  // no bonding rather than general bonding. |callback| will be called on a
  // successful connection with a BluetoothSocket instance that is to be owned
  // by the receiver. |error_callback| will be called on failure with a message
  // indicating the cause.
  virtual void ConnectToServiceInsecurely(
    const device::BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) = 0;

  // Opens a new GATT connection to this device. On success, a new
  // BluetoothGattConnection will be handed to the caller via |callback|. On
  // error, |error_callback| will be called. The connection will be kept alive,
  // as long as there is at least one active GATT connection. In the case that
  // the underlying connection gets terminated, either due to a call to
  // BluetoothDevice::Disconnect or other unexpected circumstances, the
  // returned BluetoothGattConnection will be automatically marked as inactive.
  // To monitor the state of the connection, observe the
  // BluetoothAdapter::Observer::DeviceChanged method.
  typedef base::Callback<void(std::unique_ptr<BluetoothGattConnection>)>
      GattConnectionCallback;
  virtual void CreateGattConnection(const GattConnectionCallback& callback,
                                    const ConnectErrorCallback& error_callback);

  // Set the gatt services discovery complete flag for this device.
  virtual void SetGattServicesDiscoveryComplete(bool complete);

  // Indicates whether service discovery is complete for this device.
  virtual bool IsGattServicesDiscoveryComplete() const;

  // Returns the list of discovered GATT services.
  virtual std::vector<BluetoothRemoteGattService*> GetGattServices() const;

  // Returns the GATT service with device-specific identifier |identifier|.
  // Returns NULL, if no such service exists.
  virtual BluetoothRemoteGattService* GetGattService(
      const std::string& identifier) const;

  // Returns service data of a service given its UUID.
  virtual base::BinaryValue* GetServiceData(BluetoothUUID serviceUUID) const;

  // Returns the list UUIDs of services that have service data.
  virtual UUIDList GetServiceDataUUIDs() const;

  // Returns the |address| in the canonical format: XX:XX:XX:XX:XX:XX, where
  // each 'X' is a hex digit.  If the input |address| is invalid, returns an
  // empty string.
  static std::string CanonicalizeAddress(const std::string& address);

  // Return the timestamp for when this device was last seen.
  base::Time GetLastUpdateTime() const { return last_update_time_; }

  // Update the last time this device was seen.
  void UpdateTimestamp();

  // Return associated BluetoothAdapter.
  BluetoothAdapter* GetAdapter() { return adapter_; }

 protected:
  // BluetoothGattConnection is a friend to call Add/RemoveGattConnection.
  friend BluetoothGattConnection;
  FRIEND_TEST_ALL_PREFIXES(
      BluetoothTest,
      BluetoothGattConnection_DisconnectGatt_SimulateConnect);
  FRIEND_TEST_ALL_PREFIXES(
      BluetoothTest,
      BluetoothGattConnection_DisconnectGatt_SimulateDisconnect);
  FRIEND_TEST_ALL_PREFIXES(BluetoothTest,
                           BluetoothGattConnection_ErrorAfterConnection);
  FRIEND_TEST_ALL_PREFIXES(BluetoothTest,
                           BluetoothGattConnection_DisconnectGatt_Cleanup);
  FRIEND_TEST_ALL_PREFIXES(BluetoothTest, GetDeviceName_NullName);
  FRIEND_TEST_ALL_PREFIXES(BluetoothTest, RemoveOutdatedDevices);
  FRIEND_TEST_ALL_PREFIXES(BluetoothTest, RemoveOutdatedDeviceGattConnect);

  BluetoothDevice(BluetoothAdapter* adapter);

  // Returns the internal name of the Bluetooth device, used by
  // GetNameForDisplay().
  virtual std::string GetDeviceName() const = 0;

  // Implements platform specific operations to initiate a GATT connection.
  // Subclasses must also call DidConnectGatt, DidFailToConnectGatt, or
  // DidDisconnectGatt immediately or asynchronously as the connection state
  // changes.
  virtual void CreateGattConnectionImpl() = 0;

  // Disconnects GATT connection on platforms that maintain a specific GATT
  // connection.
  virtual void DisconnectGatt() = 0;

  // Calls any pending callbacks for CreateGattConnection based on result of
  // subclasses actions initiated in CreateGattConnectionImpl or related
  // disconnection events. These may be called at any time, even multiple times,
  // to ensure a change in platform state is correctly tracked.
  //
  // Under normal behavior it is expected that after CreateGattConnectionImpl
  // an platform will call DidConnectGatt or DidFailToConnectGatt, but not
  // DidDisconnectGatt.
  void DidConnectGatt();
  void DidFailToConnectGatt(ConnectErrorCode);
  void DidDisconnectGatt();

  // Tracks BluetoothGattConnection instances that act as a reference count
  // keeping the GATT connection open. Instances call Add/RemoveGattConnection
  // at creation & deletion.
  void AddGattConnection(BluetoothGattConnection*);
  void RemoveGattConnection(BluetoothGattConnection*);

  // Clears the list of service data.
  void ClearServiceData();

  // Set the data of a given service designated by its UUID.
  void SetServiceData(BluetoothUUID serviceUUID, const char* buffer,
                      size_t size);

  // Update last_update_time_ so that the device appears as expired.
  void SetAsExpiredForTesting();

  // Raw pointer to adapter owning this device object. Subclasses use platform
  // specific pointers via adapter_.
  BluetoothAdapter* adapter_;

  // Callbacks for pending success and error result of CreateGattConnection.
  std::vector<GattConnectionCallback> create_gatt_connection_success_callbacks_;
  std::vector<ConnectErrorCallback> create_gatt_connection_error_callbacks_;

  // BluetoothGattConnection objects keeping the GATT connection alive.
  std::set<BluetoothGattConnection*> gatt_connections_;

  // Mapping from the platform-specific GATT service identifiers to
  // BluetoothRemoteGattService objects.
  typedef base::ScopedPtrHashMap<std::string,
                                 std::unique_ptr<BluetoothRemoteGattService>>
      GattServiceMap;
  GattServiceMap gatt_services_;
  bool gatt_services_discovery_complete_;

  // Mapping from service UUID represented as a std::string of a bluetooth
  // service to
  // the specific data. The data is stored as BinaryValue.
  std::unique_ptr<base::DictionaryValue> services_data_;

  // Timestamp for when an advertisement was last seen.
  base::Time last_update_time_;

 private:
  // Returns a localized string containing the device's bluetooth address and
  // a device type for display when |name_| is empty.
  base::string16 GetAddressWithLocalizedDeviceTypeName() const;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_H_
