// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_switches.h"

namespace precache {
namespace switches {

// Enables the proactive populating of the disk cache with Web resources that
// are likely to be needed in future page fetches.
const char kEnablePrecache[]            = "enable-precache";

// The URL that provides the PrecacheConfigurationSettings proto.
const char kPrecacheConfigSettingsURL[] = "precache-config-settings-url";

// Precache manifests will be served from URLs with this prefix.
const char kPrecacheManifestURLPrefix[] = "precache-manifest-url-prefix";

}  // namespace switches
}  // namespace precache
