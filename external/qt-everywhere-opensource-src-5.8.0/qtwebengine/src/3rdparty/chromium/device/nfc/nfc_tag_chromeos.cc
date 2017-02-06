// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/nfc/nfc_tag_chromeos.h"

#include "chromeos/dbus/dbus_thread_manager.h"
#include "device/nfc/nfc_tag_technology_chromeos.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using device::NfcTag;
using device::NfcTagTechnology;
using device::NfcNdefTagTechnology;

namespace chromeos {

namespace {

// Converts an NFC tag type value returned by neard to a NfcTag::TagType enum
// value.
NfcTag::TagType DBusTypePropertyToTagType(const std::string& type) {
  if (type == nfc_tag::kTagType1)
    return NfcTag::kTagType1;
  if (type == nfc_tag::kTagType2)
    return NfcTag::kTagType2;
  if (type == nfc_tag::kTagType3)
    return NfcTag::kTagType3;
  if (type == nfc_tag::kTagType4)
    return NfcTag::kTagType4;
  return NfcTag::kTagTypeUnknown;
}

// Converts an NFC tag protocol value returned by neard to a NfcTag::Protocol
// enum value.
NfcTag::Protocol DBusProtocolPropertyToTagProtocol(
    const std::string& protocol) {
  if (protocol == nfc_common::kProtocolFelica)
    return NfcTag::kProtocolFelica;
  if (protocol == nfc_common::kProtocolIsoDep)
    return NfcTag::kProtocolIsoDep;
  if (protocol == nfc_common::kProtocolJewel)
    return NfcTag::kProtocolJewel;
  if (protocol == nfc_common::kProtocolMifare)
    return NfcTag::kProtocolMifare;
  if (protocol == nfc_common::kProtocolNfcDep)
    return NfcTag::kProtocolNfcDep;
  return NfcTag::kProtocolUnknown;
}

}  // namespace

NfcTagChromeOS::NfcTagChromeOS(const dbus::ObjectPath& object_path)
    : object_path_(object_path),
      is_ready_(false),
      ndef_technology_(new NfcNdefTagTechnologyChromeOS(this)) {
  DBusThreadManager::Get()->GetNfcTagClient()->AddObserver(this);
}

NfcTagChromeOS::~NfcTagChromeOS() {
  DBusThreadManager::Get()->GetNfcTagClient()->RemoveObserver(this);
}

void NfcTagChromeOS::AddObserver(NfcTag::Observer* observer) {
  observers_.AddObserver(observer);
}

void NfcTagChromeOS::RemoveObserver(NfcTag::Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::string NfcTagChromeOS::GetIdentifier() const {
  return object_path_.value();
}

NfcTag::TagType NfcTagChromeOS::GetType() const {
  DCHECK(object_path_.IsValid());
  return DBusTypePropertyToTagType(
      DBusThreadManager::Get()->GetNfcTagClient()->
          GetProperties(object_path_)->type.value());
}

bool NfcTagChromeOS::IsReadOnly() const {
  DCHECK(object_path_.IsValid());
  return DBusThreadManager::Get()->GetNfcTagClient()->
      GetProperties(object_path_)->read_only.value();
}

NfcTag::Protocol NfcTagChromeOS::GetSupportedProtocol() const {
  DCHECK(object_path_.IsValid());
  return DBusProtocolPropertyToTagProtocol(
      DBusThreadManager::Get()->GetNfcTagClient()->
          GetProperties(object_path_)->protocol.value());
}

NfcTagTechnology::TechnologyTypeMask
NfcTagChromeOS::GetSupportedTechnologies() const {
  // Determine supported technologies based on the tag's protocol and
  // type.
  NfcTag::TagType type = GetType();
  NfcTag::Protocol protocol = GetSupportedProtocol();
  if (type == NfcTag::kTagTypeUnknown || protocol == kProtocolUnknown) {
    VLOG(1) << "Tag type and protocol unknown.";
    return 0;
  }

  NfcTagTechnology::TechnologyTypeMask technologies = 0;
  technologies |= NfcTagTechnology::kTechnologyTypeNdef;
  if (type == NfcTag::kTagType3) {
    DCHECK(protocol == NfcTag::kProtocolFelica);
    return technologies | NfcTagTechnology::kTechnologyTypeNfcF;
  }

  if (protocol == NfcTag::kProtocolIsoDep) {
    DCHECK(type == NfcTag::kTagType4);
    technologies |= NfcTagTechnology::kTechnologyTypeIsoDep;
    // TODO(armansito): Neard doesn't provide enough information to determine
    // if the underlying wave-form is type A or type B. For now, report
    // neither.
    return technologies;
  }

  return technologies | NfcTagTechnology::kTechnologyTypeNfcA;
}

bool NfcTagChromeOS::IsReady() const {
  return is_ready_;
}

NfcNdefTagTechnology* NfcTagChromeOS::GetNdefTagTechnology() {
  return ndef_technology_.get();
}

void NfcTagChromeOS::TagPropertyChanged(const dbus::ObjectPath& object_path,
                                        const std::string& property_name) {
  if (object_path != object_path_)
    return;

  NfcTagClient::Properties* properties =
    DBusThreadManager::Get()->GetNfcTagClient()->GetProperties(object_path_);
  DCHECK(properties);

  if (property_name == properties->type.name()) {
    FOR_EACH_OBSERVER(NfcTag::Observer, observers_,
                      TagTypeChanged(this, GetType()));
  } else if (property_name == properties->read_only.name()) {
    FOR_EACH_OBSERVER(NfcTag::Observer, observers_,
                      TagWritePermissionChanged(this, IsReadOnly()));
  } else if (property_name == properties->protocol.name()) {
    FOR_EACH_OBSERVER(
        NfcTag::Observer, observers_,
        TagSupportedProtocolChanged(this, GetSupportedProtocol()));
  }
}

void NfcTagChromeOS::TagPropertiesReceived(
    const dbus::ObjectPath& object_path) {
  if (is_ready_ || object_path != object_path_)
    return;

  is_ready_ = true;
  FOR_EACH_OBSERVER(NfcTag::Observer, observers_, TagReady(this));
}

}  // namespace chromeos
