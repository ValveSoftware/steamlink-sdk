// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_NFC_NFC_TAG_CHROMEOS_H_
#define DEVICE_NFC_NFC_TAG_CHROMEOS_H_

#include <string>

#include "base/memory/scoped_ptr.h"
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
  virtual void AddObserver(device::NfcTag::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(device::NfcTag::Observer* observer) OVERRIDE;
  virtual std::string GetIdentifier() const OVERRIDE;
  virtual TagType GetType() const OVERRIDE;
  virtual bool IsReadOnly() const OVERRIDE;
  virtual device::NfcTag::Protocol GetSupportedProtocol() const OVERRIDE;
  virtual device::NfcTagTechnology::TechnologyTypeMask
      GetSupportedTechnologies() const OVERRIDE;
  virtual bool IsReady() const OVERRIDE;
  virtual device::NfcNdefTagTechnology* GetNdefTagTechnology() OVERRIDE;

  // NfcTagClient::Observer overrides.
  virtual void TagPropertyChanged(const dbus::ObjectPath& object_path,
                                  const std::string& property_name) OVERRIDE;
  virtual void TagPropertiesReceived(
      const dbus::ObjectPath& object_path) OVERRIDE;

  // Object path representing the remote tag object.
  const dbus::ObjectPath& object_path() const { return object_path_; }

 private:
  friend class NfcAdapterChromeOS;

  explicit NfcTagChromeOS(const dbus::ObjectPath& object_path);
  virtual ~NfcTagChromeOS();

  // Object path of the tag that we are currently tracking.
  dbus::ObjectPath object_path_;

  // Stores whether or not the initial set of properties have been received.
  bool is_ready_;

  // The NfcNdefTagTechnology instance that allows users to perform NDEF
  // read and write on this tag.
  scoped_ptr<NfcNdefTagTechnologyChromeOS> ndef_technology_;

  // List of observers interested in event notifications from us.
  ObserverList<device::NfcTag::Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(NfcTagChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_NFC_NFC_TAG_CHROMEOS_H_
