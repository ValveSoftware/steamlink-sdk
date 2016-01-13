// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_NFC_NFC_TAG_TECHNOLOGY_CHROMEOS_H_
#define DEVICE_NFC_NFC_TAG_TECHNOLOGY_CHROMEOS_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/dbus/nfc_record_client.h"
#include "device/nfc/nfc_tag_technology.h"

namespace chromeos {

class NfcTagChromeOS;

// The NfcNdefTagTechnologyChromeOS class implements
// device::NfcNdefTagTechnology for the Chrome OS platform. The lifetime of an
// instance of this class must be tied to an instance of NfcTagChromeOS.
// Instances of this class must never outlast the owning NfcTagChromeOS
// instance.
class NfcNdefTagTechnologyChromeOS : public device::NfcNdefTagTechnology,
                                     public NfcRecordClient::Observer {
 public:
  virtual ~NfcNdefTagTechnologyChromeOS();

  // device::NfcNdefTagTechnology overrides.
  virtual void AddObserver(device::NfcNdefTagTechnology::Observer* observer)
    OVERRIDE;
  virtual void RemoveObserver(device::NfcNdefTagTechnology::Observer* observer)
    OVERRIDE;
  virtual const device::NfcNdefMessage& GetNdefMessage() const OVERRIDE;
  virtual void WriteNdef(const device::NfcNdefMessage& message,
                         const base::Closure& callback,
                         const ErrorCallback& error_callback) OVERRIDE;

  // NfcRecordClient::Observer overrides.
  virtual void RecordAdded(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void RecordRemoved(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void RecordPropertiesReceived(
      const dbus::ObjectPath& object_path) OVERRIDE;

 private:
  friend class NfcTagChromeOS;

  // Mapping from D-Bus object paths to NfcNdefRecord objects.
  typedef std::map<dbus::ObjectPath, device::NfcNdefRecord*> NdefRecordMap;

  explicit NfcNdefTagTechnologyChromeOS(NfcTagChromeOS* tag);

  // Called by dbus:: on completion of the D-Bus method call to write an NDEF.
  void OnWriteNdefMessage(const base::Closure& callback);
  void OnWriteNdefMessageError(const ErrorCallback& error_callback,
                               const std::string& error_name,
                               const std::string& error_message);

  // Creates a record object for the record with object path |object_path| and
  // notifies the observers, if a record object did not already exist for it.
  void AddRecord(const dbus::ObjectPath& object_path);

  // A map containing the NDEF records that were received from the tag.
  NdefRecordMap records_;

  // Message instance that contains pointers to all created records that are
  // in |records_|. This is mainly used as the cached return value for
  // GetNdefMessage().
  device::NfcNdefMessage message_;

  // List of observers interested in event notifications from us.
  ObserverList<device::NfcNdefTagTechnology::Observer> observers_;

  // D-Bus object path of the remote tag or device that this object operates
  // on.
  dbus::ObjectPath object_path_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<NfcNdefTagTechnologyChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NfcNdefTagTechnologyChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_NFC_NFC_TAG_TECHNOLOGY_CHROMEOS_H_
