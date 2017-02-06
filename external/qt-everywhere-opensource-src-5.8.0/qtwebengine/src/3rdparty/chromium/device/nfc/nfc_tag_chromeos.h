// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_NFC_NFC_TAG_CHROMEOS_H_
#define DEVICE_NFC_NFC_TAG_CHROMEOS_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/dbus/nfc_tag_client.h"
#include "dbus/object_path.h"
#include "device/nfc/nfc_tag.h"

namespace chromeos {

class NfcNdefTagTechnologyChromeOS;

// The NfcTagChromeOS class implements device::NfcTag for the Chrome OS
// platform.
class NfcTagChromeOS : public device::NfcTag,
                       public NfcTagClient::Observer {
 public:
  // device::NfcTag overrides.
  void AddObserver(device::NfcTag::Observer* observer) override;
  void RemoveObserver(device::NfcTag::Observer* observer) override;
  std::string GetIdentifier() const override;
  TagType GetType() const override;
  bool IsReadOnly() const override;
  device::NfcTag::Protocol GetSupportedProtocol() const override;
  device::NfcTagTechnology::TechnologyTypeMask GetSupportedTechnologies()
      const override;
  bool IsReady() const override;
  device::NfcNdefTagTechnology* GetNdefTagTechnology() override;

  // NfcTagClient::Observer overrides.
  void TagPropertyChanged(const dbus::ObjectPath& object_path,
                          const std::string& property_name) override;
  void TagPropertiesReceived(const dbus::ObjectPath& object_path) override;

  // Object path representing the remote tag object.
  const dbus::ObjectPath& object_path() const { return object_path_; }

 private:
  friend class NfcAdapterChromeOS;

  explicit NfcTagChromeOS(const dbus::ObjectPath& object_path);
  ~NfcTagChromeOS() override;

  // Object path of the tag that we are currently tracking.
  dbus::ObjectPath object_path_;

  // Stores whether or not the initial set of properties have been received.
  bool is_ready_;

  // The NfcNdefTagTechnology instance that allows users to perform NDEF
  // read and write on this tag.
  std::unique_ptr<NfcNdefTagTechnologyChromeOS> ndef_technology_;

  // List of observers interested in event notifications from us.
  base::ObserverList<device::NfcTag::Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(NfcTagChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_NFC_NFC_TAG_CHROMEOS_H_
