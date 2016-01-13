// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/nfc/nfc_peer_chromeos.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/stl_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/nfc_device_client.h"
#include "device/nfc/nfc_ndef_record_utils_chromeos.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using device::NfcNdefMessage;
using device::NfcNdefRecord;

namespace chromeos {

namespace {

typedef std::vector<dbus::ObjectPath> ObjectPathVector;

}  // namespace

NfcPeerChromeOS::NfcPeerChromeOS(const dbus::ObjectPath& object_path)
    : object_path_(object_path),
      weak_ptr_factory_(this) {
  // Create record objects for all records that were received before.
  const ObjectPathVector& records =
      DBusThreadManager::Get()->GetNfcRecordClient()->
          GetRecordsForDevice(object_path_);
  for (ObjectPathVector::const_iterator iter = records.begin();
       iter != records.end(); ++iter) {
    AddRecord(*iter);
  }
  DBusThreadManager::Get()->GetNfcRecordClient()->AddObserver(this);
}

NfcPeerChromeOS::~NfcPeerChromeOS() {
  DBusThreadManager::Get()->GetNfcRecordClient()->RemoveObserver(this);
  STLDeleteValues(&records_);
}

void NfcPeerChromeOS::AddObserver(device::NfcPeer::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void NfcPeerChromeOS::RemoveObserver(device::NfcPeer::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

std::string NfcPeerChromeOS::GetIdentifier() const {
  return object_path_.value();
}

const NfcNdefMessage& NfcPeerChromeOS::GetNdefMessage() const {
  return message_;
}

void NfcPeerChromeOS::PushNdef(const NfcNdefMessage& message,
                               const base::Closure& callback,
                               const ErrorCallback& error_callback) {
  if (message.records().empty()) {
    LOG(ERROR) << "Given NDEF message is empty. Cannot push it.";
    error_callback.Run();
    return;
  }
  // TODO(armansito): neard currently supports pushing only one NDEF record
  // to a remote device and won't support multiple records until 0.15. Until
  // then, report failure if |message| contains more than one record.
  if (message.records().size() > 1) {
    LOG(ERROR) << "Currently, pushing only 1 NDEF record is supported.";
    error_callback.Run();
    return;
  }
  const NfcNdefRecord* record = message.records()[0];
  base::DictionaryValue attributes;
  if (!nfc_ndef_record_utils::NfcNdefRecordToDBusAttributes(
          record, &attributes)) {
    LOG(ERROR) << "Failed to extract NDEF record fields for NDEF push.";
    error_callback.Run();
    return;
  }
  DBusThreadManager::Get()->GetNfcDeviceClient()->Push(
      object_path_,
      attributes,
      base::Bind(&NfcPeerChromeOS::OnPushNdef,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&NfcPeerChromeOS::OnPushNdefError,
                 weak_ptr_factory_.GetWeakPtr(),
                 error_callback));
}

void NfcPeerChromeOS::StartHandover(HandoverType handover_type,
                                    const base::Closure& callback,
                                    const ErrorCallback& error_callback) {
  // TODO(armansito): Initiating handover with a peer is currently not
  // supported. For now, return an error immediately.
  LOG(ERROR) << "NFC Handover currently not supported.";
  error_callback.Run();
}

void NfcPeerChromeOS::RecordAdded(const dbus::ObjectPath& object_path) {
  // Don't create the record object yet. Instead, wait until all record
  // properties have been received and contruct the object and notify observers
  // then.
  VLOG(1) << "Record added: " << object_path.value() << ". Waiting until "
          << "all properties have been fetched to create record object.";
}

void NfcPeerChromeOS::RecordRemoved(const dbus::ObjectPath& object_path) {
  NdefRecordMap::iterator iter = records_.find(object_path);
  if (iter == records_.end())
    return;
  VLOG(1) << "Lost remote NDEF record object: " << object_path.value()
          << ", removing record.";
  NfcNdefRecord* record = iter->second;
  message_.RemoveRecord(record);
  delete record;
  records_.erase(iter);
}

void NfcPeerChromeOS::RecordPropertiesReceived(
    const dbus::ObjectPath& object_path) {
  VLOG(1) << "Record properties received for: " << object_path.value();

  // Check if the found record belongs to this device.
  bool record_found = false;
  const ObjectPathVector& records =
      DBusThreadManager::Get()->GetNfcRecordClient()->
          GetRecordsForDevice(object_path_);
  for (ObjectPathVector::const_iterator iter = records.begin();
       iter != records.end(); ++iter) {
    if (*iter == object_path) {
      record_found = true;
      break;
    }
  }
  if (!record_found) {
    VLOG(1) << "Record \"" << object_path.value() << "\" doesn't belong to this"
            << " device. Ignoring.";
    return;
  }

  AddRecord(object_path);
}

void NfcPeerChromeOS::OnPushNdef(const base::Closure& callback) {
  callback.Run();
}

void NfcPeerChromeOS::OnPushNdefError(const ErrorCallback& error_callback,
                                      const std::string& error_name,
                                      const std::string& error_message) {
  LOG(ERROR) << object_path_.value() << ": Failed to Push NDEF message: "
             << error_name << ": " << error_message;
  error_callback.Run();
}

void NfcPeerChromeOS::AddRecord(const dbus::ObjectPath& object_path) {
  // Ignore this call if an entry for this record already exists.
  if (records_.find(object_path) != records_.end()) {
    VLOG(1) << "Record object for remote \"" << object_path.value()
            << "\" already exists.";
    return;
  }

  NfcRecordClient::Properties* record_properties =
      DBusThreadManager::Get()->GetNfcRecordClient()->
          GetProperties(object_path);
  DCHECK(record_properties);

  NfcNdefRecord* record = new NfcNdefRecord();
  if (!nfc_ndef_record_utils::RecordPropertiesToNfcNdefRecord(
          record_properties, record)) {
    LOG(ERROR) << "Failed to create record object for record with object "
               << "path \"" << object_path.value() << "\"";
    delete record;
    return;
  }

  message_.AddRecord(record);
  records_[object_path] = record;
  FOR_EACH_OBSERVER(NfcPeer::Observer, observers_,
                    RecordReceived(this, record));
}

}  // namespace chromeos
