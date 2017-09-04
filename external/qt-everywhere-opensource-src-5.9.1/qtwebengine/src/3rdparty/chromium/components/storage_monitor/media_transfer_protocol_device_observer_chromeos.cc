// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/media_transfer_protocol_device_observer_chromeos.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/storage_monitor/media_storage_util.h"
#include "components/storage_monitor/removable_device_constants.h"
#include "device/media_transfer_protocol/mtp_storage_info.pb.h"

namespace storage_monitor {

namespace {

// Device root path constant.
const char kRootPath[] = "/";

// Constructs and returns the location of the device using the |storage_name|.
std::string GetDeviceLocationFromStorageName(const std::string& storage_name) {
  // Construct a dummy device path using the storage name. This is only used
  // for registering device media file system.
  // E.g.: If the |storage_name| is "usb:2,2:12345" then "/usb:2,2:12345" is the
  // device location.
  DCHECK(!storage_name.empty());
  return kRootPath + storage_name;
}

// Returns the storage identifier of the device from the given |storage_name|.
// E.g. If the |storage_name| is "usb:2,2:65537", the storage identifier is
// "65537".
std::string GetStorageIdFromStorageName(const std::string& storage_name) {
  std::vector<base::StringPiece> name_parts = base::SplitStringPiece(
      storage_name, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  return name_parts.size() == 3 ? name_parts[2].as_string() : std::string();
}

// Returns a unique device id from the given |storage_info|.
std::string GetDeviceIdFromStorageInfo(const MtpStorageInfo& storage_info) {
  const std::string storage_id =
      GetStorageIdFromStorageName(storage_info.storage_name());
  if (storage_id.empty())
    return std::string();

  // Some devices have multiple data stores. Therefore, include storage id as
  // part of unique id along with vendor, model and volume information.
  const std::string vendor_id = base::UintToString(storage_info.vendor_id());
  const std::string model_id = base::UintToString(storage_info.product_id());
  return StorageInfo::MakeDeviceId(
      StorageInfo::MTP_OR_PTP,
      kVendorModelVolumeStoragePrefix + vendor_id + ":" + model_id + ":" +
          storage_info.volume_identifier() + ":" + storage_id);
}

// Returns the |data_store_id| string in the required format.
// If the |data_store_id| is 65537, this function returns " (65537)".
std::string GetFormattedIdString(const std::string& data_store_id) {
  return ("(" + data_store_id + ")");
}

// Helper function to get device label from storage information.
base::string16 GetDeviceLabelFromStorageInfo(
    const MtpStorageInfo& storage_info) {
  std::string device_label;
  const std::string& vendor_name = storage_info.vendor();
  device_label = vendor_name;

  const std::string& product_name = storage_info.product();
  if (!product_name.empty()) {
    if (!device_label.empty())
      device_label += " ";
    device_label += product_name;
  }

  // Add the data store id to the device label.
  if (!device_label.empty()) {
    const std::string& volume_id = storage_info.volume_identifier();
    if (!volume_id.empty()) {
      device_label += GetFormattedIdString(volume_id);
    } else {
      const std::string data_store_id =
          GetStorageIdFromStorageName(storage_info.storage_name());
      if (!data_store_id.empty())
        device_label += GetFormattedIdString(data_store_id);
    }
  }
  return base::UTF8ToUTF16(device_label);
}

// Helper function to get the device storage details such as device id, label
// and location. On success and fills in |id|, |label|, |location|,
// |vendor_name|, and |product_name|.
void GetStorageInfo(const std::string& storage_name,
                    device::MediaTransferProtocolManager* mtp_manager,
                    std::string* id,
                    base::string16* label,
                    std::string* location,
                    base::string16* vendor_name,
                    base::string16* product_name) {
  DCHECK(!storage_name.empty());
  const MtpStorageInfo* storage_info =
      mtp_manager->GetStorageInfo(storage_name);

  if (!storage_info)
    return;

  *id = GetDeviceIdFromStorageInfo(*storage_info);
  *label = GetDeviceLabelFromStorageInfo(*storage_info);
  *location = GetDeviceLocationFromStorageName(storage_name);
  *vendor_name = base::UTF8ToUTF16(storage_info->vendor());
  *product_name = base::UTF8ToUTF16(storage_info->product());
}

}  // namespace

MediaTransferProtocolDeviceObserverChromeOS::
    MediaTransferProtocolDeviceObserverChromeOS(
        StorageMonitor::Receiver* receiver,
        device::MediaTransferProtocolManager* mtp_manager)
    : mtp_manager_(mtp_manager),
      get_storage_info_func_(&GetStorageInfo),
      notifications_(receiver) {
  mtp_manager_->AddObserver(this);
  EnumerateStorages();
}

// This constructor is only used by unit tests.
MediaTransferProtocolDeviceObserverChromeOS::
    MediaTransferProtocolDeviceObserverChromeOS(
        StorageMonitor::Receiver* receiver,
        device::MediaTransferProtocolManager* mtp_manager,
        GetStorageInfoFunc get_storage_info_func)
    : mtp_manager_(mtp_manager),
      get_storage_info_func_(get_storage_info_func),
      notifications_(receiver) {}

MediaTransferProtocolDeviceObserverChromeOS::
    ~MediaTransferProtocolDeviceObserverChromeOS() {
  mtp_manager_->RemoveObserver(this);
}

bool MediaTransferProtocolDeviceObserverChromeOS::GetStorageInfoForPath(
    const base::FilePath& path,
    StorageInfo* storage_info) const {
  DCHECK(storage_info);

  if (!path.IsAbsolute())
    return false;

  std::vector<base::FilePath::StringType> path_components;
  path.GetComponents(&path_components);
  if (path_components.size() < 2)
    return false;

  // First and second component of the path specifies the device location.
  // E.g.: If |path| is "/usb:2,2:65537/DCIM/Folder_a", "/usb:2,2:65537" is the
  // device location.
  StorageLocationToInfoMap::const_iterator info_it =
      storage_map_.find(GetDeviceLocationFromStorageName(path_components[1]));
  if (info_it == storage_map_.end())
    return false;

  *storage_info = info_it->second;
  return true;
}

void MediaTransferProtocolDeviceObserverChromeOS::EjectDevice(
    const std::string& device_id,
    base::Callback<void(StorageMonitor::EjectStatus)> callback) {
  std::string location;
  if (!GetLocationForDeviceId(device_id, &location)) {
    callback.Run(StorageMonitor::EJECT_NO_SUCH_DEVICE);
    return;
  }

  // TODO(thestig): Change this to tell the mtp manager to eject the device.

  StorageChanged(false, location);
  callback.Run(StorageMonitor::EJECT_OK);
}

// device::MediaTransferProtocolManager::Observer override.
void MediaTransferProtocolDeviceObserverChromeOS::StorageChanged(
    bool is_attached,
    const std::string& storage_name) {
  DCHECK(!storage_name.empty());

  // New storage is attached.
  if (is_attached) {
    std::string device_id;
    base::string16 storage_label;
    std::string location;
    base::string16 vendor_name;
    base::string16 product_name;
    get_storage_info_func_(storage_name, mtp_manager_, &device_id,
                           &storage_label, &location, &vendor_name,
                           &product_name);

    // Keep track of device id and device name to see how often we receive
    // empty values.
    MediaStorageUtil::RecordDeviceInfoHistogram(false, device_id,
                                                storage_label);
    if (device_id.empty() || storage_label.empty())
      return;

    DCHECK(!base::ContainsKey(storage_map_, location));

    StorageInfo storage_info(device_id, location, storage_label, vendor_name,
                             product_name, 0);
    storage_map_[location] = storage_info;
    notifications_->ProcessAttach(storage_info);
  } else {
    // Existing storage is detached.
    StorageLocationToInfoMap::iterator it =
        storage_map_.find(GetDeviceLocationFromStorageName(storage_name));
    if (it == storage_map_.end())
      return;
    notifications_->ProcessDetach(it->second.device_id());
    storage_map_.erase(it);
  }
}

void MediaTransferProtocolDeviceObserverChromeOS::EnumerateStorages() {
  typedef std::vector<std::string> StorageList;
  StorageList storages = mtp_manager_->GetStorages();
  for (StorageList::const_iterator storage_iter = storages.begin();
       storage_iter != storages.end(); ++storage_iter) {
    StorageChanged(true, *storage_iter);
  }
}

bool MediaTransferProtocolDeviceObserverChromeOS::GetLocationForDeviceId(
    const std::string& device_id,
    std::string* location) const {
  for (StorageLocationToInfoMap::const_iterator it = storage_map_.begin();
       it != storage_map_.end(); ++it) {
    if (it->second.device_id() == device_id) {
      *location = it->first;
      return true;
    }
  }

  return false;
}

}  // namespace storage_monitor
