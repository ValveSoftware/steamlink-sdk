// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/media_storage_util.h"

#include <vector>

#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/storage_monitor/removable_device_constants.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace storage_monitor {

namespace {

// MediaDeviceNotification.DeviceInfo histogram values.
enum DeviceInfoHistogramBuckets {
  MASS_STORAGE_DEVICE_NAME_AND_UUID_AVAILABLE,
  MASS_STORAGE_DEVICE_UUID_MISSING,
  MASS_STORAGE_DEVICE_NAME_MISSING,
  MASS_STORAGE_DEVICE_NAME_AND_UUID_MISSING,
  MTP_STORAGE_DEVICE_NAME_AND_UUID_AVAILABLE,
  MTP_STORAGE_DEVICE_UUID_MISSING,
  MTP_STORAGE_DEVICE_NAME_MISSING,
  MTP_STORAGE_DEVICE_NAME_AND_UUID_MISSING,
  DEVICE_INFO_BUCKET_BOUNDARY
};

#if !defined(OS_WIN)
const char kRootPath[] = "/";
#endif

typedef std::vector<StorageInfo> StorageInfoList;

base::FilePath::StringType FindRemovableStorageLocationById(
    const std::string& device_id) {
  StorageInfoList devices =
      StorageMonitor::GetInstance()->GetAllAvailableStorages();
  for (StorageInfoList::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    if (it->device_id() == device_id
        && StorageInfo::IsRemovableDevice(device_id))
      return it->location();
  }
  return base::FilePath::StringType();
}

void FilterAttachedDevicesOnFileThread(MediaStorageUtil::DeviceIdSet* devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  MediaStorageUtil::DeviceIdSet missing_devices;

  for (MediaStorageUtil::DeviceIdSet::const_iterator it = devices->begin();
       it != devices->end();
       ++it) {
    StorageInfo::Type type;
    std::string unique_id;
    if (!StorageInfo::CrackDeviceId(*it, &type, &unique_id)) {
      missing_devices.insert(*it);
      continue;
    }

    if (type == StorageInfo::FIXED_MASS_STORAGE ||
        type == StorageInfo::ITUNES ||
        type == StorageInfo::PICASA) {
      if (!base::PathExists(base::FilePath::FromUTF8Unsafe(unique_id)))
        missing_devices.insert(*it);
      continue;
    }

    if (!MediaStorageUtil::IsRemovableStorageAttached(*it))
      missing_devices.insert(*it);
  }

  for (MediaStorageUtil::DeviceIdSet::const_iterator it =
           missing_devices.begin();
       it != missing_devices.end();
       ++it) {
    devices->erase(*it);
  }
}

}  // namespace

// static
bool MediaStorageUtil::HasDcim(const base::FilePath& mount_point) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  base::FilePath::StringType dcim_dir(kDCIMDirectoryName);
  if (!base::DirectoryExists(mount_point.Append(dcim_dir))) {
    // Check for lowercase 'dcim' as well.
    base::FilePath dcim_path_lower(
        mount_point.Append(base::ToLowerASCII(dcim_dir)));
    if (!base::DirectoryExists(dcim_path_lower))
      return false;
  }
  return true;
}

// static
bool MediaStorageUtil::CanCreateFileSystem(const std::string& device_id,
                                           const base::FilePath& path) {
  StorageInfo::Type type;
  if (!StorageInfo::CrackDeviceId(device_id, &type, NULL))
    return false;

  if (type == StorageInfo::MAC_IMAGE_CAPTURE)
    return true;

  return !path.empty() && path.IsAbsolute() && !path.ReferencesParent();
}

// static
void MediaStorageUtil::FilterAttachedDevices(DeviceIdSet* devices,
                                             const base::Closure& done) {
  if (BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    FilterAttachedDevicesOnFileThread(devices);
    done.Run();
    return;
  }
  BrowserThread::PostTaskAndReply(BrowserThread::FILE,
                                  FROM_HERE,
                                  base::Bind(&FilterAttachedDevicesOnFileThread,
                                             devices),
                                  done);
}

// TODO(kmadhusu) Write unit tests for GetDeviceInfoFromPath().
// static
bool MediaStorageUtil::GetDeviceInfoFromPath(const base::FilePath& path,
                                             StorageInfo* device_info,
                                             base::FilePath* relative_path) {
  DCHECK(device_info);
  DCHECK(relative_path);

  if (!path.IsAbsolute())
    return false;

  StorageInfo info;
  StorageMonitor* monitor = StorageMonitor::GetInstance();
  bool found_device = monitor->GetStorageInfoForPath(path, &info);

  if (found_device && StorageInfo::IsRemovableDevice(info.device_id())) {
    base::FilePath sub_folder_path;
    base::FilePath device_path(info.location());
    if (path != device_path) {
      bool success = device_path.AppendRelativePath(path, &sub_folder_path);
      DCHECK(success);
    }

    *device_info = info;
    *relative_path = sub_folder_path;
    return true;
  }

  // On Posix systems, there's one root so any absolute path could be valid.
  // TODO(gbillock): Delete this stanza? Posix systems should have the root
  // volume information. If not, we should move the below into the
  // right GetStorageInfoForPath implementations.
#if !defined(OS_POSIX)
  if (!found_device)
    return false;
#endif

  // Handle non-removable devices. Note: this is just overwriting
  // good values from StorageMonitor.
  // TODO(gbillock): Make sure return values from that class are definitive,
  // and don't do this here.
  info.set_device_id(
      StorageInfo::MakeDeviceId(StorageInfo::FIXED_MASS_STORAGE,
                                path.AsUTF8Unsafe()));
  *device_info = info;
  *relative_path = base::FilePath();
  return true;
}

// static
base::FilePath MediaStorageUtil::FindDevicePathById(
    const std::string& device_id) {
  StorageInfo::Type type;
  std::string unique_id;
  if (!StorageInfo::CrackDeviceId(device_id, &type, &unique_id))
    return base::FilePath();

  if (type == StorageInfo::FIXED_MASS_STORAGE ||
      type == StorageInfo::ITUNES ||
      type == StorageInfo::PICASA) {
    // For this type, the unique_id is the path.
    return base::FilePath::FromUTF8Unsafe(unique_id);
  }

  // For ImageCapture, the synthetic filesystem will be rooted at a fake
  // top-level directory which is the device_id.
  if (type == StorageInfo::MAC_IMAGE_CAPTURE) {
#if !defined(OS_WIN)
    return base::FilePath(kRootPath + device_id);
#endif
  }

  DCHECK(type == StorageInfo::MTP_OR_PTP ||
         type == StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM ||
         type == StorageInfo::REMOVABLE_MASS_STORAGE_NO_DCIM);
  return base::FilePath(FindRemovableStorageLocationById(device_id));
}

// static
void MediaStorageUtil::RecordDeviceInfoHistogram(
    bool mass_storage,
    const std::string& device_uuid,
    const base::string16& device_label) {
  unsigned int event_number = 0;
  if (!mass_storage)
    event_number = 4;

  if (device_label.empty())
    event_number += 2;

  if (device_uuid.empty())
    event_number += 1;
  enum DeviceInfoHistogramBuckets event =
      static_cast<enum DeviceInfoHistogramBuckets>(event_number);
  if (event >= DEVICE_INFO_BUCKET_BOUNDARY) {
    NOTREACHED();
    return;
  }
  UMA_HISTOGRAM_ENUMERATION("MediaDeviceNotifications.DeviceInfo", event,
                            DEVICE_INFO_BUCKET_BOUNDARY);
}

// static
bool MediaStorageUtil::IsRemovableStorageAttached(const std::string& id) {
  StorageMonitor* monitor = StorageMonitor::GetInstance();
  if (!monitor)
    return false;

  StorageInfoList devices = monitor->GetAllAvailableStorages();
  for (const auto& device : devices) {
    if (StorageInfo::IsRemovableDevice(id) && device.device_id() == id)
      return true;
  }
  return false;
}

}  // namespace storage_monitor
