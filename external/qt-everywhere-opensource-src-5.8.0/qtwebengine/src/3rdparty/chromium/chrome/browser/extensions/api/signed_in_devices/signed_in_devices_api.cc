// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/signed_in_devices/signed_in_devices_api.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_vector.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/signed_in_devices/id_mapping_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/extensions/api/signed_in_devices.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/sync_driver/device_info_tracker.h"
#include "components/sync_driver/local_device_info_provider.h"
#include "extensions/browser/extension_prefs.h"

using base::DictionaryValue;
using sync_driver::DeviceInfo;
using sync_driver::DeviceInfoTracker;
using sync_driver::LocalDeviceInfoProvider;

namespace extensions {

static const char kPrefStringForIdMapping[] = "id_mapping_dictioanry";

// Gets the dictionary that stores the id mapping. The dictionary is stored
// in the |ExtensionPrefs|.
const base::DictionaryValue* GetIdMappingDictionary(
    ExtensionPrefs* extension_prefs,
    const std::string& extension_id) {
  const base::DictionaryValue* out_value = NULL;
  if (!extension_prefs->ReadPrefAsDictionary(
          extension_id,
          kPrefStringForIdMapping,
          &out_value) || out_value == NULL) {
    // Looks like this is the first call to get the dictionary. Let us create
    // a dictionary and set it in to |extension_prefs|.
    std::unique_ptr<base::DictionaryValue> dictionary(
        new base::DictionaryValue());
    out_value = dictionary.get();
    extension_prefs->UpdateExtensionPref(
        extension_id,
        kPrefStringForIdMapping,
        dictionary.release());
  }

  return out_value;
}

// Helper routine to get all signed in devices. The helper takes in
// the pointers for |DeviceInfoTracker| and |Extensionprefs|. This
// makes it easier to test by passing mock values for these pointers.
ScopedVector<DeviceInfo> GetAllSignedInDevices(
    const std::string& extension_id,
    DeviceInfoTracker* device_tracker,
    ExtensionPrefs* extension_prefs) {
  DCHECK(device_tracker);
  ScopedVector<DeviceInfo> devices = device_tracker->GetAllDeviceInfo();
  const base::DictionaryValue* mapping_dictionary = GetIdMappingDictionary(
      extension_prefs,
      extension_id);

  CHECK(mapping_dictionary);

  // |mapping_dictionary| is const. So make an editable copy.
  std::unique_ptr<base::DictionaryValue> editable_mapping_dictionary(
      mapping_dictionary->DeepCopy());

  CreateMappingForUnmappedDevices(&(devices.get()),
                                  editable_mapping_dictionary.get());

  // Write into |ExtensionPrefs| which will get persisted in disk.
  extension_prefs->UpdateExtensionPref(extension_id,
                                       kPrefStringForIdMapping,
                                       editable_mapping_dictionary.release());
  return devices;
}

ScopedVector<DeviceInfo> GetAllSignedInDevices(
    const std::string& extension_id,
    Profile* profile) {
  // Get the device tracker and extension prefs pointers
  // and call the helper.
  DeviceInfoTracker* device_tracker =
      ProfileSyncServiceFactory::GetForProfile(profile)->GetDeviceInfoTracker();
  DCHECK(device_tracker);
  if (!device_tracker->IsSyncing()) {
    // Devices are not sync'ing.
    return ScopedVector<DeviceInfo>();
  }

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile);

  return GetAllSignedInDevices(extension_id, device_tracker, extension_prefs);
}

std::unique_ptr<DeviceInfo> GetLocalDeviceInfo(const std::string& extension_id,
                                               Profile* profile) {
  ProfileSyncService* pss = ProfileSyncServiceFactory::GetForProfile(profile);
  if (!pss) {
    return std::unique_ptr<DeviceInfo>();
  }

  LocalDeviceInfoProvider* local_device = pss->GetLocalDeviceInfoProvider();
  DCHECK(local_device);
  std::string guid = local_device->GetLocalSyncCacheGUID();
  std::unique_ptr<DeviceInfo> device =
      GetDeviceInfoForClientId(guid, extension_id, profile);
  return device;
}

bool SignedInDevicesGetFunction::RunSync() {
  std::unique_ptr<api::signed_in_devices::Get::Params> params(
      api::signed_in_devices::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool is_local = params->is_local.get() ? *params->is_local : false;

  if (is_local) {
    std::unique_ptr<DeviceInfo> device =
        GetLocalDeviceInfo(extension_id(), GetProfile());
    std::unique_ptr<base::ListValue> result(new base::ListValue());
    if (device.get()) {
      result->Append(device->ToValue());
    }
    SetResult(std::move(result));
    return true;
  }

  ScopedVector<DeviceInfo> devices =
      GetAllSignedInDevices(extension_id(), GetProfile());

  std::unique_ptr<base::ListValue> result(new base::ListValue());

  for (ScopedVector<DeviceInfo>::const_iterator it = devices.begin();
       it != devices.end();
       ++it) {
    result->Append((*it)->ToValue());
  }

  SetResult(std::move(result));
  return true;
}

}  // namespace extensions

