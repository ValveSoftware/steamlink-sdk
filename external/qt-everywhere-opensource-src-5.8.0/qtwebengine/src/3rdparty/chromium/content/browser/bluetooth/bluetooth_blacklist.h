// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_BLACKLIST_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_BLACKLIST_H_

#include <map>
#include <vector>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/web_bluetooth.mojom.h"

namespace content {

// Implements the Web Bluetooth Blacklist policy as defined in the Web Bluetooth
// specification:
// https://webbluetoothcg.github.io/web-bluetooth/#the-gatt-blacklist
//
// Client code may query UUIDs to determine if they are excluded from use by the
// blacklist.
//
// Singleton access via Get() enforces only one copy of blacklist.
class CONTENT_EXPORT BluetoothBlacklist final {
 public:
  // Blacklist value terminology from Web Bluetooth specification:
  // https://webbluetoothcg.github.io/web-bluetooth/#the-gatt-blacklist
  enum class Value {
    EXCLUDE,        // Implies EXCLUDE_READS and EXCLUDE_WRITES.
    EXCLUDE_READS,  // Excluded from read operations.
    EXCLUDE_WRITES  // Excluded from write operations.
  };

  ~BluetoothBlacklist();

  // Returns a singleton instance of the blacklist.
  static BluetoothBlacklist& Get();

  // Adds a UUID to the blacklist to be excluded from operations, merging with
  // any previous value and resulting in the strictest exclusion rule from the
  // combination of the two, E.G.:
  //   Add(uuid, EXCLUDE_READS);
  //   Add(uuid, EXCLUDE_WRITES);
  //   IsExcluded(uuid);  // true.
  // Requires UUID to be valid.
  void Add(const device::BluetoothUUID&, Value);

  // Adds UUIDs to the blacklist by parsing a blacklist string and calling
  // Add(uuid, value).
  //
  // The blacklist string format is defined at
  // ContentBrowserClient::GetWebBluetoothBlacklist().
  //
  // Malformed pairs in the string are ignored, including invalid UUID or
  // exclusion values. Duplicate UUIDs follow Add()'s merging rule.
  void Add(base::StringPiece blacklist_string);

  // Returns if a UUID is excluded from all operations. UUID must be valid.
  bool IsExcluded(const device::BluetoothUUID&) const;

  // Returns if any UUID in a set of filters is excluded from all operations.
  // UUID must be valid.
  bool IsExcluded(
      const mojo::Array<blink::mojom::WebBluetoothScanFilterPtr>& filters);

  // Returns if a UUID is excluded from read operations. UUID must be valid.
  bool IsExcludedFromReads(const device::BluetoothUUID&) const;

  // Returns if a UUID is excluded from write operations. UUID must be valid.
  bool IsExcludedFromWrites(const device::BluetoothUUID&) const;

  // Modifies |options->optional_services|, removing any UUIDs with
  // Value::EXCLUDE.
  void RemoveExcludedUUIDs(
      blink::mojom::WebBluetoothRequestDeviceOptions* options);

  // Size of blacklist.
  size_t size() { return blacklisted_uuids_.size(); }

  void ResetToDefaultValuesForTest();

 private:
  // friend LazyInstance to permit access to private constructor.
  friend base::DefaultLazyInstanceTraits<BluetoothBlacklist>;

  BluetoothBlacklist();

  void PopulateWithDefaultValues();

  // Populates blacklist with values obtained dynamically from a server, able
  // to be updated without shipping new executable versions.
  void PopulateWithServerProvidedValues();

  // Map of UUID to blacklisted value.
  std::map<device::BluetoothUUID, Value> blacklisted_uuids_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothBlacklist);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_BLACKLIST_H_
