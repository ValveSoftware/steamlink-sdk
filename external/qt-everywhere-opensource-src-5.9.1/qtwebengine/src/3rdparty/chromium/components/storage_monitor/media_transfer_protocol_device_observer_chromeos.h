// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STORAGE_MONITOR_MEDIA_TRANSFER_PROTOCOL_DEVICE_OBSERVER_CHROMEOS_H_
#define COMPONENTS_STORAGE_MONITOR_MEDIA_TRANSFER_PROTOCOL_DEVICE_OBSERVER_CHROMEOS_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/storage_monitor/storage_monitor.h"
#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"

namespace base {
class FilePath;
}

namespace storage_monitor {

// Gets the mtp device information given a |storage_name|. On success,
// fills in |id|, |name|, |location|, |vendor_name|, and |product_name|.
typedef void (*GetStorageInfoFunc)(
    const std::string& storage_name,
    device::MediaTransferProtocolManager* mtp_manager,
    std::string* id,
    base::string16* name,
    std::string* location,
    base::string16* vendor_name,
    base::string16* product_name);

// Helper class to send MTP storage attachment and detachment events to
// StorageMonitor.
class MediaTransferProtocolDeviceObserverChromeOS
    : public device::MediaTransferProtocolManager::Observer {
 public:
  MediaTransferProtocolDeviceObserverChromeOS(
      StorageMonitor::Receiver* receiver,
      device::MediaTransferProtocolManager* mtp_manager);
  ~MediaTransferProtocolDeviceObserverChromeOS() override;

  // Finds the storage that contains |path| and populates |storage_info|.
  // Returns false if unable to find the storage.
  bool GetStorageInfoForPath(const base::FilePath& path,
                             StorageInfo* storage_info) const;

  void EjectDevice(const std::string& device_id,
                   base::Callback<void(StorageMonitor::EjectStatus)> callback);

 protected:
  // Only used in unit tests.
  MediaTransferProtocolDeviceObserverChromeOS(
      StorageMonitor::Receiver* receiver,
      device::MediaTransferProtocolManager* mtp_manager,
      GetStorageInfoFunc get_storage_info_func);

  // device::MediaTransferProtocolManager::Observer implementation.
  // Exposed for unit tests.
  void StorageChanged(bool is_attached,
                      const std::string& storage_name) override;

 private:
  // Mapping of storage location and mtp storage info object.
  typedef std::map<std::string, StorageInfo> StorageLocationToInfoMap;

  // Enumerate existing mtp storage devices.
  void EnumerateStorages();

  // Find the |storage_map_| key for the record with this |device_id|. Returns
  // true on success, false on failure.
  bool GetLocationForDeviceId(const std::string& device_id,
                              std::string* location) const;

  // Pointer to the MTP manager. Not owned. Client must ensure the MTP
  // manager outlives this object.
  device::MediaTransferProtocolManager* mtp_manager_;

  // Map of all attached mtp devices.
  StorageLocationToInfoMap storage_map_;

  // Function handler to get storage information. This is useful to set a mock
  // handler for unit testing.
  GetStorageInfoFunc get_storage_info_func_;

  // The notifications object to use to signal newly attached devices.
  // Guaranteed to outlive this class.
  StorageMonitor::Receiver* const notifications_;

  DISALLOW_COPY_AND_ASSIGN(MediaTransferProtocolDeviceObserverChromeOS);
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_MEDIA_TRANSFER_PROTOCOL_DEVICE_OBSERVER_CHROMEOS_H_
