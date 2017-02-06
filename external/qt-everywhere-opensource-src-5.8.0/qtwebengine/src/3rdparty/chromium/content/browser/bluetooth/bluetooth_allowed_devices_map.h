// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ALLOWED_DEVICES_MAP_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ALLOWED_DEVICES_MAP_

#include <map>
#include <memory>
#include <set>
#include <unordered_set>
#include <vector>

#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/web_bluetooth.mojom.h"
#include "url/origin.h"

namespace device {
class BluetoothUUID;
}

namespace content {

struct BluetoothScanFilter;

// Keeps track of which origins are allowed to access which devices and
// their services.
//
// |AddDevice| generates device ids, which are random strings that are unique
// in the map.
class CONTENT_EXPORT BluetoothAllowedDevicesMap final {
 public:
  BluetoothAllowedDevicesMap();
  ~BluetoothAllowedDevicesMap();

  // Adds the Bluetooth Device with |device_address| to the map of allowed
  // devices for that origin. Generates and returns a device id. Because
  // unique origins generate the same hash, unique origins are not supported.
  // Calling this function with a unique origin will CHECK-fail.
  const std::string& AddDevice(
      const url::Origin& origin,
      const std::string& device_address,
      const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options);

  // Removes the Bluetooth Device with |device_address| from the map of allowed
  // devices for |origin|.
  void RemoveDevice(const url::Origin& origin,
                    const std::string& device_address);

  // Returns the Bluetooth Device's id for |origin|. Returns an empty string
  // if |origin| is not allowed to access the device.
  const std::string& GetDeviceId(const url::Origin& origin,
                                 const std::string& device_address);

  // For |device_id| in |origin|, returns the Bluetooth device's address. If
  // there is no such |device_id| in |origin|, returns an empty string.
  const std::string& GetDeviceAddress(const url::Origin& origin,
                                      const std::string& device_id);

  // Returns true if the origin has previously been granted access to
  // the service.
  bool IsOriginAllowedToAccessService(
      const url::Origin& origin,
      const std::string& device_id,
      const device::BluetoothUUID& service_uuid) const;

 private:
  typedef std::map<std::string, std::string> DeviceAddressToIdMap;
  typedef std::map<std::string, std::string> DeviceIdToAddressMap;
  typedef std::map<
      std::string,
      std::unordered_set<device::BluetoothUUID, device::BluetoothUUIDHash>>
      DeviceIdToServicesMap;

  // Returns an id guaranteed to be unique for the map. The id is randomly
  // generated so that an origin can't guess the id used in another origin.
  std::string GenerateDeviceId();
  void AddUnionOfServicesTo(
      const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options,
      std::unordered_set<device::BluetoothUUID, device::BluetoothUUIDHash>*
          unionOfServices);

  // TODO(ortuno): Now that there is only one instance of this class per frame
  // and that this map gets destroyed when navigating consider removing the
  // origin mapping.
  // http://crbug.com/610343
  std::map<url::Origin, DeviceAddressToIdMap>
      origin_to_device_address_to_id_map_;
  std::map<url::Origin, DeviceIdToAddressMap>
      origin_to_device_id_to_address_map_;
  std::map<url::Origin, DeviceIdToServicesMap>
      origin_to_device_id_to_services_map_;

  // Keep track of all device_ids in the map.
  std::set<std::string> device_id_set_;
};

}  //  namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ALLOWED_DEVICES_MAP_
