// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chromeos/dbus/nfc_record_client.h"

#ifndef DEVICE_NFC_CHROMEOS_NDEF_RECORD_UTILS_CHROMEOS_H_
#define DEVICE_NFC_CHROMEOS_NDEF_RECORD_UTILS_CHROMEOS_H_

namespace device {
class NfcNdefRecord;
}  // namespace device;

namespace chromeos {
namespace nfc_ndef_record_utils {

// Converts the NfcNdefRecord |record| to a dictionary that can be passed to
// NfcDeviceClient::Push and NfcTagClient::Write and stores it in |out|.
// Returns false, if an error occurs during conversion.
bool NfcNdefRecordToDBusAttributes(
    const device::NfcNdefRecord* record,
    base::DictionaryValue* out);

// Converts an NDEF record D-Bus properties structure to an NfcNdefRecord
// instance by populating the instance passed in |out|. |out| must not be NULL
// and must not be already populated. Returns false, if an error occurs during
// conversion.
bool RecordPropertiesToNfcNdefRecord(
    const NfcRecordClient::Properties* properties,
    device::NfcNdefRecord* out);

}  // namespace nfc_ndef_record_utils
}  // namespace chromeos

#endif  // DEVICE_NFC_CHROMEOS_NDEF_RECORD_UTILS_CHROMEOS_H_
