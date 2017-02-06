// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CATALOG_STORE_H_
#define SERVICES_CATALOG_STORE_H_

#include <memory>

#include "base/values.h"

namespace catalog {

// Implemented by an object that provides storage for the application catalog
// (e.g. in Chrome, preferences). The Catalog is the canonical owner of the
// contents of the store, so no one else must modify its contents.
class Store {
 public:
  // Value is an integer.
  static const char kManifestVersionKey[];
  // Value is a string.
  static const char kNameKey[];
  // Value is a string.
  static const char kQualifierKey[];
  // Value is a string.
  static const char kDisplayNameKey[];
  // Value is a dictionary.
  static const char kCapabilitiesKey[];
  // Value is a dictionary.
  static const char kCapabilities_ProvidedKey[];
  // Value is a dictionary.
  static const char kCapabilities_RequiredKey[];
  // Value is a list.
  static const char kCapabilities_ClassesKey[];
  // Value is a list.
  static const char kCapabilities_InterfacesKey[];
  // Value is a list.
  static const char kApplicationsKey[];

  virtual ~Store() {}

  // Called during initialization to construct the Catalog's catalog.
  // Returns a serialized list of the apps. Each entry in the returned list
  // corresponds to an app (as a dictionary). Each dictionary has a name,
  // display name and capabilities. The return value is owned by the caller.
  virtual const base::ListValue* GetStore() = 0;

  // Write the catalog to the store. Called when the Catalog learns of a newly
  // encountered application.
  virtual void UpdateStore(std::unique_ptr<base::ListValue> store) = 0;
};

}  // namespace catalog

#endif  // SERVICES_CATALOG_STORE_H_
