// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_NFC_NFC_ADAPTER_CHROMEOS_H_
#define DEVICE_NFC_NFC_ADAPTER_CHROMEOS_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/dbus/nfc_adapter_client.h"
#include "chromeos/dbus/nfc_device_client.h"
#include "chromeos/dbus/nfc_tag_client.h"
#include "dbus/object_path.h"
#include "device/nfc/nfc_adapter.h"

namespace device {

class NfcAdapterFactory;

}  // namespace device

namespace chromeos {

// The NfcAdapterChromeOS class implements NfcAdapter for the Chrome OS
// platform.
class NfcAdapterChromeOS : public device::NfcAdapter,
                           public chromeos::NfcAdapterClient::Observer,
                           public chromeos::NfcDeviceClient::Observer,
                           public chromeos::NfcTagClient::Observer {
 public:
  // NfcAdapter overrides.
  virtual void AddObserver(NfcAdapter::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(NfcAdapter::Observer* observer) OVERRIDE;
  virtual bool IsPresent() const OVERRIDE;
  virtual bool IsPowered() const OVERRIDE;
  virtual bool IsPolling() const OVERRIDE;
  virtual bool IsInitialized() const OVERRIDE;
  virtual void SetPowered(bool powered,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE;
  virtual void StartPolling(const base::Closure& callback,
                            const ErrorCallback& error_callback) OVERRIDE;
  virtual void StopPolling(const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE;

 private:
  friend class device::NfcAdapterFactory;
  friend class NfcChromeOSTest;

  NfcAdapterChromeOS();
  virtual ~NfcAdapterChromeOS();

  // NfcAdapterClient::Observer overrides.
  virtual void AdapterAdded(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void AdapterRemoved(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void AdapterPropertyChanged(
      const dbus::ObjectPath& object_path,
      const std::string& property_name) OVERRIDE;

  // NfcDeviceClient::Observer overrides.
  virtual void DeviceAdded(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void DeviceRemoved(const dbus::ObjectPath& object_path) OVERRIDE;

  // NfcTagClient::Observer overrides.
  virtual void TagAdded(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void TagRemoved(const dbus::ObjectPath& object_path) OVERRIDE;

  // Set the tracked adapter to the one in |object_path|, this object will
  // subsequently operate on that adapter until it is removed.
  void SetAdapter(const dbus::ObjectPath& object_path);

  // Remove the currently tracked adapter. IsPresent() will return false after
  // this is called.
  void RemoveAdapter();

  // Announce to observers a change in the adapter state.
  void PoweredChanged(bool powered);
  void PollingChanged(bool polling);
  void PresentChanged(bool present);

  // Called by dbus:: on completion of the powered property change.
  void OnSetPowered(const base::Closure& callback,
                    const ErrorCallback& error_callback,
                    bool success);

  // Called by dbus:: on completion of the D-Bus method call to start polling.
  void OnStartPolling(const base::Closure& callback);
  void OnStartPollingError(const ErrorCallback& error_callback,
                           const std::string& error_name,
                           const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method call to stop polling.
  void OnStopPolling(const base::Closure& callback);
  void OnStopPollingError(const ErrorCallback& error_callback,
                          const std::string& error_name,
                          const std::string& error_message);

  // Object path of the adapter that we are currently tracking.
  dbus::ObjectPath object_path_;

  // List of observers interested in event notifications from us.
  ObserverList<device::NfcAdapter::Observer> observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<NfcAdapterChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NfcAdapterChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_NFC_NFC_ADAPTER_CHROMEOS_H_
