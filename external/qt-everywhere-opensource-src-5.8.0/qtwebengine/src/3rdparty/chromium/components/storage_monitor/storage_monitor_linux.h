// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StorageMonitorLinux processes mount point change events, notifies listeners
// about the addition and deletion of media devices, and answers queries about
// mounted devices.
// StorageMonitorLinux lives on the UI thread, and uses a MtabWatcherLinux on
// the FILE thread to get mount point change events.

#ifndef COMPONENTS_STORAGE_MONITOR_STORAGE_MONITOR_LINUX_H_
#define COMPONENTS_STORAGE_MONITOR_STORAGE_MONITOR_LINUX_H_

#if defined(OS_CHROMEOS)
#error "Use the ChromeOS-specific implementation instead."
#endif

#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/storage_monitor/mtab_watcher_linux.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_thread.h"

namespace storage_monitor {

class MediaTransferProtocolDeviceObserverLinux;

class StorageMonitorLinux : public StorageMonitor,
                            public MtabWatcherLinux::Delegate {
 public:
  // Should only be called by browser start up code.
  // Use StorageMonitor::GetInstance() instead.
  // |mtab_file_path| is the path to a mtab file to watch for mount points.
  explicit StorageMonitorLinux(const base::FilePath& mtab_file_path);
  ~StorageMonitorLinux() override;

  // Must be called for StorageMonitorLinux to work.
  void Init() override;

 protected:
  // Gets device information given a |device_path| and |mount_point|.
  using GetDeviceInfoCallback = base::Callback<std::unique_ptr<StorageInfo>(
      const base::FilePath& device_path,
      const base::FilePath& mount_point)>;

  void SetGetDeviceInfoCallbackForTest(
      const GetDeviceInfoCallback& get_device_info_callback);

  void SetMediaTransferProtocolManagerForTest(
      device::MediaTransferProtocolManager* test_manager);

  // MtabWatcherLinux::Delegate implementation.
  void UpdateMtab(
      const MtabWatcherLinux::MountPointDeviceMap& new_mtab) override;

 private:
  // Structure to save mounted device information such as device path, unique
  // identifier, device name and partition size.
  struct MountPointInfo {
    base::FilePath mount_device;
    StorageInfo storage_info;
  };

  // For use with std::unique_ptr.
  struct MtabWatcherLinuxDeleter {
    void operator()(MtabWatcherLinux* mtab_watcher) {
      content::BrowserThread::DeleteSoon(content::BrowserThread::FILE,
                                         FROM_HERE, mtab_watcher);
    }
  };

  // Mapping of mount points to MountPointInfo.
  typedef std::map<base::FilePath, MountPointInfo> MountMap;

  // (mount point, priority)
  // For devices that are mounted to multiple mount points, this helps us track
  // which one we've notified system monitor about.
  typedef std::map<base::FilePath, bool> ReferencedMountPoint;

  // (mount device, map of known mount points)
  // For each mount device, track the places it is mounted and which one (if
  // any) we have notified system monitor about.
  typedef std::map<base::FilePath, ReferencedMountPoint> MountPriorityMap;

  // StorageMonitor implementation.
  bool GetStorageInfoForPath(const base::FilePath& path,
                             StorageInfo* device_info) const override;
  void EjectDevice(const std::string& device_id,
                   base::Callback<void(EjectStatus)> callback) override;
  device::MediaTransferProtocolManager* media_transfer_protocol_manager()
      override;

  // Called when the MtabWatcher has been created.
  void OnMtabWatcherCreated(MtabWatcherLinux* watcher);

  bool IsDeviceAlreadyMounted(const base::FilePath& mount_device) const;

  // Assuming |mount_device| is already mounted, and it gets mounted again at
  // |mount_point|, update the mappings.
  void HandleDeviceMountedMultipleTimes(const base::FilePath& mount_device,
                                        const base::FilePath& mount_point);

  // Adds |mount_device| to the mappings and notify listeners, if any.
  void AddNewMount(const base::FilePath& mount_device,
                   std::unique_ptr<StorageInfo> storage_info);

  // Mtab file that lists the mount points.
  const base::FilePath mtab_path_;

  // Callback to get device information. Set this to a custom callback for
  // testing.
  GetDeviceInfoCallback get_device_info_callback_;

  // Mapping of relevant mount points and their corresponding mount devices.
  // Keep in mind on Linux, a device can be mounted at multiple mount points,
  // and multiple devices can be mounted at a mount point.
  MountMap mount_info_map_;

  // Because a device can be mounted to multiple places, we only want to
  // notify about one of them. If (and only if) that one is unmounted, we need
  // to notify about it's departure and notify about another one of it's mount
  // points.
  MountPriorityMap mount_priority_map_;

  std::unique_ptr<device::MediaTransferProtocolManager>
      media_transfer_protocol_manager_;
  std::unique_ptr<MediaTransferProtocolDeviceObserverLinux>
      media_transfer_protocol_device_observer_;

  std::unique_ptr<MtabWatcherLinux, MtabWatcherLinuxDeleter> mtab_watcher_;

  base::WeakPtrFactory<StorageMonitorLinux> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(StorageMonitorLinux);
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_STORAGE_MONITOR_LINUX_H_
