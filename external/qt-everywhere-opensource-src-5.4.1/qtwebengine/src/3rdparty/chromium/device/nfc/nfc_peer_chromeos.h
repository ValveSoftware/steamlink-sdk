// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_NFC_NFC_PEER_CHROMEOS_H_
#define DEVICE_NFC_NFC_PEER_CHROMEOS_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/dbus/nfc_record_client.h"
#include "dbus/object_path.h"
#include "device/nfc/nfc_ndef_record.h"
#include "device/nfc/nfc_peer.h"

namespace chromeos {

// The NfcPeerChromeOS class implements NfcPeer for the Chrome OS platform.
class NfcPeerChromeOS : public device::NfcPeer,
                        public NfcRecordClient::Observer {
 public:
  // NfcPeer overrides.
  virtual void AddObserver(device::NfcPeer::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(device::NfcPeer::Observer* observer) OVERRIDE;
  virtual std::string GetIdentifier() const OVERRIDE;
  virtual const device::NfcNdefMessage& GetNdefMessage() const OVERRIDE;
  virtual void PushNdef(const device::NfcNdefMessage& message,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) OVERRIDE;
  virtual void StartHandover(HandoverType handover_type,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) OVERRIDE;

 private:
  friend class NfcAdapterChromeOS;

  // Mapping from D-Bus object paths to NfcNdefRecord objects.
  typedef std::map<dbus::ObjectPath, device::NfcNdefRecord*> NdefRecordMap;

  explicit NfcPeerChromeOS(const dbus::ObjectPath& object_path);
  virtual ~NfcPeerChromeOS();

  // NfcRecordClient::Observer overrides.
  virtual void RecordAdded(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void RecordRemoved(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void RecordPropertiesReceived(
      const dbus::ObjectPath& object_path) OVERRIDE;

  // Called by dbus:: on completion of the D-Bus method call to push an NDEF.
  void OnPushNdef(const base::Closure& callback);
  void OnPushNdefError(const ErrorCallback& error_callback,
                       const std::string& error_name,
                       const std::string& error_message);

  // Creates a record object for the record with object path |object_path| and
  // notifies the observers, if a record object did not already exist for it.
  void AddRecord(const dbus::ObjectPath& object_path);

  // Object path of the peer that we are currently tracking.
  dbus::ObjectPath object_path_;

  // A map containing the NDEF records that were received from the peer.
  NdefRecordMap records_;

  // Message instance that contains pointers to all created records.
  device::NfcNdefMessage message_;

  // List of observers interested in event notifications from us.
  ObserverList<device::NfcPeer::Observer> observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<NfcPeerChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NfcPeerChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_NFC_NFC_PEER_CHROMEOS_H_
