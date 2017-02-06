// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_blacklist.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/string_split.h"
#include "content/public/browser/content_browser_client.h"

using device::BluetoothUUID;

namespace {

static base::LazyInstance<content::BluetoothBlacklist>::Leaky g_singleton =
    LAZY_INSTANCE_INITIALIZER;

void RecordUMAParsedNonEmptyString(bool success) {
  UMA_HISTOGRAM_BOOLEAN("Bluetooth.Web.Blacklist.ParsedNonEmptyString",
                        success);
}

}  // namespace

namespace content {

BluetoothBlacklist::~BluetoothBlacklist() {}

// static
BluetoothBlacklist& BluetoothBlacklist::Get() {
  return g_singleton.Get();
}

void BluetoothBlacklist::Add(const BluetoothUUID& uuid, Value value) {
  CHECK(uuid.IsValid());
  auto insert_result = blacklisted_uuids_.insert(std::make_pair(uuid, value));
  bool inserted = insert_result.second;
  if (!inserted) {
    Value& stored = insert_result.first->second;
    if (stored != value)
      stored = Value::EXCLUDE;
  }
}

void BluetoothBlacklist::Add(base::StringPiece blacklist_string) {
  if (blacklist_string.empty())
    return;
  base::StringPairs kv_pairs;
  bool parsed_values = false;
  bool invalid_values = false;
  SplitStringIntoKeyValuePairs(blacklist_string,
                               ':',  // Key-value delimiter
                               ',',  // Key-value pair delimiter
                               &kv_pairs);
  for (const auto& pair : kv_pairs) {
    BluetoothUUID uuid(pair.first);
    if (uuid.IsValid() && pair.second.size() == 1u) {
      switch (pair.second[0]) {
        case 'e':
          Add(uuid, Value::EXCLUDE);
          parsed_values = true;
          continue;
        case 'r':
          Add(uuid, Value::EXCLUDE_READS);
          parsed_values = true;
          continue;
        case 'w':
          Add(uuid, Value::EXCLUDE_WRITES);
          parsed_values = true;
          continue;
      }
    }
    invalid_values = true;
  }
  RecordUMAParsedNonEmptyString(parsed_values && !invalid_values);
}

bool BluetoothBlacklist::IsExcluded(const BluetoothUUID& uuid) const {
  CHECK(uuid.IsValid());
  const auto& it = blacklisted_uuids_.find(uuid);
  if (it == blacklisted_uuids_.end())
    return false;
  return it->second == Value::EXCLUDE;
}

bool BluetoothBlacklist::IsExcluded(
    const mojo::Array<blink::mojom::WebBluetoothScanFilterPtr>& filters) {
  for (const blink::mojom::WebBluetoothScanFilterPtr& filter : filters) {
    for (const base::Optional<BluetoothUUID>& service : filter->services) {
      if (IsExcluded(service.value())) {
        return true;
      }
    }
  }
  return false;
}

bool BluetoothBlacklist::IsExcludedFromReads(const BluetoothUUID& uuid) const {
  CHECK(uuid.IsValid());
  const auto& it = blacklisted_uuids_.find(uuid);
  if (it == blacklisted_uuids_.end())
    return false;
  return it->second == Value::EXCLUDE || it->second == Value::EXCLUDE_READS;
}

bool BluetoothBlacklist::IsExcludedFromWrites(const BluetoothUUID& uuid) const {
  CHECK(uuid.IsValid());
  const auto& it = blacklisted_uuids_.find(uuid);
  if (it == blacklisted_uuids_.end())
    return false;
  return it->second == Value::EXCLUDE || it->second == Value::EXCLUDE_WRITES;
}

void BluetoothBlacklist::RemoveExcludedUUIDs(
    blink::mojom::WebBluetoothRequestDeviceOptions* options) {
  mojo::Array<base::Optional<BluetoothUUID>>
      optional_services_blacklist_filtered;
  for (const base::Optional<BluetoothUUID>& uuid : options->optional_services) {
    if (!IsExcluded(uuid.value())) {
      optional_services_blacklist_filtered.push_back(uuid);
    }
  }
  options->optional_services = std::move(optional_services_blacklist_filtered);
}

void BluetoothBlacklist::ResetToDefaultValuesForTest() {
  blacklisted_uuids_.clear();
  PopulateWithDefaultValues();
  PopulateWithServerProvidedValues();
}

BluetoothBlacklist::BluetoothBlacklist() {
  PopulateWithDefaultValues();
  PopulateWithServerProvidedValues();
}

void BluetoothBlacklist::PopulateWithDefaultValues() {
  blacklisted_uuids_.clear();

  // Testing from Layout Tests Note:
  //
  // Random UUIDs for object & exclude permutations that do not exist in the
  // standard blacklist are included to facilitate integration testing from
  // Layout Tests.  Unit tests can dynamically modify the blacklist, but don't
  // offer the full integration test to the Web Bluetooth Javascript bindings.
  //
  // This is done for simplicity as opposed to exposing a testing API that can
  // add to the blacklist over time, which would be over engineered.
  //
  // Remove testing UUIDs if the specified blacklist is updated to include UUIDs
  // that match the specific permutations.
  DCHECK(BluetoothUUID("00001800-0000-1000-8000-00805f9b34fb") ==
         BluetoothUUID("1800"));

  // Blacklist UUIDs updated 2016-04-07 from:
  // https://github.com/WebBluetoothCG/registries/blob/master/gatt_blacklist.txt
  // Short UUIDs are used for readability of this list.
  //
  // Services:
  Add(BluetoothUUID("1812"), Value::EXCLUDE);
  Add(BluetoothUUID("00001530-1212-efde-1523-785feabcd123"), Value::EXCLUDE);
  Add(BluetoothUUID("f000ffc0-0451-4000-b000-000000000000"), Value::EXCLUDE);
  // Characteristics:
  Add(BluetoothUUID("2a02"), Value::EXCLUDE_WRITES);
  Add(BluetoothUUID("2a03"), Value::EXCLUDE);
  Add(BluetoothUUID("2a25"), Value::EXCLUDE);
  // Characteristics for Layout Tests:
  Add(BluetoothUUID("bad1c9a2-9a5b-4015-8b60-1579bbbf2135"),
      Value::EXCLUDE_READS);
  // Descriptors:
  Add(BluetoothUUID("2902"), Value::EXCLUDE_WRITES);
  Add(BluetoothUUID("2903"), Value::EXCLUDE_WRITES);
  // Descriptors for Layout Tests:
  Add(BluetoothUUID("bad2ddcf-60db-45cd-bef9-fd72b153cf7c"), Value::EXCLUDE);
  Add(BluetoothUUID("bad3ec61-3cc3-4954-9702-7977df514114"),
      Value::EXCLUDE_READS);
}

void BluetoothBlacklist::PopulateWithServerProvidedValues() {
  // DCHECK to maybe help debug https://crbug.com/604078.
  DCHECK(GetContentClient());
  Add(GetContentClient()->browser()->GetWebBluetoothBlacklist());
}

}  // namespace content
