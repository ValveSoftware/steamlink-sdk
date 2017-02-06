// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/nfc/nfc_ndef_record_utils_chromeos.h"

#include <memory>

#include "base/logging.h"
#include "device/nfc/nfc_ndef_record.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using device::NfcNdefRecord;

namespace chromeos {
namespace nfc_ndef_record_utils {

namespace {

// Maps the NDEF type |type| as returned by neard to the corresponding
// device::NfcNdefRecord::Type value.
NfcNdefRecord::Type DBusRecordTypeValueToNfcNdefRecordType(
    const std::string& type) {
  if (type == nfc_record::kTypeSmartPoster)
    return NfcNdefRecord::kTypeSmartPoster;
  if (type == nfc_record::kTypeText)
    return NfcNdefRecord::kTypeText;
  if (type == nfc_record::kTypeUri)
    return NfcNdefRecord::kTypeURI;
  if (type == nfc_record::kTypeHandoverRequest)
    return NfcNdefRecord::kTypeHandoverRequest;
  if (type == nfc_record::kTypeHandoverSelect)
    return NfcNdefRecord::kTypeHandoverSelect;
  if (type == nfc_record::kTypeHandoverCarrier)
    return NfcNdefRecord::kTypeHandoverCarrier;
  return NfcNdefRecord::kTypeUnknown;
}

// Maps the NDEF type |type| given as a NFC C++ API enumeration to the
// corresponding string value defined by neard.
std::string NfcRecordTypeEnumToPropertyValue(NfcNdefRecord::Type type) {
  switch (type) {
    case NfcNdefRecord::kTypeSmartPoster:
      return nfc_record::kTypeSmartPoster;
    case NfcNdefRecord::kTypeText:
      return nfc_record::kTypeText;
    case NfcNdefRecord::kTypeURI:
      return nfc_record::kTypeUri;
    case NfcNdefRecord::kTypeHandoverRequest:
      return nfc_record::kTypeHandoverRequest;
    case NfcNdefRecord::kTypeHandoverSelect:
      return nfc_record::kTypeHandoverSelect;
    case NfcNdefRecord::kTypeHandoverCarrier:
      return nfc_record::kTypeHandoverCarrier;
    default:
      return "";
  }
}

// Maps the field name |field_name| given as defined in NfcNdefRecord to the
// Neard Record D-Bus property name. This handles all fields except for
// NfcNdefRecord::kFieldTitles and NfcNdefRecord::kFieldAction, which need
// special handling.
std::string NdefRecordFieldToDBusProperty(const std::string& field_name) {
  if (field_name == NfcNdefRecord::kFieldEncoding)
    return nfc_record::kEncodingProperty;
  if (field_name == NfcNdefRecord::kFieldLanguageCode)
    return nfc_record::kLanguageProperty;
  if (field_name == NfcNdefRecord::kFieldText)
    return nfc_record::kRepresentationProperty;
  if (field_name == NfcNdefRecord::kFieldURI)
    return nfc_record::kUriProperty;
  if (field_name == NfcNdefRecord::kFieldMimeType)
    return nfc_record::kMimeTypeProperty;
  if (field_name == NfcNdefRecord::kFieldTargetSize)
    return nfc_record::kSizeProperty;
  return "";
}

std::string NfcNdefRecordActionValueToDBusActionValue(
    const std::string& action) {
  // TODO(armansito): Add bindings for values returned by neard to
  // service_constants.h.
  if (action == device::NfcNdefRecord::kSmartPosterActionDo)
    return "Do";
  if (action == device::NfcNdefRecord::kSmartPosterActionSave)
    return "Save";
  if (action == device::NfcNdefRecord::kSmartPosterActionOpen)
    return "Edit";
  return "";
}

std::string DBusActionValueToNfcNdefRecordActionValue(
    const std::string& action) {
  // TODO(armansito): Add bindings for values returned by neard to
  // service_constants.h.
  if (action == "Do")
    return device::NfcNdefRecord::kSmartPosterActionDo;
  if (action == "Save")
    return device::NfcNdefRecord::kSmartPosterActionSave;
  if (action == "Edit")
    return device::NfcNdefRecord::kSmartPosterActionOpen;
  return "";
}


// Translates the given dictionary of NDEF fields by recursively converting
// each key in |record_data| to its corresponding Record property name defined
// by the Neard D-Bus API. The output is stored in |out|. Returns false if an
// error occurs.
bool ConvertNdefFieldsToDBusAttributes(
    const base::DictionaryValue& fields,
    base::DictionaryValue* out) {
  DCHECK(out);
  for (base::DictionaryValue::Iterator iter(fields);
       !iter.IsAtEnd(); iter.Advance()) {
    // Special case the "titles" and "action" fields.
    if (iter.key() == NfcNdefRecord::kFieldTitles) {
      const base::ListValue* titles = NULL;
      bool value_result = iter.value().GetAsList(&titles);
      DCHECK(value_result);
      DCHECK(titles->GetSize() != 0);
      // TODO(armansito): For now, pick the first title in the list and write
      // its contents directly to the top level of the field. This is due to an
      // error in the Neard D-Bus API design. This code will need to be updated
      // if the neard API changes to correct this.
      const base::DictionaryValue* first_title = NULL;
      value_result = titles->GetDictionary(0, &first_title);
      DCHECK(value_result);
      if (!ConvertNdefFieldsToDBusAttributes(*first_title, out)) {
        LOG(ERROR) << "Invalid title field.";
        return false;
      }
    } else if (iter.key() == NfcNdefRecord::kFieldAction) {
      // The value of the action field needs to be translated.
      std::string action_value;
      bool value_result = iter.value().GetAsString(&action_value);
      DCHECK(value_result);
      std::string action =
          NfcNdefRecordActionValueToDBusActionValue(action_value);
      if (action.empty()) {
        VLOG(1) << "Invalid action value: \"" << action_value << "\"";
        return false;
      }
      out->SetString(nfc_record::kActionProperty, action);
    } else {
      std::string dbus_property = NdefRecordFieldToDBusProperty(iter.key());
      if (dbus_property.empty()) {
        LOG(ERROR) << "Invalid field: " << iter.key();
        return false;
      }
      out->Set(dbus_property, iter.value().DeepCopy());
    }
  }
  return true;
}

}  // namespace

bool NfcNdefRecordToDBusAttributes(
      const NfcNdefRecord* record,
      base::DictionaryValue* out) {
  DCHECK(record);
  DCHECK(out);
  if (!record->IsPopulated()) {
    LOG(ERROR) << "Record is not populated.";
    return false;
  }
  out->SetString(nfc_record::kTypeProperty,
                 NfcRecordTypeEnumToPropertyValue(record->type()));
  return ConvertNdefFieldsToDBusAttributes(record->data(), out);
}

bool RecordPropertiesToNfcNdefRecord(
      const NfcRecordClient::Properties* properties,
      device::NfcNdefRecord* out) {
  if (out->IsPopulated()) {
    LOG(ERROR) << "Record is already populated!";
    return false;
  }
  NfcNdefRecord::Type type =
      DBusRecordTypeValueToNfcNdefRecordType(properties->type.value());
  if (type == NfcNdefRecord::kTypeUnknown) {
    LOG(ERROR) << "Record type is unknown.";
    return false;
  }

  // Extract each property.
  base::DictionaryValue attributes;
  if (!properties->uri.value().empty())
    attributes.SetString(NfcNdefRecord::kFieldURI, properties->uri.value());
  if (!properties->mime_type.value().empty()) {
    attributes.SetString(NfcNdefRecord::kFieldMimeType,
                         properties->mime_type.value());
  }
  if (properties->size.value() != 0) {
    attributes.SetDouble(NfcNdefRecord::kFieldTargetSize,
                         static_cast<double>(properties->size.value()));
  }
  std::string action_value =
    DBusActionValueToNfcNdefRecordActionValue(properties->action.value());
  if (!action_value.empty())
    attributes.SetString(NfcNdefRecord::kFieldAction, action_value);

  // The "representation", "encoding", and "language" properties will be stored
  // differently, depending on whether the record type is "SmartPoster" or
  // "Text".
  {
    std::unique_ptr<base::DictionaryValue> text_attributes(
        new base::DictionaryValue());
    if (!properties->representation.value().empty()) {
      text_attributes->SetString(NfcNdefRecord::kFieldText,
                                 properties->representation.value());
    }
    if (!properties->encoding.value().empty()) {
      text_attributes->SetString(NfcNdefRecord::kFieldEncoding,
                                 properties->encoding.value());
    }
    if (!properties->language.value().empty()) {
      text_attributes->SetString(NfcNdefRecord::kFieldLanguageCode,
                                 properties->language.value());
    }
    if (!text_attributes->empty()) {
      if (type == NfcNdefRecord::kTypeSmartPoster) {
        base::ListValue* titles = new base::ListValue();
        titles->Append(text_attributes.release());
        attributes.Set(NfcNdefRecord::kFieldTitles, titles);
      } else {
        attributes.MergeDictionary(text_attributes.get());
      }
    }
  }

  // Populate the given record.
  return out->Populate(type, &attributes);
}

}  // namespace nfc_ndef_record_utils
}  // namespace chromeos
