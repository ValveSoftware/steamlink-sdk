// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/store.h"

namespace catalog {

// static
const char Store::kNameKey[] = "name";
// static
const char Store::kDisplayNameKey[] = "display_name";
// static
const char Store::kInterfaceProviderSpecsKey[] = "interface_provider_specs";
// static
const char Store::kInterfaceProviderSpecs_ProvidesKey[] = "provides";
// static
const char Store::kInterfaceProviderSpecs_RequiresKey[] = "requires";
// static
const char Store::kServicesKey[] = "services";

}  // namespace catalog
