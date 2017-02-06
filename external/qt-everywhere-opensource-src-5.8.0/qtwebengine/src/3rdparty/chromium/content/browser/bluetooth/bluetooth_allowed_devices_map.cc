// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_allowed_devices_map.h"

#include <vector>

#include "base/base64.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "content/browser/bluetooth/bluetooth_blacklist.h"
#include "crypto/random.h"

using device::BluetoothUUID;

namespace content {

namespace {
const size_t kIdLength = 16 /* 128bits */;

std::string GetBase64Id() {
  std::string bytes(
      kIdLength + 1 /* to avoid bytes being reallocated by WriteInto */, '\0');

  crypto::RandBytes(
      base::WriteInto(&bytes /* str */, kIdLength + 1 /* length_with_null */),
      kIdLength);

  base::Base64Encode(bytes, &bytes);

  return bytes;
}
}  // namespace

BluetoothAllowedDevicesMap::BluetoothAllowedDevicesMap() {}
BluetoothAllowedDevicesMap::~BluetoothAllowedDevicesMap() {}

const std::string& BluetoothAllowedDevicesMap::AddDevice(
    const url::Origin& origin,
    const std::string& device_address,
    const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options) {
  VLOG(1) << "Adding a device to Map of Allowed Devices.";

  // "Unique" Origins generate the same key in maps, therefore are not
  // supported.
  CHECK(!origin.unique());

  auto device_address_to_id_map = origin_to_device_address_to_id_map_[origin];
  auto id_iter = device_address_to_id_map.find(device_address);
  if (id_iter != device_address_to_id_map.end()) {
    VLOG(1) << "Device already in map of allowed devices.";
    const auto& device_id = id_iter->second;

    AddUnionOfServicesTo(
        options, &origin_to_device_id_to_services_map_[origin][device_id]);

    return origin_to_device_address_to_id_map_[origin][device_address];
  }
  const std::string device_id = GenerateDeviceId();
  VLOG(1) << "Id generated for device: " << device_id;

  origin_to_device_address_to_id_map_[origin][device_address] = device_id;
  origin_to_device_id_to_address_map_[origin][device_id] = device_address;
  AddUnionOfServicesTo(
      options, &origin_to_device_id_to_services_map_[origin][device_id]);

  CHECK(device_id_set_.insert(device_id).second);

  return origin_to_device_address_to_id_map_[origin][device_address];
}

void BluetoothAllowedDevicesMap::RemoveDevice(
    const url::Origin& origin,
    const std::string& device_address) {
  const std::string device_id = GetDeviceId(origin, device_address);
  DCHECK(!device_id.empty());

  // 1. Remove from all three maps.
  CHECK(origin_to_device_address_to_id_map_[origin].erase(device_address));
  CHECK(origin_to_device_id_to_address_map_[origin].erase(device_id));
  CHECK(origin_to_device_id_to_services_map_[origin].erase(device_id));

  // 2. Remove empty map for origin.
  if (origin_to_device_address_to_id_map_[origin].empty()) {
    CHECK(origin_to_device_address_to_id_map_.erase(origin));
    CHECK(origin_to_device_id_to_address_map_.erase(origin));
    CHECK(origin_to_device_id_to_services_map_.erase(origin));
  }

  // 3. Remove from set of ids.
  CHECK(device_id_set_.erase(device_id));
}

const std::string& BluetoothAllowedDevicesMap::GetDeviceId(
    const url::Origin& origin,
    const std::string& device_address) {
  auto address_map_iter = origin_to_device_address_to_id_map_.find(origin);
  if (address_map_iter == origin_to_device_address_to_id_map_.end()) {
    return base::EmptyString();
  }

  const auto& device_address_to_id_map = address_map_iter->second;

  auto id_iter = device_address_to_id_map.find(device_address);
  if (id_iter == device_address_to_id_map.end()) {
    return base::EmptyString();
  }
  return id_iter->second;
}

const std::string& BluetoothAllowedDevicesMap::GetDeviceAddress(
    const url::Origin& origin,
    const std::string& device_id) {
  auto id_map_iter = origin_to_device_id_to_address_map_.find(origin);
  if (id_map_iter == origin_to_device_id_to_address_map_.end()) {
    return base::EmptyString();
  }

  const auto& device_id_to_address_map = id_map_iter->second;

  auto id_iter = device_id_to_address_map.find(device_id);

  return id_iter == device_id_to_address_map.end() ? base::EmptyString()
                                                   : id_iter->second;
}

bool BluetoothAllowedDevicesMap::IsOriginAllowedToAccessService(
    const url::Origin& origin,
    const std::string& device_id,
    const BluetoothUUID& service_uuid) const {
  if (BluetoothBlacklist::Get().IsExcluded(service_uuid)) {
    return false;
  }

  auto id_map_iter = origin_to_device_id_to_services_map_.find(origin);
  if (id_map_iter == origin_to_device_id_to_services_map_.end()) {
    return false;
  }

  const auto& device_id_to_services_map = id_map_iter->second;

  auto id_iter = device_id_to_services_map.find(device_id);

  return id_iter == device_id_to_services_map.end()
             ? false
             : ContainsKey(id_iter->second, service_uuid);
}

std::string BluetoothAllowedDevicesMap::GenerateDeviceId() {
  std::string device_id = GetBase64Id();
  while (ContainsKey(device_id_set_, device_id)) {
    LOG(WARNING) << "Generated repeated id.";
    device_id = GetBase64Id();
  }
  return device_id;
}

void BluetoothAllowedDevicesMap::AddUnionOfServicesTo(
    const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options,
    std::unordered_set<BluetoothUUID, device::BluetoothUUIDHash>*
        unionOfServices) {
  for (const auto& filter : options->filters) {
    for (const base::Optional<BluetoothUUID>& uuid : filter->services) {
      unionOfServices->insert(uuid.value());
    }
  }
  for (const base::Optional<BluetoothUUID>& uuid : options->optional_services) {
    unionOfServices->insert(uuid.value());
  }
}

}  // namespace content
