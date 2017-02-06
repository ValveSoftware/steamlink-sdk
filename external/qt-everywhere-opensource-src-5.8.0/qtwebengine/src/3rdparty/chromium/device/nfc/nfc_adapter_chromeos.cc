// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/nfc/nfc_adapter_chromeos.h"

#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "device/nfc/nfc_peer_chromeos.h"
#include "device/nfc/nfc_tag_chromeos.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

typedef std::vector<dbus::ObjectPath> ObjectPathVector;

}  // namespace

NfcAdapterChromeOS::NfcAdapterChromeOS()
    : weak_ptr_factory_(this) {
  DBusThreadManager::Get()->GetNfcAdapterClient()->AddObserver(this);
  DBusThreadManager::Get()->GetNfcDeviceClient()->AddObserver(this);
  DBusThreadManager::Get()->GetNfcTagClient()->AddObserver(this);

  const ObjectPathVector& object_paths =
      DBusThreadManager::Get()->GetNfcAdapterClient()->GetAdapters();
  if (!object_paths.empty()) {
    VLOG(1) << object_paths.size() << " NFC adapter(s) available.";
    SetAdapter(object_paths[0]);
  }
}

NfcAdapterChromeOS::~NfcAdapterChromeOS() {
  DBusThreadManager::Get()->GetNfcAdapterClient()->RemoveObserver(this);
  DBusThreadManager::Get()->GetNfcDeviceClient()->RemoveObserver(this);
  DBusThreadManager::Get()->GetNfcTagClient()->RemoveObserver(this);
}

void NfcAdapterChromeOS::AddObserver(NfcAdapter::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void NfcAdapterChromeOS::RemoveObserver(NfcAdapter::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

bool NfcAdapterChromeOS::IsPresent() const {
  return !object_path_.value().empty();
}

bool NfcAdapterChromeOS::IsPowered() const {
  if (!IsPresent())
    return false;
  return DBusThreadManager::Get()->GetNfcAdapterClient()->
      GetProperties(object_path_)->powered.value();
}

bool NfcAdapterChromeOS::IsPolling() const {
  if (!IsPresent())
    return false;
  return DBusThreadManager::Get()->GetNfcAdapterClient()->
      GetProperties(object_path_)->polling.value();
}

bool NfcAdapterChromeOS::IsInitialized() const {
  return true;
}

void NfcAdapterChromeOS::SetPowered(bool powered,
                                    const base::Closure& callback,
                                    const ErrorCallback& error_callback) {
  if (!IsPresent()) {
    LOG(WARNING) << "Adapter not present. Cannot power up the antenna.";
    error_callback.Run();
    return;
  }
  DBusThreadManager::Get()->GetNfcAdapterClient()->
      GetProperties(object_path_)->powered.Set(
          powered,
          base::Bind(&NfcAdapterChromeOS::OnSetPowered,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     error_callback));
}

void NfcAdapterChromeOS::StartPolling(const base::Closure& callback,
                                      const ErrorCallback& error_callback) {
  // Always poll in "Initiator" mode.
  DBusThreadManager::Get()->GetNfcAdapterClient()->
      StartPollLoop(object_path_,
                    nfc_adapter::kModeInitiator,
                    base::Bind(&NfcAdapterChromeOS::OnStartPolling,
                               weak_ptr_factory_.GetWeakPtr(),
                               callback),
                    base::Bind(&NfcAdapterChromeOS::OnStartPollingError,
                               weak_ptr_factory_.GetWeakPtr(),
                               error_callback));
}

void NfcAdapterChromeOS::StopPolling(const base::Closure& callback,
                                     const ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetNfcAdapterClient()->
      StopPollLoop(object_path_,
                   base::Bind(&NfcAdapterChromeOS::OnStopPolling,
                              weak_ptr_factory_.GetWeakPtr(),
                              callback),
                   base::Bind(&NfcAdapterChromeOS::OnStopPollingError,
                              weak_ptr_factory_.GetWeakPtr(),
                              error_callback));
}

void NfcAdapterChromeOS::AdapterAdded(const dbus::ObjectPath& object_path) {
  // Set the adapter to the newly added adapter only if no adapter is present.
  if (!IsPresent())
    SetAdapter(object_path);
}

void NfcAdapterChromeOS::AdapterRemoved(const dbus::ObjectPath& object_path) {
  if (object_path != object_path_)
    return;

  // The current adapter was removed, so mark us as not present and clean up
  // peers and tags.
  RemoveAdapter();

  // There may still be other adapters present on the system. Set the next
  // available adapter as the current one.
  const ObjectPathVector& object_paths =
      DBusThreadManager::Get()->GetNfcAdapterClient()->GetAdapters();
  for (ObjectPathVector::const_iterator iter =
          object_paths.begin();
       iter != object_paths.end(); ++iter) {
    // The removed object will still be available until the call to
    // AdapterRemoved returns. Make sure that we are not re-adding the
    // removed adapter.
    if (*iter == object_path)
      continue;
    SetAdapter(*iter);
  }
}

void NfcAdapterChromeOS::AdapterPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  if (object_path != object_path_)
    return;
  NfcAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetNfcAdapterClient()->
          GetProperties(object_path_);
  if (property_name == properties->powered.name())
    PoweredChanged(properties->powered.value());
  else if (property_name == properties->polling.name())
    PollingChanged(properties->polling.value());
}

void NfcAdapterChromeOS::DeviceAdded(const dbus::ObjectPath& object_path) {
  if (!IsPresent())
    return;

  if (GetPeer(object_path.value()))
    return;

  VLOG(1) << "NFC device found: " << object_path.value();

  // Check to see if the device belongs to this adapter.
  const ObjectPathVector& devices =
      DBusThreadManager::Get()->GetNfcDeviceClient()->
          GetDevicesForAdapter(object_path_);
  bool device_found = false;
  for (ObjectPathVector::const_iterator iter = devices.begin();
       iter != devices.end(); ++iter) {
    if (*iter == object_path) {
      device_found = true;
      break;
    }
  }
  if (!device_found) {
    VLOG(1) << "Found peer device does not belong to the current adapter.";
    return;
  }

  // Create the peer object.
  NfcPeerChromeOS* peer_chromeos = new NfcPeerChromeOS(object_path);
  SetPeer(object_path.value(), peer_chromeos);
  FOR_EACH_OBSERVER(NfcAdapter::Observer, observers_,
                    PeerFound(this, peer_chromeos));
}

void NfcAdapterChromeOS::DeviceRemoved(const dbus::ObjectPath& object_path) {
  VLOG(1) << "NFC device lost: " << object_path.value();
  device::NfcPeer* peer = RemovePeer(object_path.value());
  if (!peer) {
    VLOG(1) << "Removed peer device does not belong to the current adapter.";
    return;
  }
  FOR_EACH_OBSERVER(NfcAdapter::Observer, observers_, PeerLost(this, peer));
  delete peer;
}

void NfcAdapterChromeOS::TagAdded(const dbus::ObjectPath& object_path) {
  if (!IsPresent())
    return;

  if (GetTag(object_path.value()))
    return;

  VLOG(1) << "NFC tag found: " << object_path.value();

  // Check to see if the tag belongs to this adapter.
  const std::vector<dbus::ObjectPath>& tags =
      DBusThreadManager::Get()->GetNfcTagClient()->
          GetTagsForAdapter(object_path_);
  bool tag_found = false;
  for (std::vector<dbus::ObjectPath>::const_iterator iter = tags.begin();
       iter != tags.end(); ++iter) {
    if (*iter == object_path) {
      tag_found = true;
      break;
    }
  }
  if (!tag_found) {
    VLOG(1) << "Found tag does not belong to the current adapter.";
    return;
  }

  // Create the tag object.
  NfcTagChromeOS* tag_chromeos = new NfcTagChromeOS(object_path);
  SetTag(object_path.value(), tag_chromeos);
  FOR_EACH_OBSERVER(NfcAdapter::Observer, observers_,
                    TagFound(this, tag_chromeos));
}

void NfcAdapterChromeOS::TagRemoved(const dbus::ObjectPath& object_path) {
  VLOG(1) << "NFC tag lost : " << object_path.value();
  device::NfcTag* tag = RemoveTag(object_path.value());
  if (!tag) {
    VLOG(1) << "Removed tag does not belong to the current adapter.";
    return;
  }
  FOR_EACH_OBSERVER(NfcAdapter::Observer, observers_, TagLost(this, tag));
  delete tag;
}

void NfcAdapterChromeOS::SetAdapter(const dbus::ObjectPath& object_path) {
  DCHECK(!IsPresent());
  object_path_ = object_path;
  VLOG(1) << "Using NFC adapter: " << object_path.value();

  NfcAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetNfcAdapterClient()->
          GetProperties(object_path_);
  PresentChanged(true);
  if (properties->powered.value())
    PoweredChanged(true);
  if (properties->polling.value())
    PollingChanged(true);

  // Create peer objects for peers that were added before the adapter was set.
  const ObjectPathVector& devices =
      DBusThreadManager::Get()->GetNfcDeviceClient()->
          GetDevicesForAdapter(object_path_);
  for (ObjectPathVector::const_iterator iter = devices.begin();
       iter != devices.end(); ++iter) {
    const dbus::ObjectPath& object_path = *iter;
    if (GetPeer(object_path.value()))
      continue;
    NfcPeerChromeOS* peer_chromeos = new NfcPeerChromeOS(object_path);
    SetPeer(object_path.value(), peer_chromeos);
    FOR_EACH_OBSERVER(NfcAdapter::Observer, observers_,
                      PeerFound(this, peer_chromeos));
  }

  // Create tag objects for tags that were added before the adapter was set.
  const std::vector<dbus::ObjectPath>& tags =
      DBusThreadManager::Get()->GetNfcTagClient()->
          GetTagsForAdapter(object_path_);
  for (std::vector<dbus::ObjectPath>::const_iterator iter = tags.begin();
       iter != tags.end(); ++iter) {
    const dbus::ObjectPath& object_path = *iter;
    if (GetTag(object_path.value()))
      continue;
    NfcTagChromeOS* tag_chromeos = new NfcTagChromeOS(object_path);
    SetTag(object_path.value(), tag_chromeos);
    FOR_EACH_OBSERVER(NfcAdapter::Observer, observers_,
                      TagFound(this, tag_chromeos));
  }
}

void NfcAdapterChromeOS::RemoveAdapter() {
  DCHECK(IsPresent());
  VLOG(1) << "NFC adapter removed: " << object_path_.value();

  NfcAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetNfcAdapterClient()->
          GetProperties(object_path_);
  if (properties->powered.value())
    PoweredChanged(false);
  if (properties->polling.value())
    PollingChanged(false);

  // Copy the tags and peers here and clear the original containers so that
  // GetPeers and GetTags return no values during the *Removed observer calls.
  PeerList peers;
  TagList tags;
  GetPeers(&peers);
  GetTags(&tags);
  ClearPeers();
  ClearTags();

  for (PeerList::iterator iter = peers.begin();
       iter != peers.end(); ++iter) {
    device::NfcPeer* peer = *iter;
    FOR_EACH_OBSERVER(NfcAdapter::Observer, observers_,
                      PeerLost(this, peer));
    delete peer;
  }
  for (TagList::iterator iter = tags.begin();
       iter != tags.end(); ++iter) {
    device::NfcTag* tag = *iter;
    FOR_EACH_OBSERVER(NfcAdapter::Observer, observers_,
                      TagLost(this, tag));
    delete tag;
  }

  object_path_ = dbus::ObjectPath("");
  PresentChanged(false);
}

void NfcAdapterChromeOS::PoweredChanged(bool powered) {
  FOR_EACH_OBSERVER(NfcAdapter::Observer, observers_,
                    AdapterPoweredChanged(this, powered));
}

void NfcAdapterChromeOS::PollingChanged(bool polling) {
  FOR_EACH_OBSERVER(NfcAdapter::Observer, observers_,
                    AdapterPollingChanged(this, polling));
}

void NfcAdapterChromeOS::PresentChanged(bool present) {
  FOR_EACH_OBSERVER(NfcAdapter::Observer, observers_,
                    AdapterPresentChanged(this, present));
}

void NfcAdapterChromeOS::OnSetPowered(const base::Closure& callback,
                                      const ErrorCallback& error_callback,
                                      bool success) {
  VLOG(1) << "NfcAdapterChromeOS::OnSetPowered result: " << success;
  if (success) {
    // TODO(armansito): There is a bug in neard 0.13 that causes it not to emit
    // a signal when the "Powered" property changes. Sync the properties here,
    // but remove it in neard 0.14.
    if (IsPresent()) {
      DBusThreadManager::Get()->GetNfcAdapterClient()->
          GetProperties(object_path_)->GetAll();
    }
    callback.Run();
  } else {
    LOG(ERROR) << "Failed to power up the NFC antenna radio.";
    error_callback.Run();
  }
}

void NfcAdapterChromeOS::OnStartPolling(const base::Closure& callback) {
  callback.Run();
}

void NfcAdapterChromeOS::OnStartPollingError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(ERROR) << object_path_.value() << ": Failed to start polling: "
             << error_name << ": " << error_message;
  error_callback.Run();
}

void NfcAdapterChromeOS::OnStopPolling(const base::Closure& callback) {
  callback.Run();
}

void NfcAdapterChromeOS::OnStopPollingError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(ERROR) << object_path_.value() << ": Failed to stop polling: "
             << error_name << ": " << error_message;
  error_callback.Run();
}

}  // namespace chromeos
